# $FreeBSD: src/sys/modules/wi/Makefile,v 1.16 2003/01/15 20:13:30 sam Exp $

.PATH: ${.CURDIR}/../../dev/wi

KMOD=	if_wi
SRCS=	opt_wi.h if_wi.c if_wi_pccard.c if_wi_pci.c \
	card_if.h device_if.h bus_if.h pci_if.h

opt_wi.h:
	echo "#define WI_SYMBOL_FIRMWARE 1" > ${.TARGET}

.include <bsd.kmod.mk>
