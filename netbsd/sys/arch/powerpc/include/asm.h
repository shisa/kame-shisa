/*	$NetBSD: asm.h,v 1.8 2001/06/13 06:01:48 simonb Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PPC_ASM_H_
#define _PPC_ASM_H_

#ifdef PIC
#define PIC_PROLOGUE	XXX
#define PIC_EPILOGUE	XXX
#define PIC_PLT(x)	x@plt
#ifdef	__STDC__
#define PIC_GOT(x)	XXX
#define PIC_GOTOFF(x)	XXX
#else	/* not __STDC__ */
#define PIC_GOT(x)	XXX
#define PIC_GOTOFF(x)	XXX
#endif	/* __STDC__ */
#else
#define PIC_PROLOGUE
#define PIC_EPILOGUE
#define PIC_PLT(x)	x
#define PIC_GOT(x)	x
#define PIC_GOTOFF(x)	x
#endif

#define	_C_LABEL(x)	x
#define	_ASM_LABEL(x)	x

#define	_GLOBAL(x) \
	.data; .align 2; .globl x; x:

#define _ENTRY(x) \
	.text; .align 2; .globl x; .type x,@function; x:

#ifdef GPROF
# define _PROF_PROLOGUE	mflr 0; stw 0,4(1); bl _mcount
#else
# define _PROF_PROLOGUE
#endif

#define	ENTRY(y)	_ENTRY(_C_LABEL(y)); _PROF_PROLOGUE
#define	ASENTRY(y)	_ENTRY(_ASM_LABEL(y)); _PROF_PROLOGUE
#define	GLOBAL(y)	_GLOBAL(_C_LABEL(y))

#define	ASMSTR		.asciz

#define RCSID(x)	.text; .asciz x

#ifdef __ELF__
#define	WEAK_ALIAS(alias,sym)						\
	.weak alias;							\
	alias = sym
#endif

#ifdef __STDC__
#define	WARN_REFERENCES(_sym,_msg)				\
	.section .gnu.warning. ## _sym ; .ascii _msg ; .text
#else
#define	WARN_REFERENCES(_sym,_msg)				\
	.section .gnu.warning./**/_sym ; .ascii _msg ; .text
#endif /* __STDC__ */

/* Condition Register Bit Fields */

#define cr0     0
#define cr1     1
#define cr2     2
#define cr3     3
#define cr4     4
#define cr5     5
#define cr6     6
#define cr7     7

/* General Purpose Registers (GPRs) */

#define r0      0
#define r1      1
#define r2      2
#define r3      3
#define r4      4
#define r5      5
#define r6      6
#define r7      7
#define r8      8
#define r9      9
#define r10     10
#define r11     11
#define r12     12
#define r13     13
#define r14     14
#define r15     15
#define r16     16
#define r17     17
#define r18     18
#define r19     19
#define r20     20
#define r21     21
#define r22     22
#define r23     23
#define r24     24
#define r25     25
#define r26     26
#define r27     27
#define r28     28
#define r29     29
#define r30     30
#define r31     31

/* Floating Point Registers (FPRs) */

#define fr0     0
#define fr1     1
#define fr2     2
#define fr3     3
#define fr4     4
#define fr5     5
#define fr6     6
#define fr7     7
#define fr8     8
#define fr9     9
#define fr10    10
#define fr11    11
#define fr12    12
#define fr13    13
#define fr14    14
#define fr15    15
#define fr16    16
#define fr17    17
#define fr18    18
#define fr19    19
#define fr20    20
#define fr21    21
#define fr22    22
#define fr23    23
#define fr24    24
#define fr25    25
#define fr26    26
#define fr27    27
#define fr28    28
#define fr29    29
#define fr30    30
#define fr31    31

#endif /* !_PPC_ASM_H_ */
