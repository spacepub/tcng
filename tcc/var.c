/*
 * var.c - Variable handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots
 */


#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "location.h"
#include "var.h"


static VAR_SCOPE *curr_scope = NULL;


/* ----- Variable hash management ------------------------------------------ */


#define HASH_SIZE	20011
#define HASH_SHIFT	2


static VAR_HASH **hash_table = NULL;


static VAR_HASH *hash_lookup(const char *name)
{
    static unsigned int hash_feedback[1 << HASH_SHIFT] = {
	0,
	13001,
	30011,
	50777,
    };
    unsigned int hash = 0;
    const char *p;
    VAR_HASH **walk;

    if (!hash_table) {
	int i;

	hash_table = alloc(sizeof(VAR_HASH)*HASH_SIZE);
	for (i = 0; i < HASH_SIZE; i++)
	    hash_table[i] = NULL;
    }
    for (p = name; *p; p++)
	hash ^= (hash >> HASH_SHIFT) ^
	  hash_feedback[hash & ((1 << HASH_SHIFT)-1)] ^ (*p*13);
    for (walk = hash_table+(hash % HASH_SIZE); *walk; walk = &(*walk)->next)
	if (!strcmp((*walk)->name,name)) return *walk;
    *walk = alloc_t(VAR_HASH);
    (*walk)->name = stralloc(name);
    (*walk)->vars = NULL;
    (*walk)->next = NULL;
    return *walk;
}


void dump_hash(FILE *file)
{
    int entries = 0,slots = 0,max_len = 0;
    int i;

    for (i = 0; i < HASH_SIZE; i++) {
	const VAR_HASH *walk;
	int len = 0;

	if (!hash_table[i]) continue;
	slots++;
	for (walk = hash_table[i]; walk; walk = walk->next)
	    len++;
	if (len > max_len) max_len = len;
	entries += len;
    }
    fprintf(file,"variable hash: %d entries in %d slots, max collisions %d\n",
      entries,slots,max_len-1);
}


/* ----- Variable lookup and creation -------------------------------------- */


static VAR *var_new(VAR_HASH *hash)
{
    VAR *var;

    var = alloc_t(VAR);
    var->data = data_none();
    var->data_ref = NULL;
    var->usage = vu_new;
    var->location = current_location();
    var->hash = hash;
    var->scope = curr_scope;
    var->next_in_scope = NULL;
    *curr_scope->last = var;
    curr_scope->last = &var->next_in_scope;
    /*
     * If there is a variable with the same name, it must be in a different
     * scope. We add the new variable at the beginning of the list of
     * equivocal variables, so that the current scope is on top, and it is
     *  easily removed when closing the scope.
     */
    assert(!hash->vars || hash->vars->scope != curr_scope);
    var->next_with_name = hash->vars;
    hash->vars = var;
    return var;
    
}


VAR *var_find(const char *name)
{
    VAR_HASH *hash;
    VAR *var;

    hash = hash_lookup(name);
    if (!hash->vars) (void) var_new(hash);
    var = hash->vars;
    var->found = current_location();
    debugf("var_find: %s -> %p (%s)",name,var,
      var->usage == vu_new ? "new" : var->usage == vu_unused ? "unused" :
      var->usage == vu_used ? "used" : "forward");
    return var;
}


/* ----- Variable scopes --------------------------------------------------- */


/*
 * var_outer_scope goes back one scope. Because ending a scope assumes that
 * we're at the top (innermost) scope, we can't allow new scopes to be opened
 * or closed while var_outer_scope is active. Hence the asserts.
 */

static VAR_SCOPE *saved_scope = NULL;


void var_begin_scope(void)
{
    VAR_SCOPE *new_scope;

    assert(!saved_scope);
    new_scope = alloc_t(VAR_SCOPE);
    new_scope->vars = NULL;
    new_scope->last = &new_scope->vars;
    new_scope->next = curr_scope;
    curr_scope = new_scope;
    debugf("var_begin_scope");
}


void var_end_scope(void)
{
    VAR_SCOPE *scope;

    assert(!saved_scope);
    while (curr_scope->vars) {
	VAR *this;

	this = curr_scope->vars;
	curr_scope->vars = this->next_in_scope;
	assert(this->scope == curr_scope);
	if (this->usage == vu_unused && warn_unused)
	    lwarnf(this->location,"unused variable %s",this->hash->name);
	if (this->usage == vu_forward)
	    lerrorf(this->location,"unresolved forward-declared variable %s",
	      this->hash->name);
	this->hash->vars = this->next_with_name;
	/*
	 * Don't destroy object - we need it for output
	 */
	debugf("  removing %s",this->hash->name);
	free(this);
    }
    scope = curr_scope;
    curr_scope = scope->next;
    free(scope);
}


void var_outer_scope(void)
{
    debugf("var_outer_scope");
    assert(!saved_scope);
    saved_scope = curr_scope;
    curr_scope = curr_scope->next;
    assert(curr_scope); /* can't go beyond global scope */
}


void var_inner_scope(void)
{
    debugf("var_inner_scope");
    assert(saved_scope);
    curr_scope = saved_scope;
    saved_scope = NULL;
}


/* ----- Variable access --------------------------------------------------- */


static struct var_access {
    VAR *var;
    const char *new_entry;
    DATA clone;			/* cloned data, so that we don't clobber orig
				   e.g. if in outer scope */
    DATA *data;			/* insertion point */
    const DATA *orig_data;	/* retrieval point */
    const DATA_ASSOC *entry;	/* position in associative array */
    struct var_access *next;	/* next on stack */
} current_access;

/*
 * Write accesses can be nested using compound expressions, so we need
 * to stack them. Read accesses are never nested, so a static top of
 * stack element is sufficient.
 */


void var_begin_access(VAR *var)
{
    current_access.var = var;
    current_access.new_entry = NULL;
    current_access.clone = data_clone(var->data);
    current_access.data = &current_access.clone;
    current_access.orig_data = &var->data;
    current_access.entry = NULL;
}


void var_push_access(void)
{
    struct var_access *new;

    new = alloc_t(struct var_access);
    *new = current_access;
    current_access.next = new;
}


void var_pop_access(void)
{
    struct var_access *top;

    top = current_access.next;
    current_access = *top;
    free(top);
}


void var_follow(const char *name)
{
    DATA_ASSOC **entry;

    if (current_access.data->type != dt_assoc) {
	data_destroy(*current_access.data);
	*current_access.data = data_assoc();
    }
    for (entry = &current_access.data->u.assoc; *entry;
      entry = &(*entry)->next)
        if (!strcmp((*entry)->name,name)) break;
    if (*entry) {
	const DATA_ASSOC *walk;

	for (walk = current_access.orig_data->u.assoc; walk; walk = walk->next)
	    if (!strcmp(walk->name,name)) break;
	assert(walk);
	current_access.orig_data = &walk->data;
    }
    else {
	*entry = alloc_t(DATA_ASSOC);
	(*entry)->name = stralloc(name);
	(*entry)->data = data_none();
	(*entry)->parent = current_access.entry;
	(*entry)->next = NULL;
	if (!current_access.new_entry)
	    current_access.new_entry = (*entry)->name;
	current_access.orig_data = NULL;
    }
    current_access.data = &(*entry)->data;
    current_access.entry = *entry;
}


void var_set(DATA data)
{
    debugf("var_set %s",current_access.var->hash->name);
    register_var(current_access.var->hash->name,current_access.entry,data);
    if (current_access.var->usage == vu_forward) {
	if (current_access.entry)
	    yyerror("cannot access structure entries in forward-declared "
	      "variable");
	current_access.var->usage = vu_used;
	*current_access.data = *current_access.var->data_ref = data;
	current_access.var->data = current_access.clone;
	return;
    }
    if (!current_access.entry || current_access.var->data.type != dt_assoc) {
	if (current_access.var->usage != vu_new &&
	 current_access.var->scope == curr_scope) {
	    if (current_access.var->usage == vu_unused && warn_unused)
		lwarnf(current_access.var->location,"unused variable %s",
		  current_access.var->hash->name);
	    if (warn_redefine)
		lwarnf(current_access.var->found,"variable %s redefined",
		  current_access.var->hash->name);
	    current_access.var->location = current_access.var->found;
	    if (!var_use_file) data_destroy(current_access.var->data);
	}
	if (current_access.var->scope != curr_scope)
	    current_access.var = var_new(current_access.var->hash);
	      /* no need to reset current_access.data here, because it can only
		 point to &current_access.clone */
	current_access.var->usage = vu_unused;
    }
    *current_access.data = data;
    current_access.var->data = current_access.clone;
}


DATA *var_forward(void)
{
    debugf("var_forward %s",current_access.var->hash->name);
    if (current_access.var->usage != vu_new &&
      current_access.var->usage != vu_forward &&
      current_access.var->scope == curr_scope)
	yyerrorf("variable %s redefined",current_access.var->hash->name);
    if (current_access.entry)
	yyerrorf("cannot forward-declare structure entries");
    if (current_access.var->scope != curr_scope)
	current_access.var = var_new(current_access.var->hash);
    if (!current_access.var->data_ref)
	current_access.var->data_ref = alloc_t(DATA);
    current_access.var->usage = vu_forward;
    return current_access.var->data_ref;
}


int var_initialized(const VAR *var)
{
    return !(var->usage == vu_new || var->usage == vu_forward);
}


DATA var_get(void)
{
    debugf("var_get %s",current_access.var->hash->name);
    if (!var_initialized(current_access.var))
	yyerrorf("variable %s is uninitialized",current_access.var->hash->name);
    if (current_access.var->data.type != dt_assoc &&
      current_access.clone.type == dt_assoc)
	yyerrorf("expected structure, not type %s",
	  type_name(current_access.var->data.type));
    if (current_access.new_entry)
	yyerrorf("no entry \"%s\" in structure",current_access.new_entry);
    data_destroy(current_access.clone);
    current_access.var->usage = vu_used;
    return *current_access.orig_data;
}
