/*	$NetBSD: cacheops.h,v 1.8 2000/04/05 19:38:33 is Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Leo Weppelman
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#ifndef _M68K_CACHEOPS_H_
#define	_M68K_CACHEOPS_H_

#if notyet /* XXX */
#include <machine/cpuconf.h>
#endif

#include <m68k/cacheops_20.h>
#include <m68k/cacheops_30.h>
#include <m68k/cacheops_40.h>
#include <m68k/cacheops_60.h>

#if defined(M68020) && !(defined(M68030)||defined(M68040)||defined(M68060))

#define	TBIA()		TBIA_20()
#define	TBIS(va)	TBIS_20((va))
#define	TBIAS()		TBIAS_20()
#define	TBIAU()		TBIAU_20()
#define	ICIA()		ICIA_20()
#define	ICPA()		ICPA_20()
#define	DCIA()		DCIA_20()
#define	DCIS()		DCIS_20()
#define	DCIU()		DCIU_20()
#define	DCIAS(pa)	DCIAS_20((pa))
#define	PCIA()		PCIA_20()

#elif defined(M68030) && !(defined(M68020)||defined(M68040)||defined(M68060))

#define	TBIA()		TBIA_30()
#define	TBIS(va)	TBIS_30((va))
#define	TBIAS()		TBIAS_30()
#define	TBIAU()		TBIAU_30()
#define	ICIA()		ICIA_30()
#define	ICPA()		ICPA_30()
#define	DCIA()		DCIA_30()
#define	DCIS()		DCIS_30()
#define	DCIU()		DCIU_30()
#define	DCIAS(pa)	DCIAS_30((pa))
#define	PCIA()		PCIA_30()

#elif defined(M68040) && !(defined(M68020)||defined(M68030)||defined(M68060))

#define	TBIA()		TBIA_40()
#define	TBIS(va)	TBIS_40((va))
#define	TBIAS()		TBIAS_40()
#define	TBIAU()		TBIAU_40()
#define	ICIA()		ICIA_40()
#define	ICPA()		ICPA_40()
#define	DCIA()		DCIA_40()
#define	DCIS()		DCIS_40()
#define	DCIU()		DCIU_40()
#define	DCIAS(pa)	DCIAS_40((pa))
#define	PCIA()		PCIA_40()
#define	DCFA()		DCFA_40()
#define	ICPL(pa)	ICPL_40((pa))
#define	ICPP(pa)	ICPP_40((pa))
#define	DCPL(pa)	DCPL_40((pa))
#define	DCPP(pa)	DCPP_40((pa))
#define	DCPA()		DCPA_40()
#define	DCFL(pa)	DCFL_40((pa))
#define	DCFP(pa)	DCFP_40((pa))

#elif defined(M68060) && !(defined(M68020)||defined(M68030)||defined(M68040))

#define	TBIA()		TBIA_60()
#define	TBIS(va)	TBIS_60((va))
#define	TBIAS()		TBIAS_60()
#define	TBIAU()		TBIAU_60()
#define	ICIA()		ICIA_60()
#define	ICPA()		ICPA_60()
#define	DCIA()		DCIA_60()
#define	DCIS()		DCIS_60()
#define	DCIU()		DCIU_60()
#define	DCIAS(pa)	DCIAS_60((pa))
#define	PCIA()		PCIA_60()
#define	DCFA()		DCFA_60()
#define	ICPL(pa)	ICPL_60((pa))
#define	ICPP(pa)	ICPP_60((pa))
#define	DCPL(pa)	DCPL_60((pa))
#define	DCPP(pa)	DCPP_60((pa))
#define	DCPA()		DCPA_60()
#define	DCFL(pa)	DCFL_60((pa))
#define	DCFP(pa)	DCFP_60((pa))

#else /* Multi-CPU config */

/* XXX: From cpuconf.h? */
#ifndef _MULTI_CPU
#define	_MULTI_CPU
#endif

void	_TBIA __P((void));
void	_TBIS __P((vaddr_t));
void	_TBIAS __P((void));
void	_TBIAU __P((void));
void	_ICIA __P((void));
void	_ICPA __P((void));
void	_DCIA __P((void));
void	_DCIS __P((void));
void	_DCIU __P((void));
void	_DCIAS __P((paddr_t));
void	_PCIA __P((void));

#define	TBIA()		_TBIA()
#define	TBIS(va)	_TBIS((va))
#define	TBIAS()		_TBIAS()
#define	TBIAU()		_TBIAU()
#define	ICIA()		_ICIA()
#define	ICPA()		_ICPA()
#define	DCIA()		_DCIA()
#define	DCIS()		_DCIS()
#define	DCIU()		_DCIU()
#define	DCIAS(pa)	_DCIAS((pa))
#define	PCIA()		_PCIA()

#if defined(M68040)||defined(M68060)

void	_DCFA __P((void));
void	_ICPL __P((paddr_t));
void	_ICPP __P((paddr_t));
void	_DCPL __P((paddr_t));
void	_DCPP __P((paddr_t));
void	_DCPA __P((void));
void	_DCFL __P((paddr_t));
void	_DCFP __P((paddr_t));

#define	DCFA()		_DCFA()
#define	ICPL(pa)	_ICPL((pa))
#define	ICPP(pa)	_ICPP((pa))
#define	DCPL(pa)	_DCPL((pa))
#define	DCPP(pa)	_DCPP((pa))
#define	DCPA()		_DCPA()
#define	DCFL(pa)	_DCFL((pa))
#define	DCFP(pa)	_DCFP((pa))

#endif /* defined(M68040)||defined(M68060) */

#endif

#endif /* _M68K_CACHEOPS_H_ */
