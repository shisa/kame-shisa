# $FreeBSD: src/sys/modules/svr4/Makefile,v 1.13.2.5 2002/08/07 16:32:00 ru Exp $

MAINTAINER=	newton@FreeBSD.org

.PATH:	${.CURDIR}/../../${MACHINE_ARCH}/svr4 ${.CURDIR}/../../svr4
KMOD=	svr4
SRCS=	svr4_sysent.c svr4_sysvec.c opt_compat.h opt_svr4.h opt_vmpage.h \
	vnode_if.h imgact_svr4.c svr4_signal.c svr4_fcntl.c svr4_misc.c \
	svr4_ioctl.c svr4_stat.c svr4_filio.c svr4_ttold.c svr4_termios.c \
	svr4_stream.c svr4_socket.c svr4_sockio.c svr4_machdep.c \
	svr4_resource.c svr4_ipc.c
OBJS=	svr4_locore.o 
MAN=	svr4.8

EXPORT_SYMS=_svr4_mod
CLEANFILES= svr4_assym.h svr4_genassym.o

svr4_assym.h:	svr4_genassym.o
.if exists(@)
svr4_assym.h:	@/kern/genassym.sh
.endif
	sh @/kern/genassym.sh svr4_genassym.o > ${.TARGET}

svr4_locore.o:	svr4_locore.s svr4_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
		${.IMPSRC} -o ${.TARGET}

svr4_genassym.o:	svr4_genassym.c svr4.h @ machine
	${CC} -c ${CFLAGS} ${.IMPSRC}

opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h

opt_svr4.h:
	touch opt_svr4.h
.if defined(DEBUG)
	echo "#define DEBUG_SVR4 1" >> opt_svr4.h
.endif

afterinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/svr4.sh ${DESTDIR}/usr/sbin/svr4

.include <bsd.kmod.mk>
