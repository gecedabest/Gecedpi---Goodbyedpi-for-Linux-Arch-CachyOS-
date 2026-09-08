#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#define IS_TLS_CLIENTHELLO(payload, len) \
    ((len) >= 3 && (payload)[0] == 0x16 && (payload)[1] == 0x03)

#define IS_HTTP_REQUEST(payload, len) \
    ((len) >= 4 && \
     (memcmp((payload), "GET ", 4) == 0 || \
      memcmp((payload), "POST", 4) == 0 || \
      memcmp((payload), "HEAD", 4) == 0 || \
      memcmp((payload), "CONN", 4) == 0))

typedef struct {
    int is_ipv6;
    struct iphdr   *ip4;
    struct ip6_hdr *ip6;
    struct tcphdr  *tcp;
    struct udphdr  *udp;
    uint8_t        *payload;
    int             payload_len;
    uint8_t        *raw;
    int             raw_len;
} parsed_pkt_t;

int packet_parse(uint8_t *data, int len, parsed_pkt_t *out);
uint16_t checksum_ip(const void *data, int len);
uint16_t checksum_tcp4(const struct iphdr *ip, const struct tcphdr *tcp,
                        const uint8_t *payload, int payload_len);
uint16_t checksum_tcp6(const struct ip6_hdr *ip6, const struct tcphdr *tcp,
                        const uint8_t *payload, int payload_len);

#endif
