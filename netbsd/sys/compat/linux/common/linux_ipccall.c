/*	$NetBSD: linux_ipccall.c,v 1.22 2001/11/15 09:48:01 lukem Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden and Eric Haszlakiewicz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: linux_ipccall.c,v 1.22 2001/11/15 09:48:01 lukem Exp $");

#if defined(_KERNEL_OPT)
#include "opt_sysv.h"
#endif

#include <sys/param.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/proc.h>
#include <sys/systm.h>

/* real syscalls */
#include <sys/mount.h>
#include <sys/syscallargs.h>

/* sys_ipc + args prototype */
#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_signal.h>

#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_syscall.h>

/* general ipc defines */
#include <compat/linux/common/linux_ipc.h>

/* prototypes for real/normal linux-emul syscalls */
#include <compat/linux/common/linux_msg.h>
#include <compat/linux/common/linux_shm.h>
#include <compat/linux/common/linux_sem.h>

/* prototypes for sys_ipc stuff */
#include <compat/linux/common/linux_ipccall.h>

/* Used on: arm, i386, m68k, mips, ppc, sparc, sparc64 */
/* Not used on: alpha */

/*
 * Stuff to deal with the SysV ipc/shm/semaphore interface in Linux.
 * The main difference is, that Linux handles it all via one
 * system call, which has the usual maximum amount of 5 arguments.
 * This results in a kludge for calls that take 6 of them.
 *
 * The SYSV??? options have to be enabled to get the appropriate
 * functions to work.
 */

int
linux_sys_ipc(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;

	switch (SCARG(uap, what)) {
#ifdef SYSVSEM
	case LINUX_SYS_semop:
		return linux_semop(p, uap, retval);
	case LINUX_SYS_semget:
		return linux_semget(p, uap, retval);
	case LINUX_SYS_semctl: {
		struct linux_sys_semctl_args bsa;
		union linux_semun arg;
		int error;

		SCARG(&bsa, semid) = SCARG(uap, a1);
		SCARG(&bsa, semnum) = SCARG(uap, a2);
		SCARG(&bsa, cmd) = SCARG(uap, a3);
		/* Convert from (union linux_semun *) to (union linux_semun) */
		if ((error = copyin(SCARG(uap, ptr), &arg, sizeof arg)))
			return error;
		SCARG(&bsa, arg) = arg;

		return linux_sys_semctl(p, &bsa, retval);
	    }
#endif
#ifdef SYSVMSG
	case LINUX_SYS_msgsnd:
		return linux_msgsnd(p, uap, retval);
	case LINUX_SYS_msgrcv:
		return linux_msgrcv(p, uap, retval);
	case LINUX_SYS_msgget:
		return linux_msgget(p, uap, retval);
	case LINUX_SYS_msgctl: {
		struct linux_sys_msgctl_args bsa;

		SCARG(&bsa, msqid) = SCARG(uap, a1);
		SCARG(&bsa, cmd) = SCARG(uap, a2);
		SCARG(&bsa, buf) = (struct linux_msqid_ds *)SCARG(uap, ptr);

		return linux_sys_msgctl(p, &bsa, retval);
	    }
#endif
#ifdef SYSVSHM
	case LINUX_SYS_shmat: {
		struct linux_sys_shmat_args bsa;
	
		SCARG(&bsa, shmid) = SCARG(uap, a1);
		SCARG(&bsa, shmaddr) = (void *)SCARG(uap, ptr);
		SCARG(&bsa, shmflg) = SCARG(uap, a2);
		/* XXX passing pointer inside int here */
		SCARG(&bsa, raddr) = (u_long *)SCARG(uap, a3);

		return linux_sys_shmat(p, &bsa, retval);
	    }
	case LINUX_SYS_shmdt:
		return linux_shmdt(p, uap, retval);
	case LINUX_SYS_shmget:
		return linux_shmget(p, uap, retval);
	case LINUX_SYS_shmctl: {
		struct linux_sys_shmctl_args bsa;

		SCARG(&bsa, shmid) = SCARG(uap, a1);
		SCARG(&bsa, cmd) = SCARG(uap, a2);
		SCARG(&bsa, buf) = (struct linux_shmid_ds *)SCARG(uap, ptr);

		return linux_sys_shmctl(p, &bsa, retval);
	    }
#endif
	default:
		return ENOSYS;
	}
}

#ifdef SYSVSEM
inline int
linux_semop(p, uap, retval)
	struct proc *p;
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap;
	register_t *retval;
{
	struct sys_semop_args bsa;

	SCARG(&bsa, semid) = SCARG(uap, a1);
	SCARG(&bsa, sops) = (struct sembuf *)SCARG(uap, ptr);
	SCARG(&bsa, nsops) = SCARG(uap, a2);

	return sys_semop(p, &bsa, retval);
}

inline int
linux_semget(p, uap, retval)
	struct proc *p;
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap;
	register_t *retval;
{
	struct sys_semget_args bsa;

	SCARG(&bsa, key) = (key_t)SCARG(uap, a1);
	SCARG(&bsa, nsems) = SCARG(uap, a2);
	SCARG(&bsa, semflg) = SCARG(uap, a3);

	return sys_semget(p, &bsa, retval);
}

#endif /* SYSVSEM */

#ifdef SYSVMSG

inline int
linux_msgsnd(p, uap, retval)
	struct proc *p;
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap;
	register_t *retval;
{
	struct sys_msgsnd_args bma;

	SCARG(&bma, msqid) = SCARG(uap, a1);
	SCARG(&bma, msgp) = SCARG(uap, ptr);
	SCARG(&bma, msgsz) = SCARG(uap, a2);
	SCARG(&bma, msgflg) = SCARG(uap, a3);

	return sys_msgsnd(p, &bma, retval);
}

inline int
linux_msgrcv(p, uap, retval)
	struct proc *p;
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap;
	register_t *retval;
{
	struct sys_msgrcv_args bma;
	struct linux_msgrcv_msgarg kluge;
	int error;

	if ((error = copyin(SCARG(uap, ptr), &kluge, sizeof kluge)))
		return error;

	SCARG(&bma, msqid) = SCARG(uap, a1);
	SCARG(&bma, msgp) = kluge.msg;
	SCARG(&bma, msgsz) = SCARG(uap, a2);
	SCARG(&bma, msgtyp) = kluge.type;
	SCARG(&bma, msgflg) = SCARG(uap, a3);

	return sys_msgrcv(p, &bma, retval);
}

inline int
linux_msgget(p, uap, retval)
	struct proc *p;
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap;
	register_t *retval;
{
	struct sys_msgget_args bma;

	SCARG(&bma, key) = (key_t)SCARG(uap, a1);
	SCARG(&bma, msgflg) = SCARG(uap, a2);

	return sys_msgget(p, &bma, retval);
}

#endif /* SYSVMSG */

#ifdef SYSVSHM
/*
 * shmdt(): this could have been mapped directly, if it wasn't for
 * the extra indirection by the linux_ipc system call.
 */
inline int
linux_shmdt(p, uap, retval)
	struct proc *p;
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap;
	register_t *retval;
{
	struct sys_shmdt_args bsa;

	SCARG(&bsa, shmaddr) = SCARG(uap, ptr);

	return sys_shmdt(p, &bsa, retval);
}

/*
 * Same story as shmdt.
 */
inline int
linux_shmget(p, uap, retval)
	struct proc *p;
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap;
	register_t *retval;
{
	struct sys_shmget_args bsa;

	SCARG(&bsa, key) = SCARG(uap, a1);
	SCARG(&bsa, size) = SCARG(uap, a2);
	SCARG(&bsa, shmflg) = SCARG(uap, a3);

	return sys_shmget(p, &bsa, retval);
}

#endif /* SYSVSHM */
