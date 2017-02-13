/*
 * attr.c - Packet attributes
 *
 * Written 2002 by Werner Almesberger
 * Copyright 2002 Werner Almesberger
 */


#include <netinet/if_ether.h>

#include "attr.h"


struct attributes no_attributes = {
    .present = 0
};

struct attributes default_attributes = {
    .present = attr_nfmark | attr_priority | attr_protocol | attr_tc_index,
    .dflt = attr_nfmark | attr_priority | attr_protocol | attr_tc_index,
    .nfmark = 0,
    .priority = 0,
    .protocol = ETH_P_IP,
    .tc_index = 0,
};


#define MERGE(attr) \
  if ((old.present & attr_##attr) && \
    (!(new.present & attr_##attr) || \
    (!(old.dflt & attr_##attr) && (new.dflt & attr_##attr)))) { \
      new.attr = old.attr; \
      new.present |= attr_##attr; \
      new.dflt = (new.dflt & ~attr_##attr) | (old.dflt & attr_##attr); \
   }


struct attributes merge_attributes(struct attributes old,
  struct attributes new)
{
    MERGE(nfmark);
    MERGE(priority);
    MERGE(protocol);
    MERGE(tc_index);
    return new;
}
