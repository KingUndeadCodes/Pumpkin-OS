#include "udp.h"
#include "../port.cpp"
#include "../serial/serial.h"
#include <stdlib.h>
#include <string.h>

namespace UserDatagramProtocol {
    struct Listener {
        uint16_t port;
        udp_handler_t handler;
        bool used;
    };
    static Listener listeners[UDP_MAX_LISTENERS];
    // RFC 768 checksum: ones-complement sum of a pseudo-header (src ip, dst
    // ip, zero byte, protocol, udp length) followed by the udp header (with
    // checksum field zeroed) and data. Built as one contiguous byte buffer
    // and summed as big-endian 16-bit words -- this must match the packing
    // net.py's build_udp() uses so both sides agree on the wire format.
    static uint16_t udp_checksum(uint8_t* src_ip, uint8_t* dst_ip, udp_header_t* udp, uint16_t len) {
        uint16_t pseudo_len = 12; // 4 (src) + 4 (dst) + 1 (zero) + 1 (proto) + 2 (length)
        uint16_t buf_len = pseudo_len + len + (len & 1); // pad to an even length
        uint8_t* buf = (uint8_t*)malloc(buf_len);
        if (!buf) return 0;
        memset(buf, 0, buf_len);
        memcpy(buf, src_ip, 4);
        memcpy(buf + 4, dst_ip, 4);
        buf[8] = 0;
        buf[9] = PROTOCOL_UDP;
        buf[10] = (uint8_t)(len >> 8);
        buf[11] = (uint8_t)(len & 0xff);
        memcpy(buf + pseudo_len, udp, len);
        uint32_t sum = 0;
        for (uint16_t i = 0; i < buf_len; i += 2) sum += ((uint16_t)buf[i] << 8) | buf[i + 1];
        free(buf);
        while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
        uint16_t result = (uint16_t)(~sum & 0xffff);
        return result ? result : 0xffff; // 0 means "no checksum" per RFC 768
    }
    bool sendPacket(uint8_t* dst_ip, uint16_t src_port, uint16_t dst_port, const void* data, uint16_t len) {
        uint16_t total_len = sizeof(udp_header_t) + len;
        udp_header_t* udp = (udp_header_t*)malloc(total_len);
        if (!udp) return false;
        udp->src_port = htons(src_port);
        udp->dst_port = htons(dst_port);
        udp->length = htons(total_len);
        udp->checksum = 0;
        memcpy(udp->data, data, len);
        udp->checksum = htons(udp_checksum(my_ip, dst_ip, udp, total_len));
        bool ok = InternetProtocol::sendPacket(dst_ip, udp, total_len, PROTOCOL_UDP);
        free(udp);
        return ok;
    }
    void handlePacket(ip_packet_t* packet, udp_header_t* udp, uint16_t len) {
        uint16_t dport = ntohs(udp->dst_port);
        uint16_t sport = ntohs(udp->src_port);
        uint16_t data_len = (len > sizeof(udp_header_t)) ? (len - sizeof(udp_header_t)) : 0;
        for (int i = 0; i < UDP_MAX_LISTENERS; i++) {
            if (listeners[i].used && listeners[i].port == dport) {
                listeners[i].handler(packet->src_ip, sport, udp->data, data_len);
                return;
            }
        }
        serial_write_string("UDP: no listener for port, dropping.\n", false, INFO);
    }
    bool listen(uint16_t port, udp_handler_t handler) {
        for (int i = 0; i < UDP_MAX_LISTENERS; i++) {
            if (listeners[i].used && listeners[i].port == port) return false; // already taken
        }
        for (int i = 0; i < UDP_MAX_LISTENERS; i++) {
            if (!listeners[i].used) {
                listeners[i].used = true;
                listeners[i].port = port;
                listeners[i].handler = handler;
                return true;
            }
        }
        return false;
    }
    void unlisten(uint16_t port) {
        for (int i = 0; i < UDP_MAX_LISTENERS; i++) {
            if (listeners[i].used && listeners[i].port == port) {
                listeners[i].used = false;
                return;
            }
        }
    }
}
