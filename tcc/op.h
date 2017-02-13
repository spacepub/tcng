/*
 * op.h - Operators
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#ifndef OP_H
#define OP_H


#include <stdio.h>

#include "data.h"


struct _op;


typedef struct {
    const char *name;
    DATA (*eval)(struct _op *op);
    /*
     * Evaluate constant expression; return non-zero if evaluation not
     * possible; issue error if type conflict.
     */
} OP_DSC;

typedef struct _op {
    const OP_DSC *dsc;
    DATA a,b;		/* unused operands are initialized to dt_none */
    DATA c;		/* for : : */
} OP;


extern OP_DSC op_plus;
extern OP_DSC op_minus;
extern OP_DSC op_unary_minus;
extern OP_DSC op_mult;
extern OP_DSC op_div;
extern OP_DSC op_mod;
extern OP_DSC op_not;
extern OP_DSC op_and;
extern OP_DSC op_or;
extern OP_DSC op_xor;
extern OP_DSC op_mask;
extern OP_DSC op_shift_left;
extern OP_DSC op_shift_right;
extern OP_DSC op_eq;
extern OP_DSC op_ne;
extern OP_DSC op_gt;
extern OP_DSC op_ge;
extern OP_DSC op_lt;
extern OP_DSC op_le;
extern OP_DSC op_logical_not;
extern OP_DSC op_logical_or;
extern OP_DSC op_logical_and;
extern OP_DSC op_access; /* access(base,offset,size) */
extern OP_DSC op_offset; /* offset(expr,offset) - only for if */
extern OP_DSC op_conform; /* conform(bucket) */
extern OP_DSC op_count; /* count(bucket) */
extern OP_DSC op_precond; /* precond(expr) */
extern OP_DSC op_precond2; /* precond2(expr,precondition) - only for fields */
extern OP_DSC op_field_root; /* field_root(expr,root pointer) */


DATA op_unary(const OP_DSC *dsc,DATA a);
DATA op_binary(const OP_DSC *dsc,DATA a,DATA b);
DATA op_ternary(const OP_DSC *dsc,DATA a,DATA b,DATA c);

void print_expr(FILE *file,DATA d);
void debug_expr(const char *title,DATA d);

#endif /* OP_H */
