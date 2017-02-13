/*
 * match.c - Simple packet matcher using tccext
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001,2002 Network Robots
 * Copyright 2002 Werner Almesberger
 */

/*
 * Note: the code is really sloppy with value ranges in policing. So be sure
 * to use it only with broad safety margins.
 */


#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>

#include <tccmeta.h>

#include "tccext.h"


#define _S(x) #x
#define S(x) _S(x)


extern double get_time(void); /* from cls_ext_test.c */


static uint32_t extract_bits(uint8_t *data,int start,int bits)
{
    uint32_t res = 0;

    if ((bits+(start & 7)) > 32) abort();
    data += start >> 3;
    start &= 7;
    res = *data++ & ((1 << (8-start))-1);
    bits -= 8-start;
    while (bits > 0) {
	res = (res << 8) | *data++;
	bits -= 8;
    }
    if (bits) res >>= -bits;
    return res;
}


static int offset(const TCCEXT_OFFSET *group,uint8_t *raw)
{
    if (!group) return 0;
    if (group == &tccext_meta_field_group) {
	fprintf(stderr,"\"match\" can't handle meta fields in offset\n");
	exit(1);
    }
    return offset(group->base,raw)+
      (extract_bits(raw,offset(group->field.offset_group,raw)+
      group->field.offset,group->field.length) << group->shift_left);
}


static int match_1(const TCCEXT_MATCH *m,uint8_t *raw,uint16_t protocol)
{
    if (m->field.offset_group == &tccext_meta_field_group) {
	if (m->field.offset != META_PROTOCOL_OFFSET*8) {
	    fprintf(stderr,"invalid offset for meta_protocol\n");
	    exit(1);
	}
	if (m->field.length != META_PROTOCOL_SIZE*8) {
	    fprintf(stderr,"invalid size for meta_protocol access");
	    exit(1);
	}
	return ntohs(protocol) ==
	  extract_bits(m->data,(-m->field.length) & 7,m->field.length);
    }
    return extract_bits(raw,offset(m->field.offset_group,raw)+m->field.offset,
      m->field.length) == extract_bits(m->data,(-m->field.length) & 7,
      m->field.length);
}


struct my_bucket {
    TCCEXT_BUCKET b;	/* tccext's bucket data */
    int tokens;		/* current number of tokens in bucket */
    double last_refill;	/* time of last bucket refill */
    double max_time;	/* maximum delta time for bucket, with overflow chain */
    int initialized;	/* 1 if initialized, 0 otherwise (set to 0 by tccext) */
};


static void initialize_bucket(struct my_bucket *bucket)
{
    TCCEXT_BUCKET *overflow;
    int max_tokens;

    if (bucket->initialized) return;
    bucket->initialized = 1;
    bucket->tokens = bucket->b.initial_tokens;
    max_tokens = bucket->b.depth;
    for (overflow = bucket->b.overflow; overflow; overflow = overflow->overflow)
	max_tokens += overflow->depth;
    bucket->max_time = max_tokens/(double) bucket->b.rate;
    bucket->last_refill = get_time();
}


static void refill_bucket(struct my_bucket *bucket)
{
    TCCEXT_BUCKET *walk;
    int tokens;
    double now,delta;

    initialize_bucket(bucket);
    now = get_time();
    delta = now-bucket->last_refill;
    bucket->last_refill = now;
    if (delta > bucket->max_time) delta = bucket->max_time;
    tokens = delta*bucket->b.rate;
    for (walk = &bucket->b; walk && tokens; walk = walk->overflow) {
	struct my_bucket *b = (struct my_bucket *) walk;

	b->tokens += tokens;
	if (b->tokens <= walk->depth) tokens = 0;
	else {
	    tokens = b->tokens-walk->depth;
	    b->tokens = walk->depth;
	}
    }
}


static int action(const TCCEXT_ACTION *a,int len,uint32_t *class)
{
    struct my_bucket *bucket;
    int policed_len;

    switch (a->type) {
	case tat_class:
	    *class = (a->u.class_list->class->parent_qdisc->index << 16) |
	      a->u.class_list->class->index;
	    return 0; /* TC_POLICE_OK */
	case tat_drop:
	    return 2; /* TC_POLICE_SHOT */
	case tat_unspec:
	    return -1; /* TC_POLICE_UNSPEC */
	case tat_conform:
	    bucket = (struct my_bucket *) a->u.conform.bucket;
	    refill_bucket(bucket);
	    policed_len = len > bucket->b.mpu ? len : bucket->b.mpu;
	    return bucket->tokens >= policed_len ?
	      action(a->u.conform.yes_action,len,class) :
	      action(a->u.conform.no_action,len,class);
	case tat_count:
	    bucket = (struct my_bucket *) a->u.count.bucket;
	    refill_bucket(bucket);
	    policed_len = len > bucket->b.mpu ? len : bucket->b.mpu;
	    if (bucket->tokens < policed_len) bucket->tokens = 0;
	    else bucket->tokens -= policed_len;
	    return action(a->u.count.action,len,class);
	default:
	    fprintf(stderr,"\"match\" can't handle action type %d\n",
	      (int) a->type);
	    exit(1);
    }
}


int match(uint8_t *raw,int len,uint16_t protocol,uint32_t *class);
  /* extern in cls_ext_test.c */

int match(uint8_t *raw,int len,uint16_t protocol,uint32_t *class)
{
    static TCCEXT_CONTEXT *context;
    TCCEXT_RULE *r;

    if (!context) {
	static TCCEXT_SIZES sizes = { .bucket = sizeof(struct my_bucket) };
	FILE *file;

	file = fopen("__" S(NAME) ".out","r");
	if (!file) {
	    perror("_" S(NAME) ".out");
	    exit(1);
	}
	context = tccext_parse(file,&sizes);
	if (remove("__" S(NAME) ".out")) perror("remove __" S(NAME) ".out");
    }
    if (context->blocks->next) {
	fprintf(stderr,"match.c cannot handle multiple blocks\n");
	exit(1);
    }
    for (r = context->blocks->rules; r; r = r->next) {
	TCCEXT_MATCH *m;

	if (!r->actions) continue;
	for (m = r->matches; m; m = m->next)
	    if (!match_1(m,raw,protocol)) break;
	if (!m) {
	    int act;

	    act = action(r->actions,len,class);
	    if (act != -1) return act;
	    while (r && r->actions) r = r->next;
	    if (!r) break;
	}
    }
    return -1; /* TC_POLICE_UNSPEC */
}
