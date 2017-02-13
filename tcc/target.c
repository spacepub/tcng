/*
 * target.c - Selection of output targets
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 Werner Almesberger
 * Copyright 2002 Bivio Networks
 */


#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "ext.h"
#include "target.h"


struct ext_target *ext_targets = NULL;


struct target {
    const char *element;
    const char *target;
    int on;
    struct target *next;
};


static struct target *targets = NULL;


void target_register(const char *element,const char *target,int on)
{
    struct target *t;

    t = alloc_t(struct target);
    t->element = stralloc(element);
    t->target = stralloc(target);
    t->on = on;
    t->next = targets;
    targets = t;
}


static struct target *find_target(const char *element,const char *target)
{
    struct target *t;

    for (t = targets; t; t = t->next)
	if (!strcmp(t->element,element) && !strcmp(t->target,target))
	    break;
    return t;
}


int have_target(const char *element,const char *target)
{
    struct target *t;

    t = find_target(element,target);
    return t ? t->on : 0;
}


void switch_target(const char *arg)
{
    struct target *t;
    char *target = NULL;
    char *s,*colon;
    int on = 1;

    s = stralloc(arg);
    colon = strchr(s,':');
    if (!colon) target = s;
    else {
	*colon = 0;
	target = colon+1; 
    }
    if (!strncmp(target,"no",2)) {
	on = 0;
	target += 2;
    }
    if (colon) {
	t = find_target(s,target);
	if (!t) errorf("no such target: %s:%s",s,target);
	t->on = on;
    }
    else {
	int none = 1;

	for (t = targets; t; t = t->next)
	    if (!strcmp(t->target,target)) {
		none = 0;
		t->on = on;
	    }
	if (none) errorf("no targets found: %s",target);
    }
    free(s);
}


void ext_target_register(const char *target)
{
    struct ext_target *new;
    char *s,*colon;

    s = stralloc(target);
    colon = strchr(s,':');
    if (!colon) error("no element in external target specification");
    *colon = 0;
    if (ext_targets) error("sorry, can only use one external target for now");
    if (!strcmp(s,"all")) dump_all = 1;
    target_register(s,colon+1,1);
    new = alloc_t(struct ext_target);
    new->name = stralloc(colon+1);
    new->next = ext_targets;
    ext_targets = new;
    free(s);
}


void ext_targets_configure(void)
{
    const struct ext_target *target;

    for (target = ext_targets; target; target = target->next)
	ext_config(target->name);
}
