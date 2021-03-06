/*-
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)procfs_fpregs.c	8.2 (Berkeley) 6/15/94
 *
 * From:
 *	$Id: procfs_fpregs.c,v 1.1 2004/09/22 07:28:25 t-momose Exp $
 * $FreeBSD: src/sys/fs/procfs/procfs_fpregs.c,v 1.29 2003/10/02 15:00:55 nectar Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/uio.h>

#include <machine/reg.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>

int
procfs_doprocfpregs(PFS_FILL_ARGS)
{
	int error;
	struct fpreg r;

	PROC_LOCK(p);
	if (p_candebug(td, p)) {
		PROC_UNLOCK(p);
		return (EPERM);
	}

	_PHOLD(p);
	/* XXXKSE: */
	error = proc_read_fpregs(FIRST_THREAD_IN_PROC(p), &r);
	if (error == 0) {
		PROC_UNLOCK(p);
		error = uiomove_frombuf(&r, sizeof(r), uio);
		PROC_LOCK(p);
	}
	if (error == 0 && uio->uio_rw == UIO_WRITE) {
		if (!P_SHOULDSTOP(p))
			error = EBUSY;
		else
			/* XXXKSE: */
			error = proc_write_fpregs(FIRST_THREAD_IN_PROC(p), &r);
	}
	_PRELE(p);
	PROC_UNLOCK(p);

	uio->uio_offset = 0;
	return (error);
}
