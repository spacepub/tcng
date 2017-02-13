/*
 * echoh.c - Print qdisc/class hierarchy
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 Network Robots
 * Distributed under the LGPL.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tccext.h"
#include "echoh.h"


static void indent(FILE *file,int level)
{
    while (level--) fprintf(file,"  ");
}


static void force_failure(const TCCEXT_PRAGMA *pragmas,const char *location)
{
    const TCCEXT_PRAGMA *p;

    for (p = pragmas; p; p = p->next)
	if (!strcmp(p->pragma,"force_failure")) {
	    fprintf(stderr,"%s: forced failure\n",location);
	    exit(1);
	}
}


static void qdiscs(FILE *file,TCCEXT_QDISC *q,int level);


static void classes(FILE *file,TCCEXT_CLASS *c,int level)
{
    while (c) {
	force_failure(c->pragmas,c->location);
	indent(file,level);
	fprintf(file,"class %d\n",c->index);
	classes(file,c->classes,level+1);
	if (c->qdisc) qdiscs(file,c->qdisc,level+1);
	c = c->next;
    }
}


static void qdiscs(FILE *file,TCCEXT_QDISC *q,int level)
{
    force_failure(q->pragmas,q->location);
    indent(file,level);
    fprintf(file,"qdisc %d %s\n",q->index,q->type);
    classes(file,q->classes,level+1);
}


void echoh_blocks(FILE *file,TCCEXT_BLOCK *b)
{
   while (b) {
	force_failure(b->pragmas,b->location);
	fprintf(file,"%s %sgress\n",b->name,b->role == tbr_ingress ? "in" :
	  "e");
	qdiscs(file,b->qdiscs,1);
	b = b->next;
   }
}


void echoh_buckets(FILE *file,TCCEXT_BUCKET *p)
{
   while (p) {
	force_failure(p->pragmas,p->location);
	fprintf(file,"bucket %d\n",p->index);
	p = p->next;
   }
}
