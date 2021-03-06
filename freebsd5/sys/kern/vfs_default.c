/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed
 * to Berkeley by John Heidemann of the UCLA Ficus project.
 *
 * Source: * @(#)i405_init.c 2.10 92/04/27 UCLA Ficus project
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/vfs_default.c,v 1.91 2003/11/05 04:30:07 kan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

static int	vop_nolookup(struct vop_lookup_args *);
static int	vop_nostrategy(struct vop_strategy_args *);

/*
 * This vnode table stores what we want to do if the filesystem doesn't
 * implement a particular VOP.
 *
 * If there is no specific entry here, we will return EOPNOTSUPP.
 *
 */

vop_t **default_vnodeop_p;
static struct vnodeopv_entry_desc default_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_eopnotsupp },
	{ &vop_advlock_desc,		(vop_t *) vop_einval },
	{ &vop_bmap_desc,		(vop_t *) vop_stdbmap },
	{ &vop_close_desc,		(vop_t *) vop_null },
	{ &vop_createvobject_desc,	(vop_t *) vop_stdcreatevobject },
	{ &vop_destroyvobject_desc,	(vop_t *) vop_stddestroyvobject },
	{ &vop_fsync_desc,		(vop_t *) vop_null },
	{ &vop_getpages_desc,		(vop_t *) vop_stdgetpages },
	{ &vop_getvobject_desc,		(vop_t *) vop_stdgetvobject },
	{ &vop_inactive_desc,		(vop_t *) vop_stdinactive },
	{ &vop_ioctl_desc,		(vop_t *) vop_enotty },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_lease_desc,		(vop_t *) vop_null },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_lookup_desc,		(vop_t *) vop_nolookup },
	{ &vop_open_desc,		(vop_t *) vop_null },
	{ &vop_pathconf_desc,		(vop_t *) vop_einval },
	{ &vop_poll_desc,		(vop_t *) vop_nopoll },
	{ &vop_putpages_desc,		(vop_t *) vop_stdputpages },
	{ &vop_readlink_desc,		(vop_t *) vop_einval },
	{ &vop_revoke_desc,		(vop_t *) vop_revoke },
	{ &vop_specstrategy_desc,	(vop_t *) vop_panic },
	{ &vop_strategy_desc,		(vop_t *) vop_nostrategy },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ NULL, NULL }
};

static struct vnodeopv_desc default_vnodeop_opv_desc =
        { &default_vnodeop_p, default_vnodeop_entries };

VNODEOP_SET(default_vnodeop_opv_desc);

/*
 * Series of placeholder functions for various error returns for
 * VOPs.
 */

int
vop_eopnotsupp(struct vop_generic_args *ap)
{
	/*
	printf("vop_notsupp[%s]\n", ap->a_desc->vdesc_name);
	*/

	return (EOPNOTSUPP);
}

int
vop_ebadf(struct vop_generic_args *ap)
{

	return (EBADF);
}

int
vop_enotty(struct vop_generic_args *ap)
{

	return (ENOTTY);
}

int
vop_einval(struct vop_generic_args *ap)
{

	return (EINVAL);
}

int
vop_null(struct vop_generic_args *ap)
{

	return (0);
}

/*
 * Used to make a defined VOP fall back to the default VOP.
 */
int
vop_defaultop(struct vop_generic_args *ap)
{

	return (VOCALL(default_vnodeop_p, ap->a_desc->vdesc_offset, ap));
}

/*
 * Helper function to panic on some bad VOPs in some filesystems.
 */
int
vop_panic(struct vop_generic_args *ap)
{

	panic("filesystem goof: vop_panic[%s]", ap->a_desc->vdesc_name);
}

/*
 * vop_std<something> and vop_no<something> are default functions for use by
 * filesystems that need the "default reasonable" implementation for a
 * particular operation.
 *
 * The documentation for the operations they implement exists (if it exists)
 * in the VOP_<SOMETHING>(9) manpage (all uppercase).
 */

/*
 * Default vop for filesystems that do not support name lookup
 */
static int
vop_nolookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 *	vop_nostrategy:
 *
 *	Strategy routine for VFS devices that have none.
 *
 *	BIO_ERROR and B_INVAL must be cleared prior to calling any strategy
 *	routine.  Typically this is done for a BIO_READ strategy call.
 *	Typically B_INVAL is assumed to already be clear prior to a write
 *	and should not be cleared manually unless you just made the buffer
 *	invalid.  BIO_ERROR should be cleared either way.
 */

static int
vop_nostrategy (struct vop_strategy_args *ap)
{
	KASSERT(ap->a_vp == ap->a_bp->b_vp, ("%s(%p != %p)",
	    __func__, ap->a_vp, ap->a_bp->b_vp));
	printf("No strategy for buffer at %p\n", ap->a_bp);
	vprint("vnode", ap->a_vp);
	vprint("device vnode", ap->a_bp->b_vp);
	ap->a_bp->b_ioflags |= BIO_ERROR;
	ap->a_bp->b_error = EOPNOTSUPP;
	bufdone(ap->a_bp);
	return (EOPNOTSUPP);
}

/*
 * vop_stdpathconf:
 *
 * Standard implementation of POSIX pathconf, to get information about limits
 * for a filesystem.
 * Override per filesystem for the case where the filesystem has smaller
 * limits.
 */
int
vop_stdpathconf(ap)
	struct vop_pathconf_args /* {
	struct vnode *a_vp;
	int a_name;
	int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
		case _PC_LINK_MAX:
			*ap->a_retval = LINK_MAX;
			return (0);
		case _PC_MAX_CANON:
			*ap->a_retval = MAX_CANON;
			return (0);
		case _PC_MAX_INPUT:
			*ap->a_retval = MAX_INPUT;
			return (0);
		case _PC_PIPE_BUF:
			*ap->a_retval = PIPE_BUF;
			return (0);
		case _PC_CHOWN_RESTRICTED:
			*ap->a_retval = 1;
			return (0);
		case _PC_VDISABLE:
			*ap->a_retval = _POSIX_VDISABLE;
			return (0);
		default:
			return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Standard lock, unlock and islocked functions.
 */
int
vop_stdlock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

#ifndef	DEBUG_LOCKS
	return (lockmgr(vp->v_vnlock, ap->a_flags, VI_MTX(vp), ap->a_td));
#else
	return (debuglockmgr(vp->v_vnlock, ap->a_flags, VI_MTX(vp),
	    ap->a_td, "vop_stdlock", vp->filename, vp->line));
#endif
}

/* See above. */
int
vop_stdunlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	return (lockmgr(vp->v_vnlock, ap->a_flags | LK_RELEASE, VI_MTX(vp),
	    ap->a_td));
}

/* See above. */
int
vop_stdislocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{

	return (lockstatus(ap->a_vp->v_vnlock, ap->a_td));
}

/* Mark the vnode inactive */
int
vop_stdinactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{

	VOP_UNLOCK(ap->a_vp, 0, ap->a_td);
	return (0);
}

/*
 * Return true for select/poll.
 */
int
vop_nopoll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	/*
	 * Return true for read/write.  If the user asked for something
	 * special, return POLLNVAL, so that clients have a way of
	 * determining reliably whether or not the extended
	 * functionality is present without hard-coding knowledge
	 * of specific filesystem implementations.
	 * Stay in sync with kern_conf.c::no_poll().
	 */
	if (ap->a_events & ~POLLSTANDARD)
		return (POLLNVAL);

	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Implement poll for local filesystems that support it.
 */
int
vop_stdpoll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	if (ap->a_events & ~POLLSTANDARD)
		return (vn_pollrecord(ap->a_vp, ap->a_td, ap->a_events));
	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Stubs to use when there is no locking to be done on the underlying object.
 * A minimal shared lock is necessary to ensure that the underlying object
 * is not revoked while an operation is in progress. So, an active shared
 * count is maintained in an auxillary vnode lock structure.
 */
int
vop_sharedlock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{
	/*
	 * This code cannot be used until all the non-locking filesystems
	 * (notably NFS) are converted to properly lock and release nodes.
	 * Also, certain vnode operations change the locking state within
	 * the operation (create, mknod, remove, link, rename, mkdir, rmdir,
	 * and symlink). Ideally these operations should not change the
	 * lock state, but should be changed to let the caller of the
	 * function unlock them. Otherwise all intermediate vnode layers
	 * (such as union, umapfs, etc) must catch these functions to do
	 * the necessary locking at their layer. Note that the inactive
	 * and lookup operations also change their lock state, but this
	 * cannot be avoided, so these two operations will always need
	 * to be handled in intermediate layers.
	 */
	struct vnode *vp = ap->a_vp;
	int vnflags, flags = ap->a_flags;

	switch (flags & LK_TYPE_MASK) {
	case LK_DRAIN:
		vnflags = LK_DRAIN;
		break;
	case LK_EXCLUSIVE:
#ifdef DEBUG_VFS_LOCKS
		/*
		 * Normally, we use shared locks here, but that confuses
		 * the locking assertions.
		 */
		vnflags = LK_EXCLUSIVE;
		break;
#endif
	case LK_SHARED:
		vnflags = LK_SHARED;
		break;
	case LK_UPGRADE:
	case LK_EXCLUPGRADE:
	case LK_DOWNGRADE:
		return (0);
	case LK_RELEASE:
	default:
		panic("vop_sharedlock: bad operation %d", flags & LK_TYPE_MASK);
	}
	vnflags |= flags & (LK_INTERLOCK | LK_EXTFLG_MASK);
#ifndef	DEBUG_LOCKS
	return (lockmgr(vp->v_vnlock, vnflags, VI_MTX(vp), ap->a_td));
#else
	return (debuglockmgr(vp->v_vnlock, vnflags, VI_MTX(vp), ap->a_td,
	    "vop_sharedlock", vp->filename, vp->line));
#endif
}

/*
 * Stubs to use when there is no locking to be done on the underlying object.
 * A minimal shared lock is necessary to ensure that the underlying object
 * is not revoked while an operation is in progress. So, an active shared
 * count is maintained in an auxillary vnode lock structure.
 */
int
vop_nolock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{
#ifdef notyet
	/*
	 * This code cannot be used until all the non-locking filesystems
	 * (notably NFS) are converted to properly lock and release nodes.
	 * Also, certain vnode operations change the locking state within
	 * the operation (create, mknod, remove, link, rename, mkdir, rmdir,
	 * and symlink). Ideally these operations should not change the
	 * lock state, but should be changed to let the caller of the
	 * function unlock them. Otherwise all intermediate vnode layers
	 * (such as union, umapfs, etc) must catch these functions to do
	 * the necessary locking at their layer. Note that the inactive
	 * and lookup operations also change their lock state, but this
	 * cannot be avoided, so these two operations will always need
	 * to be handled in intermediate layers.
	 */
	struct vnode *vp = ap->a_vp;
	int vnflags, flags = ap->a_flags;

	switch (flags & LK_TYPE_MASK) {
	case LK_DRAIN:
		vnflags = LK_DRAIN;
		break;
	case LK_EXCLUSIVE:
	case LK_SHARED:
		vnflags = LK_SHARED;
		break;
	case LK_UPGRADE:
	case LK_EXCLUPGRADE:
	case LK_DOWNGRADE:
		return (0);
	case LK_RELEASE:
	default:
		panic("vop_nolock: bad operation %d", flags & LK_TYPE_MASK);
	}
	vnflags |= flags & (LK_INTERLOCK | LK_EXTFLG_MASK);
	return(lockmgr(vp->v_vnlock, vnflags, VI_MTX(vp), ap->a_td));
#else /* for now */
	/*
	 * Since we are not using the lock manager, we must clear
	 * the interlock here.
	 */
	if (ap->a_flags & LK_INTERLOCK)
		VI_UNLOCK(ap->a_vp);
	return (0);
#endif
}

/*
 * Do the inverse of vop_nolock, handling the interlock in a compatible way.
 */
int
vop_nounlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{

	/*
	 * Since we are not using the lock manager, we must clear
	 * the interlock here.
	 */
	if (ap->a_flags & LK_INTERLOCK)
		VI_UNLOCK(ap->a_vp);
	return (0);
}

/*
 * Return whether or not the node is in use.
 */
int
vop_noislocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{

	return (0);
}

/*
 * Return our mount point, as we will take charge of the writes.
 */
int
vop_stdgetwritemount(ap)
	struct vop_getwritemount_args /* {
		struct vnode *a_vp;
		struct mount **a_mpp;
	} */ *ap;
{

	*(ap->a_mpp) = ap->a_vp->v_mount;
	return (0);
}

/* Create the VM system backing object for this vnode */
int
vop_stdcreatevobject(ap)
	struct vop_createvobject_args /* {
		struct vnode *vp;
		struct ucred *cred;
		struct thread *td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ucred *cred = ap->a_cred;
	struct thread *td = ap->a_td;
	struct vattr vat;
	vm_object_t object;
	int error = 0;

	GIANT_REQUIRED;

	if (!vn_isdisk(vp, NULL) && vn_canvmio(vp) == FALSE)
		return (0);

retry:
	if ((object = vp->v_object) == NULL) {
		if (vp->v_type == VREG || vp->v_type == VDIR) {
			if ((error = VOP_GETATTR(vp, &vat, cred, td)) != 0)
				goto retn;
			object = vnode_pager_alloc(vp, vat.va_size, 0, 0);
		} else if (devsw(vp->v_rdev) != NULL) {
			/*
			 * This simply allocates the biggest object possible
			 * for a disk vnode.  This should be fixed, but doesn't
			 * cause any problems (yet).
			 */
			object = vnode_pager_alloc(vp, IDX_TO_OFF(INT_MAX), 0, 0);
		} else {
			goto retn;
		}
		/*
		 * Dereference the reference we just created.  This assumes
		 * that the object is associated with the vp.
		 */
		VM_OBJECT_LOCK(object);
		object->ref_count--;
		VM_OBJECT_UNLOCK(object);
		vrele(vp);
	} else {
		VM_OBJECT_LOCK(object);
		if (object->flags & OBJ_DEAD) {
			VOP_UNLOCK(vp, 0, td);
			msleep(object, VM_OBJECT_MTX(object), PDROP | PVM,
			    "vodead", 0);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
			goto retry;
		}
		VM_OBJECT_UNLOCK(object);
	}

	KASSERT(vp->v_object != NULL, ("vfs_object_create: NULL object"));
	vp->v_vflag |= VV_OBJBUF;

retn:
	return (error);
}

/* Destroy the VM system object associated with this vnode */
int
vop_stddestroyvobject(ap)
	struct vop_destroyvobject_args /* {
		struct vnode *vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	vm_object_t obj = vp->v_object;

	GIANT_REQUIRED;

	if (obj == NULL)
		return (0);
	VM_OBJECT_LOCK(obj);
	if (obj->ref_count == 0) {
		/*
		 * vclean() may be called twice. The first time
		 * removes the primary reference to the object,
		 * the second time goes one further and is a
		 * special-case to terminate the object.
		 *
		 * don't double-terminate the object
		 */
		if ((obj->flags & OBJ_DEAD) == 0)
			vm_object_terminate(obj);
		else
			VM_OBJECT_UNLOCK(obj);
	} else {
		/*
		 * Woe to the process that tries to page now :-).
		 */
		vm_pager_deallocate(obj);
		VM_OBJECT_UNLOCK(obj);
	}
	return (0);
}

/*
 * Return the underlying VM object.  This routine may be called with or
 * without the vnode interlock held.  If called without, the returned
 * object is not guarenteed to be valid.  The syncer typically gets the
 * object without holding the interlock in order to quickly test whether
 * it might be dirty before going heavy-weight.  vm_object's use zalloc
 * and thus stable-storage, so this is safe.
 */
int
vop_stdgetvobject(ap)
	struct vop_getvobject_args /* {
		struct vnode *vp;
		struct vm_object **objpp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vm_object **objpp = ap->a_objpp;

	if (objpp)
		*objpp = vp->v_object;
	return (vp->v_object ? 0 : EINVAL);
}

/* XXX Needs good comment and VOP_BMAP(9) manpage */
int
vop_stdbmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(ap->a_vp->v_mount->mnt_stat.f_iosize);
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

int
vop_stdfsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct buf *bp;
	struct buf *nbp;
	int s, error = 0;
	int maxretry = 100;     /* large, arbitrarily chosen */

	VI_LOCK(vp);
loop1:
	/*
	 * MARK/SCAN initialization to avoid infinite loops.
	 */
	s = splbio();
        TAILQ_FOREACH(bp, &vp->v_dirtyblkhd, b_vnbufs) {
                bp->b_vflags &= ~BV_SCANNED;
		bp->b_error = 0;
	}
	splx(s);

	/*
	 * Flush all dirty buffers associated with a block device.
	 */
loop2:
	s = splbio();
	for (bp = TAILQ_FIRST(&vp->v_dirtyblkhd); bp != NULL; bp = nbp) {
		nbp = TAILQ_NEXT(bp, b_vnbufs);
		if ((bp->b_vflags & BV_SCANNED) != 0)
			continue;
		bp->b_vflags |= BV_SCANNED;
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL))
			continue;
		VI_UNLOCK(vp);
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("fsync: not dirty");
		if ((vp->v_vflag & VV_OBJBUF) && (bp->b_flags & B_CLUSTEROK)) {
			vfs_bio_awrite(bp);
			splx(s);
		} else {
			bremfree(bp);
			splx(s);
			bawrite(bp);
		}
		VI_LOCK(vp);
		goto loop2;
	}

	/*
	 * If synchronous the caller expects us to completely resolve all
	 * dirty buffers in the system.  Wait for in-progress I/O to
	 * complete (which could include background bitmap writes), then
	 * retry if dirty blocks still exist.
	 */
	if (ap->a_waitfor == MNT_WAIT) {
		while (vp->v_numoutput) {
			vp->v_iflag |= VI_BWAIT;
			msleep((caddr_t)&vp->v_numoutput, VI_MTX(vp),
			    PRIBIO + 1, "fsync", 0);
		}
		if (!TAILQ_EMPTY(&vp->v_dirtyblkhd)) {
			/*
			 * If we are unable to write any of these buffers
			 * then we fail now rather than trying endlessly
			 * to write them out.
			 */
			TAILQ_FOREACH(bp, &vp->v_dirtyblkhd, b_vnbufs)
				if ((error = bp->b_error) == 0)
					continue;
			if (error == 0 && --maxretry >= 0) {
				splx(s);
				goto loop1;
			}
			vprint("fsync: giving up on dirty", vp);
			error = EAGAIN;
		}
	}
	VI_UNLOCK(vp);
	splx(s);

	return (error);
}

/* XXX Needs good comment and more info in the manpage (VOP_GETPAGES(9)). */
int
vop_stdgetpages(ap)
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_reqpage;
		vm_ooffset_t a_offset;
	} */ *ap;
{

	return vnode_pager_generic_getpages(ap->a_vp, ap->a_m,
	    ap->a_count, ap->a_reqpage);
}

/* XXX Needs good comment and more info in the manpage (VOP_PUTPAGES(9)). */
int
vop_stdputpages(ap)
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_sync;
		int *a_rtvals;
		vm_ooffset_t a_offset;
	} */ *ap;
{

	return vnode_pager_generic_putpages(ap->a_vp, ap->a_m, ap->a_count,
	     ap->a_sync, ap->a_rtvals);
}

/*
 * vfs default ops
 * used to fill the vfs function table to get reasonable default return values.
 */
int
vfs_stdroot (mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

int
vfs_stdstatfs (mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{
	return (EOPNOTSUPP);
}

int
vfs_stdvptofh (vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	return (EOPNOTSUPP);
}

int
vfs_stdstart (mp, flags, td)
	struct mount *mp;
	int flags;
	struct thread *td;
{
	return (0);
}

int
vfs_stdquotactl (mp, cmds, uid, arg, td)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct thread *td;
{
	return (EOPNOTSUPP);
}

int
vfs_stdsync(mp, waitfor, cred, td)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct thread *td;
{
	struct vnode *vp, *nvp;
	int error, lockreq, allerror = 0;

	lockreq = LK_EXCLUSIVE | LK_INTERLOCK;
	if (waitfor != MNT_WAIT)
		lockreq |= LK_NOWAIT;
	/*
	 * Force stale buffer cache information to be flushed.
	 */
	MNT_ILOCK(mp);
loop:
	for (vp = TAILQ_FIRST(&mp->mnt_nvnodelist); vp != NULL; vp = nvp) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;

		nvp = TAILQ_NEXT(vp, v_nmntvnodes);

		VI_LOCK(vp);
		if (TAILQ_EMPTY(&vp->v_dirtyblkhd)) {
			VI_UNLOCK(vp);
			continue;
		}
		MNT_IUNLOCK(mp);

		if ((error = vget(vp, lockreq, td)) != 0) {
			MNT_ILOCK(mp);
			if (error == ENOENT)
				goto loop;
			continue;
		}
		error = VOP_FSYNC(vp, cred, waitfor, td);
		if (error)
			allerror = error;

		VOP_UNLOCK(vp, 0, td);
		vrele(vp);
		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	return (allerror);
}

int
vfs_stdnosync (mp, waitfor, cred, td)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct thread *td;
{
	return (0);
}

int
vfs_stdvget (mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

int
vfs_stdfhtovp (mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

int
vfs_stdinit (vfsp)
	struct vfsconf *vfsp;
{
	return (0);
}

int
vfs_stduninit (vfsp)
	struct vfsconf *vfsp;
{
	return(0);
}

int
vfs_stdextattrctl(mp, cmd, filename_vp, attrnamespace, attrname, td)
	struct mount *mp;
	int cmd;
	struct vnode *filename_vp;
	int attrnamespace;
	const char *attrname;
	struct thread *td;
{
	if (filename_vp != NULL)
		VOP_UNLOCK(filename_vp, 0, td);
	return(EOPNOTSUPP);
}

/* end of vfs default ops */
