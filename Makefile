#
# Makefile - Build tcng (tcng and tcsim)
#
# Written 2001-2004 by Werner Almesberger
# Copyright 2001 EPFL-ICA
# Copyright 2001,2002 Bivio Networks, Network Robots
# Copyright 2002-2004 Werner Almesberger
#

all:	config symlinks
	for n in $(DIRS); do \
	  ( grep TCSIM=false config >/dev/null && [ $$n = tcsim ]; ) || \
	  ( grep BUILD_MANUAL=false config >/dev/null && [ $$n = doc ]; ) || \
	    $(MAKE) -C $$n all || exit 1; done

include Common.make

DIRS=shared tcc tcsim scripts doc build

FILES=README CHANGES VERSION TODO COPYING.GPL COPYING.LGPL \
  Makefile Common.make \
  configure \
  build/README build/Makefile build/bytesex.c \
  build/tcng.spec.in build/tcsim.spec.in build/valgrind.supp.in \
  build/findsrc \
  build/tcng.ebuild \
  doc/README doc/Makefile doc/tcng.tex doc/preamble.inc \
  doc/intro.inc \
    doc/tccsys.fig doc/tccsys.eps doc/tccsys.asc \
    doc/tcsimsys.fig doc/tcsimsys.eps doc/tcsimsys.asc \
  doc/tcng.inc \
    doc/incoming.fig doc/incoming.eps doc/incoming.asc \
    doc/slb.fig doc/slb.eps doc/slb.asc \
    doc/dlb.fig doc/dlb.eps doc/dlb.asc \
    doc/srtcm.fig doc/srtcm.eps doc/srtcm.asc \
    doc/trtcm.fig doc/trtcm.eps doc/trtcm.asc \
  doc/tcc.inc \
    doc/phtcc.fig doc/phtcc.eps doc/phtcc.asc \
  doc/tcsim.inc \
    doc/phtcsim.fig doc/phtcsim.eps doc/phtcsim.asc \
    doc/multi.fig doc/multi.eps doc/multi.asc \
  doc/external.inc \
    doc/build.fig doc/build.eps doc/build.asc \
  doc/param.inc doc/bib.inc \
  doc/README.tccext \
  tcc/README tcc/CHANGES.old tcc/LANGUAGE tcc/PARAMETERS tcc/example \
  tcc/Makefile tcc/Parameters tcc/mkprminc tcc/meta.pl tcc/tcc_var2fix.pl \
  tcc/tcc_var2fix.pl_old \
  tcc/default.tc tcc/meta.tc tcc/fields.tc tcc/fields4.tc tcc/fields6.tc \
  tcc/values.tc tcc/meters.tc.in tcc/ports.tc tcc/idiomatic.tc \
  tcc/tcc.c tcc/tcng.l tcc/tcng.y tcc/config.h tcc/error.h tcc/error.c \
  tcc/util.h tcc/util.c tcc/var.h tcc/var.c tcc/data.h tcc/data.c \
  tcc/registry.h tcc/registry.c \
  tcc/sprintf.h tcc/sprintf.c tcc/sprintf_generic.h tcc/sprintf_generic.c \
  tcc/device.h tcc/device.c \
  tcc/qdisc.h tcc/qdisc.c tcc/qdisc_common.h tcc/q_ingress.c tcc/q_cbq.c \
  tcc/q_dsmark.c tcc/q_fifo.c tcc/q_gred.c tcc/q_prio.c tcc/q_red.c \
  tcc/q_sfq.c tcc/q_tbf.c tcc/q_htb.c tcc/csp.c \
  tcc/filter.h tcc/filter.c tcc/filter_common.h tcc/f_if.c tcc/f_fw.c \
  tcc/f_route.c tcc/f_rsvp.c tcc/f_tcindex.c \
  tcc/police.h tcc/police.c tcc/param.h tcc/param.c \
  tcc/tc.h tcc/tc.c tcc/tree.h tcc/op.h tcc/op.c tcc/tcdefs.h \
  tcc/field.h tcc/field.c tcc/named.h tcc/named.c tcc/target.h tcc/target.c \
  tcc/if.h tcc/if_u32.c tcc/if_c.c tcc/if_ext.c tcc/iflib_actdb.c \
  tcc/iflib.h tcc/iflib_comb.c tcc/iflib_misc.c tcc/iflib_off.c \
  tcc/iflib_red.c tcc/iflib_act.c tcc/iflib_arith.c tcc/iflib_not.c \
  tcc/iflib_cheap.c tcc/iflib_bit.c tcc/iflib_newbit.c tcc/iflib_fastbit.c \
  tcc/ext_all.h tcc/ext_all.c tcc/ext.h tcc/ext.c \
  tcc/ext_io.c tcc/ext_dump.c tcc/location.h tcc/location.c \
  tcc/tcc-module.in tcc/tcm_cls.c tcc/tcm_f.c \
  tcc/ext/Makefile tcc/ext/tccext.h tcc/ext/tccext.c tcc/ext/match.c \
  tcc/ext/cls_ext_test.c tcc/ext/f_ext_test.c tcc/ext/tcc-ext-test.in \
  tcc/ext/tcc-ext-err tcc/ext/tcc-ext-null tcc/ext/tcc-ext-file \
  tcc/ext/tcc-ext-xlat tcc/ext/tcc-ext-abort \
  tcc/ext/echo.c tcc/ext/echoh.h tcc/ext/echoh.c tcc/ext/echoh_main.c \
  tcsim/README tcsim/CHANGES.old tcsim/BUGS \
  tcsim/Makefile tcsim/Makefile.unclean tcsim/Makefile.clean \
  tcsim/default.tcsim \
  tcsim/ip.def tcsim/packet.def tcsim/packet4.def tcsim/packet6.def \
  tcsim/tcngreg.def \
  tcsim/tcsim_pretty tcsim/tcsim_filter tcsim/tcsim_plot \
  tcsim/tcsim.c tcsim/tcsim.h tcsim/tckernel.h \
  tcsim/jiffies.h tcsim/jiffies.c tcsim/timer.h tcsim/timer.c \
  tcsim/command.h tcsim/command.c tcsim/trace.c tcsim/var.h tcsim/var.c \
  tcsim/attr.h tcsim/attr.c \
  tcsim/host.h tcsim/host.c tcsim/module.c tcsim/cfg.l tcsim/cfg.y \
  tcsim/Makefile.klib tcsim/setup.klib tcsim/ksvc.c tcsim/klink.c \
  tcsim/Makefile.ulib tcsim/setup.ulib tcsim/usvc.c \
  tcsim/modules/Makefile tcsim/modules/kmod_cc.in tcsim/modules/tcmod_cc.in \
  tcsim/modules/sch_discard.c tcsim/modules/q_discard.c \
  tcsim/modules/cls_unspec.c tcsim/modules/f_unspec.c \
  shared/Makefile \
  shared/u128.h shared/u128.c shared/addr.h shared/addr.c \
  shared/memutil.h shared/memutil.c \
  toys/tunnel toys/reminiscence toys/cspnest toys/comtc \
  scripts/Makefile scripts/topdir.sh scripts/trinity.sh.in scripts/t2x.pl \
  scripts/runtests.sh scripts/rlatex scripts/rndlogtst.pl scripts/fsm2dav.pl \
  scripts/symlinks.sh scripts/cvsupdate.sh scripts/localize.sh \
  scripts/uninstall.sh \
  scripts/run-all-tests scripts/minksrc.sh scripts/minisrc.sh \
  scripts/compatibility.sh \
  scripts/update-port-numbers.sh scripts/extract-port-numbers.pl \
  examples/pfifo_fast examples/prio+rsvp examples/dsmark+policing examples/tbf \
  examples/ef-prio examples/ingress examples/gred examples/prio+fw \
  examples/sfq examples/u32 \
  examples-ng/pfifo_fast examples-ng/prio+rsvp \
  examples-ng/dsmark+policing examples-ng/tbf examples-ng/ef-prio \
  examples-ng/ingress examples-ng/gred examples-ng/prio+fw \
  examples-ng/sfq examples-ng/priority examples-ng/xdev examples-ng/u32 \
  tests/README \
  tests/tbf tests/basic tests/ingress tests/echo tests/route tests/modules \
  tests/host tests/trace tests/variables tests/if tests/ifopt tests/dscp \
  tests/c tests/ext tests/additive tests/pol_ext tests/misfeatures \
  tests/named tests/pol_traffic tests/field tests/arith tests/not \
  tests/pragma tests/vartype tests/unspec tests/tcm tests/bitopt tests/extqch \
  tests/defifo tests/until tests/extflt tests/drop tests/usage tests/location \
  tests/tag tests/extndm tests/defdsm tests/defcbq tests/phasex tests/extcbq \
  tests/extmult tests/cbqroot tests/dupext tests/examples tests/sort \
  tests/fredef tests/devbra tests/varuse tests/dsind tests/selpath \
  tests/selpathext tests/selpathgred tests/fastpath tests/expensive \
  tests/classvar tests/varexp tests/ifnamsiz tests/grederr tests/selpathcbq \
  tests/extlocbas tests/extlocext tests/warn tests/selpathdup tests/varredef \
  tests/relop tests/phasep tests/waprpos tests/meters tests/field2 \
  tests/precond tests/subbyte tests/buckfill tests/semicolon tests/extrasemi \
  tests/newsynmsc tests/string tests/named2 tests/mettagpra tests/bands \
  tests/access tests/bittest tests/ports tests/typerr tests/cppdollar \
  tests/blocks tests/extend tests/cpp31 tests/bitfield tests/bare \
  tests/packet tests/relpref tests/cse tests/ne tests/grpalloc tests/froot \
  tests/bucket tests/compound tests/struct tests/idiomatic tests/includes \
  tests/u32drop tests/meters2 tests/u32slb tests/u32dlb tests/metmac \
  tests/nocombsta tests/nocombmet tests/barrier tests/ingext tests/fastpref \
  tests/varhash tests/nullcsp tests/ingin tests/fastpol tests/egress \
  tests/dpcwng tests/merged tests/ipv4 tests/ipv6 tests/decerr tests/tcsiru \
  tests/ipv6u32 tests/ipv6ext tests/definc tests/intro tests/range \
  tests/ipv6tcs tests/ipv4tcs tests/extall tests/ipv6relar tests/u32trtcm \
  tests/u32srtcm tests/ipv6relne tests/ipv6relge tests/relmask tests/sprintf \
  tests/tcssnap tests/mpu tests/tcsenq tests/cbqzero tests/tcssi \
  tests/tcsrelt tests/tstcond tests/htb tests/protu32 tests/constpfx \
  tests/htbng tests/tcsattpsv tests/tcsattset tests/tcsattpro tests/dropon \
  tests/tcsdefinc tests/tcstimstp tests/htbquant tests/newexp tests/comtc \
  tests/tccin tests/protcomb tests/protc tests/protext tests/metau32 \
  tests/u32pol tests/tbfqdsyn tests/tbfqdtc tests/tbfqdext tests/tbfqdrun \
  tests/tcng-1g tests/tcng-1h tests/tcng-1i tests/tcng-1j tests/tcng-1n \
  tests/tcng-1o tests/tcng-2c tests/tcng-2e tests/tcng-2f tests/tcng-2h \
  tests/tcng-2i tests/tcng-2j tests/tcng-2k tests/tcng-2l tests/tcng-2n \
  tests/tcng-2o tests/tcng-2q tests/tcng-2u tests/tcng-3b tests/tcng-3g \
  tests/tcng-3h tests/tcng-3i tests/tcng-3k tests/tcng-3m tests/tcng-3n \
  tests/tcng-3p tests/tcng-3r tests/tcng-3s tests/tcng-3t tests/tcng-4a \
  tests/tcng-4c tests/tcng-4d tests/tcng-4f tests/tcng-4h tests/tcng-4i \
  tests/tcng-4j tests/tcng-4l tests/tcng-4n tests/tcng-4o tests/tcng-4p \
  tests/tcng-4u tests/tcng-4w tests/tcng-4z tests/tcng-5a tests/tcng-5c \
  tests/tcng-5f tests/tcng-5g tests/tcng-5j tests/tcng-5q tests/tcng-5s \
  tests/tcng-5y tests/tcng-5z tests/tcng-6b tests/tcng-6c tests/tcng-6d \
  tests/tcng-6j tests/tcng-6n tests/tcng-6o tests/tcng-6r tests/tcng-6t \
  tests/tcng-6u tests/tcng-6v tests/tcng-6w tests/tcng-6x tests/tcng-6y \
  tests/tcng-6z tests/tcng-7a tests/tcng-7c tests/tcng-7f tests/tcng-7g \
  tests/tcng-7h tests/tcng-7j tests/tcng-7m tests/tcng-7n tests/tcng-7o \
  tests/tcng-7p tests/tcng-7q tests/tcng-7t tests/tcng-7u tests/tcng-7w \
  tests/tcng-7y tests/tcng-7z tests/tcng-8a tests/tcng-8h tests/tcng-8j \
  tests/tcng-8q tests/tcng-8t tests/tcng-8u tests/tcng-8x tests/tcng-8y \
  tests/tcng-8z tests/tcng-9a tests/tcng-9c tests/tcng-9g tests/tcng-9m \
  tests/lib/nested.tc tests/lib/nested.tcsim tests/lib/additive.tc \
  tests/lib/additive.tcsim tests/lib/pol_single.tcsim \
  tests/lib/pol_single_drop.tcsim \
  patches/tc-rsvp-gpi patches/k-u32-offset patches/tc-netlink-buf \
  patches/tc-dsmark-dflt patches/tc-u32-incr

# runtests.sh:	_in.* _ref.* _out.* _err.*
# runtests.sh:	_tmp_err.* _tmp_out.*
# trinity.sh:	_tri_*.*
# tcc-ext-test:	f_m*.so cls_m*.o

CLEAN_TEST=_in.* _ref.* _out.* _err.* \
  _tmp_err.* _tmp_out.* _tri_*.* \
  f_m*.so cls_m*.o
CLEAN=.symlinks .cpptest config.tmp .configure.tmp
SPOTLESS=build/bytesex config .distisuptodate \
  $(shell scripts/symlinks.sh list .)
SPOTLESS_DIRS=build-test rpm bin lib

INSTALL_DIR=$(shell sed 's/^INSTALL_DIR=//p;d' config)

TCC_BINDIST=localize.sh \
  bin/tcc bin/tcc_var2fix.pl lib/tcng/bin/tcc-module \
  lib/tcng/bin/tcc-ext-err lib/tcng/bin/tcc-ext-null \
  lib/tcng/bin/tcc-ext-file \
  lib/tcng/lib/libtccext.a lib/tcng/lib/tcm_cls.c lib/tcng/lib/tcm_f.c \
  lib/tcng/include/tccext.h lib/tcng/include/echoh.h \
  lib/tcng/include/tccmeta.h \
  lib/tcng/include/default.tc \
  lib/tcng/include/meta.tc \
  lib/tcng/include/fields.tc lib/tcng/include/values.tc \
  lib/tcng/include/fields4.tc lib/tcng/include/fields6.tc \
  lib/tcng/include/meters.tc lib/tcng/include/ports.tc \
  lib/tcng/include/idiomatic.tc
TCSIM_BINDIST=localize.sh \
  bin/tcsim_filter bin/tcsim_plot bin/tcsim_pretty \
  lib/tcng/bin/kmod_cc lib/tcng/bin/tcmod_cc \
  bin/tcsim \
  lib/tcng/include/default.tcsim lib/tcng/include/ip.def \
  lib/tcng/include/packet.def \
  lib/tcng/include/packet4.def lib/tcng/include/packet6.def \
  lib/tcng/include/tcngreg.def \
  lib/tcng/include/klib/include \
  lib/tcng/include/ulib/iproute2/include \
  lib/tcng/include/ulib/iproute2/include-glibc \
  lib/tcng/include/ulib/iproute2/tc/tc_util.h \
  lib/tcng/include/ulib/iproute2/tc/tc_core.h
TCNG_TESTS_BINDIST=run-all-tests scripts/runtests.sh scripts/trinity.sh \
  tcc/ext/tcc-ext-echo tcc/ext/tcc-ext-echoh tcc/ext/tcc-ext-test \
  tcc/ext/tcc-ext-xlat \
  tcc/ext/cls_ext_test.c tcc/ext/f_ext_test.c tcc/ext/match.c \
  tcsim/modules/cls_unspec.o tcsim/modules/f_unspec.so \
  tcsim/modules/q_discard.so tcsim/modules/sch_discard.o \
  tests/README tests/[a-z]* tests/lib/[a-z]* \
  toys/comtc \
  examples/* examples-ng/* tcc/example

.PHONY:			all dist test-build build-test dep depend symlinks
.PHONY:			upload sf-upload upload-sf cvsupdate need-tcsim
.PHONY:			test tests count-tests valgrind compatibility
.PHONY:			bindist bindist-tcc bindist-tcsim bindist-tcng-tests
.PHONY:			install install-tcc install-tcsim install-tests
.PHONY:			uninstall uninstall-tcc uninstall-tcsim uninstall-tests
.PHONY:			check-install-dir
.PHONY:			clean spotless clean-test clean-tests rmaltdist
.PHONY:			rpm rpm-setup tcc-rpms tcsim-rpms
.PHONY:			gentoo
.PHONY:			$(DIRS)

config:			configure
			./configure

tcc:			symlinks shared
			$(MAKE) -C tcc LD_OPTS=$(LD_OPTS)

tcsim:			need-tcsim symlinks shared
			$(MAKE) -C tcsim

need-tcsim:		config
			grep TCSIM=true config >/dev/null || { \
			    echo \
			     "need tcsim for this (disabled in configuration)" \
			      1>&2; \
			    exit 1; \
			}

# ----- Setup -----------------------------------------------------------------

scripts:
			$(MAKE) -C scripts

symlinks:		.symlinks scripts/symlinks.sh

.symlinks:		scripts/symlinks.sh config
			scripts/symlinks.sh link .
			touch .symlinks

dep depend:
			for n in $(DIRS); do \
			  $(MAKE) -C $$n dep || exit 1; done

# ----- Testing ---------------------------------------------------------------

test tests:		tcc tcsim scripts
			. config; [ -z "$$NO_TESTS" ] || \
			  { echo "tests not available: $$NO_TESTS"; exit 1; }
			scripts/run-all-tests $(RUNTESTS_FLAGS)

count-tests:
			TCNG_TESTS_COUNT=y scripts/run-all-tests

test-build build-test:	dist config need-tcsim
			rm -rf test-build
			mkdir test-build
			cd test-build && \
			  tar xfz ../tcng-`cat ../VERSION`.tar.gz && \
			  cd tcng && \
			  ./configure `sed '/^KSRC=/s//-k /p;d' ../../config` \
			    `sed '/^ISRC=/s//-i /p;d' ../../config` && \
			  make && make tests
			rm -rf test-build

valgrind:		tcc tcsim scripts
			scripts/run-all-tests -g $(RUNTESTS_FLAGS)

compatibility:
			scripts/compatibility.sh

# ----- Distribution ----------------------------------------------------------

dist:			.distisuptodate

.distisuptodate:	$(FILES)
			@if sed 's/#.*//' <Common.make | \
			  grep '[-]lefence' >/dev/null; then \
			    echo 'you forgot to remove @lefence' | \
			      tr @ - 1>&2; \
			    exit 1; \
			fi
			cd ..; tar cfz tcng/tcng-`cat tcng/VERSION`.tar.gz \
			  `echo $(FILES) | xargs -n 1 echo | sed 's|^|tcng/|'`
			touch .distisuptodate

# ----- Binary distribution ---------------------------------------------------

bindist-tcng:		tcc
			ln -sf scripts/localize.sh .
			tar cfzh tcng-`cat VERSION`-bin.tar.gz $(TCC_BINDIST)
			rm -f localize.sh

bindist-tcsim:		tcsim
			ln -sf scripts/localize.sh .
			tar --ignore-failed-read -czhf \
			  tcsim-`cat VERSION`-bin.tar.gz $(TCSIM_BINDIST)
			rm -f localize.sh

bindist-tcng-tests:	tcc tcsim scripts
			ln -sf scripts/run-all-tests .
			tar cfz tcng-tests-`cat VERSION`-bin.tar.gz \
			  $(TCNG_TESTS_BINDIST)
			rm -f run-all-tests

bindist:		bindist-tcng bindist-tcsim # bindist-tcng-tests

# ----- install ---------------------------------------------------------------

check-install-dir:
			@[ ! -z "$(INSTALL_DIR)" ] || { \
			  echo "no installation directory configured" 1>&2; \
			  exit 1; }

install-tcng:		tcc check-install-dir scripts/localize.sh
			ln -sf scripts/localize.sh .
			tar cfh - $(TCC_BINDIST) | \
			  (cd $(DESTDIR)$(INSTALL_DIR) && tar xf -)
			cd $(DESTDIR)$(INSTALL_DIR) && ./localize.sh $(DESTDIR)
			rm -f localize.sh $(DESTDIR)$(INSTALL_DIR)/localize.sh

install-tcsim:		tcsim check-install-dir scripts/localize.sh
			ln -sf scripts/localize.sh .
			tar --ignore-failed-read cfh - $(TCSIM_BINDIST) | \
			  (cd $(DESTDIR)$(INSTALL_DIR) && tar xf -)
			cd $(DESTDIR)$(INSTALL_DIR) && ./localize.sh $(DESTDIR)
			rm -f localize.sh $(DESTDIR)$(INSTALL_DIR)/localize.sh

install-tests:		tcsim check-install-dir
			mkdir -p $(DESTDIR)$(INSTALL_DIR)/lib/tcng/tests
			ln -sf scripts/run-all-tests .
			tar cfh - $(TCNG_TESTS_BINDIST) | \
			  (cd $(DESTDIR)$(INSTALL_DIR)/lib/tcng/tests && tar xf -)
			rm -f run-all-tests

install:
			for n in tcng tcsim; do \
			  grep TCSIM=false config >/dev/null && \
			  [ $$n = tcsim ] || $(MAKE) install-$$n || exit 1; \
			  done

# ----- uninstall -------------------------------------------------------------

uninstall-tcc uninstall-tcng:
			scripts/uninstall.sh $(DESTDIR)$(INSTALL_DIR) $(TCC_BINDIST)
			rm -f $(DESTDIR)$(INSTALL_DIR)/bin/tcc.bin
			rm -f $(DESTDIR)$(INSTALL_DIR)/bin/tcng

uninstall-tcsim:
			scripts/uninstall.sh $(DESTDIR)$(INSTALL_DIR) $(TCSIM_BINDIST)
			-rmdir $(DESTDIR)$(INSTALL_DIR)/lib/tcng/include/ulib
			-rmdir $(DESTDIR)$(INSTALL_DIR)/lib/tcng/include
			rm -f $(DESTDIR)$(INSTALL_DIR)/bin/tcsim.bin
			
uninstall-tests:
			scripts/uninstall.sh $(DESTDIR)$(INSTALL_DIR)/lib/tcng/tests \
			  $(TCNG_TESTS_BINDIST)
			-rmdir $(DESTDIR)$(INSTALL_DIR)/lib/tcng/tests/tcsim
			-rmdir $(DESTDIR)$(INSTALL_DIR)/lib/tcng/tests

uninstall:		uninstall-tcng uninstall-tcsim
			[ ! -d $(DESTDIR)$(INSTALL_DIR)/lib/tcng/tests ] || \
			  $(MAKE) uninstall-tests
			-rmdir $(DESTDIR)$(INSTALL_DIR)/lib/tcng

# ----- RPMs ------------------------------------------------------------------

RPMBUILD=$(shell if rpmbuild --version >/dev/null 2>&1; then \
	   echo rpmbuild; else echo rpm; fi)

rpm:			tcng-rpms #tcsim-rpms

rpm-setup:		dist
			$(MAKE) -C build rpmstuff
			mkdir -p rpm/SOURCES rpm/BUILD rpm/RPMS rpm/SRPMS
			ln -sf `pwd`/tcng-`cat VERSION`.tar.gz rpm/SOURCES

tcng-rpms:		rpm-setup
			$(RPMBUILD) --rcfile build/rpmrc -ba build/tcng.spec
			mv rpm/RPMS/*/tcng-`cat VERSION`-*.rpm .
			mv rpm/RPMS/*/tcng-devel-`cat VERSION`-*.rpm .
			mv rpm/SRPMS/tcng-`cat VERSION`-*.src.rpm .
			rm -rf rpm

#
# Similar to the variables in build/Makefile, but not identical
#
SUGGESTED_KERNEL=`sed '/^$$ wget .*\/\(linux-[^/]*$$\)/s//\1/p;d' <README`
SUGGESTED_IPROUTE =`sed '/^$$ wget .*\/\(iproute2-[^/]*$$\)/s//\1/p;d' <README`

RPM_SOURCES=$(shell rpm --eval='%_sourcedir')
SOURCE_DIRS=$(shell pwd) /usr/src $(RPM_SOURCES)

tcsim-rpms:		rpm-setup README
			kernel=`PATH=.:$$PATH build/findsrc -k \
			  $(SOURCE_DIRS)`; \
			iproute=`PATH=.:$$PATH build/findsrc -i \
			  $(SOURCE_DIRS)`; \
			  if [ -z "$$kernel" ]; then \
			    echo "Please download the kernel source, e.g.\
$(SUGGESTED_KERNEL)" 1>&2; \
			    exit 1; \
			  fi; \
			  if [ -z "$$iproute" ]; then \
			    echo "Please download the iproute2 source, e.g.\
$(SUGGESTED_IPROUTE)" 1>&2; \
			    exit 1; \
			  fi; \
			  ln -sf $$kernel $$iproute rpm/SOURCES
			$(RPMBUILD) --rcfile build/rpmrc -bb build/tcsim.spec
			mv rpm/RPMS/*/tcsim-`cat VERSION`-*.rpm .
			mv rpm/RPMS/*/tcsim-devel-`cat VERSION`-*.rpm .
			mv rpm/RPMS/*/tcng-tests-`cat VERSION`-*.rpm .
			rm -rf rpm

# ----- Gentoo ----------------------------------------------------------------

#
# Note: this ebuild isn't particularly good. The one from Gentoo is light years
# better.
#

GENTOO_TCNG_DIR=/usr/portage/net-misc/tcng
GENTOO_EBUILD=$(GENTOO_TCNG_DIR)/tcng-`cat VERSION`.ebuild

gentoo:
			mkdir -p $(GENTOO_TCNG_DIR)
			cp build/tcng.ebuild $(GENTOO_EBUILD)
			ebuild $(GENTOO_EBUILD) digest
			emerge -p tcng
			emerge tcng

# ----- Uploads ---------------------------------------------------------------

upload sf-upload upload-sf:	dist
			md5sum tcng-`cat VERSION`.tar.gz
			scp tcng-`cat VERSION`.tar.gz CHANGES $(SF_UPLOAD)
			ssh $(SF_ACCOUNT) $(SF_SCRIPT) $(SF_DIR) `cat VERSION`

cvsupdate:
			@scripts/cvsupdate.sh

# ----- Cleanup ---------------------------------------------------------------

ephemeral:		ephemeral-mod
			for n in $(DIRS); do \
			  $(MAKE) -C $$n -s ephemeral | tr ' ' '\012' | \
			  sed "s|^|$$n/|" || exit 1; done
			echo $(SPOTLESS) $(CLEAN) $(CLEAN_TEST)
			[ ! -d test-build ] || find test-build -print

clean-test clean-tests:	clean-mod
			rm -f $(CLEAN_TEST)

clean:			clean-test
			for n in $(DIRS); do \
			  $(MAKE) -C $$n clean || exit 1; done
			rm -f $(CLEAN)
			rm -rf test-build

spotless:		clean
			for n in $(DIRS); do \
			  $(MAKE) -C $$n spotless || exit 1; done
			rm -f $(SPOTLESS)
			rm -rf $(SPOTLESS_DIRS)

immaculate:		spotless
			for n in $(DIRS); do \
			  $(MAKE) -C $$n immaculate || exit 1; done
			rm -f $(IMMACULATE)

rmaltdist:		# ... foo-* and foo-devel-* ... is of course redundant
			rm -f tcng-*-bin.tar.gz tcsim-*-bin.tar.gz
			rm -f tcng-tests-*-bin.tar.gz
			rm -f tcng-*-*.*.rpm tcng-devel-*-*.*.rpm
			rm -f tcsim-*-*.*.rpm tcsim-devel-*-*.*.rpm
			rm -f tcng-tests-*-*.*.rpm
			rm -f tcng-*-*.src.rpm

# ----- Help on make targets -------------------------------------------------

help:
			@echo 'Useful make targets:'
			@echo '  all        build tcng and tcsim (default)'
			@echo '  tests      run all regression tests'
			@echo '  valgrind   like "tests", but use valgrind'
			@echo '  install    install tcng and tcsim'
			@echo '  uninstall  uninstall tcng and tcsim'
			@echo '  dist       create tcng-<version>.tar.gz'
			@echo '  rpm        make an RPM for tcng'
			@echo '  tcsim-rpms make RPMs for tcsim and tests'
			@echo '  clean      remove temporary files'
			@echo '  spotless   remove all generated file'
			@echo '  immaculate like spotless, plus pre-built files'
