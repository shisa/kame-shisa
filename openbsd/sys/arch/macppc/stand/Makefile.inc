#	$OpenBSD: Makefile.inc,v 1.4 2004/01/09 16:50:30 drahn Exp $
#	$NetBSD: Makefile.inc,v 1.1 1996/09/30 16:34:59 ws Exp $

.if !defined(__stand_makefile_inc)
__stand_makefile_inc=1

KERN_AS=	library

S=$(.CURDIR)/../../../$(R)

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj) && !defined(NOMACHINE)
.BEGIN:
	@([ -h machine ] || ln -s $(S)/arch/$(MACHINE)/include machine)
.endif

#
EXTRACFLAGS=	-msoft-float
REAL_VIRT?=	-v
ENTRY?=	_start

INCLUDES+=	-I. -I$(.OBJDIR) -I$(.CURDIR)/.. -I$(S)/arch -I$(S)
INCLUDES+=	-I$(S)/lib/libsa
DEFS+=		-DSTANDALONE -DFIREPOWERBUGS
CFLAGS+=	$(INCLUDES) $(DEFS) $(EXTRACFLAGS)
CFLAGS+=	-fno-stack-protector
CFLAGS+=	-fno-builtin-vprintf -fno-builtin-printf -fno-builtin-putchar
LDFLAGS?=	-X -N -Ttext $(RELOC) -e $(ENTRY)

cleandir:
	rm -rf lib machine

.endif
