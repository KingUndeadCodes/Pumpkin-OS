#include "./ac97.h"
#include "../../port.cpp"
#include "../../pit/pit.h"
#include "../../pci/pci.h"
#include "../../serial/serial.h"
#include "../../memory/allocator.h"
#include "../../chorus/chorus.h"
#include "../../idt/irq.h"

/**
 * AC97 PCM-out streaming driver.
 *
 * This version treats the AC97 Buffer Descriptor List as a cyclic DMA ring.
 * Chorus owns the actual PCM DMA memory. AC97 owns the BDL and advances LVI
 * as each completed descriptor is refilled.
 */

static struct AC97 AC97Device;

static volatile bool    ac97_stream_active = false;
static volatile uint8_t ac97_next_completion_index = 0;
static uint32_t         ac97_fragment_bytes = AC97_RING_FRAGMENT_BYTES;
static uint32_t         ac97_ring_bytes = AC97_RING_FRAGMENT_BYTES * AC97_BDL_ENTRY_COUNT;

static inline uint16_t ac97_pcm_out_base(void) {
    return (uint16_t)(AC97Device.nabm_base + AC97Registers::PCMOutputRegisterBox);
}

static inline uint16_t ac97_pcm_out_reg(uint8_t reg) {
    return (uint16_t)(ac97_pcm_out_base() + reg);
}

static inline uint32_t ac97_min_u32(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

static inline uint32_t ac97_align4_down(uint32_t value) {
    return value & ~3u;
}

// timer_wait(N) waits N PIT ticks. pit_init(1000) makes a tick 1ms instead
// of the old ~18.2Hz default's ~55ms, so these loop bounds are scaled up
// ~55x to keep the same ~5.5s worst-case timeout — polling now happens
// every 1ms instead of every ~55ms, so the typical (codec already ready)
// case is also faster, not just the timeout unchanged.
static inline void ac97_wait_codec_ready(uint16_t nabm_base) {
    const uint32_t GS = nabm_base + AC97Registers::GlobalStatus;
    int ticks = 0;
    while (((inl(GS) & (1u << 8)) == 0) && ticks < 5500) {
        timer_wait(1);
        ticks++;
    }
}

static inline void ac97_reset_stream(uint16_t nabm_base, uint8_t box_base) {
    const uint16_t CR = nabm_base + box_base + AC97Registers::PCMOut_CR;

    outb(CR, 0x00);
    outb(CR, AC97_PCM_CR_RESET);

    int ticks = 0;
    while ((inb(CR) & AC97_PCM_CR_RESET) && ticks < 5500) {
        timer_wait(1);
        ticks++;
    }
}

static void ac97_refill_fragment(uint8_t index) {
    if (!sound_buffer_refilling_info || !sound_buffer_refilling_info->fill_buffer || !pcm_data) {
        return;
    }

    uint8_t *target = pcm_data + ((uint32_t)index * ac97_fragment_bytes);
    sound_buffer_refilling_info->fill_buffer(target);
    sound_buffer_refilling_info->last_filled_buffer = index;
}

static void ac97_complete_descriptor(uint8_t index) {
    if (!sound_buffer_refilling_info) {
        return;
    }

    sound_buffer_refilling_info->played_bytes_by_finished_buffers += ac97_fragment_bytes;
    sound_buffer_refilling_info->played_bytes = sound_buffer_refilling_info->played_bytes_by_finished_buffers;
    sound_buffer_refilling_info->actually_playing_buffer = (uint8_t)((index + 1) & (AC97_BDL_ENTRY_COUNT - 1));

    if (sound_buffer_refilling_info->played_bytes_by_finished_buffers >=
        sound_buffer_refilling_info->size_of_full_pcm_output_in_bytes) {
        AC97StopPlayback();
        return;
    }

    ac97_refill_fragment(index);

    /*
     * Mark this descriptor valid again. Since the card has moved past it,
     * setting LVI to this index extends the circular queue behind CIV.
     */
    outb(ac97_pcm_out_reg(AC97Registers::PCMOut_LVI), index);
}

static void AC97_IRQ_HANDLER(struct regs *r) {
    if (!ac97_stream_active) {
        return;
    }

    uint16_t sr = inw(ac97_pcm_out_reg(AC97Registers::PCMOut_SR));

    /* Shared IRQ line: ignore interrupts that AC97 did not raise. */
    if ((sr & AC97_PCM_SR_CLEAR_MASK) == 0) {
        return;
    }

    /* Ack the AC97 stream first so the status latch is cleared. */
    outw(ac97_pcm_out_reg(AC97Registers::PCMOut_SR), sr & AC97_PCM_SR_CLEAR_MASK);

    uint8_t civ = inb(ac97_pcm_out_reg(AC97Registers::PCMOut_CIV)) & 31;

    /* Process every descriptor completed since the previous AC97 IRQ. */
    while (ac97_stream_active && ac97_next_completion_index != civ) {
        uint8_t completed = ac97_next_completion_index;
        ac97_next_completion_index = (uint8_t)((ac97_next_completion_index + 1) & 31);
        ac97_complete_descriptor(completed);
    }

    /*
     * If the device halted because LVI was reached, restart it after we have
     * refilled/advanced LVI. This is a safety net for delayed IRQ handling.
     */
    if (ac97_stream_active) {
        uint16_t new_sr = inw(ac97_pcm_out_reg(AC97Registers::PCMOut_SR));
        if (new_sr & AC97_PCM_SR_DCH) {
            outb(ac97_pcm_out_reg(AC97Registers::PCMOut_CR),
                 AC97_PCM_CR_RUN | AC97_PCM_CR_IOCE | AC97_PCM_CR_FEIE | AC97_PCM_CR_LVBIE);
        }
    }
}

uint32_t AC97GetPreferredBufferSize(void) {
    return AC97_RING_FRAGMENT_BYTES;
}

void AC97_INIT(uint8_t bus, uint8_t device, uint8_t function) {
    serial_write_string("AC97_INIT called.");

    uint16_t nam_base = pciConfigReadWord(bus, device, function, 0x10) & ~0x7;
    uint16_t nabm_base = pciConfigReadWord(bus, device, function, 0x14) & ~0x7;

    uint16_t cmd = pciConfigReadWord(bus, device, function, 0x04);
    cmd |= (1 << 0) | (1 << 2); /* I/O space + bus master */
    pciConfigWriteWord(bus, device, function, 0x04, cmd);

    /* Enable global AC97 interrupt bit as well as controller reset. */
    outl(nabm_base + AC97Registers::GlobalControl, (1 << 1));
    timer_wait(165); // ~165ms settle delay; was timer_wait(3) at the old ~18.2Hz tick rate
    ac97_wait_codec_ready(nabm_base);

    ac97_reset_stream(nabm_base, AC97Registers::PCMInputRegisterBox);
    ac97_reset_stream(nabm_base, AC97Registers::PCMOutputRegisterBox);
    ac97_reset_stream(nabm_base, AC97Registers::MicrophoneRegisterBox);

    outw(nam_base + AC97Registers::ResetRegisterCapabilities, 0x0000);
    outw(nam_base + AC97Registers::SetPCMOutputVolume, 0x0000);
    outw(nam_base + AC97Registers::SetAUXOutputVolume, 0x2020);

    dma_buffer_t allocation = dma_alloc(sizeof(struct AC97BufferEntry) * AC97_BDL_ENTRY_COUNT, 16);
    clear_memory((uint32_t)allocation.virt, allocation.size);

    uint16_t extended_capabilities = inw(nam_base + AC97Registers::ExtendedCapabilities);
    if ((extended_capabilities & AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) == AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) {
        outw(nam_base + AC97Registers::ExtendedCapabilitiesControl, AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE);
    }

    uint16_t aux_out_number_of_volume_steps = 31;
    if ((inw(nam_base + AC97Registers::SetAUXOutputVolume) & 0x2020) == 0x2020) {
        aux_out_number_of_volume_steps = 63;
    }

    AC97Device.bus = bus;
    AC97Device.device = device;
    AC97Device.function = function;
    AC97Device.nam_base = nam_base;
    AC97Device.nabm_base = nabm_base;
    AC97Device.buffer = (uint8_t*)allocation.virt;
    AC97Device.buffer_phys = allocation.phys;
    AC97Device.extended_capabilities = extended_capabilities;
    AC97Device.aux_out_number_of_volume_steps = aux_out_number_of_volume_steps;
    AC97Device.irq_line = pciConfigReadWord(bus, device, function, 0x3C) & 0xFF;

    uint8_t sound_volume = 29;
    AC97SetVolumeRegister(AC97Registers::SetAUXOutputVolume, aux_out_number_of_volume_steps, aux_out_number_of_volume_steps);
    AC97SetVolumeRegister(AC97Registers::SetMasterOutputVolume, AC97_SPEAKER_OUTPUT_NUMBER_OF_VOLUME_STEPS, sound_volume);

    if (AC97Device.irq_line < 16) {
        irq_install_handler(AC97Device.irq_line, AC97_IRQ_HANDLER);
        serial_write_string("AC97: IRQ handler installed.\n");
    } else {
        serial_write_string("AC97: invalid PCI IRQ line.\n");
    }
}

void AC97SetVolumeRegister(uint32_t offset, uint8_t number_of_volume_steps, uint8_t volume) {
    if (volume == 0) {
        outw(AC97Device.nam_base + offset, 0x8000);
        return;
    }

    volume = ((100 - volume) * number_of_volume_steps / 100);
    outw(AC97Device.nam_base + offset, ((volume) | (volume << 8)));
}

void AC97SetSampleRate(uint16_t sample_rate) {
    if ((AC97Device.extended_capabilities & AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) == AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) {
        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_frontSampleRate, sample_rate);
        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_surrSampleRate, sample_rate);
        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_lfeSampleRate, sample_rate);
        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_lrSampleRate, sample_rate);
    }
}


bool AC97IsPlaying(void) {
    return ac97_stream_active;
}

void AC97StopPlayback(void) {
    uint16_t nabm_base = AC97Device.nabm_base;

    ac97_stream_active = false;
    outb(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_CR, 0x00);
    outw(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_SR, AC97_PCM_SR_CLEAR_MASK);
}

void AC97PlayData(uint32_t sample_rate) {
    if (!AC97Device.buffer || !pcm_data || pcm_data_phys == 0 || !sound_buffer_refilling_info) {
        serial_write_string("AC97: cannot start; missing DMA buffer or stream info.\n");
        return;
    }

    uint16_t nabm_base = AC97Device.nabm_base;

    AC97StopPlayback();
    AC97SetSampleRate((uint16_t)sample_rate);
    ac97_reset_stream(nabm_base, AC97Registers::PCMOutputRegisterBox);

    uint32_t requested_fragment = sound_buffer_refilling_info->buffer_size;
    if (requested_fragment == 0) {
        requested_fragment = AC97_RING_FRAGMENT_BYTES;
    }

    requested_fragment = ac97_align4_down(requested_fragment);
    requested_fragment = ac97_min_u32(requested_fragment, AC97_RING_FRAGMENT_BYTES);
    requested_fragment = ac97_min_u32(requested_fragment, (uint32_t)(AC97_MAX_BD_WORDS * 2));

    if (requested_fragment < 4) {
        requested_fragment = 4;
    }

    ac97_fragment_bytes = requested_fragment;
    ac97_ring_bytes = ac97_fragment_bytes * AC97_BDL_ENTRY_COUNT;
    sound_buffer_refilling_info->buffer_size = ac97_fragment_bytes;

    clear_memory((uint32_t)AC97Device.buffer, sizeof(struct AC97BufferEntry) * AC97_BDL_ENTRY_COUNT);

    struct AC97BufferEntry* bdl = (struct AC97BufferEntry*)AC97Device.buffer;

    for (uint32_t i = 0; i < AC97_BDL_ENTRY_COUNT; i++) {
        bdl[i].sample_memory = pcm_data_phys + (i * ac97_fragment_bytes);
        bdl[i].number_of_samples = (uint16_t)(ac97_fragment_bytes / 2);
        bdl[i].interrupt_on_completion = 1;
        bdl[i].last_buffer_entry = 0;
    }

    outl(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_BDBAR,
         (uint32_t)AC97Device.buffer_phys);

    ac97_next_completion_index = 0;

    outw(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_SR, AC97_PCM_SR_CLEAR_MASK);
    outb(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_LVI,
         AC97_BDL_ENTRY_COUNT - 1);

    ac97_stream_active = true;

    outb(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_CR,
         AC97_PCM_CR_RUN | AC97_PCM_CR_IOCE | AC97_PCM_CR_FEIE | AC97_PCM_CR_LVBIE);
}

bool AC97IsSupportedSampleRate(uint16_t sample_rate) {
    if (sample_rate == 48000) {
        return true;
    }

    if ((AC97Device.extended_capabilities & AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) == AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) {
        if (sample_rate == 44100) {
            return true;
        }

        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_frontSampleRate, sample_rate);
        return inw(AC97Device.nam_base + AC97Registers::PCM_DAC_frontSampleRate) == sample_rate;
    }

    return false;
}
