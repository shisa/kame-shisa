/*	$KAME: natpt_var.h,v 1.40 2002/12/16 09:21:50 fujisawa Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000 and 2001 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

extern int		natpt_enable;
extern int		natpt_initialized;
extern int		natpt_error;
extern int		natpt_param;
extern u_int		natpt_debug;
extern u_int		natpt_dump;
extern struct in6_addr	natpt_prefix;
extern u_int		natpt_forceFragment4;
extern u_int		natpt_uselog;
extern u_int		natpt_usesyslog;

extern caddr_t	natptctl_vars[];


/*
 *
 */

/*  natpt_dispatch.c */
caddr_t		 natpt_pyldaddr		__P((struct ip6_hdr *, caddr_t, int *, struct ip6_frag **));
int		 natpt_setPrefix	__P((caddr_t));
int		 natpt_setValue		__P((caddr_t));
int		 natpt_getValue		__P((caddr_t));
int		 natpt_testLog		__P((caddr_t));
int		 natpt_break		__P((const char *));


/*  natpt_log.c */
void		 natpt_logTSlot		__P((int, struct tSlot *, char, int));
void		 natpt_logFSlot		__P((int, struct fragment *, char));
void		 natpt_logMsg		__P((int, char *, ...));
void		 natpt_logMBuf		__P((int, struct mbuf *, ...));
void		 natpt_logIp4		__P((int, struct ip *, ...));
void		 natpt_logIp6		__P((int, struct ip6_hdr *, ...));
int		 natpt_log		__P((int, int, void *, size_t));
int		 natpt_logIN6addr	__P((int, char *, struct in6_addr *));
struct mbuf	*natpt_lbuf		__P((int, int, size_t));
int		 natpt_ntop		__P((int, const void *, char *, size_t));


/*  natpt_rule.c */
struct cSlot	*natpt_lookForRule6	__P((struct pcv *));
struct sockaddr_in *natpt_reverseLookForRule6 __P((struct sockaddr_in6 *sin6));
struct cSlot	*natpt_lookForRule4	__P((struct pcv *));
int		 natpt_setRules		__P((caddr_t));
int		 natpt_openTemporaryRule __P((int, struct pAddr *, struct pAddr *));
int		 natpt_prependRule	__P((struct cSlot *));
int		 natpt_renumRules	__P((caddr_t));
int		 natpt_rmRules		__P((caddr_t));
int		 natpt_flushRules	__P((caddr_t));
int		 natpt_setOnOff		__P((int));
void		 natpt_init_rule	__P((void));


/*  natpt_trans.c */
struct mbuf	*natpt_translateIPv6To4	__P((struct pcv *, struct pAddr *));
struct mbuf	*natpt_translateIPv4To6	__P((struct pcv *, struct pAddr *));
struct mbuf	*natpt_translateIPv4To4	__P((struct pcv *, struct pAddr *));
struct mbuf	*natpt_translateFragment6    __P((struct pcv *, struct pAddr *));
struct mbuf	*natpt_translateFragment4to6 __P((struct pcv *, struct pAddr *));
struct mbuf	*natpt_translateFragment4to4 __P((struct pcv *, struct pAddr *));


/*  natpt_tslot.c */
struct tSlot	*natpt_lookForHash4	__P((struct pcv *));
struct tSlot	*natpt_lookForHash6	__P((struct pcv *));
struct tSlot	*natpt_internHash4	__P((struct cSlot *, struct pcv *));
struct tSlot	*natpt_internHash6	__P((struct cSlot *, struct pcv *));
struct tSlot	*natpt_openIncomingV4Conn __P((int, struct pAddr *, struct pAddr *));
struct tSlot	*natpt_checkICMP6return	__P((struct pcv *));
struct tSlot	*natpt_checkICMP	__P((struct pcv *));
struct pAddr	*natpt_remapRemote4Port	__P((struct cSlot *, struct pAddr *));

struct fragment	*natpt_internFragment6	__P((struct pcv *, struct tSlot *));
struct fragment	*natpt_internFragment4	__P((struct pcv *, struct tSlot *));
struct tSlot	*natpt_lookForFragment6	__P((struct pcv *));
struct tSlot	*natpt_lookForFragment4	__P((struct pcv *));
void		 natpt_removeFragmentEntry __P((struct fragment *));

int		 natpt_sessions		__P((caddr_t));
int		 natpt_xlate		__P((caddr_t));

void		 natpt_init_tslot	__P((void));


/*  natpt_usrreq.c */
void		 natpt_input		__P((struct mbuf *, struct sockproto *,
					     struct sockaddr *, struct sockaddr *));
