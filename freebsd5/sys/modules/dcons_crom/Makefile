# $Id: Makefile,v 1.1 2004/09/22 07:29:09 t-momose Exp $
# $FreeBSD: src/sys/modules/dcons_crom/Makefile,v 1.1 2003/10/24 15:44:10 simokawa Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons_crom
SRCS	= dcons_crom.c dcons.h \
		bus_if.h device_if.h

#KMODDEPS = firewire dcons

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
