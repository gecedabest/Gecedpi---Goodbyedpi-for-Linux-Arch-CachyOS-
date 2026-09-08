#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>

#include "main.h"
#include "conntrack.h"
#include "utils/uthash.h"

#define CONN_EXPIRE_SECS 60
#define CONN_KEY_LEN 36

typedef struct conn_entry {
    char           key[CONN_KEY_LEN];
    time_t         timestamp;
    UT_hash_handle hh;
} conn_entry_t;

static conn_entry_t *conn_table = NULL;
static time_t last_cleanup = 0;

void conntrack_init(void) {
    conn_table = NULL;
    last_cleanup = time(NULL);
}

static void build_key(const parsed_pkt_t *pkt, char *key) {
    memset(key, 0, CONN_KEY_LEN);

    if (!pkt->is_ipv6) {
        uint32_t src = pkt->ip4->saddr;
        uint32_t dst = pkt->ip4->daddr;
        memcpy(key + 12, &src, 4);
        memcpy(key + 28, &dst, 4);
    } else {
        memcpy(key,      &pkt->ip6->ip6_src, 16);
        memcpy(key + 16, &pkt->ip6->ip6_dst, 16);
    }

    uint16_t sport = pkt->tcp->source;
    uint16_t dport = pkt->tcp->dest;
    memcpy(key + 32, &sport, 2);
    memcpy(key + 34, &dport, 2);
}

int conntrack_is_done(const parsed_pkt_t *pkt) {
    if (!pkt->tcp) return 0;

    time_t now = time(NULL);
    if (now - last_cleanup > CONN_EXPIRE_SECS) {
        conntrack_cleanup_expired();
        last_cleanup = now;
    }

    char key[CONN_KEY_LEN];
    build_key(pkt, key);

    conn_entry_t *entry = NULL;
    HASH_FIND(hh, conn_table, key, CONN_KEY_LEN, entry);
    return (entry != NULL) ? 1 : 0;
}

void conntrack_mark_done(const parsed_pkt_t *pkt) {
    if (!pkt->tcp) return;

    char key[CONN_KEY_LEN];
    build_key(pkt, key);

    conn_entry_t *entry = NULL;
    HASH_FIND(hh, conn_table, key, CONN_KEY_LEN, entry);
    if (entry) {
        entry->timestamp = time(NULL);
        return;
    }

    entry = malloc(sizeof(conn_entry_t));
    if (!entry) return;
    memcpy(entry->key, key, CONN_KEY_LEN);
    entry->timestamp = time(NULL);
    HASH_ADD(hh, conn_table, key, CONN_KEY_LEN, entry);

    debug("conntrack: added entry, total=%u\n", HASH_COUNT(conn_table));
}

void conntrack_cleanup_expired(void) {
    conn_entry_t *entry, *tmp;
    time_t now = time(NULL);
    int removed = 0;

    HASH_ITER(hh, conn_table, entry, tmp) {
        if (now - entry->timestamp > CONN_EXPIRE_SECS) {
            HASH_DEL(conn_table, entry);
            free(entry);
            removed++;
        }
    }
    if (removed)
        debug("conntrack: cleaned %d expired entries\n", removed);
}

void conntrack_cleanup_all(void) {
    conn_entry_t *entry, *tmp;
    HASH_ITER(hh, conn_table, entry, tmp) {
        HASH_DEL(conn_table, entry);
        free(entry);
    }
}
