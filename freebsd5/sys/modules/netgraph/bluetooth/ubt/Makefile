# $Id: Makefile,v 1.1.1.1 2004/09/22 07:29:21 t-momose Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/ubt/Makefile,v 1.4 2003/10/12 22:04:20 emax Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

KMOD=		ng_ubt
SRCS=		ng_ubt.c bus_if.h device_if.h vnode_if.h opt_usb.h
MAN4=		ng_ubt.4

.include <bsd.kmod.mk>

