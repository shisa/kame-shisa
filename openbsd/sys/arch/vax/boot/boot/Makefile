#	$OpenBSD: Makefile,v 1.5 2003/08/10 15:51:18 deraadt Exp $
#	$NetBSD: Makefile,v 1.27 2002/04/07 07:00:25 matt Exp $

S!=	cd ${.CURDIR}/../../../../; pwd
NOMAN=	# defined

.include <bsd.own.mk>

PROG=	boot
WARNS?=	1
DEVS=	hp.c ctu.c ra.c mfm.c if_qe.c if_le.c if_ze.c if_de.c if_ni.c
SRCS=	srt0.S boot.c devopen.c conf.c autoconf.c netio.c rom.c romread.S \
	urem.s udiv.s consio.c consio2.S str.S ${DEVS} findcpu.c

CLEANFILES+=${PROG} ${PROG}.sym ${PROG}.mop
CPPFLAGS+=-DSUPPORT_BOOTPARAMS -DSUPPORT_DHCP -D_STANDALONE \
	-DNO_MID_CHECK
#CPPFLAGS+=-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG -DNFS_DEBUG -DDEV_DEBUG \
#	-DRPC_DEBUG -DRARP_DEBUG -DPARANOID

BINDIR=/usr/mdec

#SA_AS= library
#SAMISCMAKEFLAGS=SA_USE_CREAD=yes SA_INCLUDE_NET=yes SA_USE_LOADFILE=yes
SA_ZLIB=	yes
SAREL=
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=	${SALIB}

Z_AS=	library
CFLAGS+=-I${S}/lib/libsa 
.include "${S}/lib/libz/Makefile.inc"
LIBZ=	${ZLIB}

#KERN_AS=library
#.include "${S}/lib/libkern/Makefile.inc"
#LIBKERN=${KERNLIB}

.if ${MACHINE} == "vax"
.PHONY: machine-links
beforedepend: machine-links
machine-links:
	@rm -f machine && ln -s ${S}/arch/${MACHINE}/include machine
	@rm -f ${MACHINE_ARCH} && ln -s ${S}/arch/${MACHINE_ARCH}/include ${MACHINE_ARCH}
.NOPATH: machine ${MACHINE_ARCH}
CLEANFILES+= machine ${MACHINE_ARCH}
.endif

#.if ${OBJECT_FMT} == "ELF"
#START=start
#.else
START=nisse
#.endif

${PROG}: machine-links ${OBJS} ${LIBSA} ${LIBZ} ${LIBKERN}
	${LD} -N -Ttext ${RELOC} -e ${START} -o ${PROG}.sym -Llib/sa -L. ${OBJS} \
	    ${LIBSA} ${LIBZ} ${LIBSA} ${LIBKERN}
	/usr/sbin/mopa.out ${PROG}.sym ${PROG}.mop
	/bin/cp ${PROG}.sym ${PROG}
	/usr/bin/strip ${PROG}
	/usr/bin/size ${PROG}

clean::	
	rm -f a.out [Ee]rrs mklog core *.core ${PROG} ${OBJS} ${LOBJS} \
	    ${CLEANFILES}

#install:
#	${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} -m 555 \
#	    ${PROG} ${DESTDIR}${MDEC_DIR}/${PROG}

.include <bsd.prog.mk>
