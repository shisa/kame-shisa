# $Whistle: Makefile,v 1.1 1999/12/08 20:20:39 archie Exp $
# $FreeBSD: src/sys/modules/netgraph/mppc/Makefile,v 1.12 2003/02/05 19:11:11 ambrisko Exp $

KMOD=	ng_mppc
SRCS= 	ng_mppc.c opt_netgraph.h

NETGRAPH_MPPC_COMPRESSION?=	0
NETGRAPH_MPPC_ENCRYPTION?=	1

.if ${NETGRAPH_MPPC_COMPRESSION} > 0
# XXX These files don't exist yet, but hopefully someday they will...
.PATH: ${.CURDIR}/../../../net
SRCS+=	mppcc.c mppcd.c
.endif

.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
.PATH: ${.CURDIR}/../../../crypto
SRCS+= sha1.c
.endif

opt_netgraph.h:
	touch ${.TARGET}
.if ${NETGRAPH_MPPC_COMPRESSION} > 0
	echo "#define NETGRAPH_MPPC_COMPRESSION 1" >> ${.TARGET}
.endif
.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
	echo "#define NETGRAPH_MPPC_ENCRYPTION 1" >> ${.TARGET}
.endif

.include <bsd.kmod.mk>
