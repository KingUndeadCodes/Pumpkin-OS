#ifndef __AC97_H__
#define __AC97_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* AC97 Controller and Buffer Structures                                      */
/* -------------------------------------------------------------------------- */

typedef struct {
    /* 0x00 */ uint32_t bufferDescriptorListPhysicalAddress;
    /* 0x04 */ uint8_t  actualProcessedBufferDescriptorNumber;
    /* 0x05 */ uint8_t  descriptorEntryCount;
    /* 0x06 */ uint16_t status;
} __attribute__((packed)) NativeAudioBusMasterRegisterBox;

struct AC97 {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint32_t nam_base;      // mixer I/O base
    uint32_t nabm_base;     // bus master I/O base
    uint8_t* buffer;        // virtual pointer to BDL
    uintptr_t buffer_phys;  // physical address of BDL
    uint8_t  aux_out_number_of_volume_steps;
    uint16_t extended_capabilities;
} __attribute__((packed));

/* Buffer Descriptor entry */
struct AC97BufferEntry {
    uint32_t sample_memory;         // physical address of PCM buffer
    uint16_t number_of_samples;     // length in 16-bit words
    uint16_t reserved : 14;
    uint8_t  last_buffer_entry : 1;
    uint8_t  interrupt_on_completion : 1;
} __attribute__((packed));

/* -------------------------------------------------------------------------- */
/* Register Offsets                                                           */
/* -------------------------------------------------------------------------- */

namespace AC97Registers {

    /* BAR0: Native Audio Mixer Registers */
    enum NativeAudioMixerRegisters : uint8_t {
        ResetRegisterCapabilities   = 0x00,
        SetMasterOutputVolume       = 0x02,
        SetAUXOutputVolume          = 0x04,
        SetMicrophoneVolume         = 0x0E,
        SetPCMOutputVolume          = 0x18,
        SetInputDevice              = 0x1A,
        SetInputGain                = 0x1C,
        SetMicrophoneGain           = 0x1E,
        ExtendedCapabilities        = 0x28,
        ExtendedCapabilitiesControl = 0x2A,
        PCM_DAC_frontSampleRate     = 0x2C,
        PCM_DAC_surrSampleRate      = 0x2E,
        PCM_DAC_lfeSampleRate       = 0x30,
        PCM_DAC_lrSampleRate        = 0x32
    };

    /* BAR1: Native Audio Bus Master Registers (boxes) */
    enum NativeAudioBusMasterRegisters : uint8_t {
        PCMInputRegisterBox         = 0x00,
        PCMOutputRegisterBox        = 0x10,
        MicrophoneRegisterBox       = 0x20,
        GlobalControlRegister       = 0x2C,
        GlobalStatusRegister        = 0x30
    };

    /* Registers inside each Bus Master box */
    enum NativeAudioBusMasterBoxRegisters : uint8_t {
        PCMOut_BDBAR                = 0x00,
        PCMOut_CIV                  = 0x04,
        PCMOut_LVI                  = 0x05,
        PCMOut_SR                   = 0x06,
        PCMOut_PICB                 = 0x08,
        PCMOut_CR                   = 0x0B,

        PCMIn_BDBAR                 = 0x10,
        PCMIn_CIV                   = 0x14,
        PCMIn_LVI                   = 0x15,
        PCMIn_SR                    = 0x16,
        PCMIn_PICB                  = 0x18,
        PCMIn_CR                    = 0x1B,

        Mic_BDBAR                   = 0x20,
        Mic_CIV                     = 0x24,
        Mic_LVI                     = 0x25,
        Mic_SR                      = 0x26,
        Mic_PICB                    = 0x28,
        Mic_CR                      = 0x2B,

        GlobalControl               = 0x2C,
        GlobalStatus                = 0x30
    };
}

/* -------------------------------------------------------------------------- */
/* Constants                                                                  */
/* -------------------------------------------------------------------------- */

#define AC97_EXTENDED_CAPABILITY_VARIABLE_SAMPLE_RATE 0x0001
#define AC97_SPEAKER_OUTPUT_NUMBER_OF_VOLUME_STEPS    31

/* -------------------------------------------------------------------------- */
/* Function Declarations                                                      */
/* -------------------------------------------------------------------------- */

void AC97_INIT(uint8_t bus, uint8_t device, uint8_t function);
void AC97SetVolumeRegister(uint32_t offset, uint8_t number_of_volume_steps, uint8_t volume);
void AC97SetSampleRate(uint16_t sample_rate);
void AC97PlayData(uint32_t sample_rate);
bool AC97IsSupportedSampleRate(uint16_t sample_rate);

// void AC97_BeepSmokeTest();

#endif /* __AC97_H__ */
