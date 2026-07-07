#ifndef __CHORUS__
#define __CHORUS__ 1

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Size: 4 seconds @ 48kHz, stereo, 16-bit: 4 * 48000 * 4 = 768000 bytes */
#define SIZE_OF_PCM_DATA_BUFFER (4 * 48000 * 4)

#define SOUND_BUFFER_0 0
#define SOUND_BUFFER_1 1

struct sound_buffer_refilling_info_t {
    uint8_t  *source_data_pointer;
    uint32_t  source_data_length;
    void    (*fill_buffer)(uint8_t *buffer);
    uint32_t  buffer_size;
    uint8_t  *buffer_0_pointer;
    uint8_t  *buffer_1_pointer;
    uint8_t   actually_playing_buffer : 4;
    uint8_t   last_filled_buffer      : 4;
    uint32_t  played_bytes_by_finished_buffers;
    uint32_t  played_bytes;
    uint32_t  size_of_full_pcm_output_in_bytes;
} __attribute__((packed));

/* Exported globals used by AC97 driver */
extern struct sound_buffer_refilling_info_t *sound_buffer_refilling_info;
extern uint8_t  *pcm_data;       // virtual address (CPU)
extern uint32_t  pcm_data_phys;  // physical address (DMA)

/* API */
void chorus_initalize(void);
void clear_memory(uint32_t memory, uint32_t length);

/**
 * Begin playback of a (possibly streaming) buffer using a double-buffer scheme.
 * - source_data_pointer/source_data_length: your audio payload (optional for generators)
 * - size_of_full_pcm_output_in_bytes: total bytes you intend to play (can equal source_data_length)
 * - sample_rate: e.g. 48000
 * - size_of_buffer: size of each half (buffer_0 / buffer_1)
 * - fill_buffer: callback that writes 'size_of_buffer' bytes into the provided buffer
 */
void play_sound_with_refilling_buffer(
    uint8_t *source_data_pointer,
    uint32_t source_data_length,
    uint32_t size_of_full_pcm_output_in_bytes,
    uint32_t sample_rate,
    uint32_t size_of_buffer,
    void (*fill_buffer)(uint8_t *buffer)
);

#endif /* __CHORUS__ */
