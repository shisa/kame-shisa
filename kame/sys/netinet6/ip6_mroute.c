/*	$KAME: ip6_mroute.c,v 1.130 2004/08/12 04:36:11 jinmei Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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

/*	BSDI ip_mroute.c,v 2.10 1996/11/14 00:29:52 jch Exp	*/

/*
 * Copyright (c) 1989 Stephen Deering
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      @(#)ip_mroute.c 8.2 (Berkeley) 11/15/93
 */

/*
 * IP multicast forwarding procedures
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenner, PARC, April 1994
 *
 * MROUTING Revision: 3.5.1.2 + PIM-SMv2 (pimd) Support
 */

/*
 * XXX it seems that home address option processing should be reverted
 * before calls to socket_send().  see sys/netinet6/dest6.c for details.
 */

#ifdef __FreeBSD__
#include "opt_inet.h"
#include "opt_inet6.h"
#endif
#ifdef __NetBSD__
#include "opt_inet.h"
#endif

#ifndef _KERNEL
# ifdef KERNEL
#  define _KERNEL
# endif
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#if defined(__NetBSD__) || defined(__FreeBSD__)
#include <sys/callout.h>
#elif defined(__OpenBSD__)
#include <sys/timeout.h>
#endif
#ifdef __FreeBSD__
#include <sys/malloc.h>
#endif
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#ifndef __FreeBSD__
#include <sys/ioctl.h>
#endif
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#ifdef MULTICAST_PMTUD
#include <netinet/icmp6.h>
#endif

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/pim6.h>
#include <netinet6/pim6_var.h>

#include <net/net_osdep.h>

#ifdef __FreeBSD__
static MALLOC_DEFINE(M_MRTABLE, "mf6c", "multicast forwarding cache entry");
#endif

#ifdef __FreeBSD__
#define M_READONLY(x)	(!M_WRITABLE(x))
#endif

static int ip6_mdq __P((struct mbuf *, struct ifnet *, struct mf6c *));
static void phyint_send __P((struct ip6_hdr *, struct mif6 *, struct mbuf *));

static int set_pim6 __P((int *));
#ifndef __FreeBSD__
static int get_pim6 __P((struct mbuf *));
#endif
static int socket_send __P((struct socket *, struct mbuf *,
	    struct sockaddr_in6 *));
static int register_send __P((struct ip6_hdr *, struct mif6 *, struct mbuf *));

/*
 * Globals.  All but ip6_mrouter, ip6_mrtproto and mrt6stat could be static,
 * except for netstat or debugging purposes.
 */
struct socket  *ip6_mrouter = NULL;
int		ip6_mrouter_ver = 0;
int		ip6_mrtproto = IPPROTO_PIM;    /* for netstat only */
struct mrt6stat	mrt6stat;

#define NO_RTE_FOUND 	0x1
#define RTE_FOUND	0x2

struct mf6c	*mf6ctable[MF6CTBLSIZ];
u_char		n6expire[MF6CTBLSIZ];
#ifdef __OpenBSD__
struct mif6 mif6table[MAXMIFS];
#else
static struct mif6 mif6table[MAXMIFS];
#endif
#ifdef MRT6DEBUG
u_int		mrt6debug = 0;	  /* debug level 	*/
#define DEBUG_MFC	0x02
#define DEBUG_FORWARD	0x04
#define DEBUG_EXPIRE	0x08
#define DEBUG_XMIT	0x10
#define DEBUG_REG	0x20
#define DEBUG_PIM	0x40
#endif

static void	expire_upcalls __P((void *));
#define	EXPIRE_TIMEOUT	(hz / 4)	/* 4x / second */
#define	UPCALL_EXPIRE	6		/* number of timeouts */

#ifdef INET
#ifdef MROUTING
extern struct socket *ip_mrouter;
#endif
#endif

extern struct domain inet6domain;

/*
 * 'Interfaces' associated with decapsulator (so we can tell
 * packets that went through it from ones that get reflected
 * by a broken gateway).  These interfaces are never linked into
 * the system ifnet list & no routes point to them.  I.e., packets
 * can't be sent this way.  They only exist as a placeholder for
 * multicast source verification.
 */
struct ifnet multicast_register_if;

#define ENCAP_HOPS 64

/*
 * Private variables.
 */
static mifi_t nummifs = 0;
static mifi_t reg_mif_num = (mifi_t)-1;

static struct pim6stat pim6stat;
static int pim6;

/*
 * Hash function for a source, group entry
 */
#define MF6CHASH(a, g) MF6CHASHMOD((a).s6_addr32[0] ^ (a).s6_addr32[1] ^ \
				   (a).s6_addr32[2] ^ (a).s6_addr32[3] ^ \
				   (g).s6_addr32[0] ^ (g).s6_addr32[1] ^ \
				   (g).s6_addr32[2] ^ (g).s6_addr32[3])

/*
 * Find a route for a given origin IPv6 address and Multicast group address.
 * Quality of service parameter to be added in the future!!!
 */

#define MF6CFIND(o, g, rt) do { \
	struct mf6c *_rt = mf6ctable[MF6CHASH(o,g)]; \
	rt = NULL; \
	mrt6stat.mrt6s_mfc_lookups++; \
	while (_rt) { \
		if (IN6_ARE_ADDR_EQUAL(&_rt->mf6c_origin.sin6_addr, &(o)) && \
		    IN6_ARE_ADDR_EQUAL(&_rt->mf6c_mcastgrp.sin6_addr, &(g)) && \
		    (_rt->mf6c_stall == NULL)) { \
			rt = _rt; \
			break; \
		} \
		_rt = _rt->mf6c_next; \
	} \
	if (rt == NULL) { \
		mrt6stat.mrt6s_mfc_misses++; \
	} \
} while (/*CONSTCOND*/ 0)

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) do { \
	    int xxs; \
		\
	    delta = (a).tv_usec - (b).tv_usec; \
	    if ((xxs = (a).tv_sec - (b).tv_sec)) { \
	       switch (xxs) { \
		      case 2: \
			  delta += 1000000; \
			      /* FALLTHROUGH */ \
		      case 1: \
			  delta += 1000000; \
			  break; \
		      default: \
			  delta += (1000000 * xxs); \
	       } \
	    } \
} while (/*CONSTCOND*/ 0)

#define TV_LT(a, b) (((a).tv_usec < (b).tv_usec && \
	      (a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

#ifdef UPCALL_TIMING
#define UPCALL_MAX	50
u_long upcall_data[UPCALL_MAX + 1];
static void collate();
#endif /* UPCALL_TIMING */

static int get_sg_cnt __P((struct sioc_sg_req6 *));
static int get_mif6_cnt __P((struct sioc_mif_req6 *));
static int ip6_mrouter_init __P((struct socket *, int, int));
static int add_m6if __P((struct mif6ctl *));
static int del_m6if __P((mifi_t *));
static int add_m6fc __P((struct mf6cctl *));
static int del_m6fc __P((struct mf6cctl *));

#ifdef __NetBSD__
static struct callout expire_upcalls_ch = CALLOUT_INITIALIZER;
#elif defined(__FreeBSD__)
static struct callout expire_upcalls_ch;
#elif defined(__OpenBSD__)
static struct timeout expire_upcalls_ch;
#endif

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
#ifdef __FreeBSD__
int
ip6_mrouter_set(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
{
	int error = 0;
	int optval;
	struct mif6ctl mifc;
	struct mf6cctl mfcc;
	mifi_t mifi;

	if (so != ip6_mrouter && sopt->sopt_name != MRT6_INIT)
		return (EACCES);

	switch (sopt->sopt_name) {
	case MRT6_INIT:
#ifdef MRT6_OINIT
	case MRT6_OINIT:
#endif
		error = sooptcopyin(sopt, &optval, sizeof(optval),
		    sizeof(optval));
		if (error)
			break;
		error = ip6_mrouter_init(so, optval, sopt->sopt_name);
		break;
	case MRT6_DONE:
		error = ip6_mrouter_done();
		break;
	case MRT6_ADD_MIF:
		error = sooptcopyin(sopt, &mifc, sizeof(mifc), sizeof(mifc));
		if (error)
			break;
		error = add_m6if(&mifc);
		break;
	case MRT6_ADD_MFC:
		error = sooptcopyin(sopt, &mfcc, sizeof(mfcc), sizeof(mfcc));
		if (error)
			break;
		error = add_m6fc(&mfcc);
		break;
	case MRT6_DEL_MFC:
		error = sooptcopyin(sopt, &mfcc, sizeof(mfcc), sizeof(mfcc));
		if (error)
			break;
		error = del_m6fc(&mfcc);
		break;
	case MRT6_DEL_MIF:
		error = sooptcopyin(sopt, &mifi, sizeof(mifi), sizeof(mifi));
		if (error)
			break;
		error = del_m6if(&mifi);
		break;
	case MRT6_PIM:
		error = sooptcopyin(sopt, &optval, sizeof(optval),
		    sizeof(optval));
		if (error)
			break;
		error = set_pim6(&optval);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}
#else
int
ip6_mrouter_set(cmd, so, m)
	int cmd;
	struct socket *so;
	struct mbuf *m;
{
	if (cmd != MRT6_INIT && so != ip6_mrouter)
		return (EACCES);

	switch (cmd) {
#ifdef MRT6_OINIT
	case MRT6_OINIT:
#endif
	case MRT6_INIT:
		if (m == NULL || m->m_len < sizeof(int))
			return (EINVAL);
		return (ip6_mrouter_init(so, *mtod(m, int *), cmd));
	case MRT6_DONE:
		return (ip6_mrouter_done());
	case MRT6_ADD_MIF:
		if (m == NULL || m->m_len < sizeof(struct mif6ctl))
			return (EINVAL);
		return (add_m6if(mtod(m, struct mif6ctl *)));
	case MRT6_DEL_MIF:
		if (m == NULL || m->m_len < sizeof(mifi_t))
			return (EINVAL);
		return (del_m6if(mtod(m, mifi_t *)));
	case MRT6_ADD_MFC:
		if (m == NULL || m->m_len < sizeof(struct mf6cctl))
			return (EINVAL);
		return (add_m6fc(mtod(m, struct mf6cctl *)));
	case MRT6_DEL_MFC:
		if (m == NULL || m->m_len < sizeof(struct mf6cctl))
			return (EINVAL);
		return (del_m6fc(mtod(m,  struct mf6cctl *)));
	case MRT6_PIM:
		if (m == NULL || m->m_len < sizeof(int))
			return (EINVAL);
		return (set_pim6(mtod(m, int *)));
	default:
		return (EOPNOTSUPP);
	}
}
#endif

/*
 * Handle MRT getsockopt commands
 */
#ifdef __FreeBSD__
int
ip6_mrouter_get(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
{
	int error = 0;

	if (so != ip6_mrouter)
		return (EACCES);

	switch (sopt->sopt_name) {
	case MRT6_PIM:
		error = sooptcopyout(sopt, &pim6, sizeof(pim6));
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}
#else
int
ip6_mrouter_get(cmd, so, m)
	int cmd;
	struct socket *so;
	struct mbuf **m;
{
	struct mbuf *mb;

	if (so != ip6_mrouter)
		return (EACCES);

	*m = mb = m_get(M_WAIT, MT_SOOPTS);

	switch (cmd) {
	case MRT6_PIM:
		return (get_pim6(mb));
	default:
		m_free(mb);
		return (EOPNOTSUPP);
	}
}
#endif

/*
 * Handle ioctl commands to obtain information from the cache
 */
int
mrt6_ioctl(cmd, data)
	int cmd;
	caddr_t data;
{

	switch (cmd) {
	case SIOCGETSGCNT_IN6:
		return (get_sg_cnt((struct sioc_sg_req6 *)data));
	case SIOCGETMIFCNT_IN6:
		return (get_mif6_cnt((struct sioc_mif_req6 *)data));
	default:
		return (EOPNOTSUPP);
	}
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
static int
get_sg_cnt(req)
	struct sioc_sg_req6 *req;
{
	struct mf6c *rt;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	MF6CFIND(req->src.sin6_addr, req->grp.sin6_addr, rt);
	splx(s);
	if (rt != NULL) {
		req->pktcnt = rt->mf6c_pkt_cnt;
		req->bytecnt = rt->mf6c_byte_cnt;
		req->wrong_if = rt->mf6c_wrong_if;
	} else
		return (ESRCH);
#if 0
		req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
#endif

	return (0);
}

/*
 * returns the input and output packet and byte counts on the mif provided
 */
static int
get_mif6_cnt(req)
	struct sioc_mif_req6 *req;
{
	mifi_t mifi = req->mifi;

	if (mifi >= nummifs)
		return (EINVAL);

	req->icount = mif6table[mifi].m6_pkt_in;
	req->ocount = mif6table[mifi].m6_pkt_out;
	req->ibytes = mif6table[mifi].m6_bytes_in;
	req->obytes = mif6table[mifi].m6_bytes_out;

	return (0);
}

#ifndef __FreeBSD__
/*
 * Get PIM processiong global
 */
static int
get_pim6(m)
	struct mbuf *m;
{
	int *i;

	i = mtod(m, int *);

	*i = pim6;

	return (0);
}
#endif

static int
set_pim6(i)
	int *i;
{
	if ((*i != 1) && (*i != 0))
		return (EINVAL);

	pim6 = *i;

	return (0);
}

/*
 * Enable multicast routing
 */
static int
ip6_mrouter_init(so, v, cmd)
	struct socket *so;
	int v;
	int cmd;
{
#ifdef MRT6DEBUG
	if (mrt6debug)
		log(LOG_DEBUG,
		    "ip6_mrouter_init: so_type = %d, pr_protocol = %d\n",
		    so->so_type, so->so_proto->pr_protocol);
#endif

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_ICMPV6)
		return (EOPNOTSUPP);

	if (v != 1)
		return (ENOPROTOOPT);

	if (ip6_mrouter != NULL)
		return (EADDRINUSE);

	ip6_mrouter = so;
	ip6_mrouter_ver = cmd;

	bzero((caddr_t)mf6ctable, sizeof(mf6ctable));
	bzero((caddr_t)n6expire, sizeof(n6expire));

	pim6 = 0;/* used for stubbing out/in pim stuff */

#if defined(__NetBSD__) || defined(__FreeBSD__)
	callout_reset(&expire_upcalls_ch, EXPIRE_TIMEOUT,
	    expire_upcalls, NULL);
#elif defined(__OpenBSD__)
	timeout_set(&expire_upcalls_ch, expire_upcalls, NULL);
	timeout_add(&expire_upcalls_ch, EXPIRE_TIMEOUT);
#else
	timeout(expire_upcalls, (caddr_t)NULL, EXPIRE_TIMEOUT);
#endif

#ifdef MRT6DEBUG
	if (mrt6debug)
		log(LOG_DEBUG, "ip6_mrouter_init\n");
#endif

	return (0);
}

/*
 * Disable multicast routing
 */
int
ip6_mrouter_done()
{
	mifi_t mifi;
	int i;
	struct ifnet *ifp;
	struct in6_ifreq ifr;
	struct mf6c *rt;
	struct rtdetq *rte;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif

	/*
	 * For each phyint in use, disable promiscuous reception of all IPv6
	 * multicasts.
	 */
#ifdef INET
#ifdef MROUTING
	/*
	 * If there is still IPv4 multicast routing daemon,
	 * we remain interfaces to receive all muliticasted packets.
	 * XXX: there may be an interface in which the IPv4 multicast
	 * daemon is not interested...
	 */
	if (!ip_mrouter)
#endif
#endif
	{
		for (mifi = 0; mifi < nummifs; mifi++) {
			if (mif6table[mifi].m6_ifp &&
			    !(mif6table[mifi].m6_flags & MIFF_REGISTER)) {
				ifr.ifr_addr.sin6_family = AF_INET6;
				ifr.ifr_addr.sin6_addr = in6addr_any;
				ifp = mif6table[mifi].m6_ifp;
				(*ifp->if_ioctl)(ifp, SIOCDELMULTI,
						 (caddr_t)&ifr);
			}
		}
	}
#ifdef notyet
	bzero((caddr_t)qtable, sizeof(qtable));
	bzero((caddr_t)tbftable, sizeof(tbftable));
#endif
	bzero((caddr_t)mif6table, sizeof(mif6table));
	nummifs = 0;

	pim6 = 0; /* used to stub out/in pim specific code */

#if defined(__NetBSD__) || defined(__FreeBSD__)
	callout_stop(&expire_upcalls_ch);
#elif defined(__OpenBSD__)
	timeout_del(&expire_upcalls_ch);
#else
	untimeout(expire_upcalls, (caddr_t)NULL);
#endif

	/*
	 * Free all multicast forwarding cache entries.
	 */
	for (i = 0; i < MF6CTBLSIZ; i++) {
		rt = mf6ctable[i];
		while (rt) {
			struct mf6c *frt;

			for (rte = rt->mf6c_stall; rte != NULL; ) {
				struct rtdetq *n = rte->next;

				m_free(rte->m);
				free(rte, M_MRTABLE);
				rte = n;
			}
			frt = rt;
			rt = rt->mf6c_next;
			free(frt, M_MRTABLE);
		}
	}

	bzero((caddr_t)mf6ctable, sizeof(mf6ctable));

	/*
	 * Reset register interface
	 */
	if (inet6domain.dom_ifdetach) {
		ifp = &multicast_register_if;
		if (ifp->if_afdata[AF_INET6])
			inet6domain.dom_ifdetach(ifp, ifp->if_afdata[AF_INET6]);
		ifp->if_afdata[AF_INET6] = NULL;
	}
	reg_mif_num = -1;

	ip6_mrouter = NULL;
	ip6_mrouter_ver = 0;

	splx(s);

#ifdef MRT6DEBUG
	if (mrt6debug)
		log(LOG_DEBUG, "ip6_mrouter_done\n");
#endif

	return (0);
}

void
ip6_mrouter_detach(ifp)
	struct ifnet *ifp;
{
	struct rtdetq *rte;
	struct mf6c *mfc;
	mifi_t mifi;
	int i;

	if (ip6_mrouter == NULL)
		return;

	/*
	 * Delete a mif which points to ifp.
	 */
	for (mifi = 0; mifi < nummifs; mifi++)
		if (mif6table[mifi].m6_ifp == ifp)
			del_m6if(&mifi);

	/*
	 * Clear rte->ifp of cache entries received on ifp.
	 */
	for (i = 0; i < MF6CTBLSIZ; i++) {
		if (n6expire[i] == 0)
			continue;

		for (mfc = mf6ctable[i]; mfc != NULL; mfc = mfc->mf6c_next) {
			for (rte = mfc->mf6c_stall; rte != NULL; rte = rte->next) {
				if (rte->ifp == ifp)
					rte->ifp = NULL;
			}
		}
	}
}

static struct sockaddr_in6 sin6 = { sizeof(sin6), AF_INET6 };

/*
 * Add a mif to the mif table
 */
static int
add_m6if(mifcp)
	struct mif6ctl *mifcp;
{
	struct mif6 *mifp;
	struct ifnet *ifp;
#ifndef __FreeBSD__
	struct in6_ifreq ifr;
#endif
	int error, s;
#ifdef notyet
	struct tbf *m_tbf = tbftable + mifcp->mif6c_mifi;
#endif

	if (mifcp->mif6c_mifi >= MAXMIFS)
		return (EINVAL);
	mifp = mif6table + mifcp->mif6c_mifi;
	if (mifp->m6_ifp)
		return (EADDRINUSE); /* XXX: is it appropriate? */
	if (mifcp->mif6c_pifi == 0 || mifcp->mif6c_pifi >= if_indexlim)
		return (ENXIO);
	/*
	 * XXX: some OSes can remove ifp and clear ifindex2ifnet[id]
	 * even for id between 0 and if_index.
	 */
#if defined(__FreeBSD__) && __FreeBSD__ >= 5
	ifp = ifnet_byindex(mifcp->mif6c_pifi);
#else
	ifp = ifindex2ifnet[mifcp->mif6c_pifi];
#endif
	if (ifp == NULL)
		return (ENXIO);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif

	if (mifcp->mif6c_flags & MIFF_REGISTER) {
		ifp = &multicast_register_if;

		if (reg_mif_num == (mifi_t)-1) {
#if defined(__NetBSD__) || defined(__OpenBSD__) 
			strlcpy(ifp->if_xname, "register_mif",
			    sizeof(ifp->if_xname));
#elif defined(__FreeBSD__) && __FreeBSD_version > 502000
			if_initname(ifp, "register_mif", 0);
#else
			ifp->if_name = "register_mif";
#endif
			ifp->if_flags |= IFF_LOOPBACK;
			ifp->if_index = mifcp->mif6c_mifi;
			reg_mif_num = mifcp->mif6c_mifi;
			if (inet6domain.dom_ifattach) {
				ifp->if_afdata[AF_INET6]
				    = inet6domain.dom_ifattach(ifp);
			}
		}

	} /* if REGISTER */
	else {
		/* Make sure the interface supports multicast */
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return (EOPNOTSUPP);

#ifdef __FreeBSD__
		error = if_allmulti(ifp, 1);
#else
		/*
		 * Enable promiscuous reception of all IPv6 multicasts
		 * from the interface.
		 */
		ifr.ifr_addr.sin6_family = AF_INET6;
		ifr.ifr_addr.sin6_addr = in6addr_any;
		error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);
#endif
		if (error) {
			splx(s);
			return (error);
		}
	}

	mifp->m6_flags     = mifcp->mif6c_flags;
	mifp->m6_ifp       = ifp;
#ifdef notyet
	/* scaling up here allows division by 1024 in critical code */
	mifp->m6_rate_limit = mifcp->mif6c_rate_limit * 1024 / 1000;
#endif
	/* initialize per mif pkt counters */
	mifp->m6_pkt_in    = 0;
	mifp->m6_pkt_out   = 0;
	mifp->m6_bytes_in  = 0;
	mifp->m6_bytes_out = 0;

	/* Adjust nummifs up if the mifi is higher than nummifs */
	if (nummifs <= mifcp->mif6c_mifi)
		nummifs = mifcp->mif6c_mifi + 1;

#ifdef MRT6DEBUG
	if (mrt6debug)
		log(LOG_DEBUG,
#if defined(__NetBSD__) || defined(__OpenBSD__)
		    "add_mif #%d, phyint %s\n",
		    mifcp->mif6c_mifi,
		    ifp->if_xname);
#else
		    "add_mif #%d, phyint %s%d\n",
		    mifcp->mif6c_mifi,
		    ifp->if_name, ifp->if_unit);
#endif
#endif

	splx(s);

	return (0);
}

/*
 * Delete a mif from the mif table
 */
static int
del_m6if(mifip)
	mifi_t *mifip;
{
	struct mif6 *mifp = mif6table + *mifip;
	mifi_t mifi;
	struct ifnet *ifp;
#ifndef __FreeBSD__
	struct in6_ifreq ifr;
#endif
	int s;

	if (*mifip >= nummifs)
		return (EINVAL);
	if (mifp->m6_ifp == NULL)
		return (EINVAL);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif

	if ((mifp->m6_flags & MIFF_REGISTER) && reg_mif_num != (mifi_t) -1) {
		reg_mif_num = -1;
		if (inet6domain.dom_ifdetach) {
			ifp = &multicast_register_if;
			inet6domain.dom_ifdetach(ifp, ifp->if_afdata[AF_INET6]);
			ifp->if_afdata[AF_INET6] = NULL;
		}
	}
	if (!(mifp->m6_flags & MIFF_REGISTER)) {
		/*
		 * XXX: what if there is yet IPv4 multicast daemon
		 *      using the interface?
		 */
		ifp = mifp->m6_ifp;

#ifdef __FreeBSD__
		if_allmulti(ifp, 0);
#else
		ifr.ifr_addr.sin6_family = AF_INET6;
		ifr.ifr_addr.sin6_addr = in6addr_any;
		(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
#endif
	}

#ifdef notyet
	bzero((caddr_t)qtable[*mifip], sizeof(qtable[*mifip]));
	bzero((caddr_t)mifp->m6_tbf, sizeof(*(mifp->m6_tbf)));
#endif
	bzero((caddr_t)mifp, sizeof(*mifp));

	/* Adjust nummifs down */
	for (mifi = nummifs; mifi > 0; mifi--)
		if (mif6table[mifi - 1].m6_ifp)
			break;
	nummifs = mifi;

	splx(s);

#ifdef MRT6DEBUG
	if (mrt6debug)
		log(LOG_DEBUG, "del_m6if %d, nummifs %d\n", *mifip, nummifs);
#endif

	return (0);
}

/*
 * Add an mfc entry
 */
static int
add_m6fc(mfccp)
	struct mf6cctl *mfccp;
{
	struct mf6c *rt;
	u_long hash;
	struct rtdetq *rte;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	MF6CFIND(mfccp->mf6cc_origin.sin6_addr,
		 mfccp->mf6cc_mcastgrp.sin6_addr, rt);

	/* If an entry already exists, just update the fields */
	if (rt) {
#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_MFC)
			log(LOG_DEBUG,"add_m6fc update o %s g %s p %x\n",
			    ip6_sprintf(&mfccp->mf6cc_origin.sin6_addr),
			    ip6_sprintf(&mfccp->mf6cc_mcastgrp.sin6_addr),
			    mfccp->mf6cc_parent);
#endif

		rt->mf6c_parent = mfccp->mf6cc_parent;
		rt->mf6c_ifset = mfccp->mf6cc_ifset;
		splx(s);
		return (0);
	}

	/*
	 * Find the entry for which the upcall was made and update
	 */
	hash = MF6CHASH(mfccp->mf6cc_origin.sin6_addr,
			mfccp->mf6cc_mcastgrp.sin6_addr);
	for (rt = mf6ctable[hash]; rt; rt = rt->mf6c_next) {
		if (IN6_ARE_ADDR_EQUAL(&rt->mf6c_origin.sin6_addr,
				       &mfccp->mf6cc_origin.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&rt->mf6c_mcastgrp.sin6_addr,
				       &mfccp->mf6cc_mcastgrp.sin6_addr) &&
		    (rt->mf6c_stall != NULL)) {
#ifdef MRT6DEBUG
			if (mrt6debug & DEBUG_MFC)
				log(LOG_DEBUG,
				    "add_m6fc o %s g %s p %x dbg %x\n",
				    ip6_sprintf(&mfccp->mf6cc_origin.sin6_addr),
				    ip6_sprintf(&mfccp->mf6cc_mcastgrp.sin6_addr),
				    mfccp->mf6cc_parent, rt->mf6c_stall);
#endif

			rt->mf6c_origin     = mfccp->mf6cc_origin;
			rt->mf6c_mcastgrp   = mfccp->mf6cc_mcastgrp;
			rt->mf6c_parent     = mfccp->mf6cc_parent;
			rt->mf6c_ifset	    = mfccp->mf6cc_ifset;
			/* initialize pkt counters per src-grp */
			rt->mf6c_pkt_cnt    = 0;
			rt->mf6c_byte_cnt   = 0;
			rt->mf6c_wrong_if   = 0;

			rt->mf6c_expire = 0;	/* Don't clean this guy up */
			n6expire[hash]--;

			/* free packets Qed at the end of this entry */
			for (rte = rt->mf6c_stall; rte != NULL; ) {
				struct rtdetq *n = rte->next;
				if (rte->ifp) {
					ip6_mdq(rte->m, rte->ifp, rt);
				}
				m_freem(rte->m);
#ifdef UPCALL_TIMING
				collate(&(rte->t));
#endif /* UPCALL_TIMING */
				free(rte, M_MRTABLE);
				rte = n;
			}
			rt->mf6c_stall = NULL;

			break;
		}
	}

	/*
	 * It is possible that an entry is being inserted without an upcall
	 */
	if (rt == NULL) {
#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_MFC)
			log(LOG_DEBUG,
			    "add_m6fc no upcall h %d o %s g %s p %x\n",
			    hash,
			    ip6_sprintf(&mfccp->mf6cc_origin.sin6_addr),
			    ip6_sprintf(&mfccp->mf6cc_mcastgrp.sin6_addr),
			    mfccp->mf6cc_parent);
#endif
		/* no upcall, so make a new entry */
		rt = (struct mf6c *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
		if (rt == NULL) {
			splx(s);
			return (ENOBUFS);
		}

		/* insert new entry at head of hash chain */
		rt->mf6c_origin     = mfccp->mf6cc_origin;
		rt->mf6c_mcastgrp   = mfccp->mf6cc_mcastgrp;
		rt->mf6c_parent     = mfccp->mf6cc_parent;
		rt->mf6c_ifset	    = mfccp->mf6cc_ifset;
		/* initialize pkt counters per src-grp */
		rt->mf6c_pkt_cnt    = 0;
		rt->mf6c_byte_cnt   = 0;
		rt->mf6c_wrong_if   = 0;
		rt->mf6c_expire     = 0;
		rt->mf6c_stall = NULL;

		/* link into table */
		rt->mf6c_next  = mf6ctable[hash];
		mf6ctable[hash] = rt;
	}
	splx(s);
	return (0);
}

#ifdef UPCALL_TIMING
/*
 * collect delay statistics on the upcalls
 */
static void
collate(t)
	struct timeval *t;
{
	u_long d;
	struct timeval tp;
	u_long delta;

	GET_TIME(tp);

	if (TV_LT(*t, tp))
	{
		TV_DELTA(tp, *t, delta);

		d = delta >> 10;
		if (d > UPCALL_MAX)
			d = UPCALL_MAX;

		++upcall_data[d];
	}
}
#endif /* UPCALL_TIMING */

/*
 * Delete an mfc entry
 */
static int
del_m6fc(mfccp)
	struct mf6cctl *mfccp;
{
	struct sockaddr_in6 	origin;
	struct sockaddr_in6 	mcastgrp;
	struct mf6c 		*rt;
	struct mf6c	 	**nptr;
	u_long 		hash;
	int s;

	origin = mfccp->mf6cc_origin;
	mcastgrp = mfccp->mf6cc_mcastgrp;
	hash = MF6CHASH(origin.sin6_addr, mcastgrp.sin6_addr);

#ifdef MRT6DEBUG
	if (mrt6debug & DEBUG_MFC)
		log(LOG_DEBUG,"del_m6fc orig %s mcastgrp %s\n",
		    ip6_sprintf(&origin.sin6_addr),
		    ip6_sprintf(&mcastgrp.sin6_addr));
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif

	nptr = &mf6ctable[hash];
	while ((rt = *nptr) != NULL) {
		if (IN6_ARE_ADDR_EQUAL(&origin.sin6_addr,
				       &rt->mf6c_origin.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&mcastgrp.sin6_addr,
				       &rt->mf6c_mcastgrp.sin6_addr) &&
		    rt->mf6c_stall == NULL)
			break;

		nptr = &rt->mf6c_next;
	}
	if (rt == NULL) {
		splx(s);
		return (EADDRNOTAVAIL);
	}

	*nptr = rt->mf6c_next;
	free(rt, M_MRTABLE);

	splx(s);

	return (0);
}

static int
socket_send(s, mm, src)
	struct socket *s;
	struct mbuf *mm;
	struct sockaddr_in6 *src;
{
	/*
	 * XXX: src may be an invalid address wrt the scope zone.  However,
	 * this doesn't actually matter because pim6[sd]d don't care about
	 * the "dummy" source address. 
	 */
	if (s) {
		if (sbappendaddr(&s->so_rcv,
				 (struct sockaddr *)src,
				 mm, (struct mbuf *)0) != 0) {
			sorwakeup(s);
			return (0);
		}
	}
	m_freem(mm);
	return (-1);
}

/*
 * IPv6 multicast forwarding function. This function assumes that the packet
 * pointed to by "ip6" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IPv6 multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */

int
ip6_mforward(ip6, ifp, m)
	struct ip6_hdr *ip6;
	struct ifnet *ifp;
	struct mbuf *m;
{
	struct mf6c *rt;
	struct mif6 *mifp;
	struct mbuf *mm;
	int s;
	mifi_t mifi;
#ifndef __FreeBSD__
	long time_second = time.tv_sec;
#endif

#ifdef MRT6DEBUG
	if (mrt6debug & DEBUG_FORWARD)
		log(LOG_DEBUG, "ip6_mforward: src %s, dst %s, ifindex %d\n",
		    ip6_sprintf(&ip6->ip6_src), ip6_sprintf(&ip6->ip6_dst),
		    ifp->if_index);
#endif

	/*
	 * Don't forward a packet with Hop limit of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (ip6->ip6_hlim <= 1 || IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&ip6->ip6_dst))
		return (0);
	ip6->ip6_hlim--;

	/*
	 * Source address check: do not forward packets with unspecified
	 * source. It was discussed in July 2000, on ipngwg mailing list.
	 * This is rather more serious than unicast cases, because some
	 * MLD packets can be sent with the unspecified source address
	 * (although such packets must normally set the hop limit field to 1).
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
		ip6stat.ip6s_cantforward++;
		if (ip6_log_time + ip6_log_interval < time_second) {
			ip6_log_time = time_second;
			log(LOG_DEBUG,
			    "cannot forward "
			    "from %s to %s nxt %d received on %s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst),
			    ip6->ip6_nxt,
			    m->m_pkthdr.rcvif ?
			    if_name(m->m_pkthdr.rcvif) : "?");
		}
		return (0);
	}

	/*
	 * Determine forwarding mifs from the forwarding cache table
	 */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	MF6CFIND(ip6->ip6_src, ip6->ip6_dst, rt);

	/* Entry exists, so forward if necessary */
	if (rt) {
		splx(s);
		return (ip6_mdq(m, ifp, rt));
	} else {
		/*
		 * If we don't have a route for packet's origin,
		 * Make a copy of the packet &
		 * send message to routing daemon
		 */

		struct mbuf *mb0;
		struct rtdetq *rte;
		u_long hash;
/*		int i, npkts;*/
#ifdef UPCALL_TIMING
		struct timeval tp;

		GET_TIME(tp);
#endif /* UPCALL_TIMING */

		mrt6stat.mrt6s_no_route++;
#ifdef MRT6DEBUG
		if (mrt6debug & (DEBUG_FORWARD | DEBUG_MFC))
			log(LOG_DEBUG, "ip6_mforward: no rte s %s g %s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst));
#endif

		/*
		 * Allocate mbufs early so that we don't do extra work if we
		 * are just going to fail anyway.
		 */
		rte = (struct rtdetq *)malloc(sizeof(*rte), M_MRTABLE,
					      M_NOWAIT);
		if (rte == NULL) {
			splx(s);
			return (ENOBUFS);
		}
		mb0 = m_copy(m, 0, M_COPYALL);
		/*
		 * Pullup packet header if needed before storing it,
		 * as other references may modify it in the meantime.
		 */
		if (mb0 &&
		    (M_READONLY(mb0) || mb0->m_len < sizeof(struct ip6_hdr)))
			mb0 = m_pullup(mb0, sizeof(struct ip6_hdr));
		if (mb0 == NULL) {
			free(rte, M_MRTABLE);
			splx(s);
			return (ENOBUFS);
		}

		/* is there an upcall waiting for this packet? */
		hash = MF6CHASH(ip6->ip6_src, ip6->ip6_dst);
		for (rt = mf6ctable[hash]; rt; rt = rt->mf6c_next) {
			if (IN6_ARE_ADDR_EQUAL(&ip6->ip6_src,
					       &rt->mf6c_origin.sin6_addr) &&
			    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
					       &rt->mf6c_mcastgrp.sin6_addr) &&
			    (rt->mf6c_stall != NULL))
				break;
		}

		if (rt == NULL) {
			struct mrt6msg *im;
#ifdef MRT6_OINIT
			struct omrt6msg *oim;
#endif

			/* no upcall, so make a new entry */
			rt = (struct mf6c *)malloc(sizeof(*rt), M_MRTABLE,
						  M_NOWAIT);
			if (rt == NULL) {
				free(rte, M_MRTABLE);
				m_freem(mb0);
				splx(s);
				return (ENOBUFS);
			}
			/*
			 * Make a copy of the header to send to the user
			 * level process
			 */
			mm = m_copy(mb0, 0, sizeof(struct ip6_hdr));

			if (mm == NULL) {
				free(rte, M_MRTABLE);
				m_freem(mb0);
				free(rt, M_MRTABLE);
				splx(s);
				return (ENOBUFS);
			}

			/*
			 * Send message to routing daemon
			 */
			sin6.sin6_addr = ip6->ip6_src;

			im = NULL;
#ifdef MRT6_OINIT
			oim = NULL;
#endif
			switch (ip6_mrouter_ver) {
#ifdef MRT6_OINIT
			case MRT6_OINIT:
				oim = mtod(mm, struct omrt6msg *);
				oim->im6_msgtype = MRT6MSG_NOCACHE;
				oim->im6_mbz = 0;
				break;
#endif
			case MRT6_INIT:
				im = mtod(mm, struct mrt6msg *);
				im->im6_msgtype = MRT6MSG_NOCACHE;
				im->im6_mbz = 0;
				break;
			default:
				free(rte, M_MRTABLE);
				m_freem(mb0);
				free(rt, M_MRTABLE);
				splx(s);
				return (EINVAL);
			}

#ifdef MRT6DEBUG
			if (mrt6debug & DEBUG_FORWARD)
				log(LOG_DEBUG,
				    "getting the iif info in the kernel\n");
#endif

			for (mifp = mif6table, mifi = 0;
			     mifi < nummifs && mifp->m6_ifp != ifp;
			     mifp++, mifi++)
				;

			switch (ip6_mrouter_ver) {
#ifdef MRT6_OINIT
			case MRT6_OINIT:
				oim->im6_mif = mifi;
				break;
#endif
			case MRT6_INIT:
				im->im6_mif = mifi;
				break;
			}

			if (socket_send(ip6_mrouter, mm, &sin6) < 0) {
				log(LOG_WARNING, "ip6_mforward: ip6_mrouter "
				    "socket queue full\n");
				mrt6stat.mrt6s_upq_sockfull++;
				free(rte, M_MRTABLE);
				m_freem(mb0);
				free(rt, M_MRTABLE);
				splx(s);
				return (ENOBUFS);
			}

			mrt6stat.mrt6s_upcalls++;

			/* insert new entry at head of hash chain */
			bzero(rt, sizeof(*rt));
			rt->mf6c_origin.sin6_family = AF_INET6;
			rt->mf6c_origin.sin6_len = sizeof(struct sockaddr_in6);
			rt->mf6c_origin.sin6_addr = ip6->ip6_src;
			rt->mf6c_mcastgrp.sin6_family = AF_INET6;
			rt->mf6c_mcastgrp.sin6_len = sizeof(struct sockaddr_in6);
			rt->mf6c_mcastgrp.sin6_addr = ip6->ip6_dst;
			rt->mf6c_expire = UPCALL_EXPIRE;
			n6expire[hash]++;
			rt->mf6c_parent = MF6C_INCOMPLETE_PARENT;

			/* link into table */
			rt->mf6c_next  = mf6ctable[hash];
			mf6ctable[hash] = rt;
			/* Add this entry to the end of the queue */
			rt->mf6c_stall = rte;
		} else {
			/* determine if q has overflowed */
			struct rtdetq **p;
			int npkts = 0;

			for (p = &rt->mf6c_stall; *p != NULL; p = &(*p)->next)
				if (++npkts > MAX_UPQ6) {
					mrt6stat.mrt6s_upq_ovflw++;
					free(rte, M_MRTABLE);
					m_freem(mb0);
					splx(s);
					return (0);
				}

			/* Add this entry to the end of the queue */
			*p = rte;
		}

		rte->next = NULL;
		rte->m = mb0;
		rte->ifp = ifp;
#ifdef UPCALL_TIMING
		rte->t = tp;
#endif /* UPCALL_TIMING */

		splx(s);

		return (0);
	}
}

/*
 * Clean up cache entries if upcalls are not serviced
 * Call from the Slow Timeout mechanism, every 0.25 seconds.
 */
static void
expire_upcalls(unused)
	void *unused;
{
	struct rtdetq *rte;
	struct mf6c *mfc, **nptr;
	int i;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	for (i = 0; i < MF6CTBLSIZ; i++) {
		if (n6expire[i] == 0)
			continue;
		nptr = &mf6ctable[i];
		while ((mfc = *nptr) != NULL) {
			rte = mfc->mf6c_stall;
			/*
			 * Skip real cache entries
			 * Make sure it wasn't marked to not expire (shouldn't happen)
			 * If it expires now
			 */
			if (rte != NULL &&
			    mfc->mf6c_expire != 0 &&
			    --mfc->mf6c_expire == 0) {
#ifdef MRT6DEBUG
				if (mrt6debug & DEBUG_EXPIRE)
					log(LOG_DEBUG, "expire_upcalls: expiring (%s %s)\n",
					    ip6_sprintf(&mfc->mf6c_origin.sin6_addr),
					    ip6_sprintf(&mfc->mf6c_mcastgrp.sin6_addr));
#endif
				/*
				 * drop all the packets
				 * free the mbuf with the pkt, if, timing info
				 */
				do {
					struct rtdetq *n = rte->next;
					m_freem(rte->m);
					free(rte, M_MRTABLE);
					rte = n;
				} while (rte != NULL);
				mrt6stat.mrt6s_cache_cleanups++;
				n6expire[i]--;

				*nptr = mfc->mf6c_next;
				free(mfc, M_MRTABLE);
			} else {
				nptr = &mfc->mf6c_next;
			}
		}
	}
	splx(s);
#if defined(__NetBSD__) || defined(__FreeBSD__)
	callout_reset(&expire_upcalls_ch, EXPIRE_TIMEOUT,
	    expire_upcalls, NULL);
#elif defined(__OpenBSD__)
	timeout_set(&expire_upcalls_ch, expire_upcalls, NULL);
	timeout_add(&expire_upcalls_ch, EXPIRE_TIMEOUT);
#else
	timeout(expire_upcalls, (caddr_t)NULL, EXPIRE_TIMEOUT);
#endif
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
ip6_mdq(m, ifp, rt)
	struct mbuf *m;
	struct ifnet *ifp;
	struct mf6c *rt;
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	mifi_t mifi, iif;
	struct mif6 *mifp;
	int plen = m->m_pkthdr.len;

/*
 * Macro to send packet on mif.  Since RSVP packets don't get counted on
 * input, they shouldn't get counted on output, so statistics keeping is
 * separate.
 */

#define MC6_SEND(ip6, mifp, m) do {				\
	if ((mifp)->m6_flags & MIFF_REGISTER)			\
		register_send((ip6), (mifp), (m));		\
	else							\
		phyint_send((ip6), (mifp), (m));		\
} while (/*CONSTCOND*/ 0)

	/*
	 * Don't forward if it didn't arrive from the parent mif
	 * for its origin.
	 */
	mifi = rt->mf6c_parent;
	if ((mifi >= nummifs) || (mif6table[mifi].m6_ifp != ifp)) {
		/* came in the wrong interface */
#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_FORWARD)
			log(LOG_DEBUG,
			    "wrong if: ifid %d mifi %d mififid %x\n",
			    ifp->if_index, mifi,
			    mif6table[mifi].m6_ifp ?
			    mif6table[mifi].m6_ifp->if_index : -1); 
#endif
		mrt6stat.mrt6s_wrong_if++;
		rt->mf6c_wrong_if++;
		/*
		 * If we are doing PIM processing, and we are forwarding
		 * packets on this interface, send a message to the
		 * routing daemon.
		 */
		/* have to make sure this is a valid mif */
		if (mifi < nummifs && mif6table[mifi].m6_ifp)
			if (pim6 && (m->m_flags & M_LOOP) == 0) {
				/*
				 * Check the M_LOOP flag to avoid an
				 * unnecessary PIM assert.
				 * XXX: M_LOOP is an ad-hoc hack...
				 */
				static struct sockaddr_in6 sin6 =
				{ sizeof(sin6), AF_INET6 };

				struct mbuf *mm;
				struct mrt6msg *im;
#ifdef MRT6_OINIT
				struct omrt6msg *oim;
#endif

				mm = m_copy(m, 0, sizeof(struct ip6_hdr));
				if (mm &&
				    (M_READONLY(mm) ||
				     mm->m_len < sizeof(struct ip6_hdr)))
					mm = m_pullup(mm, sizeof(struct ip6_hdr));
				if (mm == NULL)
					return (ENOBUFS);

#ifdef MRT6_OINIT
				oim = NULL;
#endif
				im = NULL;
				switch (ip6_mrouter_ver) {
#ifdef MRT6_OINIT
				case MRT6_OINIT:
					oim = mtod(mm, struct omrt6msg *);
					oim->im6_msgtype = MRT6MSG_WRONGMIF;
					oim->im6_mbz = 0;
					break;
#endif
				case MRT6_INIT:
					im = mtod(mm, struct mrt6msg *);
					im->im6_msgtype = MRT6MSG_WRONGMIF;
					im->im6_mbz = 0;
					break;
				default:
					m_freem(mm);
					return (EINVAL);
				}

				for (mifp = mif6table, iif = 0;
				    iif < nummifs && mifp &&
				    mifp->m6_ifp != ifp;
				    mifp++, iif++)
					;

				switch (ip6_mrouter_ver) {
#ifdef MRT6_OINIT
				case MRT6_OINIT:
					oim->im6_mif = iif;
					sin6.sin6_addr = oim->im6_src;
					break;
#endif
				case MRT6_INIT:
					im->im6_mif = iif;
					sin6.sin6_addr = im->im6_src;
					break;
				}

				mrt6stat.mrt6s_upcalls++;

				if (socket_send(ip6_mrouter, mm, &sin6) < 0) {
#ifdef MRT6DEBUG
					if (mrt6debug)
						log(LOG_WARNING, "mdq, ip6_mrouter socket queue full\n");
#endif
					++mrt6stat.mrt6s_upq_sockfull;
					return (ENOBUFS);
				}	/* if socket Q full */
			}		/* if PIM */
		return (0);
	}			/* if wrong iif */

	/* If I sourced this packet, it counts as output, else it was input. */
	if (m->m_pkthdr.rcvif == NULL) {
		/* XXX: is rcvif really NULL when output?? */
		mif6table[mifi].m6_pkt_out++;
		mif6table[mifi].m6_bytes_out += plen;
	} else {
		mif6table[mifi].m6_pkt_in++;
		mif6table[mifi].m6_bytes_in += plen;
	}
	rt->mf6c_pkt_cnt++;
	rt->mf6c_byte_cnt += plen;

	/*
	 * For each mif, forward a copy of the packet if there are group
	 * members downstream on the interface.
	 */
	for (mifp = mif6table, mifi = 0; mifi < nummifs; mifp++, mifi++) {
		if (IF_ISSET(mifi, &rt->mf6c_ifset)) {
			struct sockaddr_in6 src_sa, dst_sa;
			u_int32_t sscopeout, dscopeout;

			if (mif6table[mifi].m6_ifp == NULL)
				continue;
			/*
			 * check if the outgoing packet is going to break
			 * a scope boundary.
			 * XXX For packets through PIM register tunnel
			 * interface, we believe a routing daemon.
			 */
			bzero(&src_sa, sizeof(src_sa));
			src_sa.sin6_family = AF_INET6;
			src_sa.sin6_len = sizeof(src_sa);
			if (in6_recoverscope(&src_sa, &ip6->ip6_src,
			    m->m_pkthdr.rcvif)) {
				ip6stat.ip6s_badscope++;
				continue;
			}
			
			bzero(&dst_sa, sizeof(dst_sa));
			dst_sa.sin6_family = AF_INET6;
			dst_sa.sin6_len = sizeof(dst_sa);
			if (in6_recoverscope(&dst_sa, &ip6->ip6_dst,
			    m->m_pkthdr.rcvif)) {
				ip6stat.ip6s_badscope++;
				continue;
			}
			
			if (!(mif6table[rt->mf6c_parent].m6_flags &
			      MIFF_REGISTER) &&
			    !(mif6table[mifi].m6_flags & MIFF_REGISTER)) {
				if (in6_addr2zoneid(mif6table[mifi].m6_ifp,
				    &ip6->ip6_dst, &dscopeout) ||
				    in6_addr2zoneid(mif6table[mifi].m6_ifp,
				    &ip6->ip6_src, &sscopeout) ||
				    dst_sa.sin6_scope_id != dscopeout ||
				    src_sa.sin6_scope_id != sscopeout) {
					ip6stat.ip6s_badscope++;
					continue;
				}
			}

			mifp->m6_pkt_out++;
			mifp->m6_bytes_out += plen;
			MC6_SEND(ip6, mifp, m);
		}
	}
	return (0);
}

static void
phyint_send(ip6, mifp, m)
	struct ip6_hdr *ip6;
	struct mif6 *mifp;
	struct mbuf *m;
{
	struct mbuf *mb_copy;
	struct ifnet *ifp = mifp->m6_ifp;
	int error = 0;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int s = splsoftnet();	/* needs to protect static "ro" below. */
#else
	int s = splnet();	/* needs to protect static "ro" below. */
#endif
#ifdef NEW_STRUCT_ROUTE
	static struct route ro;
#else
	static struct route_in6 ro;
#endif
	struct in6_multi *in6m;
	struct sockaddr_in6 dst6;
	u_long linkmtu;

	/*
	 * Make a new reference to the packet; make sure that
	 * the IPv6 header is actually copied, not just referenced,
	 * so that ip6_output() only scribbles on the copy.
	 */
	mb_copy = m_copy(m, 0, M_COPYALL);
	if (mb_copy &&
	    (M_READONLY(mb_copy) || mb_copy->m_len < sizeof(struct ip6_hdr)))
		mb_copy = m_pullup(mb_copy, sizeof(struct ip6_hdr));
	if (mb_copy == NULL) {
		splx(s);
		return;
	}
	/* set MCAST flag to the outgoing packet */
	mb_copy->m_flags |= M_MCAST;

	/*
	 * If we sourced the packet, call ip6_output since we may divide
	 * the packet into fragments when the packet is too big for the
	 * outgoing interface.
	 * Otherwise, we can simply send the packet to the interface
	 * sending queue.
	 */
	if (m->m_pkthdr.rcvif == NULL) {
		struct ip6_moptions im6o;

		im6o.im6o_multicast_ifp = ifp;
		/* XXX: ip6_output will override ip6->ip6_hlim */
		im6o.im6o_multicast_hlim = ip6->ip6_hlim;
		im6o.im6o_multicast_loop = 1;
		error = ip6_output(mb_copy, NULL, &ro,
				   IPV6_FORWARDING, &im6o, NULL
#if defined(__FreeBSD__) && __FreeBSD_version >= 480000
				   , NULL
#endif
				  );

#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_XMIT)
			log(LOG_DEBUG, "phyint_send on mif %d err %d\n",
			    mifp - mif6table, error);
#endif
		splx(s);
		return;
	}

	/*
	 * If we belong to the destination multicast group
	 * on the outgoing interface, loop back a copy.
	 */
	/* 
	 * Does not have to check source info, as it's alreay covered by 
	 * ip6_input
	 */
	bzero(&dst6, sizeof(dst6));
	dst6.sin6_family = AF_INET6;
	dst6.sin6_len = sizeof(struct sockaddr_in6);
	dst6.sin6_addr = ip6->ip6_dst;

	IN6_LOOKUP_MULTI(ip6->ip6_dst, ifp, in6m);
	if (in6m != NULL)
		ip6_mloopback(ifp, m, &dst6);

	/*
	 * Put the packet into the sending queue of the outgoing interface
	 * if it would fit in the MTU of the interface.
	 */
	linkmtu = IN6_LINKMTU(ifp);
	if (mb_copy->m_pkthdr.len <= linkmtu || linkmtu < IPV6_MMTU) {
		/*
		 * We could call if_output directly here, but we use
		 * nd6_output on purpose to see if IPv6 operation is allowed
		 * on the interface.
		 */
		error = nd6_output(ifp, ifp, mb_copy, &dst6, NULL);
#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_XMIT)
			log(LOG_DEBUG, "phyint_send on mif %d err %d\n",
			    mifp - mif6table, error);
#endif
	} else {
#ifdef MULTICAST_PMTUD
		icmp6_error(mb_copy, ICMP6_PACKET_TOO_BIG, 0, linkmtu);
#else
#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_XMIT)
			log(LOG_DEBUG,
			    "phyint_send: packet too big on %s o %s g %s"
			    " size %d(discarded)\n",
			    if_name(ifp),
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst),
			    mb_copy->m_pkthdr.len);
#endif /* MRT6DEBUG */
		m_freem(mb_copy); /* simply discard the packet */
#endif
	}

	splx(s);
}

static int
register_send(ip6, mif, m)
	struct ip6_hdr *ip6;
	struct mif6 *mif;
	struct mbuf *m;
{
	struct mbuf *mm;
	int i, len = m->m_pkthdr.len;
	struct mrt6msg *im6;
	struct sockaddr_in6 sin6;

#ifdef MRT6DEBUG
	if (mrt6debug)
		log(LOG_DEBUG, "** IPv6 register_send **\n src %s dst %s\n",
		    ip6_sprintf(&ip6->ip6_src), ip6_sprintf(&ip6->ip6_dst));
#endif
	++pim6stat.pim6s_snd_registers;

	/* Make a copy of the packet to send to the user level process */
	MGETHDR(mm, M_DONTWAIT, MT_HEADER);
	if (mm == NULL)
		return (ENOBUFS);
	mm->m_pkthdr.rcvif = NULL;
	mm->m_data += max_linkhdr;
	mm->m_len = sizeof(struct ip6_hdr);

	if ((mm->m_next = m_copy(m, 0, M_COPYALL)) == NULL) {
		m_freem(mm);
		return (ENOBUFS);
	}
	i = MHLEN - M_LEADINGSPACE(mm);
	if (i > len)
		i = len;
	mm = m_pullup(mm, i);
	if (mm == NULL)
		return (ENOBUFS);
/* TODO: check it! */
	mm->m_pkthdr.len = len + sizeof(struct ip6_hdr);

	/*
	 * Send message to routing daemon
	 */
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = ip6->ip6_src;

	im6 = mtod(mm, struct mrt6msg *);
	im6->im6_msgtype      = MRT6MSG_WHOLEPKT;
	im6->im6_mbz          = 0;

	im6->im6_mif = mif - mif6table;

	/* iif info is not given for reg. encap.n */
	mrt6stat.mrt6s_upcalls++;

	if (socket_send(ip6_mrouter, mm, &sin6) < 0) {
#ifdef MRT6DEBUG
		if (mrt6debug)
			log(LOG_WARNING,
			    "register_send: ip6_mrouter socket queue full\n");
#endif
		++mrt6stat.mrt6s_upq_sockfull;
		return (ENOBUFS);
	}
	return (0);
}

/*
 * PIM sparse mode hook
 * Receives the pim control messages, and passes them up to the listening
 * socket, using rip6_input.
 * The only message processed is the REGISTER pim message; the pim header
 * is stripped off, and the inner packet is passed to register_mforward.
 */
int
pim6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct pim *pim; /* pointer to a pim struct */
	struct ip6_hdr *ip6;
	int pimlen;
	struct mbuf *m = *mp;
	int minlen;
	int off = *offp;

	++pim6stat.pim6s_rcv_total;

	ip6 = mtod(m, struct ip6_hdr *);
	pimlen = m->m_pkthdr.len - *offp;

	/*
	 * Validate lengths
	 */
	if (pimlen < PIM_MINLEN) {
		++pim6stat.pim6s_rcv_tooshort;
#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_PIM)
			log(LOG_DEBUG,"pim6_input: PIM packet too short\n");
#endif
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/*
	 * if the packet is at least as big as a REGISTER, go ahead
	 * and grab the PIM REGISTER header size, to avoid another
	 * possible m_pullup() later.
	 *
	 * PIM_MINLEN       == pimhdr + u_int32 == 8
	 * PIM6_REG_MINLEN   == pimhdr + reghdr + eip6hdr == 4 + 4 + 40
	 */
	minlen = (pimlen >= PIM6_REG_MINLEN) ? PIM6_REG_MINLEN : PIM_MINLEN;

	/*
	 * Make sure that the IP6 and PIM headers in contiguous memory, and
	 * possibly the PIM REGISTER header
	 */
#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, minlen, IPPROTO_DONE);
	/* adjust pointer */
	ip6 = mtod(m, struct ip6_hdr *);

	/* adjust mbuf to point to the PIM header */
	pim = (struct pim *)((caddr_t)ip6 + off);
#else
	IP6_EXTHDR_GET(pim, struct pim *, m, off, minlen);
	if (pim == NULL) {
		pim6stat.pim6s_rcv_tooshort++;
		return (IPPROTO_DONE);
	}
#endif

	/* PIM version check */
	if (pim->pim_ver != PIM_VERSION) {
		++pim6stat.pim6s_rcv_badversion;
#ifdef MRT6DEBUG
		log(LOG_ERR,
		    "pim6_input: incorrect version %d, expecting %d\n",
		    pim->pim_ver, PIM_VERSION);
#endif
		m_freem(m);
		return (IPPROTO_DONE);
	}

#define PIM6_CHECKSUM
#ifdef PIM6_CHECKSUM
	{
		int cksumlen;

		/*
		 * Validate checksum.
		 * If PIM REGISTER, exclude the data packet
		 */
		if (pim->pim_type == PIM_REGISTER)
			cksumlen = PIM_MINLEN;
		else
			cksumlen = pimlen;

		if (in6_cksum(m, IPPROTO_PIM, off, cksumlen)) {
			++pim6stat.pim6s_rcv_badsum;
#ifdef MRT6DEBUG
			if (mrt6debug & DEBUG_PIM)
				log(LOG_DEBUG,
				    "pim6_input: invalid checksum\n");
#endif
			m_freem(m);
			return (IPPROTO_DONE);
		}
	}
#endif /* PIM_CHECKSUM */

	if (pim->pim_type == PIM_REGISTER) {
		/*
		 * since this is a REGISTER, we'll make a copy of the register
		 * headers ip6+pim+u_int32_t+encap_ip6, to be passed up to the
		 * routing daemon.
		 */
		static struct sockaddr_in6 dst = { sizeof(dst), AF_INET6 };

		struct mbuf *mcp;
		struct ip6_hdr *eip6;
		u_int32_t *reghdr;
		int rc;

		++pim6stat.pim6s_rcv_registers;

		if ((reg_mif_num >= nummifs) || (reg_mif_num == (mifi_t) -1)) {
#ifdef MRT6DEBUG
			if (mrt6debug & DEBUG_PIM)
				log(LOG_DEBUG,
				    "pim6_input: register mif not set: %d\n",
				    reg_mif_num);
#endif
			m_freem(m);
			return (IPPROTO_DONE);
		}

		reghdr = (u_int32_t *)(pim + 1);

		if ((ntohl(*reghdr) & PIM_NULL_REGISTER))
			goto pim6_input_to_daemon;

		/*
		 * Validate length
		 */
		if (pimlen < PIM6_REG_MINLEN) {
			++pim6stat.pim6s_rcv_tooshort;
			++pim6stat.pim6s_rcv_badregisters;
#ifdef MRT6DEBUG
			log(LOG_ERR,
			    "pim6_input: register packet size too "
			    "small %d from %s\n",
			    pimlen, ip6_sprintf(&ip6->ip6_src));
#endif
			m_freem(m);
			return (IPPROTO_DONE);
		}

		eip6 = (struct ip6_hdr *) (reghdr + 1);
#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_PIM)
			log(LOG_DEBUG,
			    "pim6_input[register], eip6: %s -> %s, "
			    "eip6 plen %d\n",
			    ip6_sprintf(&eip6->ip6_src),
			    ip6_sprintf(&eip6->ip6_dst),
			    ntohs(eip6->ip6_plen));
#endif

		/* verify the version number of the inner packet */
		if ((eip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
			++pim6stat.pim6s_rcv_badregisters;
#ifdef MRT6DEBUG
			log(LOG_DEBUG, "pim6_input: invalid IP version (%d) "
			    "of the inner packet\n",
			    (eip6->ip6_vfc & IPV6_VERSION));
#endif
			m_freem(m);
			return (IPPROTO_NONE);
		}

		/* verify the inner packet is destined to a mcast group */
		if (!IN6_IS_ADDR_MULTICAST(&eip6->ip6_dst)) {
			++pim6stat.pim6s_rcv_badregisters;
#ifdef MRT6DEBUG
			if (mrt6debug & DEBUG_PIM)
				log(LOG_DEBUG,
				    "pim6_input: inner packet of register "
				    "is not multicast %s\n",
				    ip6_sprintf(&eip6->ip6_dst));
#endif
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/*
		 * make a copy of the whole header to pass to the daemon later.
		 */
		mcp = m_copy(m, 0, off + PIM6_REG_MINLEN);
		if (mcp == NULL) {
#ifdef MRT6DEBUG
			log(LOG_ERR,
			    "pim6_input: pim register: "
			    "could not copy register head\n");
#endif
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/*
		 * forward the inner ip6 packet; point m_data at the inner ip6.
		 */
		m_adj(m, off + PIM_MINLEN);
#ifdef MRT6DEBUG
		if (mrt6debug & DEBUG_PIM) {
			log(LOG_DEBUG,
			    "pim6_input: forwarding decapsulated register: "
			    "src %s, dst %s, mif %d\n",
			    ip6_sprintf(&eip6->ip6_src),
			    ip6_sprintf(&eip6->ip6_dst),
			    reg_mif_num);
		}
#endif

#ifdef __FreeBSD__
#if (__FreeBSD_version >= 410000)
		rc = if_simloop(mif6table[reg_mif_num].m6_ifp, m,
		    dst.sin6_family, NULL);
#else
		rc = if_simloop(mif6table[reg_mif_num].m6_ifp, m,
		    (struct sockaddr *)&dst, NULL);

#endif
#else
		rc = looutput(mif6table[reg_mif_num].m6_ifp, m,
		    (struct sockaddr *)&dst, (struct rtentry *) NULL);
#endif

		/* prepare the register head to send to the mrouting daemon */
		m = mcp;
	}

	/*
	 * Pass the PIM message up to the daemon; if it is a register message
	 * pass the 'head' only up to the daemon. This includes the
	 * encapsulator ip6 header, pim header, register header and the
	 * encapsulated ip6 header.
	 */
  pim6_input_to_daemon:
	rip6_input(&m, offp, proto);
	return (IPPROTO_DONE);
}