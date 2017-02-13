%{
/*
 * tcng.y - New TC configuration language
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots, Werner Almesberger
 * Copyright 2003 Werner Almesberger
 */

/*
 * Note: this parser is full of memory leaks. It makes it much easier to
 * implement, and isn't a problem in real life, because it only runs for
 * one translation pass, and then all memory is freed on process exit.
 */


#include <stdint.h>
#include <stddef.h> /* for NULL */
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h> /* for IFNAMSIZ */

#include <u128.h>
#include <addr.h>

#include "config.h"
#include "tcdefs.h"
#include "util.h"
#include "location.h"
#include "error.h"
#include "var.h"
#include "data.h"
#include "op.h"
#include "param.h"
#include "sprintf.h"
#include "tree.h"
#include "field.h"
#include "named.h"
#include "device.h"
#include "qdisc.h"
#include "filter.h"
#include "police.h"


static ELEMENT *current_element = NULL;
static PARAM_DEF *param_def = NULL;
static uint32_t *number_anchor = NULL;
static char **tag_anchor = NULL;
static char *device_tag; /* devices are handled out-of-sequence */

static PARENT parent;
static PARENT tmp_parent = { .qdisc = NULL };


#define NONZERO(dd) \
  if (!(dd->data.type == dt_unum ? dd->data.u.unum : dd->data.u.fnum)) \
    yyerrorf("parameter \"%s\" must be non-zero",dd->dsc->id);
#define UNUM_RANGE(dd,fmt,lower,upper) \
  if (dd->data.u.unum < lower) \
    yyerrorf("parameter \"%s\" value " fmt " below limit " fmt, \
      dd->dsc->id,(unsigned long) dd->data.u.unum,(unsigned long) lower); \
  if (dd->data.u.unum > upper) \
    yyerrorf("parameter \"%s\" value " fmt " above limit " fmt, \
      dd->dsc->id,(unsigned long) dd->data.u.unum,(unsigned long) upper)


static void begin_scope(DATA d)
{
    var_begin_scope();
    field_begin_scope();
    var_use_begin_scope(d);
}


static void end_scope(void)
{
    var_use_end_scope();
    field_end_scope();
    var_end_scope();
}


/*
 * "device" may be allocated before calling create_device. Look for
 * device_spec: to find the explanation. This kludge should be removed once
 * curly braces around device bodies are mandatory.
 */

static DEVICE *create_device(DEVICE *device,const char *name,PARAM *params)
{
    if (strlen(name) >= IFNAMSIZ)
	yyerrorf("interface name longer than %d characters",IFNAMSIZ-1);
    if (!device) device = alloc_t(DEVICE);
    device->name = name;
    device->location = current_location();
    device->params = params;
    device->egress = NULL;
    device->ingress = NULL;
    device->next = NULL;
    add_device(device);
    parent.device = device;
    parent.class = NULL;
    parent.filter = NULL;
    param_def = NULL;
    number_anchor = NULL;
    tag_anchor = NULL; /* we've already parsed this ! */
    return device;
}


static CLASS *begin_class(int implicit)
{
    CLASS *class;

    class = make_class(parent.qdisc,parent.class,implicit);
    class->parent = parent; /* @@@ still necessary ? */
    parent.class = class;
    param_def = parent.qdisc->dsc->class_param;
    class->location = current_location();
    number_anchor = &class->number;
    tag_anchor = &class->location.tag;
    begin_scope(data_class(class));
    return class;
}


static void end_class(PARAM *params,QDISC *qdisc)
{
    parent.class->params = params;
    if (parent.qdisc->dsc->class_param)
	check_required(params,parent.qdisc->dsc->class_param->required,
	  parent.class->location);
    parent.class->qdisc = qdisc;
    parent = parent.class->parent;
}


static FILTER *begin_filter(const FILTER_DSC *dsc)
{
    FILTER *filter;

    filter = alloc_t(FILTER);
    filter->parent = parent;
    add_filter(filter);
    filter->number = UNDEF_U32;
    filter->location = current_location();
    filter->dsc = dsc;
    filter->elements = NULL;
    filter->next = NULL;
    param_def = dsc->filter_param;
    number_anchor = &filter->number;
    tag_anchor = &filter->location.tag;
    parent.filter = filter;
    begin_scope(data_filter(filter));
    return filter;
}


static void end_filter(FILTER *filter,PARAM *params)
{
    filter->params = params;
    check_required(params,filter->dsc->filter_param->required,
      filter->location);
    end_scope();
    parent = filter->parent;
}


static void begin_element(void)
{
    if (!parent.filter) yyerror("filter element without filter");
    if (!parent.filter->dsc->element_param)
	yyerrorf("filter %s has no elements",parent.filter->dsc->name);
    if (!parent.class) yyerror("filter element without class");
    current_element = alloc_t(ELEMENT);
    add_element(current_element,parent);
    current_element->number = UNDEF_U32;
    current_element->location = current_location();
    current_element->police = NULL;
    current_element->next = NULL;
    param_def = parent.filter->dsc->element_param;
    number_anchor = &current_element->number;
    tag_anchor = &current_element->location.tag;
}


static void end_element(PARAM *params,POLICE *police)
{
    current_element->params = params;
    check_required(params,parent.filter->dsc->element_param->required,
      current_element->location);
    current_element->police = police; 
    current_element = NULL;
}


static void add_tag(const char *tag)
{
    const char *s;

    if (!tag_anchor) yyerror("tag not valid here");
    if (*tag_anchor) yyerror("tag already set");
    if (!*tag) return;
    for (s = tag; *s; s++)
	if (*s <= ' ' || *s > '~') yyerror("invalid character(s) in tag");
    *tag_anchor = tag;
}


static double si_prefix(char prefix,double f,int warn_milli)
{
    static int print_milli_warning = 1;

    switch (prefix) {
	case 'm':
	    /* Alright, they asked for it. They'll get it. Badly. */
	    if (warn_milli && print_milli_warning) {
		yywarn("\"m\" means \"milli\", not \"Mega\"");
		print_milli_warning = 0;
	    }
	    /* I'm too kind ... */
	    return 1.0/f;
	case 'k':
	    return f;
	case 'M':
	    return f*f;
	case 'G':
	    return f*f*f;
	default:
	    return 1.0;
    }
}

%}

%union {
    int unum;
    U128 u128;
    double fnum;
    const char *str;
    DATA data;
    DATA_LIST *list;
    VAR *var;
    DEVICE *dev;
    QDISC *qdisc;
    QDISC_DSC *qddsc;
    CLASS *class;
    FILTER *filter;
    FILTER_DSC *fdsc;
    ELEMENT *element;
    POLICE *police;
    DECISION decis;
    PARAM *param;
    FIELD *field;
    struct {
	DATA b,c;
    } mask;
    LOCATION location; /* abuse for temporary variable */
    PARENT parent; /* abuse for temporary variable */
};


%token		TOK_WARN TOK_DEV
%token		TOK_QDISC TOK_CLASS TOK_FILTER TOK_ELEMENT TOK_TUNNEL TOK_ON
%token		TOK_POLICE TOK_BUCKET TOK_ELSE TOK_IF TOK_IF_ANCHOR
%token		TOK_HOST TOK_HOST4 TOK_HOST6 TOK_SPRINTF
%token		TOK_PRAGMA
%token		TOK_PASS TOK_RECLASSIFY TOK_CONTINUE TOK_FIELD TOK_TAG
%token		TOK_FIELD_ROOT
%token		TOK_CONFORM TOK_COUNT TOK_PRECOND TOK_DROP
%token		TOK_CBQ TOK_DSMARK TOK_FIFO TOK_GRED TOK_HTB TOK_PRIO TOK_RED
%token		TOK_SFQ TOK_TBF
%token		TOK_INGRESS TOK_EGRESS
%token		TOK_FW TOK_ROUTE TOK_RSVP TOK_TCINDEX
%token		TOK_AH TOK_ALLOT TOK_AVPKT TOK_BANDS TOK_BANDWIDTH TOK_BOUNDED
%token		TOK_BURST TOK_CBURST TOK_CEIL
%token		TOK_DEFAULT TOK_DEFAULT_INDEX TOK_DPORT TOK_DST
%token		TOK_ECN TOK_ESP TOK_EWMA TOK_FALL_THROUGH
%token		TOK_FROM TOK_FROMIF TOK_GRIO TOK_HASH
%token		TOK_INDICES TOK_ISOLATED TOK_IPPROTO
%token		TOK_LIMIT TOK_MASK TOK_MAX TOK_MAXBURST TOK_MIN TOK_MINBURST
%token		TOK_MPU TOK_MTU TOK_ORDER TOK_OVERFLOW
%token		TOK_PASS_ON TOK_PEAKRATE TOK_PERTURB TOK_PRIOMAP
%token		TOK_PROBABILITY TOK_PROTOCOL TOK_QUANTUM TOK_R2Q TOK_RATE
%token		TOK_SET_TC_INDEX TOK_SHIFT TOK_SKIP TOK_SPORT TOK_SRC
%token		TOK_TO TOK_VALUE TOK_WEIGHT
%token		SHIFT_RIGHT SHIFT_LEFT LOGICAL_AND LOGICAL_OR
%token		REL_EQ REL_NE REL_LE REL_GE

%token <unum>	UNUM IPV4
%token <u128>	IPV6
%token <fnum>	FNUM
%token <str>	WORD STRING VARIABLE
%token <field>	FIELD_NAME

%type  <qdisc>	qdisc_spec opt_qdisc_or_class_body qdisc_or_class_body
%type  <qdisc>	qdisc_or_class_body_element
%type  <qddsc>	qdisc_name
%type  <class>	class_spec
%type  <filter>	filter_spec opt_filter short_filter_spec
%type  <police>	opt_police police_spec bucket_spec
%type  <decis>	opt_else policing_decision opt_policing_decision
%type  <fdsc>	filter_name
%type  <param>	opt_parameters parameters parameter_list parameter
%type  <param>  parameter_expression
%type  <data>	opt_condition
%type  <data>	complex_expression constant_expression global_expression
%type  <data>	mask_expression unary_expression simple_expression
%type  <data>	additive_expression shift_expression multiplicative_expression
%type  <data>	and_expression inclusive_or_expression exclusive_or_expression
%type  <data>	relational_expression equality_expression expression
%type  <data>	logical_and_expression logical_or_expression
%type  <data>	qdisc_expression class_expression filter_expression
%type  <data>	short_filter_expression
%type  <data>	police_expression police_expression_with_parentheses
%type  <data>	standalone_police_expression
%type  <data>	standalone_police_or_bucket_expression
%type  <data>	primary_expression simple_primary_expression variable_expression
%type  <data>	assignments_with_expression
%type  <data>	primary_policing_expression
%type  <data>	opt_unit_qualifier opt_mask_len
%type  <data>	access_expression opt_index_expression
%type  <data>	host primary_host primary_host4 primary_host6
%type  <data>	port ipproto ether_proto
%type  <list>	forward_class_list pragma_list opt_sprintf_args
%type  <unum>	opt_size_qualifier
%type  <mask>	mask_selector
%type  <str>	device_name

%%


all:
	{
	    field_set("raw",
	      op_ternary(&op_access,
		data_field_root(field_root(0)),data_none(),
	      data_unum(0)),data_none());
	    field_set("__meta",
	      op_ternary(&op_access,
		data_field_root(field_root(1)),data_none(),
	      data_unum(0)),data_none());
	}
      extra_semicolons global_assignments opt_no_dev_specs device_specs
    ;


/* ----- Preamble ---------------------------------------------------------- */


preamble_item:
    TOK_WARN warning_switches
    ;

warning_switches:
    warning_switch
    | warning_switches ',' warning_switch
    ;

warning_switch:
    STRING
	{
	    if (!set_warn_switch($1,0)) yywarn("unrecognized warning switch");
	    free($1);
	}
    ;

/* ----- Assignments and fields -------------------------------------------- */


assignments:
    | assignments assignment
    ;

assignment:
    variable_assignment
    | field
    | preamble_item semicolon
    ;

variable_assignment:
    variable '='
	{
	    var_push_access();
	}
      complex_expression
	{
	    var_pop_access();
	    var_set(field_snapshot($4));
	}
    ;

global_assignments:
    | global_assignments global_assignment
    ;

global_assignment:
    global_variable_assignment
    | field
    | preamble_item semicolon
    | TOK_PRAGMA
	{
	    if (pragma) yyerror("only one global pragma allowed");
	}
      '(' pragma_list ')'
	{
	    pragma = $4;
	}
      semicolon
    ;

global_variable_assignment:
    variable '='
	{
	    var_push_access();
	}
      global_expression
	{
	    var_pop_access();
	    var_set(field_snapshot($4));
	}
    ;


/* ----- Fields ------------------------------------------------------------ */


field:
    TOK_FIELD WORD '=' expression opt_condition
	{
	    field_set($2,$4,$5);
	    free($2);
	}
      semicolon
    | TOK_FIELD FIELD_NAME '=' expression opt_condition
	{
	    field_redefine($2,$4,$5);
	}
      semicolon
    ;

opt_condition:
	{
	    $$ = data_none();
	}
    | TOK_IF expression
	{
	    $$ = $2;
	}
    | TOK_IF_ANCHOR expression /* at this point, we may get either token */
	{
	    $$ = $2;
	}
    ;


/* ----- Device list ------------------------------------------------------- */


device_specs:
    | device_spec device_specs
    ;

device_spec:
	{
	    /*
	     * Make sure local variables get local scope. Note that we have to
	     * enter the scope before parsing the device, because a variable
	     * assignment can be the token immediately following the device
	     * name, so lexer and parser would get de-synchronized.
	     *
	     * By pre-allocating the device, we can register a valid device
	     * pointer, even though the device is created after the first
	     * variable in its scope may have been scanned.
	     *
	     * All this is extremely messy. Should start disallowing device
	     * bodies without curly braces.
	     */
	    parent.device = alloc_t(DEVICE);
	    begin_scope(data_device(parent.device));
	    /*
	     * Language design bugs: if omitting device and braces, variables
	     * may get global scope. If not using braces, variables are pulled
	     * into local scope of previous device, which may be a bit of a
	     * surprise.
	     *
	     * The solution: always use braces.
	     */
	}
      device device_body
    ;

device_body:
    '{' extra_semicolons assignments qdisc_expressions
	{
	    end_scope();
	}
      '}' extra_semicolons global_assignments
    ;

opt_no_dev_specs:
    |
	{
	    create_device(NULL,default_device,NULL);
		/* stralloc this if ever implementing deallocations */
	    begin_scope(data_device(parent.device));
	}
      qdisc_expressions_noblock
	{
	    end_scope();
	}
    | {
	    create_device(NULL,default_device,NULL);
		/* stralloc this if ever implementing deallocations */
	    begin_scope(data_device(parent.device));
	}
      '{' extra_semicolons assignments qdisc_expressions
	{
	    end_scope();
	}
      '}' extra_semicolons global_assignments
    ;

qdisc_expressions_noblock:
    single_qdisc_expression
    | single_qdisc_expression qdisc_expressions_noblock
    ;

qdisc_expressions:
    single_qdisc_expression
    | single_qdisc_expression qdisc_expressions
    |	{
	    begin_scope(data_none());
	}
      '{' extra_semicolons assignments qdisc_expressions
	{
	    end_scope();
	}
      '}' extra_semicolons assignments
    ;

single_qdisc_expression:
    qdisc_expression assignments
	{
	    if ($1.u.qdisc->dsc == &ingress_dsc) {
		if (parent.device->ingress)
		    yyerror("ingress qdisc is already set");
		parent.device->ingress = $1.u.qdisc;
	    }
	    else {
		if (parent.device->egress)
		    yyerror("egress qdisc is already set");
		parent.device->egress = $1.u.qdisc;
	    }
	}
    ;

device:
    device_name
	{
	    param_def = &device_def;
	    device_tag = NULL;
	    tag_anchor = &device_tag;
	}
      opt_parameters
	{
	    DEVICE *device;

	    param_def = NULL;
	    device = create_device(parent.device,$1,$3);
	    device->location.tag = device_tag;
	}
    ;

device_name:
    STRING
	{
	    $$ = $1;
	}
    | WORD
	{
	    $$ = $1;
	}
    | TOK_DEV WORD
	{
	    $$ = $2;
	}
    | TOK_DEV expression
	{
	    $$ = data_convert($2,dt_string).u.string;
	}
    ;


/* ----- Expressions ------------------------------------------------------- */


complex_expression:
    expression semicolon
	{
	    $$ = $1;
	}
    | qdisc_expression
	{
	    $$ = $1;
	}
    | class_expression
	{
	    $$ = $1;
	}
    | filter_expression
	{
	    $$ = $1;
	}
    ;

global_expression:
    expression semicolon
	{
	    $$ = $1;
	}
    ;

qdisc_expression:
    TOK_QDISC variable_expression
	{
	    $<data>$ = data_convert($2,dt_qdisc);
	    if ($<data>$.u.qdisc->parent.device != parent.device)
		yyerror("qdisc was created in different scope");
	    $<parent>1 = parent; /* a little hackish ... @@@ */
	    parent.qdisc = $<data>$.u.qdisc;
	    parent.class = NULL;
	    parent.filter = NULL;
	    parent.tunnel = NULL;
	    begin_scope($<data>$);
	}
      opt_qdisc_or_class_body
	{
	    /* opt_qdisc_or_class_body ends the scope */
	    parent = $<parent>1;
	    $$ = $<data>3;
	}
    | qdisc_spec
	{
	    $$ = data_qdisc($1);
	}
    ;

class_expression:
    TOK_CLASS variable_expression
	{
	    $<data>$ = data_convert($2,dt_class);
	    if ($<data>$.u.class->parent.qdisc != parent.qdisc ||
	      $<data>$.u.class->parent.class != parent.class)
		yyerror("class was created in different scope");
	    $<data>$.u.class->parent = parent;
		/* okay to change it a little - the fields we may modify are
		   only used temporarily during construction anyway */
	    parent.class = $<data>$.u.class;
	    begin_scope($<data>$);
	}
      opt_selectors opt_qdisc_or_class_body
	{
	    /* end_scope is called in opt_qdisc_or_class_body */
	    if ($5) {
		if (parent.class->qdisc) yyerror("class already has a qdisc");
		parent.class->qdisc = $5;
	    }
	    parent = parent.class->parent;
	    $$ = $<data>3;
	}
    | class_spec
	{
	    $$ = data_class($1);
	}
    ;

short_filter_expression:
    TOK_FILTER variable_expression
	{
	    $$ = data_convert($2,dt_filter);
	    if ($$.u.filter->parent.qdisc != parent.qdisc ||
	      $$.u.filter->parent.class != parent.class)
		yyerror("filter was created in different scope");
	}
    | short_filter_spec
	{
	    $$ = data_filter($1);
	    if ($$.u.filter->parent.qdisc != parent.qdisc ||
	      $$.u.filter->parent.class != parent.class)
		yyerror("filter was created in different scope");
	}
    ;

filter_expression:
    TOK_FILTER variable_expression
	{
	    $<data>$ = data_convert($2,dt_filter);
	    if ($<data>$.u.filter->parent.qdisc != parent.qdisc)
		yyerror("filter was created in different scope");
	    $<data>$.u.filter->parent.filter = parent.filter;
	    parent.filter = $<data>$.u.filter;
	    /* we don't dare to touch any other fields of parent */
	    begin_scope($<data>$);
	}
      opt_filter_body
	{
	    end_scope();
	    parent.filter = parent.filter->parent.filter;
	    $$ = $<data>3;
	}
    | filter_spec
	{
	    $$ = data_filter($1);
	    if ($$.u.filter->parent.qdisc != parent.qdisc ||
	      $$.u.filter->parent.class != parent.class)
		yyerror("filter was created in different scope");
	}
    ;

standalone_police_or_bucket_expression:
    standalone_police_expression
    | TOK_BUCKET variable
	{
	    $$ = data_convert(var_get(),dt_bucket);
	}
    | bucket_spec
	{
	    $$ = data_bucket($1);
	}
    ;

standalone_police_expression:
    TOK_POLICE variable
	{
	    $$ = data_convert(var_get(),dt_police);
	}
    | police_spec
	{
	    $$ = data_police($1);
	}
    ;

police_expression_with_parentheses:
   police_expression
	{
	    $$ = $1;
	}
   | '(' expression ')'
	{
	    $$ = data_convert($2,$2.type == dt_bucket ? dt_bucket : dt_police);
	}
   ;

police_expression:
    opt_police_keyword variable
	/* allow dt_bucket -> dt_police for backward compatibility */
	{
	    DATA d = var_get();
	    $$ = data_convert(d,d.type == dt_bucket ? dt_bucket : dt_police);
	}
    | TOK_BUCKET variable
	{
	    $$ = data_convert(var_get(),dt_bucket);
	}
    | police_spec
	{
	    $$ = data_police($1);
	}
    | bucket_spec
	{
	    $$ = data_bucket($1);
	}
    ;

opt_police_keyword:
    | TOK_POLICE
    ;

constant_expression:
    expression
	{
	    $$ = $1;
	    if ($$.op) yyerror("constant expression isn't");
	}
    ;


/*
 * The following expression syntax is an almost exact copy from K&R 2nd ed.,
 * with a few modifications near simple_expression.
 */

expression:
    logical_or_expression
	{
	    $$ = $1;
	}
    ;

logical_or_expression:
    logical_and_expression
	{
	    $$ = $1;
	}
    | logical_or_expression LOGICAL_OR logical_and_expression
	{
	    $$ = op_binary(&op_logical_or,$1,$3);
	}
    ;

logical_and_expression:
    inclusive_or_expression
	{
	    $$ = $1;
	}
    | logical_and_expression LOGICAL_AND inclusive_or_expression
	{
	    $$ = op_binary(&op_logical_and,$1,$3);
	}
    ;

inclusive_or_expression:
    exclusive_or_expression
	{
	    $$ = $1;
	}
    | inclusive_or_expression '|' exclusive_or_expression
	{
	    $$ = op_binary(&op_or,$1,$3);
	}
    ;

exclusive_or_expression:
    and_expression
	{
	    $$ = $1;
	}
    | exclusive_or_expression '^' and_expression
	{
	    $$ = op_binary(&op_xor,$1,$3);
	}
    ;

and_expression:
    equality_expression
	{
	    $$ = $1;
	}
    | and_expression '&' equality_expression
	{
	    $$ = op_binary(&op_and,$1,$3);
	}
    ;

equality_expression:
    relational_expression
	{
	    $$ = $1;
	}
    | equality_expression REL_EQ relational_expression
	{
	    $$ = op_binary(&op_eq,$1,$3);
	}
    | equality_expression REL_NE relational_expression
	{
	    $$ = op_binary(&op_ne,$1,$3);
	}
    ;

relational_expression:
    shift_expression
	{
	    $$ = $1;
	}
    | relational_expression '<' shift_expression
	{
	    $$ = op_binary(&op_lt,$1,$3);
	}
    | relational_expression '>' shift_expression
	{
	    $$ = op_binary(&op_gt,$1,$3);
	}
    | relational_expression REL_LE shift_expression
	{
	    $$ = op_binary(&op_le,$1,$3);
	}
    | relational_expression REL_GE shift_expression
	{
	    $$ = op_binary(&op_ge,$1,$3);
	}
    ;

shift_expression:
    additive_expression
	{
	    $$ = $1;
	}
    | shift_expression SHIFT_RIGHT additive_expression
	{
	    $$ = op_binary(&op_shift_right,$1,$3);
	}
    | shift_expression SHIFT_LEFT additive_expression
	{
	    $$ = op_binary(&op_shift_left,$1,$3);
	}
    ;

additive_expression:
    multiplicative_expression
	{
	    $$ = $1;
	}
    | additive_expression '+' multiplicative_expression
	{
	    $$ = op_binary(&op_plus,$1,$3);
	}
    | additive_expression '-' multiplicative_expression
	{
	    $$ = op_binary(&op_minus,$1,$3);
	}
    ;

multiplicative_expression:
    unary_expression
	{
	    $$ = $1;
	}
    | multiplicative_expression '*' unary_expression
	{
	    $$ = op_binary(&op_mult,$1,$3);
	}
    | multiplicative_expression '/' unary_expression
	{
	    /*
	     * Access $1 before op_binary, because op_binary may destroy it.
	     * Issue warning after op_binary, because op_binary may decide to
	     * issue an error (e.g. if $3 is not dt_unum.
	     */
	    int warn = warn_const_pfx &&
	      !$1.op && ($1.type == dt_ipv4 || $1.type == dt_ipv6);

	    $$ = op_binary(&op_div,$1,$3);
	    if (warn) yywarn("taking prefix of constant (and not field)");
	}
    | multiplicative_expression '%' unary_expression
	{
	    $$ = op_binary(&op_mod,$1,$3);
	}
    ;

unary_expression:
    mask_expression
	{
	    $$ = $1;
	}
    | '!' mask_expression
	{
	    $$ = op_unary(&op_logical_not,$2);
	}
    | '~' mask_expression
	{
	    $$ = op_unary(&op_not,$2);
	}
    | '-' mask_expression
	{
	    $$ = op_unary(&op_unary_minus,$2);
	}
    ;

mask_expression:
    simple_expression
	{
	    $$ = $1;
	}
    | simple_expression ':' mask_selector
	{
	    $$ = op_ternary(&op_mask,$1,$3.b,$3.c);
	}
/*
 * We don't use mask_expression ':' mask_selector  here, because this would
 * generate an ambiguity for cases like a:b:c:d, which is a little messy to
 * resolve. In any case, multiple masks are hardly of any practical use.
 */
    | simple_expression IPV6
	{
	    yyerror("Sorry, mis-parsed \"::\". Please use \": :\" instead.");
	}
    ;

mask_selector:
    simple_expression opt_mask_len
	{
	    $$.b = $1;
	    $$.c = $2;
	}
    | ':' simple_expression
	{
	    $$.b = data_none();
	    $$.c = $2;
	}
    ;

opt_mask_len:
	{
	    $$ = data_none();
	}
    | ':' simple_expression
	{
	    $$ = $2;
	}
    ;

simple_expression:
    primary_expression opt_unit_qualifier
	{
	    $$ = data_add_unit($1,$2);
	}
    ;

opt_unit_qualifier:
	{
	    $$ = data_none();
	}
    | WORD
	{
	    const char *unit = $1;

	    if (strchr("mkMG",*unit)) unit++;
	    $$ = data_none();
	    if (!strcmp(unit,"bps")) {
		$$.type = dt_rate;
		$$.u.fnum = si_prefix(*$1,1000,1);
	    }
	    else if (!strcmp(unit,"Bps")) {
		$$.type = dt_rate;
		$$.u.fnum = si_prefix(*$1,1000,1)*8.0;
	    }
	    else if (!strcmp(unit,"b") || !strcmp(unit,"bit") ||
	      !strcmp(unit,"bits")) {
		$$.type = dt_size;
		$$.u.fnum = si_prefix(*$1,1024,1)/8.0;
	    }
	    else if (!strcmp(unit,"B") || !strcmp(unit,"Byte") ||
	      !strcmp(unit,"Bytes")) {
		$$.type = dt_size;
		$$.u.fnum = si_prefix(*$1,1024,1);
	    }
	    else if (!strcmp(unit,"p") || !strcmp(unit,"pck") ||
	      !strcmp(unit,"pcks")) {
		$$.type = dt_psize;
		$$.u.fnum = si_prefix(*$1,1000,1);
	    }
	    else if (!strcmp(unit,"pps")) {
		$$.type = dt_prate;
		$$.u.fnum = si_prefix(*$1,1000,1);
	    }
	    else if (!strcmp(unit,"s") || !strcmp(unit,"sec") ||
	      !strcmp(unit,"secs")) {
		$$.type = dt_time;
		$$.u.fnum = si_prefix(*$1,1000,0);
	    }
	    else yyerrorf("unrecognized unit name \"%s\"",$1);
	    free($1);
	}
    ;

primary_expression:
    simple_primary_expression
    | primary_policing_expression
	{
	    $$ = $1;
	}
    ;

simple_primary_expression:
    variable_expression
	{
	    $$ = $1;
	}
    | UNUM
	{
	    $$ = data_unum($1);
	}
    | IPV4
	{
	    $$ = data_ipv4($1);
	}
    | IPV6
	{
	    $$ = data_ipv6($1);
	}
    | FNUM
	{
	    $$ = data_fnum($1);
	}
    | STRING
	{
	    $$ = data_string($1);
	    free($1);
	}
    | access_expression
	{
	    $$ = $1;
	}
    | TOK_HOST primary_host
	{
	    $$ = $2;
	}
    | TOK_HOST4 primary_host4
	{
	    $$ = $2;
	}
    | TOK_HOST6 primary_host6
	{
	    $$ = $2;
	}
    | TOK_PRECOND primary_expression
	{
	    $$ = op_unary(&op_precond,$2);
	}
    | TOK_SPRINTF '(' expression opt_sprintf_args ')'
	{
	    char *s;
	    DATA_LIST *walk,*next;

	    s = sprintf_data(data_convert($3,dt_string).u.string,$4);
	    $$ = data_string(s);
	    free(s);
	    for (walk = $4; walk; walk = next) {
		data_destroy(*walk->ref);
		free(walk->ref);
		next = walk->next;
		free(walk);
	    }
	}
    | '(' expression ')'
	{
	    $$ = $2;
	}
    |	{
	    begin_scope(data_none());
	}
	  '{' assignments_with_expression
	{
	    end_scope();
	}
	  '}'
	{
	    $$ = $3;
	}
    | standalone_police_or_bucket_expression
	{
	    $$ = $1;
	}
    ;

opt_sprintf_args:
	{
	    $$ = NULL;
	}
    | ',' expression opt_sprintf_args
	{
	    DATA *d = alloc_t(DATA);

	    *d = $2;
	    $$ = data_list_element(d);
	    $$->next = $3;
	}
    ;

assignments_with_expression:
    complex_expression
	{
	    $$ = $1;
	}
    | variable_assignment assignments_with_expression
	{
	    $$ = $2;
	}
    | field assignments_with_expression
	{
	    $$ = $2;
	}
    ;

primary_policing_expression:
    TOK_CONFORM police_expression_with_parentheses
	{
	    $$ = op_unary(&op_conform,$2);
	}
    | TOK_COUNT police_expression_with_parentheses
	{
	    $$ = op_unary(&op_count,$2);
	}
    | TOK_DROP
	{
	    $$ = data_decision(dr_drop,NULL);
	}
    | TOK_RECLASSIFY
	{
	    $$ = data_decision(dr_reclassify,NULL);
	}
    | TOK_PASS
	{
	    static int warn = 1;

	    if (warn) {
		yywarnf("direct use of \"pass\" is discouraged");
		warn = 0;
	    }
	    $$ = data_decision(dr_class,NULL);
	}
    | TOK_CONTINUE
	{
	    static int warn = 1;

	    if (warn) {
		yywarnf("direct use of \"continue\" is discouraged");
		warn = 0;
	    }
	    $$ = data_decision(dr_continue,NULL);
	}
    ;

variable_expression:
    variable
	{
	    $$ = data_clone(var_get());
	}
    ;

variable:
    VARIABLE
	{
	    var_begin_access(var_find($1));
	    free($1);
	}
    | variable '.' WORD
	{
	    var_follow($3);
	    free($3);
	}
    ;

access_expression:
    FIELD_NAME opt_index_expression opt_size_qualifier
	{
	    $$ = op_ternary(&op_access,data_field($1),$2,
	      data_unum($3 > 0 ? $3 : -$3));
	    $$.type = $1->access.type;
	    if ($3 == -32) $$.type = dt_ipv4;
	    else if ($3 == -128) $$.type = dt_ipv6;
	    else if ($3) $$.type = dt_unum;
	}
    | TOK_FIELD_ROOT '(' expression ')' opt_index_expression opt_size_qualifier
	{
	    int offset_group;

	    offset_group = data_convert($3,dt_unum).u.unum;
	    if (offset_group < RESERVED_OFFSET_GROUPS)
		yyerrorf("field root numbers < %d are reserved for internal "
		  "use",RESERVED_OFFSET_GROUPS);
	    $$ = op_ternary(&op_access,
              data_field_root(field_root(offset_group)),
	      $5,data_unum($6 > 0 ? $6 : -$6));
	    if ($6 == -32) $$.type = dt_ipv4;
	    else if ($6 == -128) $$.type = dt_ipv6;
	}
    ;

opt_size_qualifier:
	{
	    $$ = 0;
	}
    | '.' UNUM
	{
	    if ($2 != 8 && $2 != 16 && $2 != 32)
		yyerrorf("size must be 8, 16, or 32, not %d",$2);
	    $$ = $2;
	}
    | '.' WORD
	{
	    if (!strcmp($2,"b")) $$ = 8;
	    else if (!strcmp($2,"ns")) $$ = 16;
	    else if (!strcmp($2,"nl")) $$ = 32;
	    else if (!strcmp($2,"ipv4")) $$ = -32;
	    else if (!strcmp($2,"ipv6")) $$ = -128;
	    else if (!strcmp($2,"hs") || !strcmp($2,"hl"))
		yyerror("host byte order access not supported");
	    else yyerrorf("unknown size qualifier \"%s\"",$2);
	    free($2);
	}
    ;

opt_index_expression:
	{
	    $$ = data_none();
	}
    | '[' expression ']'
	{
	    $$ = $2;
	}
    ;


/* ----- Qdisc specification ----------------------------------------------- */


qdisc_spec:
    qdisc_name
	{
	    if (!parent.device) yyerror("qdisc without device");
	    $<qdisc>$ = alloc_t(QDISC);
	    $<qdisc>$->parent = parent;
	    $<qdisc>$->dsc = $1;
	    $<qdisc>$->number = UNDEF_U32;
	    $<qdisc>$->location = current_location();
	    $<qdisc>$->classes = NULL;
	    $<qdisc>$->filters = NULL;
	    $<qdisc>$->if_expr = data_none();
	    parent.qdisc = $<qdisc>$;
	    parent.class = NULL;
	    parent.filter = NULL;
	    param_def = $1->qdisc_param;
	    number_anchor = $1 != &ingress_dsc ? &$<qdisc>$->number : NULL;
	    tag_anchor = &$<qdisc>$->location.tag;
	    begin_scope(data_qdisc($<qdisc>$));
	}
      opt_parameters
	{
	    $<qdisc>$ = $<qdisc>2;
	    $<qdisc>$->params = $3;
	    check_required($3,$<qdisc>$->dsc->qdisc_param->required,
	      current_location());
	}
      opt_qdisc_or_class_body
	{
	    /* end_scope is called in opt_qdisc_or_class_body */
	    $<qdisc>$ = $<qdisc>2;
	    parent = $<qdisc>$->parent;
	}
    ;

qdisc_name:
    TOK_INGRESS
	{
	    $$ = &ingress_dsc;
	}
    | TOK_CBQ
	{
	    $$ = &cbq_dsc;
	}
    | TOK_DSMARK
	{
	    $$ = &dsmark_dsc;
	}
    | TOK_EGRESS
	{
	    $$ = &egress_dsc;
	}
    | TOK_FIFO
	{
	    $$ = &fifo_dsc;
	}
    | TOK_GRED
	{
	    $$ = &gred_dsc;
	}
    | TOK_HTB
	{
	    $$ = &htb_dsc;
	}
    | TOK_PRIO
	{
	    $$ = &prio_dsc;
	}
    | TOK_RED
	{
	    $$ = &red_dsc;
	}
    | TOK_SFQ
	{
	    $$ = &sfq_dsc;
	}
    | TOK_TBF
	{
	    $$ = &tbf_dsc;
	}
    ;

opt_qdisc_or_class_body:
    semicolon
	{
	    end_scope();
	    $$ = NULL;
	}
    | '{' extra_semicolons qdisc_or_class_body
	{
	    end_scope();
	}
      '}' extra_semicolons
	{
	    $$ = $3;
	}
    ;

qdisc_or_class_body:
	{
	    $$ = NULL;
	}
    | qdisc_or_class_body qdisc_or_class_body_element
	{
	    /* qdiscs are only returned in class body */
	    if ($1 && $2)
		lerror($2->location,"only one qdisc allowed in class body");
	    $$ = $1 ? $1 : $2;
	}
    ;

qdisc_or_class_body_element:
    assignment
	{
	    $$ = NULL;
	}
    | filter_expression
	{
	    /* ignore value */
	    $$ = NULL;
	}
    | class_expression
	{
	    /* ignore value */
	    $$ = NULL;
	}
    | drop_expression
	{
	    $$ = NULL;
	}
    |	{
	    $<class>$ = parent.class;
	    if (!parent.class) begin_class(1);
	}
      qdisc_expression
	{
	    if ($<class>1) $$ = $2.u.qdisc;
	    else {
		end_scope(); /* end_class does not end its scope */
		end_class(NULL,$2.u.qdisc);
		$$ = NULL;
	    }
	}
    |	{
	    begin_scope(data_none());
	}
	  '{' extra_semicolons qdisc_or_class_body
	{
	    end_scope();
	}
	  '}' extra_semicolons
	{
	    $$ = $4;
	}
    ;


/* ----- Parameters -------------------------------------------------------- */


opt_parameters:
	{
	    $$ = NULL;
	}
    | parameters
	{
	    $$ = $1;
	}
    ;

parameters:
    '(' parameter_list ')'
	{
	    const PARAM *walk,*scan;

	    for (walk = $2; walk; walk = walk->next)
		for (scan = walk->next; scan; scan = scan->next)
		    if (walk->dsc == scan->dsc)
			yyerrorf("duplicate parameter \"%s\"",walk->dsc->id);
	    $$ = $2;
	}
    ;

parameter_list:
	{
	    $$ = NULL;
	}
    | parameter
	{
	    $$ = $1;
	}
    | parameter ',' parameter_list
	{
	    if ($1) {
		$$ = $1;
		$$->next = $3;
	    }
	    else {
		$$ = $3;
	    }
	}
    ;

parameter:
    parameter_expression
	{
	    $$ = $1;
	    if ($$)
		check_optional($$,param_def->required,param_def->optional,
		  current_location());
	}
    ;

parameter_expression:
    constant_expression
	{
	    if ($1.type == dt_string) add_tag($1.u.string);
	    else {
		if (!number_anchor) yyerror("number/handle not valid here");
		if (*number_anchor != UNDEF_U32)
		    yyerror("number/handle already set");
		*number_anchor = data_convert($1,dt_unum).u.unum;
	    }
	    $$ = NULL;
	}
    | '<'
	{
	    var_outer_scope();
	}
	  forward_class_list
	{
	    var_inner_scope(); /* undo effect of var_outer_scope */
	}
	  '>'
	{
	    if (!(parent.qdisc->dsc->flags & QDISC_CLASS_SEL_PATHS))
		yyerrorf("%s does not support class selection paths",
		  parent.qdisc->dsc->name);
	    $$ = param_make(&prm_class_sel_path,data_list($3));
	}
    | TOK_TAG constant_expression
	{
	    add_tag(data_convert($2,dt_string).u.string);
	    $$ = NULL;
	}
    | TOK_AH constant_expression
	{
	    $$ = param_make(&prm_ah,$2);
	    NONZERO($$);
	}
    | TOK_ALLOT constant_expression
	{
	    $$ = param_make(&prm_allot,$2);
	    NONZERO($$);
	}
    | TOK_AVPKT constant_expression
	{
	    $$ = param_make(&prm_avpkt,$2);
	    NONZERO($$);
	}
    | TOK_BANDS constant_expression
	{
	    $$ = param_make(&prm_bands,$2);
	    UNUM_RANGE($$,"%lu",TCQ_PRIO_MIN_BANDS,TCQ_PRIO_BANDS);
	}
    | TOK_BANDWIDTH constant_expression
	{
	    $$ = param_make(&prm_bandwidth,$2);
	}
    | TOK_BOUNDED
	{
	    $$ = param_make(&prm_bounded,data_unum(1));
	}
    | TOK_BURST constant_expression
	{
	    $$ = param_make(&prm_burst,$2);
	    /* check against expected psched_mtu */
	}
    | TOK_CBURST constant_expression
	{
	    $$ = param_make(&prm_cburst,$2);
	}
    | TOK_CEIL constant_expression
	{
	    $$ = param_make(&prm_ceil,$2);
	}
    | TOK_DEFAULT
	{
	    $$ = param_make(&prm_default,data_unum(1));
	}
    | TOK_DEFAULT_INDEX constant_expression
	{
	    $$ = param_make(&prm_default_index,$2);
	    UNUM_RANGE($$,"0x%lx",0,0xffff);
	}
    | TOK_DPORT port
	{
	    $$ = param_make(&prm_dport,$2);
	    UNUM_RANGE($$,"%lu",1,0xffff);
	}
    | TOK_DST host
	{
	    $$ = param_make(&prm_dst,$2);
	    NONZERO($$);
	}
    | TOK_ECN
	{
	    $$ = param_make(&prm_ecn,data_unum(1));
	}
    | TOK_ESP constant_expression
	{
	    $$ = param_make(&prm_esp,$2);
	    NONZERO($$);
	}
    | TOK_EWMA constant_expression
	{
	    $$ = param_make(&prm_ewma,$2);
	    UNUM_RANGE($$,"%lu",1,TC_CBQ_MAX_EWMA);
	}
    | TOK_FALL_THROUGH
	{
	    $$ = param_make(&prm_fall_through,data_unum(1));
	}
    | TOK_FROM constant_expression
	{
	    $$ = param_make(&prm_from,$2);
	    UNUM_RANGE($$,"0x%lx",0,0xff);
	}
    | TOK_FROMIF constant_expression
	{
	    $$ = param_make(&prm_fromif,$2);
	    /* @@@ tcng should check if interface exists */
	}
    | TOK_GRIO
	{
	    $$ = param_make(&prm_grio,data_unum(1));
	}
    | TOK_HASH constant_expression
	{
	    $$ = param_make(&prm_hash,$2);
	    UNUM_RANGE($$,"%lu",1,0x10000);
	}
    | TOK_INDICES constant_expression
	{
	    $$ = param_make(&prm_indices,$2);
	    UNUM_RANGE($$,"0x%lx",1,0x10000);
	    /* 2^n checked in qdisc.c */
	}
    | TOK_IPPROTO ipproto
	{
	    $$ = param_make(&prm_ipproto,$2);
	    UNUM_RANGE($$,"%lu",0,0xff);
	}
    | TOK_ISOLATED
	{
	    $$ = param_make(&prm_isolated,data_unum(1));
	}
    | TOK_LIMIT constant_expression
	{
	    if ($2.type == dt_psize) $$ = param_make(&prm_plimit,$2);
	    else $$ = param_make(&prm_limit,$2);
	}
    | TOK_MASK constant_expression
	{
	    $$ = param_make(&prm_mask,$2);
	    UNUM_RANGE($$,"0x%lx",0,0xffff);
	    /* DSMARK: 0..0xff, checked in qdisc.c */
	}
    | TOK_MAX constant_expression
	{
	    $$ = param_make(&prm_max,$2);
	}
    | TOK_MAXBURST constant_expression
	{
	    $$ = param_make(&prm_maxburst,$2);
	    NONZERO($$);
	}
    | TOK_MIN constant_expression
	{
	    $$ = param_make(&prm_min,$2);
	}
    | TOK_MINBURST constant_expression
	{
	    $$ = param_make(&prm_minburst,$2);
	}
    | TOK_MPU constant_expression
	{
	    $$ = param_make(&prm_mpu,$2);
	}
    | TOK_MTU constant_expression
	{
	    $$ = param_make(&prm_mtu,$2);
	    NONZERO($$);
	}
    | TOK_ORDER constant_expression
	{
	    $$ = param_make(&prm_order,$2);
	    UNUM_RANGE($$,"0x%lx",0,0xff);
	}
    | TOK_OVERFLOW police_expression_with_parentheses
	{
	    $$ = param_make(&prm_overflow,$2);
	}
    | TOK_PASS_ON
	{
	    $$ = param_make(&prm_fall_through,data_unum(0));
	}
    | TOK_PEAKRATE constant_expression
	{
	    $$ = param_make(&prm_peakrate,$2);
	}
    | TOK_PERTURB constant_expression
	{
	    $$ = param_make(&prm_perturb,$2);
	}
    | TOK_PRAGMA pragma_list
	{
	    $$ = $2 ? $$ = param_make(&prm_pragma,data_list($2)) : NULL;
	}
    | TOK_PRIO constant_expression
	{
	    $$ = param_make(&prm_prio,$2);
	}
    | TOK_PRIOMAP forward_class_list
	{
	    $$ = param_make(&prm_priomap,data_list($2));
	}
    | TOK_PROBABILITY constant_expression
	{
	    $$ = param_make(&prm_probability,$2);
	}
    | TOK_PROTOCOL ether_proto
	{
	    $$ = param_make(&prm_protocol,$2);
	    UNUM_RANGE($$,"0x%lx",0,0xffff);
	}
    | TOK_QUANTUM constant_expression
	{
	    if ($2.type == dt_size) $$ = param_make(&prm_quantum,$2);
	    else $$ = param_make(&prm_tquantum,$2);
	    NONZERO($$);
	}
    | TOK_R2Q constant_expression
	{
	    $$ = param_make(&prm_r2q,$2);
	}
    | TOK_RATE constant_expression
	{
	    $$ = param_make(&prm_rate,$2);
	    /* NONZERO($$); @@@ policer may have zero rate */
	}
    | TOK_SET_TC_INDEX
	{
	    $$ = param_make(&prm_set_tc_index,data_unum(1));
	}
    | TOK_SHIFT constant_expression
	{
	    $$ = param_make(&prm_shift,$2);
	    UNUM_RANGE($$,"%lu",0,15);
	}
    | TOK_SKIP constant_expression
	{
	    $$ = param_make(&prm_skip,$2);
	    NONZERO($$);
	}
    | TOK_SPORT port
	{
	    $$ = param_make(&prm_sport,$2);
	    UNUM_RANGE($$,"%lu",1,0xffff);
	}
    | TOK_SRC host
	{
	    $$ = param_make(&prm_src,$2);
	    NONZERO($$);
	}
    | TOK_TO constant_expression
	{
	    $$ = param_make(&prm_to,$2);
	    UNUM_RANGE($$,"0x%lx",0,0xff);
	}
    | TOK_VALUE constant_expression
	{
	    $$ = param_make(&prm_value,$2);
	    UNUM_RANGE($$,"0x%lx",0,0xff);
	}
    | TOK_WEIGHT constant_expression
	{
	    $$ = param_make(&prm_weight,$2);
	    NONZERO($$);
	}
    ;

host:
    constant_expression
	{
	    if ($1.type == dt_string) $$ = data_unum(ipv4_host($1.u.string,1));
	    else $$ = $1;
	}
    ;

primary_host:
    primary_host4
	{
	    /*
	     * Note: "host" must _not_ try to "guess" the address type, because
	     * an IPv4 address is used in a completely different way than an
	     * IPv6 address.
	     */
	    $$ = $1;
	}
    ;

primary_host4:
    primary_expression
	{
	    if ($1.type == dt_string) $$ = data_ipv4(ipv4_host($1.u.string,1));
	    else $$ = data_convert($1,dt_ipv4);
	}
    ;

primary_host6:
    primary_expression
	{
	    if ($1.type == dt_string) $$ = data_ipv6(ipv6_host($1.u.string,1));
	    else $$ = data_convert($1,dt_ipv6);
	}
    ;

port:
    constant_expression
	{
	    if ($1.type == dt_string) $$ = data_unum(ip_port($1.u.string));
	    else $$ = $1;
	}
    ;

ipproto:
    constant_expression
	{
	    if ($1.type == dt_string) $$ = data_unum(ip_proto($1.u.string));
	    else $$ = $1;
	}
    ;

ether_proto:
    constant_expression
	{
	    if ($1.type == dt_string) $$ = data_unum(ether_proto($1.u.string));
	    else $$ = $1;
	}
    ;

/*
 * Lists are linked lists of DATA_LIST elements, rooted at a DATA of type
 * dt_list. The actual list content is accessed via d.u.list-{>next-}>ref.
 *
 * Magic classes are in fact forward-references. We use the variables before
 * assigning to them. This may look a little odd, but at least the semantics
 * are pretty bullet-proof.
 *
 * Something slightly ugly about forward-references: we need to back-patch the
 * storage location, so it can't be copied by value. Fortunately, lists just
 * happen to copy their elements by reference. What a lucky coincidence :-)
 */

forward_class_list:
	{
	    $$ = NULL;
	}
    | variable
	{
	    $<list>$ = data_list_element(var_forward());
	}
	  forward_class_list
	{
	    $$ = $<list>2;
	    $$->next = $3;
	}
    ;

pragma_list:
	{
	    $$ = NULL;
	}
    | simple_primary_expression pragma_list
	{
	    const char *s;
	    DATA *d;

	    $1 = data_convert($1,dt_string);
	    if (!*$1.u.string) $$ = $2;
	    else {
		for (s = $1.u.string; *s; s++)
		    if (*s <= ' ' || *s > '~')
			yyerror("invalid character in pragma");
		d = alloc_t(DATA);
		*d = $1;
		$$ = data_list_element(d);
		$$->next = $2;
	    }
	}
    ;


/* ----- Classes ----------------------------------------------------------- */


class_spec:
    TOK_CLASS
	{
	    $<class>$ = begin_class(0);
	}
      opt_parameters opt_selectors opt_qdisc_or_class_body
	{
	    $$ = $<class>2;
	    end_class($3,$<qdisc>5);
	}
    ;


drop_expression:
    TOK_DROP
	{
	    $<class>$ = parent.class;
	    parent.class = &class_is_drop;
	}
      opt_selectors
	{
	    parent.class = $<class>2;
	}
      semicolon
    ;

opt_selectors:
	{
	    if (parent.filter) yywarn("class without selector inside filter");
	    else if (parent.class == &class_is_drop)
		    yywarn("ignoring \"drop\" without selector");
	}
    | selectors
    ;

selectors:
    selector
    | selectors selector
    ;

selector:
    TOK_ON on_selector
    | TOK_IF
	{
	    $<location>$ = current_location();
	}
	  opt_filter expression
	{
	    DATA d = $4;
	    ELEMENT *element;

	    debug_expr("Raw IF expression",d);
	    d = field_expand(d);
	    debug_expr("Expanded IF expression",d);
	    if ($3 && $3->dsc != &if_dsc)
		yyerrorf("if requires filter \"if\", not \"%s\"",$3->dsc->name);
	    element = add_if(d,parent,$3,$<location>2);
	}
    ;

on_selector:
	{
	    /*
	     * This is a little ugly. Background:
	     *  - element gets class from parent.class
	     *  - if filter_expression generates a filter, its parent is the
	     *    parent of the current class.
	     */
	    /* @@@ make it work ? NO ! */
	    if (parent.class == &class_is_drop) /* @@@ fix this */
		yyerror("we currently don't support \"drop on\"");
	    tmp_parent = parent;
	    parent = parent.class->parent;
	}
    short_filter_expression
	{
	    parent = tmp_parent;
	    parent.filter = $2.u.filter;
	    if (parent.class == &class_is_tunnel &&
	      parent.filter != parent.tunnel->parent.filter)
		yyerror("can't assign tunnel to different filter");
	    parent.tunnel = NULL;
	}
      element_spec
	{
	    parent = tmp_parent;
	}
    | variable_expression
	{
	    if (parent.class == &class_is_drop)
		yyerror("we currently don't support \"drop on\"");
	    tmp_parent = parent;
	    parent.filter = data_convert($1,dt_filter).u.filter;
	    if (parent.filter->parent.qdisc != parent.qdisc)
		yyerror("filter was created in different scope");
	    if (parent.class == &class_is_tunnel &&
	      parent.filter != parent.tunnel->parent.filter)
		yyerror("can't assign tunnel to different filter");
	    parent.tunnel = NULL;
	    begin_element();
	}
      opt_element parameters opt_police
	{
	    end_element($4,$5);
	    parent = tmp_parent;
	}
    | 
	{
	    if (parent.class == &class_is_drop)
		yyerror("we currently don't support \"drop on\"");
	    begin_element();
	}
      parameters opt_police
	{
	    end_element($2,$3);
	}
    |
	{
	    if (parent.class == &class_is_drop)
		yyerror("we currently don't support \"drop on\"");
	}
      element_spec
    ;

opt_filter:
	{
	    $$ = NULL;
	}
    | TOK_FILTER variable_expression
	{
	    $$ = data_convert($2,dt_filter).u.filter;
	    if ($$->parent.qdisc != parent.qdisc)
		yyerror("filter was created in different scope");
	}
    ;

opt_element:
    | TOK_ELEMENT
    ;


/* ----- Filters ----------------------------------------------------------- */


filter_spec:
    filter_name
	{
	    $<filter>$ = begin_filter($1);
	}
	  opt_parameters opt_filter_body
	{
	    $$ = $<filter>2;
	    end_filter($$,$3);
	}
    ;

short_filter_spec:
    filter_name
	{
	    $<filter>$ = begin_filter($1);
	}
	  opt_parameters
	{
	    $$ = $<filter>2;
	    end_filter($$,$3);
	}
    ;

filter_name:
    TOK_FW
	{
	    $$ = &fw_dsc;
	}
    | TOK_ROUTE
	{
	    $$ = &route_dsc;
	}
    | TOK_RSVP
	{
	    $$ = &rsvp_dsc;
	}
    | TOK_TCINDEX
	{
	    $$ = &tcindex_dsc;
	}
    | TOK_IF_ANCHOR
	{
	    yyerror("\"if\" anchor doesn't work yet and will be re-designed");
	    $$ = &if_dsc;
	}
    ;

opt_filter_body:
    semicolon
    | '{' extra_semicolons filter_body '}' extra_semicolons
    ;

filter_body:
    | filter_body filter_body_element
    ;

filter_body_element:
    assignment
    | class_expression
	{
	    /* ignore value */
	}
    | drop_expression
    | tunnel
    | '{' extra_semicolons
	{
	    begin_scope(data_none());
	}
      filter_body
	{
	    end_scope();
	}
      '}'
    ;


/* ----- Tunnels ----------------------------------------------------------- */


tunnel:
    TOK_TUNNEL
	{
	    TUNNEL *tunnel;

	    tunnel = alloc_t(TUNNEL);
	    tunnel->parent = parent;
	    tunnel->number = UNDEF_U32;
	    tunnel->location = current_location();
	    parent.tunnel = tunnel;
	    parent.class = &class_is_tunnel;
	    param_def = &tunnel_param;
	    number_anchor = &tunnel->number;
	    tag_anchor = &tunnel->location.tag;
	    begin_scope(data_tunnel(tunnel));
	}
      opt_parameters opt_selectors
	{
	    parent.class = parent.tunnel->parent.class;
	    parent.tunnel->params = $3;
	    check_required($3,tunnel_param.required,current_location());
	}
      opt_filter_body
	{
	    end_scope();
	    parent = parent.tunnel->parent;
	}
    ;

/* ----- Filter Elements --------------------------------------------------- */


element_spec:
    TOK_ELEMENT
	{
	    begin_element();
	}
      opt_parameters opt_police
	{
	    end_element($3,$4);
	}
    ;

opt_police:
	{
	    $$ = NULL;
	}
    | standalone_police_expression
	{
	    $$ = $1.u.police;
	}
    ;

/*
 * Not opt_parameters, because a) this would conflict with variable assignment,
 * and b) it wouldn't make sense anyway.
 */

police_spec:
    TOK_POLICE
	{
	    $<police>$ = alloc_t(POLICE);
	    $<police>$->number = UNDEF_U32;
	    $<police>$->location = current_location();
	    number_anchor = &$<police>$->number;
	    tag_anchor = &$<police>$->location.tag;
	    param_def = &police_def;
	}
      parameters
	{
	    $<police>$ = $<police>2;
	    $<police>$->params = $3;
	    check_required($3,police_def.required,current_location());
	}
      opt_policing_decision opt_else
	{
	    $<police>$ = $<police>2;
	    $<police>$->out_profile = $5;
	    $<police>$->in_profile = $6;
	    add_police($<police>$);
	}
    ;

bucket_spec:
    TOK_BUCKET
	{
	    $<police>$ = alloc_t(POLICE);
	    $<police>$->number = UNDEF_U32;
	    $<police>$->location = current_location();
	    number_anchor = &$<police>$->number;
	    tag_anchor = &$<police>$->location.tag;
	    param_def = &bucket_def;
	}
      parameters
	{
	    $<police>$ = $<police>2;
	    $<police>$->params = $3;
	    check_required($3,bucket_def.required,current_location());
	    $<police>$->out_profile = pd_reclassify;
	    $<police>$->in_profile = pd_ok; 
	    add_police($<police>$);
	}
    ;

opt_policing_decision:
	{
	    $$ = pd_reclassify;
	}
    | policing_decision
	{
	    $$ = $1;
	}
    ;

opt_else:
	{
	    $$ = pd_ok;
	}
    | TOK_ELSE policing_decision
	{
	    $$ = $2;
	}
    ;

policing_decision:
    TOK_PASS
	{
	    $$ = pd_ok;
	}
    | TOK_RECLASSIFY
	{
	    $$ = pd_reclassify;
	}
    | TOK_DROP
	{
	    $$ = pd_drop;
	}
    | TOK_CONTINUE
	{
	    $$ = pd_continue;
	}
    ;


/* ----- Semicolon terminated constructs ----------------------------------- */


semicolon:
    ';' extra_semicolons
    ;

extra_semicolons:
    | extra_semicolons ';'
    ;
