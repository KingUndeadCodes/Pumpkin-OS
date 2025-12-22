/**
 * This is the Chorus Audio API.
 * Provides a DMA-backed PCM buffer for the AC’97 driver.
 */

#include "./chorus.h"
#include "../pci/drivers/ac97.h"
#include "../serial/serial.h"
#include "../memory/allocator.h"

/* Globals (exported in header) */
struct sound_buffer_refilling_info_t *sound_buffer_refilling_info = nullptr;
uint8_t  *pcm_data       = nullptr;  // virtual
uint32_t  pcm_data_phys  = 0;        // physical (DMA)

/* --- helpers ------------------------------------------------------------- */

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

/* --- init ---------------------------------------------------------------- */

void initalize(void) {
    /* Control struct */
    sound_buffer_refilling_info =
        (struct sound_buffer_refilling_info_t *)malloc(sizeof(struct sound_buffer_refilling_info_t));

    /* Allocate one big DMA region for both halves (0 + 1) */
    dma_buffer_t pcm_allocation = dma_alloc(SIZE_OF_PCM_DATA_BUFFER, 16);
    if (pcm_allocation.virt == nullptr || pcm_allocation.phys == 0) {
        serial_write_string("chorus: dma_alloc failed for PCM buffer.\n");
        return;
    }

    pcm_data      = (uint8_t *)pcm_allocation.virt;
    pcm_data_phys = (uint32_t)pcm_allocation.phys;

    /* Clear initial buffer */
    clear_memory((uint32_t)pcm_data, SIZE_OF_PCM_DATA_BUFFER);

    serial_write_string("chorus: initialized DMA PCM buffer.\n");
}

/* --- playback ------------------------------------------------------------ */

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

    /* Set up refilling info */
    sound_buffer_refilling_info->source_data_pointer  = source_data_pointer;
    sound_buffer_refilling_info->source_data_length   = source_data_length;
    sound_buffer_refilling_info->fill_buffer          = fill_buffer;
    sound_buffer_refilling_info->buffer_size          = size_of_buffer;
    sound_buffer_refilling_info->buffer_0_pointer     = pcm_data;
    sound_buffer_refilling_info->buffer_1_pointer     = pcm_data + size_of_buffer;
    sound_buffer_refilling_info->actually_playing_buffer = SOUND_BUFFER_0;
    sound_buffer_refilling_info->last_filled_buffer      = SOUND_BUFFER_0;
    sound_buffer_refilling_info->played_bytes_by_finished_buffers = 0;
    sound_buffer_refilling_info->played_bytes          = 0;
    sound_buffer_refilling_info->size_of_full_pcm_output_in_bytes = size_of_full_pcm_output_in_bytes;

    /* Clear the whole DMA region before first fill */
    clear_memory((uint32_t)pcm_data, SIZE_OF_PCM_DATA_BUFFER);
    // serial_write_string("chorus: cleared PCM buffer; filling first half...\n");

    /* Preload first half so AC97 has data immediately */
    if (fill_buffer) {
        (*fill_buffer)(sound_buffer_refilling_info->buffer_0_pointer);
    }

    // serial_write_string("chorus: first half filled; starting AC97 playback.\n");

    /* Kick the AC’97 ring — AC97PlayData will build the BDL from pcm_data_phys */
    AC97PlayData(sample_rate);

    // serial_write_string("chorus: AC97PlayData returned (DMA started or finished).\n");
}