SRCDIR=	${.CURDIR}/../../kame/rtadvd

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c

CPPFLAGS+=-DINET6 -DROUTEINFO -DHAVE_POLL_H
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
LDADD+=	-L${.CURDIR}/../../lib/libinet6 -L${.CURDIR}/../../lib/libinet6/obj \
	-L/usr/local/v6/lib -linet6 -lcompat
DPADD+=	${.CURDIR}/../../lib/libinet6/libinet6.a \
	${.CURDIR}/../../lib/libinet6/obj/libinet6.a \
	/usr/local/v6/lib/libinet6.a \
	${LIBCOMPAT}
MAN=	rtadvd.8 rtadvd.conf.5

.PATH:	${SRCDIR}

CFLAGS+=	-Wall -Werror -Wmissing-prototypes

.include <bsd.prog.mk>
