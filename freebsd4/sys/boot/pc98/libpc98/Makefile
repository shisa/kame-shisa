# $FreeBSD: src/sys/boot/pc98/libpc98/Makefile,v 1.3.2.5 2002/07/19 18:46:29 ru Exp $
#
LIB=			pc98
INTERNALLIB=		true

SRCS=	aout_freebsd.c bioscd.c biosdisk.c biosmem.c biospnp.c biospci.c \
	bootinfo.c comconsole.c devicename.c elf_freebsd.c gatea20.c \
	i386_copy.c i386_module.c nullconsole.c pxe.c pxetramp.s \
	time.c vidconsole.c

.PATH:	${.CURDIR}/../../i386/libi386

CFLAGS+=	-DPC98

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../btx/lib \
		-I${.CURDIR}/../../.. -I. -I${.CURDIR}/../../i386/libi386

BOOT_COMCONSOLE_PORT?= 0x238
CFLAGS+=	-DCOMPORT=${BOOT_COMCONSOLE_PORT}

BOOT_COMCONSOLE_SPEED?= 9600
CFLAGS+=	-DCOMSPEED=${BOOT_COMCONSOLE_SPEED}

# the location of libstand
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

.ifdef(BOOT_BIOSDISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

# Include simple terminal emulation (cons25-compatible)
CFLAGS+= -DTERM_EMU

# Make "machine" required for all objects
# (based on the more complete case in sys/i386/boot/Makefile.inc)
${SRCS:M*.c:R:S/$/.o/g}: machine

# If it's not there, don't consider it a target
.if exists(${.CURDIR}/../../../i386/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

.endif

CLEANFILES+=	machine

.include <bsd.lib.mk>
