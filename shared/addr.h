/*
 * addr.h - Address conversion
 *
 * Written 2002 by Werner Almesberger
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#ifndef ADDR_H
#define ADDR_H


#include <stdint.h>

#include "u128.h"


extern void addr_error_hook(const char *msg);

uint32_t ipv4_host(const char *name,int allow_dns);

/*
 * Returns the IPv4 host known by "name" in host byte order. "name" may also
 * be a dotted quad. Throws a fatal error if no such host is known, and
 * "name" is not a valid dotted quad.
 */

U128 ipv6_host(const char *name,int allow_dns);

/*
 * Returns the IPv6 host known by "name" in host byte order. "name" may also
 * be in numeric form. Throws a fatal error if no such host is known, and
 * "name" is not a valid numeric IPv6 address.
 */

#endif /* ADDR_H */
