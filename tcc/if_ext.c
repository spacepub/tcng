/*
 * if_ext.c - Processing of the "if" command, using external matcher
 *
 * Written 2001-2003 by Werner Almesberger 
 * Copyright 2001 Network Robots
 * Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
 * Copyright 2003 Werner Almesberger
 */

/*
 * Description of invocation and data format in ../doc/external.tex
 *
 * NOTE: if_ext.c does not yet implement the functionality described in
 *       external.tex !
 */

/*
 * @@@ KNOWN BUG: we may generate actions that are never used, because the
 * corresponding rule is eliminated.
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/wait.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "tree.h"
#include "data.h"
#include "field.h"
#include "op.h"
#include "tc.h"
#include "iflib.h"
#include "if.h"
#include "ext.h"


int generate_default_class; /* if_ext generates default classes */
int use_bit_tree; /* use experimental bit tree algorithm */
int no_combine; /* don't combine ifs into single expression */


/* ----------------------------- Offset groups ----------------------------- */


static struct offset_group {
    int number;		/* offset group number */
    int base;		/* offset group this group adds to */
    int value_base;	/* offset group of the access */
    int offset;		/* constant offset of access */
    int length;		/* length of access */
    int shift;		/* left-shift by that many bits */
    struct offset_group *next;
} *offset_groups = NULL;


static int next_offset_group = AUTO_OFFSET_GROUP_BASE;


static int lookup_offset_group(int base,int value_base,int offset,int length,
  int shift)
{
    struct offset_group **walk;

    if (debug > 1)
	debugf("lookup_offset_group(base %d,%d:%d:%d)",base,value_base,offset,
	  length);
    for (walk = &offset_groups; *walk; walk = &(*walk)->next)
	if ((*walk)->base == base && (*walk)->value_base == value_base &&
	  (*walk)->offset == offset && (*walk)->length == length &&
	  (*walk)->shift == shift) {
	    if (debug > 1)
		debugf("lookup_offset_group: found %d",(*walk)->number);
	    return (*walk)->number;
	}
    while (offset_group_taken(next_offset_group)) next_offset_group++;
    *walk = alloc_t(struct offset_group);
    (*walk)->number = next_offset_group++;
    (*walk)->base = base;
    (*walk)->value_base = value_base;
    (*walk)->offset = offset;
    (*walk)->length = length;
    (*walk)->shift = shift;
    (*walk)->next = NULL;
    if (debug > 1) debugf("lookup_offset_group: new %d",(*walk)->number);
    return (*walk)->number;
}


static int build_offset_group(int base,int value_base,DATA d);


/*
 * @@@ Merge this with iflib_red.c:do_optimize_eq
 */

static int new_offset_group(int base,int value_base,DATA d)
{
    uint32_t mask = 0xffffffff;
    int shift = 3; /* we're bit-based, op_offset is byte-based */
    int offset,length;

    if (debug > 1)
	debugf("new_offset_group(base %d,value_base %d)",base,value_base);
    while (d.op && d.op->dsc != &op_access) {
	if (d.op->dsc == &op_shift_left) {
	    if (d.op->b.op || d.op->b.type != dt_unum)
		dump_failed(d,"bad left shift");
	    shift += d.op->b.u.unum;
	    mask >>= d.op->b.u.unum;
	    d = d.op->a;
	    continue;
	}
	if (d.op->dsc == &op_shift_right) {
	    if (d.op->b.op || d.op->b.type != dt_unum)
		dump_failed(d,"bad right shift");
	    shift -= d.op->b.u.unum;
	    mask <<= d.op->b.u.unum;
	    d = d.op->a;
	    continue;
	}
	if (d.op->dsc == &op_and) {
	     if (!d.op->a.op && d.op->a.type == dt_unum) {
		mask &= d.op->a.u.unum;
		d = d.op->b;
	    }
	    else {
		if (d.op->b.op || d.op->b.type != dt_unum)
		    dump_failed(d,"no constant in and");
		mask &= d.op->b.u.unum;
		d = d.op->a;
            }
	    continue;
        }
	if (d.op->dsc == &op_field_root) {
	    value_base = d.op->b.u.field_root->offset_group;
	    d = d.op->a;
	    continue;
	}
	if (d.op->dsc == &op_offset) {
	    value_base = build_offset_group(value_base,value_base,d.op->b);
	    d = d.op->a;
	    continue;
	}
        dump_failed(d,"bad operator");
    }
    assert(d.op && d.op->dsc == &op_access);
    assert(!d.op->b.op && d.op->b.type == dt_unum);
    assert(!d.op->c.op && d.op->c.type == dt_unum);
    offset = d.op->b.u.unum*8;
    length = d.op->c.u.unum;
    assert(length);
    if (!mask) return base;
    while (!(mask & 1)) {
	length--;
	mask >>= 1;
	shift++;
    }
    if (shift < 0) dump_failed(d,"negative shift");
    mask <<= 32-length;
    if (!mask) return base;
    while (!(mask & 0x80000000)) {
	offset++;
	length--;
	mask <<= 1;
    }
    return lookup_offset_group(base,value_base,offset,length,shift);
}


static int build_offset_group(int base,int value_base,DATA d)
{
    if (debug > 1)
	debugf("build_offset_group(base %d,value_base %d)",base,value_base);
    while (d.op && d.op->dsc == &op_plus) {
	base = build_offset_group(base,value_base,d.op->a);
	d = d.op->b;
    }
    return new_offset_group(base,value_base,d);
}


int get_offset_group(DATA d,int base); /* shared with iflib_bit.c */
int get_offset_group(DATA d,int base)
{
    assert(d.op && d.op->dsc == &op_offset);
    if (debug > 1) debugf("get_offset_group(base %d)",base);
    return build_offset_group(base,base,d.op->b);
}


static void do_scan_offsets(DATA d,int base)
{
    if (debug > 1) debugf("scan_offsets(base %d)",base);
    if (!d.op) return;
    if (d.op->dsc == &op_field_root) base = d.op->b.u.field_root->offset_group;
    if (d.op->dsc == &op_offset) base = get_offset_group(d,base);
    do_scan_offsets(d.op->a,base);
    do_scan_offsets(d.op->b,base);
    do_scan_offsets(d.op->c,base);
}


static void scan_offsets(DATA d)
{
    do_scan_offsets(d,0);
}


static void dump_offsets(FILE *file)
{
    struct offset_group *group;

    for (group = offset_groups; group; group = group->next)
	fprintf(file,"offset %d = %d+(%d:%d:%d << %d)\n",group->number,
	  group->base,group->value_base,group->offset,group->length,
	  group->shift);
}


void reset_offsets(void)
{
    while (offset_groups) {
	struct offset_group *next = offset_groups->next;

	free(offset_groups);
	offset_groups = next;
    }
    next_offset_group = AUTO_OFFSET_GROUP_BASE;
}


/* ----------------------------- Actions group ----------------------------- */


static void dump_actions(DATA d)
{
    if (debug > 1) debugf("dump_actions");
    if (action_tree(d)) {
	register_actions(d);
	return;
    }
    if (!d.op) return;
    dump_actions(d.op->a);
    dump_actions(d.op->b);
    dump_actions(d.op->c);
}


/* -------------------------------- Matches -------------------------------- */


/*
 * This isn't exactly optimized for speed ...
 */


static struct bit_match {
    int offset_group;
    int offset;
    int value;
    struct bit_match *next;
} *bits = NULL,**next_bit = &bits;


static void sort_bits(void)
{
    struct bit_match *i,*j,tmp;

    /* ye ole bubblesort */
    /* sort within offset_group by offset (@@@ don't sort by offset_group for
       now - need to walk groups to extract partial order) */
    for (i = bits; i; i = i->next)
	for (j = i->next; j; j = j->next)
	    if (i->offset_group == j->offset_group && i->offset > j->offset) {
		struct bit_match *i_next = i->next,*j_next = j->next;

		tmp = *i;
		*i = *j;
		*j = tmp;
		i->next = i_next;
		j->next = j_next;
	    }
    /* TODO: check if this is really valid wrt ordering constraints @@@ */
}


static void collect_bits(FILE *file)
{
    struct bit_match *walk,*end;

    for (walk = bits; walk; walk = end) {
	struct bit_match *scan;
	int length = 1;
	int next_offset = walk->offset;
	int nibble = 0;

	for (end = walk->next; end && walk->offset_group == end->offset_group
	  && end->offset == ++next_offset; end = end->next)
	    length++;
	fprintf(file," %d:%d:%d=0x",walk->offset_group,walk->offset,length);
	for (scan = walk; length; scan = scan->next) {
	    length--;
	    nibble |= scan->value << (length & 3);
	    if (!(length & 3)) {
		fputc(nibble >= 10 ? nibble+'A'-10 : nibble+'0',file);
		nibble = 0;
	    }
	}
    }
}


static void drop_bits(void)
{
    struct bit_match *next;

    while (bits) {
	next = bits->next;
	free(bits);
	bits = next;
    }
    next_bit = &bits;
}


/*
 * bit_match returns 1 if the bit was added (or if it redundant), 0 if the test
 * would cause a conflict, so we should discard the rule.
 */


static int bit_match(int offset_group,int offset,int value)
{
    struct bit_match *bit;

    for (bit = bits; bit; bit = bit->next)
	if (bit->offset_group == offset_group && bit->offset == offset)
	    return bit->value == value; /* bit is redundant or conflicting */
    bit = alloc_t(struct bit_match);
    bit->offset_group = offset_group;
    bit->offset = offset;
    bit->value = value;
    bit->next = NULL;
    *next_bit = bit;
    next_bit = &bit->next;
    return 1;
}


/*
 * Returns 0 if the rule always fails.
 */


static int do_decompose_match(int offset_group,int offset,U128 mask128,
  U128 value128)
{
    int i;

    for (i = 3; i >= 0; i--) {
	uint32_t mask = mask128.v[i],value = value128.v[i];
	int off;

	debugf2("| [%d] mask 0x%lx, value 0x%lx",i,(unsigned long) mask,
	  (unsigned long) value);
	for (off = offset+(3-i)*32; mask; off++) {
	    if (mask & 0x80000000)
		if (!bit_match(offset_group,off,value >> 31)) return 0;
	    mask <<= 1;
	    value <<= 1;
	}
    }
    return 1;
}


static int decompose_match(int offset_group,int offset,int length,
  U128 mask,U128 value)
{
    debugf("decompose_match(off_grp %d,offset %d, length %d)",offset_group,
      offset,length);
    /* if (value & ~mask) return 0; */
    if (!u128_is_zero(u128_and(value,u128_not(mask)))) return 0;
    return do_decompose_match(offset_group,offset,
      u128_shift_left(mask,128-length),u128_shift_left(value,128-length));
}


static int dump_match(DATA d,int offset_group)
{
    DATA a,b;
    int bits;
    U128 mask = u128_not(u128_from_32(0));

    assert(d.op && d.op->dsc == &op_eq);
    /* @@@ policing */
    a = d.op->a;
    b = d.op->b;
    if (!a.op) {
	if (!b.op) dump_failed(d,"constant expression");
	a = b;
	b = d.op->a;
    }
    if (b.type != dt_unum && b.type != dt_ipv6) dump_failed(d,"eq non-number");
    while (a.op && a.op->dsc != &op_access) {
	if (a.op->dsc == &op_and) {
	    /* iflib_arith normalizes  const & access  to  access & const  */
	    if (a.op->b.op ||
	      (a.op->b.type != dt_unum && a.op->b.type != dt_ipv6))
		dump_failed(d,"must be \"& constant\"");
	    mask = u128_and(mask,data_convert(a.op->b,dt_ipv6).u.u128);
	    a = a.op->a;
	    continue;
	}
	if (a.op->dsc == &op_field_root) {
	    offset_group = a.op->b.u.field_root->offset_group;
	    a = a.op->a;
	    continue;
	}
	if (a.op->dsc == &op_offset) {
	    offset_group = get_offset_group(a,offset_group);
	    a = a.op->a;
	    continue;
	}
	dump_failed(d,"bad operator");
    }
    if (!a.op || a.op->dsc != &op_access)
	dump_failed(d,"access expected");
    bits = a.op->c.u.unum;
    assert(bits);
    /*
     * No longer needed - iflib_arith does this for us now
    mask &= (((1 << (bits-1))-1) << 1) | 1; / * (1 << bits)-1 * /
     */
    return decompose_match(offset_group,a.op->b.u.unum*8,bits,mask,
      data_convert(b,dt_ipv6).u.u128);
}


/* ------------------------------ Rules group ------------------------------ */


static void dump_action(FILE *file,const DATA *d)
{
    fprintf(file,"match");
    sort_bits();
    collect_bits(file);
    drop_bits();
    fprintf(file," action %d\n",action_number(*d));
}


static int dump_or(FILE *file,const DATA *d,int offset_group);


static int dump_other(FILE *file,const DATA *d,int offset_group)
{
    if (debug > 1) debugf("dump_other(offset_group %d)",offset_group);
    if (d->op && d->op->dsc == &op_eq) return dump_match(*d,offset_group);
    if (action_tree(*d)) {
	dump_action(file,d);
	return 1;
    }
    if (d->op && d->op->dsc == &op_field_root)
	return dump_or(file,&d->op->a,d->op->b.u.field_root->offset_group);
    if (d->op && d->op->dsc == &op_offset)
	return dump_or(file,&d->op->a,get_offset_group(*d,offset_group));
    dump_failed(*d,"bad operator");
    return 0; /* not reached */
}


static int dump_and(FILE *file,const DATA *d,int offset_group)
{
    if (debug > 1) debugf("dump_and(offset_group %d)",offset_group);
    while (d->op && d->op->dsc == &op_logical_and && !action_tree(*d)) {
	if (!dump_other(file,&d->op->a,offset_group)) {
	    drop_bits();
	    return 0;
	}
	d = &d->op->b;
    }
    return dump_other(file,d,offset_group);
}


static int dump_or(FILE *file,const DATA *d,int offset_group)
{
    int useful = 1;

    if (debug > 1) debugf("dump_or(offset_group %d)",offset_group);
    while (d->op && d->op->dsc == &op_logical_or && !action_tree(*d)) {
	if (dump_and(file,&d->op->a,offset_group)) useful = 1;
	d = &d->op->b;
    }
    return dump_and(file,d,offset_group) || useful;
}


static void dump_rules(FILE *file,const DATA *d)
{
    if (debug) printf("--- dump_rules ---\n");
    (void) dump_or(file,d,0);
}


/* ----------------------------- Global pragma ----------------------------- */


void dump_pragma(FILE *file)
{
    const DATA_LIST *walk;

    if (!pragma) return;
    fprintf(file,"pragma");
    for (walk = pragma; walk; walk = walk->next)
	fprintf(file," %s",walk->ref->u.string);
    fputc('\n',file);
}


/* ----------------------------- Queues group ------------------------------ */


void dump_block(FILE *file,const QDISC *q)
{
    const DEVICE *dev = q->parent.device;

    fprintf(file,"block %s %s",dev->name,dev->ingress == q ? "ingress" :
      "egress");
    if (prm_present(dev->params,&prm_pragma)) {
	const DATA_LIST *pragma;

	for (pragma = prm_data(dev->params,&prm_pragma).u.list;
	  pragma; pragma = pragma->next)
	    fprintf(file," %s",pragma->ref->u.string);
    }
    fputc('\n',file);
}


/* ----------------------------- Buckets group ----------------------------- */


static struct bucket {
    const POLICE *p;
    struct bucket *next;
} *buckets = NULL;


static void add_bucket(const POLICE *p)
{
    struct bucket **last;

    param_get(p->params,p->location);
    if (!prm_rate.present && prm_peakrate.present)
	error("if_ext only supports single-rate token bucket policer");
    for (last = &buckets; *last; last = &(*last)->next)
	if ((*last)->p == p) return;
    *last = alloc_t(struct bucket);
    (*last)->p = p;
    (*last)->next = NULL;
}


static void collect_buckets(const DATA *d)
{
    if (!d->op && (d->type == dt_police || d->type == dt_bucket)) { /* @@@ */
	const POLICE *p = d->u.police;

	param_get(p->params,p->location);
	if (prm_present(p->params,&prm_overflow))
	    add_bucket(prm_data(p->params,&prm_overflow).u.police);
	add_bucket(d->u.police);
	return;
    }
    if (d->op) {
	collect_buckets(&d->op->a);
	collect_buckets(&d->op->b);
	collect_buckets(&d->op->c);
    }
}


static void dump_buckets(FILE *file)
{
    struct bucket *bucket;

    for (bucket = buckets; bucket; bucket = bucket->next) {
	int overflow = 0;

	param_get(bucket->p->params,bucket->p->location);
	if (prm_present(bucket->p->params,&prm_overflow))
	    overflow =
	      prm_data(bucket->p->params,&prm_overflow).u.police->number;
	fprintf(file,"bucket %d = %lu %lu %lu %lu %d",
	  (int) bucket->p->number,(unsigned long) prm_rate.v,
	  prm_mpu.present ? (unsigned long) prm_mpu.v : 0,
	  (unsigned long) prm_burst.v,(unsigned long) prm_burst.v,
	  overflow);
	if (prm_present(bucket->p->params,&prm_pragma)) {
	    const DATA_LIST *pragma;

	    for (pragma = prm_data(bucket->p->params,&prm_pragma).u.list;
	      pragma; pragma = pragma->next)
		fprintf(file," %s",pragma->ref->u.string);
	}
	fputc('\n',file);
    }
    while (buckets) {
	struct bucket *next;

	next = buckets->next;
	free(buckets);
	buckets = next;
    }
}


/* ------------------------------------------------------------------------- */


static void dump_if_ext_expr(const FILTER *filter,DATA *d)
{
    /*
     * Normalize again, possibly undoing previous optimizations.
     * External program will do its own expression optimization.
     */
    iflib_arith(d);
    iflib_not(d);
    if (!use_bit_tree) {
	iflib_reduce(d);
	iflib_normalize(d);
	iflib_actions(filter->location,d);
    }
    iflib_offset(d,0); /* don't combine */
    debug_expr("BEFORE dumping",*d);
    scan_offsets(*d);
    collect_buckets(d);
}


static void dump_if_ext_prep_callback(const ELEMENT *e,void *user)
{
    DATA *d = prm_data_ptr(e->params,&prm_if_expr);

    complement_decisions(d,e->parent.class);
    *d = op_binary(&op_logical_or,*d,data_decision(dr_continue,NULL));
    dump_if_ext_expr(e->parent.filter,d);
}


void dump_if_ext_prepare(const FILTER *filter)
{
    QDISC *qdisc = filter->parent.qdisc;

    if (prm_present(filter->params,&prm_pragma))
	lwarn(filter->location,
	  "\"pragma\" on \"if\" ignored by external target");
    if (!no_combine) {
	DATA *d = &qdisc->if_expr;

	qdisc->if_expr = iflib_combine(qdisc);
	if (!generate_default_class)
	    *d = op_binary(&op_logical_or,*d,data_decision(dr_continue,NULL));
	else {
	    if (!qdisc->dsc->default_class)
		lerrorf(qdisc->location,
		  "qdisc \"%s\" does not provide default class",
		  qdisc->dsc->name);
	    qdisc->dsc->default_class(d,qdisc);
	}
	dump_if_ext_expr(filter,d);
    }
    else {
	iflib_comb_iterate(qdisc,dump_if_ext_prep_callback,NULL);
	if (generate_default_class) {
	    DATA tmp;

	    if (!qdisc->dsc->default_class)
		lerrorf(qdisc->location,
		  "qdisc \"%s\" does not provide default class",
		  qdisc->dsc->name);
	    qdisc->if_expr = data_unum(0);
	    qdisc->dsc->default_class(&qdisc->if_expr,qdisc);
	    assert(qdisc->if_expr.op &&
	      qdisc->if_expr.op->dsc == &op_logical_or);
	    tmp = qdisc->if_expr.op->b;
	    data_destroy_1(qdisc->if_expr.op->a);
	    data_destroy_1(qdisc->if_expr);
	    qdisc->if_expr = tmp;
	}
    }
}


void dump_if_ext_global(FILE *file)
{
    dump_offsets(file); /* @@@ may dump unused offsets */
    dump_buckets(file); /* @@@ may dump unused buckets */
}


static void dump_if_ext_dump_actions_callback(const ELEMENT *e,void *user)
{
    DATA d = prm_data(e->params,&prm_if_expr);

    if (debug > 1) debug_expr("dump_if_ext_dump_actions_callback",d);
    dump_actions(d);
}


static int need_barrier;


static void dump_if_ext_dump_rules_callback(const ELEMENT *e,void *user)
{
    FILE *file = user;

    if (need_barrier) fprintf(file,"barrier\n");
    dump_rules(file,prm_data_ptr(e->params,&prm_if_expr));
    need_barrier = 1;
}


void dump_if_ext_local(const FILTER *filter,FILE *file)
{
    QDISC *qdisc = filter->parent.qdisc;
    DATA d = qdisc->if_expr;

    if (!use_bit_tree) {
	if (!no_combine) {
	    dump_actions(d);
	    write_actions(file,filter->parent.qdisc);
	    dump_rules(file,&d);
	}
	else {
	    iflib_comb_iterate(qdisc,dump_if_ext_dump_actions_callback,NULL);
	    if (generate_default_class) dump_actions(d);
	    write_actions(file,filter->parent.qdisc);
	    need_barrier = 0;
	    iflib_comb_iterate(qdisc,dump_if_ext_dump_rules_callback,file);
	    if (generate_default_class) {
		if (need_barrier) fprintf(file,"barrier\n");
		dump_rules(file,&d);
	    }
	}
	free_actions();
	return;
    }
    switch (alg_mode) {
	case 0:
	    iflib_bit(file,filter->parent.qdisc,d);
	    break;
	case 1:
	    iflib_newbit(file,d);
	    break;
	case 2:
	    iflib_fastbit(file,d);
	    break;
	default:
	    errorf("unknown bit tree algorithm mode %d",alg_mode);
    }
}


static void dump_callback(FILE *file,void *user)
{
    const FILTER *filter = user;

    dump_pragma(file);
    dump_block(file,filter->parent.qdisc);
    dump_if_ext_prepare(filter);
    dump_if_ext_global(file);
    dump_if_ext_local(filter,file);
    reset_offsets();
}


void dump_if_ext(const FILTER *filter,const char *target)
{
    ext_build(target,NULL,filter,dump_callback,(void *) filter);
}
