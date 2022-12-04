#ifndef PCI_H
#define PCI_H

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <text.h>
#include <time.h>
// #include "../port.cpp"

#define PCI_CLASS_UNCLASSIFIED                          0x00
#define PCI_CLASS_MASS_STORAGE_CONTROLLER               0x01
#define PCI_CLASS_NETWORK_CONTROLLER                    0x02
#define PCI_CLASS_DISPLAY_CONTROLLER                    0x03
#define PCI_CLASS_MULTIMEDIA_CONTROLLER                 0x04
#define PCI_CLASS_MEMORY_CONTROLLER                     0x05
#define PCI_CLASS_BRIDGE_DEVICE                         0x06
#define PCI_CLASS_SIMPLE_COMMUNICATION_CONTROLLER       0x07
#define PCI_CLASS_BASE_SYSTEM_PERIPHERAL                0x08
#define PCI_CLASS_INPUT_DEVICE_CONTROLLER               0x09
#define PCI_CLASS_DOCKING_STATION                       0x0A
#define PCI_CLASS_PROCESSOR                             0x0B
#define PCI_CLASS_SERIAL_BUS_CONTROLLER                 0x0C
#define PCI_CLASS_WIRELESS_CONTROLLER                   0x0D
#define PCI_CLASS_INTELLIGENT_CONTROLLER                0x0E
#define PCI_CLASS_SATELLITE_COMMUNICATION_CONTROLLER    0x0F
#define PCI_CLASS_ENCRYPTION_CONTROLLER                 0x10
#define PCI_CLASS_SIGNAL_PROCESSING                     0x11
#define PCI_CLASS_PROCESSING_ACCELERATOR                0x12
#define PCI_CLASS_NON_ESSENTIAL_INSTRUMENTATION         0x13

void pciConfigWriteWord(uint32_t bus, uint32_t slot, uint32_t function, uint32_t offset, uint32_t value);
uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pciCheckVendor(uint8_t bus, uint8_t slot);
uint16_t getVendorID(uint16_t bus, uint16_t device, uint16_t function);
uint16_t getDeviceID(uint16_t bus, uint16_t device, uint16_t function);
void checkDevice(uint8_t bus, uint8_t device);
void checkFunction(uint8_t bus, uint8_t device, uint8_t function);
void checkAllBuses(void);

#endif
