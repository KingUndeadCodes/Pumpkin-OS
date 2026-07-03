#ifndef __MP3_H__
#define __MP3_H__

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

struct mp3_info_t {
    uint32_t start_of_mp3_data;
    uint32_t length_of_mp3_data;
    uint8_t  pcm_data_number_of_channels;   // channels reported by the first decoded frame
    uint32_t pcm_data_sample_rate;          // native rate reported by the first decoded frame
    uint32_t length_of_output_pcm_data;     // number of bytes that sound card will play
    uint16_t output_pcm_data_sample_rate;
} __attribute__((packed));

struct mp3_info_t *read_mp3_info(uint8_t *mp3_memory, uint32_t mp3_length);
uint32_t play_mp3(struct mp3_info_t *mp3_info, uint32_t offset_to_first_uint8_to_play);
void mp3_refill_buffer(uint8_t *buffer);

#endif
