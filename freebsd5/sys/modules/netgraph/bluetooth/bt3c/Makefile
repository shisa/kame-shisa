# $Id: Makefile,v 1.1 2004/09/22 07:29:20 t-momose Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/bt3c/Makefile,v 1.4 2003/10/12 22:04:19 emax Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/bt3c

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/bt3c

KMOD=		ng_bt3c
SRCS=		ng_bt3c_pccard.c bus_if.h card_if.h device_if.h
MAN4=		ng_bt3c.4

.include <bsd.kmod.mk>

