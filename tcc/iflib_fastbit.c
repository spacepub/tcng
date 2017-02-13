/*
 * iflib_fastbit.c - FSM based on single-bit decisions (even more improved
 *		     version)
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 Network Robots
 * Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
 */


#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "tree.h"
#include "field.h"
#include "iflib.h"


typedef uint32_t NUMBER;

typedef struct _bit {
    NUMBER number;
    int mark;
    int ref;
    int index;
    struct _bit *next[2];
} BIT;


#define TAG_MASK	0xf0000000
#define VALUE_MASK	0x0fffffff

#define METER_TAG	0x40000000
#define DECISION_TAG	0x80000000
#define DROP_TAG	(DECISION_TAG | 0x00000000)
#define CLASS_TAG	(DECISION_TAG | 0x10000000)
#define RECLASSIFY_TAG	(DECISION_TAG | 0x20000000)
#define CONTINUE_TAG	(DECISION_TAG | 0x30000000)

#define BIT_TO_NUMBER(offset_group,offset) \
  (((offset_group) << 16) | (offset))
#define CONFORM_TO_NUMBER(bucket) \
  (METER_TAG | (bucket << 1))
#define COUNT_TO_NUMBER(bucket) \
  (METER_TAG | (bucket << 1) | 1)
#define DECISION_TO_NUMBER(dec) \
  ((dec).result == dr_drop ? DROP_TAG : \
   (dec).result == dr_continue ? CONTINUE_TAG : \
   (dec).result == dr_class ? CLASS_TAG | (dec).class->number : \
   RECLASSIFY_TAG | (dec).class->number)


static int curr_mark = 1;
static int nodes = 0;

/* ----- */


static BIT *make_bit(unsigned long number,BIT *true,BIT *false)
{
    BIT *bit = alloc_t(BIT);

    bit->number = number;
    bit->mark = 0;
    bit->ref = 1;
    bit->index = -1;
    bit->next[0] = false;
    bit->next[1] = true;
    nodes++;
    return bit;
}


static BIT *get_bit(BIT *bit)
{
    if (bit) bit->ref++;
    return bit;
}


static void put_bit(BIT *bit)
{
    if (bit && !--bit->ref) {
	put_bit(bit->next[0]);
	put_bit(bit->next[1]);
	nodes--;
	free(bit);
    }
}


/* ----- DAG construction -------------------------------------------------- */


extern int get_offset_group(DATA d,int base); /* borrowing from if_ext.c */


static BIT *value_dag(int offset,uint32_t value,uint32_t mask,
  int length,BIT *true,BIT *false,int offset_group)
{
    BIT *last;
    int i;

    last = true;
    for (i = 0; i < length; i++)
	if ((mask >> i) & 1) {
	    NUMBER number = BIT_TO_NUMBER(offset_group,length-i-1+offset*8);

	    last = (value >> i) & 1 ? make_bit(number,last,get_bit(false)) :
	      make_bit(number,get_bit(false),last);
	}
    put_bit(false);
    return last;
}


static BIT *eq_dag(DATA a,DATA b,BIT *true,BIT *false,int offset_group)
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
	    put_bit(false);
	    return true;
	}
	else {
	    put_bit(true);
	    return false;
	}
    }
    return value_dag(a.op->b.u.unum,value & value_mask,field_mask,
      a.op->c.u.unum,true,false,offset_group);
}


static BIT *make_dag(DATA d,BIT *true,BIT *false,int offset_group)
{
    if (d.op) {
	if (d.op->dsc == &op_logical_or)
	    return make_dag(d.op->a,true,
	      make_dag(d.op->b,get_bit(true),false,offset_group),offset_group);
	if (d.op->dsc == &op_logical_and)
	    return make_dag(d.op->a,
	      make_dag(d.op->b,true,get_bit(false),offset_group),
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
	    put_bit(false);
	    return make_bit(COUNT_TO_NUMBER(d.op->a.u.police->number),true,
	      NULL);
	}
	if (true == false) {
	    put_bit(false);
	    return true;
	}
	if (d.op->dsc == &op_eq)
	    return eq_dag(d.op->a,d.op->b,true,false,offset_group);
	if (d.op->dsc == &op_conform)
	    return make_bit(CONFORM_TO_NUMBER(d.op->a.u.police->number),true,
	      false);
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
	    return make_bit(DECISION_TO_NUMBER(d.u.decision),NULL,NULL);
	default:
	    dump_failed(d,"bad terminal");
    }
    abort(); /* not reached */
}


/* ----- Debugging output -------------------------------------------------- */


static int index;


static void do_dump_dag(FILE *file,BIT *bit)
{
    if (!bit || bit->index != -1) return;
    do_dump_dag(file,bit->next[0]);
    do_dump_dag(file,bit->next[1]);
    bit->index = index++;
    fprintf(file,"%5d: 0x%08x %5d %5d\n",bit->index,bit->number,
      bit->next[0] ? bit->next[0]->index : -1,
      bit->next[1] ? bit->next[1]->index : -1);
}


static void reset_index(BIT *bit)
{
    if (!bit || bit->index == -1) return;
    bit->index = -1;
    reset_index(bit->next[0]);
    reset_index(bit->next[1]);
}


static void dump_dag(FILE *file,BIT *dag)
{
    index = 0;
    do_dump_dag(file,dag);
    reset_index(dag);
}


/* ----- */

static void do_verify_dag(BIT *bit)
{
    if (!bit || bit->index != -1) return;
assert(!bit->next[0] || bit->number <= bit->next[0]->number);
assert(!bit->next[1] || bit->number <= bit->next[1]->number);
    do_verify_dag(bit->next[0]);
    do_verify_dag(bit->next[1]);
    bit->index = index++;
}


static void verify_dag(BIT *dag)
{
    index = 0;
    do_verify_dag(dag);
    reset_index(dag);
}

/* ----- Graphical debugging output ---------------------------------------- */


static void print_pad(FILE *file,unsigned n,char pad,int len)
{
    if (n > 10) print_pad(file,n/10,pad,len-1);
    else {
	while (len -- > 1) fputc(pad,file);
    }
    fputc('0'+(n % 10),file);
}


static void dag_2d(FILE *file,BIT *bit,int level,int first)
{
    int i;

    if (!bit) return;
    if (bit->index == -1) bit->index = index++;
    print_pad(file,bit->index,first ? '-' : ' ',3);
    fputc(':',file);
    switch (bit->number & TAG_MASK) {
	case 0:
	    print_pad(file,bit->number & VALUE_MASK,'_',3);
	    break;
	case METER_TAG:
	    abort(); /* not yet supported @@@ */
	case DROP_TAG:
	    fprintf(file,"drp");
	    break;
	case CLASS_TAG:
	    fprintf(file,"c");
	    print_pad(file,bit->number & VALUE_MASK,'_',2);
	    break;
	case RECLASSIFY_TAG:
	    fprintf(file,"r");
	    print_pad(file,bit->number & VALUE_MASK,'_',2);
	    break;
	case CONTINUE_TAG:
	    fprintf(file,"uns");
	    break;
	default:
	    abort();
    }
    if (!bit->next[1]) fprintf(file,"\n");
    else {
	fprintf(file,"-");
	dag_2d(file,bit->next[1],level+1,1);
    }
    if (bit->next[0]) {
	for (i = 0; i < level; i++) fprintf(file,"   |    ");
	fprintf(file,"   |\n");
	for (i = 0; i < level; i++) fprintf(file,"   |    ");
	dag_2d(file,bit->next[0],level,0);
    }
}


static void dump_dag_2d(FILE *file,BIT *dag)
{
    index = 0;
    dag_2d(file,dag,0,1);
    reset_index(dag);
}


/* ---- Sorting ------------------------------------------------------------ */


static void fix_edge(BIT *bit,int edge,NUMBER border,NUMBER sorted)
{
    BIT *clone[2];
    int i,j;

fprintf(stderr,"  fix_edge(bit %p[%x],edge %d)\n",bit,bit->number,edge);
    /* FIXME: this is verboten if bit->number as a side-effect */
    while (bit->number == bit->next[edge]->number) {
	BIT *next = bit->next[edge];

	bit->next[edge] = get_bit(next->next[edge]);
	put_bit(next);
	if (!bit->next[edge]) return;
    }
fprintf(stderr,"  from %p[%x](check %p[%x], border %x, sorted %x)\n",
  bit,bit->number,bit->next[edge],
  bit->next[edge]->number,border,sorted);
    if ((bit->number^bit->next[edge]->number) & sorted) return;
      /* link points to different partition */
    if (!(bit->number & border)) return;
      /* we're in the lower part */
    if (bit->next[edge]->number & border) return;
      /* next is in the higher part */

fprintf(stderr,"    next %p[%x]\n",bit->next[edge],bit->next[edge]->number);
    for (i = 0; i < 2; i++) {
	if (bit->next[edge]->next[i] == bit->next[1-edge]) {
fprintf(stderr,"      quick clone (%d)\n",i);
	    clone[i] = get_bit(bit->next[1-edge]);
	    //continue;
	}
	else {
	    /* swap bits */
	    clone[i] = alloc_t(BIT);
fprintf(stderr,"      slow clone (new[%d] %p)\n",i,clone[i]);
	    nodes++;
	    *clone[i] = *bit;
	    clone[i]->ref = 1;
	    clone[i]->next[edge] = get_bit(bit->next[edge]->next[i]);
	    clone[i]->next[1-edge] = get_bit(bit->next[1-edge]);
	}
//fprintf("--- after cloning ---\n");
//dump_dag_2d(stdout,dag);
	/* fix anything we broke by swapping */
	for (j = 0; j < 2; j++)
	    if (clone[i]->next[j] &&
	      !((bit->number^clone[i]->number) & sorted) &&
	      !((bit->number^clone[i]->next[j]->number) & sorted))
		fix_edge(clone[i],j,border,sorted);
//fprintf("--- after fixing ---\n");
//dump_dag_2d(stdout,dag);
    }
    bit->number = bit->next[edge]->number;
    bit->mark = bit->next[edge]->mark;
    put_bit(bit->next[0]);
    put_bit(bit->next[1]);
    bit->next[0] = clone[0];
    bit->next[1] = clone[1];
    for (j = 0; j < 2; j++)
	while (bit->number == bit->next[j]->number) {
	    BIT *next = bit->next[j];

	    bit->next[j] = get_bit(next->next[j]);
	    put_bit(next);
	    if (!bit->next[j]) return;
	}
//fprintf("--- end of fixing ---\n");
//dump_dag_2d(stdout,dag);
}


static void quick_dag(BIT *bit,NUMBER border,NUMBER sorted)
{
    int i;

    if (bit->mark == curr_mark) return;
    bit->mark = curr_mark;
fprintf(stderr,"quick_dag(bit %p[%x])\n",bit,bit->number);
#if 0
    for (i = 0; i < 2; i++)
	if (bit->next[i]) {
	    quick_dag(bit->next[i],border,sorted);
	    fix_edge(bit,i,border,sorted);
#endif
    for (i = 0; i < 2; i++)
	if (bit->next[i]) {
	    quick_dag(bit->next[i],border,sorted);
	}
    for (i = 0; i < 2; i++)
	if (bit->next[i]) {
	    fix_edge(bit,i,border,sorted);
    }
}


static void sort_dag(BIT *dag)
{
    int i;

    for (i = 31; i >= 0; i--) {
	print_timer("# iteration");
	printf("---- i = %d, %d nodes\n",i,nodes);
	dump_dag_2d(stdout,dag);
	quick_dag(dag,1 << i,0xfffffffe << i);
	curr_mark = !curr_mark;
    }
}


/* ----- Optimize DAG ------------------------------------------------------ */


// static void optimize_dag(


/* ------------------------------------------------------------------------- */


void iflib_fastbit(FILE *file,DATA d)
{
    BIT *dag;

    start_timer();
    dag = make_dag(d,NULL,NULL,0);
    print_timer("# after make_dag");
    dump_dag_2d(stdout,dag);
    sort_dag(dag);
    print_timer("# after sort_dag");
    dump_dag_2d(stdout,dag);
    verify_dag(dag);
    printf("---- %d nodes\n",nodes);
}
