#
# $Id: Makefile,v 1.1 2004/09/22 07:24:59 t-momose Exp $ 
#

.if exists(${.CURDIR}/../../Makefile.opsys)
.include "${.CURDIR}/../../Makefile.opsys"
.endif

PROG=	rip6admin
.if (${OPSYS} != "NetBSD")
MAN8=	rip6admin.8
.else
MAN=	rip6admin.8
.endif
SRCS=	rip6admin.c
CFLAGS+=	-I${.CURDIR}/../include -DINET6

.include <bsd.prog.mk>
