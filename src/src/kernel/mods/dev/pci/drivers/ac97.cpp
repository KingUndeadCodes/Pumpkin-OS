#include "./ac97.h"
#include "../../port.cpp"
#include "../../pit/pit.h"
#include "../../pci/pci.h"
#include "../../serial/serial.h"
#include "../../memory/allocator.h"
#include "../../chorus/chorus.h"

/**
 * RESOURCES:
 *  #1 - https://wiki.osdev.org/AC97
 *  #2 - https://forum.osdev.org/viewtopic.php?t=33531
 *  #3 - https://github.com/VendelinSlezak/BleskOS/blob/master/source/drivers/sound/ac97.h#L58
*/

// Big shout out to BleskOS, a lot of the code is from there.

static struct AC97 AC97Device;

/* --- small helpers ------------------------------------------------------- */

static inline void ac97_wait_codec_ready(uint16_t nabm_base) {
    // Wait a bit for codec ready (Global Status bit 8, per common docs).
    // We’ll just poll briefly to avoid a hard wait loop in early boot.
    const uint32_t GS = nabm_base + AC97Registers::GlobalStatus;
    int ticks = 0;
    while (((inl(GS) & (1u << 8)) == 0) && ticks < 100) {
        timer_wait(1); // ~1ms
        ticks++;
    }
}

static inline void ac97_reset_stream(uint16_t nabm_base, uint8_t box_base) {
    // Stop & reset the given box (CR at box_base + PCMOut_CR)
    const uint16_t CR = nabm_base + box_base + AC97Registers::PCMOut_CR;
    outb(CR, 0x00);
    outb(CR, 0x02); // RST
    // wait for RST bit to clear
    int ticks = 0;
    while ((inb(CR) & 0x02) && ticks < 100) {
        timer_wait(1);
        ticks++;
    }
}

/* --- public API ---------------------------------------------------------- */

void AC97_INIT(uint8_t bus, uint8_t device, uint8_t function) {
    serial_write_string("AC97_INIT called.");

    /* Read BARs (your helpers return 16-bit; still fine for QEMU I/O BARs) */
    uint16_t nam_base = pciConfigReadWord(bus, device, function, 0x10) & ~0x7; // I/O BAR, mask low bits
    uint16_t nabm_base = pciConfigReadWord(bus, device, function, 0x14) & ~0x7;

    /* Enable I/O + Bus Master on PCI command (bits 0 and 2) */
    uint16_t cmd = pciConfigReadWord(bus, device, function, 0x04);
    cmd |= (1 << 0) | (1 << 2);
    pciConfigWriteWord(bus, device, function, 0x04, cmd);

    /* Bring controller up: Global Control (NABM) */
    outl(nabm_base + AC97Registers::GlobalControl, (0b00 << 22) | (0b00 << 20) | (0 << 2) | (1 << 1)); // cold/warm reset off, enable GIE=0
    timer_wait(3);
    ac97_wait_codec_ready(nabm_base);

    /* Reset all streams (boxes) */
    ac97_reset_stream(nabm_base, AC97Registers::PCMInputRegisterBox);
    ac97_reset_stream(nabm_base, AC97Registers::PCMOutputRegisterBox);
    ac97_reset_stream(nabm_base, AC97Registers::MicrophoneRegisterBox);

    /* NAM reset + volumes */
    outw(nam_base + AC97Registers::ResetRegisterCapabilities, 0x0000); // any write resets
    // Unmute PCM and Master
    outw(nam_base + AC97Registers::SetPCMOutputVolume, 0x0000);   // 0 = max volume (unmuted)
    // AUX path detect (to know number of steps)
    outw(nam_base + AC97Registers::SetAUXOutputVolume, 0x2020);

    /* Allocate BDL (32 entries) using your DMA allocator */
    dma_buffer_t allocation = dma_alloc(sizeof(struct AC97BufferEntry) * 32, 16);
    clear_memory((uint32_t)allocation.virt, allocation.size);

    /* Extended caps + enable VRA if present */
    uint16_t extended_capabilities = inw(nam_base + AC97Registers::ExtendedCapabilities);
    if ((extended_capabilities & AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) == AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) {
        outw(nam_base + AC97Registers::ExtendedCapabilitiesControl, AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE);
    }

    /* Determine AUX steps (31 vs 63) */
    uint16_t aux_out_number_of_volume_steps = 31;
    if ((inw(nam_base + AC97Registers::SetAUXOutputVolume) & 0x2020) == 0x2020) {
        aux_out_number_of_volume_steps = 63;
    }

    /* Save device state */
    AC97Device.bus = bus;
    AC97Device.device = device;
    AC97Device.function = function;          // <-- fixed
    AC97Device.nam_base = nam_base;
    AC97Device.nabm_base = nabm_base;
    AC97Device.buffer = (uint8_t*)allocation.virt;
    AC97Device.buffer_phys = allocation.phys;
    AC97Device.extended_capabilities = extended_capabilities;
    AC97Device.aux_out_number_of_volume_steps = aux_out_number_of_volume_steps;

    /* Set overall output volume (master) in your 0-100 style */
    uint8_t sound_volume = 29;
    AC97SetVolumeRegister(AC97Registers::SetAUXOutputVolume, aux_out_number_of_volume_steps, aux_out_number_of_volume_steps);
    AC97SetVolumeRegister(AC97Registers::SetMasterOutputVolume, AC97_SPEAKER_OUTPUT_NUMBER_OF_VOLUME_STEPS, sound_volume);

    return;
}

void AC97SetVolumeRegister(uint32_t offset, uint8_t number_of_volume_steps, uint8_t volume) {
    if (volume == 0) {
        outw(AC97Device.nam_base + offset, 0x8000); // mute
        return;
    } else {
        volume = ((100 - volume) * number_of_volume_steps / 100); // recalc 0..100 to register scale
        outw(AC97Device.nam_base + offset, ((volume) | (volume << 8))); // L=R
        return;
    }
}

void AC97SetSampleRate(uint16_t sample_rate) {
    if ((AC97Device.extended_capabilities & AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) == AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) {
        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_frontSampleRate, sample_rate);
        // Keeping your broader writes (harmless); front DAC alone is sufficient for simple playback
        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_surrSampleRate, sample_rate);
        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_lfeSampleRate, sample_rate);
        outw(AC97Device.nam_base + AC97Registers::PCM_DAC_lrSampleRate, sample_rate);
        return;
    }
}

/* ------------------------------ PLAYBACK --------------------------------- */

void AC97PlayData(uint32_t sample_rate) {
    uint16_t nabm_base = AC97Device.nabm_base;

    /* Program sample rate first */
    AC97SetSampleRate((uint16_t)sample_rate);

    /* Reset OUTPUT stream */
    ac97_reset_stream(nabm_base, AC97Registers::PCMOutputRegisterBox);

    /* Clear BDL memory (32 entries) */
    clear_memory((uint32_t)AC97Device.buffer, sizeof(struct AC97BufferEntry) * 32);

    /* Program BDBAR with PHYSICAL address of BDL */
    outl(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_BDBAR, (uint32_t)AC97Device.buffer_phys);

    /* Build BDL entries from your chorus buffer */
    // We play from pcm_data (virtual) whose physical address must be pcm_data_phys (extern from chorus.h).
    // The hardware expects length in WORDS (16-bit), max 0xFFFE words per descriptor (0x2000 words commonly used).
    uint32_t sound_phys = (uint32_t)pcm_data_phys; // MUST be physical
    uint32_t bytes_left = (sound_buffer_refilling_info->buffer_size * 2); // your code does "*2" – keeping your scheme

    struct AC97BufferEntry* bdl = (struct AC97BufferEntry*)AC97Device.buffer;
    uint8_t last_index = 0;

    for (uint32_t i = 0; i < 32; i++) {
        if (bytes_left == 0) { last_index = (i == 0) ? 0 : (uint8_t)(i - 1); break; }

        uint32_t this_bytes = bytes_left;
        const uint32_t max_bytes_per_bd = 0x2000 * 2; // 0x2000 words => 0x4000 bytes. Your original logic used 0x2000*2; keep same ceiling.
        if (this_bytes > max_bytes_per_bd) this_bytes = max_bytes_per_bd;

        // length in 16-bit words, must be even & <= 0xFFFE
        uint16_t words = (uint16_t)((this_bytes / 2) & 0xFFFE);
        if (words == 0) { last_index = (i == 0) ? 0 : (uint8_t)(i - 1); break; }

        bdl[i].sample_memory = sound_phys;
        bdl[i].number_of_samples = words;
        bdl[i].interrupt_on_completion = 1;
        bdl[i].last_buffer_entry = 0; // set last after loop

        sound_phys += (uint32_t)(words * 2);
        bytes_left -= (uint32_t)(words * 2);
        last_index = (uint8_t)i;

        if (bytes_left == 0) break;
    }

    // mark the last descriptor
    bdl[last_index].last_buffer_entry = 1;

    /* Program LVI = last valid index (0..31) */
    outb(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_LVI, last_index);

    /* Clear OUTPUT status (write-1-to-clear) */
    outw(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_SR, 0x1C);

    /* Start OUTPUT RUN */
    outb(nabm_base + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_CR, 0x01);
}

bool AC97IsSupportedSampleRate(uint16_t sample_rate) {
    if (sample_rate == 48000) {
        return true;
    } else if ((AC97Device.extended_capabilities & AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) == AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE) {
        if (sample_rate == 44100) {
            return true;
        } else {
            outw(AC97Device.nam_base + AC97Registers::PCM_DAC_frontSampleRate, sample_rate);
            return inw(AC97Device.nam_base + AC97Registers::PCM_DAC_frontSampleRate) == sample_rate;
        }
    } else {
        return false;
    }
}


/*
// --- Single-shot beep to isolate driver vs. mixer vs. QEMU ---
static void fill_square_440(uint8_t* buf, uint32_t bytes, uint32_t sample_rate) {
    int16_t* s = (int16_t*)buf;
    uint32_t frames = bytes / 4; // 2ch * 16-bit
    uint32_t period = sample_rate / 440; // ~109 at 48k
    int16_t hi = 0x3FFF, lo = -0x3FFF;
    for (uint32_t i = 0; i < frames; i++) {
        int16_t v = ((i % period) < (period/2)) ? hi : lo;
        s[2*i + 0] = v; // L
        s[2*i + 1] = v; // R
    }
}

// Force playback of a 1-second tone using our own DMA buffers.
void AC97_BeepSmokeTest() {
    const uint32_t sample_rate = 48000;
    const uint32_t bytes_total = sample_rate * 4; // 48k * (stereo 16-bit) = 192000 bytes
    const uint32_t nabm = AC97Device.nabm_base;

    // 1) Program sample rate & reset output engine
    AC97SetSampleRate(sample_rate);
    // reset PCM OUT box
    {
        const uint16_t CR = nabm + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_CR;
        outb(CR, 0x00);
        outb(CR, 0x02);
        int t=0; while ((inb(CR) & 0x02) && t++ < 100) timer_wait(1);
    }

    // 2) Allocate a DMA buffer for PCM and a DMA buffer for BDL
    dma_buffer_t pcm  = dma_alloc(bytes_total, 16);
    dma_buffer_t bdlm = dma_alloc(sizeof(AC97BufferEntry) * 32, 16);
    if (!pcm.virt || !pcm.phys || !bdlm.virt || !bdlm.phys) {
        serial_write_string("AC97_BeepSmokeTest: dma_alloc failed.\n");
        return;
    }

    // 3) Generate the tone into the PCM buffer
    fill_square_440((uint8_t*)pcm.virt, bytes_total, sample_rate);

    // 4) Build BDL entries (split in chunks that fit the controller)
    //    Spec allows up to 0xFFFE words (131070 bytes) per descriptor; we'll use 0x4000 bytes per BD for safety.
    clear_memory((uint32_t)bdlm.virt, sizeof(AC97BufferEntry) * 32);
    AC97BufferEntry* bdl = (AC97BufferEntry*)bdlm.virt;

    uint32_t bytes_left = bytes_total;
    uint32_t phys_ptr   = (uint32_t)pcm.phys;
    const uint32_t MAX_BYTES = 0x4000; // 16 KiB per BD

    uint8_t last = 0;
    for (uint8_t i = 0; i < 32 && bytes_left; i++) {
        uint32_t chunk = (bytes_left > MAX_BYTES) ? MAX_BYTES : bytes_left;
        uint16_t words = (uint16_t)((chunk / 2) & 0xFFFE);
        if (words == 0) break;

        bdl[i].sample_memory = phys_ptr;
        bdl[i].number_of_samples = words;
        bdl[i].interrupt_on_completion = 1;
        bdl[i].last_buffer_entry = 0;

        phys_ptr   += (uint32_t)words * 2;
        bytes_left -= (uint32_t)words * 2;
        last = i;
    }
    bdl[last].last_buffer_entry = 1;

    // 5) Point BDBAR to BDL (PHYS), set LVI, clear SR, then RUN
    outl(nabm + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_BDBAR, (uint32_t)bdlm.phys);
    outb(nabm + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_LVI, last);
    outw(nabm + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_SR, 0x1C);
    outb(nabm + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_CR, 0x01);

    serial_write_string("AC97_BeepSmokeTest: RUN set. Watching PICB...\n");

    // 6) Poll PICB (bytes remaining in current BD) for ~1.5s to see it count down
    for (int i=0; i<1500; i++) {
        uint16_t picb = inw(nabm + AC97Registers::PCMOutputRegisterBox + AC97Registers::PCMOut_PICB);
        (void)picb; // you can print it if you want
        timer_wait(1);
    }

    serial_write_string("AC97_BeepSmokeTest: done polling.\n");
}
*/