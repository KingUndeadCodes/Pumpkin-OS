#include "ip.h"
#include "arp.h"
#include "udp.h"
#include "../port.cpp"
#include "../serial/serial.h"
#include "../pci/drivers/rtl8139.h"
#include "../pit/pit.h"

uint8_t my_ip[] = {10, 0, 2, 14};
uint8_t zero_hardware_addr[] = {0,0,0,0,0,0};

// https://raw.githubusercontent.com/szhou42/osdev/refs/heads/master/src/kernel/network/ip.c
// https://raw.githubusercontent.com/szhou42/osdev/refs/heads/master/src/include/ip.h

namespace InternetProtocol {
    void convertString(char* ip_str, uint8_t* ip) {
        sprintf(ip_str, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    };
    uint16_t calculateChecksum(ip_packet_t* packet) {
        // Treat the packet header as a 2-byte-integer array
        // Sum all integers up and flip all bits
        int array_size = sizeof(ip_packet_t) / 2;
        uint16_t * array = (uint16_t*)packet;
        uint8_t * array2 = (uint8_t*)packet;
        uint32_t sum = 0;
        for(int i = 0; i < array_size; i++) {
            sum += flip_short(array[i]);
        }
        uint32_t carry = sum >> 16;
        sum = sum & 0x0000ffff;
        sum = sum + carry;
        uint16_t ret = ~sum;
        return ret;
    };
    bool sendPacket(uint8_t* dst_ip, void* data, int len, uint8_t protocol) {
        ip_packet_t* packet = (ip_packet_t*)malloc(sizeof(ip_packet_t) + len);
        memset(packet, 0, sizeof(ip_packet_t));
        packet->version = IP_IPV4;
        // 5 * 4 = 20 byte
        packet->ihl = 5;
        // Don't care, set to 0
        packet->tos = 0;
        packet->length = sizeof(ip_packet_t) + len;
        // Used for ip fragmentation, don't care now
        packet->id = 0;
        // Tell router to not divide the packet, and this is packet is the last piece of the fragments.
        packet->flags = 0;
        packet->fragment_offset_high = 0;
        packet->fragment_offset_low = 0;
        packet->ttl = 64;
        packet->protocol = protocol;
        memcpy(packet->src_ip, my_ip, 4);
        memcpy(packet->dst_ip, dst_ip, 4);
        void * packet_data = (void*)packet + packet->ihl * 4;
        memcpy(packet_data, data, len);
        // Fix packet data order
        *((uint8_t*)(&packet->version_ihl_ptr)) = htonb(*((uint8_t*)(&packet->version_ihl_ptr)), 4);
        *((uint8_t*)(packet->flags_fragment_ptr)) = htonb(*((uint8_t*)(packet->flags_fragment_ptr)), 3);
        packet->length = htons(sizeof(ip_packet_t) + len);
        // Make sure checksum is 0 before checksum calculation
        packet->header_checksum = 0;
        packet->header_checksum = htons(calculateChecksum(packet));
        // If the ip is in the same network, the destination mac address is the routers's mac address, the router'll figure out how to route the packet
        // Now, again, let's assume it's always in the same network, because i want to test if the simplest ip packet sending works as i write the code
        // Now look at the arp table -- if we have the mac address, just send it. If not, send an arp
        // request and retry with a real delay between attempts, bailing out (instead of spinning
        // forever) if nothing answers after a handful of tries.
        uint8_t dst_hardware_addr[6];
        int arp_attempts = 5;
        while (!AddressResolutionProtocol::lookup(dst_hardware_addr, dst_ip)) {
            if (arp_attempts <= 0) {
                serial_write_string("IP: ARP resolution timed out; dropping packet.\n", false, FAIL);
                free(packet);
                return false;
            }
            AddressResolutionProtocol::sendPacket(zero_hardware_addr, dst_ip);
            arp_attempts--;
            timer_wait(200); // give a reply time to arrive before retrying
        }
        serial_write_string("IP Packet Sent!\n");
        ethernet_send_packet(dst_hardware_addr, (unsigned char*)packet, htons(packet->length), ETHERNET_TYPE_IP);
        free(packet);
        return true;
    };
    void handlePacket(ip_packet_t* packet, uint8_t* src_mac) {
        *((uint8_t*)(&packet->version_ihl_ptr)) = ntohb(*((uint8_t*)(&packet->version_ihl_ptr)), 4);
        *((uint8_t*)(packet->flags_fragment_ptr)) = ntohb(*((uint8_t*)(packet->flags_fragment_ptr)), 3);
        if (packet->version != IP_IPV4) return;

        // Opportunistically learn the sender's IP -> MAC mapping from any IP
        // traffic, not just explicit ARP exchanges, so replying (e.g. a UDP
        // echo) doesn't need its own ARP round-trip first.
        AddressResolutionProtocol::lookupAdd(src_mac, packet->src_ip);

        void* data_ptr = (void*)packet + packet->ihl * 4;
        int data_len = ntohs(packet->length) - (packet->ihl * 4);
        if (data_len < 0) return;

        if (packet->protocol == PROTOCOL_UDP && data_len >= (int)sizeof(udp_header_t)) {
            UserDatagramProtocol::handlePacket(packet, (udp_header_t*)data_ptr, (uint16_t)data_len);
        }
    };
}