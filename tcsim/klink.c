/*
 * klink.c - Communication with the kernel
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tckernel.h"

#include <linux/socket.h>
#include <asm/system.h> /* grr, pkt_sched needs it */
#include <net/pkt_sched.h>

#include <memutil.h>

#include "tcsim.h"
#include "host.h"
#include "timer.h"
#include "attr.h"


extern int rtnetlink_rcv_skb(struct sk_buff *skb);


static struct sk_buff busy_hack;


const char *print_skb_id(unsigned long skb_id)
{
    static char buf[20];

    if (use_generation) sprintf(buf,"0x%08lx",skb_id);
    else sprintf(buf,"%p",(void *) skb_id);
    return buf;
}


unsigned long get_skb_id(struct sk_buff *skb)
{
    return use_generation ? (unsigned long) SKB_GEN(skb) : (unsigned long) skb;
}


const char *print_skb(struct sk_buff *skb)
{
    return print_skb_id(get_skb_id(skb));
}


struct net_device *lookup_net_device(const char *name)
{
    struct net_device *dev;

    for (dev = dev_base; dev; dev = dev->next)
	if (!strcmp(dev->name,name)) break;
    return dev;
}


void create_net_device(struct host *host,const char *name,int kbps)
{
    struct net_device **next,*dev;
    int index = 1;

    for (next = &dev_base; *next; next = &(*next)->next) {
	if (!strcmp((*next)->name,name))
	    errorf("duplicate interface name \"%s\"",name);
	index++;
    }
    dev = *next = alloc_t(struct net_device);
    memset(dev,0,sizeof(*dev));
    dev->name = stralloc(name);
    dev->mtu = 1500;
    dev->ifindex = index;
    dev->flags = IFF_UP;
    dev->tx_queue_len = 100; /* kernel default */
    dev->kbps = kbps;
    dev->txing = kbps <= 0 ? &busy_hack : NULL;
    dev->host = host;
    dev->peer = NULL;
    dev_activate(dev);
    next = &dev->next;
}


void find_stalled_devices(void)
{
    struct net_device *dev;

    for (dev = dev_base; dev; dev = dev->next)
	if (dev->kbps <= 0 && dev->qdisc->q.qlen)
	    errorf("device \"%s\" is stopped but has unsent packets",dev->name);
}


/*
 * Adapted from net/core/rtnetlink.c
 */


int pseudo_netlink_to_kernel(const void *buf,int size)
{
    struct sk_buff *skb;

    skb = alloc_skb(size,GFP_KERNEL);
    skb->tail += size;
    skb->len += size;
    memcpy(skb->head,buf,size);
    return rtnetlink_rcv_skb(skb);
}


struct rtnetlink_link /* @@@ */
{
    int (*doit)(struct sk_buff *, struct nlmsghdr*, void *attr);
    int (*dumpit)(struct sk_buff *, void *cb);
};


static struct rtnetlink_link link_rtnetlink_table[RTM_MAX-RTM_BASE+1];
struct rtnetlink_link *rtnetlink_links[1] = { /* PF_UNSPEC */
  link_rtnetlink_table };


static void dump_packet(void *buf,int size)
{
    int len = size < snap_len ? size : snap_len;
    int i;

    for (i = 0; i < len; i++)
	printf("%s%02x",i & 3 ? "" : " ",((unsigned char *) buf)[i]);
    printf("%s\n",size == len ? "" : " ...");
}


static int __kernel_enqueue(struct net_device *dev,struct sk_buff *skb)
{
    unsigned long skb_id;
    int res,skb_len;

    netif_schedule(skb->dev);
    if (verbose >= 0) {
	print_time(now);
	printf(" E : %s %d : %s:",print_skb(skb),skb->len,dev->name);
	dump_packet(skb->head,skb->len);
    }
/* @@@ should compute checksum on first enqueuing */
/* @@@ should decrement TTL when forwarding */
    skb_id = get_skb_id(skb);
    skb_len = skb->len;
    res = dev->qdisc->enqueue(skb,dev->qdisc);
    if (res) {
	print_time(now);
	printf(" * : %s %d : %s: enqueue returns %s\n",print_skb_id(skb_id),
	  skb_len,dev->name,enqueue_res(res));
    }
    return res;
}


int kernel_enqueue(struct net_device *dev,void *buf,int size,
  struct attributes attr)
{
    struct sk_buff *skb;

    if (!dev) {
	if (!dev_base) errorf("no device configured");
	if (dev_base->next)
	    errorf("must specify device when using multiple devices");
	dev = dev_base;
    }
    if (!dev->qdisc || !dev->qdisc->enqueue)
	errorf("device %s is not configured\n",dev->name);
    skb = alloc_skb(size,0);
    memcpy(skb->data,buf,size); /* skb_put */
    skb->tail += size;
    skb->len += size;
    skb->dev = dev;
    skb->nh.raw = skb->head;
    skb->nh.iph = (void *) skb->head;
    if (skb->nh.iph->version == 4 && skb->nh.iph->ihl >= 5 && size >= 20)
	skb->nh.iph->tot_len = htons(size);
    skb->protocol = htons(attr.protocol);
    skb->nfmark = attr.nfmark;
    skb->priority = attr.priority;
    skb->tc_index = attr.tc_index;
    return __kernel_enqueue(dev,skb);
}


static void incoming(struct net_device *dev,struct sk_buff *skb)
{
    struct route *rt;
    __u32 addr;

    if (!preserve) {
	skb->nfmark = default_attributes.nfmark;
	skb->priority = default_attributes.priority;
	skb->tc_index = default_attributes.tc_index;
    }
    if (dev->qdisc_ingress) {
	int res;

	if (verbose >= 0) {
	    print_time(now);
	    printf(" I : %s %d : %s:",print_skb(skb),skb->len,dev->name);
	    dump_packet(skb->head,skb->len);
	}
	res = dev->qdisc_ingress->enqueue(skb,dev->qdisc_ingress);
	if (res != NF_ACCEPT) {
	    print_time(now);
	    printf(" * : %s %d : %s: enqueue returns %s\n",print_skb(skb),
	      skb->len,dev->name,ingress_res(res));
	    goto drop;
	}
    }
    if (skb->len < 20)  {
	print_time(now);
	printf(" * : %s %d : %s: too short for IPv4\n",print_skb(skb),skb->len,
	  dev->name);
	goto drop;
    }
    addr = ((__u32 *) skb->nh.iph)[4];
    /* @@@ ridiculously inefficient, I know */
    for (rt = ((struct host *) dev->host)->routes; rt; rt = rt->next)
	if (!((htonl(rt->addr)^addr) & htonl(rt->mask))) break;
    if (!rt) {
	print_time(now);
	printf(" * : %s %d : %s: no route to %u.%u.%u.%u\n",print_skb(skb),
	  skb->len,dev->name,IPQ(ntohl(addr)));
	goto drop;
    }
    skb->dev = rt->dev;
    (void) __kernel_enqueue(rt->dev,skb);
    return;

drop:
    __kfree_skb(skb);
}


static void deliver(struct net_device *dev)
{
    if (dev->txing != &busy_hack) {
	if (!dev->peer) __kfree_skb(dev->txing);
	else incoming(dev->peer,dev->txing);
    }
    dev->txing = NULL;
}


struct dev_poll {
    struct timer_list timer;
    struct net_device *dev;
};


static void dev_do_poll(unsigned long data)
{
    struct dev_poll *dsc = (struct dev_poll *) data;
    struct net_device *dev = dsc->dev;

    free(dsc);
    deliver(dev);
    kernel_poll(dev,0);
}


static void kernel_poll_device(struct net_device *dev,int unbusy)
{
    struct sk_buff *skb;

    if (unbusy && dev->txing) deliver(dev);
    if (dev->txing || !dev->scheduled) return;
    if (!dev->qdisc || !dev->qdisc->dequeue) return;
    skb = dev->qdisc->dequeue(dev->qdisc);
    if (!skb) return;
    if (verbose >= 0) {
	print_time(now);
	printf(" D : %s %d : %s:",print_skb(skb),(int) skb->len,dev->name);
	dump_packet(skb->head,skb->len);
    }
    dev->txing = skb;
    if (dev->kbps > 0) {
	struct dev_poll *dsc;
	struct jiffval next;

	dsc = alloc_t(struct dev_poll);
	next = dtojiffval(jiffvaltod(now)+skb->len*8.0/1000.0/dev->kbps*HZ);
	dsc->timer.expires = next.jiffies;
	dsc->timer.expires_ujiffies = next.ujiffies;
	dsc->timer.data = (unsigned long) dsc;
	dsc->timer.function = dev_do_poll;
	dsc->dev = dev;
	add_hires_timer(&dsc->timer);
    }
}


void kernel_poll(struct net_device *dev,int unbusy)
{
    if (dev) kernel_poll_device(dev,unbusy);
    else for (dev = dev_base; dev; dev = dev->next)
	    kernel_poll_device(dev,unbusy);
}


void set_timer_resolution(void)
{
    extern double tick_in_usec;

    tick_in_usec = (HZ << PSCHED_JSCALE)/1000000.0;
}
