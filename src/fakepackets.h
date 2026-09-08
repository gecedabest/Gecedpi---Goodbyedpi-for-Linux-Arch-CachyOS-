#ifndef FAKEPACKETS_H
#define FAKEPACKETS_H

#include "packet.h"

int send_fake_https(const parsed_pkt_t *pkt, uint8_t fake_ttl);
int send_fake_http(const parsed_pkt_t *pkt, uint8_t fake_ttl);

#endif
