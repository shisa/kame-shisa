#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/route/Makefile,v 1.19 2003/08/04 21:31:51 ache Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c keywords.h
CFLAGS+=-I. -Wall -DNS
CFLAGS+=-DINET6
WARNS=	0
CLEANFILES+=keywords.h _keywords.tmp

keywords.h: keywords
	sed -e '/^#/d' -e '/^$$/d' ${.CURDIR}/keywords > _keywords.tmp
	LC_ALL=C tr 'a-z' 'A-Z' < _keywords.tmp | paste _keywords.tmp - | \
	    awk '{ \
		if (NF > 1) \
			printf "#define\tK_%s\t%d\n\t{\"%s\", K_%s},\n", \
			    $$2, NR, $$1, $$2 }' \
	    > ${.TARGET}
	rm -f _keywords.tmp

./keywords.h:	keywords.h

LDADD+=	-L${.OBJDIR}/../../lib/libinet6 \
	-L/usr/local/v6/lib -linet6
DPADD+=	${.OBJDIR}/../../lib/libinet6/libinet6.a \
	/usr/local/v6/lib/libinet6.a

.include <bsd.prog.mk>
