# $Id: Makefile,v 1.1 2004/09/22 07:29:21 t-momose Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/socket/Makefile,v 1.4 2003/10/12 22:04:20 emax Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/socket

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_btsocket
SRCS=		ng_btsocket.c \
		ng_btsocket_hci_raw.c \
		ng_btsocket_l2cap_raw.c \
		ng_btsocket_l2cap.c \
		ng_btsocket_rfcomm.c
MAN4=		ng_btsocket.4

.include <bsd.kmod.mk>
