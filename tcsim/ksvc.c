/*
 * ksvc.c - Kernel services replacement for tcsim
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h> /* struct timeval for skbuff.h */

#define __TCSIM__
#include <linux/slab.h> /* for prototypes */
#include <linux/netdevice.h> /* for prototypes */

#include <memutil.h>

#include "tckernel.h"
#include "tcsim.h"


extern int netlink_unicast(struct sock *ssk,struct sk_buff *skb,u32 pid,
  int nonblock);

struct net_device *dev_base = NULL;


void *kmalloc(size_t size, int flags)
{
    return alloc(size);
}


void kfree(const void *ptr)
{
    free((void *) ptr);
}


int printk(const char *fmt,...)
{
    va_list ap;
    int res;

    if (fmt[0] == '<' && isdigit(fmt[1]) && fmt[2] == '>') {
	if (fmt[1]-'0' > printk_threshold) return 0; /* return more ? */
	fmt += 3;
    }
    else {
	/* KERN_WARNING, linux/kernel/printk.c:DEFAULT_MESSAGE_LOGLEVEL */
	if (4 > printk_threshold) return 0;
    }
    fflush(stdout);
    fprintf(stderr,"| ");
    va_start(ap,fmt);
    res = vfprintf(stderr,fmt,ap);
    va_end(ap);
    fflush(stderr);
    return res;
}


void __kfree_skb(struct sk_buff *skb)
{
    if (--skb->users) return;
    free(skb->data);
    free(skb);
}


struct sk_buff *alloc_skb(unsigned int size,int priority)
{
    static __u32 generation = 0; /* skb generation number */
    struct sk_buff *skb;

    skb = alloc_t(struct sk_buff);
    memset(skb,0,sizeof(*skb));
    skb->data = skb->head = skb->tail = skb->end = alloc(size);
    if (!skb->data) {
	perror("out of memory");
	exit(1);
    }
    skb->users = 1;
    skb->truesize = size+sizeof(struct sk_buff);
    skb->end += size;
    SKB_GEN(skb) = generation++;
    return skb;
}


void skb_over_panic(struct sk_buff *skb,int len,void *here);
    /* from linux/include/linux/skbuff.h */

void skb_over_panic(struct sk_buff *skb,int len,void *here)
{
    fflush(stdout);
    fprintf(stderr,"PANIC: skb %p, length %d\n",skb,len);
    abort();
}


int ___pskb_trim(struct sk_buff *skb, unsigned int len, int realloc);
    /* from linux/include/linux/skbuff.h */

int ___pskb_trim(struct sk_buff *skb, unsigned int len, int realloc)
{
    abort(); /* we shouldn't need this for now @@@ */
}


/*
 * From net/core/dev.c
 */


struct net_device *dev_get_by_index(int ifindex)
{
    struct net_device *dev;

    for (dev = dev_base; dev; dev = dev->next)
	if (dev->ifindex == ifindex) return dev;
    return NULL;
}


void dev_deactivate(struct net_device *dev)
{
    /* do nothing */
}


/*
 * From net/core/rtnetlink.c
 */


int pseudo_netlink_from_kernel(void *buf,int len)
{
    return netlink_recv(buf,len);
}


int rtnetlink_send(struct sk_buff *skb,__u32 pid,unsigned group,int echo)
{
    if (group) return skb->len;
    if (debug) debugf("rtnetlink_send to pid %u",pid);
//    NETLINK_CB(skb).dst_groups = group;
    return netlink_unicast(NULL,skb,pid,0);
}


int ffz(int v)
{
    return ffs(~v)-1;
}


/*
 * From net/core/utils.c
 */


static unsigned long net_rand_seed = 152L;


unsigned long net_random(void)
{
    net_rand_seed = net_rand_seed*69069L+1;
    return net_rand_seed;
}


/*
 * ll_name_to_index and ll_index_to_name ought to be in usvc.c, not ksvc.c.
 * Unfortunately, including netdevice.h there would cause problems, that's
 * why they're here.
 */


int ll_name_to_index(const char *name); /* in ulib */

int ll_name_to_index(const char *name)
{
    struct net_device *dev;

    for (dev = dev_base; dev; dev = dev->next)
	if (!strcmp(dev->name,name)) return dev->ifindex;
    return 0;
}


const char *ll_index_to_name(int idx); /* in ulib */

const char *ll_index_to_name(int idx)
{
    struct net_device *dev;

    for (dev = dev_base; dev; dev = dev->next)
	if (dev->ifindex == idx) return dev->name;
    return NULL;
}


extern void setup_netlink(void);
extern int pktsched_init(void);


int kernel_init(void)
{
    setup_netlink();
    return pktsched_init();
}
