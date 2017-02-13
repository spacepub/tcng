%{
/*
 * tcng.l - New TC configuration language
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <addr.h>

#include "config.h"
#include "error.h"
#include "util.h"
#include "location.h"
#include "var.h"
#include "param.h"
#include "tree.h"
#include "device.h"
#include "field.h"
#include "named.h"

#include "y.tab.h"

%}

/* anchor mode, for "if" used as filter name */
%s ANCHOR

/* hex digit */
X				[0-9A-Fa-f]
/* dotted quad */
DQ				[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+

%%

warn				return TOK_WARN;
dev				return TOK_DEV;
qdisc				return TOK_QDISC;
ingress				return TOK_INGRESS;
class				{ BEGIN(INITIAL);
				  return TOK_CLASS; }
on				return TOK_ON;
<INITIAL>if			return TOK_IF;
<ANCHOR>if			return TOK_IF_ANCHOR;
filter				return TOK_FILTER;
element				return TOK_ELEMENT;
tunnel				return TOK_TUNNEL;
police				return TOK_POLICE;
bucket				return TOK_BUCKET;
conform				return TOK_CONFORM;
count				return TOK_COUNT;
else				return TOK_ELSE;
pass				return TOK_PASS;
reclassify			return TOK_RECLASSIFY;
drop				{ BEGIN(INITIAL);
				  return TOK_DROP; }
continue			return TOK_CONTINUE;

field				return TOK_FIELD;
field_root			return TOK_FIELD_ROOT;
pragma				return TOK_PRAGMA;
tag				return TOK_TAG;

cbq				return TOK_CBQ;
dsmark				return TOK_DSMARK;
egress				return TOK_EGRESS;
fifo				return TOK_FIFO;
gred				return TOK_GRED;
htb				return TOK_HTB;
prio				return TOK_PRIO;
red				return TOK_RED;
sfq				return TOK_SFQ;
tbf				return TOK_TBF;

fw				return TOK_FW;
route				return TOK_ROUTE;
rsvp				return TOK_RSVP;
tcindex				return TOK_TCINDEX;

ah				return TOK_AH;
allot				return TOK_ALLOT;
avpkt				return TOK_AVPKT;
bandwidth			return TOK_BANDWIDTH;
bands				return TOK_BANDS;
bounded				return TOK_BOUNDED;
burst				return TOK_BURST;
cburst				return TOK_CBURST;
ceil				return TOK_CEIL;
default				return TOK_DEFAULT;
default_index			return TOK_DEFAULT_INDEX;
dport				return TOK_DPORT;
dst				return TOK_DST;
ecn				return TOK_ECN;
esp				return TOK_ESP;
ewma				return TOK_EWMA;
fall_through			return TOK_FALL_THROUGH;
from				return TOK_FROM;
fromif				return TOK_FROMIF;
grio				return TOK_GRIO;
hash				return TOK_HASH;
isolated			return TOK_ISOLATED;
indices				return TOK_INDICES;
ipproto				return TOK_IPPROTO;
limit				return TOK_LIMIT;
mask				return TOK_MASK;
max				return TOK_MAX;
maxburst			return TOK_MAXBURST;
min				return TOK_MIN;
minburst			return TOK_MINBURST;
mpu				return TOK_MPU;
mtu				return TOK_MTU;
order				return TOK_ORDER;
overflow			return TOK_OVERFLOW;
pass_on				return TOK_PASS_ON;
peakrate			return TOK_PEAKRATE;
perturb				return TOK_PERTURB;
priomap				return TOK_PRIOMAP;
probability			return TOK_PROBABILITY;
protocol			return TOK_PROTOCOL;
quantum				return TOK_QUANTUM;
rate				return TOK_RATE;
r2q				return TOK_R2Q;
set_tc_index			return TOK_SET_TC_INDEX;
shift				return TOK_SHIFT;
skip				return TOK_SKIP;
sport				return TOK_SPORT;
src				return TOK_SRC;
to				return TOK_TO;
value				return TOK_VALUE;
weight				return TOK_WEIGHT;

@				return TOK_HOST;
host				return TOK_HOST;
host4				return TOK_HOST4;
host6				return TOK_HOST6;
precond				return TOK_PRECOND;
sprintf				return TOK_SPRINTF;

\$[a-zA-Z_][a-zA-Z0-9_]*	{ yylval.str = stralloc(yytext+1);
				  return VARIABLE; }
0[Xx]{X}+|0[0-7]+|[0-9]+	{ char *end;
				  yylval.unum = strtoul(yytext,&end,0);
				  if (*end) yyerror("invalid digit in number");
				  return UNUM; }
0[Bb][01]+			{ yylval.unum = strtoul(yytext+2,NULL,2);
				  return UNUM; }
[0-9]+(\.[0-9]*)?([eE][-+]?[0-9]+)? { yylval.fnum = atof(yytext);
				  return FNUM; }
::|::({X}+:)*({X}+|{DQ})|({X}+:)*{X}+::{DQ}?|({X}+:)+(:{X}+)+(:{DQ})?|({X}+:){7}({X}+)|({X}+:){6}{DQ} {
				    yylval.u128 = ipv6_host(yytext,0);
				    return IPV6;
				}
{DQ}				{ yylval.unum = ipv4_host(yytext,1);
				  return IPV4; }


<INITIAL,ANCHOR>\n?[\t ]*	lineno += *yytext == '\n';
<*>^#\ [0-9]+\ \"[^"]*\"(\ [0-9]+)*\n { char *start = strchr(yytext,'"')+1;
				  *strchr(start,'"') = 0;
				  /*
				   * /lib/cpp converts  # 1 ""  to  # 1 "."
				   * so we need to filter this when we're
				   * invoked from tcsim
				   */
				  input_file(strcmp(start,".") ? start : "");
				  lineno = strtol(yytext+2,NULL,0); }

\"[^\"\n\t]*\"			{ *strrchr(yytext,'"') = 0;
				  yylval.str = stralloc(yytext+1);
				  return STRING; }
[a-zA-Z_][a-zA-Z0-9_]*		{ FIELD *field = field_find(yytext);
				  if (field) {
					yylval.field = field;
					return FIELD_NAME;
				  }
				  yylval.str = stralloc(yytext);
				  return WORD; }

"<<"				return SHIFT_LEFT;
">>"				return SHIFT_RIGHT;
==				return REL_EQ;
!=				return REL_NE;
"<="				return REL_LE;
">="				return REL_GE;
&&				return LOGICAL_AND;
"||"				return LOGICAL_OR;

=				{ BEGIN(ANCHOR);
				  return '='; }
"}"				{ BEGIN(INITIAL);
				  return '}'; }

<*>.				return *yytext;

%%


/* -------------------------------- *yy...* -------------------------------- */


void vyyerrorf(const char *fmt,va_list ap)
{
    fflush(stdout);
    print_current_location(stderr);
    fprintf(stderr,": ");
    vfprintf(stderr,fmt,ap);
    fprintf(stderr," near \"%s\"\n",yytext);
    exit(1);
}


void yyerrorf(const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    vyyerrorf(fmt,ap);
    va_end(ap);
}


void yyerror(const char *s)
{
    yyerrorf("%s",s);
}


void vyywarnf(const char *fmt,va_list ap)
{
    if (no_warn) return;
    fflush(stdout);
    print_current_location(stderr);
    fprintf(stderr,": warning: ");
    vfprintf(stderr,fmt,ap);
    fprintf(stderr," near \"%s\"\n",yytext);
    fflush(stderr);
}


void yywarnf(const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    vyywarnf(fmt,ap);
    va_end(ap);
}


void yywarn(const char *s)
{
    yywarnf("%s",s);
}


/* --------------------------------- *l...* -------------------------------- */


void vlerrorf(LOCATION loc,const char *fmt,va_list ap)
{
    fflush(stdout);
    print_location(stderr,loc);
    fprintf(stderr,": ");
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"\n",yytext);
    exit(1);
}


void lerrorf(LOCATION loc,const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    vlerrorf(loc,fmt,ap);
    va_end(ap);
}


void lerror(LOCATION loc,const char *s)
{
    lerrorf(loc,"%s",s);
}


void lwarnf(LOCATION loc,const char *fmt,...)
{
    va_list ap;

    if (no_warn) return;
    fflush(stdout);
    print_location(stderr,loc);
    va_start(ap,fmt);
    fprintf(stderr,": warning: ");
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"\n");
    va_end(ap);
    fflush(stderr);
}


void lwarn(LOCATION loc,const char *s)
{
    lwarnf(loc,"%s",s);
}


/* --------------------------------- *...* --------------------------------- */


void verrorf(const char *fmt,va_list ap)
{
    fflush(stdout);
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"\n");
    exit(1);
}


void errorf(const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    verrorf(fmt,ap);
    va_end(ap);
}


void error(const char *s)
{
    errorf("%s",s);
    exit(1);
}


void warnf(const char *fmt,...)
{
    va_list ap;

    if (no_warn) return;
    fflush(stdout);
    va_start(ap,fmt);
    fprintf(stderr,"warning: ");
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"\n");
    va_end(ap);
    fflush(stderr);
}


void warn(const char *s)
{
    warnf("%s",s);
}
