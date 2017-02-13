#
# Makefile - Build tcc
#
# Written 2001,2002,2004 by Werner Almesberger
# Copyright 2001 EPFL-ICA, Network Robots
# Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
# Copyright 2004 Werner Almesberger
#

all:		tcc tcc-module ext meters.tc need-ports.tc

include ../Common.make

OBJS=lex.yy.o y.tab.o tcc.o util.o error.o var.o data.o param.o device.o \
     registry.o sprintf.o sprintf_generic.o \
     qdisc.o q_ingress.o q_cbq.o q_dsmark.o q_fifo.o q_gred.o q_prio.o \
     q_red.o q_sfq.o q_tbf.o q_htb.o csp.o \
     filter.o f_if.o f_fw.o f_route.o f_rsvp.o f_tcindex.o \
     police.o tc.o op.o field.o named.o \
     if_u32.o if_c.o if_ext.o iflib_actdb.o target.o location.o \
     iflib_comb.o iflib_off.o iflib_misc.o iflib_red.o iflib_act.o \
     iflib_arith.o iflib_not.o iflib_bit.o iflib_cheap.o iflib_newbit.o \
     iflib_fastbit.o ext_all.o ext.o ext_io.o ext_dump.o

CLEAN=lex.yy.c y.tab.c y.tab.h y.output $(OBJS) \
  param_decl.inc param_dsc.inc param_reset.inc \
  param_stack.inc param_push.inc param_pop.inc \
  meters.tc port-numbers.tmp tccmeta.h \
  .depend

SPOTLESS=tcc tcc-module

IMMACULATE=ports.tc port-numbers

#
# Note: we don't remove port-numbers, because re-creating the file requires
# network access. Also, removing port-numbers would violate the principle that
# "make spotless" does not remove files accessed by "make dist".
#

CFLAGS_CONFIG=-DVERSION=\"`cat ../VERSION`\" \
  $(shell sed '/TOPDIR=\(.*\)/s//-DTOPDIR=\\\"\1\\\"/p;d' <../config) \
  $(shell sed '/DOLLAR=true/s//-DDOLLAR/p;d' <../config) \
  $(shell sed '/ACTION=false/s//-DCONFIRM_EXCEED/p;d' <../config)
CFLAGS=$(CC_OPTS) $(CFLAGS_WARN) -I../shared $(CFLAGS_CONFIG)
LIBS=-lfl -lm -L../shared -ltcngmisc $(LD_OPTS)

.PHONY:			all ext depend dep ephemeral clean spotless immaculate
.PHONY:			need-ports.tc ports.tc

tcc:			$(OBJS) Makefile ../Common.make
			$(CC) $(CC_OPTS) -o tcc $(OBJS) $(LIBS)

ext:
			$(MAKE) -C ext

lex.yy.c:		tcng.l y.tab.h
			$(LEX) tcng.l

lex.yy.o:		lex.yy.c y.tab.h
			$(CC) -c $(CFLAGS) -w lex.yy.c

y.tab.c y.tab.h:	tcng.y
			$(YACC) $(YYFLAGS) -d tcng.y

y.tab.o:		y.tab.c
			$(CC) -c $(CFLAGS) -w y.tab.c

param_decl.inc:		Parameters mkprminc
			./mkprminc decl >param_decl.inc || \
			  { rm -f param_decl.inc; exit 1; }

param_dsc.inc:		Parameters mkprminc
			./mkprminc dsc >param_dsc.inc || \
			  { rm -f param_dsc.inc; exit 1; }

param_reset.inc:	Parameters mkprminc
			./mkprminc reset >param_reset.inc || \
			  { rm -f param_reset.inc; exit 1; }

param_stack.inc:	Parameters mkprminc
			./mkprminc stack >param_stack.inc || \
			  { rm -f param_stack.inc; exit 1; }

param_push.inc:		Parameters mkprminc
			./mkprminc push >param_push.inc || \
			  { rm -f param_push.inc; exit 1; }

param_pop.inc:		Parameters mkprminc
			./mkprminc pop >param_pop.inc || \
			  { rm -f param_pop.inc; exit 1; }

param.c:		param_dsc.inc param_reset.inc param_stack.inc \
			param_push.inc param_pop.inc

tccmeta.h:		meta.tc meta.pl
			perl meta.pl >tccmeta.h || { rm -f tccmeta.h; exit 1; }

meters.tc:		meters.tc.in tcc_var2fix.pl Makefile
			./tcc_var2fix.pl meters.tc.in >meters.tc || \
			  { rm -f meters.tc; exit 1; }

need-ports.tc:
			[ -r ports.tc ] || $(MAKE) ports.tc

ports.tc:
			../scripts/update-port-numbers.sh || \
			  { rm -f port-numbers; exit 1; }
			../scripts/extract-port-numbers.pl port-numbers \
			  >ports.tc || { rm -f ports.tc; exit 1; }

tcc-module:		tcc-module.in ../config
			../scripts/topdir.sh .. tcc-module

$(OBJS):		.depend ../config \
			param_decl.inc # almost everything needs it

if_u32.o:		tccmeta.h
if_c.o:			tccmeta.h
tcc.o:			../VERSION

dep depend .depend:
			$(CPP) $(CFLAGS) -MM -MG \
			  `echo *.c | sed 's/\<tcm_\(cls\|f\)\.c\>//g'` \
			  >.depend || \
			  { rm -f .depend; exit 1; }

ephemeral:		ephemeral-mod
			echo $(CLEAN) $(SPOTLESS)
			$(MAKE) -C ext -s ephemeral | tr ' ' '\012' | \
			  sed 's|^|ext/|'

clean:			clean-mod
			$(MAKE) -C ext clean
			rm -f $(CLEAN)

spotless:		clean
			$(MAKE) -C ext spotless
			rm -f $(SPOTLESS)

immaculate:		spotless
			$(MAKE) -C ext immaculate
			rm -f $(IMMACULATE)

ifeq (.depend,$(wildcard .depend))
include .depend
endif
