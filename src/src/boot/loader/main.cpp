#include "include/libboot.h"
#include "include/floppy.h"
#include "include/fat.h"

#define KERNEL_LOCATION 0x400000 

#define KERNEL_SECTORS 256
#define KERNEL_START_SECTOR (2 + 16)

#define BOOT_SECTOR_LOCATION    (KERNEL_LOCATION - 512)
#define NAME_BUFFER_ADDRESS     (BOOT_SECTOR_LOCATION - 512)

#define READ_FUNCTION_ADDRESS   (NAME_BUFFER_ADDRESS - 8)

void list_directories(uint8_t disk, const fat_BS_t* bs) {
    uint16_t root_start = bs->reserved_sector_count + (bs->table_count * bs->table_size_16);
    uint16_t root_sectors = (bs->root_entry_count * 32 + bs->bytes_per_sector - 1) / bs->bytes_per_sector;

    for (uint16_t s = 0; s < root_sectors; s++) {
        // Read one sector of the root directory
        read_floppy_lba(disk, root_start + s, (uint8_t*)NAME_BUFFER_ADDRESS);
        drive_setup();

        fat_dir_t* entries = (fat_dir_t*)NAME_BUFFER_ADDRESS;
        uint16_t count = bs->bytes_per_sector / sizeof(fat_dir_t);

        for (uint16_t i = 0; i < count; i++) {
            fat_dir_t* e = &entries[i];

            if (e->filename[0] == 0x00) return;       // End of directory
            if (e->filename[0] == 0xE5) continue;     // Deleted entry
            if (e->attributes == 0x0F) continue;      // Long file name entry

            // Print filename (trim spaces)
            char name[9] = {0};  // 8 chars + null
            for (uint8_t j = 0; j < 8 && e->filename[j] != ' '; j++) {
                name[j] = e->filename[j];
            }
            print(name, 15);

            // If it’s a file, print extension
            if (!(e->attributes & 0x10)) { // Not a directory
                char ext[4] = {0};  // 3 chars + null
                for (uint8_t j = 0; j < 3 && e->extension[j] != ' '; j++) {
                    ext[j] = e->extension[j];
                }
                if (ext[0] != 0) {
                    print(".", 15);
                    print(ext, 15);
                }
            }

            print("\n", 15);
        }
    }
}

static int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;

        // Uppercase → lowercase
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;

        if (c1 != c2) {
            return (unsigned char)c1 - (unsigned char)c2;
        }
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

void read_file(uint8_t disk, const fat_BS_t* bs, const char* filename, uint8_t* dest) {
    uint16_t root_start = bs->reserved_sector_count + (bs->table_count * bs->table_size_16);
    uint16_t root_sectors = (bs->root_entry_count * 32 + bs->bytes_per_sector - 1) / bs->bytes_per_sector;

    // --- Step 1: Locate the file in root directory ---
    fat_dir_t entry;
    uint16_t start_cluster = 0;
    uint32_t file_size = 0;
    int found = 0;

    for (uint16_t s = 0; s < root_sectors && !found; s++) {
        read_floppy_lba(disk, root_start + s, (uint8_t*)NAME_BUFFER_ADDRESS);
        drive_setup();

        fat_dir_t* entries = (fat_dir_t*)NAME_BUFFER_ADDRESS;
        uint16_t count = bs->bytes_per_sector / sizeof(fat_dir_t);

        for (uint16_t i = 0; i < count; i++) {
            fat_dir_t* e = &entries[i];
            if (e->filename[0] == 0x00) break;       // End of dir
            if (e->filename[0] == 0xE5) continue;    // Deleted
            if (e->attributes == 0x0F) continue;     // Long filename

            // Compare against filename (8.3, space padded)
            char shortname[13] = {0};
            int idx = 0;
            for (int j = 0; j < 8 && e->filename[j] != ' '; j++) shortname[idx++] = e->filename[j];
            if (e->extension[0] != ' ') {
                shortname[idx++] = '.';
                for (int j = 0; j < 3 && e->extension[j] != ' '; j++) shortname[idx++] = e->extension[j];
            }

            if (strcasecmp(shortname, filename) == 0) {
                start_cluster = e->first_cluster_low;
                file_size = e->file_size;
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        print("File not found.\n", 15);
        return;
    }

    // --- Step 2: Read FAT table into memory ---
    uint16_t sector_size = bs->bytes_per_sector;
    uint16_t reserved_sectors = bs->reserved_sector_count;
    uint8_t fat_data[sector_size * 2];

    // FAT is usually 2 sectors for floppy
    read_floppy_lba(disk, reserved_sectors, fat_data);
    drive_setup();
    read_floppy_lba(disk, reserved_sectors + 1, fat_data + sector_size);
    drive_setup();

    // --- Step 3: Follow cluster chain and load file ---
    uint16_t bytes_per_cluster = bs->sectors_per_cluster * bs->bytes_per_sector;
    uint16_t data_start = root_start + root_sectors;
    uint8_t* buf_ptr = dest;

    uint16_t cluster = start_cluster;
    while (cluster >= 2 && cluster < 0xFF8) {
        // Read each sector in the cluster
        for (uint8_t sec = 0; sec < bs->sectors_per_cluster; sec++) {
            read_floppy_lba(disk, data_start + (cluster - 2) * bs->sectors_per_cluster + sec, buf_ptr);
            drive_setup();
            buf_ptr += bs->bytes_per_sector;
        }

        // Get next cluster from FAT12
        uint16_t fat_offset = cluster + (cluster / 2);
        uint16_t table_value = *(uint16_t*)&fat_data[fat_offset];
        if (cluster & 1) {
            table_value >>= 4;
        } else {
            table_value &= 0x0FFF;
        }
        cluster = table_value;
    }

    print("File read successfully.\n", 15);
}

static fat_BS_t* bootSector = NULL; 
static uint8_t disk_id = 0;

static void read_file_frontend(const char* filename, uint8_t* dest) {
    read_file(disk_id, bootSector, filename, dest);
    return;
}

// ~4500 BYTES REMAINING

extern "C" void kernel_main() {
    // Cursor::enableCursor(0, 10);
    print("Booting from Floppy...\n", 15);
    load_floppy:
        uint8_t disk = init_floppy();
        char buf[256];
        file_alloc_table:
            read_floppy_lba(disk, 0, (uint8_t*)(BOOT_SECTOR_LOCATION));
            drive_setup();
            uint8_t* bootSectorPointer = (uint8_t*)((BOOT_SECTOR_LOCATION));
            // Use the bootSectorPointer to read the FAT boot sector
            // fat_BS_t* bootSector = (fat_BS_t*)bootSectorPointer;
            bootSector = (fat_BS_t*)bootSectorPointer;
            disk_id = disk;
            /*
            // Read the file allocation table
            itoa_signed(bootSector->sectors_per_track, 10, buf);  
            print(buf);
            */
            list_directories(disk, bootSector);
            read_file(disk, bootSector, "KERNEL.BIN", (uint8_t*)KERNEL_LOCATION);
            // Save a pointer to the read_file function to the kernel to memory address `READ_FUNCTION_ADDRESS` to be passed to the kernel by `entry.asm`.
            uint32_t* ptr = (uint32_t*)READ_FUNCTION_ADDRESS;
            *ptr = &read_file_frontend; // read_file
            // while (true);
        /*
        for (size_t i = 0; i < KERNEL_SECTORS; i++) {
            read_floppy_lba(disk, KERNEL_START_SECTOR + i, (uint8_t*)(KERNEL_LOCATION + i * 512));
            drive_setup();
        }
        */
        set_motor(0, 0);
        print("Kernel loaded successfully!\n", 15);
    // Cursor::disbaleCursor();
}