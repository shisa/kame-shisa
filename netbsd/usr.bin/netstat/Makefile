#	$NetBSD: Makefile,v 1.20 2000/02/26 09:55:24 itojun Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/12/93

PROG=	netstat
SRCS=	atalk.c if.c inet.c inet6.c ipsec.c iso.c main.c mbuf.c mroute.c \
	mroute6.c ns.c route.c tp_astring.c unix.c
.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}
CPPFLAGS+= -DINET6 -DIPSEC
CPPFLAGS+= -DDCCP
CPPFLAGS+= -DMIP6

.if exists(/usr/local/v6/lib/libinet6.a)
LDADD+=	-L${.CURDIR}/../../lib/libinet6 \
	-L${.CURDIR}/../../lib/libinet6.obj \
	-L/usr/local/v6/lib -linet6
DPADD+=	${.CURDIR}/../../lib/libinet6/libinet6.a \
	${.CURDIR}/../../lib/libinet6.obj/libinet6.a \
	/usr/local/v6/lib/libinet6.a
.endif

# shared ipsec.c
CPPFLAGS+=	-I${.CURDIR}
.PATH: ${.CURDIR}/../../kame/netstat

.include <bsd.prog.mk>
