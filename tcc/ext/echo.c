/*
 * echo.c - Read tccext data structures from stdin, and print them on stderr
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001,2002 Network Robots
 */

/*
 * Note: echo.c does is just for debugging. It does not aim to generate
 * output that could be again used at the tccext interface (although it is
 * very close to that, and perhaps one day it might).
 *
 * Some differences:
 * - parameters are not always in alphabetic order (i.e. the pseudo-parameters
 *   "qdisc" and "parent" are appended)
 * - qdiscs are output in the wrong order
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tccext.h"


static void print_field(FILE *file,TCCEXT_FIELD field)
{
    fprintf(file,"%d:%d:%d",
      field.offset_group ? field.offset_group->group_number : 0,field.offset,
      field.length);
}


static void print_pragmas(FILE *file,const TCCEXT_PRAGMA *pragmas)
{
    const TCCEXT_PRAGMA *pragma;

    for (pragma = pragmas; pragma; pragma = pragma->next)
	fprintf(file," %s",pragma->pragma);
}


static void print_pragma(FILE *file,const TCCEXT_PRAGMA *pragma)
{
    if (pragma) {
	fprintf(file,"pragma");
	print_pragmas(file,pragma);
	fputc('\n',file);
    }
}


static void print_buckets(FILE *file,const TCCEXT_BUCKET *buckets)
{
    const TCCEXT_BUCKET *bucket;

    for (bucket = buckets; bucket; bucket = bucket->next) {
	fprintf(file,"bucket %d = %lu %d %lu %lu %d",bucket->index,
	  (unsigned long) bucket->rate,bucket->mpu,
	  (unsigned long) bucket->depth,(unsigned long) bucket->initial_tokens,
	  bucket->overflow ? bucket->overflow->index : 0);
	print_pragmas(file,bucket->pragmas);
	fputc('\n',file);
    }
}


static void print_offsets(FILE *file,const TCCEXT_OFFSET *offsets)
{
    const TCCEXT_OFFSET *offset;

    for (offset = offsets; offset; offset = offset->next) {
	fprintf(file,"offset %d = %d+",offset->group_number,
	  offset->base ? offset->base->group_number : 0);
	print_field(file,offset->field);
	fprintf(file," << %d)\n",offset->shift_left);
    }
}


static void print_action(FILE *file,const TCCEXT_ACTION *a)
{
    TCCEXT_CLASS_LIST *walk;

    switch (a->type) {
	case tat_conform:
	    fprintf(file," conform %d action %d action %d",
	      a->u.conform.bucket->index,a->u.conform.yes_action->index,
	      a->u.conform.no_action->index);
	    break;
	case tat_count:
	    fprintf(file," count %d action %d",a->u.count.bucket->index,
	      a->u.count.action->index);
	    break;
	case tat_class:
	    fprintf(file," class");
	    for (walk = a->u.class_list; walk; walk = walk->next)
		fprintf(file,"%c%d:%d",walk == a->u.class_list ? ' ' : ',',
		  walk->class->parent_qdisc->index,walk->class->index);
	    break;
	case tat_drop:
	    fprintf(file," drop");
	    break;
	case tat_unspec:
	    fprintf(file," unspec");
	    break;
	default:
	    abort();
    }
}


static void print_actions(FILE *file,const TCCEXT_ACTION *actions)
{
    const TCCEXT_ACTION *action;

    for (action = actions; action; action = action->next) {
	fprintf(file,"action %d =",action->index);
	print_action(file,action);
	fputc('\n',file);
    }
}


static void print_hex(FILE *file,const uint8_t *d,int len)
{
    const uint8_t *end = d+len;

    fprintf(file,"0x");
    while (d < end && !*d) d++;
    if (d == end) {
	putc('0',file);
	return;
    }
    if (!(*d & 0xf0)) {
	fprintf(file,"%x",*d);
	d++;
    }
    while (d < end) fprintf(file,"%02x",*d++);
}


static void print_matches(FILE *file,const TCCEXT_MATCH *matches)
{
    const TCCEXT_MATCH *match;

    for (match = matches; match; match = match->next) {
	int len;

	putc(' ',file);
	print_field(file,match->field);
	putc('=',file);
	len = (match->field.length+7) >> 3;
	print_hex(file,match->data,len);
    }
}


static void print_rules(FILE *file,const TCCEXT_RULE *rules)
{
    const TCCEXT_RULE *rule;

    for (rule = rules; rule; rule = rule->next)
	if (!rule->actions) fprintf(file,"barrier\n");
	else {
	    fprintf(file,"match");
	    print_matches(file,rule->matches);
	    fprintf(file," action %d\n",rule->actions->index);
	}
}


static void print_parameters(FILE *file,const TCCEXT_PARAMETER *parameters)
{
    const TCCEXT_PARAMETER *prm;

    for (prm = parameters; prm; prm = prm->next)
	fprintf(file," %s %lu",prm->name,(unsigned long) prm->value);
}


static void print_classes(FILE *file,const TCCEXT_CLASS *classes,
  const TCCEXT_CLASS *parent_class)
{
    const TCCEXT_CLASS *class;

    for (class = classes; class; class = class->next) {
	fprintf(file,"class %d =",class->index);
	print_parameters(file,class->parameters);
	if (parent_class) fprintf(file," parent %d",parent_class->index);
	if (class->qdisc) fprintf(file," qdisc %d",class->qdisc->index);
	if (class->pragmas) {
	    fprintf(file," pragma");
	    print_pragmas(file,class->pragmas);
	}
	fputc('\n',file);
	print_classes(file,class->classes,class);
    }
}


static void print_qdiscs(FILE *file,const TCCEXT_QDISC *qdiscs)
{
    const TCCEXT_QDISC *qdisc;

    for (qdisc = qdiscs; qdisc; qdisc = qdisc->next) {
	if (!qdisc->type) continue; /* anonymous qdisc */
	fprintf(file,"qdisc %d = %s",qdisc->index,qdisc->type);
	print_parameters(file,qdisc->parameters);
	if (qdisc->pragmas) {
	    fprintf(file," pragma");
	    print_pragmas(file,qdisc->pragmas);
	}
	fputc('\n',file);
	print_classes(file,qdisc->classes,NULL);
    }
}


static void print_blocks(FILE *file,const TCCEXT_BLOCK *blocks)
{
    const TCCEXT_BLOCK *block;

    for (block = blocks; block; block = block->next) {
	fprintf(file,"block %s %s",block->name,
	  block->role == tbr_ingress ? "ingress" : "egress");
	print_pragmas(file,block->pragmas);
	fputc('\n',file);
	print_qdiscs(file,block->qdiscs);
	print_actions(stderr,block->actions);
	print_rules(stderr,block->rules);
    }
}


static void usage(const char *name)
{
    fprintf(stderr,"usage: %s mode ...\n",name);
    exit(1);
}


int main(int argc,const char **argv)
{
    TCCEXT_CONTEXT *ctx;

    if (argc < 2) usage(*argv);
    if (!strcmp(argv[1],"config")) {
	int i;

	printf("debug_target\n");
	for (i = 2; i < argc; i++) printf("%s\n",argv[i]);
	return 0;
    }
    if (!strcmp(argv[1],"check")) return 0;
    if (strcmp(argv[1],"build")) {
	fprintf(stderr,"%s: unrecognized mode %s",*argv,argv[1]);
	return 1;
    }
    if (argc < 3) usage(*argv);
    ctx = tccext_parse(stdin,NULL);
    print_pragma(stderr,ctx->pragmas);
    print_buckets(stderr,ctx->buckets);
    print_offsets(stderr,ctx->offset_groups);
    print_blocks(stderr,ctx->blocks);
    return 0;
}
