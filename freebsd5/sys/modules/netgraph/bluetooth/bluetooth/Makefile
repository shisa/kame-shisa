# $Id: Makefile,v 1.1.1.2 2004/11/18 04:52:31 t-momose Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/bluetooth/Makefile,v 1.5 2004/01/13 11:28:49 ru Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/common

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c

.include <bsd.kmod.mk>

