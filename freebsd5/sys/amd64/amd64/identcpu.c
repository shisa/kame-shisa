/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * Copyright (c) 1997 KATO Takenori.
 * Copyright (c) 2001 Tamotsu Hattori.
 * Copyright (c) 2001 Mitsuru IWASAKI.
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
 *	from: Id: machdep.c,v 1.193 1996/06/18 01:22:04 bde Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/amd64/amd64/identcpu.c,v 1.128 2003/11/21 03:01:59 peter Exp $");

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/power.h>

#include <machine/asmacros.h>
#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/md_var.h>

#include <amd64/isa/icu.h>

/* XXX - should be in header file: */
void printcpuinfo(void);
void identify_cpu(void);
void earlysetcpuclass(void);
void panicifcpuunsupported(void);

static void print_AMD_features(void);
static void print_AMD_info(void);
static void print_AMD_assoc(int i);

int	cpu_class;
u_int	cpu_exthigh;		/* Highest arg to extended CPUID */
char machine[] = "amd64";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, 
    machine, 0, "Machine class");

static char cpu_model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, 
    cpu_model, 0, "Machine model");

static int hw_clockrate;
SYSCTL_INT(_hw, OID_AUTO, clockrate, CTLFLAG_RD, 
    &hw_clockrate, 0, "CPU instruction clock rate");

static char cpu_brand[48];

static struct cpu_nameclass amd64_cpus[] = {
	{ "Clawhammer",		CPUCLASS_K8 },		/* CPU_CLAWHAMMER */
	{ "Sledgehammer",	CPUCLASS_K8 },		/* CPU_SLEDGEHAMMER */
};

void
printcpuinfo(void)
{
	u_int regs[4], i;
	char *brand;

	cpu_class = amd64_cpus[cpu].cpu_class;
	printf("CPU: ");
	strncpy(cpu_model, amd64_cpus[cpu].cpu_name, sizeof (cpu_model));

	/* Check for extended CPUID information and a processor name. */
	if (cpu_high > 0 &&
	    (strcmp(cpu_vendor, "GenuineIntel") == 0 ||
	    strcmp(cpu_vendor, "AuthenticAMD") == 0)) {
		do_cpuid(0x80000000, regs);
		if (regs[0] >= 0x80000000) {
			cpu_exthigh = regs[0];
			if (cpu_exthigh >= 0x80000004) {
				brand = cpu_brand;
				for (i = 0x80000002; i < 0x80000005; i++) {
					do_cpuid(i, regs);
					memcpy(brand, regs, sizeof(regs));
					brand += sizeof(regs);
				}
			}
		}
	}

	if (strcmp(cpu_vendor, "GenuineIntel") == 0) {
		/* How the hell did you get here?? */
		strcat(cpu_model, "Yamhill?");
	} else if (strcmp(cpu_vendor, "AuthenticAMD") == 0) {
		/*
		 * Values taken from AMD Processor Recognition
		 * http://www.amd.com/K6/k6docs/pdf/20734g.pdf
		 * (also describes ``Features'' encodings.
		 */
		strcpy(cpu_model, "AMD ");
		switch (cpu_id & 0xF00) {
		case 0xf00:
			strcat(cpu_model, "AMD64 Processor");
			break;
		default:
			strcat(cpu_model, "Unknown");
			break;
		}
	}

	/*
	 * Replace cpu_model with cpu_brand minus leading spaces if
	 * we have one.
	 */
	brand = cpu_brand;
	while (*brand == ' ')
		++brand;
	if (*brand != '\0')
		strcpy(cpu_model, brand);

	printf("%s (", cpu_model);
	switch(cpu_class) {
	case CPUCLASS_K8:
		hw_clockrate = (tsc_freq + 5000) / 1000000;
		printf("%jd.%02d-MHz ",
		       (intmax_t)(tsc_freq + 4999) / 1000000,
		       (u_int)((tsc_freq + 4999) / 10000) % 100);
		printf("K8");
		break;
	default:
		printf("Unknown");	/* will panic below... */
	}
	printf("-class CPU)\n");
	if(*cpu_vendor)
		printf("  Origin = \"%s\"",cpu_vendor);
	if(cpu_id)
		printf("  Id = 0x%x", cpu_id);

	if (strcmp(cpu_vendor, "GenuineIntel") == 0 ||
	    strcmp(cpu_vendor, "AuthenticAMD") == 0) {
		printf("  Stepping = %u", cpu_id & 0xf);
		if (cpu_high > 0) {
			/*
			 * Here we should probably set up flags indicating
			 * whether or not various features are available.
			 * The interesting ones are probably VME, PSE, PAE,
			 * and PGE.  The code already assumes without bothering
			 * to check that all CPUs >= Pentium have a TSC and
			 * MSRs.
			 */
			printf("\n  Features=0x%b", cpu_feature,
			"\020"
			"\001FPU"	/* Integral FPU */
			"\002VME"	/* Extended VM86 mode support */
			"\003DE"	/* Debugging Extensions (CR4.DE) */
			"\004PSE"	/* 4MByte page tables */
			"\005TSC"	/* Timestamp counter */
			"\006MSR"	/* Machine specific registers */
			"\007PAE"	/* Physical address extension */
			"\010MCE"	/* Machine Check support */
			"\011CX8"	/* CMPEXCH8 instruction */
			"\012APIC"	/* SMP local APIC */
			"\013oldMTRR"	/* Previous implementation of MTRR */
			"\014SEP"	/* Fast System Call */
			"\015MTRR"	/* Memory Type Range Registers */
			"\016PGE"	/* PG_G (global bit) support */
			"\017MCA"	/* Machine Check Architecture */
			"\020CMOV"	/* CMOV instruction */
			"\021PAT"	/* Page attributes table */
			"\022PSE36"	/* 36 bit address space support */
			"\023PN"	/* Processor Serial number */
			"\024CLFLUSH"	/* Has the CLFLUSH instruction */
			"\025<b20>"
			"\026DTS"	/* Debug Trace Store */
			"\027ACPI"	/* ACPI support */
			"\030MMX"	/* MMX instructions */
			"\031FXSR"	/* FXSAVE/FXRSTOR */
			"\032SSE"	/* Streaming SIMD Extensions */
			"\033SSE2"	/* Streaming SIMD Extensions #2 */
			"\034SS"	/* Self snoop */
			"\035HTT"	/* Hyperthreading (see EBX bit 16-23) */
			"\036TM"	/* Thermal Monitor clock slowdown */
			"\037IA64"	/* CPU can execute IA64 instructions */
			"\040PBE"	/* Pending Break Enable */
			);

			/*
			 * If this CPU supports hyperthreading then mention
			 * the number of logical CPU's it contains.
			 */
			if (cpu_feature & CPUID_HTT &&
			    (cpu_procinfo & CPUID_HTT_CORES) >> 16 > 1)
				printf("\n  Hyperthreading: %d logical CPUs",
				    (cpu_procinfo & CPUID_HTT_CORES) >> 16);
		}
		if (strcmp(cpu_vendor, "AuthenticAMD") == 0 &&
		    cpu_exthigh >= 0x80000001)
			print_AMD_features();
	} else if (strcmp(cpu_vendor, "CyrixInstead") == 0) {
	}
	/* Avoid ugly blank lines: only print newline when we have to. */
	if (*cpu_vendor || cpu_id)
		printf("\n");

	if (!bootverbose)
		return;

	if (strcmp(cpu_vendor, "AuthenticAMD") == 0)
		print_AMD_info();
}

void
panicifcpuunsupported(void)
{

#ifndef HAMMER
#error "You need to specify a cpu type"
#endif
	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
	case CPUCLASS_X86:
#ifndef HAMMER
	case CPUCLASS_K8:
#endif
		panic("CPU class not configured");
	default:
		break;
	}
}


/*
 * Final stage of CPU identification. -- Should I check TI?
 */
void
identify_cpu(void)
{
	u_int regs[4];

	do_cpuid(0, regs);
	cpu_high = regs[0];
	((u_int *)&cpu_vendor)[0] = regs[1];
	((u_int *)&cpu_vendor)[1] = regs[3];
	((u_int *)&cpu_vendor)[2] = regs[2];
	cpu_vendor[12] = '\0';

	do_cpuid(1, regs);
	cpu_id = regs[0];
	cpu_procinfo = regs[1];
	cpu_feature = regs[3];

	/* XXX */
	cpu = CPU_CLAWHAMMER;
}

static void
print_AMD_assoc(int i)
{
	if (i == 255)
		printf(", fully associative\n");
	else
		printf(", %d-way associative\n", i);
}

static void
print_AMD_info(void)
{

	if (cpu_exthigh >= 0x80000005) {
		u_int regs[4];

		do_cpuid(0x80000005, regs);
		printf("Data TLB: %d entries", (regs[1] >> 16) & 0xff);
		print_AMD_assoc(regs[1] >> 24);
		printf("Instruction TLB: %d entries", regs[1] & 0xff);
		print_AMD_assoc((regs[1] >> 8) & 0xff);
		printf("L1 data cache: %d kbytes", regs[2] >> 24);
		printf(", %d bytes/line", regs[2] & 0xff);
		printf(", %d lines/tag", (regs[2] >> 8) & 0xff);
		print_AMD_assoc((regs[2] >> 16) & 0xff);
		printf("L1 instruction cache: %d kbytes", regs[3] >> 24);
		printf(", %d bytes/line", regs[3] & 0xff);
		printf(", %d lines/tag", (regs[3] >> 8) & 0xff);
		print_AMD_assoc((regs[3] >> 16) & 0xff);
		if (cpu_exthigh >= 0x80000006) {	/* K6-III only */
			do_cpuid(0x80000006, regs);
			printf("L2 internal cache: %d kbytes", regs[2] >> 16);
			printf(", %d bytes/line", regs[2] & 0xff);
			printf(", %d lines/tag", (regs[2] >> 8) & 0x0f);
			print_AMD_assoc((regs[2] >> 12) & 0x0f);	
		}
	}
}

static void
print_AMD_features(void)
{
	u_int regs[4];

	/*
	 * Values taken from AMD Processor Recognition
	 * http://www.amd.com/products/cpg/athlon/techdocs/pdf/20734.pdf
	 */
	do_cpuid(0x80000001, regs);
	printf("\n  AMD Features=0x%b", regs[3] &~ cpu_feature,
		"\020"		/* in hex */
		"\001FPU"	/* Integral FPU */
		"\002VME"	/* Extended VM86 mode support */
		"\003DE"	/* Debug extensions */
		"\004PSE"	/* 4MByte page tables */
		"\005TSC"	/* Timestamp counter */
		"\006MSR"	/* Machine specific registers */
		"\007PAE"	/* Physical address extension */
		"\010MCE"	/* Machine Check support */
		"\011CX8"	/* CMPEXCH8 instruction */
		"\012APIC"	/* SMP local APIC */
		"\013<b10>"
		"\014SYSCALL"	/* SYSENTER/SYSEXIT instructions */
		"\015MTRR"	/* Memory Type Range Registers */
		"\016PGE"	/* PG_G (global bit) support */
		"\017MCA"	/* Machine Check Architecture */
		"\020ICMOV"	/* CMOV instruction */
		"\021PAT"	/* Page attributes table */
		"\022PGE36"	/* 36 bit address space support */
		"\023RSVD"	/* Reserved, unknown */
		"\024MP"	/* Multiprocessor Capable */
		"\025NX"	/* Has EFER.NXE, NX (no execute pte bit) */
		"\026<b21>"
		"\027MMX+"	/* AMD MMX Instruction Extensions */
		"\030MMX"
		"\031FXSAVE"	/* FXSAVE/FXRSTOR */
		"\032<b25>"
		"\033<b26>"
		"\034<b27>"
		"\035<b28>"
		"\036LM"	/* Long mode */
		"\0373DNow!+"	/* AMD 3DNow! Instruction Extensions */
		"\0403DNow!"	/* AMD 3DNow! Instructions */
		);
}
