#	$NetBSD: Makefile,v 1.4 2001/12/12 01:49:37 tv Exp $
#	@(#)Makefile	8.2 (Berkeley) 2/16/94

LIB=	sa
SRCS=	alloc.c bcopy.c bzero.c close.c dev.c disklabel.c getfile.c \
	getputchar.c ioctl.c lseek.c open.c printf.c read.c ufs.c write.c \
	devopen.c getenv.c gets.c strcat.c strcmp.c strcpy.c strlen.c

.PATH:	../../../../lib/libsa ../../../../lib/libkern
NOPROFILE=# defined

.include <bsd.own.mk>
.undef DESTDIR
.include <bsd.lib.mk>

CFLAGS=-O2 -I../../include -I/sys -DSMALL
