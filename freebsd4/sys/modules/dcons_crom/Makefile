# $Id: Makefile,v 1.1 2004/09/22 07:23:40 t-momose Exp $
# $FreeBSD: src/sys/modules/dcons_crom/Makefile,v 1.1.2.2 2003/11/26 08:42:39 simokawa Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons_crom
SRCS	= dcons_crom.c dcons.h \
		bus_if.h device_if.h

KMODDEPS = firewire dcons

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
