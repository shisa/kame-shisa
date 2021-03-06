/*	$NetBSD: start_dos.S,v 1.6 1999/08/23 08:24:38 kleink Exp $	*/
	
/*
 * startup for DOS .COM programs
 * with input from:
 * netbsd:sys/arch/i386/boot/start.S
 * Tor Egge's patches for NetBSD boot (pr port-i386/1002)
 * freebsd:sys/i386/boot/netboot/start2.S
 * XMS support by Martin Husemann
 */

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <machine/asm.h>
#define	addr32	.byte 0x67
#define	data32	.byte 0x66

	.data
	.globl _C_LABEL(ourseg)
_C_LABEL(ourseg):
	.long	0

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
#ifdef __ELF__
	.align	16
#else
	.align	4
#endif
gdt:
	.word	0, 0
	.byte	0, 0x00, 0x00, 0

#ifdef SUPPORT_LINUX	/* additional dummy */
	.word	0, 0
	.byte	0, 0x00, 0x00, 0
#endif

	/* kernel code segment */
	.globl flatcodeseg
flatcodeseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	/* kernel data segment */
	.globl flatdataseg
flatdataseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0

	/* boot code segment, will be patched */
bootcodeseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9e, 0x40, 0

	/* boot data segment, will be patched */
bootdataseg = . - gdt
#ifdef HEAP_BELOW_64K
	.word	0xffff, 0
	.byte	0, 0x92, 0x00, 0
#else
	.word	0xffff, 0
	.byte	0, 0x92, 0x0f, 0
#endif

	/* 16 bit real mode, will be patched */
bootrealseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9e, 0x00, 0
gdtlen = . - gdt

#ifdef __ELF__
	.align	16
#else
	.align	4
#endif
gdtarg:
	.word	gdtlen-1		/* limit */
	.long	0			/* addr, will be inserted */

	.text
ENTRY(start)
	data32
	xorl	%eax, %eax
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %es
	addr32
	data32
	movl	%eax, _C_LABEL(ourseg)
	data32
	shll	$4, %eax

	/* fix up GDT entries for bootstrap */
#define FIXUP(gdt_index) \
	addr32; \
	movl	%eax, gdt+gdt_index+2;	/* actually movw %ax */ \
	addr32; \
	movb	%bl, gdt+gdt_index+4

	data32
	shldl	$16, %eax, %ebx
	FIXUP(bootcodeseg)
	FIXUP(bootrealseg)
	FIXUP(bootdataseg)

	/* fix up GDT pointer */
	data32
	addl	$gdt, %eax
	addr32
	data32
	movl	%eax, gdtarg+2

	/* change to protected mode */
	data32
	call	_C_LABEL(real_to_prot)

	/* clear the bss */
	movl	$_C_LABEL(edata), %edi
	movl	$_C_LABEL(end), %ecx
	subl	%edi, %ecx
	subb	%al, %al
	rep
	stosb

	call	_C_LABEL(doscommain)
ENTRY(exit)
	call	_C_LABEL(prot_to_real)
	sti
	movb	$0x4c,%ah /* return */
	int	$0x21

CR0_PE		=	0x1

/*
 * real_to_prot()
 * 	transfer from real mode to protected mode.
 */
ENTRY(real_to_prot)
	# guarantee that interrupt is disabled when in prot mode
	cli

	# load the gdtr
	addr32
	data32
	lgdt	gdtarg

	# set the PE bit of CR0
	movl	%cr0, %eax

	data32
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	# make intrasegment jump to flush the processor pipeline and
	# reload CS register
	data32
	ljmp	$bootcodeseg, $xprot

xprot:
	# we are in USE32 mode now
	# set up the protected mode segment registers : DS, SS, ES
	movl	$bootdataseg, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	ret

/*
 * prot_to_real()
 * 	transfer from protected mode to real mode
 */
ENTRY(prot_to_real)
	# set up a dummy stack frame for the second seg change.
	# Adjust the intersegment jump instruction following 
	# the clearing of protected mode bit.
	# This is self-modifying code, but we need a writable
	# code segment, and an intersegment return does not give us that.

	movl	_C_LABEL(ourseg), %eax
	movw	%ax, xreal-2

	# Change to use16 mode.
	ljmp	$bootrealseg, $x16

x16:
	# clear the PE bit of CR0
	movl	%cr0, %eax
	data32
	andl 	$~CR0_PE, %eax
	movl	%eax, %cr0
	# Here we have an 16 bits intersegment jump.
	.byte 0xea
	.word xreal
	.word 0

xreal:
	# we are in real mode now
	# set up the real mode segment registers : DS, SS, ES
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	sti
	data32
	ret

/**************************************************************************
___MAIN - Dummy to keep GCC happy
**************************************************************************/
ENTRY(__main)
	ret

/*
 * pbzero(dst, cnt)
 *	where dst is a physical address and cnt is the length
 */
ENTRY(pbzero)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$flatdataseg, %eax
	movl	%ax, %es

	movl	8(%ebp), %edi		# destination
	movl	12(%ebp), %ecx		# count
	xorl	%eax, %eax		# value

	rep
	stosb

	popl	%edi
	popl	%es
	popl	%ebp
	ret

/*
 * vpbcopy(src, dst, cnt)
 *	where src is a virtual address and dst is a physical address
 */
ENTRY(vpbcopy)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%esi
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$flatdataseg, %eax
	movl	%ax, %es

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%es
	popl	%ebp
	ret

/*
 * pvbcopy(src, dst, cnt)
 *	where src is a physical address and dst is a virtual address
 */
ENTRY(pvbcopy)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ds
	pushl	%esi
	pushl	%edi

	cld

	# set %ds to point at the flat segment
	movl	$flatdataseg, %eax
	movl	%ax, %ds

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%ds
	popl	%ebp
	ret

ENTRY(vtophys)
	movl _C_LABEL(ourseg), %eax
	shll	$4, %eax
	addl 4(%esp), %eax
	ret

#ifdef XMS

/* pointer to XMS driver, 0 if no XMS used */

	.data
_C_LABEL(xmsdrv):
	.long	0

	.text
ENTRY(checkxms)
	pushl %ebp
	movl  %esp, %ebp
	pushl	%ebx
	pushl	%edx
	pushl	%es
	pushl	%esi
	pushl	%edi

	call	_C_LABEL(prot_to_real)	# enter real mode

	.byte	0xb8	/* movw $0x4300,ax */
	.word	0x4300
	int	$0x2f	/* check if XMS installed */
	cmpb	$0x80, %al
	jnz	noxms

	.byte	0xb8	/* movw $0x4310,ax */
	.word	0x4310
	int	$0x2f	/* get driver address */

	.byte	0x89,0x1e	/* save es:bx to _xmsdrv */
	.word	_C_LABEL(xmsdrv)
	.byte	0x8c,0x06
	.word	_C_LABEL(xmsdrv)+2

	movb	$0x08, %ah	/* XMS: query free extended memory */
#if 0
	movb	$0x00, %bl
#endif
	.byte	0xff, 0x1e
	.word	_C_LABEL(xmsdrv)  /* result in dx */
	jmp	xdone

noxms:		/* no XMS manager found */
	movl	$0, %edx /* comes out as movw ! */

xdone:
	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	xorl	%eax, %eax
	movw	%dx, %ax

	popl	%edi
	popl	%esi
	popl	%es
	popl	%edx
	popl	%ebx
	popl	%ebp
	ret

/*
	Allocate a block of XMS memory with the requested size
		void * xmsalloc(long int kBytes);

	Depends on _xmsdrv being set by getextmem() before first call
	to this function.

	Return value: a physical address.
*/
ENTRY(xmsalloc)
	pushl %ebp
	movl  %esp, %ebp
	pushl	%ebx
	pushl	%edx
	pushl	%esi
	pushl	%edi

	movl	0x8(%ebp), %edx # Kbytes needed

	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	$0x09, %ah	# XMS allocate block
	.byte 0xff,0x1e
	.word _C_LABEL(xmsdrv)	# result: handle in %dx
	movb	$0x0c, %ah	# XMS lock block
	.byte 0xff,0x1e
	.word _C_LABEL(xmsdrv)	# result: 32 bit physical address in DX:BX

	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	movl	%edx, %eax
	shl	$16, %eax
	movw	%bx, %ax	# result in %eax

	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ebx
	popl	%ebp
	ret

/*
 * ppbcopy(src, dst, cnt)
 *	where src and dst are physical addresses
 */
ENTRY(ppbcopy)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%esi
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$flatdataseg, %eax
	movl	%ax, %es

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	es
	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%es
	popl	%ebp
	ret

#endif
