#	$NetBSD: Makefile,v 1.3 2000/04/19 06:39:16 itojun Exp $

KDIR=	/sys/netinet
INCSDIR= /usr/include/netinet

INCS=	icmp6.h icmp_var.h if_atm.h if_ether.h if_inarp.h \
	igmp.h igmp_var.h in.h in_gif.h in_pcb.h in_systm.h \
	in_var.h ip.h ip6.h ip_auth.h ip_compat.h ip_ecn.h \
	ip_encap.h ip_fil.h ip_frag.h ip_icmp.h ip_mroute.h \
	ip_nat.h ip_proxy.h ip_state.h ip_var.h tcp.h \
	tcp_debug.h tcp_fsm.h tcp_seq.h tcp_timer.h tcp_var.h \
	tcpip.h udp.h udp_var.h
INCS+=	ip6mh.h
INCS+=	in_msf.h
INCS+=	sctp.h sctp_uio.h sctp_var.h
INCS+=	dccp.h dccp_cc_sw.h dccp_tcplike.h \
	dccp_tfrc.h dccp_tfrc_lookup.h dccp_tfrc_print.h  \
	dccp_var.h

.include <bsd.kinc.mk>
