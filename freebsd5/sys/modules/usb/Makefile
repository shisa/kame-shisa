# $FreeBSD: src/sys/modules/usb/Makefile,v 1.13 2003/07/15 22:42:37 jmg Exp $ 

MAINTAINER=	n_hibma@freebsd.org

S=	${.CURDIR}/../..

.PATH: $S/dev/usb $S/pci

KMOD=	usb
SRCS=	bus_if.h device_if.h usb_if.h usb_if.c \
	vnode_if.h \
	opt_usb.h \
	hid.c hid.h usbhid.h \
	uhub.c \
	usb.c usb.h \
	usb_mem.c usb_mem.h \
	usb_port.h \
	usb_quirks.c usb_quirks.h \
	usb_subr.c \
	usbdevs.h usbdevs_data.h \
	usbdi.c usbdi.h usbdivar.h \
	usbdi_util.c usbdi_util.h \
	usb_ethersubr.c
	
SRCS+=	uhci_pci.c uhci.c uhcireg.h uhcivar.h
SRCS+=	ohci_pci.c ohci.c ohcireg.h ohcivar.h
SRCS+=	ehci_pci.c ehci.c ehcireg.h ehcivar.h
SRCS+=	opt_bus.h pci_if.h

.include <bsd.kmod.mk>
