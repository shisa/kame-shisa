#	$NetBSD: Makefile,v 1.31 2002/03/22 18:10:22 thorpej Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93

.include <bsd.own.mk>

MAN=	telnetd.8

CPPFLAGS+=-DINET6
LDADD+=	-L${.CURDIR}/../../lib/libinet6 -L${.CURDIR}/../../lib/libinet6/obj \
	-L/usr/local/v6/lib -linet6
DPADD+=	${.CURDIR}/../../lib/libinet6/libinet6.a \
	${.CURDIR}/../../lib/libinet6/obj/libinet6.a \
	/usr/local/v6/lib/libinet6.a

PROG=	telnetd
CPPFLAGS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOPENPTY_PTY
CPPFLAGS+=-DOLD_ENVIRON -DENV_HACK -DSECURELOGIN
SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
DPADD=	${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET}
LDADD+=	-lutil -ltermcap -ltelnet

CPPFLAGS+=-I${.CURDIR}/../../lib
CPPFLAGS+=-I${.CURDIR}

.if (${USE_KERBEROS} != "no")
CPPFLAGS+=-DAUTHENTICATION -DENCRYPTION
CPPFLAGS+=-I${DESTDIR}/usr/include/krb5
CPPFLAGS+=-DKRB5

VERS!=	cd ${.CURDIR}/../../lib/libvers && ${PRINTOBJDIR}

LDADD+=	-lkrb5 -lkrb -lcrypto -lasn1 -lcom_err -L${VERS} -lvers -lroken
DPADD+=	${LIBKRB5} ${LIBKRB} ${LIBCRYPTO} ${LIBASN1} ${LIBCOM_ERR} ${LIBROKEN}
.endif

.include <bsd.prog.mk>
