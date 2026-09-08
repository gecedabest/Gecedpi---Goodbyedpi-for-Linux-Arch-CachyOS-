#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#ifndef NF_ACCEPT
#define NF_ACCEPT 1
#endif
#ifndef NF_DROP
#define NF_DROP 0
#endif

#include <libnetfilter_queue/libnetfilter_queue.h>

#include "main.h"
#include "nfqueue.h"
#include "packet.h"
#include "conntrack.h"
#include "fakepackets.h"
#include "fragment.h"

#define NFQ_BUFSIZE (65536 + 4096)

static int gdpi_nfq_callback(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
                              struct nfq_data *nfa, void *data)
{
    (void)nfmsg;
    (void)data;

    uint32_t id = 0;
    struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
    if (ph)
        id = ntohl(ph->packet_id);

    uint8_t *raw_pkt = NULL;
    int pkt_len = nfq_get_payload(nfa, &raw_pkt);
    if (pkt_len < 0 || !raw_pkt)
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

    parsed_pkt_t pkt;
    if (packet_parse(raw_pkt, pkt_len, &pkt) != 0)
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

    if (!pkt.tcp || pkt.payload_len <= 0)
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

    if (conntrack_is_done(&pkt))
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

    uint16_t dport = ntohs(pkt.tcp->dest);
    if (dport != 80 && dport != 443)
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

    int is_https = IS_TLS_CLIENTHELLO(pkt.payload, pkt.payload_len);
    int is_http  = IS_HTTP_REQUEST(pkt.payload, pkt.payload_len);

    if (!is_https && !is_http)
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

    debug("nfq: dport=%d payload_len=%d is_https=%d is_http=%d\n",
          dport, pkt.payload_len, is_https, is_http);

    if (g_cfg.fake_enabled && g_cfg.set_ttl > 0) {
        if (is_https)
            send_fake_https(&pkt, g_cfg.set_ttl);
        else
            send_fake_http(&pkt, g_cfg.set_ttl);
    }

    if (g_cfg.fragment_enabled && pkt.ip4 && g_rawsock4 >= 0) {
        if (send_fragment_pair(&pkt, g_cfg.fragment_size) == 0) {
            conntrack_mark_done(&pkt);
            return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
        }
    }

    conntrack_mark_done(&pkt);
    return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
}

void nfqueue_loop(int queue_num) {
    struct nfq_handle *h = nfq_open();
    if (!h) {
        fprintf(stderr, "nfq_open() failed. Is the kernel netfilter_queue module loaded?\n");
        return;
    }

    if (nfq_unbind_pf(h, AF_INET) < 0) {
    }
    if (nfq_bind_pf(h, AF_INET) < 0) {
        fprintf(stderr, "nfq_bind_pf(AF_INET) failed\n");
        nfq_close(h);
        return;
    }

    struct nfq_q_handle *qh = nfq_create_queue(h, queue_num, &gdpi_nfq_callback, NULL);
    if (!qh) {
        fprintf(stderr, "nfq_create_queue(%d) failed. Is another instance running?\n", queue_num);
        nfq_close(h);
        return;
    }

    if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xFFFF) < 0) {
        fprintf(stderr, "nfq_set_mode(COPY_PACKET) failed\n");
        nfq_destroy_queue(qh);
        nfq_close(h);
        return;
    }

    if (nfq_set_queue_maxlen(qh, 8192) < 0)
        fprintf(stderr, "Warning: nfq_set_queue_maxlen() failed (kernel desteklemiyor)\n");

#ifdef NFQA_CFG_F_FAIL_OPEN
    if (nfq_set_queue_flags(qh, NFQA_CFG_F_FAIL_OPEN, NFQA_CFG_F_FAIL_OPEN) < 0) {
        fprintf(stderr, "Warning: NFQA_CFG_F_FAIL_OPEN not supported\n");
    }
#endif

    int fd = nfq_fd(h);

    uint8_t *buf = malloc(NFQ_BUFSIZE);
    if (!buf) {
        perror("malloc(nfq buf)");
        nfq_destroy_queue(qh);
        nfq_close(h);
        return;
    }

    int rcvbuf = 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    fprintf(stderr, "NFQUEUE %d ready.\n", queue_num);

    while (g_running) {
        ssize_t rv = recv(fd, buf, NFQ_BUFSIZE, 0);
        if (rv < 0) {
            if (errno == EINTR || errno == ENOBUFS) {
                if (errno == ENOBUFS)
                    fprintf(stderr, "Warning: NFQUEUE buffer overflow, packets dropped\n");
                continue;
            }
            perror("recv(nfq)");
            break;
        }
        nfq_handle_packet(h, (char *)buf, (int)rv);
    }

    free(buf);
    nfq_destroy_queue(qh);
    nfq_close(h);
}
