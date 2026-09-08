#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

#define GECEDPI_VERSION "v1.0-gecedpi"
#define MAX_PACKET_SIZE 9016
#define NFQUEUE_NUM_DEFAULT 0
#define FAKE_TTL_DEFAULT 5
#define FRAGMENT_SIZE_DEFAULT 5
#define CONNTRACK_CLEANUP_INTERVAL 30

#ifndef DEBUG
#define debug(...) do {} while (0)
#else
#define debug(...) fprintf(stderr, "[DEBUG] " __VA_ARGS__)
#endif

typedef struct {
    int fragment_size;
    int fragment_enabled;
    uint8_t set_ttl;
    char dns_addr[64];
    uint16_t dns_port;
    char dnsv6_addr[64];
    uint16_t dnsv6_port;
    int queue_num;
    int fake_enabled;
    int auto_mode;
} config_t;

extern config_t g_cfg;
extern int g_rawsock4;
extern int g_rawsock6;
extern volatile int g_running;

#endif
