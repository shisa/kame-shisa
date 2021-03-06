/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      from BSDI nfs_lock.c,v 2.4 1998/12/14 23:49:56 jch Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/nfsclient/nfs_lock.c,v 1.39 2003/11/14 20:54:08 alfred Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>		/* for hz */
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/lockf.h>		/* for hz */ /* Must come after sys/malloc.h */
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <net/if.h>

#include <rpc/rpcclnt.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsmount.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfs_lock.h>
#include <nfsclient/nlminfo.h>

/*
 * XXX
 * We have to let the process know if the call succeeded.  I'm using an extra
 * field in the p_nlminfo field in the proc structure, as it is already for
 * lockd stuff.
 */

/*
 * nfs_advlock --
 *      NFS advisory byte-level locks.
 */
int
nfs_dolock(struct vop_advlock_args *ap)
{
	LOCKD_MSG msg;
	struct nameidata nd;
	struct thread *td;
	struct vnode *vp, *wvp;
	int error, error1;
	struct flock *fl;
	int fmode, ioflg;
	struct proc *p;

	td = curthread;
	p = td->td_proc;

	vp = ap->a_vp;
	fl = ap->a_fl;

	/*
	 * the NLM protocol doesn't allow the server to return an error
	 * on ranges, so we do it.
	 */
	if (fl->l_whence != SEEK_END) {
		if ((fl->l_whence != SEEK_CUR && fl->l_whence != SEEK_SET) ||
		    fl->l_start < 0 ||
		    (fl->l_len < 0 &&
		     (fl->l_start == 0 || fl->l_start + fl->l_len < 0)))
			return (EINVAL);
		if (fl->l_len > 0 &&
			 (fl->l_len - 1 > OFF_MAX - fl->l_start))
			return (EOVERFLOW);
	}

	/*
	 * Fill in the information structure.
	 */
	msg.lm_version = LOCKD_MSG_VERSION;
	msg.lm_msg_ident.pid = p->p_pid;
	/*
	 * if there is no nfsowner table yet, allocate one.
	 */
	if (p->p_nlminfo == NULL) {
		MALLOC(p->p_nlminfo, struct nlminfo *,
			sizeof(struct nlminfo), M_LOCKF, M_WAITOK | M_ZERO);
		p->p_nlminfo->pid_start = p->p_stats->p_start;
		timevaladd(&p->p_nlminfo->pid_start, &boottime);
	}
	msg.lm_msg_ident.pid_start = p->p_nlminfo->pid_start;
	msg.lm_msg_ident.msg_seq = ++(p->p_nlminfo->msg_seq);

	msg.lm_fl = *fl;
	msg.lm_wait = ap->a_flags & F_WAIT;
	msg.lm_getlk = ap->a_op == F_GETLK;
	bcopy(VFSTONFS(vp->v_mount)->nm_nam, &msg.lm_addr,
		min(sizeof msg.lm_addr, VFSTONFS(vp->v_mount)->nm_nam->sa_len));
	msg.lm_fh_len = NFS_ISV3(vp) ? VTONFS(vp)->n_fhsize : NFSX_V2FH;
	bcopy(VTONFS(vp)->n_fhp, msg.lm_fh, msg.lm_fh_len);
	msg.lm_nfsv3 = NFS_ISV3(vp);
	cru2x(td->td_ucred, &msg.lm_cred);

	/*
	 * Open the lock fifo.  If for any reason we don't find the fifo, it
	 * means that the lock daemon isn't running.  Translate any missing
	 * file error message for the user, otherwise the application will
	 * complain that the user's file is missing, which isn't the case.
	 * Note that we use proc0's cred, so the fifo is opened as root.
	 *
	 * XXX: Note that this behavior is relative to the root directory
	 * of the current process, and this may result in a variety of
	 * {functional, security} problems in chroot() environments.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, _PATH_LCKFIFO, td);

	fmode = FFLAGS(O_WRONLY | O_NONBLOCK);
	error = vn_open_cred(&nd, &fmode, 0, thread0.td_ucred, -1);
	switch (error) {
	case ENOENT:
	case ENXIO:
		/*
		 * Map a failure to find the fifo or no listener on the
		 * fifo to locking not being supported.
		 */
		return (EOPNOTSUPP);
	case 0:
		break;
	default:
		return (error);
	}
	wvp = nd.ni_vp;
	VOP_UNLOCK(wvp, 0, td);		/* vn_open leaves it locked */


	ioflg = IO_UNIT | IO_NOMACCHECK;
	for (;;) {
		VOP_LEASE(wvp, td, thread0.td_ucred, LEASE_WRITE);

		error = vn_rdwr(UIO_WRITE, wvp, (caddr_t)&msg, sizeof(msg), 0,
		    UIO_SYSSPACE, ioflg, thread0.td_ucred, NOCRED, NULL, td);

		if (error && (((ioflg & IO_NDELAY) == 0) || error != EAGAIN)) {
			break;
		}
		/*
		 * If we're locking a file, wait for an answer.  Unlocks succeed
		 * immediately.
		 */
		if (fl->l_type == F_UNLCK)
			/*
			 * XXX this isn't exactly correct.  The client side
			 * needs to continue sending it's unlock until
			 * it gets a responce back.
			 */
			break;

		/*
		 * retry after 20 seconds if we haven't gotten a responce yet.
		 * This number was picked out of thin air... but is longer
		 * then even a reasonably loaded system should take (at least
		 * on a local network).  XXX Probably should use a back-off
		 * scheme.
		 *
		 * XXX: No PCATCH here since we currently have no useful
		 * way to signal to the userland rpc.lockd that the request
		 * has been aborted.  Once the rpc.lockd implementation
		 * can handle aborts, and we report them properly,
		 * PCATCH can be put back.  In the mean time, if we did
		 * permit aborting, the lock attempt would "get lost"
		 * and the lock would get stuck in the locked state.
		 */
		error = tsleep(p->p_nlminfo, PUSER, "lockd", 20*hz);
		if (error != 0) {
			if (error == EWOULDBLOCK) {
				/*
				 * We timed out, so we rewrite the request
				 * to the fifo, but only if it isn't already
				 * full.
				 */
				ioflg |= IO_NDELAY;
				continue;
			}

			break;
		}

		if (msg.lm_getlk && p->p_nlminfo->retcode == 0) {
			if (p->p_nlminfo->set_getlk_pid) {
				fl->l_pid = p->p_nlminfo->getlk_pid;
			} else {
				fl->l_type = F_UNLCK;
			}
		}
		error = p->p_nlminfo->retcode;
		break;
	}

	error1 = vn_close(wvp, FWRITE, thread0.td_ucred, td);
	/* prefer any previous 'error' to our vn_close 'error1'. */
	return (error != 0 ? error : error1);
}

/*
 * nfslockdans --
 *      NFS advisory byte-level locks answer from the lock daemon.
 */
int
nfslockdans(struct thread *td, struct lockd_ans *ansp)
{
	struct proc *targetp;
	int error;

	/* Let root, or someone who once was root (lockd generally
	 * switches to the daemon uid once it is done setting up) make
	 * this call.
	 *
	 * XXX This authorization check is probably not right.
	 */
	if ((error = suser(td)) != 0 &&
	    td->td_ucred->cr_svuid != 0)
		return (error);

	/* the version should match, or we're out of sync */
	if (ansp->la_vers != LOCKD_ANS_VERSION)
		return (EINVAL);

	/* Find the process, set its return errno and wake it up. */
	if ((targetp = pfind(ansp->la_msg_ident.pid)) == NULL)
		return (ESRCH);

	/* verify the pid hasn't been reused (if we can), and it isn't waiting
	 * for an answer from a more recent request.  We return an EPIPE if
	 * the match fails, because we've already used ESRCH above, and this
	 * is sort of like writing on a pipe after the reader has closed it.
	 */
	if (targetp->p_nlminfo == NULL ||
	    ((ansp->la_msg_ident.msg_seq != -1) &&
	      (timevalcmp(&targetp->p_nlminfo->pid_start,
			&ansp->la_msg_ident.pid_start, !=) ||
	       targetp->p_nlminfo->msg_seq != ansp->la_msg_ident.msg_seq))) {
		PROC_UNLOCK(targetp);
		return (EPIPE);
	}

	targetp->p_nlminfo->retcode = ansp->la_errno;
	targetp->p_nlminfo->set_getlk_pid = ansp->la_set_getlk_pid;
	targetp->p_nlminfo->getlk_pid = ansp->la_getlk_pid;

	wakeup(targetp->p_nlminfo);

	PROC_UNLOCK(targetp);
	return (0);
}
