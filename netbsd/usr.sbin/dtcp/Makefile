SRCDIR=	${.CURDIR}/../../kame/dtcp

MAN=	dtcps.8 dtcpc.8

# path to Ruby interpreter
LPREFIX=	/usr/pkg/bin
#LPREFIX=	/usr/local/bin
CLEANFILES+=	dtcps dtcpc

all: dtcps dtcpc

dtcps: dtcps.rb
	sed -e 's#@PREFIX@#${LPREFIX}#' < $> > dtcps
dtcpc: dtcpc.rb
	sed -e 's#@PREFIX@#${LPREFIX}#' < $> > dtcpc

realinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 555 dtcps \
		${DESTDIR}${BINDIR}/dtcps
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 555 dtcpc \
		${DESTDIR}${BINDIR}/dtcpc

.PATH:	${SRCDIR}

.include <bsd.prog.mk>
