/*
 * var.c - Variable handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#include <string.h>

#include <u128.h>
#include <memutil.h>

#include "tcsim.h"
#include "command.h"
#include "var.h"


static struct var *vars = NULL;


static struct var *set_var(const char *name,enum var_type type)
{
    struct var **var;

    for (var = &vars; *var; var = &(*var)->next)
	if (!strcmp((*var)->name,name)) break;
    if (!*var) {
	*var = alloc_t(struct var);
	(*var)->name = stralloc(name);
	(*var)->next = NULL;
    }
    else {
	/*
	 * Don't free the command ! Somebody might be doing something nasty
	 * like  command $c = command $c
	 */
	/* if ((*var)->type == vt_cmd) cmd_free((*var)->u.cmd); */
    }
    (*var)->type = type;
    return *var;
}


void set_var_unum(const char *name,U128 value)
{
    set_var(name,vt_unum)->u.value = value;
}


void set_var_cmd(const char *name,struct command *cmd)
{
    set_var(name,vt_cmd)->u.cmd = cmd;
}


static struct var *get_var(const char *name,enum var_type type)
{
    struct var *var;

    for (var = vars; var; var = var->next)
	if (!strcmp(var->name,name)) break;
    if (!var) yyerror("unknown variable");
    if (var->type != type) yyerror("invalid variable type");
    return var;
}


U128 get_var_unum(const char *name)
{
    return get_var(name,vt_unum)->u.value;
}


struct command *get_var_cmd(const char *name)
{
    return get_var(name,vt_cmd)->u.cmd;
}
