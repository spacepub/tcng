/*
 * op.c - Operators
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <u128.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "op.h"


#define UNTYPED_NUMBER(d) \
  ((d).type == dt_unum || (d).type == dt_ipv4 || (d).type == dt_ipv6 || \
   (d).type == dt_fnum)
#define INTEGER_NUMBER(d) \
  ((d).type == dt_unum || (d).type == dt_ipv4 || (d).type == dt_ipv6)
#define IP_ADDRESS(d) ((d).type == dt_ipv4 || (d).type == dt_ipv6)
#define TYPED_NUMBER(d) \
  ((d).type == dt_rate || (d).type == dt_prate || \
   (d).type == dt_size || (d).type == dt_psize || (d).type == dt_time)
#define TYPES2(a,b,t1,t2) \
  (((a).type == (t1) && (b).type == (t2)) || \
   ((a).type == (t2) && (b).type == (t1)))

#define __OP(type,op,c_op) \
  (data_convert(data_##type(data_convert(op->a,dt_##type).u.type c_op \
  data_convert(op->b,dt_##type).u.type),mix_types(op)))
#define UNUM_OP(op,c_op) __OP(unum,op,c_op)
#define FNUM_OP(op,c_op) __OP(fnum,op,c_op)
#define ANYNUM_OP(op,c_op) \
  (all_u32(op->a,op->b) ? UNUM_OP(op,c_op) : FNUM_OP(op,c_op))


/* ----- Helper functions -------------------------------------------------- */


static DATA make_op(OP *op,DATA_TYPE type)
{
    DATA d;

    d.type = type;
    d.op = op;
    return d;
}


static void __attribute__((noreturn)) type_error1(const OP *op)
{
    yyerrorf("invalid type for operator \"%s\" (%s)",op->dsc->name,
      type_name(op->a.type));
}


static void __attribute__((noreturn)) type_error2(const OP *op)
{
    yyerrorf("invalid types for operator \"%s\" (%s and %s)",op->dsc->name,
      type_name(op->a.type),type_name(op->b.type));
}


static void __attribute__((noreturn)) type_error3(const OP *op)
{
    yyerrorf("invalid types for operator \"%s\" (%s, %s, and %s)",op->dsc->name,
      type_name(op->a.type),type_name(op->b.type),type_name(op->c.type));
}


static int all_u32(DATA a,DATA b)
{
    return (a.type == dt_unum || a.type == dt_ipv4) &&
      (b.type == dt_unum || b.type == dt_ipv4);
}


static DATA_TYPE mix_types(const OP *op)
{
    DATA_TYPE a = op->a.type;
    DATA_TYPE b = op->b.type;

    if (a == b) return a;
    if (a == dt_unum) return b;
    if (b == dt_unum) return a;
    type_error2(op);
}


/* ----- +,- --------------------------------------------------------------- */


static DATA additive_eval(OP *op)
{
    int plus = *op->dsc->name == '+';
    DATA d;

    if (UNTYPED_NUMBER(op->a) &&
      op->b.type == dt_unum && !op->b.op && !op->b.u.unum)
	return op->a;
    if (plus && UNTYPED_NUMBER(op->b) &&
      op->a.type == dt_unum && !op->a.op && !op->a.u.unum)
	return op->b;
    if (UNTYPED_NUMBER(op->a) && UNTYPED_NUMBER(op->b)) {
	if (op->a.op || op->b.op) return make_op(op,mix_types(op));
	if (plus) {
	    if (op->a.type == dt_ipv6 || op->b.type == dt_ipv6)
		return data_ipv6(u128_add(data_convert(op->a,dt_ipv6).u.u128,
		  data_convert(op->b,dt_ipv6).u.u128));
	    return ANYNUM_OP(op,+);
	}
	else {
	    if (op->a.type == dt_ipv6 || op->b.type == dt_ipv6)
		return data_ipv6(u128_sub(data_convert(op->a,dt_ipv6).u.u128,
		  data_convert(op->b,dt_ipv6).u.u128));
	    return ANYNUM_OP(op,-);
	}
    }
    if (op->a.type == op->b.type && TYPED_NUMBER(op->a)) {
	if (op->a.op || op->b.op) return make_op(op,op->a.type);
	d = op->a;
	if (plus) d.u.fnum += op->b.u.fnum;
	else d.u.fnum -= op->b.u.fnum;
	return d;
    }
    type_error2(op);
}


static DATA plus_eval(OP *op)
{
    if (op->a.type != dt_string || op->b.type != dt_string)
	return additive_eval(op);
    if (op->a.op || op->b.op) yyerror("string expressions must be constant");
    return data_string(strcat(strcpy(
      alloc(strlen(op->a.u.string)+strlen(op->b.u.string)+1),
      op->a.u.string),op->b.u.string));

}


OP_DSC op_plus = {
    .name = "+",
    .eval = plus_eval,
};

OP_DSC op_minus = {
    .name = "-",
    .eval = additive_eval,
};


/* ----- * ----------------------------------------------------------------- */


static DATA mult_eval(OP *op)
{
    DATA d;

    if (IP_ADDRESS(op->a) || IP_ADDRESS(op->b)) type_error2(op);
    if (UNTYPED_NUMBER(op->a) && UNTYPED_NUMBER(op->b)) {
	if (op->a.op || op->b.op) return make_op(op,mix_types(op));
	if (op->a.type == dt_unum && op->b.type == dt_unum &&
	  0xffffffffU/op->a.u.unum < op->b.u.unum)
	    yyerrorf("%lu*%lu exceeds 32 bit range",
	      (unsigned long) op->a.u.unum,(unsigned long) op->b.u.unum);
	return ANYNUM_OP(op,*);
    }
    if ((UNTYPED_NUMBER(op->a) && TYPED_NUMBER(op->b)) ||
      (UNTYPED_NUMBER(op->b) && TYPED_NUMBER(op->a))) {
	if (op->a.op || op->b.op)
	    return make_op(op,TYPED_NUMBER(op->a) ? op->a.type : op->b.type);
	if (TYPED_NUMBER(op->a)) {
	    d = op->a;
	    d.u.fnum *= data_convert(op->b,dt_fnum).u.fnum;
	}
	else {
	    d = op->b;
	    d.u.fnum *= data_convert(op->a,dt_fnum).u.fnum;
	}
	return d;
    }
    d.type = dt_none;
    d.u.fnum = op->a.u.fnum;
    if (TYPES2(op->a,op->b,dt_rate,dt_time)) {
	d.type = dt_size;
	d.u.fnum /= 8.0;
    }
    if (TYPES2(op->a,op->b,dt_prate,dt_time)) d.type = dt_psize;
#if 0 /* maybe later */
    if (TYPES2(op->a,op->b,dt_inv_rate,dt_rate)) d.type = dt_time;
    if (TYPES2(op->a,op->b,dt_inv_prate,dt_prate)) d.type = dt_time;
#endif
    if (d.type == dt_none) type_error2(op);
    if (op->a.op || op->b.op) return make_op(op,d.type);
    d.op = NULL;
    d.u.fnum *= op->b.u.fnum;
    return d;
}


OP_DSC op_mult = {
    .name = "*",
    .eval = mult_eval,
};


/* ----- / ----------------------------------------------------------------- */


static DATA mask_eval(OP *op);


static DATA div_eval(OP *op)
{
    DATA d;

    if (IP_ADDRESS(op->a) && op->b.type == dt_unum) {
	op->dsc = &op_mask;
	return mask_eval(op);
    }
    if (IP_ADDRESS(op->a) || IP_ADDRESS(op->b)) type_error2(op);
    if (UNTYPED_NUMBER(op->a) && UNTYPED_NUMBER(op->b)) {
	if (op->a.op || op->b.op) return make_op(op,mix_types(op));
	return ANYNUM_OP(op,/);
    }
    if (TYPED_NUMBER(op->a) && UNTYPED_NUMBER(op->b)) {
	if (op->a.op || op->b.op) return make_op(op,op->a.type);
	d = op->a;
	d.u.fnum /= data_convert(op->b,dt_fnum).u.fnum;
	return d;
    }
    d.type = dt_none;
    d.u.fnum = op->a.u.fnum;
    if (op->a.type == dt_size && op->b.type == dt_time) {
	d.type = dt_rate;
	d.u.fnum *= 8.0;
    }
    if (op->a.type == dt_psize && op->b.type == dt_time) d.type = dt_prate;
#if 0 /* maybe later */
    if (TYPES2(op->a,op->b,dt_inv_rate,dt_rate)) d.type = dt_time;
    if (TYPES2(op->a,op->b,dt_inv_prate,dt_prate)) d.type = dt_time;
#endif
    if (d.type == dt_none) type_error2(op);
    if (op->a.op || op->b.op) return make_op(op,d.type);
    d.op = NULL;
    d.u.fnum /= op->b.u.fnum;
    return d;
}


OP_DSC op_div = {
    .name = "/",
    .eval = div_eval,
};


/* ----- ~,!,- ------------------------------------------------------------- */


static DATA unum1_eval(OP *op)
{
    DATA d;

    if (!UNTYPED_NUMBER(op->a) && !TYPED_NUMBER(op->a)) type_error1(op);
    if (!INTEGER_NUMBER(op->a) && *op->dsc->name != '-') type_error1(op);
    if (op->a.op) return make_op(op,op->a.type);
    /* @@@ convert fnum to unum ? */
    switch (*op->dsc->name) {
	case '~':
	    switch (op->a.type) {
		case dt_unum:
		    return data_unum(~op->a.u.unum);
		case dt_ipv4:
		    return data_ipv4(~op->a.u.unum);
		case dt_ipv6:
		    d = op->a;
		    d.u.u128 = u128_not(d.u.u128);
		    return d;
		default:
		    abort();
	    }
	case '!':
	    if (op->a.type == dt_ipv6)
		return data_unum(u128_is_zero(op->a.u.u128));
	    return data_unum(!op->a.u.unum);
	case '-':
	    switch (op->a.type) {
		case dt_unum:
		    return data_unum(-op->a.u.unum);
		case dt_ipv4:
		    return data_ipv4(-op->a.u.unum);
		case dt_ipv6:
		    d = op->a;
		    d.u.u128 = u128_minus(d.u.u128);
		    return d;
		default:
		    d = op->a;
		    d.u.fnum = -d.u.fnum;
		    return d;
	    }
	    /* not reached */
	default:
	    abort();
    }
}


OP_DSC op_not = {
    .name = "~",
    .eval = unum1_eval,
};


OP_DSC op_logical_not = {
    .name = "!",
    .eval = unum1_eval,
};


OP_DSC op_unary_minus = {
    .name = "-",
    .eval = unum1_eval,
};


/* ----- %, &, |, ^ -------------------------------------------------------- */


/* Macros are fun ;-) */


static DATA unum2_eval(OP *op);


#define UNUM2_OPS(m) \
  m(mod,%) \
  m(and,&) m(or,|) m(xor,^)

#define _DECLARATION(n,op) \
  static const char op_name_##n[] = #op; \
  OP_DSC op_##n = { \
    .name = op_name_##n, \
    .eval = unum2_eval, \
  };

UNUM2_OPS(_DECLARATION)

#undef _DECLARATION


#define _EVALUATION(n,op) \
  if (name == op_name_##n) return a op b;

static uint32_t unum2_op(const char *name,uint32_t a,uint32_t b)
{
    /*
     * According to K&R 2nd ed., I should be able to use switch here, but
     * gcc 2.91.66 / egcs 1.1.2 won't let me.
     */
    UNUM2_OPS(_EVALUATION)
    abort();
}

#undef _EVALUATION


static DATA unum2_eval(OP *op)
{
    U128 a,b;

    if (!INTEGER_NUMBER(op->a)) type_error2(op);
    if (!INTEGER_NUMBER(op->b)) type_error2(op);
    if ((IP_ADDRESS(op->a) || IP_ADDRESS(op->b)) && *op->dsc->name == '%')
	type_error2(op);
    if (op->a.op || op->b.op) return make_op(op,mix_types(op));
    /* @@@ convert fnum to unum ? */
    switch (mix_types(op)) {
	case dt_unum:
	    return data_unum(unum2_op(op->dsc->name,op->a.u.unum,op->b.u.unum));
	case dt_ipv4:
	    return data_ipv4(unum2_op(op->dsc->name,op->a.u.unum,op->b.u.unum));
	case dt_ipv6:
	    a = data_convert(op->a,dt_ipv6).u.u128;
	    b = data_convert(op->b,dt_ipv6).u.u128;
	    switch (*op->dsc->name) {
		case '&':
		    return data_ipv6(u128_and(a,b));
		case '|':
		    return data_ipv6(u128_or(a,b));
		case '^':
		    return data_ipv6(u128_xor(a,b));
		default:
		    abort();
	    }
	default:
	    abort();
    }
}


/* ----- <<, >> ------------------------------------------------------------ */


static DATA shift_eval(OP *op)
{
    if (!INTEGER_NUMBER(op->a)) type_error2(op);
    if (!INTEGER_NUMBER(op->b)) type_error2(op);
    if (IP_ADDRESS(op->b)) type_error2(op);
    if (op->a.op || op->b.op) return make_op(op,op->a.type);
    /* @@@ convert fnum to unum ? */
    switch (*op->dsc->name) {
	case '<':
	    switch (op->a.type) {
		case dt_unum:
		case dt_ipv4:
		    if ((op->b.u.unum >= 32 && op->a.u.unum) ||
		      (op->b.u.unum && op->a.u.unum >> (32-op->b.u.unum)))
			yyerror("left shift overflows 32 bit range");
		    if (op->a.type == dt_unum)
			return data_unum(op->a.u.unum << op->b.u.unum);
		    else return data_ipv4(op->a.u.unum << op->b.u.unum);
		case dt_ipv6:
		    if ((op->b.u.unum >= 128 && !u128_is_zero(op->a.u.u128)) ||
		      !u128_is_zero(u128_shift_right(op->a.u.u128,
		      128-op->b.u.unum)))
			yyerror("left shift overflows 128 bit range");
		    return data_ipv6(
		      u128_shift_left(op->a.u.u128,op->b.u.unum));
		default:
		    abort();
	    }
	case '>':
	    switch (op->a.type) {
		case dt_unum:
		    return data_unum(
		      op->b.u.unum >= 32 ? 0 : op->a.u.unum >> op->b.u.unum);
		case dt_ipv4:
		    return data_ipv4(
		      op->b.u.unum >= 32 ? 0 : op->a.u.unum >> op->b.u.unum);
		case dt_ipv6:
		    return data_ipv6(
		      u128_shift_right(op->a.u.u128,op->b.u.unum));
		default:
		    abort();
	    }
	default:
	    abort();
    }
}


OP_DSC op_shift_left = {
    .name = "<<",
    .eval = shift_eval,
};


OP_DSC op_shift_right = {
    .name = ">>",
    .eval = shift_eval,
};


/* ----- ||, && ------------------------------------------------------------ */


static DATA and_or_eval(OP *op)
{
    uint32_t a,b;

    /*
     * use classes for short-cut evaluation in "if"
     */
    /* @@@ convert fnum to unum ? */
    if (!INTEGER_NUMBER(op->a) && op->a.type != dt_decision) type_error2(op);
    if (!INTEGER_NUMBER(op->b) && op->b.type != dt_decision) type_error2(op);

    /*
     * All short-cuts are detected and removed later, so we don't need to
     * worry about them here.
     */
    if (op->a.type == dt_decision || op->b.type == dt_decision)
	return make_op(op,dt_unum);
    if (op->a.op || op->b.op) return make_op(op,dt_unum);
    a = op->a.type != dt_ipv6 ? op->a.u.unum : !u128_is_zero(op->a.u.u128);
    b = op->b.type != dt_ipv6 ? op->b.u.unum : !u128_is_zero(op->b.u.u128);
    if (*op->dsc->name == '&') return data_unum(a && b);
    else return data_unum(a || b);
}


OP_DSC op_logical_and = {
    .name = "&&",
    .eval = and_or_eval,
};


OP_DSC op_logical_or = {
    .name = "||",
    .eval = and_or_eval,
};


/* ----- ==, !=, >, >=, <, <= ---------------------------------------------- */


static DATA rel_eval(OP *op);


#define REL_OPS(m) \
  m(eq,==) m(ne,!=) m(gt,>) m(ge,>=) m(lt,<) m(le,<=)

#define _DECLARATION(n,op) \
  static const char op_name_##n[] = #op; \
  OP_DSC op_##n = { \
    .name = op_name_##n, \
    .eval = rel_eval, \
  };

REL_OPS(_DECLARATION)

#undef _DECLARATION


#define _EVALUATION(n,op) \
  if (name == op_name_##n) return a op b;

static uint32_t rel_unum_op(const char *name,uint32_t a,uint32_t b)
{
    REL_OPS(_EVALUATION)
    abort();
}


static uint32_t rel_fnum_op(const char *name,double a,double b)
{
    REL_OPS(_EVALUATION)
    abort();
}

#undef _EVALUATION


static int eval_cmp(const char *op,int cmp)
{
    if (cmp < 0) return *op == '<' || *op == '!';
    if (cmp > 0) return *op == '>' || *op == '!';
    return *op == '=' || (op[0] != '!' && op[1] == '=');
}


static DATA rel_eval(OP *op)
{
    if (UNTYPED_NUMBER(op->a) && UNTYPED_NUMBER(op->b)) {
	DATA_TYPE type;

	if (op->a.op || op->b.op) return make_op(op,dt_unum);
	type = mix_types(op);
	switch (type) {
	    case dt_unum:
	    case dt_ipv4:
		return data_unum(rel_unum_op(op->dsc->name,
		  op->a.u.unum,op->b.u.unum));
	    case dt_ipv6:
		return data_unum(eval_cmp(op->dsc->name,
		  u128_cmp(data_convert(op->a,dt_ipv6).u.u128,
		  data_convert(op->b,dt_ipv6).u.u128)));
	    case dt_fnum:
		return data_unum(rel_fnum_op(op->dsc->name,
		  data_convert(op->a,dt_fnum).u.fnum,
		  data_convert(op->b,dt_fnum).u.fnum));
	    default:
		abort();
	}
    }
    if (op->a.type == dt_string && op->b.type == dt_string) {
	if (op->a.op || op->b.op)
	    yyerror("string expressions must be constant");
	return data_unum(eval_cmp(op->dsc->name,
	  strcmp(op->a.u.string,op->b.u.string)));
    }
    if (op->a.type != op->b.type || !TYPED_NUMBER(op->a)) type_error2(op);
    if (op->a.op || op->b.op) return make_op(op,dt_unum);
    return data_unum(rel_fnum_op(op->dsc->name,op->a.u.fnum,op->b.u.fnum));
}


/* ----- : : --------------------------------------------------------------- */


static DATA mask_eval(OP *op)
{
    int upper,bits;

    if (!INTEGER_NUMBER(op->a) ||
      (op->b.type != dt_unum && op->b.type != dt_none) ||
      (op->c.type != dt_unum && op->c.type != dt_none))
	type_error3(op);
    if (op->b.op || op->c.op) return make_op(op,op->a.type);
    bits = (op->b.type == dt_none ? 0 : op->b.u.unum)+
      (op->c.type == dt_none ? 0 : op->c.u.unum);
    upper = (op->c.type == dt_unum ? op->c : op->b).u.unum;
    if (op->a.type == dt_ipv6) {
	U128 mask;

	if (bits > 128) yyerror("mask exceeds 128 bit range");
	mask = u128_sub_32(u128_shift_left(u128_from_32(1),upper),1);
	if (op->b.type == dt_unum) mask = u128_shift_left(mask,128-bits);
	return op_binary(&op_and,op->a,data_ipv6(mask));
    }
    else {
	uint32_t mask;

	if (bits > 32) yyerror("mask exceeds 32 bit range");
	mask = upper == 32 ? 0xffffffff : (1 << upper)-1;
	if (op->b.type == dt_unum) mask <<= 32-bits;
	return op_binary(&op_and,op->a,data_unum(mask));
    }
}


OP_DSC op_mask = {
    .name = "mask (::)",
    .eval = mask_eval,
};


/* ----- Packet data access ------------------------------------------------ */


static DATA access_eval(OP *op)
{
    /* type checking and ajustment (dt_ipv4, dt_ipv6) done by parser */
    return make_op(op,dt_unum);
}


OP_DSC op_access = {
    .name = "access",
    .eval = access_eval,
};


static DATA offset_eval(OP *op)
{
    /* only used internally - no type checking needed */
    return make_op(op,op->a.type);
}


OP_DSC op_offset = {
    .name = "offset",
    .eval = offset_eval,
};


/* ----- Policing ---------------------------------------------------------- */


static DATA conform_eval(OP *op)
{
    if (op->a.type != dt_police && op->a.type != dt_bucket) type_error1(op);
	/* @@@ */
    return make_op(op,dt_unum);
}


OP_DSC op_conform = {
    .name = "conform",
    .eval = conform_eval,
};


static DATA count_eval(OP *op)
{
    if (op->a.type != dt_police && op->a.type != dt_bucket) type_error1(op);
	/* @@@ */
    return make_op(op,dt_unum);
}


OP_DSC op_count = {
    .name = "count",
    .eval = count_eval,
};


/* ----- Preconditions ----------------------------------------------------- */


static DATA precond_eval(OP *op)
{
    return make_op(op,dt_unum);
}


OP_DSC op_precond = {
    .name = "precond",
    .eval = precond_eval,
};


static DATA precond2_eval(OP *op)
{
    /* only used internally - no type checking needed */
    return make_op(op,op->a.type);
}


OP_DSC op_precond2 = {
    .name = "precond2",
    .eval = precond2_eval,
};


/* ----- Field roots ------------------------------------------------------- */


static DATA field_root_eval(OP *op)
{
    /* only used internally - no type checking needed */
    return make_op(op,op->a.type);
}


OP_DSC op_field_root= {
    .name = "field_root",
    .eval = field_root_eval,
};


/* ----- Operator construction --------------------------------------------- */


static OP *alloc_op(void)
{
    OP *op = alloc_t(OP);

    memset(op,0,sizeof(*op));
    return op;
}


static DATA eval_op(OP *op)
{
    DATA d;

    d = op->dsc->eval(op);
    if (!d.op) free(op);
    return d;
}


DATA op_unary(const OP_DSC *dsc,DATA a)
{
    OP *op = alloc_op();

    op->dsc = dsc;
    op->a = a;
    return eval_op(op);
}


DATA op_binary(const OP_DSC *dsc,DATA a,DATA b)
{
    OP *op = alloc_op();

    op->dsc = dsc;
    op->a = a;
    op->b = b;
    return eval_op(op);
}


DATA op_ternary(const OP_DSC *dsc,DATA a,DATA b,DATA c)
{
    OP *op = alloc_op();

    op->dsc = dsc;
    op->a = a;
    op->b = b;
    op->c = c;
    return eval_op(op);
}


/* ----- Debugging output -------------------------------------------------- */


struct _prev{
    int indent;
    const struct _prev *prev;
};


static void parents(FILE *file,const struct _prev *curr)
{
    int i;

    if (!curr) return;
    if (curr->prev) {
	parents(file,curr->prev);
	fprintf(file," |");
    }
    for (i = curr->indent-(curr->prev ? curr->prev->indent+2 : 0); i; i--)
	fputc(' ',file);
}


static void do_indent(FILE *file,char branch,int indent,
  const struct _prev *prev)
{
    int i;

    parents(file,prev);
    fprintf(file," %c",branch);
    for (i = indent-(prev ? prev->indent : 0)-2; i; i--) fputc('-',file);
}


static void do_print_expr(FILE *file,DATA d,int indent,
  const struct _prev *prev)
{
    struct _prev new_prev;
    int new_indent,add;

    new_prev.indent = indent;
    new_prev.prev = prev;
    if (!d.op) {
	fputc(' ',file);
	print_data(file,d);
	fputc('\n',file);
	return;
    }
    fprintf(file,"[%s",d.op->dsc->name);
    add = d.type == dt_none || d.type == dt_unum || d.type == dt_decision ? 0 :
      fprintf(file,".%s",type_name(d.type));
    fprintf(file,"]--");
    new_indent = indent+strlen(d.op->dsc->name)+4+add;
    if (d.op->b.type == dt_none) do_print_expr(file,d.op->a,new_indent,prev);
    else {
	do_print_expr(file,d.op->a,new_indent,&new_prev);
	if (d.op->c.type == dt_none) {
	    do_indent(file,'`',new_indent,&new_prev);
	    do_print_expr(file,d.op->b,new_indent,prev);
	}
	else {
	    do_indent(file,'+',new_indent,&new_prev);
	    do_print_expr(file,d.op->b,new_indent,&new_prev);
	    do_indent(file,'`',new_indent,&new_prev);
	    do_print_expr(file,d.op->c,new_indent,prev);
	}
    }
}


void print_expr(FILE *file,DATA d)
{
    do_print_expr(file,d,0,NULL);
}


void debug_expr(const char *title,DATA d)
{
    int n;

    if (!debug) return;
    n = printf("\n===== %s ",title);
    for (n = 79-n; n; n--) putchar('=');
    printf("\n\n");
    print_expr(stdout,d);
}
