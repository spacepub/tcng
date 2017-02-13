# Copyright 1999-2002 Gentoo Technologies, Inc.
# Distributed under the terms of the GNU General Public License v2
# $Header: $
 
#made by raptor
DESCRIPTION="Traffic control Next Generation.Higher level than tc"
HOMEPAGE="http://tcng.sourceforge.net"
SRC_URI="http://tcng.sourceforge.net/dist/${P}.tar.gz"
LICENSE="GPL-2 LGPL-2.1"
SLOT="0"
KEYWORDS="x86"
 
# - additional packages needed after "make immaculate":
#   transfig (fig2dev), wget
 
DEPEND=">=virtual/linux-sources-2.4.19
>=perl-5.6.1
>=iproute-20010824-r2
>=gnuplot-3.7.1-r3
>=binutils-2.13.90.0.4
>=gcc-3.2-r4
>=make-3.80
>=flex-2.5.4a
>=yacc-1.9.1-r1
>=grep-2.5-r1
>=sed-4.0.1
>=gawk-3.1.1-r1
>=textutils-2.1
>=fileutils-4.1.11
>=sh-utils-2.0.15
>=tetex-1.0.7-r12
"
#>=LaTeX
#>=dvips
 
S=${WORKDIR}/tcng
 
src_compile() {
        #first put iproute sources in tcism
        #if tcism is to be compiled. we dont do that
 
        #create /var/tmp/portage/tcng-xxx/image/usr&bin in advance
        #so that configure doesnt complain that it doesnt exists
        dodir /usr/bin
 
        ./configure --no-tcsim \
                --install-directory ${D}usr --data-directory ${D}usr/lib/tcng
        emake || die
 
#       echo
#       einfo "Now will run tcng test suite.....\n\n"
#       sleep 3
#       make test
}
 
src_install() {
        dodir /usr/bin
 
        #For localization.sh to work corectly
        #TCNG INSTALL/TOPDIR has to be /usr, instead of /var/tmp/portage/tcng-xx/image/usr...
        TCNG_INSTALL_CWD=/usr einstall
}
