#ifndef __FLOPPY_H__
#define __FLOPPY_H__ 1

#include "libboot.h"

// https://wiki.osdev.org/Floppy_Disk_Controller#The_Floppy_Subsystem_is_Ugly
#define FLOPPY_REGISTER_STATUS_REGISTER_A 0x3F0
#define FLOPPY_REGISTER_STATUS_REGISTER_B 0x3F1
#define FLOPPY_REGISTER_DIGITAL_OUTPUT_REGISTER 0x3F2
#define FLOPPY_REGISTER_TAPE_DRIVE_REGISTER 0x3F3
#define FLOPPY_REGISTER_MAIN_STATUS_REGISTER 0x3F4
#define FLOPPY_REGISTER_DATARATE_SELECT_REGISTER 0x3F4
#define FLOPPY_REGISTER_DATA_FIFO 0x3F5
#define FLOPPY_REGISTER_DIGITAL_INPUT_REGISTER 0x3F7
#define FLOPPY_REGISTER_CONFIGURATION_CONTROL_REGISTER 0x3F7

#define FLOPPY_COMMAND_READ_TRACK 0x02
#define FLOPPY_COMMAND_SPECIFY 0x03
#define FLOPPY_COMMAND_SENSE_DRIVE_STATUS 0x04
#define FLOPPY_COMMAND_WRITE_DATA 0x05
#define FLOPPY_COMMAND_READ_DATA 0x06
#define FLOPPY_COMMAND_RECALIBRATE 0x07
#define FLOPPY_COMMAND_SENSE_INTERRUPT 0x08
#define FLOPPY_COMMAND_WRITE_DELETED_DATA 0x09
#define FLOPPY_COMMAND_READ_ID 0x0A
#define FLOPPY_COMMAND_READ_DELETED_DATA 0x0C
#define FLOPPY_COMMAND_FORMAT_TRACK 0x0D
#define FLOPPY_COMMAND_DUMP_REGISTERS 0x0E
#define FLOPPY_COMMAND_SEEK 0x0F
#define FLOPPY_COMMAND_VERSION 0x10
#define FLOPPY_COMMAND_SCAN_EQUAL 0x11
#define FLOPPY_COMMAND_PERPENDICULAR_MODE 0x12
#define FLOPPY_COMMAND_CONFIGURE 0x13
#define FLOPPY_COMMAND_LOCK 0x14
#define FLOPPY_COMMAND_VERIFY 0x16
#define FLOPPY_COMMAND_SCAN_LOW_OR_EQUAL 0x19
#define FLOPPY_COMMAND_SCAN_HIGH_OR_EQUAL 0x1D
#define FLOPPY_COMMAND_MT 0x80
#define FLOPPY_COMMAND_LOCK_ON 0x80
#define FLOPPY_COMMAND_MFM 0x40
#define FLOPPY_COMMAND_SK 0x20

// DOR = Digital Output Register
#define DOR_FLAG_MOTD 0x80
#define DOR_FLAG_MOTC 0x40
#define DOR_FLAG_MOTB 0x20
#define DOR_FLAG_MOTA 0x10
#define DOR_FLAG_IRQ 0x08
#define DOR_FLAG_RESET 0x04
#define DOR_FLAG_DSEL1 0x02
#define DOR_FLAG_DSEL0 0x01

#define MSR_FLAGS_RQM 0x80
#define MSR_FLAGS_DIO 0x40
#define MSR_FLAGS_NDMA 0x20
#define MSR_FLAGS_CB 0x10
#define MSR_FLAGS_ACTD 0x08
#define MSR_FLAGS_ACTC 0x04
#define MSR_FLAGS_ACTB 0x02
#define MSR_FLAGS_ACTA 0x01

#define FLOPPY_CONFIG_FLAGS_IMPLIED_SEEK (1 << 6)
#define FLOPPY_CONFIG_FLAGS_FIFO_DISABLE (1 << 5)
#define FLOPPY_CONFIG_FLAGS_POLLING_DISABLE (1 << 4)

#define FLOPPY_DATA_RATE_500KBPS 0x00
#define FLOPPY_DATA_RATE_300KBPS 0x01
#define FLOPPY_DATA_RATE_250KBPS 0x02
#define FLOPPY_DATA_RATE_1000KBPS 0x03

typedef struct Diskinfo {
    uint8_t drive_type;
    uint8_t max_cylinders;
    uint8_t max_sectors;
    uint8_t max_heads;
    uint8_t num_drives;
    uint8_t boot_drive_number;
} Diskinfo;

void wait_ready(void);
void wait_command_done(void);
void wait_disk_active(uint8_t disknum);
int version(void);
void configure(bool perp_mode);
void lock(void);
uint16_t sense_interrupt(void);
void set_data_rate(uint16_t rate);
void specify(void);
void set_motor(uint8_t drive, uint8_t on);
void set_dsel(uint8_t drive);
void drive_select(uint8_t drive, uint16_t rate);
void drive_setup(void);
uint8_t init_floppy(void);
void read_floppy_lba(uint8_t disk, uint32_t lba, uint8_t* buffer);

#endif