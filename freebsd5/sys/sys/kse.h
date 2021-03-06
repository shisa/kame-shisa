/*
 * Copyright (C) 2001 Julian Elischer <julian@freebsd.org>
 * for the FreeBSD Foundation.
 *
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible 
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * $FreeBSD: src/sys/sys/kse.h,v 1.19 2003/08/05 12:00:55 davidxu Exp $
 */

#ifndef _SYS_KSE_H_
#define _SYS_KSE_H_

#include <machine/kse.h>
#include <sys/ucontext.h>
#include <sys/time.h>
#include <sys/signal.h>

/*
 * This file defines the structures needed for communication between
 * the userland and the kernel when running a KSE-based threading system.
 * The only programs that should see this file are the user thread
 * scheduler (UTS) and the kernel.
 */
struct kse_mailbox;

typedef void	kse_func_t(struct kse_mailbox *);

/*
 * Thread mailbox.
 *
 * This describes a user thread to the kernel scheduler.
 */
struct kse_thr_mailbox {
	ucontext_t		tm_context;	/* User and machine context */
	unsigned int		tm_flags;	/* Thread flags */
	struct kse_thr_mailbox	*tm_next;	/* Next thread in list */
	void			*tm_udata;	/* For use by the UTS */
	uint32_t		tm_uticks;
	uint32_t		tm_sticks;
	siginfo_t		tm_syncsig;
	int			tm_spare[8];
};

/*
 * KSE mailbox.
 *
 * Communication path between the UTS and the kernel scheduler specific to
 * a single KSE.
 */
struct kse_mailbox {
	int			km_version;	/* Mailbox version */
	struct kse_thr_mailbox	*km_curthread;	/* Currently running thread */
	struct kse_thr_mailbox	*km_completed;	/* Threads back from kernel */
	sigset_t		km_sigscaught;	/* Caught signals */
	uint32_t		km_flags;	/* KSE flags */
	kse_func_t		*km_func;	/* UTS function */
	stack_t			km_stack;	/* UTS context */
	void			*km_udata;	/* For use by the UTS */
	struct timespec		km_timeofday;	/* Time of day */
	int			km_quantum;	/* Upcall quantum in msecs */
	int			km_spare[8];
};

#define	KSE_VER_0	0
#define	KSE_VERSION	KSE_VER_0

/* These flags are kept in km_flags */
#define	KMF_NOUPCALL		0x01
#define	KMF_NOCOMPLETED		0x02
#define	KMF_DONE		0x04
#define	KMF_BOUND		0x08
#define	KMF_WAITSIGEVENT	0x10

/* These flags are kept in tm_flags */
#define	TMF_NOUPCALL		0x01

/* Commands for kse_thr_interrupt */
#define	KSE_INTR_INTERRUPT	0x01
#define	KSE_INTR_RESTART	0x02
#define	KSE_INTR_SENDSIG	0x03
#define	KSE_INTR_SIGEXIT	0x04

#ifndef _KERNEL
int	kse_create(struct kse_mailbox *, int);
int	kse_exit(void);
int	kse_release(struct timespec *);
int	kse_thr_interrupt(struct kse_thr_mailbox *, int, long);
int	kse_wakeup(struct kse_mailbox *);
#endif	/* !_KERNEL */

#endif	/* !_SYS_KSE_H_ */
