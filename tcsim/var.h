/*
 * var.h - Variable handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#ifndef VAR_H
#define VAR_H

#include <u128.h>

#include "command.h"


enum var_type { vt_unum,vt_cmd };

struct var {
    const char *name;
    enum var_type type;
    union {
	U128 value;
	struct command *cmd;
    } u;
    struct var *next;
};


void set_var_unum(const char *name,U128 value);
void set_var_cmd(const char *name,struct command *cmd);
U128 get_var_unum(const char *name);
struct command *get_var_cmd(const char *name);

#endif /* VAR_H */
