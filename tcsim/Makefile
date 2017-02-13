#
# Makefile - Build tcsim (front-end)
#
# Written 2003 by Werner Almesberger
# Copyright 2003 Werner Almesberger
#

.PHONY:		all tcsim modules depend dep 
.PHONY:		clean spotless ephemeral immaculate

all:		tcsim modules

# ----- non-cleanup targets ---------------------------------------------------

tcsim:
		$(MAKE) -f Makefile.unclean tcsim

modules:
		$(MAKE) -f Makefile.unclean modules

depend dep:
		$(MAKE) -f Makefile.unclean depend

# ----- cleanup targets -------------------------------------------------------

ephemeral:
		$(MAKE) -f Makefile.clean ephemeral

clean:
		$(MAKE) -f Makefile.clean clean

spotless:
		$(MAKE) -f Makefile.clean spotless

immaculate:
		$(MAKE) -f Makefile.clean immaculate
