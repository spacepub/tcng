/*
 * iflib_newbit.c - FSM based on single-bit decisions (improved version)
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 Network Robots
 */

/*
 * UNDER CONSTRUCTION - DOESN'T PRODUCE USEFUL OUTPUT YET
 */

/*
 * Known problems:
 *
 * - path selection doesn't produce useful results
 * - to add insult to injury, path selection is also dreadfully slow
 * - data provided by count_out_frequency is probably too fine-grained for path
 *   selection. Should try cuonting nodes, but instead not removing all paths
 *   to an edge, but rather individual paths.
 * - among themselves, consolidate_nodes and consolidate_node iterate with N^2
 *   over the referers. By pre-sorting the list, this could be cut to N.
 * - output has not been validated for correctness other than for trivial cases
 * - code is sprinkled to too many debugging/checking/diagnostic/profiling
 *   functions
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "field.h"
#include "tree.h"
#include "iflib.h"


typedef enum { bt_data,bt_conform,bt_count,bt_decision } BIT_TYPE;

typedef struct _bit {
    BIT_TYPE type;
    union {
	struct {
	    int bit_num;
	    int offset_group;
	} data;
	DATA decision;
	const POLICE *bucket;
    } u;
    struct _node *node; /* for terminals: identify unique node; NULL if none */
    struct _bit *next;
    /* ----- Fields used when printing a DAG ----- */
    int action;			/* action number */
int scratch;
} BIT;

typedef struct _node {
    BIT *bit;
    int ref;			/* reference count */
    struct _node *edge[2];
    unsigned long mark;		/* generic mark, used to traverse the DAG. Each
				 * traversal uses its own unique mark, so we
				 * never have to "clean up". */
    /* ----- Fields used for validating reference counts ----- */
    int ref_validation;		/* references recount; 0 if not visited */
    int referer_validation;	/* separate count for referers */
    /* ----- Fields used when sorting the DAG ----- */
    struct _node *curr_self;	/* in visited nodes, points to self or clone */
    /* ----- Fields used when decomposing the DAG ----- */
    int leaf[2];		/* leaf edge index; 0 if not leaf edge */
    /* ----- Fields used when printing a DAG ----- */
    int action;			/* action number; undefined if not action */
    int number;			/* node number, for debugging dumps */
    /* ----- Fields used when counting frequencies ----- */
    int in_frequency;		/* paths entering this node */
    int *out_frequency;		/* paths leaving this node */
    /* ----- Fields used when merging tails ----- */
    struct referer *referers;	/* table of referers; NULL if not set */
    int num_referers;		/* number of referers in table (<= ref) */
int consolidated;
} NODE;

struct referer {
    NODE *node;		/* NULL if node inaccessible (or at root) */
    int edge;
    NODE **anchor;	/* NULL if anchor inaccssible (NB: node -> anchor) */
};


/* ----- Functions to define total order for "bits" ------------------------ */

 
/*
 * Note: although we include "conform" and "count" in the total order, they are
 * never sorted in the DAG.
 */


static int compare_decision(DECISION_RESULT res,const CLASS *a,const CLASS *b)
{
    switch (res) {
	case dr_continue:
	    return 0;
	case dr_class:
	    if (a->number != b->number)
		return a->number < b->number ? -1 : 1;
	    return 0;
	case dr_drop:
	    return 0;
	case dr_reclassify:
	    if (a->number != b->number)
		return a->number < b->number ? -1 : 1;
	    return 0;
	default:
	    abort();
    }

}


static int compare_bit(const BIT *a,const BIT *b)
{
    if (a->type != b->type) return a->type < b->type ? -1 : 1;
    switch (a->type) {
	case bt_data:
	    if (a->u.data.offset_group != b->u.data.offset_group)
		return a->u.data.offset_group < b->u.data.offset_group ? -1 : 1;
	    if (a->u.data.bit_num != b->u.data.bit_num)
		return a->u.data.bit_num < b->u.data.bit_num ? -1 : 1;
	    return 0;
	case bt_conform:
	    if (a->u.bucket->number != b->u.bucket->number)
		return a->u.bucket->number < b->u.bucket->number ? -1 : 1;
	    return 0;
	case bt_count:
	    if (a->u.bucket->number != b->u.bucket->number)
		return a->u.bucket->number < b->u.bucket->number ? -1 : 1;
	    return 0;
	case bt_decision:
	    if (a->u.decision.u.decision.result !=
	      b->u.decision.u.decision.result)
		/*
		 * Not so good: this sorts "drop" between "class" and
		 * "reclassify"
		 */
		return a->u.decision.u.decision.result <
		  b->u.decision.u.decision.result ? -1 : 1;
	    return compare_decision(a->u.decision.u.decision.result,
	      a->u.decision.u.decision.class,b->u.decision.u.decision.class);
	default:
	    abort();
    }
}


/* ----- Validation of reference counts and structure ---------------------- */
 

#ifdef NDEBUG

#define validate_references(x)
#define validate_structure(x)
#define validate_dag(x)

#else

/*
 * recount_references uses "ref_validation" instead of "mark" to record which
 * nodes have already been visited. This way, validate_references can be
 * invoked even while traversing the tree.
 *
 * Because of this, we need to count referers separately, in
 * "referer_validation"
 *
 * validate_structure uses "ref_validation" for the same purpose.
 */


static void recount_references(NODE *node)
{
    if (!node || node->ref_validation) return;
    if (node->edge[0]) {
	recount_references(node->edge[0]);
	node->edge[0]->ref_validation++;
    }
    if (node->edge[1]) {
	recount_references(node->edge[1]);
	node->edge[1]->ref_validation++;
    }
    if (node->referers) {
	struct referer *ref;

	for (ref = node->referers; ref != node->referers+node->num_referers;
	  ref++)
	    if (ref->node) ref->node->referer_validation++;
    }
}


static void check_references(NODE *node)
{
    if (!node || !node->ref_validation) return;
    /*
     * Decision nodes also have one reference from node->bit->node
     */
    if (node->bit->type == bt_decision) node->ref_validation++;
    if (node->ref != node->ref_validation+node->referer_validation) {
	fprintf(stderr,"check_references: ref %d, counted %d+%d\n",node->ref,
	  node->ref_validation,node->referer_validation);
	abort();
    }
    node->ref_validation = node->referer_validation = 0;
    check_references(node->edge[0]);
    check_references(node->edge[1]);
}


static void validate_references(NODE *root)
{
    recount_references(root);
    root->ref_validation++;
    check_references(root);
}


static void do_validate_structure(NODE *node)
{
    if (!node || node->ref_validation) return;
    node->ref_validation = 1;
    if (node->edge[0])
	assert(compare_bit(node->bit,node->edge[0]->bit) < 0);
    if (node->edge[1])
	assert(compare_bit(node->bit,node->edge[1]->bit) < 0);
    assert(node->bit->type == bt_data || (
      (!node->edge[0] || node->edge[0]->bit->type != bt_data) &&
      (!node->edge[1] || node->edge[1]->bit->type != bt_data)));
    assert(node->bit->type == bt_decision || node->edge[0] != node->edge[1]);
    do_validate_structure(node->edge[0]);
    do_validate_structure(node->edge[1]);
}


static void clear_ref_validation(NODE *node)
{
    if (!node || !node->ref_validation) return;
    node->ref_validation = 0;
    clear_ref_validation(node->edge[0]);
    clear_ref_validation(node->edge[1]);
}


static void validate_structure(NODE *root)
{
    do_validate_structure(root);
    clear_ref_validation(root);
}


static void validate_dag(NODE *root)
{
    validate_references(root);
    validate_structure(root);
}

#endif /* NDEBUG */


/* ----- Unique storage of "bits" ------------------------------------------ */


static BIT *bits = NULL;


static void put_node(NODE *node);


static void free_bit(BIT *bit)
{
    put_node(bit->node);
    free(bit);
}


static BIT *store_bit(BIT *bit)
{
    BIT **walk;

    for (walk = &bits; *walk; walk = &(*walk)->next) {
	int cmp;

	cmp = compare_bit(*walk,bit);
	if (cmp > 0) break;
	if (!cmp) {
	    free_bit(bit);
	    return *walk;
	}
    }
    bit->next = *walk;
    *walk = bit;
    return bit;
}


static BIT *make_bit(BIT_TYPE type)
{
    BIT *bit;

    bit = alloc_t(BIT);
    bit->type = type;
    bit->node = NULL;
    bit->next = NULL;
    return bit;
}


/* ----- Basic node operations --------------------------------------------- */


static NODE *make_node(BIT *bit,NODE *true,NODE *false)
{
    NODE *node;

    node = alloc_t(NODE);
    node->bit = bit;
    node->ref = 1;
    node->edge[0] = false;
    node->edge[1] = true;
    node->mark = 0;
    node->ref_validation = node->referer_validation = 0;
    node->in_frequency = 0;
    node->referers = NULL;
    return node;
}


static void free_node(NODE *node)
{
    put_node(node->edge[0]);
    put_node(node->edge[1]);
    if (node->referers) free(node->referers);
    free(node);
}


static NODE *get_node(NODE *node)
{
    if (node) node->ref++;
    return node;
}


static void put_node(NODE *node)
{
//if (node) fprintf(stderr,"put_node(%p) [%d]\n",node,node->ref);
    if (node)
	if (!--node->ref) free_node(node);
}


static unsigned long curr_mark = 0;


static void new_mark(void)
{
    curr_mark++;
    assert(curr_mark); /* better dead than wrapped */
}


/* ----- Debugging DAG dump ------------------------------------------------ */


/*
 * set_node_numbers is done in a separate pass so that we can access the number
 * with any link that may exist between nodes (e.g. edge and referers)
 */


static void set_node_numbers(NODE *node,int *number)
{
    if (!node || node->mark == curr_mark) return;
    node->mark = curr_mark;
    node->number = (*number)++;
    set_node_numbers(node->edge[0],number);
    set_node_numbers(node->edge[1],number);
}


static void do_dump_dag(FILE *file,NODE *node)
{
    const BIT *bit;
    char *tmp;
    DATA d;

    if (!node || node->mark == curr_mark) return;
    node->mark = curr_mark;
    bit = node->bit;
    switch (bit->type) {
	case bt_data:
	    tmp = alloc_sprintf("bit(%d:%d)",bit->u.data.offset_group,
	      bit->u.data.bit_num);
	    break;
	case bt_conform:
	    tmp = alloc_sprintf("conform(%u)",(unsigned) bit->u.bucket->number);
	    break;
	case bt_count:
	    tmp = alloc_sprintf("count(%u)",(unsigned) bit->u.bucket->number);
	    break;
	case bt_decision:
	    d = bit->u.decision;
	    switch (d.u.decision.result) {
		case dr_continue:
		    tmp = stralloc("continue");
		    break;
		case dr_class:
		    tmp = alloc_sprintf("class(%u)",
		      (unsigned) d.u.decision.class->number);
		    break;
		case dr_drop:
		    tmp = stralloc("drop");
		    break;
		case dr_reclassify:
		    tmp = alloc_sprintf("reclassify(%u)",
		      (unsigned) d.u.decision.class->number);
		    break;
		default:
		    abort();
	    }
	    break;
	default:
	    abort();
    }
    do_dump_dag(file,node->edge[1]);
    do_dump_dag(file,node->edge[0]);
    fprintf(file,"%d: %-10s %5d %5d # %p, ref %d",node->number,tmp,
      node->edge[1] ? node->edge[1]->number : -1,
      node->edge[0] ? node->edge[0]->number : -1,node,node->ref);
    free(tmp);
    if (node->referers) {
	int i;

	fprintf(file," %d",node->num_referers);
	for (i = 0; i < node->num_referers; i++) {
	    const struct referer *ref = node->referers+i;

	    fprintf(file,"%c%d.%d",i ? ' ' : '[',
	      ref->node ? ref->node->number : -1,ref->edge);
	    if (!ref->node) {
		if (ref->anchor) fprintf(file,",%p",ref->anchor);
	    }
	    else {
		assert(ref->anchor == ref->node->edge+ref->edge);
	    }
	}
	fprintf(file,"]");
    }
    fprintf(file,"\n");
}


static void dump_dag(NODE *root,const char *fmt,...)
{
    int node_number = 0;
    va_list ap;

    if (!debug) return;
    va_start(ap,fmt);
    fprintf(stderr,"----- ");
    vfprintf(stderr,fmt,ap);
    fprintf(stderr," -----\n");
    va_end(ap);
    new_mark();
    set_node_numbers(root,&node_number);
    new_mark();
    (void) do_dump_dag(stderr,root);
}


/* ----- DAG construction -------------------------------------------------- */


extern int get_offset_group(DATA d,int base); /* borrowing from if_ext.c */


static NODE *value_dag(int offset,uint32_t value,uint32_t mask,
  int length,NODE *true,NODE *false,int offset_group)
{
    NODE *last;
    int i;

    last = true;
    for (i = 0; i < length; i++)
	if ((mask >> i) & 1) {
	    BIT *bit;

	    bit = make_bit(bt_data);
	    bit->u.data.bit_num = length-i-1+offset*8;
	    bit->u.data.offset_group = offset_group;
	    bit = store_bit(bit);
	    last = (value >> i) & 1 ? make_node(bit,last,get_node(false)) :
	      make_node(bit,get_node(false),last);
	}
    put_node(false);
    return last;
}


static NODE *eq_dag(DATA a,DATA b,NODE *true,NODE *false,int offset_group)
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
	if (a.op->dsc == &op_shift_left || a.op->dsc == &op_shift_right) {
	    int n;

	    if (a.op->b.op || a.op->b.type != dt_unum)
		dump_failed(a,"bad shift");
	    n = a.op->b.u.unum;
	    if (a.op->dsc == &op_shift_left) {
		shift -= n;
		field_mask >>= n;
		value_mask >>= n;
	    }
	    else {
		shift += n;
		field_mask <<= n;
		value_mask <<= n;
	    }
	    a = a.op->a;
	    continue;
	}
	if (a.op->dsc == &op_and) {
	    if (!a.op->a.op && a.op->a.type == dt_unum) {
		b = a.op->a;
		a = a.op->b;
	    }
	    else {
		if (a.op->b.op || a.op->b.type != dt_unum)
		    dump_failed(a,"bad shift");
		b = a.op->b;
		a = a.op->a;
	    }
	    field_mask &= b.u.unum;
	    value_mask &= b.u.unum;
	    continue;
	}
	dump_failed(a,"bad operator");
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
	    put_node(false);
	    return true;
	}
	else {
	    put_node(true);
	    return false;
	}
    }
    return value_dag(a.op->b.u.unum,value & value_mask,field_mask,
      a.op->c.u.unum,true,false,offset_group);
}


static NODE *make_dag(DATA d,NODE *true,NODE *false,int offset_group)
{
    BIT *bit;

    if (d.op) {
	if (d.op->dsc == &op_logical_or)
	    return make_dag(d.op->a,true,
	      make_dag(d.op->b,get_node(true),false,offset_group),offset_group);
	if (d.op->dsc == &op_logical_and)
	    return make_dag(d.op->a,
	      make_dag(d.op->b,true,get_node(false),offset_group),
	      false,offset_group);
	if (d.op->dsc == &op_logical_not)
	    return make_dag(d.op->a,false,true,offset_group);
	if (d.op->dsc == &op_field_root)
	    return make_dag(d.op->a,true,false,
	      d.op->b.u.field_root->offset_group);
	if (d.op->dsc == &op_offset)
	    return make_dag(d.op->a,true,false,
	      get_offset_group(d,offset_group));
	if (d.op->dsc == &op_count) {
	    bit = make_bit(bt_count);
	    bit->u.bucket = d.op->a.u.police;
	    put_node(false);
	    return make_node(store_bit(bit),true,NULL);
	}
	/*
	 * From	now on, no terminals may be inserted, and we don't have any
	 * side-effects either, so we can remove expressions where
	 * true == false
	 */
	if (true == false) {
	    put_node(false);
	    return true;
	}
	if (d.op->dsc == &op_eq)
	    return eq_dag(d.op->a,d.op->b,true,false,offset_group);
	if (d.op->dsc == &op_conform) {
	    bit = make_bit(bt_conform);
	    bit->u.bucket = d.op->a.u.police;
	    return make_node(store_bit(bit),true,false);
	}
	dump_failed(d,"bad operator");
    }
    switch (d.type) {
	case dt_unum:
	    if (d.u.unum) {
		put_node(false);
		return true;
	    }
	    else {
		put_node(true);
		return false;
	    }
	    break;
	case dt_decision:
	    put_node(true);
	    put_node(false);
	    bit = make_bit(bt_decision);
	    bit->u.decision = d;
	    bit = store_bit(bit);
	    if (!bit->node) bit->node = make_node(bit,NULL,NULL);
	    return get_node(bit->node);
	default:
	    dump_failed(d,"bad terminal");
    }
    abort(); /* not reached */
}


/* ----- Sort and uniquify the DAG ----------------------------------------- */


static int find_bit_in_dag(const BIT *bit,NODE *root)
{
    if (!root) return 0;
    if (root->bit == bit) return 1;
    if (root->mark == curr_mark) return 0;
    root->mark = curr_mark;
    return find_bit_in_dag(bit,root->edge[0]) ||
      find_bit_in_dag(bit,root->edge[1]);
}


/*
 * reduce_dag does _not_ consume references. Returns item with one reference.
 */


static NODE *reduce_dag(const BIT *bit,int edge,NODE *node)
{
    NODE *true,*false,*new;

    if (!node) return NULL;
    if (node->mark == curr_mark) return get_node(node->curr_self);
    if (node->bit == bit)
	return reduce_dag(bit,edge,node->edge[edge]);
    node->mark = curr_mark;
    node->curr_self = node;
    true = reduce_dag(bit,edge,node->edge[1]);
    false = reduce_dag(bit,edge,node->edge[0]);
    if (true == node->edge[1] && false == node->edge[0]) {
	put_node(true);
	put_node(false);
	return get_node(node);
    }
    new = make_node(node->bit,true,false);
    new->mark = curr_mark;
    node->curr_self = new->curr_self = new;
    return new;
}


/*
 * sort_subdag consumes one reference on node, returns item with one
 * reference.
 */


static NODE *sort_subdag(BIT *start_bit,NODE *node)
{
    BIT *bit;

    for (bit = start_bit; bit && bit->type == bt_data; bit = bit->next) {
	new_mark();
	if (find_bit_in_dag(bit,node)) {
	    NODE *true,*false;

	    new_mark();
	    true = reduce_dag(bit,1,node);
	    new_mark();
	    false = reduce_dag(bit,0,node);
	    /*
	     * BUG: does not catch graphs with changes that become identical
	     * after removal of BIT. Such graphs are probably _very_ rare.
	     */
	    put_node(node);
	    if (true == false) {
		put_node(false);
		return sort_subdag(bit->next,true);
	    }
	    true = sort_subdag(bit->next,true);
	    false = sort_subdag(bit->next,false);
	    if (true == false) {
		put_node(false);
		return true;
	    }
	    /*
	     * Optimization warning: the following two optimizations look
	     * tempting, but their main effect seems to be to extend the code
	     * path so much that things actually get a little slower
	     *
	    if (true == node->edge[1] && false == node->edge[0]) {
		put_node(true);
		put_node(false);
		return node;
	    }
	     *
	     *
	    if (node->ref == 1) {
		put_node(node->edge[1]);
		put_node(node->edge[0]);
		node->edge[1] = true;
		node->edge[0] = false;
		return node;
	    }
	     *
	     * Also note that put_node(node) has to move if enabling these two
	     * "optimizations".
	     */
	    return make_node(bit,true,false);
	}
    }
    return node;
}


static void sort_dag(NODE **root)
{
    NODE *sorted;

    sorted = sort_subdag(bits,*root);
    *root = sorted;
}


/* ----- Number actions ---------------------------------------------------- */


static void number_actions_in_dag(NODE *node,int *curr_num)
{
    if (!node || node->mark == curr_mark) return;
    node->mark = curr_mark;
    switch (node->bit->type) {
	case bt_data:
	    break;
	case bt_conform:
	case bt_count:
	    node->action = (*curr_num)++;
	    break;
	case bt_decision:
	    node->action = node->bit->action;
	    break;
	default:
	    abort();
    }
    number_actions_in_dag(node->edge[0],curr_num);
    number_actions_in_dag(node->edge[1],curr_num);
}


static void number_actions(NODE *root)
{
    int action_num = 0;
    BIT *bit;

    /* 
     * We mark terminals first so that we can later on assign preferred numbers
     * to them, like class IDs, or drop == 0
     */
    for (bit = bits; bit; bit = bit->next)
	if (bit->type != bt_data) bit->action = action_num++;
    new_mark();
    number_actions_in_dag(root,&action_num);
}


/* ----- Number leaf edges ------------------------------------------------- */


static struct edge {
    NODE *border;	/* border node; DO NOT DEREFERENCE */
    int edge;		/* edge to actions */
} *edges = NULL;

static int num_data_nodes; /* bt_data nodes */
static int num_action_peers; /* first-level action nodes */
static int num_leaf_edges; /* edges between data and action */


static void number_leaf_edges_in_dag(NODE *node)
{
    if (!node || node->mark == curr_mark) return;
    node->mark = curr_mark;
    if (node->bit->type != bt_data) {
	num_action_peers++;
	return;
    }
    num_data_nodes++;
    if (node->edge[0]->bit->type == bt_data) node->leaf[0] = 0;
    else node->leaf[0] = ++num_leaf_edges;
    if (node->edge[1]->bit->type == bt_data) node->leaf[1] = 0;
    else node->leaf[1] = ++num_leaf_edges;
    number_leaf_edges_in_dag(node->edge[0]);
    number_leaf_edges_in_dag(node->edge[1]);
}


static void register_leaf_edges_in_dag(NODE *node)
{
    if (!node || node->mark == curr_mark) return;
    if (node->bit->type != bt_data) return;
    node->mark = curr_mark;
    /*
     * We scribble over edges[0], but this is okay since the index is special,
     * and its entry is never accessed.
     */
    edges[node->leaf[0]].border = node;
    edges[node->leaf[0]].edge = 0;
    edges[node->leaf[1]].border = node;
    edges[node->leaf[1]].edge = 1;
    register_leaf_edges_in_dag(node->edge[0]);
    register_leaf_edges_in_dag(node->edge[1]);
}


static void number_leaf_edges(NODE *root)
{
    num_data_nodes = num_action_peers = num_leaf_edges = 0;
    new_mark();
    number_leaf_edges_in_dag(root);
    if (edges) free(edges);
    edges = alloc(sizeof(struct edge)*(num_leaf_edges+1));
    new_mark();
    register_leaf_edges_in_dag(root);
}


/* ----- Count leaf edge frequency ----------------------------------------- */


/*
 * We store nodes we need to visit in a list ordered by bit order. Since our
 * DAG is already sorted by bit order, processing nodes in this order ensures
 * that we never visit a node before all parent nodes have been visited.
 */

static struct count_todo {
    NODE *node;
    struct count_todo *next;
} *count_todo = NULL;


static void add_node_todo(NODE *node)
{
    struct count_todo **walk,*new;

    for (walk = &count_todo; *walk; walk = &(*walk)->next) {
	int cmp;

	if ((*walk)->node == node) return;
	cmp = compare_bit(node->bit,(*walk)->node->bit);
	if (cmp < 0) break; 
    }
    new = alloc_t(struct count_todo);
    new->node = node;
    new->next = *walk;
    *walk = new;
}


static NODE *get_node_todo(void)
{
    NODE *node;
    struct count_todo *next;

    if (!count_todo) return NULL;
    node = count_todo->node;
    next = count_todo->next;
    free(count_todo);
    count_todo = next;
    return node;
}


/*
 * Sets in_frequency of all bt_data nodes to the number of paths that would go
 * through that node if the node was a leaf. (I.e. for all leaf nodes, this is
 * the actual number of paths going through the node.)
 */


static void count_in_frequency(void)
{
    while (1) {
	NODE *node;

	node = get_node_todo();
	if (!node) break;
	debugf2("count_in_frequency node %p %d",node,node->in_frequency);
	if (node->edge[0] && node->edge[0]->bit->type == bt_data) {
	    node->edge[0]->in_frequency += node->in_frequency;
	    add_node_todo(node->edge[0]);
	}
	if (node->edge[1] && node->edge[1]->bit->type == bt_data) {
	    node->edge[1]->in_frequency += node->in_frequency;
	    add_node_todo(node->edge[1]);
	}
    }
}


/*
 * Sets the out_frequency array of all bt_data to the number of paths leaving
 * the node to the respective edge. The total number of paths to leaf i
 * traversing the node is therefore in_frequency*out_frequency[i]
 */


static void add_frequencies(const NODE *to,const NODE *from)
{
    int i;

    for (i = 0; i <= num_leaf_edges; i++)
	to->out_frequency[i] += from->out_frequency[i];
}


static void count_out_frequency(NODE *node)
{
    if (!node || node->mark == curr_mark) return;
    assert(node->bit->type == bt_data);
    assert(node->in_frequency);
    node->mark = curr_mark;
    node->out_frequency =
      alloc(sizeof(*node->out_frequency)*(num_leaf_edges+1));
    memset(node->out_frequency,0,
      sizeof(*node->out_frequency)*(num_leaf_edges+1));
    if (node->leaf[0]) node->out_frequency[node->leaf[0]]++;
    else {
	count_out_frequency(node->edge[0]);
	add_frequencies(node,node->edge[0]);
    }
    if (node->leaf[1]) node->out_frequency[node->leaf[1]]++;
    else {
	count_out_frequency(node->edge[1]);
	add_frequencies(node,node->edge[1]);
    }
}


static void count_frequencies(NODE *root)
{
    root->in_frequency = 1;
    if (root->bit->type == bt_data) {
	add_node_todo(root);
	count_in_frequency();
    }
    new_mark();
    count_out_frequency(root);
}


static void do_clear_frequency_counters(NODE *node)
{
    if (!node || node->mark == curr_mark) return;
    node->mark = curr_mark;
    node->in_frequency = 0;
    if (node->bit->type != bt_data) return;
    free(node->out_frequency);
    do_clear_frequency_counters(node->edge[0]);
    do_clear_frequency_counters(node->edge[1]);
}


static void clear_frequency_counters(NODE *root)
{
    new_mark();
    do_clear_frequency_counters(root);
}


/* ----- Dump frequency table ---------------------------------------------- */


static void do_dump_frequencies(FILE *file,NODE *node)
{
    int i;

    if (!node || node->mark == curr_mark) return;
    node->mark = curr_mark;
    fprintf(file,"%3d%6d",node->number,node->in_frequency);
    if (node->bit->type == bt_data)
	for (i = 1; i <= num_leaf_edges; i++)
	    fprintf(file,"%6d",node->out_frequency[i]);
    fprintf(file,"\n");
    do_dump_frequencies(file,node->edge[0]);
    do_dump_frequencies(file,node->edge[1]);
}


static void dump_frequencies(NODE *root,const char *label)
{
    int i;

    if (debug < 2) return;
    fprintf(stderr,"----- %s -----\n",label);
    fprintf(stderr,"BIT in___");
    for (i = 1; i <= num_leaf_edges; i++)
	fprintf(stderr,"%4d.%d",edges[i].border->number,edges[i].edge);
    fprintf(stderr,"\n");
    new_mark();
    do_dump_frequencies(stderr,root);
}


/* ----- Merge tails ------------------------------------------------------- */


/*
 * This is a fairly complex function. It works from the terminals towards the
 * root and combines nodes that reference the same sub-trees, e.g.
 *
 * (0:1)-->(0:2)-->C1
 *   |       |
 *   |       `---->C2
 *   v
 * (0:2)-->C1
 *   |
 *   `---->C2
 *
 * In a first step, it notices that the two nodes testing (0:2) are equivalent,
 * so it replaces them with a single instance, i.e.
 *
 * (0:1)-----.
 *   |       |
 *   |       v
 *   `---->(0:2)-->C1
 *           |
 *           `---->C2
 *
 * Afterwards, it checks whether, by combining the (0:2) nodes, it has found
 * a redundancy. If yes, it replaces all references to (0:1) by references to
 * (0:2), and repeats the same check with the former parents of (0:1).
 */


static void record_referer(NODE *referer,int edge)
{
    NODE *node = referer->edge[edge];
    struct referer *ref;

    if (!node) return;
    if (!node->referers) {
	node->referers = alloc(sizeof(struct referer)*node->ref);
	node->num_referers = 0;
    }
    ref = node->referers+node->num_referers++;
    ref->node = get_node(referer);
    ref->edge = edge;
    ref->anchor = referer->edge+edge;
}


static void record_referers(NODE *node)
{
    if (!node || node->mark == curr_mark) return;
    node->mark = curr_mark;
    record_referer(node,0);
    record_referer(node,1);
    record_referers(node->edge[0]);
    record_referers(node->edge[1]);
}


static void free_referer_slot(NODE *node,int referer)
{
    struct referer *ref = node->referers+referer;

    debugf2("free_referer_slot(node %p,referer %d)",node,referer);
    put_node(ref->node);
    ref->node = NULL;
    ref->anchor = NULL;
}


static void remove_referer(NODE *referer,int edge)
{
    NODE *node = referer->edge[edge];
    int i;

    debugf2("remove_referer(referer %p, edge %d, node %p)",referer,edge,node);
    if (!node) return; /* bt_count */
    for (i = 0; i < node->num_referers; i++) {
	struct referer *ref = node->referers+i;

	if (ref->node == referer && ref->edge == edge) {
	    free_referer_slot(node,i);
	    return;
	}
    }
    abort();
}


static void replace_node(NODE *old,NODE *new)
{
    int i;

    debugf2("replace_node(old %p,new %p)",old,new);
    get_node(old); /* avoid deallocation before last loop check */
    new->referers = realloc(new->referers,
      sizeof(struct referer)*(old->ref+new->ref));
    if (!new->referers) {
	perror("realloc");
	exit(1);
    }
    remove_referer(old,0);
    remove_referer(old,1);
    for (i = 0; i < old->num_referers; i++) {
	struct referer *ref = old->referers+i;

	debugf2("| %d: anchor %p",i,ref->anchor);
	if (ref->anchor) {
	    struct referer *ref2;

	    put_node(old);
	    *ref->anchor = get_node(new);
	    ref2 = new->referers+new->num_referers++;
	    *ref2 = *ref;
	    ref->node = NULL;
	    ref->anchor = NULL;
	    if (ref2->node && ref2->node->edge[0] == ref2->node->edge[1])
		replace_node(ref2->node,new);
	}
    }
    put_node(old);
}


static void consolidate_nodes(NODE *node);


static void consolidate_node(NODE *node,int referer)
{
    int edge = node->referers[referer].edge;
    NODE *other = node->referers[referer].node->edge[!edge];
      /* the inverse of us */
    int i;

    debugf2("consolidate_node(node %p,referer %d,other %p)",node,referer,other);
    if (other && (other->mark != curr_mark || !other->consolidated)) return;
      /* other node will do the consolidation */

    for (i = referer+1; i < node->num_referers; i++) {
	struct referer *ref1 = node->referers+referer;
	struct referer *ref2 = node->referers+i;

	debugf2("| %d: nodes %p, %p",i,ref1->node,ref2->node);
	assert(ref1->node != ref2->node);
	if (!ref2->node) continue; /* already removed @@@ ? */
	if (ref1->node->bit == ref2->node->bit && ref2->edge == edge &&
	  ref2->node->edge[!edge] == other) {
	   /*
	    * Not super-efficient: replace_node does not have to walk both
	    * lists of referers, because one is always known. Anyway, this
	    * keeps the code simpler.
	    */
	    replace_node(ref2->node,ref1->node);
	}
    }
}


static void consolidate_nodes(NODE *node)
{
    int i;

    debugf2("consolidate_nodes(node %p)",node);
    if (!node) return;
    if (node->mark == curr_mark) return;
    get_node(node); /* make sure child doesn't kill us */
    node->mark = curr_mark;
    node->consolidated = 0;
    consolidate_nodes(node->edge[0]);
    consolidate_nodes(node->edge[1]);
    node->consolidated = 1;

    /*
     * Since replace_node may realloc node->referers, we must not keep pointers
     * to node->referers+i across calls to consolidate_node.
     */
    for (i = 0; i < node->num_referers; i++) {
	debugf2("| %d: node %p",i,node->referers[i].node);
	if (node->referers[i].node) consolidate_node(node,i);
    }
    put_node(node);
}


static void remove_referers(NODE *node)
{
    struct referer *ref;

    if (!node || node->mark == curr_mark) return;
    node->mark = curr_mark;
    for (ref = node->referers; ref != node->referers+node->num_referers; ref++)
	if (ref->node) put_node(ref->node);
    free(node->referers);
    node->referers = NULL;
    remove_referers(node->edge[0]);
    remove_referers(node->edge[1]);
}


static void merge_tails(NODE **root)
{
    debugf("merge_tails(&root %p, root %p)",root,*root);
    new_mark();
    (*root)->referers = alloc_t(struct referer);
    (*root)->referers->node = NULL;
    (*root)->referers->edge = 0;
    (*root)->referers->anchor = root;
    (*root)->num_referers = 1;
    record_referers(*root);
    if (debug > 1) dump_dag(*root,"After record_referers");
    validate_references(*root);

    new_mark();
    consolidate_nodes(*root);
    if (debug > 1) dump_dag(*root,"After consolidate_nodes");
    validate_references(*root);

    new_mark();
    remove_referers(*root);
    validate_references(*root);
}


/* ----- Print out all paths with a give terminal edge --------------------- */


int rules;

struct prev {
    const NODE *node;
    int edge;
    const struct prev *prev;
};


static int print_path(FILE *file,const struct prev *last,
  const struct prev *curr,int length,int nibble)
{
    if (curr && curr->node->bit->u.data.offset_group ==
      last->node->bit->u.data.offset_group &&
      curr->node->bit->u.data.bit_num+length ==
      last->node->bit->u.data.bit_num) {
	int non_zero;

	non_zero = print_path(file,last,curr->prev,length+1,
	  (length & 3 ? nibble : 0) | curr->edge << (length & 3));
	non_zero = non_zero || nibble;
	/* print each full nibble we receive */
	if (!(length & 3) && (length <= 4 || non_zero))
	    fprintf(file,"%x",nibble);
	return non_zero;
    }
    else {
	if (curr) print_path(file,curr,curr->prev,1,curr->edge);
	fprintf(file," %d:%d:%d=0x",last->node->bit->u.data.offset_group,
	  last->node->bit->u.data.bit_num-length+1,length);
	if (length <= 4 || nibble) fprintf(file,"%x",nibble);
	return !!nibble;
    }
}


static int do_print_with_edge(FILE *file,const struct prev *prev,NODE *node,
  const NODE *last,int edge)
{
    struct prev this;

    if (node->mark == curr_mark) return 0;
    if (node->bit->type != bt_data) return 0;
    this.node = node;
    this.prev = prev;
    if (node == last) {
	this.edge = edge;
rules++;
	fprintf(file,"match");
	print_path(file,&this,prev,1,edge);
	fprintf(file," action %d\n",last->edge[edge]->action);
	return 1;
    }
    else {
	int more;

	this.edge = 0;
	more = do_print_with_edge(file,&this,node->edge[0],last,edge);
	this.edge = 1;
	if (do_print_with_edge(file,&this,node->edge[1],last,edge)) more = 1;
	if (!more) node->mark = curr_mark;
	return more;
    }
}


static void print_with_edge(FILE *file,NODE *root,const NODE *last,int edge)
{
    new_mark();
    if (!do_print_with_edge(file,NULL,root,last,edge)) abort();
}


/* ----- Select the best path for removal ---------------------------------- */


static void do_select_path(NODE *node,NODE **last,int *edge,double *quality)
{
    int i;

    if (!node || node->bit->type != bt_data || node->mark == curr_mark) return;
    node->mark = curr_mark;
    do_select_path(node->edge[0],last,edge,quality);
    do_select_path(node->edge[1],last,edge,quality);
    for (i = 1; i <= num_leaf_edges; i++) {
	if (node->out_frequency[i]) {
	    double q;
	    int out,j;

	    for (j = 1; j <= num_leaf_edges; j++)
		edges[j].border->edge[edges[j].edge]->bit->scratch = 0;
	    for (j = 1; j <= num_leaf_edges; j++)
		if (i != j)
		    edges[j].border->edge[edges[j].edge]->bit->scratch +=
		      node->out_frequency[j];
	    out = 0;
	    for (j = 1; j <= num_leaf_edges; j++)
		if (edges[j].border->edge[edges[j].edge]->bit->scratch > out)
		    out = edges[j].border->edge[edges[j].edge]->bit->scratch;
	    q = (double) node->in_frequency*
	      (double) (out-node->out_frequency[i]-1);
	    if (q > *quality) {
//fprintf(stderr,"SELECT: node %p, edge %d, q %f\n",node,i,(float) q);
		*quality = q;
		*last = edges[i].border;
		*edge = edges[i].edge;
	    }
	}
    }
//    assert(best_i);
    assert(node->in_frequency);
}
#if 0
static void do_select_path(NODE *node,NODE **last,int *edge,double *quality)
{
    double q;
    int i,best_i = 0,best_out = 1 << 30;
    int total = 0;

    if (!node || node->bit->type != bt_data || node->mark == curr_mark) return;
    node->mark = curr_mark;
    do_select_path(node->edge[0],last,edge,quality);
    do_select_path(node->edge[1],last,edge,quality);
    for (i = 1; i <= num_leaf_edges; i++)
	if (node->out_frequency[i]) {
	    int out,j;

	    out = 0;
	    total += node->out_frequency[i];
	    for (j = 1; j <= num_leaf_edges; j++)
		if (edges[i].border->edge[edges[i].edge]->bit ==
		  edges[j].border->edge[edges[j].edge]->bit)
		    out += node->out_frequency[j];
//	    if (out < best_out) {
//		best_out = out;
//	    out = node->out_frequency[i];
//	out = edges[i].border->in_frequency;
//fprintf(stderr,"%d*%d=%d\n",node->out_frequency[i],edges[i].border->in_frequency,out);
	    if (out < best_out) {
		best_out = out;
		best_i = i;
	    }
	}
    assert(best_i);
    assert(node->in_frequency);
//    q = (double) (total-best_out)/(double) best_out/log((double) node->in_frequency);
//    q = (double) total/((double) node->out_frequency[best_i]*
//     (double) edges[best_i].border->in_frequency);
//    q = (double) total/(double) best_out/log(node->in_frequency);
    q = (double) total/(double) best_out/sqrt(node->in_frequency);
//    q = (double) total/(double) best_out/(double) node->in_frequency;
//    q = (double) (total-best_out)/
//      ((double) best_out*(double) node->in_frequency);
#if 0
fprintf(stderr,"total %d, out[%d] %d, in %d = q %f\n",total,best_i,
  node->out_frequency[best_i],node->in_frequency,(float) q);
#endif
    if (q > *quality) {
	*quality = q;
	*last = edges[best_i].border;
	*edge = edges[best_i].edge;
    }
}
#endif


static void select_path(NODE *root,NODE **last,int *edge)
{
    double quality = -1e30;

#if 1
    new_mark();
    do_select_path(root,last,edge,&quality);
    debugf("do_select_path selected %p %d (%f)",*last,*edge,(float) quality);
#else
    while (!root->leaf[0]) root = root->edge[0];
    *last = root;
    *edge = 0;
#endif
}


/* ----- Remove an edge from the DAG and optimize the result --------------- */


static NODE *remove_edge(NODE *node,const NODE *last,int edge)
{
    if (node->bit->type != bt_data || node->mark == curr_mark) return node;
    if (node == last) {
	put_node(node->edge[edge]);
	node->edge[edge] = get_node(node->edge[1-edge]);
    }
    else {
	node->edge[0] = remove_edge(node->edge[0],last,edge);
	node->edge[1] = remove_edge(node->edge[1],last,edge);
    }
    if (node->edge[0] == node->edge[1]) {
	NODE *next;

	next = get_node(node->edge[0]);
	put_node(node);
	return next;
    }
    else {
	node->mark = curr_mark;
	return node;
    }
    
}


/* ----- Extract paths from DAG -------------------------------------------- */


static void extract_paths(FILE *file,NODE *root)
{
rules = 1;
    number_actions(root);
    while (root->bit->type == bt_data) {
	NODE *last;
	int edge;

	merge_tails(&root);
	dump_dag(root,"After merge_tails");
	validate_references(root);
print_timer("# tails merged");

	number_leaf_edges(root);
	  /* sigh, merge_tails and remove_edge change this */
	debugf("counts: %d data nodes, %d leaf edges, %d action peers",
	  num_data_nodes,num_leaf_edges,num_action_peers);
	count_frequencies(root);
	dump_frequencies(root,"After count_frequencies");
	select_path(root,&last,&edge);
	clear_frequency_counters(root);
	print_with_edge(file,root,last,edge);
	new_mark();
	root = remove_edge(root,last,edge);
	dump_dag(root,"After remove_edge (%p,%d)",last,edge);
#if 0
exit(1);
#endif
	validate_dag(root);
    }
    fprintf(file,"match action %d\n",root->action);
fprintf(stderr,"# %d rules !\n",rules);
}


/* ------------------------------------------------------------------------- */


void iflib_newbit(FILE *file,DATA d)
{
    NODE *dag;

    start_timer();
    dag = make_dag(d,NULL,NULL,0);
    dump_dag(dag,"After make_dag");
    validate_references(dag);

    sort_dag(&dag);
    dump_dag(dag,"After sort_dag");
    validate_dag(dag);

    print_timer("# new bit after sort_dag");
//    extract_paths(file,dag);
    extract_paths(stderr,dag);
    print_timer("# new bit done");
    exit(1);
}
