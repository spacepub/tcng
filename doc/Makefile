#
# doc/Makefile - Build documentation
#
# Written 2001,2002 by Werner Almesberger
# Copyright 2001 Network Robots
# Copyright 2002 Bivio Networks, Werner Almesberger
#

.SUFFIXES:	.tex .txt .dvi .ps .pdf .fig .eps

COMMON=tcng.tex *.inc

FIG=build dlb incoming multi phtcc phtcsim slb srtcm tccsys tcsimsys trtcm
EPS=$(shell for n in $(FIG); do echo $$n.eps; done)
ASC=$(shell for n in $(FIG); do echo $$n.asc; done)

CLEAN=*.aux *.log *.toc *.dvi
SPOTLESS=*.txt *.ps *.pdf
IMMACULATE=$(EPS)

.PHONY:		all ephemeral clean spotless immaculate dep depend

all:		tcng.ps tcng.txt

tcng.dvi:	$(COMMON) $(EPS)
tcng.txt:	$(COMMON) $(ASC)

.tex.txt:
		perl ../scripts/t2x.pl $< >$@ || { rm -f $@; exit 1; }

.tex.dvi:
		../scripts/rlatex $<

.dvi.ps:
		dvips -o - $< | \
		  sed '/%%BeginDocument/,/%%EndDocument/s/%%.*//' >$@ || \
		  { rm -f $@; exit 1; }

.ps.pdf:
		ps2pdf $< $@ || { rm -rf $@; exit 1; }

.fig.eps:
		fig2dev -L eps $< $@

dep depend:

ephemeral:
		echo $(CLEAN) $(SPOTLESS)

clean:
		rm -f $(CLEAN)

spotless:	clean
		rm -f $(SPOTLESS)

immaculate:	spotless
		rm -f $(IMMACULATE)
