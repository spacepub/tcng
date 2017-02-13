%{
/*
 * cfg.y - TC simulation language
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>


#define MAX_ARGC 4096
#define MAX_PACKET 70000


#include <u128.h>

#include "tcsim.h"
#include "jiffies.h"
#include "host.h"
#include "var.h"
#include "attr.h"
#include "command.h"


char *current_dev = NULL;

static int argc;
static char *argv[MAX_ARGC];
static unsigned char send_buf[MAX_PACKET];
static unsigned char *hex;
static struct host *current_host = NULL;
static char *echo_string = NULL;

static struct attributes curr_attr;


static void add_echo_string(const char *next)
{
    int len;

    len = echo_string ? strlen(echo_string)+1 : 0;
    echo_string = realloc(echo_string,len+strlen(next)+1);
    if (!echo_string) {
	perror("realloc");
	exit(1);
    }
    if (len) echo_string[len-1] = ' ';
    strcpy(echo_string+len,next);
}


static void bad_rate_unit(const char *unit)
{
    yyerrorf("valid rate units are bps, kBps, Mbps, ... (B for Bytes)");
}


%}

%union {
    const char *str;
    uint32_t num;
    U128 u128;
    struct jiffval ujiff;
    struct command *cmd;
    struct net_device *dev;
    struct attributes attr;
};

%token		TOK_DEV TOK_TC TOK_TIME TOK_SEND TOK_POLL TOK_EVERY TOK_UNTIL
%token		TOK_END TOK_HOST TOK_CONNECT TOK_ROUTE TOK_NETMASK TOK_DEFAULT
%token		TOK_INSMOD TOK_PRELOAD TOK_COMMAND TOK_ATTRIBUTE
%token		TOK_NL TOK_TCC TOK_ECHO SHIFT_RIGHT SHIFT_LEFT
%token		TOK_NFMARK TOK_PRIORITY TOK_PROTOCOL TOK_TC_INDEX
%token	<str>	TOK_WORD TOK_STRING ASSIGNMENT VARIABLE TOK_PRINTF_FORMAT
%token	<num>	TOK_NUM TOK_FORMAT TOK_DQUAD
%token	<ujiff>	TOK_UJIFFIES
%token	<u128>	TOK_IPV6

%type	<num>	opt_rate_expression opt_rate_unit opt_format opt_repetition
%type	<u128>	expression inclusive_or_expression exclusive_or_expression
%type	<u128>	and_expression shift_expression additive_expression
%type	<u128>	multiplicative_expression unary_expression primary_expression
%type	<num>	opt_mask
%type	<ujiff>	abs_time delta_time opt_until
%type	<cmd>	command tc_command
%type	<dev>	opt_dev device
%type	<str>	device_name printf_format
%type	<attr>	attributes attribute_with_flags attribute

%%

all:
    assignments items
    ;

items:
    | items item
    ;

item:
    TOK_HOST '{'
	{
	    current_host = create_host();
	}
      host_items '}'
	{
	    current_host = NULL;
	}
    | connect assignments
    | device_def assignments
    | TOK_INSMOD TOK_WORD
	{
	    kernel_module($2);
	    free($2);
	}
    | TOK_PRELOAD TOK_WORD
	{
	    preload_tc_module($2);
	    free($2);
	}
    | TOK_TIME abs_time assignments
	{
	    if (terminating) yyerror("TIME after END");
	    if (advance_time($2) < 0) yyerror("can't turn back time");
	}
    | TOK_END assignments
	{
	    struct jiffval end = {
		.jiffies = ~0,
		.ujiffies = ~0,
	    };

	    find_stalled_devices();
	    terminating = 1;
	    (void) advance_time(end);
	}
    | command
	{
	    if (!check_only) cmd_run($1);
	    cmd_free($1);
	}
    | TOK_COMMAND ASSIGNMENT '=' command
	{
	    set_var_cmd($2,$4);
	    free($2);
	}
    | TOK_ATTRIBUTE attributes
	{
	    default_attributes = merge_attributes(default_attributes,$2);
	}
    ;

host_items:
    | host_item host_items
    ;

host_item:
    device_def
    | TOK_ROUTE route
    | connect
    | tc_command
	{
	    if (!check_only) cmd_run($1);
	    cmd_free($1);
	}
    ;

device_def:
    TOK_DEV device_name
	{
	    if (current_dev) free(current_dev);
	    current_dev = $2;
	}
      opt_rate_expression opt_tcc
	{
	    create_net_device(current_host,$2,$4);
	}
    ;

device_name:
    TOK_WORD
	{
	    $$ = $1;
	}
    | TOK_STRING
	{
	    $$ = $1;
	}
    ;

route:
    TOK_DQUAD opt_mask device
	{
	    add_route(current_host,$1,$2,$3);
	}
    | TOK_DEFAULT device
	{
	    add_route(current_host,0,0,$2);
	}
    ;

opt_mask:
	{
	    $$ = 0xffffffff;
	}
    | TOK_NETMASK TOK_DQUAD
	{
	    $$ = $2;
	}
    ;

connect:
    TOK_CONNECT device device
	{
	    connect_dev($2,$3);
	}
    ;

opt_rate_expression:
	{
	    $$ = -1;
	}
    | TOK_NUM opt_rate_unit
	{
	    if ($2 > 1) $$ = $1*($2/1000);
	    else {
		if ($1 % 1000)
		    yyerror("interface rate must be at least 1 kbps");
		$$ = $1/1000;
	    }
	}
    ;

opt_rate_unit:
	{
	    $$ = 1000;
	}
    | TOK_WORD
	{
	    const char *p = $1;

	    switch (*p) {
		case 'k':
		    $$ = 1000;
		    p++;
		    break;
		case 'M':
		    $$ = 1000*1000;
		    p++;
		    break;
		case 'G':
		    $$ = 1000*1000*1000;
		    p++;
		    break;
		default:
		    /* no SI prefix ? */
		    $$ = 1;
		    break;
	    }
	    switch (*p) {
		case 'b':
		    break;
		case 'B':
		    $$ *= 8;
		    break;
		default:
		    bad_rate_unit($1);
	    }
	    if (strcmp(p+1,"ps")) bad_rate_unit($1);
	    free($1);
	}
    ;

opt_tcc:
    | TOK_TCC
	/*
	 * TOK_TCC is for error checking only. Otherwise, one could have curly
	 * braces with tcc execution anywhere and the parser wouldn't notice
	 * until it's far too late.
	 */
    ;

argv:
	{
	    argv[argc] = NULL;
	}
    | argv TOK_WORD
	{
	    if (argc == MAX_ARGC) yyerror("too many arguments");
	    argv[argc++] = $2;
	}
    ;

command:
    tc_command
	{
	    $$ = $1;
	}
    | TOK_SEND opt_dev
	{
	   curr_attr = default_attributes;
	}
      attributes assignments
	{
	    curr_attr = merge_attributes(curr_attr,$4);
	    hex = send_buf;
	}
      hex_string
	{
	    $$ = cmd_send($2,send_buf,hex-send_buf,curr_attr);
	}
    | TOK_POLL opt_dev assignments
	{
	    $$ = cmd_poll($2);
	}
    | TOK_EVERY delta_time opt_until command
	{
	    if (terminating) yyerror("EVERY doesn't work after END");
	    $$ = cmd_every($2,$3,$4);
	}
    | TOK_ECHO echo_args assignments
	{
	    $$ = cmd_echo(echo_string);
	    echo_string = NULL;
	}
    | TOK_COMMAND VARIABLE assignments
	{
	    $$ = cmd_clone(get_var_cmd($2));
	    free($2);
	}
    ;

attributes:
	{
	    $$ = no_attributes;
	}
    | attributes attribute_with_flags
	{
	    $$ = merge_attributes($1,$2);
	}
    ;

attribute_with_flags:
    attribute
	{
	    $$ = $1;
	}
    | TOK_DEFAULT attribute_with_flags
	{
	    $$ = $2;
	    $$.dflt = $$.present;
	}
    ;

attribute:
    TOK_NFMARK '=' expression
	{
	    $$.present = attr_nfmark;
	    $$.dflt = 0;
	    $$.nfmark = u128_to_32($3);
	}
    | TOK_PRIORITY '=' expression
	{
	    $$.present = attr_priority;
	    $$.dflt = 0;
	    $$.priority = u128_to_32($3);
	}
    | TOK_PROTOCOL '=' expression
	{
	    $$.present = attr_protocol;
	    $$.dflt = 0;
	    $$.protocol = u128_to_32($3);
	}
    | TOK_TC_INDEX '=' expression
	{
	    uint32_t tmp = u128_to_32($3);

	    $$.present = attr_tc_index;
	    $$.dflt = 0;
	    if (tmp > 0xffff)
		yyerrorf("tc_index %lu > 0xffff",(unsigned long) tmp);
	    $$.tc_index = tmp;
	}
    ;

opt_until:
	{
	    $$.jiffies = ~0UL;
	    $$.ujiffies = ~0UL;
	}
    | TOK_UNTIL abs_time
	{
	    $$ = $2;
	}
    ;

echo_args:
    | echo_arg echo_args
    ;

echo_arg:
    TOK_STRING
	{
	    add_echo_string($1);
	    free($1);
	}
    | printf_format expression
	{
	    char tmp[40]; /* worst case: IPv6 address */

	    if (u128_is_32($2))
		sprintf(tmp,$1 ? $1 : "%u",(unsigned int) u128_to_32($2));
	    else {
		char *p;
		int i;

		if ($1) yywarn("ignoring format string for 128 bit value");
		p = tmp;
		for (i = 3; i >= 0; i--)
		    p += sprintf(p,"%X:%X%s",$2.v[i] >> 16,$2.v[i] & 0xffff,
		      i ? ":" : "");
	    }
	    if ($1) free($1);
	    add_echo_string(tmp);
	}
    ;

printf_format:
	{
	    $$ = NULL;
	}
    | TOK_PRINTF_FORMAT
	{
	    $$ = $1;
	}
    ;

tc_command:
    TOK_TC
	{
	    argv[0] = "tc";
	    argc = 1;
	}
      argv TOK_NL assignments
	{
	    int i;

	    $$ = cmd_tc(argc,argv);
	    for (i = 2; i < argc; i++) free(argv[i]);
	}
    ;

opt_dev:
	{
	    $$ = NULL;
	}
    | device
	{
	    $$ = $1;
	}
    ;

device:
    device_name
	{
	    $$ = lookup_net_device($1);
	    if (!$$) yyerrorf("unknown device \"%s\"",$1);
	    free($1);
	}
    ;

abs_time:
    TOK_UJIFFIES
	{
	    $$ = $1;
	}
    | '+' TOK_UJIFFIES
	{
	    $$ = jv_add(now,$2);
	}
    ;

delta_time:
    TOK_UJIFFIES
	{
	    $$ = $1;
	}
    ;

hex_string:
    | hex_value attributes assignments hex_string
	{
	    curr_attr = merge_attributes(curr_attr,$2);
	}
    ;

hex_value:
    opt_format expression opt_repetition
	{
	    int format,size,i,j;

	    format = $1;
	    if (!format) format = 8;
	    size = (format < 0 ? -format : format)/8;
	    switch (size) {
		case 1:
		    if (u128_is_32($2) && u128_to_32($2) >= 0x100)
			yyerrorf("value 0x%lx is too big for a byte",
			  (unsigned long) u128_to_32($2));
		    break;
		case 2:
		    if (u128_is_32($2) && u128_to_32($2) >= 0x10000)
			yyerrorf("value 0x%lx is too big for a 16 bit word",
			  (unsigned long) u128_to_32($2));
		    break;
		case 4:
		    if (!u128_is_32($2))
			yyerror("value is too big for a 32 bit word");
		    break;
		default:
		    break;
	    }
	    for (i = 0; i < $3; i++) {
		if (hex+size > send_buf+MAX_PACKET) yyerror("packet too big");
		switch (format) {
		    case -8:
		    case 8:
			*hex++ = $2.v[0];
			break;
		    case -16:
			$2.v[0] = ntohs($2.v[0]);
			/* fall through */
		    case 16:
			*hex++ = $2.v[0] >> 8;
			*hex++ = $2.v[0];
			break;
		    case -32:
			$2.v[0] = ntohl($2.v[0]);
			/* fall through */
		    case 32:
			*hex++ = $2.v[0] >> 24;
			*hex++ = $2.v[0] >> 16;
			*hex++ = $2.v[0] >> 8;
			*hex++ = $2.v[0];
			break;
		    case 128:
			for (j = 3; j >= 0; j--) {
			    *hex++ = $2.v[j] >> 24;
			    *hex++ = $2.v[j] >> 16;
			    *hex++ = $2.v[j] >> 8;
			    *hex++ = $2.v[j];
			}
			break;
		    default:
			abort();
		}
	    }
	}
    ;

opt_format:
	{
	    $$ = 0;
	}
    | TOK_FORMAT
	{
	    $$ = $1;
	}
    ;

opt_repetition:
	{
	    $$ = 1;
	}
    | 'x' expression
	{
	    $$ = u128_to_32($2);
	}
    ;

assignments:
    | assignments ASSIGNMENT '=' expression
	{
	    set_var_unum($2,$4);
	    free($2);
	}
    ;

/*
 * Use this indirection so that we can add logical expressions in the future,
 * if we really want them ...
 */

expression:
    inclusive_or_expression
	{
	    $$ = $1;
	}
    ;

inclusive_or_expression:
    exclusive_or_expression
	{
	    $$ = $1;
	}
    | inclusive_or_expression '|' exclusive_or_expression
	{
	    $$ = u128_or($1,$3);
	}
    ;

exclusive_or_expression:
    and_expression
	{
	    $$ = $1;
	}
    | exclusive_or_expression '^' and_expression
	{
	    $$ = u128_xor($1,$3);
	}
    ;

/*
 * Skip equality_expression and such, and go straight to shift_expression
 */

and_expression:
    shift_expression
	{
	    $$ = $1;
	}
    | and_expression '&' shift_expression
	{
	    $$ = u128_and($1,$3);
	}
    ;

shift_expression:
    additive_expression
	{
	    $$ = $1;
	}
    | shift_expression SHIFT_RIGHT additive_expression
	{
	    $$ = u128_shift_right($1,u128_to_32($3));
	}
    | shift_expression SHIFT_LEFT additive_expression
	{
	    $$ = u128_shift_left($1,u128_to_32($3));
	}
    ;

additive_expression:
    multiplicative_expression
	{
	    $$ = $1;
	}
    | additive_expression '+' multiplicative_expression
	{
	    $$ = u128_add($1,$3);
	}
    | additive_expression '-' multiplicative_expression
	{
	    $$ = u128_sub($1,$3);
	}
    ;

multiplicative_expression:
    unary_expression
	{
	    $$ = $1;
	}
    | multiplicative_expression '*' unary_expression
	{
	    $$ = u128_from_32(u128_to_32($1)*u128_to_32($3));
	}
    | multiplicative_expression '/' unary_expression
	{
	    $$ = u128_from_32(u128_to_32($1)/u128_to_32($3));
	}
    | multiplicative_expression '%' unary_expression
	{
	    $$ = u128_from_32(u128_to_32($1) % u128_to_32($3));
	}
   ;

unary_expression:
    primary_expression
	{
	    $$ = $1;
	}
    | '~' primary_expression
	{
	    $$ = u128_not($2);
	}
    ;

primary_expression:
    TOK_NUM
	{
	    $$ = u128_from_32($1);
	}
    | TOK_DQUAD
	{
	    $$ = u128_from_32($1);
	}
    | TOK_IPV6
	{
	    $$ = $1;
	}
    | VARIABLE
	{
	    $$ = get_var_unum($1);
	    free($1);
	}
    | '(' expression ')'
	{
	    $$ = $2;
	}
    ;
