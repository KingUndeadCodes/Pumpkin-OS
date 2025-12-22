//BleskOS

/*
* MIT License
* Copyright (c) 2023-2025 BleskOS developers
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __WAV_H__
#define __WAV_H__

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#define WAV_FORMAT_PCM 0x0001
#define STATUS_ERROR 1

struct wav_info_t {
    uint32_t start_of_pcm_data;
    uint32_t length_of_pcm_data;
    uint8_t pcm_data_number_of_channels;
    uint16_t pcm_data_sample_rate;
    uint8_t pcm_data_bits_per_sample;
    uint32_t length_of_output_pcm_data; //number of bytes that sound card will play
    uint16_t output_pcm_data_sample_rate;
}__attribute__((packed));

static struct wav_info_t *actually_played_wav_info;

static uint32_t converted_file_memory, converted_file_size;

uint32_t convert_wav_to_sound_data(uint32_t wav_memory, uint32_t wav_length);
struct wav_info_t *read_wav_info(uint8_t *wav_memory, uint32_t wav_length);
uint32_t play_wav(struct wav_info_t *wav_info, uint32_t offset_to_first_uint8_to_play);
void wav_refill_buffer(uint8_t *buffer);
// void convert_sound_data_to_wav(uint8_t *pcm_data_pointer, uint32_t size_of_pcm_data_in_bytes, uint8_t bits_per_sample, uint8_t number_of_channels, uint16_t sample_rate);

#endif