/*
 * addr.c - Address conversion
 *
 * Written 2002 by Werner Almesberger
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#include <stdint.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "memutil.h"

#include "u128.h"
#include "addr.h"


extern void addr_error_hook(const char *msg);


/* ----- Cache management -------------------------------------------------- */


static struct cache_entry {
    const char *name;
    U128 value;
    struct cache_entry *next;
} *ipv4_cache = NULL,*ipv6_cache = NULL;


static const struct cache_entry *cache_lookup(const struct cache_entry *root,
  const char *name)
{
    while (root && strcmp(root->name,name)) root = root->next;
    return root;
}


static void cache_add(struct cache_entry **root,const char *name,U128 value)
{
    struct cache_entry *new = alloc_t(struct cache_entry);

    new->name = stralloc(name);
    new->value = value;
    new->next = *root;
    *root = new;
}


/* ----- Lookup functions -------------------------------------------------- */


uint32_t ipv4_host(const char *name,int allow_dns)
{
    struct addrinfo hints,*res;
    uint32_t r;
    int error;

    if (allow_dns) {
	const struct cache_entry *entry;

	entry = cache_lookup(ipv4_cache,name);
	if (entry) return u128_to_32(entry->value);
    }
    memset(&hints,0,sizeof(hints));
    hints.ai_flags = allow_dns ? 0 : AI_NUMERICHOST;
    hints.ai_family = PF_INET;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
    error = getaddrinfo(name,NULL,&hints,&res);
    if (error) addr_error_hook(gai_strerror(error));
    r = ntohl(((struct sockaddr_in *) res->ai_addr)->sin_addr.s_addr);
    freeaddrinfo(res);
    if (allow_dns) cache_add(&ipv4_cache,name,u128_from_32(r));
    return r;
}


U128 ipv6_host(const char *name,int allow_dns)
{
    struct addrinfo hints,*res;
    unsigned char *a;
    U128 r;
    int error;

    if (allow_dns) {
	const struct cache_entry *entry;

	entry = cache_lookup(ipv6_cache,name);
	if (entry) return entry->value;
    }
    memset(&hints,0,sizeof(hints));
    hints.ai_flags = allow_dns ? 0 : AI_NUMERICHOST;
    hints.ai_family = PF_INET6;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
    error = getaddrinfo(name,NULL,&hints,&res);
    if (error) addr_error_hook(gai_strerror(error));
    a = ((struct sockaddr_in6 *) res->ai_addr)->sin6_addr.s6_addr;
    r.v[3] = (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
    r.v[2] = (a[4] << 24) | (a[5] << 16) | (a[6] << 8) | a[7];
    r.v[1] = (a[8] << 24) | (a[9] << 16) | (a[10] << 8) | a[11];
    r.v[0] = (a[12] << 24) | (a[13] << 16) | (a[14] << 8) | a[15];
    freeaddrinfo(res);
    if (allow_dns) cache_add(&ipv6_cache,name,r);
    return r;
}
