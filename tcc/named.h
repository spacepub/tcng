/*
 * names.h - Databases of named values
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#ifndef NANED_H
#define NAMED_H

#include <inttypes.h>

#include <u128.h>


uint16_t ether_proto(const char *name);

/*
 * Returns the ethernet protocol (ETH_P_*) known by "name" in host byte order.
 * Throws a fatal error if no such protocol is known.
 */

const char *ether_proto_print(uint16_t proto);

/*
 * ether_proto_print returns a pointer to a static buffer containing either
 * the protocol name or the hexadecimal number (with 0x) of the protocol.
 * The protocol number is in host byte order.
 */

uint8_t ip_proto(const char *name);

/*
 * Returns the IP protocol (IPPROTO_*)
 * Throws a fatal error if no such protocol is known.
 */

uint16_t ip_port(const char *name);

/*
 * Returns the TCP or UDP port known by "name" in host byte order.
 * Throws a fatal error if no such port is known.
 */

#endif /* NAMED_H */
