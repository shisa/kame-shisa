/*	$OpenBSD: limits.h,v 1.13 2002/09/15 09:01:59 deraadt Exp $	*/
/*	$NetBSD: limits.h,v 1.1 1996/09/30 16:34:28 ws Exp $	*/

/*-
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

#ifndef _POWERPC_LIMITS_H_
#define _POWERPC_LIMITS_H_

#define	MB_LEN_MAX	1		/* no multibyte characters	*/

#if !defined(_ANSI_SOURCE)
#define	SIZE_MAX	UINT_MAX	/* max value for a size_t */
#define SSIZE_MAX	INT_MAX		/* max value for a ssize_t */  

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#define	SIZE_T_MAX	UINT_MAX	/* max value for a size_t */

#define	UQUAD_MAX	0xffffffffffffffffULL		/* max unsigned quad */
#define	QUAD_MAX	0x7fffffffffffffffLL		/* max signed quad */
#define	QUAD_MIN	(-0x7fffffffffffffffLL-1)	/* min signed quad */
#endif	/* !_POSIX_SOURCE && !_XOPEN_SOURCE */
#endif	/* !_ANSI_SOURCE */

#endif /* _POWERPC_LIMITS_H_ */
