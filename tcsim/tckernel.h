/*
 * tckernel.h - Kernel interface
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 */


#ifndef TCKERNEL_H
#define TCKERNEL_H

#include <sys/time.h>		/* skbuff.h needs struct timeval */

#define __TCSIM__		/* enable tcsim extensions in header files */

#include <linux/types.h>
#include <linux/skbuff.h>	/* struct sk_buff */
#include <linux/sched.h>	/* struct timer_list */
#include <linux/netdevice.h>	/* struct net_device */
#include <linux/netfilter.h>


#define IPQ(a) (a) >> 24,((a) >> 16) & 0xff,((a) >> 8) & 0xff,(a) & 0xff

extern unsigned long ujiffies;
extern int snap_len;

extern struct sk_buff *alloc_skb(unsigned int size,int priority);
extern void __kfree_skb(struct sk_buff *skb);

extern void dev_activate(struct net_device *dev);
extern void dev_deactivate(struct net_device *dev);

extern void add_hires_timer(struct timer_list *timer);
extern int rtnetlink_send(struct sk_buff *skb,__u32 pid,unsigned group,
  int echo);
int netlink_recv(void *buf,int len);

#endif /* TCKERNEL_H */
