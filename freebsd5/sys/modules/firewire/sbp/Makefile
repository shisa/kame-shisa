# $FreeBSD: src/sys/modules/firewire/sbp/Makefile,v 1.3 2003/10/02 04:06:56 simokawa Exp $

# Makefile for the SBP-II (Serial Bus Protocol 2/SCSI over IEEE1394)

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= sbp
SRCS	= bus_if.h device_if.h \
	  opt_cam.h opt_scsi.h \
	  sbp.c sbp.h \
	  firewire.h firewirereg.h \
	  iec13213.h

.include <bsd.kmod.mk>

