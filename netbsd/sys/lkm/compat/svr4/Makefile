#	$NetBSD: Makefile,v 1.9 2002/02/10 15:42:57 mrg Exp $

COMPAT_UTIL_OVERRIDE=	svr4
.include "../Makefile.inc"
USE_32=
.include "../svr4/Makefile.common"

# common source files
SRCS+=	lkminit_emul.c
SRCS+=	svr4_errno.c svr4_exec.c svr4_fcntl.c svr4_filio.c svr4_ioctl.c \
	svr4_ipc.c svr4_lwp.c svr4_misc.c svr4_net.c svr4_resource.c	\
	svr4_signal.c svr4_socket.c svr4_sockio.c svr4_stat.c svr4_stream.c \
	svr4_syscalls.c svr4_sysent.c svr4_termios.c svr4_ttold.c
SRCS+=	svr4_machdep.c

# XXXX - temporary, not applicable for kernel with COMPAT_43
#.PATH: $S/compat/common
#SRCS+=	uipc_syscalls_43.c vfs_syscalls_43.c

.if ${MACHINE} == "i386"
SRCS+=	svr4_sigcode.s svr4_syscall.c
.endif

.if ${MACHINE} == "sparc"
SRCS+=	svr4_sigcode.s
.endif

.if ${MACHINE} == "sparc64"
.if ${MACHINE_ARCH} == "sparc64"
SRCS+=	svr4_sigcode64.s
.else
SRCS+=	svr4_32_sigcode.s
.endif
.endif

.include <bsd.kmod.mk>
