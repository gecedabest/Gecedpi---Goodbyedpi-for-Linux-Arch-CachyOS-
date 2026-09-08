#ifndef CONNTRACK_H
#define CONNTRACK_H

#include "packet.h"

void conntrack_init(void);
int conntrack_is_done(const parsed_pkt_t *pkt);
void conntrack_mark_done(const parsed_pkt_t *pkt);
void conntrack_cleanup_expired(void);
void conntrack_cleanup_all(void);

#endif
