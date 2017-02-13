%{
/*
 * cfg.l - TC simulation language
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <u128.h>
#include <addr.h>
#include <memutil.h>

#include "tcsim.h"
#include "jiffies.h"
#include "y.tab.h"


extern char *current_dev;

static char *file_name = NULL;
static int lineno = 1;
static int brace_level = 0;
static char *tcc = NULL; /* tcc input; NULL if none allocated */
static char *tcng = NULL; /* tcc output; NULL if not parsing */
static int tcc_len; /* input string length; undefined if tcc is NULL */
static YY_BUFFER_STATE sim_file;
static char *sim_file_name;
static int sim_line;


void yywarn(const char *s);
void yyerror(const char *s);


static int newlines(const char *s)
{
    int nls = 0;

    while (*s)
	if (*s++ == '\n') nls++;
    return nls;
}


static struct jiffval atoj(const char *s)
{
    struct jiffval res;
    char *curr = strchr(s,'.');
    int value = 100000;

    res.jiffies = atoi(yytext);
    res.ujiffies = 0;
    if (!curr) return res;
    while (*++curr) {
	if (!isdigit(*curr)) break;
	if (!value) {
	    yywarn("warning: extra digits ignored");
	    break;
	}
	res.ujiffies += value*(*curr-'0');
	value /= 10;
    }
    return res;
}


static void si_prefix(struct jiffval *jv,char prefix)
{
    switch (prefix) {
	case 'M':
	    jv->jiffies *= 1000000;
	    jv->jiffies += jv->ujiffies;
	    jv->ujiffies = 0;
	    break;
	case 'k':
	    jv->jiffies *= 1000;
	    jv->jiffies += jv->ujiffies/1000;
	    jv->ujiffies = (jv->ujiffies % 1000)*1000;
	    break;
	case 'm':
	    jv->ujiffies /= 1000;
	    jv->ujiffies += (jv->jiffies % 1000)*1000;
	    jv->jiffies /= 1000;
	    break;
	case 'u':
	    jv->ujiffies = jv->jiffies;
	    jv->jiffies = 0;
	    break;
	default:
	    break;
    }
}


static void do_add_tcc(const char *s,int recurse)
{
    static int last_line; /* undefined while tcc == NULL */
    int new_line,len;

    if (!tcc) {
	tcc_len = 0;
	last_line = -1;
    }
    /*
     * Only insert explicit location if can't fix it by adding a newline
     */
    new_line = lineno != last_line;
    if (new_line && lineno != last_line+1 && recurse) {
	char *here;

	here = alloc_sprintf("# %d \"%s\"",lineno,
	  file_name ? file_name : "<stdin>");
	do_add_tcc(here,0);
	free(here);
    }
    last_line = lineno;
    len = strlen(s);
    tcc = realloc(tcc,tcc_len+len+2);
    if (!tcc) {
	perror("realloc");
	exit(1);
    }
    tcc[tcc_len++] = new_line ? '\n' : ' ';
    strcpy(tcc+tcc_len,s);
    tcc_len += len;
    tcc[tcc_len] = 0;
}


static void add_tcc(const char *s)
{
    do_add_tcc(s,1);
}


%}

/* words (tc commands) */
%x WORD
/* path (insmod, preload) */
%x PATH
/* tcc input */
%x TCC
/* "magic" curly braces after TOK_DEV */
%s DEV

/* variable name */
var				\$[a-zA-Z_][a-zA-Z_0-9]*
/* hex digit */
X                               [0-9A-Fa-f]
/* dotted quad */
DQ     				[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+

%%

host				{ BEGIN(INITIAL);
				  return TOK_HOST; }
dev				{ BEGIN(DEV);
				  return TOK_DEV; }
connect				return TOK_CONNECT;
route				return TOK_ROUTE;
netmask				return TOK_NETMASK;
default				return TOK_DEFAULT;
tc				{ BEGIN(WORD);
				  return TOK_TC; }
time				return TOK_TIME;
send				return TOK_SEND;
poll				return TOK_POLL;
every				return TOK_EVERY;
until				return TOK_UNTIL;
echo				return TOK_ECHO;
end				return TOK_END;
insmod				{ BEGIN(PATH);
				  return TOK_INSMOD; }
preload				{ BEGIN(PATH);
				  return TOK_PRELOAD; }
command				return TOK_COMMAND;

nfmark				return TOK_NFMARK;
priority			return TOK_PRIORITY;
protocol			return TOK_PROTOCOL;
tc_index			return TOK_TC_INDEX;

attribute			return TOK_ATTRIBUTE;
default				return TOK_DEFAULT;

<WORD>[^\t\n# ]+		{ yylval.str = stralloc(yytext);
				  return TOK_WORD; }
<PATH>[^\t\n# ]+		{ BEGIN(INITIAL);
				  yylval.str = stralloc(yytext);
				  return TOK_WORD; }

[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+ { yylval.num = ipv4_host(yytext,0);
				  return TOK_DQUAD; }
::|::({X}+:)*({X}+|{DQ})|({X}+:)*{X}+::{DQ}?|({X}+:)+(:{X}+)+(:{DQ})?|({X}+:){7}({X}+)|({X}+:){6}{DQ}		{ yylval.u128 = ipv6_host(yytext,0);
				  return TOK_IPV6; }

[0-9]+|0[Xx][0-9a-fA-F]+	{ char *end;
				  yylval.num = strtoul(yytext,&end,0);
				  if (*end) yyerror("invalid digit in number");
				  return TOK_NUM; }
0[Bb][01]+			{ yylval.num = strtoul(yytext+2,NULL,2);
				  return TOK_NUM; }
[0-9]+(\.[0-9]*)?[Mkmu]?s(ecs?)? { struct jiffval jv = atoj(yytext);
				  si_prefix(&jv,strchr(yytext,'s')[-1]);
				  yylval.ujiff = stoj(jv);
				  return TOK_UJIFFIES; }
[0-9]+(\.[0-9]*)?[Mkmu]?j(iffies)? { yylval.ujiff = atoj(yytext);
				  si_prefix(&yylval.ujiff,
				    strchr(yytext,'j')[-1]);
				  return TOK_UJIFFIES; }

\"[^\"\n\t]+\"			{ *strrchr(yytext,'"') = 0;
				  yylval.str = stralloc(yytext+1);
				  return TOK_STRING; }
\%[ux]|0x\%x			{ yylval.str = stralloc(yytext);
				  return TOK_PRINTF_FORMAT; }

{var}/[\t ]*=[\t ]*		{ yylval.str = stralloc(yytext+1);
				  return ASSIGNMENT; }
{var}				{ yylval.str = stralloc(yytext+1);
				  return VARIABLE; }

b:				{ yylval.num = 8;
				  return TOK_FORMAT; }
ns:				{ yylval.num = 16;
				  return TOK_FORMAT; }
hs:				{ yylval.num = -16;
				  return TOK_FORMAT; }
nl:				{ yylval.num = 32;
				  return TOK_FORMAT; }
hl:				{ yylval.num = -32;
				  return TOK_FORMAT; }
ipv4:				{ yylval.num = 32;
				  return TOK_FORMAT; }
ipv6:				{ yylval.num = 128;
				  return TOK_FORMAT; }

[a-wyzA-Z_]|[a-zA-Z_][a-zA-Z_0-9]+ { yylval.str = stralloc(yytext);
				  return TOK_WORD; }

<DEV>"{"			{ BEGIN(TCC);
				  brace_level = 1;
				  return TOK_TCC; }
<TCC>"{"			{ brace_level++;
				  add_tcc("{"); }
<TCC>"}"			{ if (--brace_level) add_tcc("}");
				  else {
				    BEGIN(INITIAL);
				    if (tcc) {
					/* make sure there's a terminating \n */
					add_tcc("\n");
					tcng = run_tcc(current_dev,tcc);
					free(tcc);
					tcc = NULL;
					sim_file = YY_CURRENT_BUFFER;
					sim_file_name = file_name;
					file_name = NULL;
					sim_line = lineno;
					yy_scan_string(tcng);
				    }
				  }
				}
<TCC>[^\t\n{} ]+		add_tcc(yytext);

<*>\\\n[\t\n ]*			lineno += newlines(yytext);
				  /* redundant: cpp handles this */
<*>[\t ]+			;
<*>^#\ [0-9]+\ \"[^"]*\"(\ [0-9]+)*\n[\t\n ]* {
				  char *start = strchr(yytext,'"')+1;
				  char *end = strchr(start,'"');
				  char *s;
				  *end = 0;
				  if (file_name) free(file_name);
				  file_name = stralloc(start);
				  lineno = strtol(yytext+2,NULL,0);
				  lineno += newlines(end+1)-1; }

<INITIAL,DEV,TCC>\n[\t\n ]*	lineno += newlines(yytext);
<WORD>\n[\t\n ]*		{ BEGIN(INITIAL);
				  lineno += newlines(yytext);
				  return TOK_NL; }
#[^\n]*\n[\t\n ]*		lineno += newlines(yytext);
<INITIAL,DEV,PATH,WORD>;[^\n]*/\n[\t\n ]*
<WORD>#[^\n]*\n[\t\n ]*		{ BEGIN(INITIAL);
				  lineno += newlines(yytext);
				  return TOK_NL; }

">>"				return SHIFT_RIGHT;
"<<"				return SHIFT_LEFT;

<*>.				return *yytext;

<TCC><<EOF>>			yyerror("EOF in tcng specification");

<<EOF>>				{ if (!tcng) yyterminate();
				  yy_delete_buffer(YY_CURRENT_BUFFER);
				  file_name = sim_file_name;
				  lineno = sim_line;
				  free(tcng);
				  tcng = NULL;
				  yy_switch_to_buffer(sim_file); }

%%

static void vyywarnf(const char *fmt,va_list ap)
{
    fflush(stdout);
    fprintf(stderr,"%s:%d: ",file_name && *file_name ? file_name : "<stdin>",
      lineno);
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


void yyerrorf(const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    vyywarnf(fmt,ap);
    va_end(ap);
    exit(1);
}

void yyerror(const char *s)
{
    yywarn(s);
    exit(1);
}


static void vdebugf(const char *fmt,va_list ap)
{
    fflush(stdout);
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"\n");
    fflush(stderr);
}


static void vwarnf(const char *fmt,va_list ap)
{
    fflush(stdout);
    fprintf(stderr,"warning: ");
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"\n");
    fflush(stderr);
}


void debugf(const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    vdebugf(fmt,ap);
    va_end(ap);
}


void errorf(const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    vdebugf(fmt,ap);
    va_end(ap);
    exit(1);
}


void warnf(const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    vwarnf(fmt,ap);
    va_end(ap);
}
