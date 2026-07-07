#include "./mp3.h"
#include "./chorus.h"
#include "../serial/serial.h"
#include "../../ports/minimp3/minimp3.h"

static struct mp3_info_t *actually_played_mp3_info;

static mp3dec_t mp3_playback_decoder;
static int16_t  mp3_carry_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int      mp3_carry_frame_channels;
static int      mp3_carry_frames_remaining;
static int      mp3_carry_read_offset;

struct mp3_info_t *read_mp3_info(uint8_t *mp3_memory, uint32_t mp3_length)
{
    struct mp3_info_t *mp3_info = (struct mp3_info_t *)calloc(1, sizeof(struct mp3_info_t));
    if (!mp3_info) {
        return NULL;
    }

    mp3dec_t scan_decoder;
    mp3dec_init(&scan_decoder);

    uint8_t  *scan_pointer = mp3_memory;
    uint32_t  scan_remaining = mp3_length;
    uint32_t  total_native_frames = 0;
    bool      have_format = false;

    while (scan_remaining > 0)
    {
        mp3dec_frame_info_t frame_info;
        // pcm==NULL makes minimp3 skip Huffman/IMDCT/synthesis and just
        // return the frame's header-declared sample count, so this prescan
        // only pays for header parsing instead of a full second decode pass.
        int samples = mp3dec_decode_frame(&scan_decoder, scan_pointer, (int)scan_remaining, NULL, &frame_info);

        if (frame_info.frame_bytes == 0) {
            break;
        }

        scan_pointer += frame_info.frame_bytes;
        scan_remaining -= frame_info.frame_bytes;

        if (samples > 0)
        {
            if (!have_format)
            {
                mp3_info->pcm_data_number_of_channels = (uint8_t)frame_info.channels;
                mp3_info->pcm_data_sample_rate = (uint32_t)frame_info.hz;
                have_format = true;
            }
            total_native_frames += (uint32_t)samples;
        }
    }

    if (!have_format || total_native_frames == 0)
    {
        serial_write_string("\nMP3: no decodable audio frames found");
        free(mp3_info);
        return NULL;
    }

    // recalculate what sample rate we will use, same table as read_wav_info
    mp3_info->output_pcm_data_sample_rate = 0;
    if (mp3_info->pcm_data_sample_rate == 48000 || mp3_info->pcm_data_sample_rate == 24000 || mp3_info->pcm_data_sample_rate == 12000)
    {
        mp3_info->output_pcm_data_sample_rate = 48000;
    }
    else if (mp3_info->pcm_data_sample_rate == 44100 || mp3_info->pcm_data_sample_rate == 22050 || mp3_info->pcm_data_sample_rate == 11025)
    {
        mp3_info->output_pcm_data_sample_rate = 44100;
    }

    if (mp3_info->output_pcm_data_sample_rate == 0)
    {
        serial_write_string("\nMP3: unsupported sample rate");
        free(mp3_info);
        return NULL;
    }

    uint32_t rate_ratio = mp3_info->output_pcm_data_sample_rate / mp3_info->pcm_data_sample_rate;
    if (rate_ratio == 0) {
        rate_ratio = 1;
    }

    mp3_info->start_of_mp3_data = (uint32_t)mp3_memory;
    mp3_info->length_of_mp3_data = mp3_length;
    mp3_info->length_of_output_pcm_data = total_native_frames * rate_ratio * 4; // forced stereo, 16-bit

    return mp3_info;
}

uint32_t play_mp3(struct mp3_info_t *mp3_info, uint32_t offset_to_first_uint8_to_play)
{
    actually_played_mp3_info = mp3_info;

    mp3dec_init(&mp3_playback_decoder);
    mp3_carry_frame_channels = 0;
    mp3_carry_frames_remaining = 0;
    mp3_carry_read_offset = 0;

    // MP3 frames are variable-length/compressed, so mid-stream seeking by
    // decoded-PCM byte offset isn't supported: playback always starts from
    // the beginning of the source, only the target total length is trimmed.
    play_sound_with_refilling_buffer(
        (uint8_t *)mp3_info->start_of_mp3_data,
        mp3_info->length_of_mp3_data,
        mp3_info->length_of_output_pcm_data - offset_to_first_uint8_to_play,
        mp3_info->output_pcm_data_sample_rate,
        mp3_info->output_pcm_data_sample_rate * 4,
        mp3_refill_buffer
    );

    return 0;
}

void mp3_refill_buffer(uint8_t *buffer)
{
    clear_memory((uint32_t)buffer, sound_buffer_refilling_info->buffer_size);

    if (!actually_played_mp3_info) {
        return;
    }

    uint32_t rate_ratio =
        actually_played_mp3_info->output_pcm_data_sample_rate /
        actually_played_mp3_info->pcm_data_sample_rate;

    if (rate_ratio == 0) {
        rate_ratio = 1;
    }

    uint32_t buffer_size = sound_buffer_refilling_info->buffer_size;
    uint32_t bytes_written = 0;

    while (bytes_written < buffer_size)
    {
        if (mp3_carry_frames_remaining == 0)
        {
            if (sound_buffer_refilling_info->source_data_length == 0) {
                break;
            }

            mp3dec_frame_info_t frame_info;
            int samples = mp3dec_decode_frame(
                &mp3_playback_decoder,
                sound_buffer_refilling_info->source_data_pointer,
                (int)sound_buffer_refilling_info->source_data_length,
                mp3_carry_pcm,
                &frame_info
            );

            if (frame_info.frame_bytes == 0) {
                sound_buffer_refilling_info->source_data_length = 0;
                break;
            }

            sound_buffer_refilling_info->source_data_pointer += frame_info.frame_bytes;
            sound_buffer_refilling_info->source_data_length -= frame_info.frame_bytes;

            if (samples == 0) {
                continue; // frame consumed (e.g. junk/tag data) but produced no audio
            }

            mp3_carry_frame_channels = frame_info.channels;
            mp3_carry_frames_remaining = samples;
            mp3_carry_read_offset = 0;
        }

        int16_t first_channel_sample = mp3_carry_pcm[mp3_carry_read_offset * mp3_carry_frame_channels];
        int16_t second_channel_sample = (mp3_carry_frame_channels == 2)
            ? mp3_carry_pcm[mp3_carry_read_offset * mp3_carry_frame_channels + 1]
            : first_channel_sample;

        for (uint32_t j = 0; j < rate_ratio && bytes_written < buffer_size; j++)
        {
            buffer[0] = (uint8_t)(first_channel_sample & 0xFF);
            buffer[1] = (uint8_t)((first_channel_sample >> 8) & 0xFF);
            buffer[2] = (uint8_t)(second_channel_sample & 0xFF);
            buffer[3] = (uint8_t)((second_channel_sample >> 8) & 0xFF);
            buffer += 4;
            bytes_written += 4;
        }

        mp3_carry_read_offset++;
        mp3_carry_frames_remaining--;
    }
}

/*
 * Boot-time smoke test (previously lived in p-kernel.cpp as
 * test_mp3_playback). Not wired into kernel_main right now -- kept here as
 * a reference for how to load an MP3 off the boot floppy and play it
 * through the AC97 driver. `read_file` is the same typedef kernel_main
 * uses: `typedef uint32_t (*read_file)(const char* filename, uint8_t* dest);`
 *
static void test_mp3_playback(read_file load_floppy) {
    uint8_t* lowmem_floppy = (uint8_t*)0x100000;
    disablePaging();
    uint32_t lengthFloppyBuffer = load_floppy("TEST.MP3", lowmem_floppy);
    enablePaging();
    if (lengthFloppyBuffer == 0) {
        serial_write_string("Failed to load TEST.MP3\n", false, FAIL);
        return;
    }
    char* floppyBuffer = malloc(lengthFloppyBuffer);
    if (!floppyBuffer) {
        serial_write_string("Failed to allocate MP3 buffer\n", false, FAIL);
        return;
    }
    memcpy(floppyBuffer, lowmem_floppy, lengthFloppyBuffer);
    initalize();
    struct mp3_info_t* mp3_info = read_mp3_info((uint8_t*)floppyBuffer, lengthFloppyBuffer);
    if (!mp3_info) {
        serial_write_string("Failed to parse MP3 file\n", false, FAIL);
        free(floppyBuffer);
        return;
    } else {
        char* string_alloc = malloc(512);
        sprintf(
            string_alloc,
            "MP3_AudioFile {\n\t\"length_of_mp3_data\": %d,\n\t\"pcm_data_number_of_channels\": %d,\n\t\"pcm_data_sample_rate\": %d,\n\t\"length_of_output_pcm_data\": %d,\n\t\"output_pcm_data_sample_rate\": %d\n}\n",
            mp3_info->length_of_mp3_data,
            mp3_info->pcm_data_number_of_channels,
            mp3_info->pcm_data_sample_rate,
            mp3_info->length_of_output_pcm_data,
            mp3_info->output_pcm_data_sample_rate
        );
        serial_write_string(string_alloc, false, NONE);
        free(string_alloc);
    }
    play_mp3(mp3_info, 0);
    while (AC97IsPlaying()) {
        asm volatile("hlt");
    }
    free(floppyBuffer);
    serial_write_string("AC97 Audio Codec test has ended. MP3 buffer freed.\n", false, NONE);
}
*/
