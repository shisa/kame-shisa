SRCDIR=	${.CURDIR}/../../kame/setkey
LIBPFKEYDIR!=	cd ${.CURDIR}/../../lib/libpfkey; make print-objdir

PROG=	setkey
SRCS=	setkey.c parse.y token.l
CFLAGS+=-g
CPPFLAGS+=	-I${.CURDIR}/../../kame/libipsec
LDADD+=	-ll -ly
DPADD+=	${LIBL} ${LIBY}
CLEANFILES+=	y.tab.c y.tab.h key_test.o keytest
YHEADER=	yes

#SCRIPTS=	scriptdump

all: ${PROG} scriptdump

CPPFLAGS+=-DIPSEC_DEBUG -DINET6 -DYY_NO_UNPUT
CPPFLAGS+=-I${.CURDIR} -I${SRCDIR}
LDADD+=	-L${LIBPFKEYDIR} -lpfkey
DPADD+=	${LIBPFKEYDIR}/libpfkey.a
LDADD+=	-L${.CURDIR}/../../lib/libipsec -L${.CURDIR}/../../lib/libipsec/obj \
	-L${.CURDIR}/../../lib/libinet6 -L${.CURDIR}/../../lib/libinet6/obj \
	-L/usr/local/v6/lib -lipsec -linet6
DPADD+=	${.CURDIR}/../../lib/libipsec/libipsec.a \
	${.CURDIR}/../../lib/libipsec/obj/libipsec.a \
	/usr/local/v6/lib/libipsec.a
DPADD+=	${.CURDIR}/../../lib/libinet6/libinet6.a \
	${.CURDIR}/../../lib/libinet6/obj/libinet6.a \
	/usr/local/v6/lib/libinet6.a
CLEANFILES+=	scriptdump

MAN=	setkey.8
LOCALPREFIX=	/usr/pkg

scriptdump: scriptdump.pl
	sed -e 's#@LOCALPREFIX@#${LOCALPREFIX}#' < $> > scriptdump

.PATH:	${SRCDIR}

.include <bsd.prog.mk>
