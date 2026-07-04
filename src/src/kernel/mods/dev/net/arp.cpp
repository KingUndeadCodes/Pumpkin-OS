#include "arp.h"
#include "ip.h"
#include "../pci/drivers/rtl8139.h"
#include "../serial/serial.h"

#define ARP_REQUEST 1
#define ARP_REPLY 2

// https://github.com/szhou42/osdev/blob/master/src/kernel/network/arp.c
// https://github.com/szhou42/osdev/blob/master/src/include/arp.h#L20

uint8_t broadcast_mac_address[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
arp_table_entry_t arp_table[512];
int arp_table_size;
int arp_table_curr;

namespace AddressResolutionProtocol {
    void handlePacket(arp_packet_t * arp_packet, int len) {
        // Save the source hardware and protocol address fields
        uint8_t dst_hardware_addr[6];
        uint8_t dst_protocol_addr[4];
        memcpy(dst_hardware_addr, arp_packet->src_hardware_addr, 6);
        memcpy(dst_protocol_addr, arp_packet->src_protocol_addr, 4);
        // Get the ARP opcode
        uint16_t op = ntohs(arp_packet->opcode);
        switch (op) {
            case ARP_REQUEST: {
                if (memcmp(arp_packet->dst_protocol_addr, my_ip, 4) == 0) {
                    serial_write_string("Received ARP request for my IP address.\n");
                    // Set source MAC address, IP address
                    uint8_t* macAddress = RTL8139_MAC_ADDR();
                    memcpy(arp_packet->src_hardware_addr, macAddress, 6);
                    memcpy(arp_packet->src_protocol_addr, my_ip, 4);
                    // Set destination MAC address, IP address
                    memcpy(arp_packet->dst_hardware_addr, arp_packet->src_hardware_addr, 6);
                    memcpy(arp_packet->dst_protocol_addr, arp_packet->src_protocol_addr, 4);
                    // Set opcode
                    arp_packet->opcode = htons(/* ARP_REPLY */ 0x02);
                    // Set lengths
                    arp_packet->hardware_addr_len = 6;
                    arp_packet->protocol_addr_len = 4;
                    // Set hardware type
                    arp_packet->hardware_type = htons(0x01); // Ethernet
                    // Set protocol = IPv4
                    arp_packet->protocol = htons(0x0800); // IPv4
                    ethernet_send_packet(dst_hardware_addr, (uint8_t*)arp_packet, sizeof(arp_packet_t), 0x0806);
                    serial_write_string("Sent ARP reply.\n");
                } else {
                    serial_write_string("Received ARP request for a different IP address.\n");
                    // Ignore the ARP request, we don't care about it.
                }
                break;
            }
            case ARP_REPLY: break;
            default: break;
        }
        // Now, store the ip-mac address mapping relation
        memcpy(&arp_table[arp_table_curr].ip_addr, dst_protocol_addr, 4);
        memcpy(&arp_table[arp_table_curr].mac_addr, dst_hardware_addr, 6);
        if(arp_table_size < 512) arp_table_size++;
        arp_table_curr++;
        // Wrap around
        if(arp_table_curr >= 512) arp_table_curr = 0;
    }
    void sendPacket(uint8_t* dst_hardware_addr, uint8_t* dst_protocol_addr) {
        arp_packet_t* arp_packet = (arp_packet_t*)malloc(sizeof(arp_packet_t));
        uint8_t* macAddress = RTL8139_MAC_ADDR();
        memcpy(arp_packet->src_hardware_addr, macAddress, 6);
        memcpy(arp_packet->src_protocol_addr, my_ip, 4);
        memcpy(arp_packet->dst_hardware_addr, dst_hardware_addr, 6);
        memcpy(arp_packet->dst_protocol_addr, dst_protocol_addr, 4);
        // Set opcode
        arp_packet->opcode = htons(ARP_REQUEST);
        // Set lengths
        arp_packet->hardware_addr_len = 6;
        arp_packet->protocol_addr_len = 4;
        // Set hardware type
        arp_packet->hardware_type = htons(0x01);
        // Set protocol = IPv4
        arp_packet->protocol = htons(0x0806);
        // Now send it with ethernet
        ethernet_send_packet(broadcast_mac_address, (uint8_t*)arp_packet, sizeof(arp_packet_t), 0x0806);
        // Free the arp_packet
        free(arp_packet);
    }
    int lookup(uint8_t * ret_hardware_addr, uint8_t * ip_addr) {
        uint32_t ip_entry = *((uint32_t*)(ip_addr));
        for(int i = 0; i < 512; i++) {
            if(arp_table[i].ip_addr == ip_entry) {
                memcpy(ret_hardware_addr, &arp_table[i].mac_addr, 6);
                return 1;
            }
        }
        return 0;
    }
    void lookupAdd(uint8_t * ret_hardware_addr, uint8_t * ip_addr) {
        memcpy(&arp_table[arp_table_curr].ip_addr, ip_addr, 4);
        memcpy(&arp_table[arp_table_curr].mac_addr, ret_hardware_addr, 6);
        if(arp_table_size < 512) arp_table_size++;
        arp_table_curr++;
        // Wrap around
        if(arp_table_curr >= 512) arp_table_curr = 0;
    }
    void initalize() {
        uint8_t broadcast_ip[4];
        uint8_t broadcast_mac[6];
        // Added these
        arp_table_size = 0;
        arp_table_curr = 0;
        // -------------
        memset(broadcast_ip, 0xff, 4);
        memset(broadcast_mac, 0xff, 6);
        AddressResolutionProtocol::lookupAdd(broadcast_mac, broadcast_ip);
    }
}