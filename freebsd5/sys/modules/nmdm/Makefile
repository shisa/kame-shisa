# $FreeBSD: src/sys/modules/nmdm/Makefile,v 1.2 2003/03/05 08:16:29 das Exp $

.PATH: ${.CURDIR}/../../dev/nmdm

KMOD=	nmdm
SRCS=	nmdm.c
SRCS+=	opt_compat.h opt_tty.h vnode_if.h

opt_compat.h:
	echo "#define COMPAT_43 1" >opt_compat.h

opt_tty.h:
	echo "#define TTYHOG 8192" >opt_tty.h

.include <bsd.kmod.mk>
