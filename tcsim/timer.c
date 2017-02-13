/*
 * timer.c - Timer handling
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <linux/sched.h> /* for HZ */

#include "tckernel.h"
#include "tcsim.h"
#include "jiffies.h"
#include "timer.h"


struct every {
   struct jiffval interval;
   struct timer_list timer;
   void *buffer;
   int len;
};

unsigned long jiffies = 0;	/* mirrors now.jiffies */
struct jiffval now = { 0, 0 };

static struct timer_list *timers = NULL;



static int timer_lt(const struct timer_list *a,const struct timer_list *b)
{
    if (a->expires < b->expires) return 1;
    if (a->expires > b->expires) return 0;
    return a->expires_ujiffies < b->expires_ujiffies;
}


static int timer_le(const struct timer_list *a,const struct timer_list *b)
{
    if (a->expires < b->expires) return 1;
    if (a->expires > b->expires) return 0;
    return a->expires_ujiffies <= b->expires_ujiffies;
}


void add_hires_timer(struct timer_list *timer)
{
    struct timer_list tmp = {
	.expires = now.jiffies,
	.expires_ujiffies = now.ujiffies,
    };
    struct timer_list **walk;

    if (debug)
	debugf("%ld.%06ld: adding timer %ld.%06ld (%p)",now.jiffies,
	  now.ujiffies,timer->expires,timer->expires_ujiffies,timer->function);
    if (timer_lt(timer,&tmp)) errorf("timer before current time");
    for (walk = &timers; *walk; walk = &(*walk)->next)
	if (!timer_le(*walk,timer)) break;
    timer->next = *walk;
    *walk = timer;
}


void add_timer(struct timer_list *timer)
{
    timer->expires_ujiffies = 0;
    add_hires_timer(timer);
}


int del_timer(struct timer_list *timer)
{
    struct timer_list **walk;

    for (walk = &timers; *walk && *walk != timer; walk = &(*walk)->next);
    if (!*walk) return 0;
    *walk = timer->next;
    return 1;
}


int mod_timer(struct timer_list *timer,unsigned long expires)
{
    int res;

    res = del_timer(timer);
    timer->expires = expires;
    timer->expires_ujiffies = 0;
    add_timer(timer);
    return res;
}


int advance_time(struct jiffval next)
{
    struct timer_list tmp = {
	.expires = next.jiffies,
	.expires_ujiffies = next.ujiffies,
    };

    if (jiffval_cmp(next,now) < 0) return -1;
    while (timers && timer_le(timers,&tmp)) {
	struct timer_list *this = timers;

	timers = timers->next;
	now.jiffies = jiffies = this->expires;
	now.ujiffies = this->expires_ujiffies;
	this->function(this->data);
	kernel_poll(NULL,0);
    }
    now = next;
    jiffies = now.jiffies;
    return 0;
}


void do_gettimeofday(struct timeval *tv)
{
    struct jiffval tmp = jtos(now);

    tv->tv_sec = tmp.jiffies;
    tv->tv_usec = tmp.ujiffies;
}
