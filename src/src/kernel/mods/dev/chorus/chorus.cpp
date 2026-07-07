/**
 * Chorus Audio API.
 *
 * The AC97 driver now streams using a 32-entry DMA ring. Chorus owns the PCM
 * DMA memory and preloads every ring fragment before AC97 starts.
 */

#include "./chorus.h"
#include "../pci/drivers/ac97.h"
#include "../serial/serial.h"
#include "../memory/allocator.h"

struct sound_buffer_refilling_info_t *sound_buffer_refilling_info = nullptr;
uint8_t  *pcm_data       = nullptr;
uint32_t  pcm_data_phys  = 0;

void clear_memory(uint32_t memory, uint32_t length) {
    uint32_t *mem32 = (uint32_t *)(memory);
    for (uint32_t i = 0; i < (length >> 2); i++) {
        *mem32++ = 0;
    }

    uint8_t *mem8 = (uint8_t *)mem32;
    for (uint32_t i = 0; i < (length & 3u); i++) {
        *mem8++ = 0;
    }
}

void chorus_initalize(void) {
    sound_buffer_refilling_info =
        (struct sound_buffer_refilling_info_t *)malloc(sizeof(struct sound_buffer_refilling_info_t));

    dma_buffer_t pcm_allocation = dma_alloc(SIZE_OF_PCM_DATA_BUFFER, 16);
    if (pcm_allocation.virt == nullptr || pcm_allocation.phys == 0) {
        serial_write_string("chorus: dma_alloc failed for PCM buffer.\n");
        return;
    }

    pcm_data      = (uint8_t *)pcm_allocation.virt;
    pcm_data_phys = (uint32_t)pcm_allocation.phys;

    clear_memory((uint32_t)pcm_data, SIZE_OF_PCM_DATA_BUFFER);

    serial_write_string("chorus: initialized DMA PCM buffer.\n");
}

static uint32_t chorus_clamp_fragment_size(uint32_t requested) {
    uint32_t preferred = AC97GetPreferredBufferSize();

    if (requested == 0 || requested > preferred) {
        requested = preferred;
    }

    requested &= ~3u; // keep stereo 16-bit frame alignment

    if (requested < 4) {
        requested = 4;
    }

    uint32_t max_by_allocation = SIZE_OF_PCM_DATA_BUFFER / AC97_BDL_ENTRY_COUNT;
    max_by_allocation &= ~3u;

    if (requested > max_by_allocation) {
        requested = max_by_allocation;
    }

    return requested;
}

void play_sound_with_refilling_buffer(
    uint8_t *source_data_pointer,
    uint32_t source_data_length,
    uint32_t size_of_full_pcm_output_in_bytes,
    uint32_t sample_rate,
    uint32_t size_of_buffer,
    void (*fill_buffer)(uint8_t *buffer)
) {
    if (!sound_buffer_refilling_info || !pcm_data || pcm_data_phys == 0) {
        serial_write_string("chorus: not initialized; call initalize() first.\n");
        return;
    }

    uint32_t fragment_size = chorus_clamp_fragment_size(size_of_buffer);
    uint32_t ring_size = fragment_size * AC97_BDL_ENTRY_COUNT;

    sound_buffer_refilling_info->source_data_pointer  = source_data_pointer;
    sound_buffer_refilling_info->source_data_length   = source_data_length;
    sound_buffer_refilling_info->fill_buffer          = fill_buffer;
    sound_buffer_refilling_info->buffer_size          = fragment_size;
    sound_buffer_refilling_info->buffer_0_pointer     = pcm_data;
    sound_buffer_refilling_info->buffer_1_pointer     = pcm_data + fragment_size;
    sound_buffer_refilling_info->actually_playing_buffer = SOUND_BUFFER_0;
    sound_buffer_refilling_info->last_filled_buffer      = 0;
    sound_buffer_refilling_info->played_bytes_by_finished_buffers = 0;
    sound_buffer_refilling_info->played_bytes          = 0;
    sound_buffer_refilling_info->size_of_full_pcm_output_in_bytes = size_of_full_pcm_output_in_bytes;

    clear_memory((uint32_t)pcm_data, ring_size);

    /* Preload every descriptor's fragment. The IRQ handler refills them later. */
    for (uint32_t i = 0; i < AC97_BDL_ENTRY_COUNT; i++) {
        uint8_t *fragment = pcm_data + (i * fragment_size);
        if (fill_buffer) {
            fill_buffer(fragment);
        }
        sound_buffer_refilling_info->last_filled_buffer = (uint8_t)i;
    }

    AC97PlayData(sample_rate);
}