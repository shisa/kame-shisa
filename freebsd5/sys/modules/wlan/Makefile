# $FreeBSD: src/sys/modules/wlan/Makefile,v 1.2 2003/06/28 06:19:01 sam Exp $

.PATH: ${.CURDIR}/../../net80211

KMOD=	wlan
SRCS=	ieee80211.c ieee80211_crypto.c ieee80211_input.c ieee80211_ioctl.c \
	ieee80211_node.c ieee80211_output.c ieee80211_proto.c
SRCS+=	bus_if.h device_if.h opt_inet.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
