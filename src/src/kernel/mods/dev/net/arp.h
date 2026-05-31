#ifndef __ARP_H__
#define __ARP_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../port.cpp"

typedef struct arp_packet {
    uint16_t hardware_type;
    uint16_t protocol;
    uint8_t hardware_addr_len;
    uint8_t protocol_addr_len;
    uint16_t opcode;
    uint8_t src_hardware_addr[6];
    uint8_t src_protocol_addr[4];
    uint8_t dst_hardware_addr[6];
    uint8_t dst_protocol_addr[4];
} __attribute__((packed)) arp_packet_t;

typedef struct arp_table_entry {
    uint32_t ip_addr;
    uint64_t mac_addr;
} arp_table_entry_t;

namespace AddressResolutionProtocol {
    void handlePacket(arp_packet_t * arp_packet, int len);
    void sendPacket(uint8_t * dst_hardware_addr, uint8_t * dst_protocol_addr);
    int lookup(uint8_t * ret_hardware_addr, uint8_t * ip_addr);
    void lookupAdd(uint8_t * ret_hardware_addr, uint8_t * ip_addr);
    void initalize();
}

#endif
