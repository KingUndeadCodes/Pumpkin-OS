#ifndef __FAT_H__
#define __FAT_H__

#include <stdint.h>

/*
// 32-byte FAT12 directory entry
typedef struct {
    char     name[8];           // Filename (space-padded)
    char     ext[3];            // Extension (space-padded)
    uint8_t  attr;              // File attributes
    uint8_t  reserved;          // Reserved for Windows NT
    uint8_t  creationTimeTenths; // Millisecond stamp at file creation
    uint16_t creationTime;      // Time file was created
    uint16_t creationDate;      // Date file was created
    uint16_t lastAccessDate;    // Last access date
    uint16_t firstClusterHigh;  // High 16 bits of first cluster (always 0 in FAT12/FAT16)
    uint16_t writeTime;         // Last write time
    uint16_t writeDate;         // Last write date
    uint16_t firstClusterLow;   // Low 16 bits of first cluster
    uint32_t fileSize;          // File size in bytes
} __attribute__((packed)) FAT12DirectoryEntry;
*/

// https://wiki.osdev.org/User:Oros/FAT#File_Allocation_Table

typedef struct fat_BS {
	uint8_t  bootjmp[3];
	uint8_t  oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t  sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t  table_count;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t  media_type;
	uint16_t table_size_16;
	uint16_t sectors_per_track;
	uint16_t head_side_count;
	uint32_t hidden_sector_count;
	uint32_t total_sectors_32;
    uint8_t  bios_drive_num;
	uint8_t  reserved1;
	uint8_t  boot_signature;
	uint32_t volume_id;
	uint8_t  volume_label[11];
	uint8_t  fat_type_label[8];
} __attribute__((packed)) fat_BS_t;

typedef struct fat_dir {
	char filename[8];
	char extension[3];
	uint8_t attributes;
	uint8_t reserved; // Unused.
	uint8_t creation_time_tenths; // Milliseconds
	uint16_t creation_time; // Time of creation
	uint16_t creation_date; // Date of creation
	uint16_t last_access_date; // Last access date
	uint16_t first_cluster_high; // High 16 bits of the first cluster number
	uint16_t last_write_time; // Last write time
	uint16_t last_write_date; // Last write date
	uint16_t first_cluster_low; // Low 16 bits of the first cluster number
	uint32_t file_size; // Size of the file in bytes
} __attribute__((packed)) fat_dir_t;

#endif