/*
 * iflib_bit.c - FSM based on single-bit decisions
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 Network Robots
 */


/*
 * Known bugs and restrictions: @@@
 * - doesn't assign "nice" numbers to actions
 * - doesn't re-order bits
 * - tree walking is usually terribly inefficient. Some examples:
 *   count_paths: should try to multiply paths in and out of a node instead of
 *     enumerating them
 *   find_least_common_branch: should remember nodes visited
 * - if_ext may issue offset groups and buckets which are actually never used
 *   (only happens if user quite explicitly puts redundancy in the expression)
 */


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "field.h"
#include "iflib.h"
#include "ext_all.h"


extern int get_offset_group(DATA d,int base); /* borrowing from if_ext.c */


typedef enum { bt_data,bt_conform,bt_count,bt_decision } BIT_TYPE;

typedef struct _bit {
    BIT_TYPE type;
    int ref;			/* reference count */
    union {
	struct {		/* packet data */
	    int bit_num;
	    int offset_group;
	} data;
	DATA decision;
	const POLICE *bucket;
    } u;
    struct _bit *true,*false;	/* next bit if true or false */
    struct _referer *referers;	/* list of referers; built during optimiz. */
    int number;			/* state or action number; -1 for unassigned */
    int dump_number;		/* idem, used for dumps only */
    int ref_validation;		/* counter used for reference validation */
    unsigned long node_count;	/* number of paths going through node */
    unsigned long branch_count[2]; /* number of paths going through branch */
} BIT;

typedef struct _bit_list {
    BIT *bit;
    struct _bit_list *next;
} BIT_LIST;

typedef struct _referer {
    BIT *bit;
    int branch;	/* 0: false, 1: true */
    struct _referer *next;
} REFERER;


/* --------------------------- Helper functions ---------------------------- */


static BIT *make_bit(BIT_TYPE type,BIT *true,BIT *false)
{
    BIT *bit;

    bit = alloc_t(BIT);
    bit->type = type;
    bit->ref = 1;
    bit->true = true;
    bit->false = false;
    bit->referers = NULL;
    bit->number = bit->dump_number = -1;
    bit->ref_validation = 0;
    bit->node_count= bit->branch_count[0] = bit->branch_count[1] = 0;
    return bit;  
}


static BIT *get_bit(BIT *bit)
{
    if (bit) bit->ref++;
    return bit;
}


static void clear_referers(BIT *bit);


static void put_bit(BIT *bit)
{
    if (!bit) return;
    if (!--bit->ref) {
	put_bit(bit->true);
	put_bit(bit->false);
	clear_referers(bit);
	free(bit);
    }
}


static void add_to_list(BIT_LIST **root,BIT *bit)
{
    BIT_LIST *new;

    new = alloc_t(BIT_LIST);
    new->bit = get_bit(bit);
    new->next = *root;
    *root = new;
}


static void clear_list(BIT_LIST *list)
{
    while (list) {
	BIT_LIST *next = list->next;

	put_bit(list->bit);
	free(list);
	list = next;
    }
}


static BIT *clone_bit(const BIT *in)
{
    BIT *bit;

    assert(!in->referers);
    bit = alloc_t(BIT);
    *bit = *in;
    bit->ref = 1;
    get_bit(bit->true);
    get_bit(bit->false);
    return bit;
}


static void add_referer(BIT *bit,int branch)
{
    REFERER *new;

    new = alloc_t(REFERER);
    new->bit = get_bit(bit);
    new->branch = branch;
    new->next = (branch ? bit->true : bit->false)->referers;
    (branch ? bit->true : bit->false)->referers = new;
}


static void clear_referers(BIT *bit)
{
    while (bit->referers) {
	REFERER *next = bit->referers->next;

	put_bit(bit->referers->bit);
	free(bit->referers);
	bit->referers = next;
    }
}


/* ---------------------- Reference count validation ----------------------- */


#ifdef NDEBUG

#define validate_references(x)

#else

static void recount_references(BIT *bit)
{
    if (!bit || bit->ref_validation) return;
    if (bit->true) {
	recount_references(bit->true);
	bit->true->ref_validation++;
    }
    if (bit->false) {
	recount_references(bit->false);
	bit->false->ref_validation++;
    }
}


static void check_references(BIT *bit)
{
    if (!bit || !bit->ref_validation) return;
    if (bit->ref != bit->ref_validation) {
	fprintf(stderr,"check_references: ref %d, counted %d\n",bit->ref,
	  bit->ref_validation);
	abort();
    }
    bit->ref_validation = 0;
    check_references(bit->true);
    check_references(bit->false);
}


static void validate_references(BIT *root)
{
    recount_references(root);
    root->ref_validation++;
    check_references(root);
}

#endif /* NDEBUG */


/* --------------------------- FSM construction ---------------------------- */


static BIT *value_fsm(int offset,uint32_t value,uint32_t mask,
  int length,BIT *true,BIT *false,int offset_group)
{
    BIT *last;
    int i;

    last = true;
    for (i = 0; i < length; i++)
	if ((mask >> i) & 1) {
	    last = (value >> i) & 1 ? make_bit(bt_data,last,get_bit(false)) :
	      make_bit(bt_data,get_bit(false),last);
	    last->u.data.bit_num = length-i-1+offset*8;
	    last->u.data.offset_group = offset_group;
	}
    put_bit(false);
    return last;
}


static BIT *eq_fsm(DATA a,DATA b,BIT *true,BIT *false,int offset_group)
{
    uint32_t value;
    uint32_t field_mask = 0xffffffff,value_mask = 0xffffffff;
    int shift = 0; /* negative means shift value right */
    int constant = 0; /* 0: field content affects result; -1: always false;
			 1: always true */
    /*
     * We expect the expression to follow the following constraints:
     * - left-hand side must contain a packet data access
     * - the offsets must be constant (iflib_off does this for us)
     * - right-hand side must be a constant (iflib_arith does this)
     */
    if (b.op || b.type != dt_unum) dump_failed(b,"\"eq const\" expected");

    value = b.u.unum;
    while (!a.op || a.op->dsc != &op_access) {
	if (!a.op) dump_failed(a,"operator expected");
	if (a.op->dsc == &op_offset) {
	    offset_group = get_offset_group(a,offset_group);
	    a = a.op->a;
	}
	else if (a.op->dsc == &op_and) {
	    if (!a.op->a.op && a.op->a.type == dt_unum) {
		b = a.op->a;
		a = a.op->b;
	    }
	    else {
		if (a.op->b.op || a.op->b.type != dt_unum)
		    dump_failed(a,"no constant in and");
		b = a.op->b;
		a = a.op->a;
	    }
	    field_mask &= b.u.unum;
	    value_mask &= b.u.unum;
	}
	else dump_failed(a,"bad operator");
    }
    if (a.op->b.op || a.op->b.type != dt_unum) dump_failed(a,"bad offset");
    if (a.op->c.op || a.op->c.type != dt_unum) dump_failed(a,"bad size");
#if 0
fprintf(stderr,"value 0x%lx, shift %d\n",(unsigned long) value,shift);
#endif
    if (shift) {
	if (shift < 0) {
	    if (value << (32+shift)) constant = -1;
	    value >>= -shift;
	}
	else {
	    if (value >> (32-shift)) constant = -1;
	    value <<= shift;
	}
    }
#if 0
fprintf(stderr,"(field & 0x%lx) == (0x%lx & 0x%lx) [0x%lx]\n",
  (unsigned long) field_mask,(unsigned long) value,(unsigned long) value_mask,
  (unsigned long) (value & value_mask));
#endif
    if (a.op->c.u.unum != 32) field_mask &= (1 << a.op->c.u.unum)-1;
    if (!constant && (shift < -32 || shift > 32))
	constant = field_mask ? -1 : 1;
    if (!constant && (value & ~value_mask)) constant = -1;
    if (constant) {
	yywarnf("comparison is always %s",constant > 0 ? "true" : "false");
	if (constant > 0) {
	    put_bit(false);
	    return true;
	}
	else {
	    put_bit(true);
	    return false;
	}
    }
    return value_fsm(a.op->b.u.unum,value & value_mask,field_mask,
      a.op->c.u.unum,true,false,offset_group);
}


static BIT *bit_tree(DATA d,BIT *true,BIT *false,int offset_group)
{
    BIT *bit;

    if (d.op) {
	if (d.op->dsc == &op_logical_or)
	    return bit_tree(d.op->a,true,
	      bit_tree(d.op->b,get_bit(true),false,offset_group),offset_group);
	if (d.op->dsc == &op_logical_and)
	    return bit_tree(d.op->a,
	      bit_tree(d.op->b,true,get_bit(false),offset_group),
	      false,offset_group);
	if (d.op->dsc == &op_logical_not)
	    return bit_tree(d.op->a,false,true,offset_group);
	if (d.op->dsc == &op_eq)
	    return eq_fsm(d.op->a,d.op->b,true,false,offset_group);
	if (d.op->dsc == &op_field_root)
	    return bit_tree(d.op->a,true,false,
	      d.op->b.u.field_root->offset_group);
	if (d.op->dsc == &op_offset)
	    return bit_tree(d.op->a,true,false,
	      get_offset_group(d,offset_group));
	if (d.op->dsc == &op_conform) {
	    BIT *bit;

	    bit = make_bit(bt_conform,true,false);
	    bit->u.bucket = d.op->a.u.police;
	    return bit;
	}
	if (d.op->dsc == &op_count) {
	    BIT *bit;

	    bit = make_bit(bt_count,true,NULL);
	    bit->u.bucket = d.op->a.u.police;
	    put_bit(false);
	    return bit;
	}
	dump_failed(d,"bad operator");
    }
    switch (d.type) {
	case dt_unum:
	    if (d.u.unum) {
		put_bit(false);
		return true;
	    }
	    else {
		put_bit(true);
		return false;
	    }
	    break;
	case dt_decision:
	    put_bit(true);
	    put_bit(false);
	    bit = make_bit(bt_decision,NULL,NULL);
	    bit->u.decision = d;
	    return bit;
	default:
	    dump_failed(d,"bad terminal");
    }
    abort(); /* not reached */
}


/* --------------------- Bubble static classification ---------------------- */

/*
 * count(static(A,B)) -> static(count(A),count(B))
 * conform(static(A,B),X) -> static(conform(A,X),conform(B,X))
 * conform(X,static(A,B)) -> static(conform(X,A),conform(X,B))
 */


static BIT *swap_bits(BIT *bit,int true)
{
    BIT *new,*old;

    old = true ? bit->true : bit->false;
    new = clone_bit(old);
    new->true = clone_bit(bit);
    new->false = clone_bit(bit);
    if (true) {
	new->true->true = old->true;
	new->false->true = old->false;
    }
    else {
	new->true->false = old->true;
	new->false->false = old->false;
    }
    put_bit(old); /* references from both clones */
    put_bit(old);
    return new;
}


static void bubble_static(BIT **bit)
{
    if ((*bit)->true) bubble_static(&(*bit)->true);
    if ((*bit)->false) bubble_static(&(*bit)->false);
    switch ((*bit)->type) {
	BIT *tmp;

	case bt_conform:
	    if ((*bit)->false->type == bt_data) {
		tmp = swap_bits(*bit,0);
		put_bit(*bit);
		*bit = tmp;
		bubble_static(bit);
		break;
	    }
	    /* fall through */
	case bt_count:
	    if ((*bit)->true->type == bt_data) {
		tmp = swap_bits(*bit,1);
		put_bit(*bit);
		*bit = tmp;
		bubble_static(bit);
	    }
	    break;
	default:
	    break;
    }
}


/* -------------------- Removal of duplicate decisions --------------------- */


typedef struct _prev {
    BIT *bit;
    int branch; /* 0: false, 1: true */
    struct _prev *prev;
} PREV;


static BIT *eliminate(BIT *bit,const PREV *old,PREV *prev)
{
    BIT *res,*next;
    PREV *walk;

    res = next = get_bit(old->branch ? bit->true : bit->false);
    for (walk = prev; walk != old; walk = walk->prev) {
	BIT *this;

	this = clone_bit(walk->bit);
	walk->bit = this;
	if (walk->branch) {
	    put_bit(this->true);
	    this->true = next;
	}
	else {
	    put_bit(this->false);
	    this->false = next;
	}
	next = this;
    }
    if (old->branch) {
	put_bit(old->bit->true);
	old->bit->true = next;
    }
    else {
	put_bit(old->bit->false);
	old->bit->false = next;
    }
    return res;
}


static void remove_duplicates(BIT *bit,PREV *prev)
{
    PREV this;
    const PREV *walk;

    switch (bit->type) {
	case bt_data:
	    for (walk = prev; walk; walk = walk->prev)
		if (walk->bit->type == bt_data &&
		  walk->bit->u.data.bit_num == bit->u.data.bit_num &&
		  walk->bit->u.data.offset_group == bit->u.data.offset_group) {
		    remove_duplicates(eliminate(bit,walk,prev),prev);
		    return; /* tail recursion */
		}
	    break;
	case bt_conform:
	    for (walk = prev; walk; walk = walk->prev) {
		if (walk->bit->type == bt_count &&
		  walk->bit->u.bucket == bit->u.bucket)
		    break;
		if (walk->bit->type == bt_conform &&
		  walk->bit->u.bucket == bit->u.bucket) {
		    remove_duplicates(eliminate(bit,walk,prev),prev);
		    return; /* tail recursion */
		}
	    }
	    break;
	case bt_count:
	case bt_decision:
	    break;
	default:
	    abort();
    }
    this.bit = bit;
    this.prev = prev;
    if (bit->false) {
	this.branch = 0;
	remove_duplicates(bit->false,&this);
    }
    /*
     * this.bit may have been changed underneath us, and the old "bit" may
     * no longer exist.
     */
    if (this.bit->true) {
	this.branch = 1;
	remove_duplicates(this.bit->true,&this);
    }
}


/* ---------------------- Registry for sharing leaves ---------------------- */


static BIT_LIST *leaves = NULL;


static int bit_equal(const BIT *a,const BIT *b)
{
    if (a->type != b->type) return 0;
    switch (a->type) {
	case bt_data:
	    if (a->u.data.bit_num != b->u.data.bit_num) return 0;
	    return a->u.data.offset_group == b->u.data.offset_group;
	case bt_conform:
	case bt_count:
	    return a->u.bucket == b->u.bucket;
	case bt_decision:
	    return data_equal(a->u.decision,b->u.decision);
	default:
	    abort();
    }
}


static BIT *find_leaf(BIT *bit)
{
    BIT_LIST *leaf;

    for (leaf = leaves; leaf; leaf = leaf->next)
	if (bit_equal(leaf->bit,bit))
	    return leaf->bit;
    add_to_list(&leaves,bit);
    return bit;
}


/* ------------------ Support for debugging unique_state ------------------- */


BIT *merged; /* @@@ debugging */


#ifdef NDEBUG

#define validate(x)

#else

static void do_validate(const BIT *bit)
{
    if (!bit) return;
    if (bit->type & ~3) abort();
    do_validate(bit->true);
    do_validate(bit->false);
}


static void validate(const BIT *bit)
{
    if (debug) do_validate(bit);
}


#endif /* NDEBUG */


/* ------------------------- Combine common states ------------------------- */


static BIT *unique_state(BIT *bit)
{
    BIT *tmp;
    REFERER *a,*b;

    if (!bit) return NULL;
    debugf("unique_state: begin bit %d [ref %d]",bit->type,bit->ref);
    tmp = unique_state(bit->true);
    debugf("| %p [%d] -> %p [%d]",bit->true,bit->true ? bit->true->ref : 0,
      tmp,tmp ? tmp->ref : 0);
    put_bit(bit->true);
    bit->true = tmp;
    validate(merged);
    tmp = unique_state(bit->false);
    debugf("| %p [%d] -> %p [%d]",bit->false,bit->false ? bit->false->ref : 0,
      tmp,tmp ? tmp->ref : 0);
    validate(merged);
    put_bit(bit->false);
    bit->false = tmp;
    validate(merged);
    if (bit->type != bt_decision && bit->true == bit->false) {
	get_bit(tmp);
	debugf("| eliminating: %p [%d] becomes %p [%d]",bit,bit->ref,tmp,
	  tmp->ref);
	validate(merged);
	return tmp;
    }
    switch (bit->type) {
	case bt_data:
	case bt_conform:
	    debugf("| bt_data/bt_conform: searching");
	    for (a = bit->true->referers; a; a = a->next) {
		if (!a->branch) continue;
		if (a->bit == bit) return get_bit(bit);
		if (bit_equal(a->bit,bit))
		    for (b = bit->false->referers; b; b = b->next)
			if (a->bit == b->bit && !b->branch)
			    return get_bit(a->bit);
	    }
	    add_referer(bit,1);
	    add_referer(bit,0);
	    debugf("bt_data/bt_conform: new [%d]",bit->ref);
	    validate(merged);
	    return get_bit(bit);
	case bt_count:
	    debugf("bt_count: searching");
	    for (a = bit->true->referers; a; a = a->next) {
		if (a->bit == bit) return get_bit(bit);
		if (bit_equal(a->bit,bit))
		    return get_bit(a->bit);
	    }
	    add_referer(bit,1);
	    debugf("bt_count: new [%d]",bit->ref);
	    validate(merged);
	    return get_bit(bit);
	case bt_decision:
	    validate(merged);
	    debugf("bt_decision");
	    return get_bit(find_leaf(bit));
	default:
	    abort();
    }
}


static void purge_referers(BIT *bit)
{
    if (!bit) return;
    /* okay to visit same state twice */
    clear_referers(bit);
    purge_referers(bit->true);
    purge_referers(bit->false);
}


static void merge_common(BIT **fsm)
{
//    BIT *merged; /* @@@ debugging */

    validate(*fsm);
    merged = *fsm; /* @@@ debugging */
    merged = unique_state(*fsm);
    put_bit(*fsm);
    purge_referers(merged);
    clear_list(leaves);
    leaves = NULL;
    *fsm = merged;
}


/* ------------------ Collapse subtrees with common state ------------------ */


/*
 * This works by searching the sub-trees below each node for a state (leaf or
 * single-bit decision) that is always reached. If such a state exists, the
 * node can be reduced to that common node.
 *
 * We can include "conform" in this optimization, because it has (nominally) no
 * side-effect. However, we can't progress beyond "count" (which implies
 * possible elimination), because of the side-effect.
 */


static void intersect_list(BIT_LIST **list,const PREV *prev)
{
    BIT_LIST **walk,**next;

    for (walk = list; *walk; walk = next) {
	const PREV *scan;

	for (scan = prev; scan; scan = scan->prev)
	    if (scan->bit == (*walk)->bit) break;
	if (scan) next = &(*walk)->next;
	else {
	    BIT_LIST *curr = *walk;

	    *walk = curr->next;
	    free(curr);
	    next = walk;
	}
    }
}


static void copy_list(BIT_LIST **list,const PREV *prev)
{
    const PREV *walk;
    BIT_LIST **last = list;

    for (walk = prev; walk; walk = walk->prev) {
	BIT_LIST *new;

	new = alloc_t(BIT_LIST);
	new->bit = walk->bit;
	new->next = NULL;
	*last = new;
	last = &new->next;
    }
}


static void gather_common(BIT *bit,BIT_LIST **list,int *first,
  PREV *prev)
{
    PREV this;

    if (!bit) return;
    this.bit = bit;
    this.prev = prev;
    switch (bit->type) {
	case bt_data:
	case bt_conform:
	    gather_common(bit->true,list,first,&this);
	    gather_common(bit->false,list,first,&this);
	    break;
	case bt_count:
	case bt_decision:
	    if (!*first) intersect_list(list,&this);
	    else {
		*first = 0;
		copy_list(list,&this);
	    }
	    break;
	default:
	    abort();
    }
}


static void collapse_common(BIT **bit)
{
    BIT_LIST *list = NULL;
    int first = 1;

    if (!*bit) return;
    if ((*bit)->type != bt_count) {
	gather_common((*bit)->true,&list,&first,NULL);
	gather_common((*bit)->false,&list,&first,NULL);
	if (list) {
	    get_bit(list->bit);
	    put_bit(*bit);
	    *bit = list->bit;
	}
	intersect_list(&list,NULL); /* clear list */
    }
    collapse_common(&(*bit)->true);
    collapse_common(&(*bit)->false);
}


/* ------------------------ Raw dump of a bit tree ------------------------- */


static void deassign_numbers(BIT *bit)
{
    if (!bit) return;
    if (bit->dump_number == -1) return; /* already visited */
    bit->dump_number = -1;
    deassign_numbers(bit->true);
    deassign_numbers(bit->false);
}


static void do_dump(FILE *file,BIT *bit,int *number)
{
    if (!bit) return;
    if (bit->dump_number != -1) return; /* already visited */
    bit->dump_number = (*number)++;
    do_dump(file,bit->true,number);
    do_dump(file,bit->false,number);
    fprintf(file,"%d: ",bit->dump_number);
    switch (bit->type) {
	case bt_data:
	    fprintf(file,"data %d %d",bit->u.data.offset_group,
	      bit->u.data.bit_num);
	    break;
	case bt_conform:
	    fprintf(file,"conform %lu",(unsigned long) bit->u.bucket->number);
	    break;
	case bt_count:
	    fprintf(file,"count %lu",(unsigned long) bit->u.bucket->number);
	    break;
	case bt_decision:
	    print_data(file,bit->u.decision);
	    break;
	default:
	    abort();
    }
    if (bit->true) fprintf(file," %d",bit->true->dump_number);
    if (bit->false) fprintf(file," %d",bit->false->dump_number);
    fprintf(file," # ref %d count %lu (%lu,%lu)\n",bit->ref,bit->node_count,
      bit->branch_count[1],bit->branch_count[0]);
}


static void dump_fsm(FILE *file,BIT *fsm)
{
    int number = 0;

    deassign_numbers(fsm);
    do_dump(file,fsm,&number);
}


static void debug_fsm(const char *label,BIT *fsm)
{
    if (!debug) return;
    fprintf(stderr,"----- %s ---------------\n",label);
    dump_fsm(stderr,fsm);
}


/* ----------------------------- Dump actions ------------------------------ */


static void dump_this_action(FILE *file,QDISC *qdisc,BIT *bit,int top)
{
    DATA d;

    if (!bit) return;
    if (bit->number != -1 && !top) {
	fprintf(file," action %d",bit->number);
	return;
    }
    switch (bit->type) {
	case bt_conform:
	    fprintf(file," conform %d",(int) bit->u.bucket->number);
	    break;
	case bt_count:
	    fprintf(file," count %d",(int) bit->u.bucket->number);
	    break;
	case bt_decision:
	    d = bit->u.decision;
	    switch (d.u.decision.result) {
		case dr_continue:
		    fprintf(file," unspec");
		    break;
		case dr_class:
		    if (dump_all_decision(file,qdisc,
		      d.u.decision.class->number))
			break;
		    fprintf(file," class %u:%u",
		      (unsigned) d.u.decision.class->parent.qdisc->number,
		      (unsigned) d.u.decision.class->number);
		    break;
		case dr_drop:
		    fprintf(file," drop");
		    break;
		default:
		     /* @@@ handle reclassify */
		     abort();
	    }
	    break;
	default:
	    abort();
    }
    dump_this_action(file,qdisc,bit->true,0);
    dump_this_action(file,qdisc,bit->false,0);
}


static void number_actions(FILE *file,QDISC *qdisc,BIT *bit,int *number,
  int from_static)
{
    if (!bit) return;
    if (bit->number != -1) return; /* already visited */
    if (bit->type == bt_data) {
	number_actions(file,qdisc,bit->true,number,1);
	number_actions(file,qdisc,bit->false,number,1);
    }
    else {
	number_actions(file,qdisc,bit->true,number,0);
	number_actions(file,qdisc,bit->false,number,0);
	bit->number = (*number)++;
	fprintf(file,"action %d =",bit->number);
	dump_this_action(file,qdisc,bit,1);
	fprintf(file,"\n");
    }
}


static void dump_actions(FILE *file,QDISC *qdisc,BIT *fsm)
{
    int number = 0;

    number_actions(file,qdisc,fsm,&number,1);
}


/* ------------------------ Dump static classifier ------------------------- */


typedef struct _history {
    BIT *bit;
    int branch;
    struct _history *prev;
    struct _history *next;
    struct _history *lnk; /* alternative to next */
} HISTORY;


static void dump_match(FILE *file,HISTORY *start,int length)
{
    HISTORY *walk = start;
    int i,curr = 0,first = 1;

    fprintf(file," %d:%d:%d=0x",start->bit->u.data.offset_group,
      start->bit->u.data.bit_num,length);
    for (i = 1; i <= length; i++) {
	int in_nibble;

	in_nibble = (length-i) & 3;
	if (walk->branch) curr |= 1 << in_nibble;
	if (!in_nibble && (curr || !first)) {
	    fprintf(file,"%x",curr);
	    first = curr = 0;
	}
	walk = walk->next;
    }
    if (first) putc('0',file);
}


static void dump_static(FILE *file,BIT *bit,HISTORY *root,HISTORY *prev)
{
    if (!bit) return; /* hmm, we just skipped a useless branch */
    if (bit->type == bt_data) {
	HISTORY this;

	this.bit = bit;
	this.prev = prev;
	this.next = NULL;
	if (prev) prev->next = &this;
	this.branch = 1;
	dump_static(file,bit->true,root ? root : &this,&this);
	this.branch = 0;
	dump_static(file,bit->false,root ? root : &this,&this);
	if (prev) prev->next = NULL;
    }
    else {
	HISTORY *walk,*start = NULL;
	int length = 0; /* for gcc */

	fprintf(file,"match");
	for (walk = root; walk; walk = walk->next)
	    if (start && start->bit->u.data.bit_num+length ==
	      walk->bit->u.data.bit_num)
		length++;
	    else {
		if (start) dump_match(file,start,length);
		start = walk;
		length = 1;
	    }
	if (start) dump_match(file,start,length);
	fprintf(file," action %d\n",bit->number);
    }
}


/* ---------------------- Dump static classifier (2) ----------------------- */


static void dump_all_paths_to(FILE *file,BIT *bit,HISTORY *root,HISTORY *prev,
  const BIT *stop,int last_branch)
{
    if (!bit) return; /* hmm, we just skipped a useless branch */
    if (!stop) {
	fprintf(file,"match action %d\n",bit->number);
	return;
    }
    if (bit == stop) {
	HISTORY this,*walk,*start = NULL;
	HISTORY *real_root;
	int length = 0; /* for gcc */

	this.bit = bit;
	this.prev = prev;
	this.next = NULL;
	if (prev) prev->next = &this;
	this.branch = last_branch;
	fprintf(file,"match");
	/*
	 * @@@ Very inefficient and needlessly messy way of sorting. We should
	 * do all this even before removing redundant nodes.
	 */
	{
	    HISTORY **a,*b;
	    int last = -1;

	    for (walk = root ? root : &this; walk; walk = walk->lnk)
		walk->lnk = walk->next;
	    if (!root) root = &this; /* hack, but safe */
	    real_root = root;
	    a = &root;
	    while (1) {
		*a = NULL;
		for (b = real_root; b; b = b->lnk)
		    if (last < b->bit->u.data.bit_num)
			if (!*a || b->bit->u.data.bit_num <
			  (*a)->bit->u.data.bit_num) *a = b;
		if (!*a) break;
		last = (*a)->bit->u.data.bit_num;
		a = &(*a)->next;
	    }
	}
	for (walk = root ? root : &this; walk; walk = walk->next)
	    if (start && start->bit->u.data.bit_num+length ==
	      walk->bit->u.data.bit_num)
		length++;
	    else {
		if (start) dump_match(file,start,length);
		start = walk;
		length = 1;
	    }
	if (start) dump_match(file,start,length);
	{
	    /* fixup */
	    for (walk = real_root; walk; walk = walk->lnk)
		walk->next = walk->lnk;
	}
	fprintf(file," action %d\n",last_branch ? bit->true->number :
	  bit->false->number);
	return;
    }
    if (bit->type == bt_data) {
	HISTORY this;

	this.bit = bit;
	this.prev = prev;
	this.next = NULL;
	if (prev) prev->next = &this;
	this.branch = 1;
	dump_all_paths_to(file,bit->true,root ? root : &this,&this,stop,
	  last_branch);
	this.branch = 0;
	dump_all_paths_to(file,bit->false,root ? root : &this,&this,stop,
	  last_branch);
    }
}


/* ---------------- Smart dumping of static classification ----------------- */



static void clear_counters(BIT *bit)
{
    if (!bit) return;
    if (!bit->node_count) return; /* already visited */
    bit->node_count = bit->branch_count[0] = bit->branch_count[1] = 0;
    clear_counters(bit->true);
    clear_counters(bit->false);
}


static int count_paths(BIT *bit)
{
    int count0,count1;

    if (!bit) return 0;
    if (bit->type != bt_data) {
	bit->node_count++;
	return 1;
    }
    count0 = count_paths(bit->false);
    count1 = count_paths(bit->true);
    bit->branch_count[0] += count0;
    bit->branch_count[1] += count1;
    bit->node_count += count0+count1;
    return count0+count1;
}


/*
 * pick_best is truly the brain of the smart dumping. The goal is to pick
 * the branch whose removal will eliminate as many paths as possible, at as
 * little cost as possible.
 *
 * The heuristics used here are:
 * - cost equals the number of paths we need to dump to reach this branch
 * - if cost is equal, we pick the branch that goes to the action node with
 *   the lowest reference count, hoping that we'll be able to quickly
 *   eliminate the entire node
 *
 * This strategy is not perfect. One of its problems is that is does not take
 * into consideration the cost of removing further branches to the action
 * node, e.g. if we have a node with three "cheap" branches, and one
 * prohibitively expensive branch, we will dump the cheap branches just to
 * find out that we'll have to try to eliminate some other node first, so
 * it would have been better to start right with that other node.
 */


#if 0 /* idiot algorithm */

/*
 * Primary: branch with lowest frequency
 * Secondary: action node with lowest reference count
 */

static void pick_best(BIT *bit,int branch,BIT **end,int *best_branch,
  unsigned long *best_freq,unsigned long *best_alternative)
{
    BIT *next = branch ? bit->true : bit->false;

    if (next->type == bt_data) return;
    if (*best_freq < bit->branch_count[branch]) return;
    if (*best_freq == bit->branch_count[branch] &&
      *best_alternative < next->ref) return;
    *best_freq = bit->branch_count[branch];
    *best_alternative = next->ref;
    *end = bit;
    *best_branch = branch;
}

#endif /* idiot algorithm */


#if 1 /* merely stupid algorithm */

/*
 * Primary: action node with lowest frequency
 * Secondary: branch with lowest frequency
 */

static void pick_best(BIT *bit,int branch,BIT **end,int *best_branch,
  unsigned long *best_freq,unsigned long *best_alternative)
{
    BIT *next = branch ? bit->true : bit->false;

    if (next->type == bt_data) return;
    if (*best_freq < next->node_count) return;
    if (*best_freq == next->node_count &&
      *best_alternative < bit->branch_count[branch]) return;
    *best_freq = next->node_count;
    *best_alternative = bit->branch_count[branch];
    *end = bit;
    *best_branch = branch;
}

#endif /* stupid algorithm */


static void do_find_least_common_branch(BIT *bit,BIT **end,int *best_branch,
  unsigned long *best_freq,unsigned long *best_alternative)
{
    if (!bit) return;
    if (bit->type == bt_data) {
	pick_best(bit,0,end,best_branch,best_freq,best_alternative);
	pick_best(bit,1,end,best_branch,best_freq,best_alternative);
    }
    do_find_least_common_branch(bit->true,end,best_branch,best_freq,
      best_alternative);
    do_find_least_common_branch(bit->false,end,best_branch,best_freq,
      best_alternative);
}


static void find_least_common_branch(BIT *bit,BIT **end,int *best_branch)
{
    unsigned long best_freq = ~0,best_alternative = 0;

    do_find_least_common_branch(bit,end,best_branch,&best_freq,
      &best_alternative);
}


static void eliminate_bundle(BIT *end,int last_branch)
{
    /*
     * This is a little hairy. There are a few constraints which are met when
     * we call this function:
     * - end points to a bt_data node
     * - end->true and end->false are different (or the node would have been
     *   eliminated before)
     *
     * Also, once we make end->true and end->false equal, we can leave it to
     * the other optimization algorithms to remove all redundancy.
     */
    assert(end);
    assert(end->type == bt_data);
    assert(end->true != end->false);
    if (last_branch) {
	put_bit(end->true);
	end->true = get_bit(end->false);
    }
    else {
	put_bit(end->false);
	end->false = get_bit(end->true);
    }
}


static void smart_dump(FILE *file,BIT *fsm)
{
    while (1) {
	BIT *end = NULL;
	int branch;

	clear_counters(fsm);
	count_paths(fsm);
	debug_fsm("smart_dump, AFTER counting",fsm);
	find_least_common_branch(fsm,&end,&branch);
	dump_all_paths_to(file,fsm,NULL,NULL,end,branch);
	if (fsm->type != bt_data) return;
	debug_fsm("smart_dump, BEFORE elimination",fsm);
	eliminate_bundle(end,branch);
	debug_fsm("smart_dump, about to reduce",fsm);
	merge_common(&fsm); /* still needed ? */
	debug_fsm("smart_dump, after merging, before collapsing",fsm);
	collapse_common(&fsm);
	debug_fsm("smart_dump, AFTER reduction",fsm);
    }
}


/* ------------------------------------------------------------------------- */


void iflib_bit(FILE *file,QDISC *qdisc,DATA d)
{
    BIT *fsm;

//    start_timer();
    fsm = bit_tree(d,NULL,NULL,0);
    if (debug) validate_references(fsm);
    debug_fsm("after bit_tree",fsm);

    remove_duplicates(fsm,NULL);
    validate_references(fsm);
    debug_fsm("after remove_duplicates",fsm);

    bubble_static(&fsm);
    validate_references(fsm);
    debug_fsm("after bubble_static",fsm);

    merge_common(&fsm);
    validate_references(fsm);
    debug_fsm("after merge_common",fsm);

    collapse_common(&fsm);
    validate_references(fsm);
    debug_fsm("after collapse_common",fsm);

//    print_timer("old bit");
    dump_actions(file,qdisc,fsm);
    /*
     * Note: with debugging enabled, calls to debug_fsm in smart_dump will
     * clobber the action numbers assigned by dump_actions. @@@
     */
    smart_dump(file,fsm);
//    dump_static(stderr,fsm,NULL,NULL);
}
