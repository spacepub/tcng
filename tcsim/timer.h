/*
 * timer.h - Timer handling
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 */


#ifndef TIMER_H
#define TIMER_H

#include "tckernel.h"
#include "jiffies.h"


void add_hires_timer(struct timer_list *timer);
void add_timer(struct timer_list *timer);
int del_timer(struct timer_list *timer);
int mod_timer(struct timer_list *timer,unsigned long expires);
int advance_time(struct jiffval next);

#endif /* TIMER_H */
