/*
 * tcm_cls.c - Common code for tcc-generated classifiers, kernel part
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 Werner Almesberger, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h> /* for debugging */
#include <linux/netdevice.h> /* net/pkt_cls.h needs things from here */
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/pkt_sched.h> /* linux/pkt_cls.h needs tc_ratespec */
#include <linux/pkt_cls.h>
#include <net/pkt_cls.h>
#include <net/pkt_sched.h>


#define _C(a,b) a##b
#define C(a,b) _C(a,b)

#define _C3(a,b,c) a##b##c
#define C3(a,b,c) _C3(a,b,c)

#define _S(x) #x
#define S(x) _S(x)


#define _ACCESS8(off)		(*(u8 *) (skb->nh.raw+(off)))
#define _ACCESS16(off)		ntohs(*(u16 *) (skb->nh.raw+(off)))
#define _ACCESS32(off)		ntohl(*(u32 *) (skb->nh.raw+(off)))
#define _ACCESS_META(conv,acc)	conv(acc)

#define _DEBUG_ACCESS(b,off) \
  ({ printk(KERN_DEBUG "u%d @ %d -> 0x%lx\n",b,(off), \
     (unsigned long) _ACCESS##b(off)); _ACCESS##b(off); })
#define _DEBUG_ACCESS_META(conv,acc) \
  ({ printk(KERN_DEBUG #acc " -> 0x%lx\n", \
     (unsigned long) _ACCESS_META(conv,acc)); _ACCESS_META(conv,acc); })

#ifndef DEBUG

#define ACCESS8(off)		_ACCESS8(off)
#define ACCESS16(off)		_ACCESS16(off)
#define ACCESS32(off)		_ACCESS32(off)
#define ACCESS_META_PROTOCOL	_ACCESS_META(ntohs,skb->protocol)

#define DEBUGF(...)

#else /* DEBUG */

#define ACCESS8(off)		_DEBUG_ACCESS(8,off)
#define ACCESS16(off)		_DEBUG_ACCESS(16,off)
#define ACCESS32(off)		_DEBUG_ACCESS(32,off)
#define ACCESS_META_PROTOCOL	_DEBUG_ACCESS_META(ntohs,skb->protocol)

#define DEBUGF(...)		printk(KERN_DEBUG __VA_ARGS__)

#endif /* DEBUG */

#include MDI_FILE


#define TIME_STEPS 256

#define _TOK_F(t,scale,rate) (((t) << scale)*(double) (rate)/1000000.0)

#define _T2B_1(t,rate,scale,depth) \
  (_TOK_F(t,scale,rate) >= (depth) ? (depth) : _TOK_F(t,scale,rate)),

#define _T2B_16m(hi,rate,scale,depth) \
  _T2B_1(hi+0,rate,scale,depth)  _T2B_1(hi+1,rate,scale,depth) \
  _T2B_1(hi+2,rate,scale,depth)  _T2B_1(hi+3,rate,scale,depth) \
  _T2B_1(hi+4,rate,scale,depth)  _T2B_1(hi+5,rate,scale,depth) \
  _T2B_1(hi+6,rate,scale,depth)  _T2B_1(hi+7,rate,scale,depth) \
  _T2B_1(hi+8,rate,scale,depth)  _T2B_1(hi+9,rate,scale,depth) \
  _T2B_1(hi+10,rate,scale,depth) _T2B_1(hi+11,rate,scale,depth) \
  _T2B_1(hi+12,rate,scale,depth) _T2B_1(hi+13,rate,scale,depth) \
  _T2B_1(hi+14,rate,scale,depth) _T2B_1(hi+15,rate,scale,depth)

#define _T2B_16(hi,rate,scale,depth) _T2B_16m((hi << 4),rate,scale,depth)

#define T2B(rate,scale,depth) \
  _T2B_16(0,rate,scale,depth)  _T2B_16(1,rate,scale,depth) \
  _T2B_16(2,rate,scale,depth)  _T2B_16(3,rate,scale,depth) \
  _T2B_16(4,rate,scale,depth)  _T2B_16(5,rate,scale,depth) \
  _T2B_16(6,rate,scale,depth)  _T2B_16(7,rate,scale,depth) \
  _T2B_16(8,rate,scale,depth)  _T2B_16(9,rate,scale,depth) \
  _T2B_16(10,rate,scale,depth) _T2B_16(11,rate,scale,depth) \
  _T2B_16(12,rate,scale,depth) _T2B_16(13,rate,scale,depth) \
  _T2B_16(14,rate,scale,depth) _T2B_16(15,rate,scale,depth)

#define INIT_BUCKET(_rate,_mpu,_depth,_initial,_overflow,_max_time, \
  _total_depth,_scale) \
  { \
    .tokens = (_initial), \
    .mpu = (_mpu), \
    .depth = (_depth), \
    .last_refill = { 0, 0 }, \
    .carry = 0, \
    .max_time = (_max_time), \
    .scale = (_scale), \
    .t2b = { T2B(_rate,_scale,_total_depth) }, \
    .overflow = (_overflow) == -1 ? NULL : buckets+(_overflow), \
  }
  
static struct bucket {
    uint32_t tokens;		/* current number of tokens in bucket */
    uint32_t mpu;		/* minimum policed unit */
    uint32_t depth;		/* maximum token depth */
    struct timeval last_refill;	/* time of last bucket refill */
    int carry;			/* microseconds ignored at last refill */
    uint32_t max_time;		/* maximum time interval until bucket (plus
				   overflow buckets) fills up */
    int scale;			/* divide time by 2^scale */
    uint32_t t2b[TIME_STEPS];	/* time to bytes (tokens) translation */
    struct bucket *overflow;	/* bucket to overflow to; NULL if none */
} buckets[] = {
    BUCKETS
};


static struct tcf_result results[ELEMENTS];


static void refill_bucket(struct bucket *b)
{
    struct bucket *add;
    uint32_t delta_s,delta_us,tokens;
    struct timeval now;

    /* compute time */
    do_gettimeofday(&now);
    delta_s = now.tv_sec-b->last_refill.tv_sec;
    delta_us = now.tv_usec-b->last_refill.tv_usec+b->carry;
    if (delta_us & 0x80000000) {
	delta_us += 1000000;
	delta_s--;
    }
    if (delta_s < 4) delta_us += 1000000*delta_s;
    else if (delta_s > 4 || b->max_time < 4000000) delta_us = b->max_time;
	else if (delta_us > b->max_time-4000000) delta_us = b->max_time;
	    else delta_us += 1000000*delta_s;
    if (delta_us > b->max_time) delta_us = b->max_time;
    b->carry = delta_us & ((1 << b->scale)-1);
    b->last_refill = now;

    /* update buckets */
    tokens = b->t2b[delta_us >> b->scale];
    DEBUGF("delta_us %lu, tokens %lu, bucket %lu/%lu\n",
      (unsigned long) delta_us,(unsigned long) tokens,
      (unsigned long) b->tokens,(unsigned long) b->depth);
    for (add = b; add; add = add->overflow) {
	uint32_t free = add->depth-add->tokens;

	if (tokens <= free) {
	    add->tokens += tokens;
	    break;
	}
	tokens -= free;
	add->tokens = add->depth;
    }

}


static int __attribute__((unused)) police_conform(const struct sk_buff *skb,
  int bucket)
{
    struct bucket *b = buckets+bucket;
    uint32_t policed_len = skb->len < b->mpu ? b->mpu : skb->len;

    refill_bucket(b);
    /* check conformance */
    return b->tokens >= policed_len;
}


static void __attribute__((unused)) police_count(const struct sk_buff *skb,
  int bucket)
{
    struct bucket *b = buckets+bucket;
    uint32_t policed_len = skb->len < b->mpu ? b->mpu : skb->len;

    refill_bucket(b);
    if (b->tokens < policed_len) b->tokens = 0;
    else b->tokens -= policed_len;
}


static int C(NAME,_classify)(struct sk_buff *skb,struct tcf_proto *tp,
  struct tcf_result *res)
{
	#define RESULT(r,n) *res = results[n]; return (r);
	(void) EXPRESSION;
	return TC_POLICE_UNSPEC;
}


static void cleanup(struct Qdisc *q)
{
	int i;

	for (i = 0; results[i].class && i < ELEMENTS; i++)
		q->ops->cl_ops->unbind_tcf(q,results[i].class);
}


static int C(NAME,_init)(struct tcf_proto *tp)
{
	#define __INIT(n,c) \
	  results[n].classid = c; \
          results[n].class = tp->q->ops->cl_ops->bind_tcf(tp->q, \
	    0 /* qdisc */,c); \
	  if (!results[n].class) goto error;

	ELEMENT(__INIT);
	MOD_INC_USE_COUNT;
	return 0;

error:
	cleanup(tp->q);
	return -EINVAL;
}


static void C(NAME,_destroy)(struct tcf_proto *tp)
{
	cleanup(tp->q);
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


struct tcf_proto_ops C3(cls_,NAME,_ops) = {
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
	return register_tcf_proto_ops(&C3(cls_,NAME,_ops));
}


void cleanup_module(void)
{
	unregister_tcf_proto_ops(&C3(cls_,NAME,_ops));
}

#endif
