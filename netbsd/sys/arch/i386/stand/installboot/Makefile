#	$NetBSD: Makefile,v 1.23 2002/02/17 20:03:10 thorpej Exp $

NOMAN=	# defined

# XXX: bsd.own.mk included so LDSTATIC can be overwridden
.include <bsd.own.mk>

PROG=	installboot
LIBSA=${.CURDIR}/../../../../lib/libsa

.PATH.c: ${LIBSA}

SRCS=	installboot.c bootblks.c getmount.c byteorder.c loadfile.c \
	loadfile_aout.c loadfile_elf32.c

CPPFLAGS+= -I${.CURDIR}/../lib/crt/bootsect
CPPFLAGS+= -I${LIBSA} -I.
CFLAGS+= -Wall -Wmissing-prototypes -Wstrict-prototypes
LDSTATIC?=-static

.BEGIN: machine
.NOPATH: machine
${SRCS} realdepend realall: machine
CLEANFILES+= machine

machine::
	-rm -f $@
	ln -s ${.CURDIR}/../../include $@

.include <bsd.prog.mk>
