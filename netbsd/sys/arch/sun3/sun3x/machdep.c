/*	$NetBSD: machdep.c,v 1.79 2002/05/18 06:59:12 lukem Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: Utah Hdr: machdep.c 1.74 92/12/20
 *	from: @(#)machdep.c	8.10 (Berkeley) 4/20/94
 */

#include "opt_ddb.h"
#include "opt_kgdb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/mount.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/vnode.h>
#include <sys/syscallargs.h>
#ifdef	KGDB
#include <sys/kgdb.h>
#endif

#include <uvm/uvm_extern.h>

#include <sys/sysctl.h>

#include <dev/cons.h>

#include <machine/cpu.h>
#include <machine/dvma.h>
#include <machine/idprom.h>
#include <machine/kcore.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>

#if defined(DDB)
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

#include <sun3/sun3/machdep.h>

/* Defined in locore.s */
extern char kernel_text[];
/* Defined by the linker */
extern char etext[];

/* Our exported CPU info; we can have only one. */  
struct cpu_info cpu_info_store;

struct vm_map *exec_map = NULL;  
struct vm_map *mb_map = NULL;
struct vm_map *phys_map = NULL;

int	physmem;
int	fputype;
caddr_t	msgbufaddr;

/* Virtual page frame for /dev/mem (see mem.c) */
vaddr_t vmmap;

union sun3sir sun3sir;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = PSL_LOWIPL;

u_char cpu_machine_id = 0;
char *cpu_string = NULL;
int cpu_has_vme = 0;
int has_iocache = 0;

vaddr_t dumppage;

static void identifycpu __P((void));
static void initcpu __P((void));

/*
 * Console initialization: called early on from main,
 * before vm init or cpu_startup.  This system is able
 * to use the console for output immediately (via PROM)
 * but can not use it for input until after this point.
 */
void
consinit()
{

	/*
	 * Switch from the PROM console (output only)
	 * to our own console driver.
	 */
	cninit();

#ifdef DDB
	{
		extern int nsym;
		extern char *ssym, *esym;

		ddb_init(nsym, ssym, esym);
	}
#endif	/* DDB */

	/*
	 * Now that the console can do input as well as
	 * output, consider stopping for a debugger.
	 */
	if (boothowto & RB_KDB) {
#ifdef KGDB
		/* XXX - Ask on console for kgdb_dev? */
		/* Note: this will just return if kgdb_dev==NODEV */
		kgdb_connect(1);
#else	/* KGDB */
		/* Either DDB or no debugger (just PROM). */
		Debugger();
#endif	/* KGDB */
	}
}

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 *
 * This is called early in init_main.c:main(), after the
 * kernel memory allocator is ready for use, but before
 * the creation of processes 1,2, and mountroot, etc.
 */
void
cpu_startup()
{
	caddr_t v;
	int sz, i;
	vsize_t size;
	int base, residual;
	vaddr_t minaddr, maxaddr;
	char pbuf[9];

	/*
	 * Initialize message buffer (for kernel printf).
	 * This is put in physical page zero so it will
	 * always be in the same place after a reboot.
	 * Its mapping was prepared in pmap_bootstrap().
	 * Also, offset some to avoid PROM scribbles.
	 */
	v = (caddr_t) KERNBASE;
	msgbufaddr = (caddr_t)(v + MSGBUFOFF);
	initmsgbuf(msgbufaddr, MSGBUFSIZE);

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	initfpu();	/* also prints FPU type */

	format_bytes(pbuf, sizeof(pbuf), ctob(physmem));
	printf("total memory = %s\n", pbuf);

	/*
	 * Get scratch page for dumpsys().
	 */
	if ((dumppage = uvm_km_alloc(kernel_map, NBPG)) == 0)
		panic("startup: alloc dumppage");

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys(NULL, NULL);
	if ((v = (caddr_t)uvm_km_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v, NULL) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)) != 0)
		panic("startup: cannot allocate VM for buffers");
	minaddr = (vaddr_t)buffers;
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * Each buffer has MAXBSIZE bytes of VM space allocated.  Of
		 * that MAXBSIZE space, we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t) buffers + (i * MAXBSIZE);
		curbufsize = NBPG * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL) 
				panic("cpu_startup: not enough memory for "
				    "buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
				       VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(pmap_kernel());

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * We don't use a submap for physio, and use a separate map
	 * for DVMA allocations.  Our vmapbuf just maps pages into
	 * the kernel map (any kernel mapping is OK) and then the
	 * device drivers clone the kernel mappings into DVMA space.
	 */

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
	mb_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				 nmbclusters * mclbytes, VM_MAP_INTRSAFE,
				 FALSE, NULL);

	format_bytes(pbuf, sizeof(pbuf), ptoa(uvmexp.free));
	printf("avail memory = %s\n", pbuf);
	format_bytes(pbuf, sizeof(pbuf), bufpages * NBPG);
	printf("using %d buffers containing %s of memory\n", nbuf, pbuf);

	/*
	 * Allocate a virtual page (for use by /dev/mem)
	 * This page is handed to pmap_enter() therefore
	 * it has to be in the normal kernel VA range.
	 */
	vmmap = uvm_km_valloc_wait(kernel_map, NBPG);

	/*
	 * Create the DVMA maps.
	 */
	dvma_init();

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
}

/*
 * Set registers on exec.
 */
void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{
	struct trapframe *tf = (struct trapframe *)p->p_md.md_regs;

	tf->tf_sr = PSL_USERSET;
	tf->tf_pc = pack->ep_entry & ~1;
	tf->tf_regs[D0] = 0;
	tf->tf_regs[D1] = 0;
	tf->tf_regs[D2] = 0;
	tf->tf_regs[D3] = 0;
	tf->tf_regs[D4] = 0;
	tf->tf_regs[D5] = 0;
	tf->tf_regs[D6] = 0;
	tf->tf_regs[D7] = 0;
	tf->tf_regs[A0] = 0;
	tf->tf_regs[A1] = 0;
	tf->tf_regs[A2] = (int)p->p_psstr;
	tf->tf_regs[A3] = 0;
	tf->tf_regs[A4] = 0;
	tf->tf_regs[A5] = 0;
	tf->tf_regs[A6] = 0;
	tf->tf_regs[SP] = stack;

	/* restore a null state frame */
	p->p_addr->u_pcb.pcb_fpregs.fpf_null = 0;
	if (fputype)
		m68881_restore(&p->p_addr->u_pcb.pcb_fpregs);

	p->p_md.md_flags = 0;
}

/*
 * Info for CTL_HW
 */
char	machine[16] = MACHINE;		/* from <machine/param.h> */
char	kernel_arch[16] = "sun3x";	/* XXX needs a sysctl node */
char	cpu_model[120];

/*
 * XXX - Should empirically estimate the divisor...
 * Note that the value of delay_divisor is roughly
 * 2048 / cpuclock	(where cpuclock is in MHz).
 */
int delay_divisor = 62;		/* assume the fastest (33 MHz) */

void
identifycpu()
{
	u_char machtype;

	machtype = identity_prom.idp_machtype;
	if ((machtype & IDM_ARCH_MASK) != IDM_ARCH_SUN3X) {
		printf("Bad IDPROM arch!\n");
		sunmon_abort();
	}

	cpu_machine_id = machtype;
	switch (cpu_machine_id) {

	case SUN3X_MACH_80:
		cpu_string = "80";  	/* Hydra */
		delay_divisor = 102;	/* 20 MHz */
		cpu_has_vme = FALSE;
		break;

	case SUN3X_MACH_470:
		cpu_string = "470"; 	/* Pegasus */
		delay_divisor = 62; 	/* 33 MHz */
		cpu_has_vme = TRUE;
		break;

	default:
		printf("unknown sun3x model\n");
		sunmon_abort();
	}

	/* Other stuff? (VAC, mc6888x version, etc.) */
	/* Note: miniroot cares about the kernel_arch part. */
	sprintf(cpu_model, "%s %s", kernel_arch, cpu_string);

	printf("Model: %s\n", cpu_model);
}

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	int error;
	dev_t consdev;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		error = sysctl_rdstruct(oldp, oldlenp, newp,
		    &consdev, sizeof consdev);
		break;

#if 0	/* XXX - Not yet... */
	case CPU_ROOT_DEVICE:
		error = sysctl_rdstring(oldp, oldlenp, newp, root_device);
		break;

	case CPU_BOOTED_KERNEL:
		error = sysctl_rdstring(oldp, oldlenp, newp, booted_kernel);
		break;
#endif

	default:
		error = EOPNOTSUPP;
	}
	return (error);
}

/* See: sig_machdep.c */

/*
 * Do a sync in preparation for a reboot.
 * XXX - This could probably be common code.
 * XXX - And now, most of it is in vfs_shutdown()
 * XXX - Put waittime checks in there too?
 */
int waittime = -1;	/* XXX - Who else looks at this? -gwr */
static void
reboot_sync __P((void))
{

	/* Check waittime here to localize its use to this function. */
	if (waittime >= 0)
		return;
	waittime = 0;
	vfs_shutdown();
}

/*
 * Common part of the BSD and SunOS reboot system calls.
 */
__dead void
cpu_reboot(howto, user_boot_string)
	int howto;
	char *user_boot_string;
{
	/* Note: this string MUST be static! */
	static char bootstr[128];
	char *p;

	/* If system is cold, just halt. (early panic?) */
	if (cold)
		goto haltsys;

	/* Un-blank the screen if appropriate. */
	cnpollc(1);

	if ((howto & RB_NOSYNC) == 0) {
		reboot_sync();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 *
		 * XXX - However, if the kernel has been sitting in ddb,
		 * the time will be way off, so don't set the HW clock!
		 * XXX - Should do sanity check against HW clock. -gwr
		 */
		/* resettodr(); */
	}

	/* Disable interrupts. */
	splhigh();

	/* Write out a crash dump if asked. */
	if (howto & RB_DUMP)
		dumpsys();

	/* run any shutdown hooks */
	doshutdownhooks();

	if (howto & RB_HALT) {
	haltsys:
		printf("halted.\n");
		sunmon_halt();
	}

	/*
	 * Automatic reboot.
	 */
	if (user_boot_string)
		strncpy(bootstr, user_boot_string, sizeof(bootstr));
	else {
		/*
		 * Build our own boot string with an empty
		 * boot device/file and (maybe) some flags.
		 * The PROM will supply the device/file name.
		 */
		p = bootstr;
		*p = '\0';
		if (howto & (RB_KDB|RB_ASKNAME|RB_SINGLE)) {
			/* Append the boot flags. */
			*p++ = ' ';
			*p++ = '-';
			if (howto & RB_KDB)
				*p++ = 'd';
			if (howto & RB_ASKNAME)
				*p++ = 'a';
			if (howto & RB_SINGLE)
				*p++ = 's';
			*p = '\0';
		}
	}
	printf("rebooting...\n");
	sunmon_reboot(bootstr);
	for (;;) ;
	/*NOTREACHED*/
}

/*
 * These variables are needed by /sbin/savecore
 */
u_int32_t dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

#define DUMP_EXTRA	1	/* CPU-dependent extra pages */

/*
 * This is called by main to set dumplo, dumpsize.
 * Dumps always skip the first NBPG of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
cpu_dumpconf()
{
	int devblks;	/* size of dump device in blocks */
	int dumpblks;	/* size of dump image in blocks */
	int maj;
	int (*getsize)__P((dev_t));

	if (dumpdev == NODEV)
		return;

	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	getsize = bdevsw[maj].d_psize;
	if (getsize == NULL)
		return;
	devblks = (*getsize)(dumpdev);
	if (devblks <= ctod(1))
		return;
	devblks &= ~(ctod(1) - 1);

	/*
	 * Note: savecore expects dumpsize to be the
	 * number of pages AFTER the dump header.
	 */
	dumpsize = physmem; 	/* pages */

	/* Position dump image near end of space, page aligned. */
	dumpblks = ctod(physmem + DUMP_EXTRA);
	dumplo = devblks - dumpblks;

	/* If it does not fit, truncate it by moving dumplo. */
	/* Note: Must force signed comparison. */
	if (dumplo < ((long)ctod(1))) {
		dumplo = ctod(1);
		dumpsize = dtoc(devblks - dumplo) - DUMP_EXTRA;
	}
}

/* Note: gdb looks for "dumppcb" in a kernel crash dump. */
struct pcb dumppcb;

/*
 * Write a crash dump.  The format while in swap is:
 *   kcore_seg_t cpu_hdr;
 *   cpu_kcore_hdr_t cpu_data;
 *   padding (NBPG-sizeof(kcore_seg_t))
 *   pagemap (2*NBPG)
 *   physical memory...
 */
void
dumpsys()
{
	struct bdevsw *dsw;
	kcore_seg_t *kseg_p;
	cpu_kcore_hdr_t *chdr_p;
	struct sun3x_kcore_hdr *sh;
	phys_ram_seg_t *crs_p;
	char *vaddr;
	paddr_t paddr;
	int psize, todo, seg, segsz;
	daddr_t blkno;
	int error = 0;

	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo <= 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	savectx(&dumppcb);

	dsw = &bdevsw[major(dumpdev)];
	psize = (*(dsw->d_psize))(dumpdev);
	if (psize == -1) {
		printf("dump area unavailable\n");
		return;
	}

	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

	/*
	 * Prepare the dump header
	 */
	blkno = dumplo;
	todo = dumpsize;	/* pages */
	vaddr = (char *)dumppage;
	memset(vaddr, 0, NBPG);

	/* Set pointers to all three parts. */
	kseg_p = (kcore_seg_t *)vaddr;
	chdr_p = (cpu_kcore_hdr_t *)(kseg_p + 1);
	sh = &chdr_p->un._sun3x;

	/* Fill in kcore_seg_t part. */
	CORE_SETMAGIC(*kseg_p, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	kseg_p->c_size = (ctob(DUMP_EXTRA) - sizeof(*kseg_p));

	/* Fill in cpu_kcore_hdr_t part. */
	strncpy(chdr_p->name, kernel_arch, sizeof(chdr_p->name));
	chdr_p->page_size = NBPG;
	chdr_p->kernbase = KERNBASE;

	/* Fill in the sun3x_kcore_hdr part. */
	pmap_kcore_hdr(sh);

	/* Write out the dump header. */
	error = (*dsw->d_dump)(dumpdev, blkno, vaddr, NBPG);
	if (error)
		goto fail;
	blkno += btodb(NBPG);

	/*
	 * Now dump physical memory.  Note that physical memory
	 * might NOT be congiguous, so do it by segments.
	 */

	vaddr = (char *)vmmap;	/* Borrow /dev/mem VA */

	for (seg = 0; seg < SUN3X_NPHYS_RAM_SEGS; seg++) {
		crs_p = &sh->ram_segs[seg];
		paddr = crs_p->start;
		segsz = crs_p->size;

		while (todo && (segsz > 0)) {

			/* Print pages left after every 16. */
			if ((todo & 0xf) == 0)
				printf("\r%4d", todo);

			/* Make a temporary mapping for the page. */
			pmap_kenter_pa(vmmap, paddr | PMAP_NC, VM_PROT_READ);
			pmap_update(pmap_kernel());
			error = (*dsw->d_dump)(dumpdev, blkno, vaddr, NBPG);
			pmap_kremove(vmmap, NBPG);
			pmap_update(pmap_kernel());
			if (error)
				goto fail;
			paddr += NBPG;
			segsz -= NBPG;
			blkno += btodb(NBPG);
			todo--;
		}
	}
	printf("\rdump succeeded\n");
	return;
fail:
	printf(" dump error=%d\n", error);
}

static void
initcpu()
{
	/* XXX: Enable RAM parity/ECC checking? */
	/* XXX: parityenable(); */

#ifdef	HAVECACHE
	cache_enable();
#endif
}

/* straptrap() in trap.c */

/* from hp300: badaddr() */
/* peek_byte(), peek_word() moved to bus_subr.c */

/* XXX: parityenable() ? */
/* regdump() moved to regdump.c */

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 *
 * Determine if the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	return ENOEXEC;
}