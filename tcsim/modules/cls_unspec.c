/*
 * cls_unspec.c - Minimum classifier; always returns unspec
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/netdevice.h> /* net/pkt_cls.h needs things from here */
#include <linux/rtnetlink.h> /* for TCA_OPTIONS */
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/pkt_sched.h> /* linux/pkt_cls.h needs tc_ratespec */
#include <linux/pkt_cls.h>
#include <net/pkt_cls.h>


static int unspec_classify(struct sk_buff *skb,struct tcf_proto *tp,
  struct tcf_result *res)
{
    return TC_POLICE_UNSPEC;
}


static int unspec_init(struct tcf_proto *tp)
{
    MOD_INC_USE_COUNT;
    return 0;
}


static void unspec_destroy(struct tcf_proto *tp)
{
    MOD_DEC_USE_COUNT;
}


static unsigned long unspec_get(struct tcf_proto *tp,u32 handle)
{
    return 0;
}


static int unspec_change(struct tcf_proto *tp,unsigned long base,u32 handle,
  struct rtattr **tca,unsigned long *arg)
{
    struct rtattr *opt = tca[TCA_OPTIONS-1];

    return opt ? -EINVAL : 0;
}


static int unspec_delete(struct tcf_proto *tp,unsigned long arg)
{
    return -EINVAL;
}


struct tcf_proto_ops cls_unspec_ops = {
        NULL,
        "unspec",
        unspec_classify,
        unspec_init,
        unspec_destroy,

        unspec_get,
        NULL,		/* no put */
        unspec_change,
        unspec_delete,
        NULL,		/* no walk */
        NULL		/* no dump */
};


#ifdef MODULE

int init_module(void)
{
	return register_tcf_proto_ops(&cls_unspec_ops);
}


void cleanup_module(void)
{
	unregister_tcf_proto_ops(&cls_unspec_ops);
}

#endif
