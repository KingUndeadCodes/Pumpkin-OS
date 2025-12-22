#include "include/floppy.h"
#include "include/libboot.h"

// All of this is from MellOS. 

static Diskinfo* diskinfo = (Diskinfo*)0x5200;

void wait_ready(void) {
    uint8_t msr = inb(FLOPPY_REGISTER_MAIN_STATUS_REGISTER);
    while(!((msr & MSR_FLAGS_RQM) && (msr &~ MSR_FLAGS_DIO))){
        msr = inb(FLOPPY_REGISTER_MAIN_STATUS_REGISTER);
    }
}

void wait_command_done(void) {
    uint32_t i = 0;
    while(!(inb(FLOPPY_REGISTER_MAIN_STATUS_REGISTER) & MSR_FLAGS_RQM) && i < 10000000) i++;
    if (i == 10000000) print("Floppy command timed out\n");
    return;
}

void wait_disk_active(uint8_t disknum) {
    while(inb(FLOPPY_REGISTER_MAIN_STATUS_REGISTER) & (disknum & 0x0F));
}

// 0xFF, "controller not found" (very bad)
// 0x90, "enhanced (82077AA) controller found" (good)
// 0x80, "NEC controller found"
// 0x81, "VMware controller found"

int version(void) {
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_VERSION);
    wait_command_done();
    return inb(FLOPPY_REGISTER_DATA_FIFO);
}

void configure(bool perp_mode) {
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_CONFIGURE);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, 0 /*Protocol requires empty byte*/);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_CONFIG_FLAGS_IMPLIED_SEEK | FLOPPY_CONFIG_FLAGS_POLLING_DISABLE | 0x8 /*Threshold value*/);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, 0 /*Write precompensation. 0 = as specified by manufacturer*/);
    wait_command_done();
    if (perp_mode) {   
        outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_PERPENDICULAR_MODE);
        wait_ready();
        outb(FLOPPY_REGISTER_DATA_FIFO, 1 << 2); // Enable for drive 0
        wait_ready();
    }
}

void lock(void) { // Locks settings so that reset does not clear them
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_LOCK | FLOPPY_COMMAND_LOCK_ON);
    wait_command_done();
    uint8_t res = inb(FLOPPY_REGISTER_DATA_FIFO); // Read the result
    if (res != 1 << 4) {
        print("Lock failed\n");
    }
}

uint16_t sense_interrupt(void) {
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_SENSE_INTERRUPT);
    wait_command_done();
    uint8_t st0 = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
    uint8_t st1 = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
    return st1 << 8 | st0;
}

void set_data_rate(/* enum FloppyDatarates */ uint16_t rate) {
    wait_ready();
    uint8_t dsr = inb(FLOPPY_REGISTER_DATARATE_SELECT_REGISTER) &~ 0x03;
    wait_ready();
    outb(FLOPPY_REGISTER_DATARATE_SELECT_REGISTER, dsr | rate);
    wait_ready();
    uint8_t ccr = inb(FLOPPY_REGISTER_CONFIGURATION_CONTROL_REGISTER) &~ 0x03;
    wait_ready();
    outb(FLOPPY_REGISTER_CONFIGURATION_CONTROL_REGISTER, ccr | rate);
    wait_ready();
}

void specify(void) {
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_SPECIFY);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, 8 << 4 | 15); // SRT = 8, HUT = 15
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, 5 << 1 | 1); // HLT = 5, NDMA = 1
    wait_ready();
}

void set_motor(uint8_t drive, uint8_t on) {
    wait_ready();
    uint8_t dor = inb(FLOPPY_REGISTER_DIGITAL_OUTPUT_REGISTER) & 0x0F; // Unset motor bits
    wait_ready();
    dor = dor | (on << (drive + 4));
    outb(FLOPPY_REGISTER_DIGITAL_OUTPUT_REGISTER, dor);
    wait_ready();
}

void set_dsel(uint8_t drive) {
    wait_ready();
    uint8_t dor = inb(FLOPPY_REGISTER_DIGITAL_OUTPUT_REGISTER) &~ 0x03; // Unset drive select bits
    wait_ready();
    dor = dor | (drive & 0x03);
    outb(FLOPPY_REGISTER_DIGITAL_OUTPUT_REGISTER, dor);
    wait_ready();
}

void drive_select(uint8_t drive, /* enum FloppyDatarates */ uint16_t rate) {
    set_data_rate(rate);
    specify();
    set_motor(drive, 1);
    set_dsel(drive);
}

void drive_reset(void) {
    uint8_t status = inb(FLOPPY_REGISTER_MAIN_STATUS_REGISTER)  &~ DOR_FLAG_IRQ;
    outb(FLOPPY_REGISTER_DIGITAL_OUTPUT_REGISTER, 0);
    wait_command_done();
    outb(FLOPPY_REGISTER_DIGITAL_OUTPUT_REGISTER, status);
    wait_command_done();
    char buf[256];
    itoa_signed(status, 16, buf);
}

void recalibrate(uint8_t disknum) {
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_RECALIBRATE);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, disknum);
    wait_disk_active(disknum);
    uint8_t res = sense_interrupt() >> 0x0F;
}

void seek(uint8_t disknum, uint8_t cylinder, uint8_t head) {
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_SEEK);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, head << 2 | disknum);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, cylinder);
    wait_disk_active(disknum);
    uint8_t res = sense_interrupt() >> 0x0F;
    uint32_t timeout = 0; // Give some time to seek. Arbitrary value, increase if the floppy is giving read issues
    while(timeout--);
}

void read_floppy(uint8_t drive, uint8_t cylinder, uint8_t head, uint8_t sector, uint8_t* buffer){
    seek(drive, cylinder, head);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, FLOPPY_COMMAND_READ_DATA | FLOPPY_COMMAND_MT | FLOPPY_COMMAND_MFM);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, head << 2 | drive);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, cylinder);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, head);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, sector);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, 2); // Sector size
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, diskinfo->max_sectors);
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, 0x1B); // "GAP1 default size"
    wait_ready();
    outb(FLOPPY_REGISTER_DATA_FIFO, 0xFF); // Sector size
    // Execution phase. Read data. Reads into 1024 byte buffer, beware
    // const int bytes = 1024; // 2.88 MB floppies
    const int bytes = 512; // 1.44 MB floppies
    for (int i = 0; i < /* 1024 */ bytes; i++){
        uint8_t msr = inb(FLOPPY_REGISTER_MAIN_STATUS_REGISTER);
        while((msr & (MSR_FLAGS_RQM | MSR_FLAGS_NDMA)) != (MSR_FLAGS_RQM | MSR_FLAGS_NDMA)){
            msr = inb(FLOPPY_REGISTER_MAIN_STATUS_REGISTER);
        }
        buffer[i] = inb(FLOPPY_REGISTER_DATA_FIFO);
    }
    // Result phase. Read status
    wait_ready();
    uint8_t st0 = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
    uint8_t st1 = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
    uint8_t st2 = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
    uint8_t end_cyl = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
    uint8_t end_hd = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
    uint8_t end_sec = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
    uint8_t two = inb(FLOPPY_REGISTER_DATA_FIFO);
    wait_ready();
}

void drive_setup(void) {
    drive_reset();
    // drive_select(diskinfo->boot_drive_number, FLOPPY_DATA_RATE_1000KBPS); // This is for 2.88MB floppies
    drive_select(diskinfo->boot_drive_number, FLOPPY_DATA_RATE_500KBPS); // This is for 1.44MB floppies
    recalibrate(diskinfo->boot_drive_number);
    recalibrate(diskinfo->boot_drive_number);
    uint32_t timeout = 0; // Give some time to recalibrate. Arbitrary value, increase if the floppy is giving read issues
    while(timeout--);
}

uint8_t init_floppy(void) {
    char buf[256];
    itoa_signed(version(), 16, buf);
    print("Floppy version: ");
    print(buf);
    print("\n");
    // configure(true); // Perpendicular mode, for 2.88MB floppies
    configure(false);
    lock();
    drive_setup();
    // ...
    print("(");
    itoa_signed(diskinfo->drive_type, 10, buf);
    print(buf);
    print(", ");
    itoa_signed(diskinfo->max_cylinders, 10, buf);
    print(buf);
    print(", ");
    itoa_signed(diskinfo->max_sectors, 10, buf);
    print(buf);
    print(", ");
    itoa_signed(diskinfo->max_heads, 10, buf);
    print(buf);
    print(", ");
    itoa_signed(diskinfo->num_drives, 10, buf);
    print(buf);
    print(", ");
    itoa_signed(diskinfo->boot_drive_number, 10, buf);
    print(buf);
    print(")\n");
    // ...
    return diskinfo->boot_drive_number;
}

void read_floppy_lba(uint8_t disk, uint32_t lba, uint8_t* buffer) {
    uint8_t cylinder = lba / ((diskinfo->max_heads  + 1) * diskinfo->max_sectors);
    uint8_t head = (lba % ((diskinfo->max_heads  + 1) * diskinfo->max_sectors)) / diskinfo->max_sectors;
    uint8_t sector = lba % diskinfo->max_sectors + 1;
    // uint8_t sector = (lba % (diskinfo->max_heads * diskinfo->max_sectors)) % diskinfo->max_sectors + 1;
    read_floppy(disk, cylinder, head, sector, buffer);
}