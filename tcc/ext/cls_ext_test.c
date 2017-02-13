/*
 * cls_ext_test.c - Classifier for testing the external interface
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 Network Robots
 * Copyright 2002 Werner Almesberger
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


extern int match(u8 *raw,int len,u16 protocol,uint32_t *class);

/*
 * stdio functions required for cleanup
 */

int remove(const char *pathname);
void perror(const char *s);


#define _C(a,b) a##b
#define C(a,b) _C(a,b)

#define _S(x) #x
#define S(x) _S(x)


/*
 * get_time is for match.c.
 * We implement it here in order to have clean access to linux/time.h
 */

double get_time(void);

double get_time(void)
{
    struct timeval tv;

    do_gettimeofday(&tv);
    return tv.tv_sec+tv.tv_usec/1000000.0;
}


static int C(NAME,_classify)(struct sk_buff *skb,struct tcf_proto *tp,
  struct tcf_result *res)
{
    res->class = 0;
    return match(skb->nh.raw,skb->len,skb->protocol,&res->classid);
}


static int C(NAME,_init)(struct tcf_proto *tp)
{
    if (remove("cls_" S(NAME) ".o")) perror("remove cls_" S(NAME) ".o");
    MOD_INC_USE_COUNT;
    return 0;
}


static void C(NAME,_destroy)(struct tcf_proto *tp)
{
    MOD_DEC_USE_COUNT;
}


static unsigned long C(NAME,_get)(struct tcf_proto *tp,u32 handle)
{
    return 0;
}


static int C(NAME,_change)(struct tcf_proto *tp,unsigned long base,u32 handle,
  struct rtattr **tca,unsigned long *arg)
{
    struct rtattr *opt = tca[TCA_OPTIONS-1];

    return opt ? -EINVAL : 0;
}


static int C(NAME,_delete)(struct tcf_proto *tp,unsigned long arg)
{
    return -EINVAL;
}


struct tcf_proto_ops C(NAME,_ops) = {
        NULL,
        S(NAME),
        C(NAME,_classify),
        C(NAME,_init),
        C(NAME,_destroy),

        C(NAME,_get),
        NULL,		/* no put */
        C(NAME,_change),
        C(NAME,_delete),
        NULL,		/* no walk */
        NULL		/* no dump */
};


#ifdef MODULE

int init_module(void)
{
	return register_tcf_proto_ops(&C(NAME,_ops));
}


void cleanup_module(void)
{
	unregister_tcf_proto_ops(&C(NAME,_ops));
}

#endif
