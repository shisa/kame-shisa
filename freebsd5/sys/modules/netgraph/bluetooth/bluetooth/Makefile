# $Id: Makefile,v 1.1.1.1 2004/09/22 07:29:20 t-momose Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/bluetooth/Makefile,v 1.4 2003/10/12 22:04:19 emax Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/common

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c
MAN4=		ng_bluetooth.4

.include <bsd.kmod.mk>

