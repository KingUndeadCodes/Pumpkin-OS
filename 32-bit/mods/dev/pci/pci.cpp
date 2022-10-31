#include "pci.h"
#include "../port.cpp"

const char* __PCI_classes[] {
    "Unclassified",
    "Mass Storage Controller",
    "Network Controller",
    "Display Controller",
    "Multimedia Controller",
    "Memory Controller",
    "Bridge Device",
    "Simple Communication Controller",
    "Base System Peripheral",
    "Input Device Controller",
    "Docking Station", 
    "Processor",
    "Serial Bus Controller",
    "Wireless Controller",
    "Intelligent Controller",
    "Satellite Communication Controller",
    "Encryption Controller",
    "Signal Processing Controller",
    "Processing Accelerator",
    "Non Essential Instrumentation"
};


/*
// https://wiki.osdev.org/PCI
// 0x80 is always Other
static const char* __PCI_subclasss[0x13][0x20] = {
    {"Non-VGA-Compatible Unclassified Device", "VGA-Compatible Unclassified Device"},
    {"SCSI Bus Controller", "IDE Controller", "Floppy Disk Controller", "IPI Bus Controller", "RAID Controller", "ATA Controller", "Serial ATA Controller", "Serial Attached SCSI Controller", "Non-Volatile Memory Controller"},
    {"Ethernet Controller", "Token Ring Controller", "FDDI Controller", "ATM Controller", "ISDN Controller", "WorldFip Controller", "PICMG 2.14 Multi Computing Controller", "PICMG 2.14 Multi Computing Controller", "Fabric Controller"},
    {"VGA Compatible Controller", "XGA Controller", "3D Controller (Not VGA-Compatible)"},
    {"Multimedia Video Controller", "Multimedia Audio Controller", "Computer Telephony Device", "Audio Device"},
    {"RAM Controller", "Flash Controller"},
    {"Host Bridge", "ISA Bridge", "EISA Bridge", "MCA Bridge", "PCI-to-PCI Bridge", "PCMCIA Bridge", "NuBus Bridge", "CardBus Bridge", "RACEway Bridge", " PCI-to-PCI Bridge", "InfiniBand-to-PCI Host Bridge"},
    {"Serial Controller", "Parallel Controller", "Multiport Serial Controller", "Modem", "IEEE 488.1/2 (GPIB) Controller", "Smart Card Controller"},
    {"PIC", "DMA Controller", "Timer", "RTC Controller", "PCI Hot-Plug Controller", "SD Host controller", "IOMMU"},
    {"Keyboard Controller", "Digitizer Pen", "Mouse Controller", "Scanner Controller", "Gameport Controller"},
    {"Generic"},
    {"Intel i386", "Intel i486", "Intel Pentium", "Intel Pentium Pro"},
    {"FireWire (IEEE 1394) Controller", "ACCESS Bus Controller", "SSA", "USB Controller", "Fiber Channel", "SMBus Controller", "InfiniBand Controller", "IPMI Interface", "SERCOS Interface (IEC 61491)", "CANbus Controller"},
    {"iRDA Compatible Controller", "Consumer IR Controller"}, // "RF Controller", "Bluetooth Controller", "Broadband Controller", "Ethernet Controller (802.1a)", "Ethernet Controller (802.1b)"
    {"I20"}, // sidenote: ?
    {"Satellite TV Controller", "Satellite Audio Controller", "Satellite Voice Controller", "Satellite Data Controller"},
    {"Network and Computing Encrpytion/Decryption"},
    {"DPIO Module", "Performance Counters"}
};
*/

static inline uint32_t inl(uint16_t port) {
    uint32_t res;
    asm volatile ("in %%dx, %%eax\n" : "=a" (res) : "d" (port));
    return res;
}

static inline void outl(uint16_t port, uint32_t value) {
    asm volatile ("out %%eax, %%dx\n" :: "d" (port), "a" (value));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t data;
    asm volatile("inw %w1, %w0" : "=a" (data) : "Nd" (port));
    return data;
}

static inline void outw(uint16_t port, uint16_t data) {
    asm volatile("outw %w0, %w1" : : "a" (data), "Nd" (port));
}

uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
    // Create configuration address as per Figure 1
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    // Write out the address
    outl(0xCF8, address);
    // Read in the data
    // (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
    tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
    return tmp;
}

uint16_t pciCheckVendor(uint8_t bus, uint8_t slot) {
    uint16_t vendor, device;
    /* Try and read the first configuration register. Since there are no
     * vendors that == 0xFFFF, it must be a non-existent device. */
    if ((vendor = pciConfigReadWord(bus, slot, 0, 0)) != 0xFFFF) {
       device = pciConfigReadWord(bus, slot, 0, 2);
    } return (vendor);
}

uint16_t getVendorID(uint16_t bus, uint16_t device, uint16_t function) {
    uint32_t r0 = pciConfigReadWord(bus,device,function,0);
    return r0;
}

uint16_t getDeviceID(uint16_t bus, uint16_t device, uint16_t function) {
    uint32_t r0 = pciConfigReadWord(bus,device,function,2);
    return r0;
}

uint16_t getClassId(uint16_t bus, uint16_t device, uint16_t function) {
    uint32_t r0 = pciConfigReadWord(bus,device,function,0xA);
    return (r0 & ~0x00FF) >> 8;
}

uint16_t getSubClassId(uint16_t bus, uint16_t device, uint16_t function) {
    uint32_t r0 = pciConfigReadWord(bus,device,function,0xA);
    return (r0 & ~0xFF00);
}

// https://github.com/pdoane/osdev/blob/423da913cbdd558fca0de652125c359c686b4ba3/pci/driver.c#L88

void checkDevice(uint8_t bus, uint8_t device) {
    uint8_t function = 0;
    uint16_t vendorID = getVendorID(bus, device, function);
    if (vendorID == 0xFFFF) return; // Device doesn't exist
    checkFunction(bus, device, function);
    uint8_t header = pciConfigReadWord(bus, device, function, 0xE) & 0xFF;
    if ((header & 0x80) != 0) {
        for (function = 1; function < 8; function++) {
            if (getVendorID(bus, device, function) != 0xFFFF) checkFunction(bus, device, function);
        }
    }
}

char* deviceName(int vendorID, int deviceID) {
    if ((int)vendorID == 32902 && deviceID == 4110) return "E1000";  // https://pci-ids.ucw.cz/read/PC/8086/100e (qemu -device e1000) port https://wiki.osdev.org/Intel_Ethernet_i217
    else if ((int)vendorID == 4332 && deviceID == 33081) return "RTL8139";
    return "UNKNOWN_DEVICE";
}

void checkFunction(uint8_t bus, uint8_t device, uint8_t function) {
    const int vendorID = (int)getVendorID(bus, device, function);
    const int deviceID = (int)getDeviceID(bus, device, function);
    const char* device_name = deviceName(vendorID, deviceID);
    print("[");
    print("PCI", 10);
    printf("] - %s", __PCI_classes[getClassId(bus, device, function)]);
    (device_name != "UNKNOWN_DEVICE") ? printf(" | %s", device_name) : 0;
    print("\n");
    if (device_name == "RTL8139") {
        // https://doxygen.reactos.org/db/dbb/drivers_2network_2dd_2rtl8139_2hardware_8c_source.html
        uintptr_t rx_buffer;
        uint32_t ioaddr = pciConfigReadWord(bus, device, function, 0x10);
        outb(ioaddr + 0x52, 0x0);
        outb(ioaddr + 0x37, 0x10);
        while((inb(ioaddr + 0x37) & 0x10) != 0) {};
        outb(ioaddr + 0x30, (uintptr_t)rx_buffer);
        outw(ioaddr + 0x3C, 0x0005);
        outl(ioaddr + 0x44, 0xf | (1 << 7));
        outb(ioaddr + 0x37, 0x0C);
        outw(ioaddr + 0x3E, 0x5);
        uint32_t mac_addr[6];
        for (int i = 0; i < 6; i++) mac_addr[i] = inb(ioaddr + i - 1); 
        printf("MAC Address: %d:%d:%d:%d:%d:%d\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    }
    // printf("(%d, %d)", vendorID, deviceID);
    return;
}

void checkBus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; device++) checkDevice(bus, device);
}

void checkAllBuses(void) {
    uint8_t function;
    uint8_t bus; 
    uint8_t headerType = pciConfigReadWord(0, 0, 0, 0xE) & 0xFF;;
    if ((headerType & 0x80) == 0) {
        // Single PCI host controller
        checkBus(0);
    } else {
        // Multiple PCI host controllers
        for (function = 0; function < 8; function++) {
            if (getVendorID(0, 0, function) != 0xFFFF) break;
            bus = function;
            checkBus(bus);
        }
    }
}