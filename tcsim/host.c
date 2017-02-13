/*
 * host.c - Host and route handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>

#include <memutil.h>

#include "tcsim.h"
#include "tckernel.h"
#include "host.h"


static struct host *hosts = NULL;


struct host *create_host(void)
{
    struct host *host;

    host = alloc_t(struct host);
    host->routes = NULL;
    host->next = hosts;
    hosts = host;
    return host;
}


void add_route(struct host *host,uint32_t addr,uint32_t mask,
  struct net_device *dev)
{
    struct route **rt;

    if (dev->host != host)
	errorf("route to \"%s\" on different host",dev->name);
    addr &= mask; /* should this be an error ? */
    for (rt = &host->routes; *rt; rt = &(*rt)->next)
	if (!((*rt)->mask & ~mask) && !(((*rt)->addr^addr) & (*rt)->mask))
	    errorf("%u.%u.%u.%u netmask %u.%u.%u.%u clashes with %u.%u.%u.%u "
	      "netmask %u.%u.%u.%u",IPQ(addr),IPQ(mask),IPQ((*rt)->addr),
	      IPQ((*rt)->mask));
    *rt = alloc_t(struct route);
    (*rt)->addr = addr;
    (*rt)->mask = mask;
    (*rt)->dev = dev;
    (*rt)->next = NULL;
}


void connect_dev(struct net_device *a,struct net_device *b)
{
    if (a->peer) errorf("device %s is already connected",a->name);
    if (b->peer) errorf("device %s is already connected",b->name);
    a->peer = b;
    b->peer = a;
}
