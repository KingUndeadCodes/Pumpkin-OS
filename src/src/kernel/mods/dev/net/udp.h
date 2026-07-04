#ifndef __UDP_H__
#define __UDP_H__

#include <stddef.h>
#include <stdint.h>
#include "ip.h"

typedef struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;   // header + data, in bytes, network byte order
    uint16_t checksum;
    uint8_t data[];
} __attribute__((packed)) udp_header_t;

// data/len point directly into the packet buffer owned by the receive path
// (freed once the calling chain returns) -- copy anything that needs to
// outlive the handler call.
typedef void (*udp_handler_t)(uint8_t* src_ip, uint16_t src_port, void* data, uint16_t len);

#define UDP_MAX_LISTENERS 8

namespace UserDatagramProtocol {
    bool sendPacket(uint8_t* dst_ip, uint16_t src_port, uint16_t dst_port, const void* data, uint16_t len);
    void handlePacket(ip_packet_t* packet, udp_header_t* udp, uint16_t len);
    bool listen(uint16_t port, udp_handler_t handler);
    void unlisten(uint16_t port);
}

#endif
