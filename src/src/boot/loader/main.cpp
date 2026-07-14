#include "include/libboot.h"
#include "include/floppy.h"
#include "include/fat.h"

#define KERNEL_LOCATION 0x400000 

#define BOOT_SECTOR_LOCATION    (KERNEL_LOCATION - 512)
#define NAME_BUFFER_ADDRESS     (BOOT_SECTOR_LOCATION - 512)

// FAT12 floppy usually has 9 FAT sectors.
// Reserve 12 sectors just to be safe.
#define FAT_BUFFER_ADDRESS      (NAME_BUFFER_ADDRESS - (512 * 12))
#define MAX_FAT_SECTORS         12

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

static uint16_t fat12_next_cluster(uint8_t* fat_data, uint16_t cluster) {
    uint16_t fat_offset = cluster + (cluster / 2);
    uint16_t table_value = *(uint16_t*)&fat_data[fat_offset];

    if (cluster & 1) {
        table_value >>= 4;
    } else {
        table_value &= 0x0FFF;
    }

    return table_value;
}

uint32_t read_file(uint8_t disk, const fat_BS_t* bs, const char* filename, uint8_t* dest) {
    uint16_t root_start =
        bs->reserved_sector_count +
        (bs->table_count * bs->table_size_16);

    uint16_t root_sectors =
        ((bs->root_entry_count * 32) + bs->bytes_per_sector - 1) /
        bs->bytes_per_sector;

    uint16_t data_start = root_start + root_sectors;

    uint16_t start_cluster = 0;
    uint32_t file_size = 0;
    bool found = false;
    bool end_of_directory = false;

    // -----------------------------
    // 1. Find file in root directory
    // -----------------------------
    for (uint16_t s = 0; s < root_sectors && !found && !end_of_directory; s++) {
        read_floppy_lba(disk, root_start + s, (uint8_t*)NAME_BUFFER_ADDRESS);
        drive_setup();

        fat_dir_t* entries = (fat_dir_t*)NAME_BUFFER_ADDRESS;
        uint16_t count = bs->bytes_per_sector / sizeof(fat_dir_t);

        for (uint16_t i = 0; i < count; i++) {
            fat_dir_t* e = &entries[i];

            if ((uint8_t)e->filename[0] == 0x00) {
                end_of_directory = true;
                break;
            }

            if ((uint8_t)e->filename[0] == 0xE5) continue; // deleted
            if (e->attributes == 0x0F) continue;           // long filename entry
            if (e->attributes & 0x08) continue;            // volume label
            if (e->attributes & 0x10) continue;            // directory

            char shortname[13];
            for (int k = 0; k < 13; k++) shortname[k] = 0;

            int idx = 0;

            for (int j = 0; j < 8 && e->filename[j] != ' '; j++) {
                shortname[idx++] = e->filename[j];
            }

            if (e->extension[0] != ' ') {
                shortname[idx++] = '.';

                for (int j = 0; j < 3 && e->extension[j] != ' '; j++) {
                    shortname[idx++] = e->extension[j];
                }
            }

            shortname[idx] = '\0';

            if (strcasecmp(shortname, filename) == 0) {
                start_cluster = e->first_cluster_low;
                file_size = e->file_size;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        print("File not found: ", 12);
        print(filename, 12);
        print("\n", 12);
        return 0;
    }

    if (file_size == 0) {
        print("File is empty.\n", 14);
        return 0;
    }

    // -----------------------------
    // 2. Read the whole FAT
    // -----------------------------
    if (bs->table_size_16 > MAX_FAT_SECTORS) {
        print("FAT too large for bootloader buffer.\n", 12);
        return 0;
    }

    uint8_t* fat_data = (uint8_t*)FAT_BUFFER_ADDRESS;

    for (uint16_t s = 0; s < bs->table_size_16; s++) {
        read_floppy_lba(
            disk,
            bs->reserved_sector_count + s,
            fat_data + (s * bs->bytes_per_sector)
        );

        drive_setup();
    }

    // -----------------------------
    // 3. Follow FAT cluster chain
    // -----------------------------
    uint8_t* out = dest;
    uint32_t remaining = file_size;
    uint16_t cluster = start_cluster;

    while (cluster >= 2 && cluster < 0xFF8 && remaining > 0) {
        for (uint8_t sec = 0; sec < bs->sectors_per_cluster && remaining > 0; sec++) {
            uint32_t lba =
                data_start +
                ((cluster - 2) * bs->sectors_per_cluster) +
                sec;

            if (remaining >= bs->bytes_per_sector) {
                read_floppy_lba(disk, lba, out);
                drive_setup();

                out += bs->bytes_per_sector;
                remaining -= bs->bytes_per_sector;
            } else {
                // Last partial sector: read into scratch first.
                read_floppy_lba(disk, lba, (uint8_t*)NAME_BUFFER_ADDRESS);
                drive_setup();

                uint8_t* scratch = (uint8_t*)NAME_BUFFER_ADDRESS;

                for (uint32_t i = 0; i < remaining; i++) {
                    out[i] = scratch[i];
                }

                out += remaining;
                remaining = 0;
            }
        }

        if (remaining == 0) {
            break;
        }

        cluster = fat12_next_cluster(fat_data, cluster);
    }

    if (remaining != 0) {
        print("Warning: FAT chain ended before file size.\n", 14);
        return file_size - remaining;
    }

    print("File read successfully: ", 10);
    print(filename, 10);
    print("\n", 10);

    return file_size;
}

static fat_BS_t* bootSector = NULL; 
static uint8_t disk_id = 0;

static uint32_t read_file_frontend(const char* filename, uint8_t* dest) {
    return read_file(disk_id, bootSector, filename, dest);
}

// See docs/DOCS.md ("Bootloader -> kernel read_file handoff") for why
// this is a real linked symbol, referenced by name from entry.asm,
// instead of a hand-computed absolute address shared between the two
// files as a bare literal.
extern "C" uint32_t (*g_read_file_ptr)(const char* filename, uint8_t* dest) = NULL;

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
            // entry.asm reads this symbol directly (`extern g_read_file_ptr`)
            // and pushes it as kernel_main()'s read_file argument.
            g_read_file_ptr = &read_file_frontend;
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