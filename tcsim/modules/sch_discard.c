/*
 * sch_discard.c - Minimum queuing discipline; discards all packets
 *
 * Written 2001,2003,2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2003,2004 Werner Almesberger
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/netdevice.h> /* for NET_XMIT_DROP */
#include <linux/skbuff.h>
#include <linux/rtnetlink.h> /* for struct rtattr */
#include <linux/pkt_sched.h>
#include <net/pkt_sched.h>


#if KVERSIONNUM >= 0x20416  /* gratuitous interface change in 2.4.22 :-( */
#define SCH_DROP_UNSIGNED unsigned
#else
#define SCH_DROP_UNSIGNED
#endif


static int discard_enqueue(struct sk_buff *skb,struct Qdisc *sch)
{
    sch->stats.drops++;
    kfree_skb(skb);
    return NET_XMIT_DROP;
}


static int discard_requeue(struct sk_buff *skb,struct Qdisc *sch)
{
    kfree_skb(skb);
    return NET_XMIT_DROP;
}


static struct sk_buff *discard_dequeue(struct Qdisc *sch)
{
    return NULL; /* no packets to return */
}


static SCH_DROP_UNSIGNED int discard_drop(struct Qdisc *sch)
{
    return 0; /* dropped no packets */
}


static int discard_init(struct Qdisc *sch,struct rtattr *opt)
{
    MOD_INC_USE_COUNT;
    return 0;
}


static void discard_destroy(struct Qdisc *sch)
{
    MOD_DEC_USE_COUNT;
}


struct Qdisc_ops discard_qdisc_ops = {
        NULL,	/* next */
        NULL,	/* no cl_ops */
        "discard",
        0,	/* no private data */

        discard_enqueue,
        discard_dequeue,
        discard_requeue,
        discard_drop,

        discard_init,
        NULL,	/* no reset */
        discard_destroy,
        NULL,	/* no change */

#ifdef CONFIG_RTNETLINK
        NULL,	/* no dump */
#endif
};


#ifdef MODULE

int init_module(void)
{
	return register_qdisc(&discard_qdisc_ops);
}


void cleanup_module(void)
{
	unregister_qdisc(&discard_qdisc_ops);
}

#endif /* MODULE */
