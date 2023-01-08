#include "pci.h"
#include "../port.cpp"
#include "./drivers/rtl8139.h"
#include "../serial/serial.h"

static const char* __PCI_classes[] {
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

// Remove more and more cases when it breaks.
static const char* getSubClassName (int base_class, int sub_class) {
    const char* Unknown = "Unknown";
    if (sub_class == 0x80) return "Other";
    switch (base_class) {
        case PCI_CLASS_UNCLASSIFIED: {
            switch (sub_class) {
                case 0x00: return "Non-VGA-Compatible Unclassified Device";
                case 0x01: return "VGA-Compatible Unclassified Device";
                default: return Unknown;
            }
        }
        case PCI_CLASS_MASS_STORAGE_CONTROLLER: {
            switch (sub_class) {
                case 0x00: return "SCSI Bus Controller";
                case 0x01: return "IDE Controller";
                case 0x02: return "Floppy Disk Controller";
                case 0x03: return "IPI Bus Controller";
                case 0x04: return "RAID Controller";
                case 0x05: return "ATA Controller";
                case 0x06: return "Serial ATA Controller";
                case 0x07: return "Serial Attached SCSI Controller";
                case 0x08: return "Non-Volatile Memory Controller";
                default: return Unknown;
            }
        }
        case PCI_CLASS_NETWORK_CONTROLLER: {
            switch (sub_class) {
                case 0x00: return "Ethernet Controller";
                case 0x01: return "Token Ring Controller";
                case 0x02: return "FDDI Controller";
                case 0x03: return "ATM Controller";
                case 0x04: return "ISDN Controller";
                case 0x05: return "WorldFip Controller";
                case 0x06: return "PICMG 2.14 Multi Computing Controller";
                case 0x07: return "PICMG 2.14 Multi Computing Controller";
                case 0x08: return "Fabric Controller";
                default: return Unknown;
            }
        }
        case PCI_CLASS_DISPLAY_CONTROLLER: {
            switch (sub_class) {
                case 0x00: return "VGA Compatible Controller";
                case 0x01: return "XGA Controller";
                case 0x02: return "3D Controller (Not VGA-Compatible)";
                default: return Unknown;
            }
        }
        case PCI_CLASS_MULTIMEDIA_CONTROLLER: {
            switch (sub_class) {
                case 0x00: return "Multimedia Video Controller";
                case 0x01: return "Multimedia Audio Controller";
                case 0x02: return "Computer Telephony Device";
                case 0x03: return "Audio Device";
                default: return Unknown;
            }
        }
        case PCI_CLASS_MEMORY_CONTROLLER: {
            switch (sub_class) {
                case 0x00: return "RAM Controller";
                case 0x01: return "Flash Controller";
                default: return Unknown;
            }
        }
        case PCI_CLASS_BRIDGE_DEVICE: {
            switch (sub_class) {
                case 0x00: return "Host Bridge";
                case 0x01: return "ISA Bridge";
                case 0x02: return "EISA Bridge";
                case 0x03: return "MCA Bridge"; 
                case 0x04: return "PCI-to-PCI Bridge";
                case 0x05: return "PCMCIA Bridge";
                case 0x06: return "NuBus Bridge";
                case 0x07: return "CardBus Bridge";
                case 0x08: return "RACEway Bridge";
                case 0x09: return "PCI-to-PCI Bridge";
                case 0x0A: return "InfiniBand-to-PCI Host Bridge";
                default: return Unknown;
            }   
        }
        case PCI_CLASS_SIMPLE_COMMUNICATION_CONTROLLER: {
            switch (sub_class) {
                case 0x00: return "Serial Controller";
                case 0x01: return "Parallel Controller";
                case 0x02: return "Multiport Serial Controller";
                case 0x03: return "Modem";
                case 0x04: return "IEEE 488.1/2 (GPIB) Controller";
                case 0x05: return "Smart Card Controller";
                default: return Unknown;
            }
        }
        case PCI_CLASS_BASE_SYSTEM_PERIPHERAL: {
            switch (sub_class) {
                case 0x00: return "PIC";
                case 0x01: return "DMA Controller";
                case 0x02: return "Timer";
                case 0x03: return "RTC Controller";
                case 0x04: return "PCI Hot-Plug Controller";
                case 0x05: return "SD Host controller";
                case 0x06: return "IOMMU";
                default: return Unknown;
            }
        }
        default: return "Unknown Class or Undefined Subclass";
    }
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

void pciConfigWriteWord(uint32_t bus, uint32_t slot, uint32_t function, uint32_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    // Write out the address
    outl(0xCF8, address);
	outl(0xCFC + (offset & 0x2), value);
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
    if (
        /* RTL8129 */    (vendorID == 0x10ec && deviceID == 0x8129) || \
        /* RTL8139 */    (vendorID == 0x10ec && deviceID == 0x8139) || \
        /* RTL8139B */   (vendorID == 0x10ec && deviceID == 0x8138)
    ) return "RTL8139"; // TODO: Change to "RTL8139 or Compatible"
    return "UNKNOWN_DEVICE";
}

void checkFunction(uint8_t bus, uint8_t device, uint8_t function) {
    const int vendorID = (int)getVendorID(bus, device, function);
    const int deviceID = (int)getDeviceID(bus, device, function);
    const char* device_name = deviceName(vendorID, deviceID);
    serial_write_string("Found PCI ", true, INFO);
    getSubClassName(getClassId(bus, device, function), getSubClassId(bus, device, function)) == "Other" ? serial_write_string(__PCI_classes[getClassId(bus, device, function)], false, NONE) : serial_write_string(getSubClassName(getClassId(bus, device, function), getSubClassId(bus, device, function)), false, NONE);
    print("[");
    print("PCI", 10);
    printf("] - %s / %s", __PCI_classes[getClassId(bus, device, function)], getSubClassName(getClassId(bus, device, function), getSubClassId(bus, device, function)));
    (device_name != "UNKNOWN_DEVICE") ? printf(" / %s", device_name) : 0;
    print("\n");
    if (device_name == "RTL8139") { 
        serial_write_string(" | RTL8139\n", false, NONE);
        RTL8139Methods.test(bus, device, function);
    } else {
        serial_write_string("<", false, NONE);
        serial_write_string(itoa(vendorID, 10), false, NONE);
        serial_write_string(", ", false, NONE);
        serial_write_string(itoa(deviceID, 10), false, NONE);
        serial_write_string(">\n", false, NONE);
    }
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