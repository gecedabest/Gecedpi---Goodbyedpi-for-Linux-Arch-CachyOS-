#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>

#include "main.h"
#include "autotune.h"

#define PROBE_PORT           "443"
#define AT_START_DELAY_MS    500
#define CONNECT_TIMEOUT_MS   2000
#define RESPONSE_TIMEOUT_MS  1500
#define ATTEMPTS             2
#define SPEED_TOLERANCE_MS   10
#define NETWORK_DOWN_MS      2500
#define CLIENTHELLO_MAX      512

typedef struct {
    const char *name;
    int frag;
    int ttl;
    int fake;
} at_mode_t;

static const at_mode_t at_modes[] = {
    { "classic frag5 ttl5 fake",   5, 5, 1 },
    { "so nofrag ttl3 fake",       0, 3, 1 },
    { "so frag9 ttl5 fake",        9, 5, 1 },
    { "so frag5 ttl5 nofake",      5, 5, 0 },
    { "classic frag2 ttl5 fake",   2, 5, 1 },
    { "classic frag1 ttl5 fake",   1, 5, 1 },
    { "aggressive frag1 ttl3 fake",1, 3, 1 },
};
#define AT_NMODES (sizeof(at_modes) / sizeof(at_modes[0]))

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xff);
}

static void put24(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 16) & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)(v & 0xff);
}

static int build_clienthello(const char *host, uint8_t *out, size_t cap) {
    int hlen = (int)strlen(host);
    if (hlen < 1 || hlen > 255)
        return -1;

    static const uint16_t ciphers[] = {
        0x1301, 0x1302, 0x1303,
        0xc02b, 0xc02f, 0xc02c, 0xc030, 0xcca9, 0xcca8,
    };
    static const uint16_t groups[] = { 0x001d, 0x0017, 0x0018, 0x0019 };
    static const uint16_t sigalgs[] = {
        0x0403, 0x0503, 0x0603, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601,
    };

    uint8_t ext[512];
    int e = 0;

    put16(ext + e, 0x0000); e += 2;
    put16(ext + e, (uint16_t)(1 + 2 + hlen)); e += 2;
    ext[e++] = 0x00;
    put16(ext + e, (uint16_t)hlen); e += 2;
    memcpy(ext + e, host, (size_t)hlen); e += hlen;

    put16(ext + e, 0x000a); e += 2;
    put16(ext + e, (uint16_t)(sizeof(groups) / sizeof(groups[0]) * 2)); e += 2;
    for (size_t i = 0; i < sizeof(groups) / sizeof(groups[0]); i++) {
        put16(ext + e, groups[i]); e += 2;
    }

    put16(ext + e, 0x000d); e += 2;
    put16(ext + e, (uint16_t)(sizeof(sigalgs) / sizeof(sigalgs[0]) * 2)); e += 2;
    for (size_t i = 0; i < sizeof(sigalgs) / sizeof(sigalgs[0]); i++) {
        put16(ext + e, sigalgs[i]); e += 2;
    }

    put16(ext + e, 0x002b); e += 2;
    put16(ext + e, 1 + 4); e += 2;
    ext[e++] = 4;
    put16(ext + e, 0x0304); e += 2;
    put16(ext + e, 0x0303); e += 2;

    put16(ext + e, 0x000b); e += 2;
    put16(ext + e, 2); e += 2;
    ext[e++] = 1;
    ext[e++] = 0x00;

    uint8_t body[CLIENTHELLO_MAX];
    int b = 0;
    body[b++] = 0x03;
    body[b++] = 0x03;

    uint32_t seed = (uint32_t)time(NULL) ^ (uint32_t)getpid();
    for (int i = 0; i < 32; i++) {
        seed = seed * 1103515245 + 12345;
        body[b++] = (uint8_t)((seed >> 16) & 0xff);
    }

    body[b++] = 0x00;

    put16(body + b, (uint16_t)(sizeof(ciphers) / sizeof(ciphers[0]) * 2)); b += 2;
    for (size_t i = 0; i < sizeof(ciphers) / sizeof(ciphers[0]); i++) {
        put16(body + b, ciphers[i]); b += 2;
    }

    body[b++] = 0x01;
    body[b++] = 0x00;

    put16(body + b, (uint16_t)e); b += 2;
    memcpy(body + b, ext, (size_t)e); b += e;

    if (10 + b > (int)cap)
        return -1;

    out[0] = 0x16;
    out[1] = 0x03;
    out[2] = 0x01;
    put16(out + 3, (uint16_t)(4 + b));
    out[5] = 0x01;
    put24(out + 6, (uint32_t)b);
    memcpy(out + 10, body, (size_t)b);

    return 10 + b;
}

static int probe_once(const char *host, int *rtt_out) {
    struct addrinfo hints, *res = NULL, *r;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, PROBE_PORT, &hints, &res) != 0 || !res)
        return 0;

    struct timeval t_start;
    gettimeofday(&t_start, NULL);

    int ok = 0;
    for (r = res; r && !ok; r = r->ai_next) {
        int s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
        if (s < 0)
            continue;

        int fl = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, fl | O_NONBLOCK);

        int rc = connect(s, r->ai_addr, r->ai_addrlen);
        if (rc < 0 && errno == EINPROGRESS) {
            struct pollfd pfd = { s, POLLOUT, 0 };
            rc = poll(&pfd, 1, CONNECT_TIMEOUT_MS);
            if (rc > 0 && !(pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
                getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &sl);
                rc = (soerr == 0) ? 0 : -1;
            } else {
                rc = -1;
            }
        } else if (rc < 0) {
            rc = -1;
        }

        if (rc == 0) {
            uint8_t hello[CLIENTHELLO_MAX];
            int hlen = build_clienthello(host, hello, sizeof(hello));
            if (hlen > 0)
                (void)send(s, hello, (size_t)hlen, MSG_NOSIGNAL);

            struct pollfd p = { s, POLLIN, 0 };
            int done = 0;
            while (!done) {
                rc = poll(&p, 1, RESPONSE_TIMEOUT_MS);
                if (rc <= 0) {
                    ok = 1;
                    done = 1;
                    break;
                }
                if (p.revents & POLLIN) {
                    char buf[64];
                    ssize_t n = recv(s, buf, sizeof(buf), 0);
                    if (n > 0 || n == 0) {
                        ok = 1;
                        done = 1;
                        break;
                    }
                    if (n < 0) {
                        if (errno == ECONNRESET || errno == EPIPE ||
                            errno == ENOTCONN || errno == ETIMEDOUT) {
                            ok = 0;
                            done = 1;
                            break;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            continue;
                        ok = 1;
                        done = 1;
                        break;
                    }
                } else {
                    ok = 0;
                    done = 1;
                    break;
                }
            }
        }

        close(s);
    }

    freeaddrinfo(res);

    struct timeval t_end;
    gettimeofday(&t_end, NULL);
    if (rtt_out)
        *rtt_out = (int)((t_end.tv_sec - t_start.tv_sec) * 1000 +
                         (t_end.tv_usec - t_start.tv_usec) / 1000);

    return ok;
}

static void apply_mode(const at_mode_t *m) {
    g_cfg.fragment_size = m->frag;
    g_cfg.fragment_enabled = (m->frag > 0) ? 1 : 0;
    g_cfg.set_ttl = (uint8_t)m->ttl;
    g_cfg.fake_enabled = (m->fake != 0);
}

static int probe_mode(const at_mode_t *m, int *best_ms) {
    int ok_any = 0;
    int best = -1;
    for (int a = 0; a < ATTEMPTS; a++) {
        apply_mode(m);
        int ms = 0;
        if (probe_once(PROBE_HOST, &ms)) {
            ok_any = 1;
            if (best < 0 || ms < best)
                best = ms;
        } else if (ms >= NETWORK_DOWN_MS) {
            break;
        }
        usleep(100 * 1000);
    }
    *best_ms = best;
    return ok_any;
}

static void *autotune_worker(void *arg) {
    (void)arg;
    usleep(AT_START_DELAY_MS * 1000);

    int chosen = -1;
    int chosen_ms = -1;
    int best_ms = -1;

    for (size_t i = 0; i < AT_NMODES; i++) {
        int ms = -1;
        if (probe_mode(&at_modes[i], &ms)) {
            fprintf(stderr, "[gecedpi] auto: %s works (%d ms)\n",
                    at_modes[i].name, ms);
            if (chosen < 0) {
                chosen = (int)i;
                chosen_ms = ms;
                best_ms = ms;
            } else if (ms < best_ms - SPEED_TOLERANCE_MS) {
                chosen = (int)i;
                chosen_ms = ms;
                best_ms = ms;
                fprintf(stderr, "[gecedpi] auto: %s is faster, switching\n",
                        at_modes[i].name);
            } else {
                fprintf(stderr, "[gecedpi] auto: %s within tolerance, keeping %s\n",
                        at_modes[i].name, at_modes[chosen].name);
            }
        } else {
            fprintf(stderr, "[gecedpi] auto: %s blocked\n", at_modes[i].name);
            if (ms >= NETWORK_DOWN_MS) {
                fprintf(stderr, "[gecedpi] auto: network not ready, keeping default\n");
                break;
            }
        }
    }

    if (chosen >= 0) {
        apply_mode(&at_modes[chosen]);
        fprintf(stderr, "[gecedpi] auto: selected %s (%d ms)\n",
                at_modes[chosen].name, chosen_ms);
    } else {
        apply_mode(&at_modes[0]);
        fprintf(stderr, "[gecedpi] auto: no mode worked, falling back to classic\n");
    }

    return NULL;
}

int autotune_start(void) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, autotune_worker, NULL) != 0) {
        fprintf(stderr, "autotune: thread creation failed\n");
        return -1;
    }
    pthread_detach(tid);
    return 0;
}