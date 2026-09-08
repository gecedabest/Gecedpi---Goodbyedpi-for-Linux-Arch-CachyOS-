#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "main.h"
#include "packet.h"
#include "fragment.h"

static int send_tcp_segment4(const parsed_pkt_t *pkt,
                              const uint8_t *payload, int payload_len,
                              uint32_t seq_offset)
{
    int ip_hdr_len  = pkt->ip4->ihl * 4;
    int tcp_hdr_len = pkt->tcp->doff * 4;
    int total_len   = ip_hdr_len + tcp_hdr_len + payload_len;

    if (total_len > MAX_PACKET_SIZE)
        return -1;

    uint8_t buf[MAX_PACKET_SIZE];
    memset(buf, 0, total_len);

    memcpy(buf, pkt->ip4, ip_hdr_len);
    struct iphdr *ip = (struct iphdr *)buf;
    ip->tot_len = htons((uint16_t)total_len);
    ip->id = htons(ntohs(ip->id) + (uint16_t)(seq_offset > 0 ? 1 : 0));
    ip->check = 0;
    ip->check = checksum_ip(ip, ip_hdr_len);

    memcpy(buf + ip_hdr_len, pkt->tcp, tcp_hdr_len);
    struct tcphdr *tcp = (struct tcphdr *)(buf + ip_hdr_len);

    if (seq_offset > 0)
        tcp->seq = htonl(ntohl(tcp->seq) + seq_offset);

    if (payload_len > 0)
        memcpy(buf + ip_hdr_len + tcp_hdr_len, payload, payload_len);

    tcp->check = 0;
    tcp->check = checksum_tcp4(ip, tcp,
                               buf + ip_hdr_len + tcp_hdr_len,
                               payload_len);

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family      = AF_INET;
    dst.sin_addr.s_addr = ip->daddr;

    ssize_t sent = sendto(g_rawsock4, buf, total_len, 0,
                          (struct sockaddr *)&dst, sizeof(dst));
    if (sent < 0) {
        perror("sendto(fragment)");
        return -1;
    }
    debug("fragment: sent segment seq_off=%u len=%d (%zd bytes on wire)\n",
          seq_offset, payload_len, sent);
    return 0;
}

int send_fragment_pair(const parsed_pkt_t *pkt, int frag_size) {
    if (!pkt->ip4 || !pkt->tcp)
        return -1;

    int payload_len = pkt->payload_len;
    const uint8_t *payload = pkt->payload;

    if (payload_len <= frag_size)
        return send_tcp_segment4(pkt, payload, payload_len, 0);

    int ret = send_tcp_segment4(pkt, payload, frag_size, 0);
    if (ret != 0) return ret;

    ret = send_tcp_segment4(pkt, payload + frag_size,
                             payload_len - frag_size,
                             (uint32_t)frag_size);
    return ret;
}
