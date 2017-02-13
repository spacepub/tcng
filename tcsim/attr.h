/*
 * attr.h - Packet attributes
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Werner Almesberger
 */


#ifndef ATTR_H
#define ATTR_H

enum {
    attr_nfmark = 1,
    attr_priority = 2,
    attr_protocol = 4,
    attr_tc_index = 8,
};

struct attributes {
    unsigned char present;	/* attributes present in this set */
    unsigned char dflt;		/* attributes with default priority */
    unsigned long nfmark;
    unsigned long priority;
    unsigned long protocol;
    unsigned long tc_index;
};

/*
 * A bit in "defaulted" are only valid if the corresponding bit in "present"
 * is also set. Likewise, attribute values are only valid if the corresponding
 * bit in "present" is set.
 */


extern struct attributes no_attributes;
extern struct attributes default_attributes;

struct attributes merge_attributes(struct attributes old,
  struct attributes new);

#endif /* ATTR_H */
