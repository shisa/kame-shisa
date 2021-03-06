/*	$NetBSD: linux_exec.h,v 1.4 2002/01/17 17:19:03 bjh21 Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#ifndef _M68K_LINUX_EXEC_H
#define _M68K_LINUX_EXEC_H

/*
 * Linux a.out format parameters
 */
#define LINUX_M_68020		2
#define LINUX_MID_MACHINE	LINUX_M_68020

/*
 * Linux Elf32 format parameters
 */
#define LINUX_GCC_SIGNATURE	1

#define LINUX_ELF_AUX_ARGSIZ \
	(howmany(ELF_AUX_ENTRIES * sizeof(Aux32Info), sizeof(Elf32_Addr)))
#ifdef ELF32NAME
#define LINUX_COPYARGS_FUNCTION	ELF32NAME(copyargs)
#else
#define LINUX_COPYARGS_FUNCTION	ELFNAME(copyargs)
#endif

/* Linux uses the standard syscall handler. */
#define LINUX_SYSCALL_FUNCTION syscall

#endif /* !_M68K_LINUX_EXEC_H */
