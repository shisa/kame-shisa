# $Id: Makefile,v 1.1.1.1 2004/09/22 07:29:21 t-momose Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/l2cap/Makefile,v 1.4 2003/10/12 22:04:20 emax Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/l2cap

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/l2cap

KMOD=		ng_l2cap
SRCS=		ng_l2cap_main.c ng_l2cap_cmds.c ng_l2cap_evnt.c \
		ng_l2cap_ulpi.c ng_l2cap_llpi.c ng_l2cap_misc.c
MAN4=		ng_l2cap.4

.include <bsd.kmod.mk>
