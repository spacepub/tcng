/*
 * tcsim.h - Items shared by several components of tcsim
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#ifndef TCSIM_H
#define TCSIM_H

#include <string.h>
#include "jiffies.h"
#include "attr.h"


struct sk_buff;
struct net_device;
struct host; /* can't include host.h */

extern int snap_len;
extern int use_jiffies;
extern int use_generation;
extern int terminating;
extern int printk_threshold;
extern int debug;
extern int verbose;
extern int preserve;
extern int check_only;


#define SKB_GEN(skb) (*(__u32 *) ((skb)->cb+sizeof((skb)->cb)-4))


int pseudo_netlink_to_kernel(const void *buf,int size);
int pseudo_netlink_from_kernel(void *buf,int len);
int tc_main(int argc,char **argv);
int kernel_enqueue(struct net_device *dev,void *buf,int size,
  struct attributes attr);
void kernel_poll(struct net_device *dev,int unbusy);
void set_timer_resolution(void);

const char *print_skb_id(unsigned long skb_id);
unsigned long get_skb_id(struct sk_buff *skb);
const char *print_skb(struct sk_buff *skb);

void create_net_device(struct host *host,const char *name,int kbps);
void find_stalled_devices(void);
struct net_device *lookup_net_device(const char *name);
int kernel_init(void);
void reset_tc(void);
char *run_tcc(const char *dev,const char *in);
void preload_tc_module(const char *path);
void kernel_module(const char *path);

const char *enqueue_res(int ret);
const char *ingress_res(int ret);
void setup_tracing(void);

void yywarnf(const char *fmt,...);
void yywarn(const char *s);
void yyerrorf(const char *fmt,...);
void yyerror(const char *s);

void debugf(const char *fmt,...);
void errorf(const char *fmt,...);
void warnf(const char *fmt,...);

#endif /* TCSIM_H */
