#	$NetBSD: Makefile,v 1.39 2002/02/17 20:03:08 thorpej Exp $

S=	${.CURDIR}/../../../../

BASE?= biosboot
PROG= ${BASE}.sym
NOMAN=	# defined
NEWVERSWHAT=	"BIOS Boot"
STARTFILE=	${BSSTART}
RELOC=		0x0

.if (${BASE} != "biosboot")
.PATH.c: ${.CURDIR}/../biosboot/
.endif

SRCS= main.c devopen.c conf.c exec.c

CPPFLAGS+= -DCOMPAT_386BSD_MBRPART -DSUPPORT_PS2

.if (${BASE} == "biosboot")
# Various serial line configurations
CPPFLAGS+= -DSUPPORT_SERIAL=CONSDEV_PC -DDIRECT_SERIAL
#	or
#CPPFLAGS+= -DSUPPORT_SERIAL=CONSDEV_COM0KBD
#	or
#CPPFLAGS+= -DSUPPORT_SERIAL=CONSDEV_AUTO
#	and maybe
#CPPFLAGS+= -DDIRECT_SERIAL -DCOMCONS_KEYPRESS -DCONSPEED=57600
.endif

.if (${BASE} == "biosboot_com0")
CPPFLAGS+= -DSUPPORT_SERIAL=CONSDEV_COM0 -DDIRECT_SERIAL
.endif

.if (${BASE} == "biosboot_resetvideo")
CPPFLAGS+= -DRESET_VIDEO
CPPFLAGS+= -DSUPPORT_SERIAL=CONSDEV_PC -DDIRECT_SERIAL
.endif

CPPFLAGS+= -DSUPPORT_USTARFS
#CPPFLAGS+= -DSUPPORT_DOSFS
CPPFLAGS+= -DPASS_BIOSGEOM
CPPFLAGS+= -DPASS_MEMMAP
#CPPFLAGS+= -DBOOTPASSWD

#uncomment if there are problems with memory detection
#CPPFLAGS+= -DCONSERVATIVE_MEMDETECT
#increase MAXFLENTRIES if "installboot" complains about
# "not enough fragment space in bootcode" (default: 10)
#CPPFLAGS+= -DMAXFLENTRIES=15
#set PRIM_LOADSZ to <=9 to make a 720k boot floppy (default: 15)
#CPPFLAGS+= -DPRIM_LOADSZ=9

#CFLAGS= -O2 -fomit-frame-pointer -fno-defer-pop
CFLAGS+= -Wall -Wmissing-prototypes -Wstrict-prototypes -Wno-main

SAMISCCPPFLAGS+= -DHEAP_START=0x20000 -DHEAP_LIMIT=0x50000
SAMISCMAKEFLAGS+= SA_USE_CREAD=yes	# Read compressed kernels
SAMISCMAKEFLAGS+= SA_INCLUDE_NET=no	# Netboot via TFTP, NFS

.if (${BASE} == "biosboot")
VERSIONFILE= ${.CURDIR}/version
.else
VERSIONFILE= ${.CURDIR}/../biosboot/version
.endif

.include "../Makefile.booters"
