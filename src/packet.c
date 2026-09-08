#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "main.h"
#include "packet.h"

int packet_parse(uint8_t *data, int len, parsed_pkt_t *out) {
    if (!data || len < (int)sizeof(struct iphdr) || !out)
        return -1;

    memset(out, 0, sizeof(*out));
    out->raw = data;
    out->raw_len = len;

    uint8_t version = (data[0] >> 4) & 0xF;

    if (version == 4) {
        if (len < (int)sizeof(struct iphdr))
            return -1;

        struct iphdr *ip = (struct iphdr *)data;
        int ihl = ip->ihl * 4;
        if (ihl < 20 || ihl > len)
            return -1;

        out->is_ipv6 = 0;
        out->ip4 = ip;

        if (ip->protocol == IPPROTO_TCP) {
            if (len < ihl + (int)sizeof(struct tcphdr))
                return -1;
            struct tcphdr *tcp = (struct tcphdr *)(data + ihl);
            int tcp_hdr_len = tcp->doff * 4;
            if (tcp_hdr_len < 20)
                return -1;
            out->tcp = tcp;
            int payload_offset = ihl + tcp_hdr_len;
            out->payload = data + payload_offset;
            out->payload_len = len - payload_offset;
            if (out->payload_len < 0)
                out->payload_len = 0;
        } else if (ip->protocol == IPPROTO_UDP) {
            if (len < ihl + (int)sizeof(struct udphdr))
                return -1;
            out->udp = (struct udphdr *)(data + ihl);
            int udp_hdr_len = sizeof(struct udphdr);
            out->payload = data + ihl + udp_hdr_len;
            out->payload_len = len - ihl - udp_hdr_len;
            if (out->payload_len < 0)
                out->payload_len = 0;
        }

    } else if (version == 6) {
        if (len < (int)sizeof(struct ip6_hdr))
            return -1;

        struct ip6_hdr *ip6 = (struct ip6_hdr *)data;
        out->is_ipv6 = 1;
        out->ip6 = ip6;

        int next_hdr = ip6->ip6_nxt;
        int offset = sizeof(struct ip6_hdr);

        while (next_hdr != IPPROTO_TCP && next_hdr != IPPROTO_UDP &&
               next_hdr != IPPROTO_NONE) {
            if (offset + 2 > len)
                return -1;
            if (next_hdr == IPPROTO_HOPOPTS || next_hdr == IPPROTO_ROUTING ||
                next_hdr == IPPROTO_DSTOPTS) {
                next_hdr = data[offset];
                offset += (data[offset + 1] + 1) * 8;
                if (offset > len)
                    return -1;
            } else {
                break;
            }
        }

        if (next_hdr == IPPROTO_TCP) {
            if (offset + (int)sizeof(struct tcphdr) > len)
                return -1;
            struct tcphdr *tcp = (struct tcphdr *)(data + offset);
            int tcp_hdr_len = tcp->doff * 4;
            if (tcp_hdr_len < 20)
                return -1;
            out->tcp = tcp;
            int payload_offset = offset + tcp_hdr_len;
            out->payload = data + payload_offset;
            out->payload_len = len - payload_offset;
            if (out->payload_len < 0)
                out->payload_len = 0;
        } else if (next_hdr == IPPROTO_UDP) {
            if (offset + (int)sizeof(struct udphdr) > len)
                return -1;
            out->udp = (struct udphdr *)(data + offset);
            out->payload = data + offset + sizeof(struct udphdr);
            out->payload_len = len - offset - sizeof(struct udphdr);
            if (out->payload_len < 0)
                out->payload_len = 0;
        }

    } else {
        return -1;
    }

    return 0;
}

uint16_t checksum_ip(const void *data, int len) {
    const uint16_t *ptr = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1)
        sum += *(const uint8_t *)ptr;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum);
}

struct tcp_pseudo4 {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint8_t  zero;
    uint8_t  proto;
    uint16_t tcp_len;
} __attribute__((packed));

uint16_t checksum_tcp4(const struct iphdr *ip, const struct tcphdr *tcp,
                        const uint8_t *payload, int payload_len) {
    struct tcp_pseudo4 pseudo;
    memset(&pseudo, 0, sizeof(pseudo));
    pseudo.src_addr = ip->saddr;
    pseudo.dst_addr = ip->daddr;
    pseudo.zero = 0;
    pseudo.proto = IPPROTO_TCP;
    int tcp_total = tcp->doff * 4 + payload_len;
    pseudo.tcp_len = htons((uint16_t)tcp_total);

    uint32_t sum = 0;

    const uint16_t *p = (const uint16_t *)&pseudo;
    for (int i = 0; i < (int)sizeof(pseudo) / 2; i++)
        sum += p[i];

    int tcp_hdr_len = tcp->doff * 4;
    uint8_t tcp_buf[60] = {0};
    memcpy(tcp_buf, tcp, tcp_hdr_len);
    ((struct tcphdr *)tcp_buf)->check = 0;
    p = (const uint16_t *)tcp_buf;
    for (int i = 0; i < tcp_hdr_len / 2; i++)
        sum += p[i];

    p = (const uint16_t *)payload;
    int plen = payload_len;
    while (plen > 1) {
        sum += *p++;
        plen -= 2;
    }
    if (plen == 1)
        sum += *(const uint8_t *)p;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum);
}

struct tcp_pseudo6 {
    struct in6_addr src_addr;
    struct in6_addr dst_addr;
    uint32_t        tcp_len;
    uint8_t         zeros[3];
    uint8_t         next_hdr;
} __attribute__((packed));

uint16_t checksum_tcp6(const struct ip6_hdr *ip6, const struct tcphdr *tcp,
                        const uint8_t *payload, int payload_len) {
    struct tcp_pseudo6 pseudo;
    memset(&pseudo, 0, sizeof(pseudo));
    memcpy(&pseudo.src_addr, &ip6->ip6_src, sizeof(struct in6_addr));
    memcpy(&pseudo.dst_addr, &ip6->ip6_dst, sizeof(struct in6_addr));
    int tcp_total = tcp->doff * 4 + payload_len;
    pseudo.tcp_len = htonl((uint32_t)tcp_total);
    memset(pseudo.zeros, 0, 3);
    pseudo.next_hdr = IPPROTO_TCP;

    uint32_t sum = 0;

    const uint16_t *p = (const uint16_t *)&pseudo;
    for (int i = 0; i < (int)sizeof(pseudo) / 2; i++)
        sum += p[i];

    int tcp_hdr_len = tcp->doff * 4;
    uint8_t tcp_buf[60] = {0};
    memcpy(tcp_buf, tcp, tcp_hdr_len);
    ((struct tcphdr *)tcp_buf)->check = 0;
    p = (const uint16_t *)tcp_buf;
    for (int i = 0; i < tcp_hdr_len / 2; i++)
        sum += p[i];

    p = (const uint16_t *)payload;
    int plen = payload_len;
    while (plen > 1) {
        sum += *p++;
        plen -= 2;
    }
    if (plen == 1)
        sum += *(const uint8_t *)p;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum);
}
