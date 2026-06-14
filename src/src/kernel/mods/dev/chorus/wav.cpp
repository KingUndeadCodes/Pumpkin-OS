// BleskOS

#include "./wav.h"
#include "./chorus.h"
#include "../serial/serial.h"

// https://github.com/VendelinSlezak/BleskOS/blob/master/source/libraries/sound_formats/wav.c

void copy_memory(uint32_t source_memory, uint32_t destination_memory, uint32_t size)
{
    uint32_t *source32 = (uint32_t *)(source_memory);
    uint32_t *destination32 = (uint32_t *)(destination_memory);

    for (uint32_t i = 0; i < (size / 4); i++)
    {
        *destination32 = *source32;
        destination32++;
        source32++;
    }

    uint8_t *source8 = (uint8_t *)(source32);
    uint8_t *destination8 = (uint8_t *)(destination32);

    for (uint32_t i = 0; i < (size % 4); i++)
    {
        *destination8 = *source8;
        destination8++;
        source8++;
    }
}


uint32_t convert_wav_to_sound_data(uint32_t wav_memory, uint32_t wav_length)
{
    uint32_t *wav32 = (uint32_t *)wav_memory;
    uint32_t channels = 0, bits_per_sample = 0, sample_rate = 0, sound_memory = 0;
    wav_length += wav_memory;

    // check signature
    if (wav32[0] != 0x46464952 && wav32[2] != 0x45564157)
    {
        serial_write_string("\nWAV: invalid signature");
        return STATUS_ERROR;
    }

    // parse chunks
    wav32 = (uint32_t *)(wav_memory + 12);
    while ((uint32_t)wav32 < wav_length && wav32[0] != 0x00000000)
    {
        if (wav32[0] == 0x20746D66)
        { // fmt
            // check sound format
            if ((wav32[2] & 0xFFFF) != WAV_FORMAT_PCM)
            {
                serial_write_string("\nWAV: not raw PCM");
                return STATUS_ERROR;
            }

            // read other data about sound
            channels = (wav32[2] >> 16);
            sample_rate = wav32[3];
            bits_per_sample = (wav32[5] >> 16);
        }
        /*
        else if (wav32[0] == 0x61746164)
        { // data
            if (channels == 0 || sample_rate == 0 || bits_per_sample == 0)
            {
                serial_write_string("\nWAV: data chunk before fmt chunk");
                return STATUS_ERROR;
            }

            // convert PCM data
            uint32_t converted_pcm_data_memory = convert_pcm_to_2_channels_16_bit_samples_48000_44100_sample_rate(((uint32_t)wav32 + 8), wav32[1], channels, bits_per_sample, sample_rate);

            // copy sound data
            if (converted_pcm_data_memory == STATUS_ERROR)
            {
                sound_memory = create_sound(channels, bits_per_sample, sample_rate, wav32[1]);
                copy_memory(((uint32_t)wav32 + 8), (get_sound_data_memory(sound_memory)), wav32[1]);
            }
            else
            {
                sound_memory = create_sound(2, 16, converted_pcm_data_sample_rate, converted_pcm_data_length);
                copy_memory(converted_pcm_data_memory, (get_sound_data_memory(sound_memory)), converted_pcm_data_length);
                free((void *)converted_pcm_data_memory);
            }

            return sound_memory;
        }
        */
        else
        {
            serial_write_string("\nWAV: unknown chunk 0x%x", wav32[0]);
        }

        // go to next chunk
        wav32 = (uint32_t*)((uint32_t)wav32 + wav32[1] + 8);
    }

    return STATUS_ERROR;
}

struct wav_info_t *read_wav_info(uint8_t *wav_memory, uint32_t wav_length)
{
    struct wav_info_t *wav_info = (struct wav_info_t *)(/* malloc */calloc(1, sizeof(struct wav_info_t)));

    uint32_t *wav32 = (uint32_t *)wav_memory;
    uint32_t end_of_wav = ((uint32_t)wav_memory + wav_length);

    // check signature
    if (wav32[0] != 0x46464952 && wav32[2] != 0x45564157)
    {
        serial_write_string("\nWAV: invalid signature");
        return STATUS_ERROR;
    }

    // parse chunks
    wav32 = (uint32_t*)((uint32_t)wav_memory + 12);
    while ((uint32_t)wav32 < end_of_wav && wav32[0] != 0x00000000)
    {
        if (wav32[0] == 0x20746D66)
        { // fmt
            // check sound format
            if ((wav32[2] & 0xFFFF) != WAV_FORMAT_PCM)
            {
                serial_write_string("\nWAV: not raw PCM");
                return STATUS_ERROR;
            }

            // read PCM data format
            wav_info->pcm_data_number_of_channels = (wav32[2] >> 16);
            wav_info->pcm_data_sample_rate = wav32[3];
            wav_info->pcm_data_bits_per_sample = (wav32[5] >> 16);

            // recalculate what sample rate we will use
            wav_info->output_pcm_data_sample_rate = 0;
            if (wav_info->pcm_data_sample_rate == 48000 || wav_info->pcm_data_sample_rate == 24000 || wav_info->pcm_data_sample_rate == 12000)
            {
                wav_info->output_pcm_data_sample_rate = 48000;
            }
            else if (wav_info->pcm_data_sample_rate == 44100 || wav_info->pcm_data_sample_rate == 22050 || wav_info->pcm_data_sample_rate == 11025)
            {
                wav_info->output_pcm_data_sample_rate = 44100;
            }
        }
        else if (wav32[0] == 0x61746164)
        { // data
            if (wav_info->pcm_data_number_of_channels == 0 || wav_info->pcm_data_sample_rate == 0 || wav_info->pcm_data_bits_per_sample == 0)
            {
                serial_write_string("\nWAV: data chunk before fmt chunk");
                return STATUS_ERROR;
            }

            // read info about PCM data
            wav_info->start_of_pcm_data = ((uint32_t)wav32 + 8);
            wav_info->length_of_pcm_data = wav32[1];
            if (wav_info->output_pcm_data_sample_rate != 0 && (wav_info->pcm_data_bits_per_sample == 8 || wav_info->pcm_data_bits_per_sample == 16))
            {
                // recalculate length for 2 channels
                wav_info->length_of_output_pcm_data = ((wav_info->length_of_pcm_data / wav_info->pcm_data_number_of_channels) * 2);

                // recalculate according 16 bits per sample
                if (wav_info->pcm_data_bits_per_sample == 8)
                {
                    wav_info->length_of_output_pcm_data *= 2;
                }

                // recalculate according to new sample rate
                wav_info->length_of_output_pcm_data = (wav_info->length_of_output_pcm_data * (wav_info->output_pcm_data_sample_rate / wav_info->pcm_data_sample_rate));
            }
            else
            {
                serial_write_string("Unplayable WAV: 41");
                // serial_write_string("Unplayable WAV: %d %d %d", wav_info->pcm_data_number_of_channels, wav_info->pcm_data_sample_rate, wav_info->pcm_data_bits_per_sample);
                wav_info->length_of_output_pcm_data = 0;
                return STATUS_ERROR;
            }

            return wav_info;
        }
        else
        {
            serial_write_string("\nWAV: unknown chunk 0x%x", wav32[0]);
        }

        // go to next chunk
        wav32 = (uint32_t *)((uint32_t)wav32 + wav32[1] + 8);
    }

    return STATUS_ERROR;
}

uint32_t play_wav(struct wav_info_t *wav_info, uint32_t offset_to_first_uint8_to_play)
{
    uint32_t rate_ratio = wav_info->output_pcm_data_sample_rate / wav_info->pcm_data_sample_rate;
    if (rate_ratio == 0) {
        rate_ratio = 1;
    }

    uint32_t input_bytes_per_frame =
        wav_info->pcm_data_number_of_channels * (wav_info->pcm_data_bits_per_sample / 8);

    uint32_t offset_to_first_uint8_to_pcm_data_in_original_wav =
        ((offset_to_first_uint8_to_play / 4) / rate_ratio) * input_bytes_per_frame;

    actually_played_wav_info = wav_info;

    play_sound_with_refilling_buffer(
        (uint8_t *)wav_info->start_of_pcm_data + offset_to_first_uint8_to_pcm_data_in_original_wav,
        wav_info->length_of_pcm_data - offset_to_first_uint8_to_pcm_data_in_original_wav,
        wav_info->length_of_output_pcm_data - offset_to_first_uint8_to_play,
        wav_info->output_pcm_data_sample_rate,
        wav_info->output_pcm_data_sample_rate * 4,
        wav_refill_buffer
    );

    return 0;
}

void wav_refill_buffer(uint8_t *buffer)
{
    clear_memory((uint32_t)buffer, sound_buffer_refilling_info->buffer_size);

    if (!actually_played_wav_info || sound_buffer_refilling_info->source_data_length == 0) {
        return;
    }

    uint32_t input_bytes_per_frame =
        actually_played_wav_info->pcm_data_number_of_channels *
        (actually_played_wav_info->pcm_data_bits_per_sample / 8);

    if (input_bytes_per_frame == 0) {
        return;
    }

    uint32_t rate_ratio =
        actually_played_wav_info->output_pcm_data_sample_rate /
        actually_played_wav_info->pcm_data_sample_rate;

    if (rate_ratio == 0) {
        rate_ratio = 1;
    }

    uint32_t output_frames_fit = sound_buffer_refilling_info->buffer_size / 4;
    uint32_t source_frames_fit = output_frames_fit / rate_ratio;
    uint32_t source_frames_available =
        sound_buffer_refilling_info->source_data_length / input_bytes_per_frame;

    uint32_t number_of_frames_to_convert = source_frames_fit;
    if (number_of_frames_to_convert > source_frames_available) {
        number_of_frames_to_convert = source_frames_available;
    }

    for (uint32_t i = 0; i < number_of_frames_to_convert; i++)
    {
        int16_t first_channel_sample = 0;
        int16_t second_channel_sample = 0;

        if (actually_played_wav_info->pcm_data_bits_per_sample == 16)
        {
            int16_t *wav16 = (int16_t *)(sound_buffer_refilling_info->source_data_pointer);
            first_channel_sample = wav16[0];

            if (actually_played_wav_info->pcm_data_number_of_channels == 2) {
                second_channel_sample = wav16[1];
            } else {
                second_channel_sample = first_channel_sample;
            }
        }
        else if (actually_played_wav_info->pcm_data_bits_per_sample == 8)
        {
            uint8_t *wav8 = (uint8_t *)(sound_buffer_refilling_info->source_data_pointer);
            first_channel_sample = (int16_t)((int16_t)wav8[0] - 128) << 8;

            if (actually_played_wav_info->pcm_data_number_of_channels == 2) {
                second_channel_sample = (int16_t)((int16_t)wav8[1] - 128) << 8;
            } else {
                second_channel_sample = first_channel_sample;
            }
        }

        for (uint32_t j = 0; j < rate_ratio; j++)
        {
            buffer[0] = (uint8_t)(first_channel_sample & 0xFF);
            buffer[1] = (uint8_t)((first_channel_sample >> 8) & 0xFF);
            buffer[2] = (uint8_t)(second_channel_sample & 0xFF);
            buffer[3] = (uint8_t)((second_channel_sample >> 8) & 0xFF);
            buffer += 4;
        }

        sound_buffer_refilling_info->source_data_pointer += input_bytes_per_frame;
        sound_buffer_refilling_info->source_data_length -= input_bytes_per_frame;
    }
}

/*
void convert_sound_data_to_wav(uint8_t *pcm_data_pointer, uint32_t size_of_pcm_data_in_bytes, uint8_t bits_per_sample, uint8_t number_of_channels, uint16_t sample_rate)
{
    uint32_t wav_memory = (uint32_t)malloc(size_of_pcm_data_in_bytes + 44);
    uint32_t *wav32 = (uint32_t *)(wav_memory);

    // create info
    wav32[0] = 0x46464952;                                                               //'RIFF'
    wav32[1] = (size_of_pcm_data_in_bytes + 44);                                         // length of WAV file
    wav32[2] = 0x45564157;                                                               //'WAVE'
    wav32[3] = 0x20746D66;                                                               //'fmt '
    wav32[4] = 16;                                                                       // chunk size
    wav32[5] = ((WAV_FORMAT_PCM) | (number_of_channels << 16));                          // sound format and number of channels
    wav32[6] = sample_rate;                                                              // sample rate
    wav32[7] = ((sample_rate * bits_per_sample * number_of_channels) / 8);               // data rate
    wav32[8] = (((bits_per_sample / 8) * number_of_channels) | (bits_per_sample << 16)); // data block size and bits per sample
    wav32[9] = 0x61746164;                                                               //'data'
    wav32[10] = size_of_pcm_data_in_bytes;                                               // length of sound data

    // copy sound data
    // memcpy((uint32_t)pcm_data_pointer, wav_memory + 44, size_of_pcm_data_in_bytes);
    // CORRECT: copy user's pcm_data into the newly allocated wav_memory after the 44-byte header
    // memcpy((void *)(wav_memory + 44), (void *)pcm_data_pointer, size_of_pcm_data_in_bytes);

    copy_memory((uint32_t)pcm_data_pointer, wav_memory+44, size_of_pcm_data_in_bytes);

    // set variables
    converted_file_memory = wav_memory;
    converted_file_size = (size_of_pcm_data_in_bytes + 44);
}
*/