# $Id: Makefile,v 1.1.1.1 2004/09/22 07:23:40 t-momose Exp $
# $FreeBSD: src/sys/modules/dcons/Makefile,v 1.1.2.2 2003/11/16 09:54:13 simokawa Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons
SRCS	= dcons.c dcons.h \
		opt_dcons.h opt_ddb.h opt_comconsole.h \
		opt_simos.h

opt_ddb.h:
	echo "#define DDB 1" > $@

opt_comconsole.h:
	echo "#define ALT_BREAK_TO_DEBUGGER 1" > $@

CFLAGS+= -I${.CURDIR}/../.. 

.include <bsd.kmod.mk>
