#ifndef FRAGMENT_H
#define FRAGMENT_H

#include "packet.h"

int send_fragment_pair(const parsed_pkt_t *pkt, int frag_size);

#endif
