#	$NetBSD: Makefile,v 1.12 2001/10/05 12:37:39 bjh21 Exp $

KDIR=	/sys/net
INCSDIR= /usr/include/net

INCS=	bpf.h bpfdesc.h dlt.h ethertypes.h if.h if_arc.h if_arp.h \
	if_atm.h if_bridgevar.h if_dl.h if_ether.h if_fddi.h if_gif.h \
	if_gre.h if_hippi.h if_ieee1394.h if_ieee80211.h if_llc.h if_media.h \
	if_ppp.h if_pppvar.h if_pppoe.h if_slvar.h if_sppp.h if_stf.h \
	if_stripvar.h if_token.h if_tun.h if_types.h if_vlanvar.h \
	netisr.h pfil.h pfkeyv2.h ppp-comp.h ppp_defs.h radix.h \
	raw_cb.h route.h slcompress.h slip.h zlib.h
INCS+=	radix_art.h radix_mpath.h
INCS+=	pfvar.h
INCS+=	if_ist.h
INCS+=	mipsock.h if_mip.h

.include <bsd.kinc.mk>
