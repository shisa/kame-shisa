# $FreeBSD: src/sys/modules/linux/Makefile,v 1.34.2.12 2003/01/02 20:41:34 kan Exp $

MAINTAINER=	marcel@FreeBSD.org

.PATH: ${.CURDIR}/../../compat/linux ${.CURDIR}/../../${MACHINE_ARCH}/linux

KMOD=	linux
SRCS=	linux_dummy.c linux_file.c linux_getcwd.c linux_ioctl.c linux_ipc.c \
	linux_machdep.c linux_mib.c linux_misc.c linux_signal.c linux_socket.c \
	linux_stats.c linux_sysctl.c linux_sysent.c linux_sysvec.c \
	linux_util.c opt_compat.h opt_linux.h opt_vmpage.h vnode_if.h
OBJS=	linux_locore.o
MAN=	linux.8

.if ${MACHINE_ARCH} == "i386"
SRCS+=	linux_ptrace.c linux_uid16.c imgact_linux.c opt_cpu.h
.endif

CLEANFILES=	linux_assym.h linux_genassym.o

linux_assym.h: linux_genassym.o
.if exists(@)
linux_assym.h: @/kern/genassym.sh
.endif
	sh @/kern/genassym.sh linux_genassym.o > ${.TARGET}

linux_locore.o: linux_locore.s linux_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

linux_genassym.o: linux_genassym.c linux.h @ machine
	${CC} -c ${CFLAGS} ${.IMPSRC}

opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h

afterinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/linux.sh ${DESTDIR}/usr/sbin/linux

.include <bsd.kmod.mk>
