/*
 * trace.c - Tracing functions
 *
 * Written 2001-2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 * Copyright 2003,2004 Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <memutil.h>

#include "tcsim.h"
#include "tckernel.h"
#include "timer.h"

#include <linux/config.h>
#include <asm/system.h> /* for local_bh_*able */
#include <net/pkt_sched.h>
#include <net/pkt_cls.h>


extern int do_tcf_police(struct sk_buff *skb,struct tcf_police *p);
extern int do_register_qdisc(struct Qdisc_ops *qops);
extern int do_register_tcf_proto_ops(struct tcf_proto_ops *ops);
extern struct Qdisc_ops pfifo_fast_ops;
extern struct Qdisc_ops ingress_qdisc_ops;


static int level = 0;
static int ingress_num; /* ingress entry */

static struct tcf_proto_ops orig_cls_ops[100];
static struct Qdisc_ops orig_sch_ops[100];


#if KVERSIONNUM >= 0x20416	/* gratuitous interface change in 2.4.22 :-( */
#define SCH_DROP_UNSIGNED unsigned
#else
#define SCH_DROP_UNSIGNED
#endif


/* ---------------------- Classification and policing ---------------------- */


static const char *police_res(int ret)
{
    static char buf[20];

    switch (ret) {
	case TC_POLICE_UNSPEC:
	    return "UNSPEC (-1)";
	case TC_POLICE_OK:
	    return "OK (0)";
	case TC_POLICE_RECLASSIFY:
	    return "RECLASSIFY (1)";
	case TC_POLICE_SHOT:
	    return "SHOT (2)";
	default:
	    sprintf(buf,"%d",ret);
	    return buf;
    }
}


/*
 * Most of this code comes from net/sched/police.c
 */


#define L2T(t,L)   ((t)->data[(L)>>(t)->rate.cell_log])


int tcf_police(struct sk_buff *skb,struct tcf_police *p)
{
    int ret;

    if (verbose > 2) {
	print_time(now);
	printf(" p : %s %d : <%d>",print_skb(skb),skb->len,level);
	if (p->ewma_rate)
	    printf(" ewma %lu >? %lu",(unsigned long) p->stats.bps,
	      (unsigned long) p->ewma_rate);
	if (p->R_tab) {
	    psched_time_t now;
	    long toks;

	    PSCHED_GET_TIME(now);
	    toks = PSCHED_TDIFF_SAFE(now,p->t_c,p->burst,0);
	    if (p->P_tab) {
		long ptoks = toks+p->ptoks;

		if (ptoks > L2T(p->P_tab,p->mtu)) ptoks = L2T(p->P_tab,p->mtu);
		printf(" peak %ld >? %ld",
		  (unsigned long) L2T(p->P_tab,skb->len),(unsigned long) ptoks);
	    }
	    toks += p->toks;
	    if (toks > p->burst) toks = p->burst;
	    printf(" rate %ld >? %ld",(unsigned long) L2T(p->R_tab,skb->len),
	      (unsigned long) toks);
	    
	}
	printf(" mtu %lu\n",(unsigned long) p->mtu);
    }
    ret = do_tcf_police(skb,p);
    if (verbose > 0) {
	print_time(now);
	printf(" p : %s %d : <%d> returns %s\n",print_skb(skb),skb->len,level,
	  police_res(ret));
    }
    return ret;
}


void trace_u32_match(struct sk_buff *skb,u32 handle,int pos,int off2);
  /* extern in klib */

void trace_u32_match(struct sk_buff *skb,u32 handle,int pos,int off2)
{
    if (verbose > 2) {
	print_time(now);
	printf(" c : %s %d : <%d> u32 handle %x:%x:%x match "
	  "skb->nh.raw+%d(+%d)\n",print_skb(skb),skb->len,level,
	  TC_U32_USERHTID(handle),TC_U32_HASH(handle),TC_U32_NODE(handle),pos,
	  off2);
    }
}


void trace_u32_offset(struct sk_buff *skb,u32 handle,int pos,u16 mask);
  /* extern in klib */

void trace_u32_offset(struct sk_buff *skb,u32 handle,int pos,u16 mask)
{
    if (verbose > 2) {
	print_time(now);
	printf(" c : %s %d : <%d> u32 handle %x:%x:%x offset "
	  "skb->nh.raw+%d & 0x%x\n",print_skb(skb),skb->len,level,
	  TC_U32_USERHTID(handle),TC_U32_HASH(handle),TC_U32_NODE(handle),pos,
	  (unsigned) mask);
    }
}


static int classify_wrapper(int n, struct sk_buff *skb, struct tcf_proto *tp,
  struct tcf_result *res)
{
    int ret;

    if (verbose > 1) {
	print_time(now);
	printf(" c : %s %d : <%d> calling %s at %x:%x, prio 0x%08lx\n",
	  print_skb(skb),skb->len,level,tp->ops->kind,
	  (int) (TC_H_MAJ(tp->classid) >> 16),
	  (int) TC_H_MIN(tp->classid),(unsigned long) tp->prio);
    }
    level++;
    ret = orig_cls_ops[n].classify(skb,tp,res);
    level--;
    if (verbose > 0) {
	print_time(now);
	printf(" c : %s %d : <%d> %s at %x:%x returns %s",print_skb(skb),
	  skb->len,level,tp->ops->kind,(int) (TC_H_MAJ(tp->classid) >> 16),
	  (int) TC_H_MIN(tp->classid),police_res(ret));
	if (ret != TC_POLICE_OK && ret != TC_POLICE_RECLASSIFY)
	    putchar('\n');
	else printf(" (%x:%x, 0x%lu)\n",(int) (TC_H_MAJ(res->classid) >> 16),
	      (int) TC_H_MIN(res->classid),(unsigned long) res->class);
    }
    return ret;
}


/* ------------------ Enqueuing, dequeuing, and dropping ------------------- */


const char *enqueue_res(int ret)
{
    static char buf[20];

    switch (ret) {
	case NET_XMIT_SUCCESS:
	    return "SUCCESS (0)";
	case NET_XMIT_DROP:
	    return "DROP (1)";
	case NET_XMIT_CN:
	    return "CN (2)";
	case NET_XMIT_POLICED:
	    return "POLICED (3)";
	case NET_XMIT_BYPASS:
	    return "BYPASS (4)";
	default:
	    sprintf(buf,"%d",ret);
	    return buf;
    }
}


const char *ingress_res(int ret)
{
    static char buf[20];

    switch (ret) {
	case NF_DROP:
	    return "NF_DROP (0)";
	case NF_ACCEPT:
	    return "NF_ACCEPT (1)";
	default:
	    sprintf(buf,"%d",ret);
	    return buf;
    }
}


static struct reshape_entry {
    struct Qdisc *q;
    int (*fn)(struct sk_buff *skb,struct Qdisc *q);
    struct reshape_entry *next;
} *reshape_entries = NULL;


static void reshape_add(struct Qdisc *q,
  int (*fn)(struct sk_buff *skb,struct Qdisc *q))
{
    struct reshape_entry **walk;

    for (walk = &reshape_entries; *walk; walk = &(*walk)->next)
	if ((*walk)->q == q) break;
    if (!*walk) {
	*walk = alloc_t(struct reshape_entry);
	(*walk)->q = q;
	(*walk)->next = NULL;
    }
    (*walk)->fn = fn;
}
  

static int (*reshape_lookup(struct Qdisc *q))(struct sk_buff *,struct Qdisc *)
{
    struct reshape_entry *walk;

    for (walk = reshape_entries; walk->q != q; walk = walk->next);
    return walk->fn;
}


static int reshape_wrapper(struct sk_buff *skb,struct Qdisc *q)
{
    int ret;

    if (verbose > 1) {
	print_time(now);
	printf(" s : %s %d : <%d> calling\n",print_skb(skb),skb->len,level);
    }
    level++;;
    ret = reshape_lookup(q)(skb,q);
    level--;
    if (verbose > 0) {
	print_time(now);
	printf(" s : %s %d : <%d> returns %d (%s)\n",print_skb(skb),skb->len,
	  level,ret,ret ? "discard" : "enqueued");
    }
    return ret;
}


static int enqueue_wrapper(int n,struct sk_buff *skb,struct Qdisc *q)
{
    int ret;
    int len = skb->len; /* enqueue may kfree the skb */
    unsigned long skb_id;
    int (*orig_reshape_fail)(struct sk_buff *skb,struct Qdisc *q) = NULL;

    skb_id = get_skb_id(skb);
    if (verbose > 1) {
	print_time(now);
	printf(" e : %s %d : <%d> calling %s (%x:%x)\n",print_skb_id(skb_id),
	  skb->len,level,q->ops->id,(int) (TC_H_MAJ(q->handle) >> 16),
	  (int) TC_H_MIN(q->handle));
    }
    if (q->reshape_fail) {
	orig_reshape_fail = q->reshape_fail;
	reshape_add(q,q->reshape_fail);
	q->reshape_fail = reshape_wrapper;
    }
    level++;;
    ret = orig_sch_ops[n].enqueue(skb,q);
    level--;
    if (q->reshape_fail) q->reshape_fail = orig_reshape_fail;
    if (verbose > 0) {
	print_time(now);
	printf(" e : %s %d : <%d> %s (%x:%x) returns %s\n",print_skb_id(skb_id),
	  len,level,q->ops->id,(int) (TC_H_MAJ(q->handle) >> 16),
	  (int) TC_H_MIN(q->handle),enqueue_res(ret));
    }
    return ret;
}


static int ingress_wrapper(struct sk_buff *skb,struct Qdisc *q)
{
    int ret;
    int len = skb->len; /* enqueue may kfree the skb */
    unsigned long skb_id;

    skb_id = get_skb_id(skb);
    if (verbose > 1) {
	print_time(now);
	printf(" i : %s %d : <%d> calling %s (%x:%x)\n",print_skb_id(skb_id),
	  skb->len,level,q->ops->id,(int) (TC_H_MAJ(q->handle) >> 16),
	  (int) TC_H_MIN(q->handle));
    }
    level++;;
    ret = orig_sch_ops[ingress_num].enqueue(skb,q);
    level--;
    if (verbose > 0) {
	print_time(now);
	printf(" i : %s %d : <%d> %s (%x:%x) returns %s\n",print_skb_id(skb_id),
	  len,level,q->ops->id,(int) (TC_H_MAJ(q->handle) >> 16),
	  (int) TC_H_MIN(q->handle),ingress_res(ret));
    }
    return ret;
}


static struct sk_buff *dequeue_wrapper(int n,struct Qdisc *q)
{
    struct sk_buff *skb;

    if (verbose > 1) {
	print_time(now);
	printf(" d : 0x0 0 : <%d> calling %s (%x:%x)\n",level,q->ops->id,
	  (int) (TC_H_MAJ(q->handle) >> 16),(int) TC_H_MIN(q->handle));
    }
    level++;
    skb = orig_sch_ops[n].dequeue(q);
    level--;
    if (verbose > 1 && !skb) {
	print_time(now);
	printf(" d : 0x0 0 : <%d> %s (%x:%x)\n",level,q->ops->id,
	  (int) (TC_H_MAJ(q->handle) >> 16),(int) TC_H_MIN(q->handle));
    }
    if (skb && verbose > 0) {
	print_time(now);
	printf(" d : %s %d : <%d> %s (%x:%x)\n",print_skb(skb),skb->len,level,
	  q->ops->id,(int) (TC_H_MAJ(q->handle) >> 16),
	  (int) TC_H_MIN(q->handle));
    }
    return skb;
}


static int requeue_wrapper(int n,struct sk_buff *skb,struct Qdisc *q)
{
    int ret;
    int len = skb->len; /* requeue may kfree the skb */
    unsigned long skb_id;

    skb_id = get_skb_id(skb);
    if (verbose > 1) {
	print_time(now);
	printf(" r : %s %d : <%d> calling %s (%x:%x)\n",print_skb_id(skb_id),
	  skb->len,level,q->ops->id,(int) (TC_H_MAJ(q->handle) >> 16),
	  (int) TC_H_MIN(q->handle));
    }
    level++;;
    ret = orig_sch_ops[n].requeue(skb,q);
    level--;
    if (verbose > 0) {
	print_time(now);
	printf(" r : %s %d : <%d> %s (%x:%x) returns %s\n",
	  print_skb_id(skb_id),len,level,q->ops->id,
	  (int) (TC_H_MAJ(q->handle) >> 16),(int) TC_H_MIN(q->handle),
	  enqueue_res(ret));
    }
    return ret;
}


static SCH_DROP_UNSIGNED int drop_wrapper(int n,struct Qdisc *q)
{
    int ret;

    if (verbose > 1) {
	print_time(now);
	printf(" x : 0x0 0 : <%d> calling %s (%x:%x)\n",level,q->ops->id,
	  (int) (TC_H_MAJ(q->handle) >> 16),(int) TC_H_MIN(q->handle));
    }
    level++;
    ret = orig_sch_ops[n].drop(q);
    level--;
    if (verbose > 0) {
	print_time(now);
	printf(" x : 0x0 0 : <%d> %s (%x:%x) returns %d (%sdropped)\n",level,
	  q->ops->id,(int) (TC_H_MAJ(q->handle) >> 16),
	  (int) TC_H_MIN(q->handle),ret,ret ? "" : "not ");
    }
    return ret;
}


/* -------------------------- Wrapper repository --------------------------- */


/*
 * Such a large number of wrappers may seem excessive at the moment, but once
 * we get custom-built elements, each interface may have a few "private"
 * elements, so large simulations may indeed need lots of wrappers.
 */

#define NUM_CLS_WRAPPERS 100
#define NUM_SCH_WRAPPERS 100


#define CLS_WRAPPERS(nn) \
  static int classify_wrapper_##nn(struct sk_buff *skb,struct tcf_proto *tp, \
    struct tcf_result *res) \
  { \
    return classify_wrapper(1##nn-100,skb,tp,res); \
  }

#define CLS_WRAPPERS_10(n) \
  CLS_WRAPPERS(n##0) CLS_WRAPPERS(n##1) CLS_WRAPPERS(n##2) CLS_WRAPPERS(n##3) \
  CLS_WRAPPERS(n##4) CLS_WRAPPERS(n##5) CLS_WRAPPERS(n##6) CLS_WRAPPERS(n##7) \
  CLS_WRAPPERS(n##8) CLS_WRAPPERS(n##9)

#define CLS_WRAPPERS_100 \
  CLS_WRAPPERS_10(0) CLS_WRAPPERS_10(1) CLS_WRAPPERS_10(2) CLS_WRAPPERS_10(3) \
  CLS_WRAPPERS_10(4) CLS_WRAPPERS_10(5) CLS_WRAPPERS_10(6) CLS_WRAPPERS_10(7) \
  CLS_WRAPPERS_10(8) CLS_WRAPPERS_10(9)

CLS_WRAPPERS_100

#define SCH_WRAPPERS(nn) \
  static int enqueue_wrapper_##nn(struct sk_buff *skb,struct Qdisc *q) \
  { \
    return enqueue_wrapper(1##nn-100,skb,q); \
  } \
  static struct sk_buff *dequeue_wrapper_##nn(struct Qdisc *q) \
  { \
    return dequeue_wrapper(1##nn-100,q); \
  } \
  static int requeue_wrapper_##nn(struct sk_buff *skb,struct Qdisc *q) \
  { \
    return requeue_wrapper(1##nn-100,skb,q); \
  } \
  static SCH_DROP_UNSIGNED int drop_wrapper_##nn(struct Qdisc *q) \
  { \
    return drop_wrapper(1##nn-100,q); \
  }

#define SCH_WRAPPERS_10(n) \
  SCH_WRAPPERS(n##0) SCH_WRAPPERS(n##1) SCH_WRAPPERS(n##2) SCH_WRAPPERS(n##3) \
  SCH_WRAPPERS(n##4) SCH_WRAPPERS(n##5) SCH_WRAPPERS(n##6) SCH_WRAPPERS(n##7) \
  SCH_WRAPPERS(n##8) SCH_WRAPPERS(n##9)

#define SCH_WRAPPERS_100 \
  SCH_WRAPPERS_10(0) SCH_WRAPPERS_10(1) SCH_WRAPPERS_10(2) SCH_WRAPPERS_10(3) \
  SCH_WRAPPERS_10(4) SCH_WRAPPERS_10(5) SCH_WRAPPERS_10(6) SCH_WRAPPERS_10(7) \
  SCH_WRAPPERS_10(8) SCH_WRAPPERS_10(9)

SCH_WRAPPERS_100

#define INIT_CLS_OPS(nn) \
  { .classify = classify_wrapper_##nn },

#define INIT_CLS_OPS_10(n) \
  INIT_CLS_OPS(n##0) INIT_CLS_OPS(n##1) INIT_CLS_OPS(n##2) INIT_CLS_OPS(n##3) \
  INIT_CLS_OPS(n##4) INIT_CLS_OPS(n##5) INIT_CLS_OPS(n##6) INIT_CLS_OPS(n##7) \
  INIT_CLS_OPS(n##8) INIT_CLS_OPS(n##9)

#define INIT_CLS_OPS_100 \
  INIT_CLS_OPS_10(0) INIT_CLS_OPS_10(1) INIT_CLS_OPS_10(2) INIT_CLS_OPS_10(3) \
  INIT_CLS_OPS_10(4) INIT_CLS_OPS_10(5) INIT_CLS_OPS_10(6) INIT_CLS_OPS_10(7) \
  INIT_CLS_OPS_10(8) INIT_CLS_OPS_10(9)

#define INIT_SCH_OPS(nn) \
  { .enqueue = enqueue_wrapper_##nn, .dequeue = dequeue_wrapper_##nn, \
    .requeue = requeue_wrapper_##nn, .drop = drop_wrapper_##nn },

#define INIT_SCH_OPS_10(n) \
  INIT_SCH_OPS(n##0) INIT_SCH_OPS(n##1) INIT_SCH_OPS(n##2) INIT_SCH_OPS(n##3) \
  INIT_SCH_OPS(n##4) INIT_SCH_OPS(n##5) INIT_SCH_OPS(n##6) INIT_SCH_OPS(n##7) \
  INIT_SCH_OPS(n##8) INIT_SCH_OPS(n##9)

#define INIT_SCH_OPS_100 \
  INIT_SCH_OPS_10(0) INIT_SCH_OPS_10(1) INIT_SCH_OPS_10(2) INIT_SCH_OPS_10(3) \
  INIT_SCH_OPS_10(4) INIT_SCH_OPS_10(5) INIT_SCH_OPS_10(6) INIT_SCH_OPS_10(7) \
  INIT_SCH_OPS_10(8) INIT_SCH_OPS_10(9)
  
static const struct tcf_proto_ops my_cls_ops[NUM_CLS_WRAPPERS] =
  { INIT_CLS_OPS_100 };
static const struct Qdisc_ops my_sch_ops[NUM_SCH_WRAPPERS] =
  { INIT_SCH_OPS_100 };


static int wrap_qdisc(struct Qdisc_ops *ops)
{
    int n;

    for (n = 0; n < NUM_SCH_WRAPPERS; n++)
	if (!*orig_sch_ops[n].id) break;
    if (n == NUM_SCH_WRAPPERS) errorf("out of qdisc wrappers");
    orig_sch_ops[n] = *ops;
    ops->enqueue = my_sch_ops[n].enqueue;
    ops->dequeue = my_sch_ops[n].dequeue;
    ops->requeue = my_sch_ops[n].requeue;
    ops->drop = my_sch_ops[n].drop;
#ifdef CONFIG_NET_SCH_INGRESS
    if (ops == &ingress_qdisc_ops) {
	ingress_num = n;
	ops->enqueue = ingress_wrapper;
    }
#endif
    return n;
}


static int wrap_filter(struct tcf_proto_ops *ops)
{
    int n;

    for (n = 0; n < NUM_CLS_WRAPPERS; n++)
	if (!*orig_cls_ops[n].kind) break;
    if (n == NUM_CLS_WRAPPERS) errorf("out of filter wrappers");
    orig_cls_ops[n] = *ops;
    ops->classify = my_cls_ops[n].classify;
    return n;
}


int register_qdisc(struct Qdisc_ops *qops)
{
    int error;

    error = do_register_qdisc(qops);
    if (error) return error;
    (void) wrap_qdisc(qops);
    return 0;
}

int register_tcf_proto_ops(struct tcf_proto_ops *ops)
{
    int error;

    error = do_register_tcf_proto_ops(ops);
    if (error) return error;
    (void) wrap_filter(ops);
    return 0;
}


void setup_tracing(void)
{
    (void) wrap_qdisc(&noop_qdisc_ops);
    (void) wrap_qdisc(&pfifo_fast_ops);
}
