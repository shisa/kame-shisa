# $FreeBSD: src/sys/modules/coda/Makefile,v 1.7.2.1 2001/07/24 09:49:42 dd Exp $

.PATH:	${.CURDIR}/../../coda
KMOD=	coda
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c vcoda.h
NOMAN=

CLEANFILES=	vcoda.h

vcoda.h:
	echo "#define NVCODA 4" > vcoda.h

.include <bsd.kmod.mk>
