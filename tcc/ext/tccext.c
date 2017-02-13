/*
 * tccext.c - Parse information received from tcc's external interface
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001,2002 Bivio Networks, Network Robots.
 * Distributed under the LGPL.
 */


#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include "../tccmeta.h"

#include "tccext.h"


#define MAX_LINE	65536	/* maximum input line */
#define MAX_BUF		1024	/* maximum token length */


#define _S(x) #x
#define S(x) _S(x)


int tccext_visitor = 0; /* visitor number */


static TCCEXT_ACTION *find_action(TCCEXT_CONTEXT *ctx,int index);


static void *alloc_zero(size_t size)
{
    void *p;

    p = malloc(size);
    if (!p) {
	perror("malloc");
	exit(1);
    }
    memset(p,0,size);
    return p;
}


static char *stralloc(const char *s)
{
    char *new;

    new = malloc(strlen(s)+1);
    if (new) return strcpy(new,s);
    perror("malloc");
    exit(1);
}


/* ----- Location support -------------------------------------------------- */


static int log10i(int value)
{
    int len;

    if (!value) return 1;
    for (len = 0; value; value /= 10) len++;
    return len;
}


static char *make_location(const char *fmt,...)
{
    va_list ap;
    const char *s;
    char *new;
    int len;

    len = strlen(fmt);
    va_start(ap,fmt);
    for (s = fmt; *s; s++)
	if (*s == '%')
	    switch (s[1]) {
		case '.':
		    s++;
		    if (s[1] != '*') abort();
		    s++;
		    if (s[1] != 's') abort();
		    len += va_arg(ap,int)-4;
		    (void) va_arg(ap,char *);
		    break;
		case 's':
		    len += strlen(va_arg(ap,char *))-2;
		    break;
		case 'd':
		    len += log10i(va_arg(ap,int))-2;
		    break;
		case '%':
		    len--;
		    s++;
		    break;
		default:
		    abort();
	    }
    va_end(ap);
    new = malloc(len+5);
    if (!new) {
	perror("malloc");
	exit(1);
    }
    strcpy(new,"[<");
    va_start(ap,fmt);
    vsprintf(new+2,fmt,ap);
    va_end(ap);
    strcpy(new+len+2,">]");
    return new;
}


/* ----- The offset group -------------------------------------------------- */


TCCEXT_OFFSET tccext_meta_field_group = {
    .base = NULL,
    .field = {
	.offset_group = NULL,
	.offset = 0,
	.length = 0,
    },
    .shift_left = 0,
    .group_number = META_FIELD_ROOT,
    .next = NULL,
};


static TCCEXT_OFFSET *find_group(TCCEXT_CONTEXT *ctx,int number)
{
    TCCEXT_OFFSET *group;

    if (!number) return NULL;
    if (number == META_FIELD_ROOT) return &tccext_meta_field_group;
    for (group = ctx->offset_groups; group; group = group->next)
	if (group->group_number == number) return group;
    fprintf(stderr,"offset group %d not found\n",number);
    exit(1);
}


static TCCEXT_FIELD parse_field(TCCEXT_CONTEXT *ctx,const char *line,int *len)
{
    TCCEXT_FIELD field;
    int base,pos;

    if (sscanf(line,"%i:%i:%i%n",&base,&field.offset,&field.length,&pos) < 3) {
	fprintf(stderr,"invalid field expression \"%s\"\n",line);
	exit(1);
    }
    field.offset_group = find_group(ctx,base);
    if (len) *len = pos;
    return field;
}


static void parse_offset(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_OFFSET *offset;
    const TCCEXT_OFFSET *scan;
    int base,pos,pos2;

    offset = alloc_zero(TE_SIZEOF(ctx,offset,TCCEXT_OFFSET));
    if (sscanf(line,"%i = %i+(%n",&offset->group_number,&base,&pos) < 2) {
	fprintf(stderr,"invalid offset expression \"%s\"\n",line);
	exit(1);
    }
    for (scan = ctx->offset_groups; scan; scan = scan->next)
	if (scan->group_number == offset->group_number) {
	    fprintf(stderr,"duplicate offset group %d\n",offset->group_number);
	    exit(1);
	}
    offset->base = find_group(ctx,base);
    offset->field = parse_field(ctx,line+pos,&pos2);
    if (sscanf(line+pos+pos2+1,"<< %i)",&offset->shift_left) == 1) {
	offset->next = ctx->offset_groups;
	ctx->offset_groups = offset;
	return;
    }
}


/* ----- The bit group ----------------------------------------------------- */


static TCCEXT_BIT *lookup_bit(TCCEXT_CONTEXT *ctx,int index)
{
    TCCEXT_BIT *bit;

    for (bit = ctx->blocks->bits; bit; bit = bit->next)
	if (bit->index == index) break;
    return bit;
}


static TCCEXT_BIT *find_bit(TCCEXT_CONTEXT *ctx,int index)
{
    TCCEXT_BIT *bit;

    if (!index) return NULL;
    bit = lookup_bit(ctx,index);
    if (bit) return bit;
    fprintf(stderr,"bit %d not found\n",index);
    exit(1);
}


static void parse_bit(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_BIT *bit;
    int index,action,false,true;
    int pos = 0;

    bit = alloc_zero(TE_SIZEOF(ctx,bit,TCCEXT_BIT));
    if (sscanf(line," %i = %n",&bit->index,&pos) < 2) {
	fprintf(stderr,"invalid index %d\n",index);
	exit(1);
    }
    line += pos;
    if (lookup_bit(ctx,bit->index)) {
	fprintf(stderr,"duplicate bit index %d\n",bit->index);
	exit(1);
    }
    if (sscanf(line," action %i",&action) != 1) {
	bit->action = find_action(ctx,action);
	if (!bit->action) {
	    fprintf(stderr,"no action %d\n",index);
	    exit(1);
	}
	memset(&bit->field,0,sizeof(bit->field)); /* play it safe */
    }
    else {
	bit->action = NULL;
	bit->field = parse_field(ctx,line,&pos);
    }
    if (sscanf(line," %i %i",&true,&false) != 2) {
	fprintf(stderr,"invalid next bit pointers\"%s\"\n",line);
	exit(1);
    }
    bit->edge[0] = find_bit(ctx,false);
    bit->edge[1] = find_bit(ctx,true);
    bit->next = ctx->blocks->bits;
    ctx->blocks->bits = bit;
}


/* ----- The rules group --------------------------------------------------- */


static int nibble(char c)
{
    return c >= 'a' ? c-'a'+10 : c >= 'A' ? c-'A'+10 : c-'0';
}


static TCCEXT_MATCH *parse_match(TCCEXT_CONTEXT *ctx,const char *line,int *len)
{
    TCCEXT_MATCH *match;
    int pos;

    match = alloc_zero(TE_SIZEOF(ctx,match,TCCEXT_MATCH));
    match->field = parse_field(ctx,line,&pos);
    if (!strncmp(line+pos,"=0x",3)) {
	int nibbles = 0,bytes;
	const char *walk;
	uint8_t *p;

	while (line[pos+3] == '0') pos++; /* avoid overflow on 0x000000... */
	for (walk = line+pos+3; *walk && isxdigit(*walk); walk++)
	    nibbles++;
	bytes = (match->field.length+7) >> 3;
	match->data = alloc_zero((bytes+sizeof(int)-1) & ~(sizeof(int)-1));
	p = match->data+bytes-((nibbles+1) >> 1);
	walk = line+pos+3;
	if (nibbles & 1) {
	    *p++ = nibble(*walk++);
	    nibbles--;
	}
	while (nibbles) {
	    *p = nibble(*walk++) << 4;
	    *p++ |= nibble(*walk++);
	    nibbles -= 2;
	}
	match->next = NULL;
	match->context = ctx;
	if (len) *len = walk-line;
	return match;
    }
    fprintf(stderr,"invalid match \"%s\"\n",line);
    exit(1);
}


static void parse_rule(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_RULE *rule,**walk;
    TCCEXT_MATCH **next;
    int actions,pos;

    if (!ctx->blocks) {
	fprintf(stderr,"can't handle rule without block\n");
	exit(1);
    }
    rule = alloc_zero(TE_SIZEOF(ctx,rule,TCCEXT_RULE));
    rule->matches = NULL;
    next = &rule->matches;
    while (*line && isdigit(*line)) {
	*next = parse_match(ctx,line,&pos);
	next = &(*next)->next;
	line += pos+1;
    }
    if (sscanf(line," action %i",&actions) != 1) {
	fprintf(stderr,"invalid action \"%s\"\n",line);
	exit(1);
    }
    rule->actions = find_action(ctx,actions);
    if (!rule->actions) {
	fprintf(stderr,"no actions %d\n",actions);
	exit(1);
    }
    rule->next = NULL;
    for (walk = &ctx->blocks->rules; *walk; walk = &(*walk)->next);
    *walk = rule;
}


/* ----- Barriers ---------------------------------------------------------- */


static void parse_barrier(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_RULE *rule,**walk;

    if (!ctx->blocks) {
	fprintf(stderr,"can't handle rule without block\n");
	exit(1);
    }
    rule = alloc_zero(TE_SIZEOF(ctx,rule,TCCEXT_RULE));
    rule->matches = NULL;
    rule->actions = NULL;
    rule->next = NULL;
    for (walk = &ctx->blocks->rules; *walk; walk = &(*walk)->next);
    *walk = rule;
}


/* ----- Pragmas and parameters -------------------------------------------- */


static TCCEXT_PRAGMA *parse_pragmas(const TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_PRAGMA *pragmas,**last;
    char *s;
    int pos = 0,len;

    last = &pragmas;
    while (sscanf(line+pos,"%as%n",&s,&len) >= 1) {
	*last = alloc_zero(TE_SIZEOF(ctx,pragma,TCCEXT_PRAGMA));
	(*last)->pragma = s;
	last = &(*last)->next;
	pos += len;
    }
    *last = NULL;
    return pragmas;
}


static void parse_pragma(TCCEXT_CONTEXT *ctx,const char *line)
{
    ctx->pragmas = parse_pragmas(ctx,line);
}


static TCCEXT_PARAMETER *parse_parameters(TCCEXT_CONTEXT *ctx,const char **line)
{
    TCCEXT_PARAMETER *result = NULL,**next = &result;
    char *name;
    unsigned long value;
    int pos;

    while (sscanf(*line,"%as %lu%n",&name,&value,&pos) >= 2) {
	TCCEXT_PARAMETER *prm;

	if (!strcmp(name,"pragma")) {
	    free(name);
	    return result;
	}
	prm = alloc_zero(TE_SIZEOF(ctx,parameter,TCCEXT_PARAMETER));
	prm->name = name;
	prm->value = value;
	prm->next = NULL;
	*next = prm;
	next = &prm->next;
	*line += pos;
    }
    return result;
}


/* ----- Queuing discipline definitions ------------------------------------ */


static TCCEXT_QDISC *fudge_qdisc(TCCEXT_CONTEXT *ctx,int index)
{
    TCCEXT_QDISC **anchor;

    anchor = &ctx->blocks->qdiscs;
    *anchor = alloc_zero(TE_SIZEOF(ctx,qdisc,TCCEXT_QDISC));
    (*anchor)->type = NULL;
    (*anchor)->index = index;
    (*anchor)->classes = NULL;
    (*anchor)->location = stralloc(ctx->blocks->location);
    return *anchor;
}


static TCCEXT_QDISC *find_qdisc(TCCEXT_CONTEXT *ctx,int index)
{
    TCCEXT_QDISC *qdisc;

     for (qdisc = ctx->blocks->qdiscs; qdisc; qdisc = qdisc->next)
	if (qdisc->index == index) return qdisc;
    return NULL;
}


static void parse_qdisc(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_QDISC *qdisc;
    int pos;

    qdisc = alloc_zero(TE_SIZEOF(ctx,qdisc,TCCEXT_QDISC));
    if (sscanf(line,"%i = %as%n",&qdisc->index,&qdisc->type,&pos) < 2) {
	fprintf(stderr,"unrecognized qdisc definition \"%s\"\n",line);
	exit(1);
    }
    if (find_qdisc(ctx,qdisc->index)) {
	fprintf(stderr,"duplicate qdisc index %d\n",qdisc->index);
	exit(1);
    }
    line += pos+1;
    qdisc->parameters = parse_parameters(ctx,&line);
    while (*line == ' ') line++;
    if (!strncmp(line,"pragma",6)) qdisc->pragmas = parse_pragmas(ctx,line+7);
    qdisc->location = ctx->blocks->role == tbr_ingress ?
      make_location("qdisc %s:ingress",ctx->blocks->name) :
      make_location("qdisc %s:%d",ctx->blocks->name,qdisc->index);
    qdisc->next = ctx->blocks->qdiscs;
    ctx->blocks->qdiscs = qdisc;
}


/* ----- Class definitions ------------------------------------------------- */


static int extract(TCCEXT_PARAMETER **params,const char *name)
{
    TCCEXT_PARAMETER **walk;

    for (walk = params; *walk; walk = &(*walk)->next)
	if (!strcmp((*walk)->name,name)) {
	    TCCEXT_PARAMETER *this = *walk;
	    int result;

	    *walk = this->next;
	    result = this->value;
	    free((char *) this->name);
	    free(this);
	    return result;
	}
    return -1;
}


static TCCEXT_CLASS *find_class(TCCEXT_CLASS *classes,int index)
{
    TCCEXT_CLASS *class;

    for (class = classes; class; class = class->next) {
	TCCEXT_CLASS *found;

	if (class->index == index) return class;
	found = find_class(class->classes,index);
	if (found) return found;
    }
    return NULL;
}


static TCCEXT_CLASS *find_qdisc_class(TCCEXT_CONTEXT *ctx,int qdisc_index,
  int class_index)
{
    TCCEXT_QDISC *qdisc;

    qdisc = find_qdisc(ctx,qdisc_index);
    if (!qdisc) return NULL;
    return find_class(qdisc->classes,class_index);
}


static TCCEXT_CLASS *fudge_class(TCCEXT_CONTEXT *ctx,int qdisc_index,
  int class_index)
{
    TCCEXT_QDISC *qdisc;
    TCCEXT_CLASS **anchor;

    qdisc = find_qdisc(ctx,qdisc_index);
    if (!qdisc) qdisc = fudge_qdisc(ctx,qdisc_index);
    for (anchor = &qdisc->classes; *anchor; anchor = &(*anchor)->next);
    *anchor = alloc_zero(TE_SIZEOF(ctx,class,TCCEXT_CLASS));
    (*anchor)->parent_qdisc = qdisc;
    (*anchor)->index = class_index;
    (*anchor)->location = stralloc(qdisc->location);
    return *anchor;
}


static void parse_class(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_CLASS *class,**anchor;
    int pos,qdisc,parent;

    class = alloc_zero(TE_SIZEOF(ctx,class,TCCEXT_CLASS));
    if (!sscanf(line,"%i =%n",&class->index,&pos)) {
	fprintf(stderr,"unrecognized class definition \"%s\"\n",line);
	exit(1);
    }
    class->parent_qdisc = ctx->blocks->qdiscs;
    line += pos+1;
    class->parameters = parse_parameters(ctx,&line);
    parent = extract(&class->parameters,"parent");
    qdisc = extract(&class->parameters,"qdisc");
    if (qdisc == -1) class->qdisc = NULL;
    else {
	class->qdisc = find_qdisc(ctx,qdisc);
	if (!class->qdisc) {
	    fprintf(stderr,"qdisc %d not found\n",(int) qdisc);
	    exit(1);
	}
    }
    if (find_class(class->parent_qdisc->classes,class->index)) {
	fprintf(stderr,"duplicate class %d on qdisc %d\n",class->index,
	  class->parent_qdisc->index);
	exit(1);
    }
    while (*line == ' ') line++;
    if (!strncmp(line,"pragma",6)) class->pragmas = parse_pragmas(ctx,line+7);
    class->location = make_location("class %.*s:%d",
      strlen(ctx->blocks->qdiscs->location)-10,ctx->blocks->qdiscs->location+8,
      class->index);
    if (parent == -1) anchor = &class->parent_qdisc->classes;
    else {
	TCCEXT_CLASS *parent_class;

	parent_class = find_class(class->parent_qdisc->classes,parent);
	if (!parent_class) {
	    fprintf(stderr,"parent class %d not found\n",(int) parent);
	    exit(1);
	}
	anchor = &parent_class->classes;
    }
    class->next = NULL;
    while (*anchor) anchor = &(*anchor)->next;
    *anchor = class;
}


/* ----- The block group --------------------------------------------------- */


static void parse_block(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_BLOCK *block;
    const TCCEXT_BLOCK *scan;
    char role[11];
    int pos;

    block = alloc_zero(TE_SIZEOF(ctx,block,TCCEXT_BLOCK));
    if (sscanf(line,"%as %10s%n",&block->name,role,&pos) < 2) {
	fprintf(stderr,"invalid block \"%s\"\n",line);
	exit(1);
    }
    if (!strcmp(role,"ingress")) block->role = tbr_ingress;
    else if (!strcmp(role,"egress")) block->role = tbr_egress;
    else {
	fprintf(stderr,"unrecognized role \"%s\"\n",role);
	exit(1);
    }
    for (scan = ctx->blocks; scan; scan = scan->next)
	if (!strcmp(block->name,scan->name) && block->role == scan->role) {
	    fprintf(stderr,"duplicate block \"%s\" %sgress\n",block->name,
	      block->role == tbr_ingress ? "in" : "e");
	    exit(1);
	}
    block->qdiscs = NULL;
    block->rules = NULL;
    block->actions = NULL;
    block->pragmas = parse_pragmas(ctx,line+pos);
    block->location = make_location("device %s",block->name);
    block->next = ctx->blocks;
    ctx->blocks = block;
}


/* ----- The bucket group -------------------------------------------------- */


static TCCEXT_BUCKET *find_bucket(TCCEXT_CONTEXT *ctx,int index)
{
    TCCEXT_BUCKET *bucket;

    for (bucket = ctx->buckets; bucket; bucket = bucket->next)
	if (bucket->index == index) return bucket;
    fprintf(stderr,"no bucket %d\n",index);
    exit(1);
}


static void parse_bucket(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_BUCKET *bucket;
    const TCCEXT_BUCKET *scan;
    unsigned long rate,depth,initial;
    int overflow,pos;

    /* @@@ %lu should allow hex too %i */
    bucket = alloc_zero(TE_SIZEOF(ctx,bucket,TCCEXT_BUCKET));
    if (sscanf(line,"%i = %lu %i %lu %lu %i%n",&bucket->index,&rate,
      &bucket->mpu,&depth,&initial,&overflow,&pos) < 6) {
	fprintf(stderr,"unrecognized bucket \"%s\"\n",line);
	exit(1);
    }
    for (scan = ctx->buckets; scan; scan = scan->next)
	if (scan->index== bucket->index) {
	    fprintf(stderr,"duplicate bucket index %d\n",bucket->index);
	    exit(1);
	}
    bucket->rate = rate;
    bucket->depth = depth;
    bucket->initial_tokens = initial;
    bucket->overflow = overflow ? find_bucket(ctx,overflow) : NULL;
    bucket->pragmas = parse_pragmas(ctx,line+pos);
    bucket->location = make_location("police %d",bucket->index);
    bucket->next = ctx->buckets;
    ctx->buckets = bucket;
}


/* ----- The actions group ------------------------------------------------- */


static TCCEXT_ACTION *find_action(TCCEXT_CONTEXT *ctx,int index)
{
    TCCEXT_ACTION *action;

    for (action = ctx->blocks->actions; action; action = action->next)
	if (action->index == index) break;
    return action;
}


static TCCEXT_CLASS_LIST *parse_class_list(TCCEXT_CONTEXT *ctx,
  const char **line)
{
    TCCEXT_CLASS_LIST *class_list,**next = &class_list;

    do {
	int qdisc,class,pos;

	(*line)++;
	if (sscanf(*line,"%i:%i%n",&qdisc,&class,&pos) < 2 ) {
	    fprintf(stderr,"not a class \"%s\"\n",*line);
	    exit(1);
	}
	*next = alloc_zero(TE_SIZEOF(ctx,class_list,TCCEXT_CLASS_LIST));
	(*next)->class = find_qdisc_class(ctx,qdisc,class);
	if (!(*next)->class) (*next)->class = fudge_class(ctx,qdisc,class);
	(*next)->next = NULL;
	next = &(*next)->next;
	*line += pos;
    }
    while (**line == ',');
    return class_list;
}


static TCCEXT_ACTION *parse_action(TCCEXT_CONTEXT *ctx,const char **line)
{
    TCCEXT_ACTION *action;
    int next,bucket,pos;

    while (**line == ' ') (*line)++;
    if (sscanf(*line,"action %i%n",&next,&pos) >= 1) {
	*line += pos+1;
	return find_action(ctx,next);
    }
    action = alloc_zero(TE_SIZEOF(ctx,action,TCCEXT_ACTION));
    if (!strncmp(*line,"class",4)) {
	action->type = tat_class;
	*line += 4;
	action->u.class_list = parse_class_list(ctx,line);
	return action;
    }
    if (!strncmp(*line,"drop",4)) {
	action->type = tat_drop;
	*line += 4;
	return action;
    }
    if (!strncmp(*line,"unspec",7)) {
	action->type = tat_unspec;
	*line += 7;
	return action;
    }
    if (sscanf(*line,"conform %i%n",&bucket,&pos) >= 1) {
	action->type = tat_conform;
	action->u.conform.bucket = find_bucket(ctx,bucket);
	*line += pos+1;
	action->u.conform.yes_action = parse_action(ctx,line);
	action->u.conform.no_action = parse_action(ctx,line);
	return action;
    }
    if (sscanf(*line,"count %i%n",&bucket,&pos) >= 1) {
	action->type = tat_count;
	action->u.count.bucket = find_bucket(ctx,bucket);
	*line += pos+1;
	action->u.count.action = parse_action(ctx,line);
	return action;
    }
    fprintf(stderr,"unrecognized action \"%s\"\n",*line);
    exit(1);
}


static void parse_actions(TCCEXT_CONTEXT *ctx,const char *line)
{
    TCCEXT_ACTION *action;
    const TCCEXT_ACTION *scan;
    int index,pos;

    if (!ctx->blocks) {
	fprintf(stderr,"can't handle action without block\n");
	exit(1);
    }
    if (!sscanf(line,"%i =%n",&index,&pos)) {
	fprintf(stderr,"unrecognized action definition \"%s\"\n",line);
	exit(1);
    }
    for (scan = ctx->blocks->actions; scan; scan = scan->next)
	if (scan->index == index) {
	    fprintf(stderr,"duplicate action index %d\n",index);
	    exit(1);
	}
    line += pos+1;
    action = parse_action(ctx,&line);
    action->index = index;
    action->next = ctx->blocks->actions;
    ctx->blocks->actions = action;
}


/* ----- Construction ------------------------------------------------------ */


TCCEXT_CONTEXT *tccext_parse(FILE *file,TCCEXT_SIZES *sizes)
{
    TCCEXT_CONTEXT *ctx;
    char line[MAX_LINE];
    
    ctx = alloc_zero(__TE_SIZEOF(sizes ? sizes->context : 0,
      sizeof(TCCEXT_CONTEXT)));
    ctx->offset_groups = NULL;
    ctx->blocks = NULL;
    if (sizes) ctx->sizes = *sizes;
    else memset(&ctx->sizes,0,sizeof(TCCEXT_SIZES));
    while (fgets(line,sizeof(line),file)) {
	char buf[MAX_BUF];
	char *here;
	int pos;

	here = strchr(line,'\n');
	if (here) *here = 0;
	here = strchr(line,'#');
	if (here) *here = 0;
	if (!*line) continue;
	if (!sscanf(line,"%" S(MAX_BUF) "s%n",buf,&pos)) {
	    fprintf(stderr,"unrecognized line \"%s\"\n",line);
	    exit(1);
	}
	if (!strcmp(buf,"pragma")) parse_pragma(ctx,line+pos+1);
	else if (!strcmp(buf,"block")) parse_block(ctx,line+pos+1);
	else if (!strcmp(buf,"qdisc")) parse_qdisc(ctx,line+pos+1);
	else if (!strcmp(buf,"class")) parse_class(ctx,line+pos+1);
	else if (!strcmp(buf,"offset")) parse_offset(ctx,line+pos+1);
	else if (!strcmp(buf,"bucket")) parse_bucket(ctx,line+pos+1);
	else if (!strcmp(buf,"action")) parse_actions(ctx,line+pos+1);
	else if (!strcmp(buf,"bit")) parse_bit(ctx,line+pos+1);
	else if (!strcmp(buf,"match")) parse_rule(ctx,line+pos+1);
	else if (!strcmp(buf,"barrier")) parse_barrier(ctx,line+pos+1);
	else {
	    fprintf(stderr,"unrecognized line \"%s\"\n",line);
	    exit(1);
	}
    }
    return ctx;
}


/* ----- Destruction ------------------------------------------------------- */


static void destroy_pragmas(TCCEXT_PRAGMA *p)
{
    while (p) {
	TCCEXT_PRAGMA *next = p->next;

	free((char *) p->pragma);
	free(p);
	p = next;
    }
}


static void destroy_parameters(TCCEXT_PARAMETER *prm)
{
    while (prm) {
	TCCEXT_PARAMETER *next = prm->next;

	free((char *) prm->name);
	free(prm);
	prm = next;
    }
}


static void destroy_action(TCCEXT_ACTION *a)
{
    switch (a->type) {
	case tat_class:
	    while (a->u.class_list) {
		TCCEXT_CLASS_LIST *next = a->u.class_list->next;

		free(a->u.class_list);
		a->u.class_list = next;
	    }
	    break;
	case tat_conform:
	case tat_count:
	default:
	    break;
    }
}


static void destroy_classes(TCCEXT_CLASS *classes)
{
    while (classes) {
	TCCEXT_CLASS *next = classes->next;

	destroy_parameters(classes->parameters);
	destroy_pragmas(classes->pragmas);
	free((char *) classes->location);
	destroy_classes(classes->classes);
	free(classes);
	classes = next;
    }
}


void tccext_destroy(TCCEXT_CONTEXT *ctx)
{
    destroy_pragmas(ctx->pragmas);
    while (ctx->buckets) {
	TCCEXT_BUCKET *next_bucket;

	destroy_pragmas(ctx->buckets->pragmas);
	free((char *) ctx->buckets->location);
	next_bucket = ctx->buckets->next;
	free(ctx->buckets);
	ctx->buckets = next_bucket;
    }
    while (ctx->offset_groups) {
	TCCEXT_OFFSET *next_offset;

	next_offset = ctx->offset_groups->next;
	free(ctx->offset_groups);
	ctx->offset_groups = next_offset;
    }
    while (ctx->blocks) {
	TCCEXT_BLOCK *next_block;

	while (ctx->blocks->qdiscs) {
	    TCCEXT_QDISC *qdisc = ctx->blocks->qdiscs;
	    TCCEXT_QDISC *next_qdisc;

	    destroy_classes(qdisc->classes);
	    destroy_parameters(qdisc->parameters);
	    destroy_pragmas(qdisc->pragmas);
	    free((char *) qdisc->location);
	    next_qdisc = qdisc->next;
	    free(qdisc);
	    ctx->blocks->qdiscs = next_qdisc;
	}
	while (ctx->blocks->actions) {
	    TCCEXT_ACTION *action = ctx->blocks->actions;
	    TCCEXT_ACTION *next_action;

	    destroy_action(action);
	    next_action = action->next;
	    free(action);
	    ctx->blocks->actions = next_action;
	}
	while (ctx->blocks->rules) {
	    TCCEXT_RULE *rule = ctx->blocks->rules;
	    TCCEXT_RULE *next_rule;

	    while (ctx->blocks->rules->matches) {
		TCCEXT_MATCH *next_match;

		next_match = rule->matches->next;
		free(rule->matches->data);
		free(rule->matches);
		rule->matches = next_match;
	    }
	    next_rule = rule->next;
	    free(rule);
	    ctx->blocks->rules = next_rule;
	}
	while (ctx->blocks->bits) {
	    TCCEXT_BIT *next_bit = ctx->blocks->bits;

	    free(ctx->blocks->bits);
	    ctx->blocks->bits = next_bit;
	}
	/* destroy_fsm here @@@ */
	destroy_pragmas(ctx->blocks->pragmas);
	free((char *) ctx->blocks->name);
	free((char *) ctx->blocks->location);
	next_block = ctx->blocks->next;
	free(ctx->blocks);
	ctx->blocks = next_block;
    }
}
