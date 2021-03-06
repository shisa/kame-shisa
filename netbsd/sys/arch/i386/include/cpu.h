/*	$NetBSD: cpu.h,v 1.80.4.1 2003/09/07 13:09:54 tron Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

#ifndef _I386_CPU_H_
#define _I386_CPU_H_

#if defined(_KERNEL_OPT)
#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"
#endif

/*
 * Definitions unique to i386 cpu support.
 */
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/segments.h>

#include <sys/sched.h>

struct i386_cache_info {
	int		cai_index;
	u_int32_t	cai_desc;
	u_int		cai_totalsize;
	u_int		cai_associativity;
	u_int		cai_linesize;
};

#define	CAI_ITLB	0		/* Instruction TLB (4K pages) */
#define	CAI_ITLB2	1		/* Instruction TLB (2/4M pages) */
#define	CAI_DTLB	2		/* Data TLB (4K pages) */
#define	CAI_DTLB2	3		/* Data TLB (2/4M pages) */
#define	CAI_ICACHE	4		/* Instruction cache */
#define	CAI_DCACHE	5		/* Data cache */
#define	CAI_L2CACHE	6		/* Level 2 cache */

#define	CAI_COUNT	7

struct cpu_info {
	struct schedstate_percpu ci_schedstate; /* scheduler state */
#if defined(DIAGNOSTIC) || defined(LOCKDEBUG)
	u_long ci_spin_locks;		/* # of spin locks held */
	u_long ci_simple_locks;		/* # of simple locks held */
#endif

	u_int ci_cflush_lsize;	/* CFLUSH insn line size */
	struct i386_cache_info ci_cinfo[CAI_COUNT];
	void (*ci_info) __P((int));
};

#ifdef _KERNEL
extern struct cpu_info cpu_info_store;

#define	curcpu()			(&cpu_info_store)
#endif

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_swapin(p)			/* nothing */
#define	cpu_number()			0

/*
 * Arguments to hardclock, softclock and statclock
 * encapsulate the previous machine state in an opaque
 * clockframe; for now, use generic intrframe.
 *
 * Note: Since spllowersoftclock() does not actually unmask the currently
 * running (hardclock) interrupt, CLKF_BASEPRI() *must* always be 0; otherwise
 * we could stall hardclock ticks if another interrupt takes too long.
 */
#define clockframe intrframe

#define	CLKF_USERMODE(frame)	USERMODE((frame)->if_cs, (frame)->if_eflags)
#define	CLKF_BASEPRI(frame)	(0)
#define	CLKF_PC(frame)		((frame)->if_eip)
#define	CLKF_INTR(frame)	((frame)->if_ppl & (1 << IPL_TAGINTR))

/*
 * This is used during profiling to integrate system time.  It can safely
 * assume that the process is resident.
 */
#define	PROC_PC(p)		((p)->p_md.md_regs->tf_eip)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern	int	want_resched;		/* resched() was called */
#define	need_resched(ci)	(want_resched = 1, setsoftast())

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, setsoftast())

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)		setsoftast()

/*
 * We need a machine-independent name for this.
 */
#define	DELAY(x)		delay(x)

/*
 * pull in #defines for kinds of processors
 */
#include <machine/cputypes.h>

struct cpu_nocpuid_nameclass {
	int cpu_vendor;
	const char *cpu_vendorname;
	const char *cpu_name;
	int cpu_class;
	void (*cpu_setup) __P((void));
	void (*cpu_cacheinfo) __P((struct cpu_info *));
	void (*cpu_info) __P((int));
};


struct cpu_cpuid_nameclass {
	const char *cpu_id;
	int cpu_vendor;
	const char *cpu_vendorname;
	struct cpu_cpuid_family {
		int cpu_class;
		const char *cpu_models[CPU_MAXMODEL+2];
		void (*cpu_setup) __P((void));
		void (*cpu_cacheinfo) __P((struct cpu_info *));
		void (*cpu_info) __P((int));
	} cpu_family[CPU_MAXFAMILY - CPU_MINFAMILY + 1];
};

#ifdef _KERNEL
extern int biosbasemem;
extern int biosextmem;
extern int cpu;
extern int cpu_class;
extern unsigned int cpu_feature;
extern int cpu_id;
extern char cpu_vendor[];
extern int cpuid_level;
extern const struct cpu_nocpuid_nameclass i386_nocpuid_cpus[];
extern const struct cpu_cpuid_nameclass i386_cpuid_cpus[];

extern int i386_use_fxsave;
extern int i386_has_sse;
extern int i386_has_sse2;

/* machdep.c */
void	delay __P((int));
void	dumpconf __P((void));
void	cpu_reset __P((void));
void	i386_proc0_tss_ldt_init __P((void));
void	i386_bufinit __P((void));

/* locore.s */
struct region_descriptor;
void	lgdt __P((struct region_descriptor *));
void	fillw __P((short, void *, size_t));

struct pcb;
void	savectx __P((struct pcb *));
void	switch_exit __P((struct proc *));
void	proc_trampoline __P((void));

/* clock.c */
void	initrtclock __P((void));
void	startrtclock __P((void));

/* npx.c */
void	npxdrop __P((void));
void	npxsave __P((void));

/* vm_machdep.c */
int kvtop __P((caddr_t));

#if !defined(_LKM)
#include "opt_math_emulate.h"
#endif
#ifdef MATH_EMULATE
/* math_emulate.c */
int	math_emulate __P((struct trapframe *));
#endif

#if !defined(_LKM)
#include "opt_user_ldt.h"
#endif
#ifdef USER_LDT
/* sys_machdep.h */
int	i386_get_ldt __P((struct proc *, void *, register_t *));
int	i386_set_ldt __P((struct proc *, void *, register_t *));
#endif

/* isa_machdep.c */
void	isa_defaultirq __P((void));
int	isa_nmi __P((void));

#if !defined(_LKM)
#include "opt_vm86.h"
#endif
#ifdef VM86
/* vm86.c */
void	vm86_gpfault __P((struct proc *, int));
#endif /* VM86 */

/* consinit.c */
void kgdb_port_init __P((void));

/* bus_machdep.c */
void i386_bus_space_init __P((void));
void i386_bus_space_mallocok __P((void));

#endif /* _KERNEL */

/* 
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_BIOSBASEMEM		2	/* int: bios-reported base mem (K) */
#define	CPU_BIOSEXTMEM		3	/* int: bios-reported ext. mem (K) */
#define	CPU_NKPDE		4	/* int: number of kernel PDEs */
#define	CPU_BOOTED_KERNEL	5	/* string: booted kernel name */
#define CPU_DISKINFO		6	/* struct disklist *:
					 * disk geometry information */
#define CPU_FPU_PRESENT		7	/* int: FPU is present */
#define	CPU_OSFXSR		8	/* int: OS uses FXSAVE/FXRSTOR */
#define	CPU_SSE			9	/* int: OS/CPU supports SSE */
#define	CPU_SSE2		10	/* int: OS/CPU supports SSE2 */
#define CPU_TMLR_MODE		11 	/* int: longrun mode
					 * 0: minimum frequency
					 * 1: economy
					 * 2: performance
					 * 3: maximum frequency
					 */
#define CPU_TMLR_FREQUENCY	12 	/* int: current frequency */
#define CPU_TMLR_VOLTAGE	13 	/* int: curret voltage */
#define CPU_TMLR_PERCENTAGE	14	/* int: current clock percentage */
#define	CPU_MAXID		15	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "biosbasemem", CTLTYPE_INT }, \
	{ "biosextmem", CTLTYPE_INT }, \
	{ "nkpde", CTLTYPE_INT }, \
	{ "booted_kernel", CTLTYPE_STRING }, \
	{ "diskinfo", CTLTYPE_STRUCT }, \
	{ "fpu_present", CTLTYPE_INT }, \
	{ "osfxsr", CTLTYPE_INT }, \
	{ "sse", CTLTYPE_INT }, \
	{ "sse2", CTLTYPE_INT }, \
	{ "tm_longrun_mode", CTLTYPE_INT }, \
	{ "tm_longrun_frequency", CTLTYPE_INT }, \
	{ "tm_longrun_voltage", CTLTYPE_INT }, \
	{ "tm_longrun_percentage", CTLTYPE_INT }, \
}


/*
 * Structure for CPU_DISKINFO sysctl call.
 * XXX this should be somewhere else.
 */
#define MAX_BIOSDISKS	16

struct disklist {
	int dl_nbiosdisks;			   /* number of bios disks */
	struct biosdisk_info {
		int bi_dev;			   /* BIOS device # (0x80 ..) */
		int bi_cyl;			   /* cylinders on disk */
		int bi_head;			   /* heads per track */
		int bi_sec;			   /* sectors per track */
		u_int64_t bi_lbasecs;		   /* total sec. (iff ext13) */
#define BIFLAG_INVALID		0x01
#define BIFLAG_EXTINT13		0x02
		int bi_flags;
	} dl_biosdisks[MAX_BIOSDISKS];

	int dl_nnativedisks;			   /* number of native disks */
	struct nativedisk_info {
		char ni_devname[16];		   /* native device name */
		int ni_nmatches; 		   /* # of matches w/ BIOS */
		int ni_biosmatches[MAX_BIOSDISKS]; /* indices in dl_biosdisks */
	} dl_nativedisks[1];			   /* actually longer */
};

#endif /* !_I386_CPU_H_ */
