/*
 * named.c - Databases of named values
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <u128.h>

#include "error.h"
#include "named.h"


const static struct ether_proto {
    const char *in_name;	/* protocol name for name->number lookup */
    uint16_t number;		/* protocol number, in host byte order */
    const char *out_name;	/* protocol name for number->name lookup;
				   NULL if translation is not supported */
} ether_proto_db[] = {
    { "ip",	ETH_P_IP,	"ip" },
    { "ipv4",	ETH_P_IP,	"ip" },
    { "ipv6",	ETH_P_IPV6,	"ipv6" },
    { NULL,	0 }
};


uint16_t ether_proto(const char *name)
{
    const struct ether_proto *ep;

    for (ep = ether_proto_db; ep->in_name; ep++)
	if (!strcmp(ep->in_name,name)) return ep->number;
    yyerrorf("unknown ether protocol \"%s\"",name);
}


const char *ether_proto_print(uint16_t proto)
{
    static char buf[7]; /* 0xffff\0 */
    const struct ether_proto *ep;

    for (ep = ether_proto_db; ep->in_name; ep++)
	if (ep->number == proto) {
	    if (ep->out_name) return ep->out_name;
	    break;
	}
    sprintf(buf,"0x%x",(unsigned) proto);
    return buf;
}


uint8_t ip_proto(const char *name)
{
    struct protoent *pe;

    pe = getprotobyname(name);
    if (!pe) yyerrorf("unknown IP protocol \"%s\"",name);
    return pe->p_proto;
}


/* @@@ gross hack ... should know L4 protocol too */
uint16_t ip_port(const char *name)
{
    struct servent *se;

    se = getservbyname(name,"tcp");
    if (!se) se = getservbyname(name,"udp");
    if (!se) yyerrorf("unknown service/port \"%s\"",name);
    return ntohs(se->s_port);
}
