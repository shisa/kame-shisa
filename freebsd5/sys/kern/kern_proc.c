/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_proc.c	8.7 (Berkeley) 2/14/95
 * $FreeBSD: src/sys/kern/kern_proc.c,v 1.197 2003/10/16 08:39:15 jeff Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/kern_proc.c,v 1.197 2003/10/16 08:39:15 jeff Exp $");

#include "opt_ktrace.h"
#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/user.h>
#include <sys/jail.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/uma.h>
#include <machine/critical.h>

MALLOC_DEFINE(M_PGRP, "pgrp", "process group header");
MALLOC_DEFINE(M_SESSION, "session", "session header");
static MALLOC_DEFINE(M_PROC, "proc", "Proc structures");
MALLOC_DEFINE(M_SUBPROC, "subproc", "Proc sub-structures");

static void doenterpgrp(struct proc *, struct pgrp *);
static void orphanpg(struct pgrp *pg);
static void pgadjustjobc(struct pgrp *pgrp, int entering);
static void pgdelete(struct pgrp *);
static void proc_ctor(void *mem, int size, void *arg);
static void proc_dtor(void *mem, int size, void *arg);
static void proc_init(void *mem, int size);
static void proc_fini(void *mem, int size);

/*
 * Other process lists
 */
struct pidhashhead *pidhashtbl;
u_long pidhash;
struct pgrphashhead *pgrphashtbl;
u_long pgrphash;
struct proclist allproc;
struct proclist zombproc;
struct sx allproc_lock;
struct sx proctree_lock;
struct mtx pargs_ref_lock;
struct mtx ppeers_lock;
uma_zone_t proc_zone;
uma_zone_t ithread_zone;

int kstack_pages = KSTACK_PAGES;
int uarea_pages = UAREA_PAGES;
SYSCTL_INT(_kern, OID_AUTO, kstack_pages, CTLFLAG_RD, &kstack_pages, 0, "");
SYSCTL_INT(_kern, OID_AUTO, uarea_pages, CTLFLAG_RD, &uarea_pages, 0, "");

#define RANGEOF(type, start, end) (offsetof(type, end) - offsetof(type, start))

CTASSERT(sizeof(struct kinfo_proc) == KINFO_PROC_SIZE);

/*
 * Initialize global process hashing structures.
 */
void
procinit()
{

	sx_init(&allproc_lock, "allproc");
	sx_init(&proctree_lock, "proctree");
	mtx_init(&pargs_ref_lock, "struct pargs.ref", NULL, MTX_DEF);
	mtx_init(&ppeers_lock, "p_peers", NULL, MTX_DEF);
	LIST_INIT(&allproc);
	LIST_INIT(&zombproc);
	pidhashtbl = hashinit(maxproc / 4, M_PROC, &pidhash);
	pgrphashtbl = hashinit(maxproc / 4, M_PROC, &pgrphash);
	proc_zone = uma_zcreate("PROC", sched_sizeof_proc(),
	    proc_ctor, proc_dtor, proc_init, proc_fini,
	    UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uihashinit();
}

/*
 * Prepare a proc for use.
 */
static void
proc_ctor(void *mem, int size, void *arg)
{
	struct proc *p;

	p = (struct proc *)mem;
}

/*
 * Reclaim a proc after use.
 */
static void
proc_dtor(void *mem, int size, void *arg)
{
	struct proc *p;
	struct thread *td;
	struct ksegrp *kg;
	struct kse *ke;

	/* INVARIANTS checks go here */
	p = (struct proc *)mem;
	KASSERT((p->p_numthreads == 1),
	    ("bad number of threads in exiting process"));
        td = FIRST_THREAD_IN_PROC(p);
	KASSERT((td != NULL), ("proc_dtor: bad thread pointer"));
        kg = FIRST_KSEGRP_IN_PROC(p);
	KASSERT((kg != NULL), ("proc_dtor: bad kg pointer"));
        ke = FIRST_KSE_IN_KSEGRP(kg);
	KASSERT((ke != NULL), ("proc_dtor: bad ke pointer"));

	/* Dispose of an alternate kstack, if it exists.
	 * XXX What if there are more than one thread in the proc?
	 *     The first thread in the proc is special and not
	 *     freed, so you gotta do this here.
	 */
	if (((p->p_flag & P_KTHREAD) != 0) && (td->td_altkstack != 0))
		vm_thread_dispose_altkstack(td);

	/*
	 * We want to make sure we know the initial linkages.
	 * so for now tear them down and remake them.
	 * This is probably un-needed as we can probably rely
	 * on the state coming in here from wait4().
	 */
	proc_linkup(p, kg, ke, td);
}

/*
 * Initialize type-stable parts of a proc (when newly created).
 */
static void
proc_init(void *mem, int size)
{
	struct proc *p;
	struct thread *td;
	struct ksegrp *kg;
	struct kse *ke;

	p = (struct proc *)mem;
	p->p_sched = (struct p_sched *)&p[1];
	vm_proc_new(p);
	td = thread_alloc();
	ke = kse_alloc();
	kg = ksegrp_alloc();
	proc_linkup(p, kg, ke, td);
	bzero(&p->p_mtx, sizeof(struct mtx));
	mtx_init(&p->p_mtx, "process lock", NULL, MTX_DEF | MTX_DUPOK);
}

/*
 * Tear down type-stable parts of a proc (just before being discarded)
 */
static void
proc_fini(void *mem, int size)
{
	struct proc *p;
	struct thread *td;
	struct ksegrp *kg;
	struct kse *ke;

	p = (struct proc *)mem;
	KASSERT((p->p_numthreads == 1),
	    ("bad number of threads in freeing process"));
        td = FIRST_THREAD_IN_PROC(p);
	KASSERT((td != NULL), ("proc_dtor: bad thread pointer"));
        kg = FIRST_KSEGRP_IN_PROC(p);
	KASSERT((kg != NULL), ("proc_dtor: bad kg pointer"));
        ke = FIRST_KSE_IN_KSEGRP(kg);
	KASSERT((ke != NULL), ("proc_dtor: bad ke pointer"));
	vm_proc_dispose(p);
	thread_free(td);
	ksegrp_free(kg);
	kse_free(ke);
	mtx_destroy(&p->p_mtx);
}

/*
 * Is p an inferior of the current process?
 */
int
inferior(p)
	register struct proc *p;
{

	sx_assert(&proctree_lock, SX_LOCKED);
	for (; p != curproc; p = p->p_pptr)
		if (p->p_pid == 0)
			return (0);
	return (1);
}

/*
 * Locate a process by number
 */
struct proc *
pfind(pid)
	register pid_t pid;
{
	register struct proc *p;

	sx_slock(&allproc_lock);
	LIST_FOREACH(p, PIDHASH(pid), p_hash)
		if (p->p_pid == pid) {
			PROC_LOCK(p);
			break;
		}
	sx_sunlock(&allproc_lock);
	return (p);
}

/*
 * Locate a process group by number.
 * The caller must hold proctree_lock.
 */
struct pgrp *
pgfind(pgid)
	register pid_t pgid;
{
	register struct pgrp *pgrp;

	sx_assert(&proctree_lock, SX_LOCKED);

	LIST_FOREACH(pgrp, PGRPHASH(pgid), pg_hash) {
		if (pgrp->pg_id == pgid) {
			PGRP_LOCK(pgrp);
			return (pgrp);
		}
	}
	return (NULL);
}

/*
 * Create a new process group.
 * pgid must be equal to the pid of p.
 * Begin a new session if required.
 */
int
enterpgrp(p, pgid, pgrp, sess)
	register struct proc *p;
	pid_t pgid;
	struct pgrp *pgrp;
	struct session *sess;
{
	struct pgrp *pgrp2;

	sx_assert(&proctree_lock, SX_XLOCKED);

	KASSERT(pgrp != NULL, ("enterpgrp: pgrp == NULL"));
	KASSERT(p->p_pid == pgid,
	    ("enterpgrp: new pgrp and pid != pgid"));

	pgrp2 = pgfind(pgid);

	KASSERT(pgrp2 == NULL,
	    ("enterpgrp: pgrp with pgid exists"));
	KASSERT(!SESS_LEADER(p),
	    ("enterpgrp: session leader attempted setpgrp"));

	mtx_init(&pgrp->pg_mtx, "process group", NULL, MTX_DEF | MTX_DUPOK);

	if (sess != NULL) {
		/*
		 * new session
		 */
		mtx_init(&sess->s_mtx, "session", NULL, MTX_DEF);
		PROC_LOCK(p);
		p->p_flag &= ~P_CONTROLT;
		PROC_UNLOCK(p);
		PGRP_LOCK(pgrp);
		sess->s_leader = p;
		sess->s_sid = p->p_pid;
		sess->s_count = 1;
		sess->s_ttyvp = NULL;
		sess->s_ttyp = NULL;
		bcopy(p->p_session->s_login, sess->s_login,
			    sizeof(sess->s_login));
		pgrp->pg_session = sess;
		KASSERT(p == curproc,
		    ("enterpgrp: mksession and p != curproc"));
	} else {
		pgrp->pg_session = p->p_session;
		SESS_LOCK(pgrp->pg_session);
		pgrp->pg_session->s_count++;
		SESS_UNLOCK(pgrp->pg_session);
		PGRP_LOCK(pgrp);
	}
	pgrp->pg_id = pgid;
	LIST_INIT(&pgrp->pg_members);

	/*
	 * As we have an exclusive lock of proctree_lock,
	 * this should not deadlock.
	 */
	LIST_INSERT_HEAD(PGRPHASH(pgid), pgrp, pg_hash);
	pgrp->pg_jobc = 0;
	SLIST_INIT(&pgrp->pg_sigiolst);
	PGRP_UNLOCK(pgrp);

	doenterpgrp(p, pgrp);

	return (0);
}

/*
 * Move p to an existing process group
 */
int
enterthispgrp(p, pgrp)
	register struct proc *p;
	struct pgrp *pgrp;
{

	sx_assert(&proctree_lock, SX_XLOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(p->p_pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(p->p_session, MA_NOTOWNED);
	KASSERT(pgrp->pg_session == p->p_session,
		("%s: pgrp's session %p, p->p_session %p.\n",
		__func__,
		pgrp->pg_session,
		p->p_session));
	KASSERT(pgrp != p->p_pgrp,
		("%s: p belongs to pgrp.", __func__));

	doenterpgrp(p, pgrp);

	return (0);
}

/*
 * Move p to a process group
 */
static void
doenterpgrp(p, pgrp)
	struct proc *p;
	struct pgrp *pgrp;
{
	struct pgrp *savepgrp;

	sx_assert(&proctree_lock, SX_XLOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(p->p_pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(p->p_session, MA_NOTOWNED);

	savepgrp = p->p_pgrp;

	/*
	 * Adjust eligibility of affected pgrps to participate in job control.
	 * Increment eligibility counts before decrementing, otherwise we
	 * could reach 0 spuriously during the first call.
	 */
	fixjobc(p, pgrp, 1);
	fixjobc(p, p->p_pgrp, 0);

	PGRP_LOCK(pgrp);
	PGRP_LOCK(savepgrp);
	PROC_LOCK(p);
	LIST_REMOVE(p, p_pglist);
	p->p_pgrp = pgrp;
	PROC_UNLOCK(p);
	LIST_INSERT_HEAD(&pgrp->pg_members, p, p_pglist);
	PGRP_UNLOCK(savepgrp);
	PGRP_UNLOCK(pgrp);
	if (LIST_EMPTY(&savepgrp->pg_members))
		pgdelete(savepgrp);
}

/*
 * remove process from process group
 */
int
leavepgrp(p)
	register struct proc *p;
{
	struct pgrp *savepgrp;

	sx_assert(&proctree_lock, SX_XLOCKED);
	savepgrp = p->p_pgrp;
	PGRP_LOCK(savepgrp);
	PROC_LOCK(p);
	LIST_REMOVE(p, p_pglist);
	p->p_pgrp = NULL;
	PROC_UNLOCK(p);
	PGRP_UNLOCK(savepgrp);
	if (LIST_EMPTY(&savepgrp->pg_members))
		pgdelete(savepgrp);
	return (0);
}

/*
 * delete a process group
 */
static void
pgdelete(pgrp)
	register struct pgrp *pgrp;
{
	struct session *savesess;

	sx_assert(&proctree_lock, SX_XLOCKED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(pgrp->pg_session, MA_NOTOWNED);

	/*
	 * Reset any sigio structures pointing to us as a result of
	 * F_SETOWN with our pgid.
	 */
	funsetownlst(&pgrp->pg_sigiolst);

	PGRP_LOCK(pgrp);
	if (pgrp->pg_session->s_ttyp != NULL &&
	    pgrp->pg_session->s_ttyp->t_pgrp == pgrp)
		pgrp->pg_session->s_ttyp->t_pgrp = NULL;
	LIST_REMOVE(pgrp, pg_hash);
	savesess = pgrp->pg_session;
	SESS_LOCK(savesess);
	savesess->s_count--;
	SESS_UNLOCK(savesess);
	PGRP_UNLOCK(pgrp);
	if (savesess->s_count == 0) {
		mtx_destroy(&savesess->s_mtx);
		FREE(pgrp->pg_session, M_SESSION);
	}
	mtx_destroy(&pgrp->pg_mtx);
	FREE(pgrp, M_PGRP);
}

static void
pgadjustjobc(pgrp, entering)
	struct pgrp *pgrp;
	int entering;
{

	PGRP_LOCK(pgrp);
	if (entering)
		pgrp->pg_jobc++;
	else {
		--pgrp->pg_jobc;
		if (pgrp->pg_jobc == 0)
			orphanpg(pgrp);
	}
	PGRP_UNLOCK(pgrp);
}

/*
 * Adjust pgrp jobc counters when specified process changes process group.
 * We count the number of processes in each process group that "qualify"
 * the group for terminal job control (those with a parent in a different
 * process group of the same session).  If that count reaches zero, the
 * process group becomes orphaned.  Check both the specified process'
 * process group and that of its children.
 * entering == 0 => p is leaving specified group.
 * entering == 1 => p is entering specified group.
 */
void
fixjobc(p, pgrp, entering)
	register struct proc *p;
	register struct pgrp *pgrp;
	int entering;
{
	register struct pgrp *hispgrp;
	register struct session *mysession;

	sx_assert(&proctree_lock, SX_LOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(pgrp->pg_session, MA_NOTOWNED);

	/*
	 * Check p's parent to see whether p qualifies its own process
	 * group; if so, adjust count for p's process group.
	 */
	mysession = pgrp->pg_session;
	if ((hispgrp = p->p_pptr->p_pgrp) != pgrp &&
	    hispgrp->pg_session == mysession)
		pgadjustjobc(pgrp, entering);

	/*
	 * Check this process' children to see whether they qualify
	 * their process groups; if so, adjust counts for children's
	 * process groups.
	 */
	LIST_FOREACH(p, &p->p_children, p_sibling) {
		hispgrp = p->p_pgrp;
		if (hispgrp == pgrp ||
		    hispgrp->pg_session != mysession)
			continue;
		PROC_LOCK(p);
		if (p->p_state == PRS_ZOMBIE) {
			PROC_UNLOCK(p);
			continue;
		}
		PROC_UNLOCK(p);
		pgadjustjobc(hispgrp, entering);
	}
}

/*
 * A process group has become orphaned;
 * if there are any stopped processes in the group,
 * hang-up all process in that group.
 */
static void
orphanpg(pg)
	struct pgrp *pg;
{
	register struct proc *p;

	PGRP_LOCK_ASSERT(pg, MA_OWNED);

	LIST_FOREACH(p, &pg->pg_members, p_pglist) {
		PROC_LOCK(p);
		if (P_SHOULDSTOP(p)) {
			PROC_UNLOCK(p);
			LIST_FOREACH(p, &pg->pg_members, p_pglist) {
				PROC_LOCK(p);
				psignal(p, SIGHUP);
				psignal(p, SIGCONT);
				PROC_UNLOCK(p);
			}
			return;
		}
		PROC_UNLOCK(p);
	}
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(pgrpdump, pgrpdump)
{
	register struct pgrp *pgrp;
	register struct proc *p;
	register int i;

	for (i = 0; i <= pgrphash; i++) {
		if (!LIST_EMPTY(&pgrphashtbl[i])) {
			printf("\tindx %d\n", i);
			LIST_FOREACH(pgrp, &pgrphashtbl[i], pg_hash) {
				printf(
			"\tpgrp %p, pgid %ld, sess %p, sesscnt %d, mem %p\n",
				    (void *)pgrp, (long)pgrp->pg_id,
				    (void *)pgrp->pg_session,
				    pgrp->pg_session->s_count,
				    (void *)LIST_FIRST(&pgrp->pg_members));
				LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
					printf("\t\tpid %ld addr %p pgrp %p\n", 
					    (long)p->p_pid, (void *)p,
					    (void *)p->p_pgrp);
				}
			}
		}
	}
}
#endif /* DDB */
void
fill_kinfo_thread(struct thread *td, struct kinfo_proc *kp);

/*
 * Fill in a kinfo_proc structure for the specified process.
 * Must be called with the target process locked.
 */
void
fill_kinfo_proc(struct proc *p, struct kinfo_proc *kp)
{
	fill_kinfo_thread(FIRST_THREAD_IN_PROC(p), kp);
}

void
fill_kinfo_thread(struct thread *td, struct kinfo_proc *kp)
{
	struct proc *p;
	struct thread *td0;
	struct kse *ke;
	struct ksegrp *kg;
	struct tty *tp;
	struct session *sp;
	struct timeval tv;
	struct sigacts *ps;

	p = td->td_proc;

	bzero(kp, sizeof(*kp));

	kp->ki_structsize = sizeof(*kp);
	kp->ki_paddr = p;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	kp->ki_addr =/* p->p_addr; */0; /* XXXKSE */
	kp->ki_args = p->p_args;
	kp->ki_textvp = p->p_textvp;
#ifdef KTRACE
	kp->ki_tracep = p->p_tracevp;
	mtx_lock(&ktrace_mtx);
	kp->ki_traceflag = p->p_traceflag;
	mtx_unlock(&ktrace_mtx);
#endif
	kp->ki_fd = p->p_fd;
	kp->ki_vmspace = p->p_vmspace;
	if (p->p_ucred) {
		kp->ki_uid = p->p_ucred->cr_uid;
		kp->ki_ruid = p->p_ucred->cr_ruid;
		kp->ki_svuid = p->p_ucred->cr_svuid;
		/* XXX bde doesn't like KI_NGROUPS */
		kp->ki_ngroups = min(p->p_ucred->cr_ngroups, KI_NGROUPS);
		bcopy(p->p_ucred->cr_groups, kp->ki_groups,
		    kp->ki_ngroups * sizeof(gid_t));
		kp->ki_rgid = p->p_ucred->cr_rgid;
		kp->ki_svgid = p->p_ucred->cr_svgid;
	}
	if (p->p_sigacts) {
		ps = p->p_sigacts;
		mtx_lock(&ps->ps_mtx);
		kp->ki_sigignore = ps->ps_sigignore;
		kp->ki_sigcatch = ps->ps_sigcatch;
		mtx_unlock(&ps->ps_mtx);
	}
	mtx_lock_spin(&sched_lock);
	if (p->p_state != PRS_NEW &&
	    p->p_state != PRS_ZOMBIE &&
	    p->p_vmspace != NULL) {
		struct vmspace *vm = p->p_vmspace;

		kp->ki_size = vm->vm_map.size;
		kp->ki_rssize = vmspace_resident_count(vm); /*XXX*/
		if (p->p_sflag & PS_INMEM)
			kp->ki_rssize += UAREA_PAGES;
		FOREACH_THREAD_IN_PROC(p, td0) {
			if (!TD_IS_SWAPPED(td0))
				kp->ki_rssize += td0->td_kstack_pages;
			if (td0->td_altkstack_obj != NULL)
				kp->ki_rssize += td0->td_altkstack_pages;
		}
		kp->ki_swrss = vm->vm_swrss;
		kp->ki_tsize = vm->vm_tsize;
		kp->ki_dsize = vm->vm_dsize;
		kp->ki_ssize = vm->vm_ssize;
	}
	if ((p->p_sflag & PS_INMEM) && p->p_stats) {
		kp->ki_start = p->p_stats->p_start;
		timevaladd(&kp->ki_start, &boottime);
		kp->ki_rusage = p->p_stats->p_ru;
		kp->ki_childtime.tv_sec = p->p_stats->p_cru.ru_utime.tv_sec +
		    p->p_stats->p_cru.ru_stime.tv_sec;
		kp->ki_childtime.tv_usec = p->p_stats->p_cru.ru_utime.tv_usec +
		    p->p_stats->p_cru.ru_stime.tv_usec;
	}
	if (p->p_state != PRS_ZOMBIE) {
#if 0
		if (td == NULL) {
			/* XXXKSE: This should never happen. */
			printf("fill_kinfo_proc(): pid %d has no threads!\n",
			    p->p_pid);
			mtx_unlock_spin(&sched_lock);
			return;
		}
#endif
		if (td->td_wmesg != NULL) {
			strlcpy(kp->ki_wmesg, td->td_wmesg,
			    sizeof(kp->ki_wmesg));
		}
		if (TD_ON_LOCK(td)) {
			kp->ki_kiflag |= KI_LOCKBLOCK;
			strlcpy(kp->ki_lockname, td->td_lockname,
			    sizeof(kp->ki_lockname));
		}

		if (p->p_state == PRS_NORMAL) { /*  XXXKSE very approximate */
			if (TD_ON_RUNQ(td) ||
			    TD_CAN_RUN(td) ||
			    TD_IS_RUNNING(td)) {
				kp->ki_stat = SRUN;
			} else if (P_SHOULDSTOP(p)) {
				kp->ki_stat = SSTOP;
			} else if (TD_IS_SLEEPING(td)) {
				kp->ki_stat = SSLEEP;
			} else if (TD_ON_LOCK(td)) {
				kp->ki_stat = SLOCK;
			} else {
				kp->ki_stat = SWAIT;
			}
		} else {
			kp->ki_stat = SIDL;
		}

		kp->ki_sflag = p->p_sflag;
		kp->ki_swtime = p->p_swtime;
		kp->ki_pid = p->p_pid;
		kg = td->td_ksegrp;
		ke = td->td_kse;
		bintime2timeval(&p->p_runtime, &tv);
		kp->ki_runtime =
		    tv.tv_sec * (u_int64_t)1000000 + tv.tv_usec;

		/* things in the KSE GROUP */
		kp->ki_estcpu = kg->kg_estcpu;
		kp->ki_slptime = kg->kg_slptime;
		kp->ki_pri.pri_user = kg->kg_user_pri;
		kp->ki_pri.pri_class = kg->kg_pri_class;
		kp->ki_nice = kg->kg_nice;

		/* Things in the thread */
		kp->ki_wchan = td->td_wchan;
		kp->ki_pri.pri_level = td->td_priority;
		kp->ki_pri.pri_native = td->td_base_pri;
		kp->ki_lastcpu = td->td_lastcpu;
		kp->ki_oncpu = td->td_oncpu;
		kp->ki_tdflags = td->td_flags;
		kp->ki_pcb = td->td_pcb;
		kp->ki_kstack = (void *)td->td_kstack;
		kp->ki_pctcpu = sched_pctcpu(td);

		/* Things in the kse */
		if (ke)
			kp->ki_rqindex = ke->ke_rqindex;
		else
			kp->ki_rqindex = 0;

	} else {
		kp->ki_stat = SZOMB;
	}
	mtx_unlock_spin(&sched_lock);
	sp = NULL;
	tp = NULL;
	if (p->p_pgrp) {
		kp->ki_pgid = p->p_pgrp->pg_id;
		kp->ki_jobc = p->p_pgrp->pg_jobc;
		sp = p->p_pgrp->pg_session;

		if (sp != NULL) {
			kp->ki_sid = sp->s_sid;
			SESS_LOCK(sp);
			strlcpy(kp->ki_login, sp->s_login,
			    sizeof(kp->ki_login));
			if (sp->s_ttyvp)
				kp->ki_kiflag |= KI_CTTY;
			if (SESS_LEADER(p))
				kp->ki_kiflag |= KI_SLEADER;
			tp = sp->s_ttyp;
			SESS_UNLOCK(sp);
		}
	}
	if ((p->p_flag & P_CONTROLT) && tp != NULL) {
		kp->ki_tdev = dev2udev(tp->t_dev);
		kp->ki_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		if (tp->t_session)
			kp->ki_tsid = tp->t_session->s_sid;
	} else
		kp->ki_tdev = NOUDEV;
	if (p->p_comm[0] != '\0') {
		strlcpy(kp->ki_comm, p->p_comm, sizeof(kp->ki_comm));
		strlcpy(kp->ki_ocomm, p->p_comm, sizeof(kp->ki_ocomm));
	}
	kp->ki_siglist = p->p_siglist;
        SIGSETOR(kp->ki_siglist, td->td_siglist);
	kp->ki_sigmask = td->td_sigmask;
	kp->ki_xstat = p->p_xstat;
	kp->ki_acflag = p->p_acflag;
	kp->ki_flag = p->p_flag;
	/* If jailed(p->p_ucred), emulate the old P_JAILED flag. */
	if (jailed(p->p_ucred))
		kp->ki_flag |= P_JAILED;
	kp->ki_lock = p->p_lock;
	if (p->p_pptr)
		kp->ki_ppid = p->p_pptr->p_pid;
}

/*
 * Locate a zombie process by number
 */
struct proc *
zpfind(pid_t pid)
{
	struct proc *p;

	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &zombproc, p_list)
		if (p->p_pid == pid) {
			PROC_LOCK(p);
			break;
		}
	sx_sunlock(&allproc_lock);
	return (p);
}

#define KERN_PROC_ZOMBMASK	0x3
#define KERN_PROC_NOTHREADS	0x4

/*
 * Must be called with the process locked and will return with it unlocked.
 */
static int
sysctl_out_proc(struct proc *p, struct sysctl_req *req, int flags)
{
	struct thread *td;
	struct kinfo_proc kinfo_proc;
	int error = 0;
	struct proc *np;
	pid_t pid = p->p_pid;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (flags & KERN_PROC_NOTHREADS) {
		fill_kinfo_proc(p, &kinfo_proc);
		PROC_UNLOCK(p);
		error = SYSCTL_OUT(req, (caddr_t)&kinfo_proc,
				   sizeof(kinfo_proc));
		PROC_LOCK(p);
	} else {
		_PHOLD(p);
		FOREACH_THREAD_IN_PROC(p, td) {
			fill_kinfo_thread(td, &kinfo_proc);
			PROC_UNLOCK(p);
			error = SYSCTL_OUT(req, (caddr_t)&kinfo_proc,
					   sizeof(kinfo_proc));
			PROC_LOCK(p);
			if (error)
				break;
		}
		_PRELE(p);
	}
	PROC_UNLOCK(p);
	if (error)
		return (error);
	if (flags & KERN_PROC_ZOMBMASK)
		np = zpfind(pid);
	else {
		if (pid == 0)
			return (0);
		np = pfind(pid);
	}
	if (np == NULL)
		return EAGAIN;
	if (np != p) {
		PROC_UNLOCK(np);
		return EAGAIN;
	}
	PROC_UNLOCK(np);
	return (0);
}

static int
sysctl_kern_proc(SYSCTL_HANDLER_ARGS)
{
	int *name = (int*) arg1;
	u_int namelen = arg2;
	struct proc *p;
	int flags, doingzomb;
	int error = 0;

	if (oidp->oid_number == KERN_PROC_PID) {
		if (namelen != 1) 
			return (EINVAL);
		p = pfind((pid_t)name[0]);
		if (!p)
			return (ESRCH);
		if ((error = p_cansee(curthread, p))) {
			PROC_UNLOCK(p);
			return (error);
		}
		error = sysctl_out_proc(p, req, KERN_PROC_NOTHREADS);
		return (error);
	}

	switch (oidp->oid_number) {
	case KERN_PROC_ALL:
		if (namelen != 0)
			return (EINVAL);
		break;
	case KERN_PROC_PROC:
		if (namelen != 0 && namelen != 1)
			return (EINVAL);
		break;
	default:
		if (namelen != 1)
			return (EINVAL);
		break;
	}
	
	if (!req->oldptr) {
		/* overestimate by 5 procs */
		error = SYSCTL_OUT(req, 0, sizeof (struct kinfo_proc) * 5);
		if (error)
			return (error);
	}
	sysctl_wire_old_buffer(req, 0);
	sx_slock(&allproc_lock);
	for (doingzomb=0 ; doingzomb < 2 ; doingzomb++) {
		if (!doingzomb)
			p = LIST_FIRST(&allproc);
		else
			p = LIST_FIRST(&zombproc);
		for (; p != 0; p = LIST_NEXT(p, p_list)) {
			/*
			 * Skip embryonic processes.
			 */
			mtx_lock_spin(&sched_lock);
			if (p->p_state == PRS_NEW) {
				mtx_unlock_spin(&sched_lock);
				continue;
			}
			mtx_unlock_spin(&sched_lock);
			PROC_LOCK(p);
			/*
			 * Show a user only appropriate processes.
			 */
			if (p_cansee(curthread, p)) {
				PROC_UNLOCK(p);
				continue;
			}
			flags = 0;
			/*
			 * TODO - make more efficient (see notes below).
			 * do by session.
			 */
			switch (oidp->oid_number) {

			case KERN_PROC_PGRP:
				/* could do this by traversing pgrp */
				if (p->p_pgrp == NULL || 
				    p->p_pgrp->pg_id != (pid_t)name[0]) {
					PROC_UNLOCK(p);
					continue;
				}
				break;

			case KERN_PROC_TTY:
				if ((p->p_flag & P_CONTROLT) == 0 ||
				    p->p_session == NULL) {
					PROC_UNLOCK(p);
					continue;
				}
				SESS_LOCK(p->p_session);
				if (p->p_session->s_ttyp == NULL ||
				    dev2udev(p->p_session->s_ttyp->t_dev) != 
				    (udev_t)name[0]) {
					SESS_UNLOCK(p->p_session);
					PROC_UNLOCK(p);
					continue;
				}
				SESS_UNLOCK(p->p_session);
				break;

			case KERN_PROC_UID:
				if (p->p_ucred == NULL || 
				    p->p_ucred->cr_uid != (uid_t)name[0]) {
					PROC_UNLOCK(p);
					continue;
				}
				break;

			case KERN_PROC_RUID:
				if (p->p_ucred == NULL || 
				    p->p_ucred->cr_ruid != (uid_t)name[0]) {
					PROC_UNLOCK(p);
					continue;
				}
				break;

			case KERN_PROC_PROC:
				flags |= KERN_PROC_NOTHREADS;
				break;

			default:
				break;

			}

			error = sysctl_out_proc(p, req, flags | doingzomb);
			if (error) {
				sx_sunlock(&allproc_lock);
				return (error);
			}
		}
	}
	sx_sunlock(&allproc_lock);
	return (0);
}

struct pargs *
pargs_alloc(int len)
{
	struct pargs *pa;

	MALLOC(pa, struct pargs *, sizeof(struct pargs) + len, M_PARGS,
		M_WAITOK);
	pa->ar_ref = 1;
	pa->ar_length = len;
	return (pa);
}

void
pargs_free(struct pargs *pa)
{

	FREE(pa, M_PARGS);
}

void
pargs_hold(struct pargs *pa)
{

	if (pa == NULL)
		return;
	PARGS_LOCK(pa);
	pa->ar_ref++;
	PARGS_UNLOCK(pa);
}

void
pargs_drop(struct pargs *pa)
{

	if (pa == NULL)
		return;
	PARGS_LOCK(pa);
	if (--pa->ar_ref == 0) {
		PARGS_UNLOCK(pa);
		pargs_free(pa);
	} else
		PARGS_UNLOCK(pa);
}

/*
 * This sysctl allows a process to retrieve the argument list or process
 * title for another process without groping around in the address space
 * of the other process.  It also allow a process to set its own "process 
 * title to a string of its own choice.
 */
static int
sysctl_kern_proc_args(SYSCTL_HANDLER_ARGS)
{
	int *name = (int*) arg1;
	u_int namelen = arg2;
	struct pargs *newpa, *pa;
	struct proc *p;
	int error = 0;

	if (namelen != 1) 
		return (EINVAL);

	p = pfind((pid_t)name[0]);
	if (!p)
		return (ESRCH);

	if ((!ps_argsopen) && (error = p_cansee(curthread, p))) {
		PROC_UNLOCK(p);
		return (error);
	}

	if (req->newptr && curproc != p) {
		PROC_UNLOCK(p);
		return (EPERM);
	}

	pa = p->p_args;
	pargs_hold(pa);
	PROC_UNLOCK(p);
	if (req->oldptr != NULL && pa != NULL)
		error = SYSCTL_OUT(req, pa->ar_args, pa->ar_length);
	pargs_drop(pa);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (req->newlen + sizeof(struct pargs) > ps_arg_cache_limit)
		return (ENOMEM);
	newpa = pargs_alloc(req->newlen);
	error = SYSCTL_IN(req, newpa->ar_args, req->newlen);
	if (error != 0) {
		pargs_free(newpa);
		return (error);
	}
	PROC_LOCK(p);
	pa = p->p_args;
	p->p_args = newpa;
	PROC_UNLOCK(p);
	pargs_drop(pa);
	return (0);
}

static int
sysctl_kern_proc_sv_name(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;
	char *sv_name;
	int *name;
	int namelen;
	int error;

	namelen = arg2;
	if (namelen != 1) 
		return (EINVAL);

	name = (int *)arg1;
	if ((p = pfind((pid_t)name[0])) == NULL)
		return (ESRCH);
	if ((error = p_cansee(curthread, p))) {
		PROC_UNLOCK(p);
		return (error);
	}
	sv_name = p->p_sysent->sv_name;
	PROC_UNLOCK(p);
	return (sysctl_handle_string(oidp, sv_name, 0, req));
}


SYSCTL_NODE(_kern, KERN_PROC, proc, CTLFLAG_RD,  0, "Process table");

SYSCTL_PROC(_kern_proc, KERN_PROC_ALL, all, CTLFLAG_RD|CTLTYPE_STRUCT,
	0, 0, sysctl_kern_proc, "S,proc", "Return entire process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_PGRP, pgrp, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_TTY, tty, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_UID, uid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_RUID, ruid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_PID, pid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_PROC, proc, CTLFLAG_RD,
	sysctl_kern_proc, "Return process table, no threads");

SYSCTL_NODE(_kern_proc, KERN_PROC_ARGS, args, CTLFLAG_RW | CTLFLAG_ANYBODY,
	sysctl_kern_proc_args, "Process argument list");

SYSCTL_NODE(_kern_proc, KERN_PROC_SV_NAME, sv_name, CTLFLAG_RD,
	sysctl_kern_proc_sv_name, "Process syscall vector name (ABI type)");
