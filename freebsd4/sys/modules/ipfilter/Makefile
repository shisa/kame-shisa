# $FreeBSD: src/sys/modules/ipfilter/Makefile,v 1.2.2.2 2001/06/07 05:11:44 jlemon Exp $ 

.PATH: ${.CURDIR}/../../contrib/ipfilter/netinet
KMOD=	ipl
SRCS=	mlfk_ipl.c ip_nat.c ip_frag.c ip_state.c ip_proxy.c ip_auth.c \
	ip_log.c ip_fil.c fil.c
NOMAN=
CFLAGS+= -DIPFILTER_LKM -DIPFILTER_LOG
CFLAGS+=  -I${.CURDIR}/../../contrib/ipfilter
#
# If you don't want log functionality remove -DIPFILTER_LOG
#

.include <bsd.kmod.mk>
