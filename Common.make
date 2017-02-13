#
# Common.make - Common items used by all Makefiles
#
# Written 2001,2002,2004 by Werner Almesberger
# Copyright 2001 EPFL-ICA
# Copyright 2001,2002 Bivio Networks, Network Robots
# Copyright 2004 Werner Almesberger
#

# CC_OPTS: CC options for tcsim
# pick one of CFLAGS_DEBUG, CFLAGS_PROFILE, or CFLAGS_OPT

CC_OPTS=$(CFLAGS_DEBUG)

# LD_OPTS: LD options for tcc and tcsim

LD_OPTS=#-lefence

# SF_SHELL: account on SourceForge, and project host name
# SF_DIR: Web base directory
# SF_UPLOAD: Upload directory
# SF_SCRIPT: Script to update release information

SF_ACCOUNT=almesber@tcng.sourceforge.net
SF_DIR=/home/groups/t/tc/tcng/htdocs
SF_UPLOAD=$(SF_ACCOUNT):$(SF_DIR)/dist
SF_SCRIPT=$(SF_DIR)/release.sh

#------------------------------------------------------------------------------

# tcc-module:			_*.mdi *__*.*o
# tcc-ext-*:			__*.in
# tcc-ext-*, tcc-ext-test:	__*.out

CLEAN_MOD=_*.mdi *__*.*o __*.in __*.out

CFLAGS_DEBUG=-g
CFLAGS_PROFILE=-pg
CFLAGS_OPT=-O -D__NO_STRING_INLINES
CFLAGS_WARN=-Wall -Wstrict-prototypes -Wmissing-prototypes \
	    -Wmissing-declarations
LEX=flex
YACC=`sed '/^YACC="\(.*\)"$$/s//\1/p;d' <../config`
YYFLAGS=-v

.PHONY:			clean-mod ephemeral-mod

ephemeral-mod:
			echo $(CLEAN_MOD)

clean-mod:
			rm -f $(CLEAN_MOD)
