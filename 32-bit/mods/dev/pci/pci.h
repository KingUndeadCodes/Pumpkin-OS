#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <text.h>
// #include "../port.cpp"

uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pciCheckVendor(uint8_t bus, uint8_t slot);
uint16_t getVendorID(uint16_t bus, uint16_t device, uint16_t function);
uint16_t getDeviceID(uint16_t bus, uint16_t device, uint16_t function);
void checkDevice(uint8_t bus, uint8_t device);
void checkFunction(uint8_t bus, uint8_t device, uint8_t function);
void checkAllBuses(void);

#endif
