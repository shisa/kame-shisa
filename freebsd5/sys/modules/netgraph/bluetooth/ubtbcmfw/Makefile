# $Id: Makefile,v 1.1 2004/09/22 07:29:21 t-momose Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/ubtbcmfw/Makefile,v 1.2 2003/10/12 22:04:20 emax Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubtbcmfw

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubtbcmfw

KMOD=		ubtbcmfw
SRCS=		ubtbcmfw.c bus_if.h device_if.h vnode_if.h opt_usb.h
MAN4=		ubtbcmfw.4

.include <bsd.kmod.mk>

