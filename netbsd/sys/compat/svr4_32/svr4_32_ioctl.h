/*	$NetBSD: svr4_32_ioctl.h,v 1.1 2001/02/06 16:37:57 eeh Exp $	 */

/*-
 * Copyright (c) 1994 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef	_SVR4_32_IOCTL_H_
#define	_SVR4_32_IOCTL_H_

#include <compat/svr4/svr4_ioctl.h>

int	svr4_32_stream_ti_ioctl __P((struct file *, struct proc *, register_t *,
			          int, u_long, caddr_t));
int	svr4_32_stream_ioctl    __P((struct file *, struct proc *, register_t *,
				  int, u_long, caddr_t));
int	svr4_32_term_ioctl      __P((struct file *, struct proc *, register_t *,
				  int, u_long, caddr_t));
int	svr4_32_ttold_ioctl     __P((struct file *, struct proc *, register_t *,
				  int, u_long, caddr_t));
int	svr4_32_fil_ioctl	__P((struct file *, struct proc *, register_t *,
				  int, u_long, caddr_t));
int	svr4_32_sock_ioctl	__P((struct file *, struct proc *, register_t *,
				  int, u_long, caddr_t));

#endif /* !_SVR4_32_IOCTL_H_ */
