/*
 * iflib.h - Functions for processing if expressions
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Network Robots, Werner Almesberger
 */


#ifndef IFLIB_H
#define IFLIB_H

#include <stdio.h>

#include "location.h"
#include "data.h"
#include "tree.h"


void complement_decisions(DATA *d,CLASS *class);
void iflib_comb_iterate(const QDISC *qdisc,
  void (*fn)(const ELEMENT *e,void *user),void *user);
DATA iflib_combine(const QDISC *qdisc);

/* Generate if expression from all filters on that qdisc */

void iflib_normalize(DATA *d);

/* Normalize && and || */

void iflib_reduce(DATA *d);

/* General optimizations */

void iflib_offset(DATA *d,int for_u32);

/*
 * Add and reduce explicit offset operators. If "for_u32" is non-zero,
 * iflib_offset also tries to combine offsets across sub-expressions, and it
 * ignores the meta field root.
 */

void iflib_actions(LOCATION loc,DATA *d);

/*
 * Convert actions to canonical format by moving policing and such pastall
 * matches.
 */

void iflib_arith(DATA *d);

/*
 * Perform various simple arithmetic optimizations.
 */

void iflib_not(DATA *d);

/*
 * Replace negations in logical expressions by re-arranging the expression
 * around || and &&.
 */

int expr_equal(DATA a,DATA b);

/*
 * Actually, this is a test for whether the expressions are _identical_, not
 * only equal. Returns non-zero if they are.
 */

int action_tree(DATA d);

/*
 * Returns non-zero if the sub-tree rooted at d consists exclusively of
 * policing operators, class selections, and logical operators combining
 * such things. I.e. any such sub-tree goes into the "actions" group at
 * the external interface, while all or part of a sub-tree for which
 * action_tree returns zero has to go into the "rules" group.
 */

typedef enum {
    sc_no,		/* not self_contained */
    sc_no_false,	/* not self-contained, expression is false */
    sc_no_true,		/* not self-contained, expression is true */
    sc_yes		/* is self-contained */
} SELF_CONTAINED;

SELF_CONTAINED self_contained(DATA d);

/*
 * A sub-tree is self_contained if it selects a class, or if any failed
 * conformance decisions lead to a self_contained subtree.
 *
 * Note: self_contained may incorrectly return 0 if the subtree is not an
 * action_tree.
 */

int side_effect(DATA d);

void prune_shortcuts(DATA *d);

/*
 * Used by iflib_act.c to clean out shortcuts uncovered by re-arranging
 * actions.
 */

void iflib_cheap_actions(LOCATION loc,DATA d);
void iflib_cheap_ops(LOCATION loc,DATA d);

/*
 * Refuse all operations that appear to be "expensive"
 */

void iflib_bit(FILE *file,QDISC *qdisc,DATA d);
void iflib_newbit(FILE *file,DATA d);
void iflib_fastbit(FILE *file,DATA d);


/* New action database handling from iflib_actdb.c: */

enum action_type { at_conform,at_count,at_class,at_drop,at_unspec };

struct action {
    enum action_type type;
    union {
	const POLICE *conform;	/* pointer to bucket */
	const POLICE *count;	/* pointer to bucket */
	const CLASS *decision;	/* use CLASS_TO_DECISION to convert to number */
    } u;
    struct action *parent;	/* parent in tree being constructed */
    struct action *c[2];	/* child if 0/1 */
    int number;			/* action number; -1 if none assigned */
    int dumped;			/* non-zero if entry has been dumped */
    struct action *next;	/* next action in list of entry points */
    struct action_referer *referers; /* referers in inverted tree */
};

struct action_referer {
    struct action *action;
    struct action_referer *next;
};

void register_actions(DATA d);
void write_actions(FILE *file,QDISC *qdisc);
const struct action *get_action(DATA d);
int action_number(DATA d);
void free_actions(void);


#define dump_failed(d,reason) __dump_failed(d,reason,__FILE__,__LINE__)
void __dump_failed(DATA d,const char *reason,const char *file,int line);

#endif /* IFLIB_H */
