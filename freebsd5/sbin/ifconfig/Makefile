#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/ifconfig/Makefile,v 1.25 2004/02/23 20:13:52 johan Exp $

PROG=	ifconfig
SRCS=	ifconfig.c

#comment out to exclude SIOC[GS]IFMEDIA support
SRCS+=	ifmedia.c
CFLAGS+=-DUSE_IF_MEDIA
CFLAGS+=-DINET6
CFLAGS+=-DMIP6

#comment out to exclude SIOC[GS]ETVLAN support
SRCS+=	ifvlan.c
CFLAGS+=-DUSE_VLANS

#comment out to exclude SIOC[GS]IEEE80211 support
SRCS+=	ifieee80211.c
CFLAGS+=-DUSE_IEEE80211

#comment out to exclude MAC support
SRCS+=	ifmac.c
CFLAGS+=-DUSE_MAC

MAN=	ifconfig.8

.if defined(RELEASE_CRUNCH)
CFLAGS+=-DNO_IPX
.else
DPADD=	${LIBIPX}
LDADD=	-lipx
.endif

CFLAGS+=-DNS -Wall -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
	-Wnested-externs -I..

#link KAME libinet6
LDADD+=	-L${.OBJDIR}/../../lib/libinet6 \
	-L/usr/local/v6/lib -linet6
DPADD+=	${.OBJDIR}/../../lib/libinet6/libinet6.a \
	/usr/local/v6/lib/libinet6.a
WARNS=	0

.include <bsd.prog.mk>
