#	$NetBSD: Makefile,v 1.14 2002/04/08 16:58:19 bjh21 Exp $

NOMAN=	# defined

# for OBJECT_FMT
.include <bsd.own.mk>

PROG=	bootxx
SRCS=	bootxx.c filesystem.c

STRIPFLAG=
BINDIR=	/usr/mdec
BINMODE=${NONBINMODE}

LIBCRT0=

DEFS=	-D_STANDALONE  -DHEAP_VARIABLE
INCL=	-I. -I${S_LIBTOS} -I${S_MACHSA} -I${S_KERN} -I${S_SA} -I${S}

CPPFLAGS+=	${DEFS} ${INCL}
CFLAGS+=	-Wall -Wno-uninitialized
CFLAGS+=	-fomit-frame-pointer -fno-function-cse -fstrength-reduce
.if ${OBJECT_FMT} == "ELF"
LDFLAGS=	-static -nostdlib -Wl,-x,-N,-e,bootxx,-Ttext,${LOADADDR}
.else
LDFLAGS=	-static -nostdlib -Wl,-N,-Ttext,${LOADADDR}
.endif

# logically src/sys
S=${.CURDIR}/../../../..
S_SA=${S}/lib/libsa
S_KERN=${S}/lib/libkern
S_MACHSA=${S}/arch/atari/stand/libsa
S_LIBTOS=${S}/arch/atari/stand/tostools/libtos

S_MACHSAOBJ!=	cd ${S_MACHSA} && ${PRINTOBJDIR}

LIBSA=${S_MACHSAOBJ}/libsa.a

DPADD=	${LIBSA}
LDADD=	${LIBSA}

beforeinstall:
	@len=`size ${PROG} | awk 'NR==2 { print $$1+$$2 }'`;	\
	 if [ $$len -gt 6656 ]; then				\
		size ${PROG};					\
		false;						\
	 fi;							\
	${OBJCOPY} -O binary ${PROG} ${PROG}XX
	@cat ${PROG}XX /dev/zero | dd of=${PROG}X		\
			bs=1 count=6656  2>/dev/null
	@mv -f ${PROG} ${PROG}XX
	@mv -f ${PROG}X ${PROG};

afterinstall:
	@mv -f ${PROG}XX ${PROG}

.s.o:
	${CPP} ${CPPFLAGS} ${.IMPSRC} | ${AS} ${ASFLAGS} -o ${.TARGET}

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c ${.IMPSRC} -o ${.TARGET}

${PROG}:
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}

${PROG}: ../xxboot/LOADADDR

.PATH: ${S_LIBTOS}

.include "../xxboot/LOADADDR"
.include <bsd.prog.mk>
