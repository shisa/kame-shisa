#	$NetBSD: Makefile,v 1.7 2002/05/15 13:55:38 lukem Exp $

PROG=		bootxx
NOMAN=		# defined
BINMODE=	444

CFLAGS=		-Os -mmultiple -Wall -msoft-float
CPPFLAGS+=	-D_STANDALONE
STRIPFLAG=
LINKFLAGS=	-x -N -Ttext 4000 -e _start
CLEANFILES+=	${PROG}.sym

${PROG}: ${OBJS}
	${LD} -o ${.TARGET}.sym ${LINKFLAGS} ${OBJS}
	@${SIZE} ${.TARGET}.sym
	${OBJCOPY} -O binary ${.TARGET}.sym ${.TARGET}

.include <bsd.prog.mk>
