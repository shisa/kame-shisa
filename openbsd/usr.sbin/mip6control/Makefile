#
# $Id: Makefile,v 1.1 2004/09/22 07:41:19 t-momose Exp $
#
SRCDIR=	${.CURDIR}/../../kame/mip6control

PROG=	mip6control
SRCS=	mip6control.c

BINGRP= kmem
BINMODE=2555
DPADD=  ${LIBKVM}
LDADD=  -lkvm
CPPFLAGS+=-DINET6

DPADD+=	${.OBJDIR}/../../lib/libinet6/libinet6.a /usr/local/v6/lib/libinet6.a
LDADD+=	-L${.OBJDIR}/../../lib/libinet6 -L/usr/local/v6/lib -linet6

MAN=	mip6control.8

.PATH:	${SRCDIR}

.include <bsd.prog.mk>
