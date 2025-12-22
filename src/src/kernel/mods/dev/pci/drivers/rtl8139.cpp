#include "../../memory/allocator.h"
#include "../../paging/paging.h"
#include "../../serial/serial.h"
#include "../../idt/irq.h"
#include "../../idt/idt.h"
#include "../../pci/pci.h"
#include "../../net/arp.h"
#include "../../net/ip.h"
#include "../../port.cpp"
#include "rtl8139.h"

/**
 * RESOURCES:
 *  #1 - https://www.wfbsoftware.de/realtek-rtl8139-network-interface-card/
 *  #2 - https://github.com/0Nera/SynapseOS/blob/a39388115994372d80279bb1ed80d19562f23f9c/kernel/src/drivers/network/RTL8139.c
 *  #3 - https://github.com/szhou42/osdev/blob/master/src/kernel/drivers/rtl8139.c#L111
 *  #4 - https://android.googlesource.com/platform/external/syslinux/+/refs/heads/oreo-m6-s4-release/gpxe/src/drivers/net/rtl8139.c
 *  #5 - https://github.com/narke/Aragveli/blob/a34d0c97a50792e48859bad87f5cffb5691e2fff/src/kernel/arch/x86/pic.c
 *  #6 - https://forum.osdev.org/viewtopic.php?f=1&t=27901
 *  #7 - https://github.com/szhou42/osdev/blob/master/src/kernel/network/ip.c
 * TODO:
 * - 'RTL8139_RECEIVE_PACKET' needs to be re-written.
*/

static struct RTL8139 NICDevice;

void copyNIC(struct RTL8139* nic) {
    NICDevice.bus = nic->bus;
    NICDevice.device = nic->device;
    NICDevice.function = nic->function;
    NICDevice.tx_current = nic->tx_current;
    NICDevice.rx_buffer = nic->rx_buffer;
    NICDevice.ioaddr = nic->ioaddr;
    NICDevice.current_packet_ptr = nic->current_packet_ptr;
    memcpy(NICDevice.mac_address, nic->mac_address, 6);
}

/*
static struct RTL8139* getNIC(struct RTL8139* nic) {
    nic->bus = NICDevice.bus;
    nic->device = NICDevice.device;
    nic->function = NICDevice.function;
    nic->tx_current = NICDevice.tx_current;
    nic->rx_buffer = NICDevice.rx_buffer;
    nic->ioaddr = NICDevice.ioaddr;
    nic->current_packet_ptr = NICDevice.current_packet_ptr;
    memcpy(nic->mac_address, NICDevice.mac_address, 6);
    return nic;
}

inline uint16_t htons(uint16_t hostshort) {
    uint32_t first_byte = *((uint8_t*)(&hostshort));
    uint32_t second_byte = *((uint8_t*)(&hostshort) + 1);
    return (first_byte << 8) | (second_byte);
}

inline uint16_t ntohs(uint16_t netshort) {
    uint32_t first_byte = *((uint8_t*)(&netshort));
    uint32_t second_byte = *((uint8_t*)(&netshort) + 1);
    return (first_byte << 8) | (second_byte);
}
*/

enum EthernetPacketType {
    // Internet Protocol version 4 (IPv4)
    IPv4 = 0x0800,
    // Address Resolution Protocol (ARP)
    ARP = 0x0806,
    // Internet Protocol version 6 (IPv6)
    IPv6 = 0x86DD,
};

void ethernet_handle_packet(ethernet_frame* packet, int len) {
    void* data = (void*)packet + sizeof(ethernet_frame);
    int data_len = len - sizeof(ethernet_frame);
    // serial_write_string("Received Ethernet packet\n");
    switch (ntohs(packet->type)) {
        case EthernetPacketType::ARP: {
            AddressResolutionProtocol::handlePacket(data, data_len);
            // serial_write_string("Received ARP packet\n");
            break;
        } 
        case EthernetPacketType::IPv4: {
            // Handle IP packet
            InternetProtocol::handlePacket((ip_packet_t*)data);
            serial_write_string("Received IPv4 packet\n");
            break;
        }
        /*
        case EthernetPacketType::IPv6: {
            serial_write_string("Received IPv6 packet\n");
            break;
        }
        */
        default: {
            serial_write_string("Unknown packet type detected.\n");
            break;
        }
    }
}

void RTL8139_RECEIVE_PACKET() {
    // serial_write_string("\n[RTL8139] Packet Recieved!\n", false, NONE);
    uint16_t * t = (uint16_t*)(NICDevice.rx_buffer + NICDevice.current_packet_ptr);
    uint16_t packet_length = *(t + 1);
    t = t + 2;
    void* packet = malloc(packet_length);
    memcpy(packet, t, packet_length);
    ethernet_handle_packet(packet, packet_length);
    NICDevice.current_packet_ptr = (NICDevice.current_packet_ptr + packet_length + 4 + 3) & RX_READ_POINTER_MASK;
    if(NICDevice.current_packet_ptr > RX_BUFFER_SIZE) NICDevice.current_packet_ptr -= RX_BUFFER_SIZE; // RX_BUFFER_SIZE was 8192 before.
    outw(NICDevice.ioaddr + 0x38, NICDevice.current_packet_ptr - 0x10);
    free(packet);
    return;
}

void RTL8139_SEND_PACKET(void* data, uint32_t len) {
    unsigned char tx_buffer[len];
    memcpy(tx_buffer, data, len);
    outl(NICDevice.ioaddr + TSAD_array[NICDevice.tx_current], (uint32_t)tx_buffer);
    uint32_t status = 0;
    status |= len & 0x1FFFF;
    status |= 0 << 13;
    outl(NICDevice.ioaddr + TSD_array[NICDevice.tx_current++], status);
    uint32_t transmit_ok = inl(NICDevice.ioaddr + TSD_array[NICDevice.tx_current - 1]);
    while (transmit_ok & (1 << 15) == 0) {
        Logging::log("[RTL8139] Waiting for transmit_ok ...");
        transmit_ok = inl(NICDevice.ioaddr + TSD_array[NICDevice.tx_current - 1]);
    }
    if(NICDevice.tx_current > 3) NICDevice.tx_current = 0;
}

void RTL8139_HANDLER(struct regs* r) {
    // Logging::log("[RTL8139] Interupt Fired!");
    uint16_t irq = inw(NICDevice.ioaddr + 0x3E);
    if (irq & (1<<0)) RTL8139_RECEIVE_PACKET();
    if (irq & (1<<2)) Logging::log("[RTL8139] Packet sent!");
    outw(NICDevice.ioaddr + 0x3E, 0x5);
}

// TODO: Redo this function.
uint8_t* RTL8139_MAC_ADDR(void) {
    /*
    static uint8_t mac[6];
    for (int i = 0; i < 7; i++) mac[i] = NICDevice.mac_address[i];
    return mac;
    */
    return NICDevice.mac_address;
}

// echo "test" | nc -u 127.0.0.1 12345

// TODO: Redo the rx_buffer to be a pointer to a physical address with size of RX_BUFFER.
void RTL8139_INIT(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t command_port = pciConfigReadWord(bus, device, function, 4);
    if(!(command_port >> 2 & 1)) pciConfigWriteWord(bus, device, function, 4, command_port | TOK);
    struct RTL8139 NIC;
    NIC.bus = bus;
    NIC.device = device;
    NIC.function = function;
    NIC.tx_current = 0;
    NIC.current_packet_ptr = 0;
    // IO
    dma_buffer_t allocation = dma_alloc(16 * 1024, 4096);
    NIC.rx_buffer = (uint8_t*)allocation.virt;   // Set the rx_buffer to       
    // END IO
    NIC.ioaddr = pciConfigReadWord(bus, device, function, 0x10) & ~3;
    outb(NIC.ioaddr + 0x52, 0x0);
    outb(NIC.ioaddr + 0x37, 0x10);
    while((inb(NIC.ioaddr + 0x37) & 0x10) != 0) {};
    outl(NIC.ioaddr + 0x30, (uint32_t)allocation.phys); // outl(NIC.ioaddr + 0x30, (uint32_t)NIC.rx_buffer);
    outl(NIC.ioaddr + 0x44, RCR_DEFAULT);
    outl(NIC.ioaddr + 0x40, TXCFG_DEFAULT); 
    outw(NIC.ioaddr + 0x3C, 0x0005);
    outb(NIC.ioaddr + 0x37, 0x0C);
    irq_install_handler((int)(pciConfigReadWord(bus, device, function, 0x3C) & 0xFF), RTL8139_HANDLER);
    for (int i = 0; i < 6; i++) NIC.mac_address[i] = inb(pciConfigReadWord(bus, device, function, 0x10) + i - 1);
    copyNIC(&NIC); // NICDevice = NIC;
    return;
};

int ethernet_send_packet(uint8_t* dst_mac_addr, uint8_t * data, int len, uint16_t protocol) {
    // printf("%d", len);
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

// fix `ethernet_frame` to be bigger.
void RTL8139_TEST(uint8_t bus, uint8_t device, uint8_t function) {
    RTL8139_INIT(bus, device, function);
    /*
    const uint8_t *mac_address = RTL8139_MAC_ADDR();
    printf("MAC Address: %d:%d:%d:%d:%d:%d\n", mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
    const uint8_t ARPPacket1[] = {0, 1, 8, 0, 6, 4, 0, 1, mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]};
    const uint8_t ARPPacket2[] = {192, 168, 1, 68, 65, 100, 58, 87, 19, 94, 192, 168, 1, 4};
    const uint8_t ARPLength = 28;
    uint8_t* data = (uint8_t*)malloc(ARPLength);
    memcpy(data+0x0, ARPPacket1, sizeof(ARPPacket1));
    memcpy(data+0xE, ARPPacket2, sizeof(ARPPacket2));
    ethernet_send_packet((uint8_t*)mac_address, data, sizeof(uint8_t) * ARPLength, 0x0806);
    ethernet_send_packet((uint8_t*)mac_address, data, sizeof(uint8_t) * ARPLength, 0x0806);
    free(data);
    */
    AddressResolutionProtocol::initalize();
    return;
}