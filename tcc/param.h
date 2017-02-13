/*
 * param.h - Parameter handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Network Robots
 */


#ifndef PARAM_H
#define PARAM_H

#include <stdint.h>
#include <stdio.h>

#include "data.h"
#include "location.h"


typedef struct _param_dsc {
    const char *id;
    DATA_TYPE type;
    /* ---- the following field are set when evaluating parameter lists ----- */
    uint32_t v; /* value */
    int present;
} PARAM_DSC;

typedef struct _param {
    PARAM_DSC *dsc;	/* prm_... */
    DATA data;
    struct _param *next;
} PARAM;

typedef struct {
    const PARAM_DSC **required;
    const PARAM_DSC **optional;
} PARAM_DEF;


#include "param_decl.inc"


PARAM *param_make(PARAM_DSC *dsc,DATA data);
void param_add(PARAM **list,PARAM *param);
void param_get(const PARAM *params,LOCATION loc);

void check_required(const PARAM *params,const PARAM_DSC **required,
  LOCATION loc);
void check_optional(const PARAM *param,const PARAM_DSC **required,
  const PARAM_DSC **optional,LOCATION loc);
void check_params(const PARAM *params,const PARAM_DSC **required,
  const PARAM_DSC **optional,LOCATION loc);

int prm_present(const PARAM *params,const PARAM_DSC *dsc);
DATA *prm_data_ptr(PARAM *params,const PARAM_DSC *dsc);
DATA prm_data(PARAM *params,const PARAM_DSC *dsc);
uint32_t prm_unum(PARAM *params,const PARAM_DSC *dsc);

void param_push(void);
void param_pop(void);
void param_load(const PARAM *params,const PARAM_DEF *base,
  const PARAM_DEF *curr,LOCATION loc);
void param_print(FILE *file,const PARAM_DEF *base,const PARAM_DEF *child,
  const PARAM_DEF *curr,const PARAM_DSC **special,
  void (*special_fn)(FILE *file,const PARAM_DSC *param,void *arg),
  void *special_arg);

#endif /* PARAM_H */
