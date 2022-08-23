#include "pci.h"

static inline uint32_t inl(uint16_t port) {
  uint32_t res;
  asm volatile (
    "in %%dx, %%eax\n"
    : "=a" (res)
    : "d" (port)
  );
  return res;
}

static inline void outl(uint16_t port, uint32_t value) {
  asm volatile (
    "out %%eax, %%dx\n"
    :
    : "d" (port), "a" (value)
  );
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

uint16_t getVendorID(uint16_t bus, uint16_t device, uint16_t function)
{
        uint32_t r0 = pciConfigReadWord(bus,device,function,0);
        return r0;
}

uint16_t getDeviceID(uint16_t bus, uint16_t device, uint16_t function)
{
        uint32_t r0 = pciConfigReadWord(bus,device,function,2);
        return r0;
}

void checkDevice(uint8_t bus, uint8_t device) {
    uint8_t function = 0;
    uint16_t vendorID = getVendorID(bus, device, function);
    if (vendorID == 0xFFFF) return; // Device doesn't exist
    checkFunction(bus, device, function);
    for (function = 1; function < 8; function++) {
        if (getVendorID(bus, device, function) != 0xFFFF) {
            checkFunction(bus, device, function);
        }
    }
}

void checkFunction(uint8_t bus, uint8_t device, uint8_t function) {
     if (pciCheckVendor(bus, device) & 0x80) return;
     const int vendorID = (int)getVendorID(bus, device, function);
     const int deviceID = (int)getDeviceID(bus, device, function);
     print("[");
     print("PCI", 10);
     print("] Device Detected. ");
     printf("<vendor=%d, device=%d>\n", vendorID, deviceID);
     if ((int)vendorID == 4660 && deviceID == 4369) print("    - [INFO] Device is \"QEMU VGA\" Device\n");
}


void checkAllBuses(void) {
    uint16_t bus;
    uint8_t device;
    for (bus = 0; bus < 256; bus++) {
        for (device = 0; device < 32; device++) {
            checkDevice(bus, device);
        }
    }
}
