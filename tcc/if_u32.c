/*
 * if_u32.c - Processing of the "if" command, using the "u32" classifier
 *
 * Written 2001-2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Network Robots, Bivio Networks, Werner Almesberger
 * Copyright 2003,2004 Werner Almesberger
 */


#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <u128.h>

#include "config.h"
#include "tcdefs.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "field.h"
#include "qdisc.h"
#include "police.h"
#include "tc.h"
#include "iflib.h"
#include "if.h"

#include "tccmeta.h"


static int prio_offset; /* protocol pass */
static int is_meta; /* 0: undecided; -1: no; 1: yes */
static int last_meta; /* value of is_meta for previous match */


/* ----- List of protocols ------------------------------------------------- */


static struct protocol {
    uint16_t id;
    struct protocol *next;
} *protocols = NULL,*protocol;


static void add_protocol(uint16_t protocol)
{
    struct protocol *proto;

    for (proto = protocols; proto; proto = proto->next)
	if (proto->id == protocol) return;
    proto = alloc_t(struct protocol);
    proto->id = protocol;
    proto->next = protocols;
    protocols = proto;
}


/*
 * do_collect_protocols not only collects all the protocols we're checking,
 * but it also sanity-checks all other accesses to meta fields.
 */


static void do_collect_protocols(const FILTER *filter,DATA d,int field_root)
{
    if (!d.op) return;
    if (d.op->dsc == &op_field_root) {
	do_collect_protocols(filter,d.op->a,
	  d.op->b.u.field_root->offset_group);
    }
    else if (d.op->dsc == &op_eq) {
	DATA access;
	int size;

	if (field_root != META_FIELD_ROOT) return;
	if (!d.op->a.op || d.op->a.op->dsc != &op_and)
	    lerror(filter->location,"internal: == & expected");
	if (d.op->b.op || d.op->b.type != dt_unum)
	    lerror(filter->location,
	      "constant expected in meta-field comparison");
	access = d.op->a.op->a;
	if (!access.op || access.op->dsc != &op_access)
	    lerror(filter->location,"internal: == ... access expected");
	if (access.op->b.op || access.op->b.type != dt_unum)
	    lerror(filter->location,"invalid offset for meta-field");
	switch (access.op->b.u.unum) {
	    case META_PROTOCOL_OFFSET:
		size = META_PROTOCOL_SIZE;
		add_protocol(d.op->b.u.unum);
		break;
	    case META_NFMARK_OFFSET:
		size = META_NFMARK_SIZE;
		break;
	    case META_TC_INDEX_OFFSET:
		size = META_TC_INDEX_SIZE;
		break;
	    default:
		lerror(filter->location,"invalid offset for meta-field");
	}
	if (d.op->a.op->b.op || d.op->a.op->b.type != dt_unum ||
	  (size < 4 && d.op->a.op->b.u.unum != (1 << (size*8))-1))
	    lerror(filter->location,"invalid size for meta-field mask");
	if (access.op->c.op || access.op->c.type != dt_unum ||
	  access.op->c.u.unum != size*8)
	    lerror(filter->location,"invalid size for meta-field access");
    }
    else {
	do_collect_protocols(filter,d.op->a,field_root);
	do_collect_protocols(filter,d.op->b,field_root);
	do_collect_protocols(filter,d.op->c,field_root);
    }
}


static void collect_protocols(const FILTER *filter,DATA d)
{
    do_collect_protocols(filter,d,0);
}


static void free_protocols(void)
{
    while (protocols) {
	struct protocol *next =  protocols->next;

	free(protocols);
	protocols = next;
    }
}


/* ----- Handles ----------------------------------------------------------- */


static uint32_t handle;


static uint32_t new_handle(void)
{
    handle += 0x100000;
    return handle;
}


static void dump_handle(const char *tag,uint32_t handle)
{
    tc_more(" %s %x:%x:%x",tag,TC_U32_USERHTID(handle),TC_U32_HASH(handle),
      TC_U32_KEY(handle));
}


/* ----- Hashes ------------------------------------------------------------ */


/*
 * Cache next hash and dump it only if necessary.
 */


static int cached = 0;
static uint16_t cached_protocol;
static uint32_t cached_handle;


static void sync_meta(void)
{
    if (!is_meta) is_meta = -1;
    if (last_meta && last_meta != is_meta) prio_offset++;
    last_meta = is_meta;
}


static void cached_hash(const FILTER *filter)
{
    if (is_meta == 1)
	lerror(filter->location,"invalid combination of meta and non-meta "
          "fields");
    if (!cached) return;
    sync_meta();
    cached = 0;
    __tc_filter_add2(filter,cached_protocol,prio_offset);
    dump_handle("handle",cached_handle);
    tc_more(" u32 divisor 1");
    tc_nl();
}


static void next_hash(const FILTER *filter)
{
    cached_protocol = protocol ? protocol->id : ETH_P_ALL;
    cached_handle = handle+0x100000;
    cached = 1;
}


/* ----- Dump matches ------------------------------------------------------ */


static int keys; /* number of stored keys */
static uint32_t curr_handle; /* handle of current filter element */
static uint32_t meta_offset,meta_value;

static struct {
    int bits;
    int orig_offset;
    int offset;
    uint32_t value,mask;
} key[MAX_U32_KEYS];

/*
 * The rule stack remembers all decisions we took to get to the present rule.
 * If the rule needs to be issued repeatedly, we repeat the whole sequence from
 * our stack.
 */

typedef struct _match {
    int bits;
    uint32_t value;
    uint32_t mask;
    int offset;
    struct _match *next;
} MATCH;


typedef struct _rule {
    uint32_t handle;
    MATCH *matches;
    uint32_t plus; /* 0 if not using fixed increment */
    int offset;
    uint32_t mask; /* 0 if not using variable increment */
    int shift;
    uint32_t link; /* 0 if none */
    struct _rule *prev;
} RULE;

static RULE *rule_stack = NULL;



static void begin_match(uint32_t handle)
{
    curr_handle = handle;
    keys = 0;
    is_meta = 0;
}


static void dump_meta(const FILTER *filter)
{
    switch (meta_offset) {
	case META_NFMARK_OFFSET:
	    tc_more(" handle %lu fw",(unsigned long) meta_value);
	    break;
	case META_TC_INDEX_OFFSET:
	    tc_more(" handle %lu tcindex",(unsigned long) meta_value);
	    break;
	default:
	    abort();
    }
}


static int set_meta(const FILTER *filter,uint32_t offset,int size,
  uint32_t value)
{
    if (is_meta == -1 || rule_stack)
	lerror(filter->location,"invalid combination of meta and non-meta "
	  "fields");
    if (is_meta) {
	if (meta_offset != offset)
	    lerror(filter->location,"invalid combination of meta fields");
	return value == meta_value;
    }
    is_meta = 1;
    switch (offset) {
	case META_NFMARK_OFFSET:
	    if (!value)
		lerror(filter->location,"cannot test meta_nfmark == 0");
	    break;
	default:
	    break;
    }
    meta_offset = offset;
    meta_value = value;
    return 1;
}


/*
 * Ugly but true: we must issue new handles, because u32 cleverly combines all
 * expressions into a single classifier per qdisc, no matter what we say in the
 * filter's "prio" ...
 */

static void dump_rule(const FILTER *filter,const RULE *rule,uint32_t my_handle)
{
    const MATCH *match;

    __tc_filter_add2(filter,protocol ? protocol->id : ETH_P_ALL,prio_offset);
    if (TC_U32_HTID(rule->handle) == TC_U32_ROOT) tc_more(" u32");
    else {
	dump_handle("handle",my_handle);
	tc_more(" u32");
	dump_handle("ht",TC_U32_HTID(my_handle));
    }
    for (match = rule->matches; match; match = match->next)
	tc_more(" match u%d 0x%lx 0x%lx at %d",match->bits,match->value,
	  match->mask,match->offset);
    if (rule->plus) tc_more(" offset plus %d",rule->plus);
    if (rule->mask)
	tc_more(" offset at %d mask %04x shift %d eat",rule->offset,rule->mask,
	  rule->shift);
    if (rule->link) {
	dump_handle("link",TC_U32_HTID(handle));
	tc_nl();
    }
}


static void dump_stack(const FILTER *filter,const RULE *rule)
{
    uint32_t my_handle = rule->handle;

    if (rule->prev) {
	dump_stack(filter,rule->prev);
	my_handle = TC_U32_HTID(handle) | TC_U32_KEY(my_handle);
    }
    if (rule->link) {
	new_handle();
	__tc_filter_add2(filter,cached_protocol,prio_offset);
	dump_handle("handle",handle);
	tc_more(" u32 divisor 1");
	tc_nl();
    }
    dump_rule(filter,rule,my_handle);
}


static void pop_rule(void)
{
     RULE *prev_rule;

     assert(rule_stack);
     prev_rule = rule_stack->prev;
     while (rule_stack->matches) {
	MATCH *next_match = rule_stack->matches->next;

	free(rule_stack->matches);
	rule_stack->matches = next_match;
     }
     free(rule_stack);
     rule_stack = prev_rule;
}


static void dump_match_again(const FILTER *filter)
{
    if (is_meta == 1) {
	__tc_filter_add2(filter,protocol ? protocol->id : ETH_P_ALL,
	  prio_offset);
	dump_meta(filter);
	return;
    }
    assert(rule_stack);
    dump_stack(filter,rule_stack);
}


static void dump_match(const FILTER *filter)
{
    RULE *rule;
    MATCH **next;
    int i;

    sync_meta();
    if (is_meta == 1) {
	__tc_filter_add2(filter,protocol ? protocol->id : ETH_P_ALL,
	  prio_offset);
	dump_meta(filter);
	return;
    }
    rule = alloc_t(RULE);
    rule->handle = curr_handle;
    rule->plus = 0;
    rule->mask = 0;
    rule->link = 0;
    rule->matches = NULL;
    rule->prev = rule_stack;
    rule_stack = rule;
    next = &rule->matches;
    if (!keys) {
	*next = alloc_t(MATCH);
	(*next)->bits = 32;
	(*next)->value = 0;
	(*next)->mask = 0;
	(*next)->offset = 0;
	(*next)->next = NULL;
    }
    else {
	for (i = 0; i < keys; i++) {
	    int shift;

	    *next = alloc_t(MATCH);
	    shift = 32-key[i].bits-(key[i].orig_offset-key[i].offset)*8;
	    (*next)->bits = key[i].bits;
	    (*next)->value = key[i].value >> shift;
	    (*next)->mask = key[i].mask >> shift;
	    (*next)->offset = key[i].orig_offset;
	    (*next)->next = NULL;
	    next = &(*next)->next;
	}
    }
    dump_rule(filter,rule,curr_handle);
}


/*
 * Return 0 if key contradicts previous keys.
 */


static int add_key(int bits,uint32_t value,uint32_t mask,int offset)
{
    int i;

    debugf2("| bits %d, value 0x%x, mask 0x%x, offset %d",bits,value,mask,
      offset);
    if (keys == MAX_U32_KEYS)
	errorf("too many u32 keys (max %d)",MAX_U32_KEYS);
    key[keys].orig_offset = offset;
    switch (bits) {
	case 8:
	    if (offset & 1) {
		offset--;
		value >>= 8;
		mask >>= 8;
	    }
	    /* fall through */
	case 16:
	    if (offset & 2) {
		offset -= 2;
		value >>= 16;
		mask >>= 16;
	    }
	    /* fall through */
	case 32:
	    break;
    }
    key[keys].bits = bits;
    key[keys].value = value;
    key[keys].mask = mask;
    key[keys].offset = offset;
    for (i = 0; i < keys; i++)
	if (key[i].offset == offset) {
	    if ((key[i].value^value) & key[i].mask & mask) return 0;
	    if (key[i].bits == bits &&
	      key[i].orig_offset == key[keys].orig_offset) {
		key[i].value |= key[keys].value;
		key[i].mask |= key[keys].mask;
		return 1;
	    }
	}
    keys++;
    return 1;
}


static int dump_or(const FILTER *filter,DATA d,uint32_t curr_handle,int at_end,
  int field_root);
static int dump_and(const FILTER *filter,DATA d,uint32_t curr_handle,
  int at_end,int field_root);


static uint32_t __dump_link(const FILTER *filter)
{
    uint32_t next = new_handle();

    rule_stack->link = next;
    dump_handle("link",TC_U32_HTID(next));
    tc_nl();
    next_hash(filter);
    return next;
}


static void dump_link(const FILTER *filter,DATA d,int at_end,int field_root)
{
    uint32_t next;

    next = __dump_link(filter);
    dump_or(filter,d,next,at_end,field_root);
    pop_rule();
}


static void dump_offset(const FILTER *filter,DATA d,int at_end,int field_root)
{
    DATA off;

    cached_hash(filter);
    dump_match(filter);
    off = d.op->b;
    if (!off.op) {
	if (off.type != dt_unum) dump_failed(off,"non-numeric offset");
	if (off.u.unum & 3)
	    errorf("offset must be a multiple of 4 (%ld)",
	      (unsigned long) off.u.unum);
	rule_stack->plus = off.u.unum;
	tc_more(" offset plus %d",off.u.unum);
    }
    else {
	uint32_t mask = 0xffff;
	int offset,shift,bytes;

	shift = 0;
	while (off.op->dsc == &op_shift_left ||
	  off.op->dsc == &op_shift_right) {
	    if (off.op->b.op || off.op->b.type != dt_unum)
		dump_failed(off,"bad shift");
	    shift = off.op->b.u.unum;
	    if (off.op->dsc == &op_shift_right) shift = -shift;
	    off = off.op->a;
	}
	if (off.op && off.op->dsc == &op_and) {
	    if (!off.op->a.op && off.op->a.type == dt_unum) {
		mask = off.op->a.u.unum;
		off = off.op->b;
	    }
	    else {
		if (off.op->b.op || off.op->b.type != dt_unum)
		    dump_failed(off,"non-constant and");
		mask = off.op->b.u.unum;
		off = off.op->a;
	    }
	    if (mask > 0xffff)
		errorf("offset mask must be < 0x10000 (not 0x%lx)",mask);
	}
	if (!off.op || off.op->dsc != &op_access)
	    dump_failed(off,"access expected");
	if (off.op->b.op || off.op->b.type != dt_unum)
	    dump_failed(off,"bad offset");
	offset = off.op->b.u.unum;
	bytes = off.op->c.u.unum/8;
	if (bytes == 1) {
	    shift -= 8;
	    mask = (mask & 0xff) << 8;
	}
	if (offset & 1) {
	    offset -= 1;
	    shift += 8;
	    if (mask & 0xff) errorf("offset adjustment removes non-zero bits");
	    mask >>= 8;
	}
	if (shift > 0) errorf("u32 can't left-shift (%d)",shift);
	if (shift < -15) errorf("u32 can't right-shift by %d",-shift);
	if (mask & ~(0xffff << -shift))
	    warnf("some non-zero bits of mask 0x%lx are always zero in value",
	      (unsigned long) mask,-shift);
	/*
	 * Brief history of mask and value:
	 *   iproute2/tc: mask read in natural order from line
 	 *   iproute2/tc: htons(mask) -> offmask
	 *   cls_u32: offset = ntohs(offmask & *(u16 *) ...) ...
	 *
	 * So we get:
	 *   offset = ntohs(htons(mask) & *(u16 *) ...)
	 *	    = mask & ntohs(*(u16 *) ...)
	 */
	tc_more(" offset at %d mask %04x shift %d eat",offset,mask,-shift);
	assert(mask);
	rule_stack->offset = offset;
	rule_stack->mask = mask;
	rule_stack->shift = -shift;
    }
    dump_link(filter,d.op->a,at_end,field_root);
}


static int tc_match(int bits,U128 *value,U128 *mask,int *offset,
  int *bytes)
{
    int size = bits >> 3;
    int useful;

    debugf("tc_match(bits %d,offset %d,bytes %d)",bits,*offset,*bytes);
    if (*bytes < size || (*offset & (size-1))) return 1;
    useful = add_key(bits,value->v[3],mask->v[3],*offset);
    *value = u128_shift_left(*value,bits);
    *mask = u128_shift_left(*mask,bits);
    *bytes -= size;
    *offset += size;
    return useful;
}


static int dump_eq(DATA d,DATA constant,DATA var)
{
    U128 value,mask;
    int default_mask = 1;
    int offset,bytes,useful;

    if (constant.op || (constant.type != dt_unum && constant.type != dt_ipv6))
	dump_failed(d,"constant side isn't");
    if (!var.op) dump_failed(d,"variable side isn't");
    mask = u128_not(u128_from_32(0)); /* ~0 */
    while (var.op->dsc == &op_and) {
	if ((var.op->a.type == dt_unum || var.op->a.type == dt_ipv6) &&
	  !var.op->a.op) {
	    mask = u128_and(mask,data_convert(var.op->a,dt_ipv6).u.u128);
	      /* mask &= var.op->a */
	    var = var.op->b;
	}
	else {
	    if ((var.op->b.type != dt_unum && var.op->b.type != dt_ipv6) ||
	      var.op->b.op)
		dump_failed(d,"non-constant and");
	    mask = u128_and(mask,data_convert(var.op->b,dt_ipv6).u.u128);
	      /* mask &= var.op->b */
	    var = var.op->a;
	}
	default_mask = 0;
    }
    if (!var.op || var.op->dsc != &op_access) dump_failed(d,"access expected");
    if (var.op->b.op || var.op->b.type != dt_unum) dump_failed(d,"bad offset");
    offset = var.op->b.u.unum;
    bytes = var.op->c.u.unum/8;
    value = data_convert(constant,dt_ipv6).u.u128;
    if (default_mask) mask = u128_shift_right(mask,128-var.op->c.u.unum);
	  /* mask = ~0 >> ... */
    if (!u128_is_zero(u128_and(value,u128_not(mask)))) return 0;
	  /* if (value & ~mask) return 0; */
    value = u128_shift_left(value,128-var.op->c.u.unum);
    mask = u128_shift_left(mask,128-var.op->c.u.unum);
    useful = 1;
    while (bytes > 3 && !(offset & 3))
	if (!tc_match(32,&value,&mask,&offset,&bytes)) useful = 0;
    if (!tc_match(16,&value,&mask,&offset,&bytes)) useful = 0;
    while (bytes > 3 && !(offset & 3))
	if (!tc_match(32,&value,&mask,&offset,&bytes)) useful = 0;
    if (!tc_match(16,&value,&mask,&offset,&bytes)) useful = 0;
    if (!tc_match(8,&value,&mask,&offset,&bytes)) useful = 0;
    while (bytes > 3 && !(offset & 3))
	if (!tc_match(32,&value,&mask,&offset,&bytes)) useful = 0;
    if (!tc_match(16,&value,&mask,&offset,&bytes)) useful = 0;
    while (bytes > 3 && !(offset & 3))
	if (!tc_match(32,&value,&mask,&offset,&bytes)) useful = 0;
    if (!tc_match(8,&value,&mask,&offset,&bytes)) useful = 0;
    if (bytes) errorf("internal error: %d bytes left",bytes);
    return useful;
}


/* ----- Dump actions ------------------------------------------------------ */


static struct u32_policer {
    const POLICE *t_c;	/* commited rate */
    const POLICE *t_p;	/* peak rate */
    POLICE *pol;	/* combined policer */
    struct u32_policer *next;
} *u32_policers = NULL;


static struct u32_policer *make_policer(const POLICE *t_c,const POLICE *t_p,
  int clone)
{
    struct u32_policer *pol;

    if (!clone)
	for (pol = u32_policers; pol; pol = pol->next) {
	    if (pol->t_c == t_c && pol->t_p == t_p) return pol;
	    if (pol->t_c == t_c || pol->t_p == t_c)
		lerror(t_c->location,
		  "policer(s) already used in different configuration");
	    if (t_p && (pol->t_c == t_p || pol->t_p == t_p))
		lerror(t_p->location,
		  "policer(s) already used in different configuration");
	}
    pol = alloc_t(struct u32_policer);
    if (clone) pol->t_c = pol->t_p = NULL;
    else {
	pol->t_c = t_c;
	pol->t_p = t_p;
    }
    pol->next = u32_policers;
    u32_policers = pol;
    pol->pol = alloc_t(POLICE);
    pol->pol->number = UNDEF_U32;
    pol->pol->params = NULL;
    param_add(&pol->pol->params,
     param_make(&prm_burst,data_typed_fnum(prm_burst.v,dt_size)));
    param_add(&pol->pol->params,
      param_make(&prm_rate,data_typed_fnum(prm_rate.v*8.0,dt_rate)));
    if (prm_peakrate.present) {
	param_add(&pol->pol->params,
	  param_make(&prm_mtu,data_typed_fnum(prm_mtu.v,dt_size)));
	param_add(&pol->pol->params,
	  param_make(&prm_peakrate,
	  data_typed_fnum(prm_peakrate.v*8.0,dt_rate)));
    }
    if (prm_mpu.present)
	param_add(&pol->pol->params,
	  param_make(&prm_mpu,data_typed_fnum(prm_mpu.v,dt_size)));
    pol->pol->location = t_c->location;
    add_police(pol->pol);
    assign_policer_ids(pol->pol);
    pol->pol->used = 1;
    pol->pol->in_profile = pol->pol->out_profile = pd_invalid;
    return pol;
}


/*
 * Input:
 * n#  = conform (bucket)
 * N#  = conform (bucket or full policer)
 * t#  = count (bucket)
 * t#  = count (bucket or full policer)
 * d   = drop
 * c#  = class
 * u   = unspec
 * *   = end of classifier
 * n#>#, N#># = take rule only if first bucket overflows to second bucket
 *       (rules without ">" are not taken if the first bucket overflows)
 *
 * Output:
 * p#  = policer
 * P## = compose policer
 *   p'...,P'... = clone policer (i.e. suppress usage check)
 *   p#+#,P#+## = add burst of 2nd policer to 1st policer
 * d   = drop
 * D   = drop policer
 * c#  = class
 * u   = unspec
 *     = ignored (for visual separation of phases)
 */

static struct map_rule {
    const char *in;
    const char *out;
} map[] = {
    /* no meter */
    { "c0",			"c0" },
    { "d",			"Ddd" },
    { "u",			"" }, /* discard rule */
    /* SLB */
    { "N0T0c1u*",		"p0c1u" },
    { "N0T0c1d",		"p0c1d" },
    { "N0T0du*",		"p0du" },
    { "N0T0dd",			"p0dd" },
    { "N0T0dc1",		"p0dc1" },
    { "N0T0c1c2",		"p0c1u c2" },
    /* DLB */
    { "n0n1t0t1c2uu*",		"P01c2u" },
    { "n0n1t0t1c2dd",		"P01c2d" },
    { "n0n1t0t1duu*",		"P01du" },
    { "n0n1t0t1dc2c2",		"P01dc2" },
    { "n0n1t0t1c2c3c3",		"P01c2u c3" },
    /* DLB with reversed counting */
    { "n0n1t1t0c2uu*",		"P01c2u" },
    { "n0n1t1t0c2dd",		"P01c2d" },
    { "n0n1t1t0duu*",		"P01du" },
    { "n0n1t1t0dc2c2",		"P01dc2" },
    { "n0n1t1t0c2c3c3",		"P01c2u c3" },
    /* srTCM, 3 classes */
    { "n0>1t0c2n1t1c3c4",	"p0+1uc4 p'0c2u c3" },
    /* srTCM, 2 classes, drop in "red" */
    { "n0>1t0c2n1t1c3d",	"p0+1ud p'0c2u c3" },
    /* trTCM, 3 classes */
    { "n0n1t0t1c2c3n1t1c4c3",	"p1uc3 p0c2u c4" }, /* __GYR GYR GRY YGR YRG */
    { "n0n1t1t0c2t0c3c4",	"p0uc4 p1c2u c3" }, /* RGY RYG */
    /* trTCM, 2 classes, drop on "red" */
    { "n0n1t0t1c2dn1t1c3d",	"p1ud p0c2u c3" },  /* __GYR GYR GRY YGR YRG */
    { "n0n1t1t0c2t0c3d",	"p0ud p1c2u c3" },  /* RGY RYG */
    { NULL, }
};


static const void *map_mem[10];


static int store_value(const char **pos,const void *val)
{
    unsigned n = *(*pos)++-'0';

    assert(n < 10);
    assert(val);
    if (map_mem[n] && map_mem[n] != val) return 0;
    map_mem[n] = val;
    return 1;
}


static const void *get_value(const char **pos)
{
    unsigned n = *(*pos)++-'0';

    assert(n < 10);
    assert(map_mem[n]);
    return map_mem[n];
}


static int walk_tree(const struct action *act,const char **pos)
{
    switch (*(*pos)++) {
	case 'n':
	    if (act->type != at_conform) return 0;
	    if (prm_present(act->u.conform->params,&prm_peakrate)) return 0;
	    /* fall through */
	case 'N':
	    if (act->type != at_conform) return 0;
	    if (!store_value(pos,act->u.conform)) return 0;
	    if (prm_present(act->u.conform->params,&prm_overflow)) {
		if (**pos != '>') return 0;
		(*pos)++;
		if (!store_value(pos,
		  prm_data(act->u.conform->params,&prm_overflow).u.police))
		    return 0;
	    }
	    else {
		if (**pos == '>') return 0;
	    }
	    if (!walk_tree(act->c[1],pos)) return 0;
	    if (!walk_tree(act->c[0],pos)) return 0;
	    return 1;
	case 't':
	    if (act->type != at_count) return 0;
	    if (prm_present(act->u.count->params,&prm_peakrate)) return 0;
	    /* fall through */
	case 'T':
	    if (act->type != at_count) return 0;
	    if (!store_value(pos,act->u.count)) return 0;
	    if (!walk_tree(act->c[1],pos)) return 0;
	    return 1;
	case 'd':
	    if (act->type != at_drop) return 0;
	    return 1;
	case 'c':
	    if (act->type != at_class) return 0;
	    if (!store_value(pos,act->u.decision)) return 0;
	    return 1;
	case 'u':
	    if (act->type != at_unspec) return 0;
	    return 1;
	default:
	    abort();
    }
}


static const char *map_actions(const struct action *act,int at_end)
{
    struct map_rule *entry;

    for (entry = map; entry->in; entry++) {
	const char *pos;
	int i;

	for (i = 0; i < 10; i++) map_mem[i] = NULL;
	pos = entry->in;
	if (!walk_tree(act,&pos)) continue;
	if (*pos == '*') {
	    if (!at_end) continue;
	    pos++;
	}
	if (!*pos) return entry->out;
    }
    return NULL;
}


static void print_value(FILE *file,const void *val)
{
    const void **mem;

    for (mem = map_mem; *mem; mem++)
	if (*mem == val) break;
    *mem = val;
    fputc(mem-map_mem+'0',file);
}


static void print_action(FILE *file,const struct action *act)
{
    switch (act->type) {
	case at_conform:
	    fputc(prm_present(act->u.conform->params,&prm_peakrate) ? 'N' : 'n',
	      file);
	    print_value(file,act->u.conform);
	    if (prm_present(act->u.conform->params,&prm_overflow)) {
 		fputc('>',file);   
		print_value(file,
		  prm_data(act->u.conform->params,&prm_overflow).u.police);
	    }
	    print_action(file,act->c[1]);
	    print_action(file,act->c[0]);
	    return;
	case at_count:
	    fputc(prm_present(act->u.count->params,&prm_peakrate) ? 'T' : 't',
	      file);
	    print_value(file,act->u.count);
	    print_action(file,act->c[1]);
	    return;
	case at_class:
	    fputc('c',file);
	    print_value(file,act->u.decision);
	    return;
	case at_drop:
	    fputc('d',file);
	    return;
	case at_unspec:
	    fputc('u',file);
	    return;
	default:
	    abort();
    }
}


static void print_actions(FILE *file,const struct action *act,int at_end)
{
    int i;

    for (i = 0; i < 10; i++) map_mem[i] = NULL;
    print_action(file,act);
    if (at_end) fputc('*',file);
}


static const CLASS *issue_decision(const FILTER *filter,const char **out,
  DECISION *dec)
{
    switch (*(*out)++) {
	case 'c':
	    if (*dec != pd_invalid && *dec != pd_ok) goto error;
	    *dec = pd_ok;
	    return get_value(out);
	case 'd':
	    if (*dec != pd_invalid && *dec != pd_drop) goto error;
	    *dec = pd_drop;
	    return NULL;
	case 'u':
	    if (*dec != pd_invalid && *dec != pd_continue) goto error;
	    *dec = pd_continue;
	    return NULL;
	default:
	    abort();
    }

error:
    lerror(filter->location,"policer decision changed");
}


static void issue_policer(const FILTER *filter,POLICE *police,const char **out,
  int again)
{
    const CLASS *class,*tmp;

    if (again) dump_match_again(filter);
    else dump_match(filter);
    class = issue_decision(filter,out,&police->in_profile);
    tmp = issue_decision(filter,out,&police->out_profile);
    if (class) assert(!tmp);
    else if (tmp) class = tmp;
    if (!class) class = get_any_class(filter->parent.qdisc);
    tc_add_classid(class,1);
    dump_police(police);
}


static void issue_actions(const FILTER *filter,const char **out,int again)
{
    const POLICE *p0,*p1;
    uint32_t ebs,burst,rate,mpu;
    int clone = 0;

    debugf("issue_actions \"%s\"",*out);
    while (**out == ' ') (*out)++;
    switch (*(*out)++) {
	case 'p':
	    if (**out == '\'') {
		(*out)++;
		clone = 1;
	    }
	    p0 = get_value(out);
	    if (**out != '+') ebs = 0;
	    else {
		(*out)++;
		p1 = get_value(out);
		param_get(p1->params,p1->location);
		ebs = prm_burst.v;
	    }
	    param_get(p0->params,p0->location);
	    prm_burst.v += ebs;
	    issue_policer(filter,make_policer(p0,NULL,clone)->pol,out,again);
	    break;
	case 'P':
	    if (**out == '\'') {
		(*out)++;
		clone = 1;
	    }
	    p0 = get_value(out);
	    if (**out != '+') ebs = 0;
	    else {
		(*out)++;
		p1 = get_value(out);
		param_get(p1->params,p1->location);
		ebs = prm_burst.v;
	    }
	    p1 = get_value(out);
	    param_get(p1->params,p1->location);
	    burst = prm_burst.v;
	    rate = prm_rate.v;
	    mpu = prm_mpu.present ? prm_mpu.v : 0;
	    param_get(p0->params,p0->location);
	    prm_mtu.v = burst+ebs;
	    prm_mtu.present = 1;
	    prm_peakrate.v = rate;
	    prm_peakrate.present = 1;
	    if ((prm_mpu.present && prm_mpu.v != mpu) ||
	      (!prm_mpu.present && mpu))
		lerror(p0->location,
		  "buckets have incompatible MPU parameters");
	    issue_policer(filter,make_policer(p0,p1,clone)->pol,out,again);
	    break;
	case 'c':
	    if (again) dump_match_again(filter);
	    else dump_match(filter);
	    tc_add_classid(get_value(out),1);
	    break;
	case 'D':
	    issue_policer(filter,get_drop_policer(filter->location),out,again);
	    break;
	case 'u':
	    return; /* do nothing */
	case 0:
	    (*out)--; /* fix pointer */
	    return; /* do nothing */
	default:
	    abort();
    }
    tc_nl();
}


static int dump_action(const FILTER *filter,DATA d,int at_end)
{
    const struct action *act;
    const char *out;

    if (!action_tree(d)) return 0;
    act = get_action(d);
    out = map_actions(act,at_end);
    if (debug > 1 || (debug && !out)) {
	fprintf(stderr,"problem pattern: ");
	print_actions(stderr,act,at_end);
	fprintf(stderr,"\n");
    }
    if (!out) lerror(filter->location,"don't know how to build meter for this");
    issue_actions(filter,&out,0);
    while (*out) {
	if (is_meta == 1) prio_offset++;
	issue_actions(filter,&out,1);
    }
    if (is_meta == -1) pop_rule();
    return 1;
}


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


/* ----- Dump logical operators -------------------------------------------- */


static int dump_other(const FILTER *filter,DATA d,uint32_t curr_handle,
  int at_end,int field_root)
{
    if (d.op && d.op->dsc == &op_logical_and)
	error("internal error: && on left-hand side of &&");
    if (!d.op) {
	switch (d.type) {
	    case dt_unum:
		if (d.u.unum) return 1;
		return 0;
		/* earlier stages should have disappeard this ... */
	    case dt_decision:
		dump_failed(d,"unexpected decision");
	    default:
		break;
	}
    }
    else {
	 if (d.op->dsc == &op_logical_or) {
	    cached_hash(filter);
	    dump_match(filter);
	    dump_link(filter,d,at_end,field_root);
	    return 0;
	}
	if (d.op->dsc == &op_field_root) {
	    uint32_t offset = d.op->b.u.field_root->offset_group;

	    return dump_and(filter,d.op->a,curr_handle,at_end,offset);
#if 0
	    /* field roots just got simpler :-) */
	    dump_failed(d,"unexpected field root");
	    cached_hash(filter);
	    dump_match(filter);
	    dump_link(filter,d.op->a,at_end,offset);
	    return 0;
#if 0 /* premature optimization is the root of all bugs ... */
	    return dump_or(filter,d.op->a,curr_handle,at_end,
	      d.op->b.u.field_root->offset_group);
#endif
#endif
	}
	if (d.op->dsc == &op_offset) {
	    dump_offset(filter,d,at_end,field_root);
	    return 1;
	}
	if (d.op->dsc == &op_eq) {
	    if (field_root == META_FIELD_ROOT) {
		uint32_t offset = d.op->a.op->a.op->b.u.unum;

		switch (offset) {
		    case META_PROTOCOL_OFFSET:
			debugf2("meta field (curr %d, here %d)",
			  protocol ? protocol->id : 0,d.op->b.u.unum);
			return protocol && d.op->b.u.unum == protocol->id;
		    case META_NFMARK_OFFSET:
			return set_meta(filter,offset,META_NFMARK_SIZE,
			  d.op->b.u.unum);
		    case META_TC_INDEX_OFFSET:
			return set_meta(filter,offset,META_TC_INDEX_SIZE,
			  d.op->b.u.unum);
		    default:
			abort();
		}
	    }
	    if (field_root) error("u32 only supports field root 0");
	    if (is_meta == 1)
		lerror(filter->location,"invalid combination of meta and "
		  "non-meta fields");
	    is_meta = -1;
	    if ((d.op->a.type == dt_unum || d.op->a.type == dt_ipv6) &&
	      !d.op->a.op)
		return dump_eq(d,d.op->a,d.op->b);
	    if ((d.op->b.type == dt_unum || d.op->b.type == dt_ipv6) &&
	      !d.op->b.op)
		return dump_eq(d,d.op->b,d.op->a);
	}
    }
    dump_failed(d,"not recognized by dump_other");
    return 0; /* not reached */
}


static int dump_and(const FILTER *filter,DATA d,uint32_t curr_handle,
  int at_end,int field_root)
{
    /* @@@ check: 0-based key allowed ? also: check key overflow */
    if (d.op && d.op->dsc == &op_logical_or)
	error("internal error: || on left-hand side of ||");
    while (d.op && d.op->dsc == &op_logical_and) {
	if (d.op->a.op && d.op->a.op->dsc == &op_offset)
	    dump_failed(d,
	      "unsupported offset sequence - please try to reorder matches");
	if (dump_action(filter,d,at_end)) return 1;
	if (dump_action(filter,d.op->a,at_end)) return 1;
	if (!dump_other(filter,d.op->a,curr_handle,at_end,field_root)) return 0;
	d = d.op->b;
    }
    if (dump_action(filter,d,at_end)) return 1;
    return dump_other(filter,d,curr_handle,at_end,field_root);
}


static int dump_or(const FILTER *filter,DATA d,uint32_t curr_handle,int at_end,
  int field_root)
{
    int useful = 0;

    while (d.op && d.op->dsc == &op_logical_or) {
	curr_handle++;
	begin_match(curr_handle);
	if (dump_action(filter,d,at_end)) return 1;
	if (dump_and(filter,d.op->a,curr_handle,0,field_root)) useful = 1;
	d = d.op->b;
    }
    curr_handle++;
    begin_match(curr_handle);
    if (dump_action(filter,d,at_end)) return 1;
    return dump_and(filter,d,curr_handle,at_end,field_root) || useful;
}


/* ----- Determine if we need to add default classification ---------------- */


static int num_continues(DATA d)
{
    if (d.type == dt_decision) return d.u.decision.result == dr_continue;
    if (!d.op) return 0;
    return num_continues(d.op->a)+num_continues(d.op->b)+num_continues(d.op->c);
}


static int has_continue(DATA d)
{
    while (d.op && (d.op->dsc == &op_offset || d.op->dsc == &op_logical_and)) {
	if (d.op->dsc == &op_offset) d = d.op->a;
	else {
	    if (d.op->a.type == dt_decision)
		return d.op->a.u.decision.result == dr_continue;         
	    d = d.op->b;
	}
    }
    return d.type == dt_decision && d.u.decision.result == dr_continue;
}


static int continue_at_end(DATA d)
{
    int n,in_continue,this;

    n = num_continues(d);
    in_continue = 0;
    while (d.op && (d.op->dsc == &op_logical_or || d.op->dsc == &op_offset)) {
	if (d.op->dsc == &op_offset) d = d.op->a;
	else {
	    this = has_continue(d.op->a);
	    if (!this && in_continue) return 0;
	    in_continue = this;
	    if (this) n--;
	    d = d.op->b;
	}
    }
    this = has_continue(d);
    if (!this && in_continue) return 0;
    if (this) n--;
    return !n;
}


/* ------------------------------------------------------------------------- */


void dump_if_u32(const FILTER *filter)
{
    QDISC *qdisc = filter->parent.qdisc;
    DATA d;
    int offset_group = 0;
    int i;

    tc_pragma(filter->params);
    for (i = 0; i < 2; i++) {
	if (!i) {
	    d = data_clone(qdisc->if_expr);
	    d = op_binary(&op_logical_or,d,data_decision(dr_continue,NULL));
	}
	else {
	    if (!qdisc->dsc->default_class)
		lerrorf(qdisc->location,
		  "qdisc \"%s\" does not provide default class",
		  qdisc->dsc->name);
	    d = qdisc->if_expr;
	    qdisc->dsc->default_class(&d,qdisc);
	}
	iflib_arith(&d);
	iflib_not(&d);
	iflib_reduce(&d);
	iflib_normalize(&d);
	iflib_actions(filter->location,&d);
	iflib_offset(&d,1);
	if (!d.op && d.type == dt_unum && !d.u.unum) return;
	if (!i && continue_at_end(d)) break;
    }
    dump_actions(d);
    collect_protocols(filter,d);
    protocol = protocols;
    prio_offset = 0;
    handle = 0;
    while (1) {
	last_meta = 0;
	next_hash(filter);
	(void) dump_or(filter,d,TC_U32_ROOT,1,offset_group);
	if (!protocol) break;
	protocol = protocol->next;
	prio_offset++;
	new_handle();
    }
    free_actions();
    free_protocols();
}
