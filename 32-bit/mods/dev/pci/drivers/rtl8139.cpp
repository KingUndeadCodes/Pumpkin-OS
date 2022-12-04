#include "../../idt/irq.h"
#include "../../idt/idt.h"
#include "../../pci/pci.h"
#include "../../port.cpp"
#include "rtl8139.h"

static struct RTL8139 NICDevice;

void RTL8139_RECEIVE_PACKET() {
    struct RTL8139 NIC = NICDevice;
    uint16_t * t = (uint16_t*)(NIC.rx_buffer + NIC.current_packet_ptr);
    uint16_t packet_length = *(t + 1);
    t = t + 2;
    void* packet = malloc(packet_length);
    memcpy(packet, t, packet_length);
    // ethernet_handle_packet(packet, packet_length);
    NIC.current_packet_ptr = (NIC.current_packet_ptr + packet_length + 4 + 3) & RX_READ_POINTER_MASK;
    if(NIC.current_packet_ptr > RX_BUFFER_SIZE) NIC.current_packet_ptr -= RX_BUFFER_SIZE; // RX_BUFFER_SIZE was 8192 before.
    outw(NIC.ioaddr + 0x38, NIC.current_packet_ptr - 0x10);
    // free(packet);
}

void RTL8139_SEND_PACKET(void* data, uint32_t len) {
    // Setup Page Alloc and Virtual to Page
    void* transfer_data = malloc(len);
    memcpy(transfer_data, data, len);
    outl(NICDevice.ioaddr + TSAD_array[NICDevice.tx_current], (uint32_t)transfer_data);
    outl(NICDevice.ioaddr + TSD_array[NICDevice.tx_current++], len);
    if(NICDevice.tx_current > 3) NICDevice.tx_current = 0;
}

// https://github.com/0Nera/SynapseOS/blob/a39388115994372d80279bb1ed80d19562f23f9c/kernel/src/drivers/network/RTL8139.c
// https://github.com/szhou42/osdev/blob/master/src/kernel/drivers/rtl8139.c#L111
// https://android.googlesource.com/platform/external/syslinux/+/refs/heads/oreo-m6-s4-release/gpxe/src/drivers/net/rtl8139.c
// https://github.com/narke/Aragveli/blob/a34d0c97a50792e48859bad87f5cffb5691e2fff/src/kernel/arch/x86/pic.c
// https://forum.osdev.org/viewtopic.php?f=1&t=27901
void RTL8139_HANDLER(struct regs* r) {
    print("RTL8139 Interupt Fired!");
    uint16_t irq = inw(NICDevice.ioaddr + 0x3E);
    if (irq & (1<<2)) printf("Packet sent\n");
    if (irq & (1<<0)) RTL8139_RECEIVE_PACKET();
    outw(NICDevice.ioaddr + 0x3E, 0x5);
}

uint8_t* RTL8139_MAC_ADDR(void) {
    static uint8_t mac[6];
    for (int i = 0; i < 6; i++) mac[i] = NICDevice.mac_address[i];
    return mac;
}

// qemu-system-x86_64: Slirp: Failed to send packet, ret: -1

/**
 * Initalize the RTL8139
 * @param ioaddr Base IO Address (PCI BAR0)
 */
void RTL8139_INIT(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t command_port = pciConfigReadWord(bus, device, function, 4);
    if(!(command_port >> 2 & 1)) pciConfigWriteWord(bus, device, function, 4, command_port | TOK);
    struct RTL8139 NIC;
    NIC.bus = bus;
    NIC.device = device;
    NIC.function = function;
    NIC.tx_current = 0;
    NIC.ioaddr = pciConfigReadWord(bus, device, function, 0x10);
    outb(NIC.ioaddr + 0x52, 0x0);
    outb(NIC.ioaddr + 0x37, 0x10);
    while((inb(NIC.ioaddr + 0x37) & 0x10) != 0) {};
    outl(NIC.ioaddr + 0x30, (uint32_t)NIC.rx_buffer);
    outw(NIC.ioaddr + 0x3C, 0x0005);
    outl(NIC.ioaddr + 0x44, 0xF | (1 << 7));
    outb(NIC.ioaddr + 0x37, 0x0C);
    irq_install_handler((int)(pciConfigReadWord(bus, device, function, 0x3C) & 0xFF), RTL8139_HANDLER);
    for (int i = 0; i < 6; i++) NIC.mac_address[i] = inb(NIC.ioaddr + i - 1); 
    NICDevice = NIC;
    return;
};

inline uint16_t htons(uint16_t hostshort) {
    uint32_t first_byte = *((uint8_t*)(&hostshort));
    uint32_t second_byte = *((uint8_t*)(&hostshort) + 1);
    return (first_byte << 8) | (second_byte);
}

int ethernet_send_packet(uint8_t* dst_mac_addr, uint8_t * data, int len, uint16_t protocol) {
    struct ethernet_frame* frame = (struct ethernet_frame*)malloc(sizeof(struct ethernet_frame) + len);
    void* frame_data = (void*)frame + sizeof(struct ethernet_frame);
    // Get source mac address from network card driver
    // Fill in source and destination mac address
    memcpy(frame->src_mac_addr, RTL8139_MAC_ADDR(), 6);
    memcpy(frame->dst_mac_addr, dst_mac_addr, 6);
    // Fill in data
    memcpy(frame_data, data, len);
    // Fill in type
    frame->type = htons(protocol);
    // Send packet
    RTL8139_SEND_PACKET(frame, sizeof(struct ethernet_frame) + len);
    free(frame);
    return len;
}

void RTL8139_TEST(uint8_t bus, uint8_t device, uint8_t function) {
    RTL8139_INIT(bus, device, function);
    const uint8_t *mac_address = RTL8139_MAC_ADDR();
    const uint8_t data[] = {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF};
    printf("MAC Address: %d:%d:%d:%d:%d:%d\n", mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
    ethernet_send_packet((uint8_t*)mac_address, (uint8_t*)data, sizeof(data[0]) * sizeof(data), 0x0806);
    ethernet_send_packet((uint8_t*)mac_address, (uint8_t*)data, sizeof(data[0]) * sizeof(data), 0x0806);
    return;
}