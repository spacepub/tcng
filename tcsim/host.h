/*
 * host.h - Host and route handling
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#ifndef HOST_H
#define HOST_H

#ifndef _LINUX_TYPES_H
#include <inttypes.h>
#endif

#include "tcsim.h"


struct route {
    uint32_t addr,mask;
    struct net_device *dev;
    struct route *next;
};


struct host {
    struct route *routes;
    struct host *next;
};


struct host *create_host(void);
void add_route(struct host *host,uint32_t addr,uint32_t mask,
  struct net_device *dev);
void connect_dev(struct net_device *a,struct net_device *b);

#endif /* HOST_H */
