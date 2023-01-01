#ifndef __RTL8139_H
#define __RTL8139_H

#include <stdlib.h>
#include "../../idt/irq.h"
#include "../../idt/idt.h"
#include "../../pci/pci.h"
#include "../../port.cpp"

#define CAPR 0x38
#define RX_READ_POINTER_MASK    (~3)
#define ROK                     (1<<0)
#define RER                     (1<<1)
#define TOK                     (1<<2)
#define TER                     (1<<3)
#define TX_TOK                  (1<<15)
#define RX_BUFFER_SIZE          (8192 + 16 + 1500)

// RTL8139 Structures
struct RTL8139 {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t rx_buffer[1000]; // Change to Pointer with size of RX_BUFFER
    uint32_t mac_address[6];
    uint32_t ioaddr;
    uint32_t tx_current;
    uint32_t current_packet_ptr;
} __attribute__((packed));

struct ethernet_frame {
    uint8_t dst_mac_addr[6];
    uint8_t src_mac_addr[6];
    uint16_t type;
    uint8_t data[];
} __attribute__((packed));

struct NetworkDeviceCommonFunctions {
    void (*init)(uint8_t bus, uint8_t device, uint8_t function);
    void (*intr)(struct regs* r);
    void (*send)(void* data, uint32_t len);
    void (*recv)();
    void (*test)(uint8_t bus, uint8_t device, uint8_t function);
    uint8_t *(*getMacAddress)(void);
} __attribute__((packed));

const uint8_t TSAD_array[4] = {0x20, 0x24, 0x28, 0x2C};
const uint8_t TSD_array[4] = {0x10, 0x14, 0x18, 0x1C};

void RTL8139_INIT(uint8_t bus, uint8_t device, uint8_t function);
void RTL8139_TEST(uint8_t bus, uint8_t device, uint8_t function);
void RTL8139_SEND_PACKET(void* data, uint32_t len);
void RTL8139_HANDLER(struct regs* r);
void RTL8139_RECEIVE_PACKET();
uint8_t* RTL8139_MAC_ADDR(void);

// Methods
const struct NetworkDeviceCommonFunctions RTL8139Methods = {
    .init = RTL8139_INIT,
    .intr = RTL8139_HANDLER,
    .send = RTL8139_SEND_PACKET,
    .recv = RTL8139_RECEIVE_PACKET,
    .test = RTL8139_TEST,
    .getMacAddress = RTL8139_MAC_ADDR,
};

#endif