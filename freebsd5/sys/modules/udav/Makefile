# $FreeBSD: src/sys/modules/udav/Makefile,v 1.2 2004/06/27 12:33:17 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb
KMOD=	if_udav
SRCS=	if_udav.c opt_bdg.h opt_usb.h device_if.h bus_if.h opt_inet.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
