/* $NetBSD: trap.c,v 1.77.16.1 2002/06/10 16:22:34 tv Exp $ */

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Charles M. Hannum, and by Ross Harvey.
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

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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

#include "opt_fix_unaligned_vax_fp.h"
#include "opt_ddb.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: trap.c,v 1.77.16.1 2002/06/10 16:22:34 tv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <uvm/uvm_extern.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/buf.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/alpha.h>
#include <machine/rpb.h>
#ifdef DDB
#include <machine/db_machdep.h>
#endif
#include <alpha/alpha/db_instruction.h>
#include <machine/userret.h>

static int unaligned_fixup(u_long, u_long, u_long, struct proc *);
static int handle_opdec(struct proc *p, u_int64_t *ucodep);

struct evcnt fpevent_use;
struct evcnt fpevent_reuse;

/*
 * Initialize the trap vectors for the current processor.
 */
void
trap_init(void)
{

	/*
	 * Point interrupt/exception vectors to our own.
	 */
	alpha_pal_wrent(XentInt, ALPHA_KENTRY_INT); 
	alpha_pal_wrent(XentArith, ALPHA_KENTRY_ARITH);
	alpha_pal_wrent(XentMM, ALPHA_KENTRY_MM);
	alpha_pal_wrent(XentIF, ALPHA_KENTRY_IF);
	alpha_pal_wrent(XentUna, ALPHA_KENTRY_UNA); 
	alpha_pal_wrent(XentSys, ALPHA_KENTRY_SYS);

	/*
	 * Clear pending machine checks and error reports, and enable
	 * system- and processor-correctable error reporting.
	 */
	alpha_pal_wrmces(alpha_pal_rdmces() & 
	    ~(ALPHA_MCES_DSC|ALPHA_MCES_DPC));

	/*
	 * If this is the primary processor, initialize some trap
	 * event counters.
	 */
	if (cpu_number() == hwrpb->rpb_primary_cpu_id) {
		evcnt_attach_dynamic(&fpevent_use, EVCNT_TYPE_MISC, NULL,
		    "FP", "proc use");
		evcnt_attach_dynamic(&fpevent_reuse, EVCNT_TYPE_MISC, NULL,
		    "FP", "proc re-use");
	}
}

static void
printtrap(const u_long a0, const u_long a1, const u_long a2,
    const u_long entry, struct trapframe *framep, int isfatal, int user)
{
	char ubuf[64];
	const char *entryname;
	u_long cpu_id = cpu_number();

	switch (entry) {
	case ALPHA_KENTRY_INT:
		entryname = "interrupt";
		break;
	case ALPHA_KENTRY_ARITH:
		entryname = "arithmetic trap";
		break;
	case ALPHA_KENTRY_MM:
		entryname = "memory management fault";
		break;
	case ALPHA_KENTRY_IF:
		entryname = "instruction fault";
		break;
	case ALPHA_KENTRY_UNA:
		entryname = "unaligned access fault";
		break;
	case ALPHA_KENTRY_SYS:
		entryname = "system call";
		break;
	default:
		sprintf(ubuf, "type %lx", entry);
		entryname = (const char *) ubuf;
		break;
	}

	printf("\n");
	printf("CPU %lu: %s %s trap:\n", cpu_id, isfatal ? "fatal" : "handled",
	    user ? "user" : "kernel");
	printf("\n");
	printf("CPU %lu    trap entry = 0x%lx (%s)\n", cpu_id, entry,
	    entryname);
	printf("CPU %lu    a0         = 0x%lx\n", cpu_id, a0);
	printf("CPU %lu    a1         = 0x%lx\n", cpu_id, a1);
	printf("CPU %lu    a2         = 0x%lx\n", cpu_id, a2);
	printf("CPU %lu    pc         = 0x%lx\n", cpu_id,
	    framep->tf_regs[FRAME_PC]);
	printf("CPU %lu    ra         = 0x%lx\n", cpu_id,
	    framep->tf_regs[FRAME_RA]);
	printf("CPU %lu    pv         = 0x%lx\n", cpu_id,
	    framep->tf_regs[FRAME_T12]);
	printf("CPU %lu    curproc    = %p\n", cpu_id, curproc);
	if (curproc != NULL)
		printf("CPU %lu        pid = %d, comm = %s\n", cpu_id,
		    curproc->p_pid, curproc->p_comm);
	printf("\n");
}

/*
 * Trap is called from locore to handle most types of processor traps.
 * System calls are broken out for efficiency and ASTs are broken out
 * to make the code a bit cleaner and more representative of the
 * Alpha architecture.
 */
/*ARGSUSED*/
void
trap(const u_long a0, const u_long a1, const u_long a2, const u_long entry,
    struct trapframe *framep)
{
	register struct proc *p;
	register int i;
	u_int64_t ucode;
	int user;
#if defined(DDB)
	int call_debugger = 1;
#endif

	p = curproc;

	uvmexp.traps++;			/* XXXSMP: NOT ATOMIC */
	ucode = 0;
	user = (framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) != 0;
	if (user)
		p->p_md.md_tf = framep;

	switch (entry) {
	case ALPHA_KENTRY_UNA:
		/*
		 * If user-land, do whatever fixups, printing, and
		 * signalling is appropriate (based on system-wide
		 * and per-process unaligned-access-handling flags).
		 */
		if (user) {
			KERNEL_PROC_LOCK(p);
			i = unaligned_fixup(a0, a1, a2, p);
			KERNEL_PROC_UNLOCK(p);
			if (i == 0)
				goto out;

			ucode = a0;		/* VA */
			break;
		}

		/*
		 * Unaligned access from kernel mode is always an error,
		 * EVEN IF A COPY FAULT HANDLER IS SET!
		 *
		 * It's an error if a copy fault handler is set because
		 * the various routines which do user-initiated copies
		 * do so in a memcpy-like manner.  In other words, the
		 * kernel never assumes that pointers provided by the
		 * user are properly aligned, and so if the kernel
		 * does cause an unaligned access it's a kernel bug.
		 */
		goto dopanic;

	case ALPHA_KENTRY_ARITH:
		/*
		 * Resolve trap shadows, interpret FP ops requiring infinities,
		 * NaNs, or denorms, and maintain FPCR corrections.
		 */
		if (user) {
			i = alpha_fp_complete(a0, a1, p, &ucode);
			if (i == 0)
				goto out;
			break;
		}

		/* Always fatal in kernel.  Should never happen. */
		goto dopanic;

	case ALPHA_KENTRY_IF:
		/*
		 * These are always fatal in kernel, and should never
		 * happen.  (Debugger entry is handled in XentIF.)
		 */
		if (user == 0) {
#if defined(DDB)
			/*
			 * ...unless a debugger is configured.  It will
			 * inform us if the trap was handled.
			 */
			if (alpha_debug(a0, a1, a2, entry, framep))
				goto out;

			/*
			 * Debugger did NOT handle the trap, don't
			 * call the debugger again!
			 */
			call_debugger = 0;
#endif
			goto dopanic;
		}
		i = 0;
		switch (a0) {
		case ALPHA_IF_CODE_GENTRAP:
			if (framep->tf_regs[FRAME_A0] == -2) { /* weird! */
				i = SIGFPE;
				ucode =  a0;	/* exception summary */
				break;
			}
			/* FALLTHROUTH */
		case ALPHA_IF_CODE_BPT:
		case ALPHA_IF_CODE_BUGCHK:
			ucode = a0;		/* trap type */
			i = SIGTRAP;
			break;

		case ALPHA_IF_CODE_OPDEC:
			KERNEL_PROC_LOCK(p);
			i = handle_opdec(p, &ucode);
			KERNEL_PROC_UNLOCK(p);
			if (i == 0)
				goto out;
			break;

		case ALPHA_IF_CODE_FEN:
			alpha_enable_fp(p, 0);
			alpha_pal_wrfen(0);
			goto out;

		default:
			printf("trap: unknown IF type 0x%lx\n", a0);
			goto dopanic;
		}
		break;

	case ALPHA_KENTRY_MM:
		switch (a1) {
		case ALPHA_MMCSR_FOR:
		case ALPHA_MMCSR_FOE:
		case ALPHA_MMCSR_FOW:
			if (user)
				KERNEL_PROC_LOCK(p);
			else
				KERNEL_LOCK(LK_CANRECURSE|LK_EXCLUSIVE);

			pmap_emulate_reference(p, a0, user,
			    a1 == ALPHA_MMCSR_FOW ? 1 : 0);

			if (user)
				KERNEL_PROC_UNLOCK(p);
			else
				KERNEL_UNLOCK();
			goto out;

		case ALPHA_MMCSR_INVALTRANS:
		case ALPHA_MMCSR_ACCESS:
	    	{
			register vaddr_t va;
			register struct vmspace *vm = NULL;
			register struct vm_map *map;
			vm_prot_t ftype;
			int rv;

			if (user)
				KERNEL_PROC_LOCK(p);
			else {
				struct cpu_info *ci = curcpu();

				if (p == NULL) {
					/*
					 * If there is no current process,
					 * it can be nothing but a fatal
					 * error (i.e. memory in this case
					 * must be wired).
					 */
					goto dopanic;
				}

				/*
				 * If it was caused by fuswintr or suswintr,
				 * just punt.  Note that we check the faulting
				 * address against the address accessed by
				 * [fs]uswintr, in case another fault happens
				 * when they are running.
				 */
				if (p->p_addr->u_pcb.pcb_onfault ==
					(unsigned long)fswintrberr &&
				    p->p_addr->u_pcb.pcb_accessaddr == a0) {
					framep->tf_regs[FRAME_PC] =
					    p->p_addr->u_pcb.pcb_onfault;
					p->p_addr->u_pcb.pcb_onfault = 0;
					goto out;
				}

				/*
				 * If we're in interrupt context at this
				 * point, this is an error.
				 */
				if (ci->ci_intrdepth != 0)
					goto dopanic;

				KERNEL_LOCK(LK_CANRECURSE|LK_EXCLUSIVE);
			}

			/*
			 * It is only a kernel address space fault iff:
			 *	1. !user and
			 *	2. pcb_onfault not set or
			 *	3. pcb_onfault set but kernel space data fault
			 * The last can occur during an exec() copyin where the
			 * argument space is lazy-allocated.
			 */
			if (user == 0 && (a0 >= VM_MIN_KERNEL_ADDRESS ||
			    p->p_addr->u_pcb.pcb_onfault == 0))
				map = kernel_map;
			else {
				vm = p->p_vmspace;
				map = &vm->vm_map;
			}
	
			switch (a2) {
			case -1:		/* instruction fetch fault */
			case 0:			/* load instruction */
				ftype = VM_PROT_READ;
				break;
			case 1:			/* store instruction */
				ftype = VM_PROT_WRITE;
				break;
#ifdef DIAGNOSTIC
			default:		/* XXX gcc -Wuninitialized */
				if (user)
					KERNEL_PROC_UNLOCK(p);
				else
					KERNEL_UNLOCK();
				goto dopanic;
#endif
			}
	
			va = trunc_page((vaddr_t)a0);
			rv = uvm_fault(map, va,
			    (a1 == ALPHA_MMCSR_INVALTRANS) ?
			    VM_FAULT_INVALID : VM_FAULT_PROTECT, ftype);
			/*
			 * If this was a stack access we keep track of the
			 * maximum accessed stack size.  Also, if vm_fault
			 * gets a protection failure it is due to accessing
			 * the stack region outside the current limit and
			 * we need to reflect that as an access error.
			 */
			if (map != kernel_map &&
			    (caddr_t)va >= vm->vm_maxsaddr &&
			    va < USRSTACK) {
				if (rv == 0) {
					unsigned nss;
	
					nss = btoc(USRSTACK -
					    (unsigned long)va);
					if (nss > vm->vm_ssize)
						vm->vm_ssize = nss;
				} else if (rv == EACCES)
					rv = EFAULT;
			}
			if (rv == 0) {
				if (user)
					KERNEL_PROC_UNLOCK(p);
				else
					KERNEL_UNLOCK();
				goto out;
			}

			if (user == 0) {
				KERNEL_UNLOCK();

				/* Check for copyin/copyout fault */
				if (p != NULL &&
				    p->p_addr->u_pcb.pcb_onfault != 0) {
					framep->tf_regs[FRAME_PC] =
					    p->p_addr->u_pcb.pcb_onfault;
					p->p_addr->u_pcb.pcb_onfault = 0;
					goto out;
				}
				goto dopanic;
			}
			ucode = a0;
			if (rv == ENOMEM) {
				printf("UVM: pid %d (%s), uid %d killed: "
				       "out of swap\n", p->p_pid, p->p_comm,
				       p->p_cred && p->p_ucred ?
				       p->p_ucred->cr_uid : -1);
				i = SIGKILL;
			} else
				i = SIGSEGV;
			KERNEL_PROC_UNLOCK(p);
			break;
		    }

		default:
			printf("trap: unknown MMCSR value 0x%lx\n", a1);
			goto dopanic;
		}
		break;

	default:
		goto dopanic;
	}

#ifdef DEBUG
	printtrap(a0, a1, a2, entry, framep, 1, user);
#endif
	KERNEL_PROC_LOCK(p);
	trapsignal(p, i, ucode);
	KERNEL_PROC_UNLOCK(p);
out:
	if (user)
		userret(p);
	return;

dopanic:
	printtrap(a0, a1, a2, entry, framep, 1, user);

	/* XXX dump registers */

#if defined(DDB)
	if (call_debugger && alpha_debug(a0, a1, a2, entry, framep)) {
		/*
		 * The debugger has handled the trap; just return.
		 */
		goto out;
	}
#endif

	panic("trap");
}

/*
 * Set the float-point enable for the current process, and return
 * the FPU context to the named process. If check == 0, it is an
 * error for the named process to already be fpcurproc.
 */
void
alpha_enable_fp(struct proc *p, int check)
{
#if defined(MULTIPROCESSOR)
	int s;
#endif
	struct cpu_info *ci = curcpu();

	if (check && ci->ci_fpcurproc == p) {
		alpha_pal_wrfen(1);
		return;
	}
	if (ci->ci_fpcurproc == p)
		panic("trap: fp disabled for fpcurproc == %p", p);

	if (ci->ci_fpcurproc != NULL)
		fpusave_cpu(ci, 1);

	KDASSERT(ci->ci_fpcurproc == NULL);

#if defined(MULTIPROCESSOR)
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 1);
#else
	KDASSERT(p->p_addr->u_pcb.pcb_fpcpu == NULL);
#endif

	FPCPU_LOCK(&p->p_addr->u_pcb, s);

	p->p_addr->u_pcb.pcb_fpcpu = ci;
	ci->ci_fpcurproc = p;

	FPCPU_UNLOCK(&p->p_addr->u_pcb, s);

	/*
	 * Instrument FP usage -- if a process had not previously
	 * used FP, mark it as having used FP for the first time,
	 * and count this event.
	 *
	 * If a process has used FP, count a "used FP, and took
	 * a trap to use it again" event.
	 */
	if ((p->p_md.md_flags & MDP_FPUSED) == 0) {
		atomic_add_ulong(&fpevent_use.ev_count, 1);
		p->p_md.md_flags |= MDP_FPUSED;
	} else
		atomic_add_ulong(&fpevent_reuse.ev_count, 1);

	alpha_pal_wrfen(1);
	restorefpstate(&p->p_addr->u_pcb.pcb_fp);
}

/*
 * Process an asynchronous software trap.
 * This is relatively easy.
 */
void
ast(struct trapframe *framep)
{
	register struct proc *p;

	/*
	 * We may not have a current process to do AST processing
	 * on.  This happens on multiprocessor systems in which
	 * at least one CPU simply has no current process to run,
	 * but roundrobin() (called via hardclock()) kicks us to
	 * attempt to preempt the process running on our CPU.
	 */
	p = curproc;
	if (p == NULL)
		return;

	KERNEL_PROC_LOCK(p);

	uvmexp.softs++;
	p->p_md.md_tf = framep;

	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}

	if (curcpu()->ci_want_resched) {
		/*
		 * We are being preempted.
		 */
		preempt(NULL);
	}

	KERNEL_PROC_UNLOCK(p);
	userret(p);
}

/*
 * Unaligned access handler.  It's not clear that this can get much slower...
 *
 */
static const int reg_to_framereg[32] = {
	FRAME_V0,	FRAME_T0,	FRAME_T1,	FRAME_T2,
	FRAME_T3,	FRAME_T4,	FRAME_T5,	FRAME_T6,
	FRAME_T7,	FRAME_S0,	FRAME_S1,	FRAME_S2,
	FRAME_S3,	FRAME_S4,	FRAME_S5,	FRAME_S6,
	FRAME_A0,	FRAME_A1,	FRAME_A2,	FRAME_A3,
	FRAME_A4,	FRAME_A5,	FRAME_T8,	FRAME_T9,
	FRAME_T10,	FRAME_T11,	FRAME_RA,	FRAME_T12,
	FRAME_AT,	FRAME_GP,	FRAME_SP,	-1,
};

#define	irp(p, reg)							\
	((reg_to_framereg[(reg)] == -1) ? NULL :			\
	    &(p)->p_md.md_tf->tf_regs[reg_to_framereg[(reg)]])

#define	frp(p, reg)							\
	(&(p)->p_addr->u_pcb.pcb_fp.fpr_regs[(reg)])

#define	dump_fp_regs()							\
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)				\
		fpusave_proc(p, 1)

#define	unaligned_load(storage, ptrf, mod)				\
	if (copyin((caddr_t)va, &(storage), sizeof (storage)) != 0)	\
		break;							\
	signal = 0;							\
	if ((regptr = ptrf(p, reg)) != NULL)				\
		*regptr = mod (storage);

#define	unaligned_store(storage, ptrf, mod)				\
	if ((regptr = ptrf(p, reg)) != NULL)				\
		(storage) = mod (*regptr);				\
	else								\
		(storage) = 0;						\
	if (copyout(&(storage), (caddr_t)va, sizeof (storage)) != 0)	\
		break;							\
	signal = 0;

#define	unaligned_load_integer(storage)					\
	unaligned_load(storage, irp, )

#define	unaligned_store_integer(storage)				\
	unaligned_store(storage, irp, )

#define	unaligned_load_floating(storage, mod)				\
	dump_fp_regs();							\
	unaligned_load(storage, frp, mod)

#define	unaligned_store_floating(storage, mod)				\
	dump_fp_regs();							\
	unaligned_store(storage, frp, mod)

static unsigned long
Sfloat_to_reg(u_int s)
{
	unsigned long sign, expn, frac;
	unsigned long result;

	sign = (s & 0x80000000) >> 31;
	expn = (s & 0x7f800000) >> 23;
	frac = (s & 0x007fffff) >>  0;

	/* map exponent part, as appropriate. */
	if (expn == 0xff)
		expn = 0x7ff;
	else if ((expn & 0x80) != 0)
		expn = (0x400 | (expn & ~0x80));
	else if ((expn & 0x80) == 0 && expn != 0)
		expn = (0x380 | (expn & ~0x80));

	result = (sign << 63) | (expn << 52) | (frac << 29);
	return (result);
}

static unsigned int
reg_to_Sfloat(u_long r)
{
	unsigned long sign, expn, frac;
	unsigned int result;

	sign = (r & 0x8000000000000000) >> 63;
	expn = (r & 0x7ff0000000000000) >> 52;
	frac = (r & 0x000fffffe0000000) >> 29;

	/* map exponent part, as appropriate. */
	expn = (expn & 0x7f) | ((expn & 0x400) != 0 ? 0x80 : 0x00);

	result = (sign << 31) | (expn << 23) | (frac << 0);
	return (result);
}

/*
 * Conversion of T floating datums to and from register format
 * requires no bit reordering whatsoever.
 */
static unsigned long
Tfloat_reg_cvt(u_long input)
{

	return (input);
}

#ifdef FIX_UNALIGNED_VAX_FP
static unsigned long
Ffloat_to_reg(u_int f)
{
	unsigned long sign, expn, frlo, frhi;
	unsigned long result;

	sign = (f & 0x00008000) >> 15;
	expn = (f & 0x00007f80) >>  7;
	frhi = (f & 0x0000007f) >>  0;
	frlo = (f & 0xffff0000) >> 16;

	/* map exponent part, as appropriate. */
	if ((expn & 0x80) != 0)
		expn = (0x400 | (expn & ~0x80));
	else if ((expn & 0x80) == 0 && expn != 0)
		expn = (0x380 | (expn & ~0x80));

	result = (sign << 63) | (expn << 52) | (frhi << 45) | (frlo << 29);
	return (result);
}

static unsigned int
reg_to_Ffloat(u_long r)
{
	unsigned long sign, expn, frhi, frlo;
	unsigned int result;

	sign = (r & 0x8000000000000000) >> 63;
	expn = (r & 0x7ff0000000000000) >> 52;
	frhi = (r & 0x000fe00000000000) >> 45;
	frlo = (r & 0x00001fffe0000000) >> 29;

	/* map exponent part, as appropriate. */
	expn = (expn & 0x7f) | ((expn & 0x400) != 0 ? 0x80 : 0x00);

	result = (sign << 15) | (expn << 7) | (frhi << 0) | (frlo << 16);
	return (result);
}

/*
 * Conversion of G floating datums to and from register format is
 * symmetrical.  Just swap shorts in the quad...
 */
static unsigned long
Gfloat_reg_cvt(u_long input)
{
	unsigned long a, b, c, d;
	unsigned long result;

	a = (input & 0x000000000000ffff) >> 0;
	b = (input & 0x00000000ffff0000) >> 16;
	c = (input & 0x0000ffff00000000) >> 32;
	d = (input & 0xffff000000000000) >> 48;

	result = (a << 48) | (b << 32) | (c << 16) | (d << 0);
	return (result);
}
#endif /* FIX_UNALIGNED_VAX_FP */

struct unaligned_fixup_data {
	const char *type;	/* opcode name */
	int fixable;		/* fixable, 0 if fixup not supported */
	int size;		/* size, 0 if unknown */
	int acc;		/* useracc type; B_READ or B_WRITE */
};

#define	UNKNOWN()	{ "0x%lx", 0, 0, 0 }
#define	FIX_LD(n,s)	{ n, 1, s, B_READ }
#define	FIX_ST(n,s)	{ n, 1, s, B_WRITE }
#define	NOFIX_LD(n,s)	{ n, 0, s, B_READ }
#define	NOFIX_ST(n,s)	{ n, 0, s, B_WRITE }

int
unaligned_fixup(u_long va, u_long opcode, u_long reg, struct proc *p)
{
	static const struct unaligned_fixup_data tab_unknown[1] = {
		UNKNOWN(),
	};
	static const struct unaligned_fixup_data tab_0c[0x02] = {
		FIX_LD("ldwu", 2),	FIX_ST("stw", 2),
	};
	static const struct unaligned_fixup_data tab_20[0x10] = {
#ifdef FIX_UNALIGNED_VAX_FP
		FIX_LD("ldf", 4),	FIX_LD("ldg", 8),
#else
		NOFIX_LD("ldf", 4),	NOFIX_LD("ldg", 8),
#endif
		FIX_LD("lds", 4),	FIX_LD("ldt", 8),
#ifdef FIX_UNALIGNED_VAX_FP
		FIX_ST("stf", 4),	FIX_ST("stg", 8),
#else
		NOFIX_ST("stf", 4),	NOFIX_ST("stg", 8),
#endif
		FIX_ST("sts", 4),	FIX_ST("stt", 8),
		FIX_LD("ldl", 4),	FIX_LD("ldq", 8),
		NOFIX_LD("ldl_c", 4),	NOFIX_LD("ldq_c", 8),
		FIX_ST("stl", 4),	FIX_ST("stq", 8),
		NOFIX_ST("stl_c", 4),	NOFIX_ST("stq_c", 8),
	};
	const struct unaligned_fixup_data *selected_tab;
	int doprint, dofix, dosigbus, signal;
	unsigned long *regptr, longdata;
	int intdata;		/* signed to get extension when storing */
	u_int16_t worddata;	/* unsigned to _avoid_ extension */

	/*
	 * Read USP into frame in case it's the register to be modified.
	 * This keeps us from having to check for it in lots of places
	 * later.
	 */
	p->p_md.md_tf->tf_regs[FRAME_SP] = alpha_pal_rdusp();

	/*
	 * Figure out what actions to take.
	 *
	 * XXX In the future, this should have a per-process component
	 * as well.
	 */
	doprint = alpha_unaligned_print;
	dofix = alpha_unaligned_fix;
	dosigbus = alpha_unaligned_sigbus;

	/*
	 * Find out which opcode it is.  Arrange to have the opcode
	 * printed if it's an unknown opcode.
	 */
	if (opcode >= 0x0c && opcode <= 0x0d)
		selected_tab = &tab_0c[opcode - 0x0c];
	else if (opcode >= 0x20 && opcode <= 0x2f)
		selected_tab = &tab_20[opcode - 0x20];
	else
		selected_tab = tab_unknown;

	/*
	 * See if the user can access the memory in question.
	 * If it's an unknown opcode, we don't know whether to
	 * read or write, so we don't check.
	 *
	 * We adjust the PC backwards so that the instruction will
	 * be re-run.
	 */
	if (selected_tab->size != 0 &&
	   !uvm_useracc((caddr_t)va, selected_tab->size, selected_tab->acc)) {
		p->p_md.md_tf->tf_regs[FRAME_PC] -= 4;
		signal = SIGSEGV;
		goto out;
	}

	/*
	 * If we're supposed to be noisy, squawk now.
	 */
	if (doprint) {
		uprintf(
		"pid %d (%s): unaligned access: "
		"va=0x%lx pc=0x%lx ra=0x%lx sp=0x%lx op=",
		    p->p_pid, p->p_comm, va,
		    p->p_md.md_tf->tf_regs[FRAME_PC] - 4,
		    p->p_md.md_tf->tf_regs[FRAME_RA],
		    p->p_md.md_tf->tf_regs[FRAME_SP]);
		uprintf(selected_tab->type,opcode);
		uprintf("\n");
	}

	/*
	 * If we should try to fix it and know how, give it a shot.
	 *
	 * We never allow bad data to be unknowingly used by the
	 * user process.  That is, if we decide not to fix up an
	 * access we cause a SIGBUS rather than letting the user
	 * process go on without warning.
	 *
	 * If we're trying to do a fixup, we assume that things
	 * will be botched.  If everything works out OK, 
	 * unaligned_{load,store}_* clears the signal flag.
	 */
	signal = SIGBUS;
	if (dofix && selected_tab->fixable) {
		switch (opcode) {
		case 0x0c:			/* ldwu */
			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			unaligned_load_integer(worddata);
			break;

		case 0x0d:			/* stw */
			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			unaligned_store_integer(worddata);
			break;

#ifdef FIX_UNALIGNED_VAX_FP
		case 0x20:			/* ldf */
			unaligned_load_floating(intdata, Ffloat_to_reg);
			break;

		case 0x21:			/* ldg */
			unaligned_load_floating(longdata, Gfloat_reg_cvt);
			break;
#endif

		case 0x22:			/* lds */
			unaligned_load_floating(intdata, Sfloat_to_reg);
			break;

		case 0x23:			/* ldt */
			unaligned_load_floating(longdata, Tfloat_reg_cvt);
			break;

#ifdef FIX_UNALIGNED_VAX_FP
		case 0x24:			/* stf */
			unaligned_store_floating(intdata, reg_to_Ffloat);
			break;

		case 0x25:			/* stg */
			unaligned_store_floating(longdata, Gfloat_reg_cvt);
			break;
#endif

		case 0x26:			/* sts */
			unaligned_store_floating(intdata, reg_to_Sfloat);
			break;

		case 0x27:			/* stt */
			unaligned_store_floating(longdata, Tfloat_reg_cvt);
			break;

		case 0x28:			/* ldl */
			unaligned_load_integer(intdata);
			break;

		case 0x29:			/* ldq */
			unaligned_load_integer(longdata);
			break;

		case 0x2c:			/* stl */
			unaligned_store_integer(intdata);
			break;

		case 0x2d:			/* stq */
			unaligned_store_integer(longdata);
			break;

#ifdef DIAGNOSTIC
		default:
			panic("unaligned_fixup: can't get here");
#endif
		}
	} 

	/*
	 * Force SIGBUS if requested.
	 */
	if (dosigbus)
		signal = SIGBUS;

out:
	/*
	 * Write back USP.
	 */
	alpha_pal_wrusp(p->p_md.md_tf->tf_regs[FRAME_SP]);

	return (signal);
}

/*
 * Reserved/unimplemented instruction (opDec fault) handler
 *
 * Argument is the process that caused it.  No useful information
 * is passed to the trap handler other than the fault type.  The
 * address of the instruction that caused the fault is 4 less than
 * the PC stored in the trap frame.
 *
 * If the instruction is emulated successfully, this function returns 0.
 * Otherwise, this function returns the signal to deliver to the process,
 * and fills in *ucodep with the code to be delivered.
 */
int
handle_opdec(struct proc *p, u_int64_t *ucodep)
{
	alpha_instruction inst;
	register_t *regptr, memaddr;
	u_int64_t inst_pc;
	int sig;

	/*
	 * Read USP into frame in case it's going to be used or modified.
	 * This keeps us from having to check for it in lots of places
	 * later.
	 */
	p->p_md.md_tf->tf_regs[FRAME_SP] = alpha_pal_rdusp();

	inst_pc = memaddr = p->p_md.md_tf->tf_regs[FRAME_PC] - 4;
	if (copyin((caddr_t)inst_pc, &inst, sizeof (inst)) != 0) {
		/*
		 * really, this should never happen, but in case it
		 * does we handle it.
		 */
		printf("WARNING: handle_opdec() couldn't fetch instruction\n");
		goto sigsegv;
	}

	switch (inst.generic_format.opcode) {
	case op_ldbu:
	case op_ldwu:
	case op_stw:
	case op_stb:
		regptr = irp(p, inst.mem_format.rb);
		if (regptr != NULL)
			memaddr = *regptr;
		else
			memaddr = 0;
		memaddr += inst.mem_format.displacement;

		regptr = irp(p, inst.mem_format.ra);

		if (inst.mem_format.opcode == op_ldwu ||
		    inst.mem_format.opcode == op_stw) {
			if (memaddr & 0x01) {
				sig = unaligned_fixup(memaddr,
				    inst.mem_format.opcode,
				    inst.mem_format.ra, p);
				if (sig)
					goto unaligned_fixup_sig;
				break;
			}
		}

		if (inst.mem_format.opcode == op_ldbu) {
			u_int8_t b;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			if (copyin((caddr_t)memaddr, &b, sizeof (b)) != 0)
				goto sigsegv;
			if (regptr != NULL)
				*regptr = b;
		} else if (inst.mem_format.opcode == op_ldwu) {
			u_int16_t w;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			if (copyin((caddr_t)memaddr, &w, sizeof (w)) != 0)
				goto sigsegv;
			if (regptr != NULL)
				*regptr = w;
		} else if (inst.mem_format.opcode == op_stw) {
			u_int16_t w;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			w = (regptr != NULL) ? *regptr : 0;
			if (copyout(&w, (caddr_t)memaddr, sizeof (w)) != 0)
				goto sigsegv;
		} else if (inst.mem_format.opcode == op_stb) {
			u_int8_t b;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			b = (regptr != NULL) ? *regptr : 0;
			if (copyout(&b, (caddr_t)memaddr, sizeof (b)) != 0)
				goto sigsegv;
		}
		break;

	case op_intmisc:
		if (inst.operate_generic_format.function == op_sextb &&
		    inst.operate_generic_format.ra == 31) {
			int8_t b;

			if (inst.operate_generic_format.is_lit) {
				b = inst.operate_lit_format.literal;
			} else {
				if (inst.operate_reg_format.sbz != 0)
					goto sigill;
				regptr = irp(p, inst.operate_reg_format.rb);
				b = (regptr != NULL) ? *regptr : 0;
			}

			regptr = irp(p, inst.operate_generic_format.rc);
			if (regptr != NULL)
				*regptr = b;
			break;
		}
		if (inst.operate_generic_format.function == op_sextw &&
		    inst.operate_generic_format.ra == 31) {
			int16_t w;

			if (inst.operate_generic_format.is_lit) {
				w = inst.operate_lit_format.literal;
			} else {
				if (inst.operate_reg_format.sbz != 0)
					goto sigill;
				regptr = irp(p, inst.operate_reg_format.rb);
				w = (regptr != NULL) ? *regptr : 0;
			}

			regptr = irp(p, inst.operate_generic_format.rc);
			if (regptr != NULL)
				*regptr = w;
			break;
		}
		goto sigill;

	default:
		goto sigill;
	}

	/*
	 * Write back USP.  Note that in the error cases below,
	 * nothing will have been successfully modified so we don't
	 * have to write it out.
	 */
	alpha_pal_wrusp(p->p_md.md_tf->tf_regs[FRAME_SP]);

	return (0);

sigill:
	*ucodep = ALPHA_IF_CODE_OPDEC;			/* trap type */
	return (SIGILL);

sigsegv:
	sig = SIGSEGV;
	p->p_md.md_tf->tf_regs[FRAME_PC] = inst_pc;	/* re-run instr. */
unaligned_fixup_sig:
	*ucodep = memaddr;				/* faulting address */
	return (sig);
}
