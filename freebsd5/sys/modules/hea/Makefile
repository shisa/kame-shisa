# $FreeBSD: src/sys/modules/hea/Makefile,v 1.1 2002/06/03 09:13:53 mdodd Exp $

.PATH:	${.CURDIR}/../../dev/hea
  
KMOD=	hea_pci
SRCS=	hea_pci.c pci_if.h device_if.h bus_if.h
SRCS+=	hea_freebsd.c \
	eni.c eni_globals.c eni_init.c eni_receive.c eni_vcm.c \
	eni_buffer.c eni_if.c eni_intr.c eni_transmit.c

.include <bsd.kmod.mk>
