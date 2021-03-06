#	$NetBSD: Makefile.cats.inc,v 1.8.4.1 2003/08/12 18:28:34 tron Exp $

GENASSYM_EXTRAS+=	${ARM}/footbridge/genassym.cf
LOOSE_PROTOTYPES=	NO

.if (${OBJECT_FMT} == "ELF")
# Need to convert the kernel from ELF to a.out so that OpenFirmware
# can load it.

# ZMAGIC a.out includes the a.out header in front of the text
# segment, so if we link the kernel at the normal load address,
# it will be wrong once the a.out header is stuck in the file.
# XXX HOWEVER, by linking it +32 bytes to compensate, the resulting
# XXX ELF kernel cannot be booted itself.
LOADADDRESS=0xF0000020

SYSTEM_LD_TAIL_EXTRA+=; \
	${DBSYM} $@ || true; \
	echo OBJCOPY=${OBJCOPY:Q} SIZE=${SIZE:Q} \
	    sh ${ARM}/conf/elf2aout.sh $@ $@.aout; \
	OBJCOPY=${OBJCOPY:Q} SIZE=${SIZE:Q} \
	    sh ${ARM}/conf/elf2aout.sh $@ $@.aout
.endif
