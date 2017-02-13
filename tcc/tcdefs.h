/*
 * tcdefs.h - TC constants, mainly from the kernel
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */

/*
 * We define things here in order to avoid including kernel headers. This
 * a) improves stability, and b) makes it easier to cross-build tcng
 */

/* @@@ add more constants, e.g. for mask ranges */

#ifndef TCDEFS_H
#define TCDEFS_H

/* From linux/include/linux/pkt_sched.h */

#define TC_CBQ_MAXLEVEL		8
#define TC_CBQ_MAXPRIO		8

#define TCQ_GRED_MAX_DPs	16	/* MAX_DPs */

#define	TC_PRIO_MAX		16	/* +1 */
#define	TCQ_PRIO_BANDS		16

#define TC_PRIO_BESTEFFORT		0
#define TC_PRIO_FILLER			1
#define TC_PRIO_BULK			2
#define TC_PRIO_INTERACTIVE_BULK	4
#define TC_PRIO_INTERACTIVE		6
#define TC_PRIO_CONTROL			7

#define TC_HTB_MAXLEVEL		8
#define TC_HTB_MAXPRIO		8

/* From linux/include/linux/pkt_cls.h */

#define TC_U32_HTID(h)		((h)&0xFFF00000)
#define TC_U32_USERHTID(h)	(TC_U32_HTID(h)>>20)
#define TC_U32_HASH(h)		(((h)>>12)&0xFF)
#define TC_U32_NODE(h)		((h)&0xFFF)
#define TC_U32_KEY(h)		((h)&0xFFFFF)
#define TC_U32_UNSPEC		0
#define TC_U32_ROOT		(0xFFF00000)

/* TC constants that the kernel doesn't define */

#define TC_CBQ_MAX_EWMA		31
#define TCQ_PRIO_MIN_BANDS	2
#define TCQ_PRIO_DEFAULT_BANDS	3

#endif /* TCDEFS_H */
