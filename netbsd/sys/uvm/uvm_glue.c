/*	$NetBSD: uvm_glue.c,v 1.58 2002/05/15 06:57:49 matt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and
 *      its contributors.
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
 *	@(#)vm_glue.c	8.6 (Berkeley) 1/5/94
 * from: Id: uvm_glue.c,v 1.1.2.8 1998/02/07 01:16:54 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_glue.c,v 1.58 2002/05/15 06:57:49 matt Exp $");

#include "opt_kgdb.h"
#include "opt_sysv.h"
#include "opt_uvmhist.h"

/*
 * uvm_glue.c: glue functions
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/buf.h>
#include <sys/user.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <uvm/uvm.h>

#include <machine/cpu.h>

/*
 * local prototypes
 */

static void uvm_swapout __P((struct proc *));

/*
 * XXXCDC: do these really belong here?
 */

int readbuffers = 0;		/* allow KGDB to read kern buffer pool */
				/* XXX: see uvm_kernacc */


/*
 * uvm_kernacc: can the kernel access a region of memory
 *
 * - called from malloc [DIAGNOSTIC], and /dev/kmem driver (mem.c)
 */

boolean_t
uvm_kernacc(addr, len, rw)
	caddr_t addr;
	size_t len;
	int rw;
{
	boolean_t rv;
	vaddr_t saddr, eaddr;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

	saddr = trunc_page((vaddr_t)addr);
	eaddr = round_page((vaddr_t)addr + len);
	vm_map_lock_read(kernel_map);
	rv = uvm_map_checkprot(kernel_map, saddr, eaddr, prot);
	vm_map_unlock_read(kernel_map);

	/*
	 * XXX there are still some things (e.g. the buffer cache) that
	 * are managed behind the VM system's back so even though an
	 * address is accessible in the mind of the VM system, there may
	 * not be physical pages where the VM thinks there is.  This can
	 * lead to bogus allocation of pages in the kernel address space
	 * or worse, inconsistencies at the pmap level.  We only worry
	 * about the buffer cache for now.
	 */
	if (!readbuffers && rv && (eaddr > (vaddr_t)buffers &&
			     saddr < (vaddr_t)buffers + MAXBSIZE * nbuf))
		rv = FALSE;
	return(rv);
}

/*
 * uvm_useracc: can the user access it?
 *
 * - called from physio() and sys___sysctl().
 */

boolean_t
uvm_useracc(addr, len, rw)
	caddr_t addr;
	size_t len;
	int rw;
{
	struct vm_map *map;
	boolean_t rv;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

	/* XXX curproc */
	map = &curproc->p_vmspace->vm_map;

	vm_map_lock_read(map);
	rv = uvm_map_checkprot(map, trunc_page((vaddr_t)addr),
	    round_page((vaddr_t)addr + len), prot);
	vm_map_unlock_read(map);

	return(rv);
}

#ifdef KGDB
/*
 * Change protections on kernel pages from addr to addr+len
 * (presumably so debugger can plant a breakpoint).
 *
 * We force the protection change at the pmap level.  If we were
 * to use vm_map_protect a change to allow writing would be lazily-
 * applied meaning we would still take a protection fault, something
 * we really don't want to do.  It would also fragment the kernel
 * map unnecessarily.  We cannot use pmap_protect since it also won't
 * enforce a write-enable request.  Using pmap_enter is the only way
 * we can ensure the change takes place properly.
 */
void
uvm_chgkprot(addr, len, rw)
	caddr_t addr;
	size_t len;
	int rw;
{
	vm_prot_t prot;
	paddr_t pa;
	vaddr_t sva, eva;

	prot = rw == B_READ ? VM_PROT_READ : VM_PROT_READ|VM_PROT_WRITE;
	eva = round_page((vaddr_t)addr + len);
	for (sva = trunc_page((vaddr_t)addr); sva < eva; sva += PAGE_SIZE) {
		/*
		 * Extract physical address for the page.
		 */
		if (pmap_extract(pmap_kernel(), sva, &pa) == FALSE)
			panic("chgkprot: invalid page");
		pmap_enter(pmap_kernel(), sva, pa, prot, PMAP_WIRED);
	}
	pmap_update(pmap_kernel());
}
#endif

/*
 * uvm_vslock: wire user memory for I/O
 *
 * - called from physio and sys___sysctl
 * - XXXCDC: consider nuking this (or making it a macro?)
 */

int
uvm_vslock(p, addr, len, access_type)
	struct proc *p;
	caddr_t	addr;
	size_t	len;
	vm_prot_t access_type;
{
	struct vm_map *map;
	vaddr_t start, end;
	int error;

	map = &p->p_vmspace->vm_map;
	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	error = uvm_fault_wire(map, start, end, VM_FAULT_WIRE, access_type);
	return error;
}

/*
 * uvm_vsunlock: unwire user memory wired by uvm_vslock()
 *
 * - called from physio and sys___sysctl
 * - XXXCDC: consider nuking this (or making it a macro?)
 */

void
uvm_vsunlock(p, addr, len)
	struct proc *p;
	caddr_t	addr;
	size_t	len;
{
	uvm_fault_unwire(&p->p_vmspace->vm_map, trunc_page((vaddr_t)addr),
		round_page((vaddr_t)addr + len));
}

/*
 * uvm_fork: fork a virtual address space
 *
 * - the address space is copied as per parent map's inherit values
 * - a new "user" structure is allocated for the child process
 *	[filled in by MD layer...]
 * - if specified, the child gets a new user stack described by
 *	stack and stacksize
 * - NOTE: the kernel stack may be at a different location in the child
 *	process, and thus addresses of automatic variables may be invalid
 *	after cpu_fork returns in the child process.  We do nothing here
 *	after cpu_fork returns.
 * - XXXCDC: we need a way for this to return a failure value rather
 *   than just hang
 */
void
uvm_fork(p1, p2, shared, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	boolean_t shared;
	void *stack;
	size_t stacksize;
	void (*func) __P((void *));
	void *arg;
{
	struct user *up = p2->p_addr;
	int error;

	if (shared == TRUE) {
		p2->p_vmspace = NULL;
		uvmspace_share(p1, p2);
	} else
		p2->p_vmspace = uvmspace_fork(p1->p_vmspace);

	/*
	 * Wire down the U-area for the process, which contains the PCB
	 * and the kernel stack.  Wired state is stored in p->p_flag's
	 * P_INMEM bit rather than in the vm_map_entry's wired count
	 * to prevent kernel_map fragmentation.
	 *
	 * Note the kernel stack gets read/write accesses right off
	 * the bat.
	 */
	error = uvm_fault_wire(kernel_map, (vaddr_t)up, (vaddr_t)up + USPACE,
	    VM_FAULT_WIRE, VM_PROT_READ | VM_PROT_WRITE);
	if (error)
		panic("uvm_fork: uvm_fault_wire failed: %d", error);

	/*
	 * p_stats currently points at a field in the user struct.  Copy
	 * parts of p_stats, and zero out the rest.
	 */
	p2->p_stats = &up->u_stats;
	memset(&up->u_stats.pstat_startzero, 0,
	       ((caddr_t)&up->u_stats.pstat_endzero -
		(caddr_t)&up->u_stats.pstat_startzero));
	memcpy(&up->u_stats.pstat_startcopy, &p1->p_stats->pstat_startcopy,
	       ((caddr_t)&up->u_stats.pstat_endcopy -
		(caddr_t)&up->u_stats.pstat_startcopy));

	/*
	 * cpu_fork() copy and update the pcb, and make the child ready
	 * to run.  If this is a normal user fork, the child will exit
	 * directly to user mode via child_return() on its first time
	 * slice and will not return here.  If this is a kernel thread,
	 * the specified entry point will be executed.
	 */
	cpu_fork(p1, p2, stack, stacksize, func, arg);
}

/*
 * uvm_exit: exit a virtual address space
 *
 * - the process passed to us is a dead (pre-zombie) process; we
 *   are running on a different context now (the reaper).
 * - we must run in a separate thread because freeing the vmspace
 *   of the dead process may block.
 */
void
uvm_exit(p)
	struct proc *p;
{
	vaddr_t va = (vaddr_t)p->p_addr;

	uvmspace_free(p->p_vmspace);
	p->p_flag &= ~P_INMEM;
	uvm_km_free(kernel_map, va, USPACE);
	p->p_addr = NULL;
}

/*
 * uvm_init_limit: init per-process VM limits
 *
 * - called for process 0 and then inherited by all others.
 */
void
uvm_init_limits(p)
	struct proc *p;
{

	/*
	 * Set up the initial limits on process VM.  Set the maximum
	 * resident set size to be all of (reasonably) available memory.
	 * This causes any single, large process to start random page
	 * replacement once it fills memory.
	 */

	p->p_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
	p->p_rlimit[RLIMIT_STACK].rlim_max = MAXSSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_cur = DFLDSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_max = MAXDSIZ;
	p->p_rlimit[RLIMIT_RSS].rlim_cur = ptoa(uvmexp.free);
}

#ifdef DEBUG
int	enableswap = 1;
int	swapdebug = 0;
#define	SDB_FOLLOW	1
#define SDB_SWAPIN	2
#define SDB_SWAPOUT	4
#endif

/*
 * uvm_swapin: swap in a process's u-area.
 */

void
uvm_swapin(p)
	struct proc *p;
{
	vaddr_t addr;
	int s, error;

	addr = (vaddr_t)p->p_addr;
	/* make P_INMEM true */
	error = uvm_fault_wire(kernel_map, addr, addr + USPACE, VM_FAULT_WIRE,
	    VM_PROT_READ | VM_PROT_WRITE);
	if (error) {
		panic("uvm_swapin: rewiring stack failed: %d", error);
	}

	/*
	 * Some architectures need to be notified when the user area has
	 * moved to new physical page(s) (e.g.  see mips/mips/vm_machdep.c).
	 */
	cpu_swapin(p);
	SCHED_LOCK(s);
	if (p->p_stat == SRUN)
		setrunqueue(p);
	p->p_flag |= P_INMEM;
	SCHED_UNLOCK(s);
	p->p_swtime = 0;
	++uvmexp.swapins;
}

/*
 * uvm_scheduler: process zero main loop
 *
 * - attempt to swapin every swaped-out, runnable process in order of
 *	priority.
 * - if not enough memory, wake the pagedaemon and let it clear space.
 */

void
uvm_scheduler()
{
	struct proc *p;
	int pri;
	struct proc *pp;
	int ppri;

loop:
#ifdef DEBUG
	while (!enableswap)
		tsleep(&proc0, PVM, "noswap", 0);
#endif
	pp = NULL;		/* process to choose */
	ppri = INT_MIN;	/* its priority */
	proclist_lock_read();
	LIST_FOREACH(p, &allproc, p_list) {

		/* is it a runnable swapped out process? */
		if (p->p_stat == SRUN && (p->p_flag & P_INMEM) == 0) {
			pri = p->p_swtime + p->p_slptime -
			    (p->p_nice - NZERO) * 8;
			if (pri > ppri) {   /* higher priority?  remember it. */
				pp = p;
				ppri = pri;
			}
		}
	}
	/*
	 * XXXSMP: possible unlock/sleep race between here and the
	 * "scheduler" tsleep below..
	 */
	proclist_unlock_read();

#ifdef DEBUG
	if (swapdebug & SDB_FOLLOW)
		printf("scheduler: running, procp %p pri %d\n", pp, ppri);
#endif
	/*
	 * Nothing to do, back to sleep
	 */
	if ((p = pp) == NULL) {
		tsleep(&proc0, PVM, "scheduler", 0);
		goto loop;
	}

	/*
	 * we have found swapped out process which we would like to bring
	 * back in.
	 *
	 * XXX: this part is really bogus cuz we could deadlock on memory
	 * despite our feeble check
	 */
	if (uvmexp.free > atop(USPACE)) {
#ifdef DEBUG
		if (swapdebug & SDB_SWAPIN)
			printf("swapin: pid %d(%s)@%p, pri %d free %d\n",
	     p->p_pid, p->p_comm, p->p_addr, ppri, uvmexp.free);
#endif
		uvm_swapin(p);
		goto loop;
	}
	/*
	 * not enough memory, jab the pageout daemon and wait til the coast
	 * is clear
	 */
#ifdef DEBUG
	if (swapdebug & SDB_FOLLOW)
		printf("scheduler: no room for pid %d(%s), free %d\n",
	   p->p_pid, p->p_comm, uvmexp.free);
#endif
	uvm_wait("schedpwait");
#ifdef DEBUG
	if (swapdebug & SDB_FOLLOW)
		printf("scheduler: room again, free %d\n", uvmexp.free);
#endif
	goto loop;
}

/*
 * swappable: is process "p" swappable?
 */

#define	swappable(p)							\
	(((p)->p_flag & (P_SYSTEM | P_INMEM | P_WEXIT)) == P_INMEM &&	\
	 (p)->p_holdcnt == 0)

/*
 * swapout_threads: find threads that can be swapped and unwire their
 *	u-areas.
 *
 * - called by the pagedaemon
 * - try and swap at least one processs
 * - processes that are sleeping or stopped for maxslp or more seconds
 *   are swapped... otherwise the longest-sleeping or stopped process
 *   is swapped, otherwise the longest resident process...
 */
void
uvm_swapout_threads()
{
	struct proc *p;
	struct proc *outp, *outp2;
	int outpri, outpri2;
	int didswap = 0;
	extern int maxslp;
	/* XXXCDC: should move off to uvmexp. or uvm., also in uvm_meter */

#ifdef DEBUG
	if (!enableswap)
		return;
#endif

	/*
	 * outp/outpri  : stop/sleep process with largest sleeptime < maxslp
	 * outp2/outpri2: the longest resident process (its swap time)
	 */
	outp = outp2 = NULL;
	outpri = outpri2 = 0;
	proclist_lock_read();
	LIST_FOREACH(p, &allproc, p_list) {
		if (!swappable(p))
			continue;
		switch (p->p_stat) {
		case SRUN:
		case SONPROC:
			if (p->p_swtime > outpri2) {
				outp2 = p;
				outpri2 = p->p_swtime;
			}
			continue;

		case SSLEEP:
		case SSTOP:
			if (p->p_slptime >= maxslp) {
				uvm_swapout(p);
				didswap++;
			} else if (p->p_slptime > outpri) {
				outp = p;
				outpri = p->p_slptime;
			}
			continue;
		}
	}
	proclist_unlock_read();

	/*
	 * If we didn't get rid of any real duds, toss out the next most
	 * likely sleeping/stopped or running candidate.  We only do this
	 * if we are real low on memory since we don't gain much by doing
	 * it (USPACE bytes).
	 */
	if (didswap == 0 && uvmexp.free <= atop(round_page(USPACE))) {
		if ((p = outp) == NULL)
			p = outp2;
#ifdef DEBUG
		if (swapdebug & SDB_SWAPOUT)
			printf("swapout_threads: no duds, try procp %p\n", p);
#endif
		if (p)
			uvm_swapout(p);
	}
}

/*
 * uvm_swapout: swap out process "p"
 *
 * - currently "swapout" means "unwire U-area" and "pmap_collect()"
 *   the pmap.
 * - XXXCDC: should deactivate all process' private anonymous memory
 */

static void
uvm_swapout(p)
	struct proc *p;
{
	vaddr_t addr;
	int s;

#ifdef DEBUG
	if (swapdebug & SDB_SWAPOUT)
		printf("swapout: pid %d(%s)@%p, stat %x pri %d free %d\n",
	   p->p_pid, p->p_comm, p->p_addr, p->p_stat,
	   p->p_slptime, uvmexp.free);
#endif

	/*
	 * Do any machine-specific actions necessary before swapout.
	 * This can include saving floating point state, etc.
	 */
	cpu_swapout(p);

	/*
	 * Mark it as (potentially) swapped out.
	 */
	SCHED_LOCK(s);
	p->p_flag &= ~P_INMEM;
	if (p->p_stat == SRUN)
		remrunqueue(p);
	SCHED_UNLOCK(s);
	p->p_swtime = 0;
	p->p_stats->p_ru.ru_nswap++;
	++uvmexp.swapouts;

	/*
	 * Unwire the to-be-swapped process's user struct and kernel stack.
	 */
	addr = (vaddr_t)p->p_addr;
	uvm_fault_unwire(kernel_map, addr, addr + USPACE); /* !P_INMEM */
	pmap_collect(vm_map_pmap(&p->p_vmspace->vm_map));
}

/*
 * uvm_coredump_walkmap: walk a process's map for the purpose of dumping
 * a core file.
 */

int
uvm_coredump_walkmap(p, vp, cred, func, cookie)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	int (*func)(struct proc *, struct vnode *, struct ucred *,
	    struct uvm_coredump_state *);
	void *cookie;
{
	struct uvm_coredump_state state;
	struct vmspace *vm = p->p_vmspace;
	struct vm_map *map = &vm->vm_map;
	struct vm_map_entry *entry;
	vaddr_t maxstack;
	int error;

	maxstack = trunc_page(USRSTACK - ctob(vm->vm_ssize));

	for (entry = map->header.next; entry != &map->header;
	     entry = entry->next) {  
		/* Should never happen for a user process. */
		if (UVM_ET_ISSUBMAP(entry))
			panic("uvm_coredump_walkmap: user process with "
			    "submap?");

		state.cookie = cookie;
		state.start = entry->start;
		state.end = entry->end;
		state.prot = entry->protection;
		state.flags = 0;

		if (state.start >= VM_MAXUSER_ADDRESS)  
			continue;

		if (state.end > VM_MAXUSER_ADDRESS)
			state.end = VM_MAXUSER_ADDRESS;

		if (state.start >= (vaddr_t)vm->vm_maxsaddr) {
			if (state.end <= maxstack)
				continue;
			if (state.start < maxstack)
				state.start = maxstack;
			state.flags |= UVM_COREDUMP_STACK;
		}

		if ((entry->protection & VM_PROT_WRITE) == 0)
			state.flags |= UVM_COREDUMP_NODUMP;

		if (entry->object.uvm_obj != NULL &&
		    entry->object.uvm_obj->pgops == &uvm_deviceops)
			state.flags |= UVM_COREDUMP_NODUMP;

		error = (*func)(p, vp, cred, &state);
		if (error)
			return (error);
	}

	return (0);
}
