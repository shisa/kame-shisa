/*	$Id: mip6.c,v 1.3 2004/10/06 02:55:56 keiichi Exp $	*/

/*
 * Copyright (C) 2004 WIDE Project.  All rights reserved.
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

#if (defined(__FreeBSD__) && __FreeBSD__ >= 3)
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_mip6.h"
#endif
#ifdef __NetBSD__
#include "opt_inet.h"
#include "opt_mip6.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/malloc.h>
#endif
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/mipsock.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/ip_encap.h>
#include <netinet/icmp6.h>
#include <netinet/ip6mh.h>

#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/mip6.h>
#include <netinet6/mip6_var.h>
#include <net/if_mip.h>

#ifndef MIP6_BC_HASH_SIZE
#define MIP6_BC_HASH_SIZE 35			/* XXX */
#endif
/* XXX I'm not sure the effectivity of this hash function */
#define MIP6_IN6ADDR_HASH(addr)					\
	((addr)->s6_addr32[0] ^ (addr)->s6_addr32[1] ^		\
	 (addr)->s6_addr32[2] ^ (addr)->s6_addr32[3])
#define MIP6_BC_HASH_ID(addr1, addr2)				\
	((MIP6_IN6ADDR_HASH(addr1) )	\
	 % MIP6_BC_HASH_SIZE)
struct mip6_bc_internal *mip6_bc_hash[MIP6_BC_HASH_SIZE];
struct mip6_bc_list mip6_bc_list = LIST_HEAD_INITIALIZER(mip6_bc_list);

#include "mip.h"

struct mip6stat mip6stat;
u_int8_t mip6_nodetype = MIP6_NODETYPE_NONE;

/* sysctl parameters. */
int mip6ctl_use_ipsec = 1;
#ifdef MIP6_DEBUG
int mip6ctl_debug = 1;
#else
int mip6ctl_debug = 0;
#endif

extern struct ip6protosw mip6_tunnel_protosw;

static int mip6_rr_hint_pps_count = 0;
static struct timeval mip6_rr_hint_ppslim_last;

#if NMIP > 0
#ifndef MIP6_MCOA
static struct mip6_bul_internal *mip6_bul_create(const struct in6_addr *,
						 const struct in6_addr *, 
						 const struct in6_addr *, 
						 u_int16_t,
						 u_int8_t,
						 struct mip_softc *);
#else
static struct mip6_bul_internal *mip6_bul_create(const struct in6_addr *,
						 const struct in6_addr *, 
						 const struct in6_addr *, 
						 u_int16_t,
						 u_int8_t,
						 struct mip_softc *, u_int16_t);
#endif /* MIP6_MCOA */
int mip6_bu_encapcheck(const struct mbuf *, int, int, void *arg);
#endif
int mip6_rev_encapcheck(const struct mbuf *, int, int, void *arg);

#ifndef MIP6_MCOA
static struct mip6_bc_internal *mip6_bce_new_entry(struct in6_addr *,
						   struct in6_addr *,
						   struct in6_addr *,
						   struct ifaddr *, u_int16_t);
#else
static struct mip6_bc_internal *mip6_bce_new_entry(struct in6_addr *,
						   struct in6_addr *,
						   struct in6_addr *,
						   struct ifaddr *, 
						   u_int16_t, u_int16_t);
#endif /* MIP6_MCOA */

static void mip6_bc_list_insert(struct mip6_bc_internal *);
static void mip6_bc_list_remove(struct mip6_bc_internal *);

static int mip6_rr_hint_ratelimit(const struct in6_addr *,
    const struct in6_addr *);
/*
 * sysctl knobs.
 */
#if defined(__NetBSD__) || defined(__OpenBSD__)
int
mip6_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return ENOTDIR;

	switch (name[0]) {
	case MIP6CTL_DEBUG:
		return sysctl_int(oldp, oldlenp, newp, newlen, &mip6ctl_debug);
	case MIP6CTL_USE_IPSEC:
		return sysctl_int(oldp, oldlenp, newp, newlen,
		    &mip6ctl_use_ipsec);
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
#endif /* __NetBSD__ || __OpenBSD__ */

#ifdef __FreeBSD__
SYSCTL_DECL(_net_inet6_mip6);
 
SYSCTL_INT(_net_inet6_mip6, MIP6CTL_DEBUG, debug, CTLFLAG_RW,
    &mip6ctl_debug, 0, "");
SYSCTL_INT(_net_inet6_mip6, MIP6CTL_USE_IPSEC, use_ipsec, CTLFLAG_RW,
    &mip6ctl_use_ipsec, 0, "");
#endif /* __FreeBSD__ */

/*
 * Mobility header processing.
 */
int
mip6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6;
	struct ip6_mh *mh;
	int off = *offp, mhlen;
	int sum;
	int mhdefaultlen[] = {
		sizeof(struct ip6_mh_binding_request), 
		sizeof(struct ip6_mh_home_test_init), 
		sizeof(struct ip6_mh_careof_test_init),
		sizeof(struct ip6_mh_home_test),
		sizeof(struct ip6_mh_careof_test),
		sizeof(struct ip6_mh_binding_update),
		sizeof(struct ip6_mh_binding_ack),
		sizeof(struct ip6_mh_binding_error)
	};

	mip6stat.mip6s_mh++;

	ip6 = mtod(m, struct ip6_hdr *);

	/* validation of the length of the header */
#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, sizeof(*mh), IPPROTO_DONE);
	mh = (struct ip6_mh *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(mh, struct ip6_mh *, m, off, sizeof(*mh));
	if (mh == NULL)
		return (IPPROTO_DONE);
#endif
	if (mh->ip6mh_proto != IPPROTO_NONE) {
		mip6log((LOG_INFO,
		    "mip6_input:%d: Payload Proto %d.\n",
		    __LINE__, mh->ip6mh_proto));
		/* 9.2 discard and SHOULD send ICMP Parameter Problem */
		mip6stat.mip6s_payloadproto++;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    (caddr_t)&mh->ip6mh_proto - (caddr_t)ip6);
		return (IPPROTO_DONE);
	}

	/*
	 * Section 9.2 If length is invalid, issue ICMP param prob,
	 * code0. 
	 */
	mhlen = (mh->ip6mh_len + 1) << 3;
	if (mhlen < IP6OPT_MINLEN || mhlen < mhdefaultlen[mh->ip6mh_type]) {
		mip6log((LOG_INFO, "%s:%d: Mobility Header Length %d.\n",
			__FILE__, __LINE__, mhlen));
		/* 9.2 discard and SHOULD send ICMP Parameter Problem XXX */
		ip6stat.ip6s_toosmall++;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			(caddr_t)&mh->ip6mh_len - (caddr_t)ip6);
			return (IPPROTO_DONE);
	}

	/*
	 * calculate the checksum
	 */
#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, mhlen, IPPROTO_DONE);
	mh = (struct ip6_mh *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(mh, struct ip6_mh *, m, off, mhlen);
	if (mh == NULL)
		return (IPPROTO_DONE);
#endif
	if ((sum = in6_cksum(m, IPPROTO_MH, off, mhlen)) != 0) {
		mip6log((LOG_ERR,
		    "mip6_input:%d: Mobility Header checksum error"
		    "(type = %d, sum = %x) from %s\n",
		    __LINE__,
		    mh->ip6mh_type, sum, ip6_sprintf(&ip6->ip6_src)));
		m_freem(m);
		mip6stat.mip6s_checksum++;
		return (IPPROTO_DONE);
	}

	switch (mh->ip6mh_type) {
#if NMIP > 0
	case IP6_MH_TYPE_BACK:
	{
		struct mip6_bul_internal *mbul;
		/*
		 * find the binding update entry related to this
		 * binding ack message.  if the bue is for home
		 * registration, make sure the packet is protected by
		 * IPsec.
		 */
#ifndef MIP6_MCOA 
		mbul = mip6_bul_get(&ip6->ip6_src, &ip6->ip6_dst);
#else
		mbul = mip6_bul_get(&ip6->ip6_src, &ip6->ip6_dst, 0 /* XXX */);
#endif /* MIP6_MCOA */
		if (mbul != NULL
		    && (mbul->mbul_flags & IP6_MH_BU_HOME) != 0
		    && mip6ctl_use_ipsec) {
#ifndef __OpenBSD__
			if (((m->m_flags & M_DECRYPTED) == 0)
			    && (m->m_flags & M_AUTHIPHDR) == 0)
#else /* !__OpenBSD__ */
			if ((m->m_flags & M_AUTH) == 0)
#endif /* __OpenBSD__ */
			{
				mip6log((LOG_ERR,
				    "mip6_input: an unprotected binding "
				    "update from %s\n",
				    ip6_sprintf(&ip6->ip6_src)));
				m_freem(m);
				mip6stat.mip6s_unprotected++;
				return (IPPROTO_DONE);
			}
		}
		break;
	}
#endif /* NMIP > 0 */
	case IP6_MH_TYPE_BU:
		/*
		 * XXX if this BU is home registration, check if the
		 * packet is protected by IPsec.
		 */
		break;
	}

	/* deliver the packet using Raw IPv6 interface. */
	return (rip6_input(mp, offp, proto));
}

int
mip6_tunnel_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6;
#if NMIP > 0
	struct in6_addr *src, *dst;
	struct mip6_bul_internal *bul, *cnbul;
#endif /* NMIP > 0 */
#if !(defined(__FreeBSD__) && __FreeBSD_version >= 500000)
	int s;
#endif

	m_adj(m, *offp);

	switch (proto) {
	case IPPROTO_IPV6:
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (!m)
				return (IPPROTO_DONE);
		}

#if NMIP > 0
		ip6 = mtod(m, struct ip6_hdr *);
		src = &ip6->ip6_src;
		dst = &ip6->ip6_dst;
		bul = mip6_bul_get_home_agent(dst);
		cnbul = mip6_bul_get(dst, src);
		if ((bul != NULL) && 
		    ((cnbul == NULL) || !(cnbul->mbul_state & MIP6_BUL_STATE_NEEDTUNNEL))) {
			mip6_notify_rr_hint(dst, src);
		}
#endif /* NMIP > 0 */

		mip6stat.mip6s_revtunnel++;

#ifdef __NetBSD__
		s = splnet();
#elif !(defined(__FreeBSD__) && __FreeBSD_version >= 500000)
		s = splimp();
#endif

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
		if (!IF_HANDOFF(&ip6intrq, m, NULL))
			goto bad;
#else
		if (IF_QFULL(&ip6intrq)) {
			IF_DROP(&ip6intrq);	/* update statistics */
			splx(s);
			goto bad;
		}
		IF_ENQUEUE(&ip6intrq, m);
#endif

#if !(defined(__FreeBSD__) && __FreeBSD_version >= 500000)
		splx(s);
#endif
		break;
	default:
		mip6log((LOG_ERR, "protocol %d not supported.\n", proto));
		goto bad;
	}

	return (IPPROTO_DONE);

 bad:
	m_freem(m);
	return (IPPROTO_DONE);
}

struct mip6_bc_internal *
#ifndef MIP6_MCOA
mip6_bce_get(hoa, cnaddr)
	struct in6_addr *hoa;
	struct in6_addr *cnaddr;
#else
mip6_bce_get(hoa, cnaddr, coa, bid)
	struct in6_addr *hoa;
	struct in6_addr *cnaddr;
	struct in6_addr *coa;
	u_int16_t bid;
#endif /* MIP6_MCOA */
{
	struct mip6_bc_internal *mbc;
	int hash = MIP6_BC_HASH_ID(hoa, cnaddr);

	for (mbc = mip6_bc_hash[hash];
	     mbc && mbc->mbc_hash_cache == hash;
	     mbc = LIST_NEXT(mbc, mbc_entry)) {
#ifdef MIP6_MCOA
		if (bid && (mbc->mbc_bid != bid))
			continue;
		if (coa && !IN6_ARE_ADDR_EQUAL(&mbc->mbc_coa, coa)) 
			continue;
#endif /* MIP6_COA */
		if (IN6_ARE_ADDR_EQUAL(&mbc->mbc_hoa, hoa) &&
		    (cnaddr ? IN6_ARE_ADDR_EQUAL(&mbc->mbc_cnaddr, cnaddr) : 1))
			break;
	}

	return (mbc ? (mbc->mbc_hash_cache == hash ? mbc : NULL) : NULL);
}

static struct mip6_bc_internal *
#ifndef MIP6_MCOA
mip6_bce_new_entry(cnaddr, hoa, coa, ifa, flags)
	struct in6_addr *cnaddr, *hoa, *coa;
	struct ifaddr *ifa;
	u_int16_t flags;
#else
mip6_bce_new_entry(cnaddr, hoa, coa, ifa, flags, bid)
	struct in6_addr *cnaddr, *hoa, *coa;
	struct ifaddr *ifa;
	u_int16_t flags, bid;
#endif /* MIP6_MCOA */
{
	struct mip6_bc_internal *mbc = NULL;

	MALLOC(mbc, struct mip6_bc_internal *,
	       sizeof(struct mip6_bc_internal), M_TEMP, M_NOWAIT);
	if (mbc == NULL)
		return (NULL);

	bzero(mbc, sizeof(*mbc));
	mbc->mbc_cnaddr = *cnaddr;
	mbc->mbc_hoa = *hoa;
	mbc->mbc_coa = *coa;
	mbc->mbc_ifaddr = ifa;
	mbc->mbc_flags = flags;
#ifdef MIP6_MCOA
	mbc->mbc_bid = bid;
#endif /* MIP6_MCOA */

	return (mbc);
}

static void
mip6_bc_list_insert(mbc)
	struct mip6_bc_internal *mbc;
{
	int hash = MIP6_BC_HASH_ID(&mbc->mbc_hoa, &mbc->mbc_cnaddr);

	if (mip6_bc_hash[hash] != NULL)
		LIST_INSERT_BEFORE(mip6_bc_hash[hash], mbc, mbc_entry);
	else
		LIST_INSERT_HEAD(&mip6_bc_list, mbc, mbc_entry);
	mip6_bc_hash[hash] = mbc;
	mbc->mbc_hash_cache = hash;
}

int
#ifndef MIP6_MCOA
mip6_bce_update(cnaddr, hoa, coa, flags)
	struct sockaddr_in6 *cnaddr, *hoa, *coa;
	u_int16_t flags;
#else
mip6_bce_update(cnaddr, hoa, coa, flags, bid)
	struct sockaddr_in6 *cnaddr, *hoa, *coa;
	u_int16_t flags, bid;
#endif /* MIP6_MCOA */
{
	int s;
	int error = 0;
	struct mip6_bc_internal *bce = NULL;
	struct ifaddr *ifa;

	/* Non IPv6 address is not support (only for MIP6) */
	if ((cnaddr->sin6_family != AF_INET6) ||
	    (hoa->sin6_family != AF_INET6) ||
	    (coa->sin6_family != AF_INET6))
		return EPFNOSUPPORT; /* XXX ? */

#ifndef MIP6_MCOA
	bce = mip6_bce_get((struct in6_addr *)(&hoa->sin6_addr),
			   (struct in6_addr *)(&cnaddr->sin6_addr));
#else
	bce = mip6_bce_get((struct in6_addr *)(&hoa->sin6_addr),
			   (struct in6_addr *)(&cnaddr->sin6_addr), NULL, bid);
#endif /* MIP6_MCOA */
	if (bce) {
		bce->mbc_coa = coa->sin6_addr;
		return (0);
	} 

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	/* No BCE */
	ifa = ifa_ifwithaddr((struct sockaddr *)cnaddr);
#ifndef MIP6_MCOA
	bce = mip6_bce_new_entry(&cnaddr->sin6_addr, &hoa->sin6_addr,
				 &coa->sin6_addr, ifa, flags);
#else
	bce = mip6_bce_new_entry(&cnaddr->sin6_addr, &hoa->sin6_addr,
				 &coa->sin6_addr, ifa, flags, bid);
#endif /* MIP6_MCOA */
	if (!bce) {
		error = ENOMEM;
		goto done;
	}
	mip6_bc_list_insert(bce);
		
	/* Home agent */
	if (MIP6_IS_HA && bce && flags & IP6_MH_BU_HOME) {
		error = mip6_bc_proxy_control(&hoa->sin6_addr,
					      &cnaddr->sin6_addr, RTM_ADD);
		bce->mbc_encap = encap_attach_func(AF_INET6, IPPROTO_IPV6,
						   mip6_rev_encapcheck,
						   (struct protosw *)&mip6_tunnel_protosw,
						   bce);
	}

 done:
	splx(s);

	return (error);
}

void
mip6_bc_list_remove(mbc)
	struct mip6_bc_internal *mbc;
{
	int hash = MIP6_BC_HASH_ID(&mbc->mbc_hoa, &mbc->mbc_cnaddr);

	if (mip6_bc_hash[hash] == mbc) {
		struct mip6_bc_internal *next = LIST_NEXT(mbc, mbc_entry);
		if (next != NULL &&
		    hash == MIP6_BC_HASH_ID(&next->mbc_hoa, &mbc->mbc_cnaddr)) {
			mip6_bc_hash[hash] = next;
		} else {
			mip6_bc_hash[hash] = NULL;
		}
	}

	LIST_REMOVE(mbc, mbc_entry);
}

int
#ifndef MIP6_MCOA
mip6_bce_remove(cnaddr, hoa, coa, flags)
	struct sockaddr_in6 *cnaddr, *hoa, *coa;
	u_int16_t flags;
#else
mip6_bce_remove(cnaddr, hoa, coa, flags, bid)
	struct sockaddr_in6 *cnaddr, *hoa, *coa;
	u_int16_t flags, bid;
#endif /* MIP6_MCOA */
{
	int s;
	int error = 0;
	struct mip6_bc_internal *mbc;
	
#ifndef MIP6_MCOA
	mbc = mip6_bce_get(&hoa->sin6_addr, &cnaddr->sin6_addr);
#else
	mbc = mip6_bce_get(&hoa->sin6_addr, &cnaddr->sin6_addr, NULL, bid);
#endif
	if (!mbc)
		return (0);

#if 0
	mbc->mbc_refcnt--;
	if (mbc->mbc_flags & IP6_MH_BU_CLONED) {
		if (mbc->mbc_refcnt > 1)
			return (0);
	} else {
		if (mbc->mbc_refcnt > 0)
			return (0);
	}
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	mip6_bc_list_remove(mbc);

	if (MIP6_IS_HA && (mbc->mbc_flags & IP6_MH_BU_HOME)) {
#if 0
		if (MIP6_IS_BC_DAD_WAIT(mbc)) {
			mip6_dad_stop(mbc);
		} else {
#endif
			error = mip6_bc_proxy_control(&hoa->sin6_addr,
						      &cnaddr->sin6_addr, RTM_DELETE);
			error = encap_detach(mbc->mbc_encap);
#if 0
		}
#endif
	}
	FREE(mbc, M_TEMP);

	splx(s);

	return (error);
}

void
mip6_bce_remove_all ()
{
	struct mip6_bc_internal *mbc, *mbcn = NULL;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	for (mbc = LIST_FIRST(&mip6_bc_list); mbc; mbc = mbcn) {
		mbcn = LIST_NEXT(mbc, mbc_entry);

		mip6_bc_list_remove(mbc);
	}
	splx(s);

}

/* Preparing a type2 routing header for outgoing packets  */
/* 	The caller must free the returned buffer */
struct ip6_rthdr2 *
mip6_create_rthdr2(coa)
	struct in6_addr *coa;
{
	struct ip6_rthdr2 *rthdr2;
	size_t len;

	/*
	 * Mobile IPv6 uses type 2 routing header for route
	 * optimization. if the packet has a type 1 routing header
	 * already, we must add a type 2 routing header after the type
	 * 1 routing header.
	 */

	len = sizeof(struct ip6_rthdr2)	+ sizeof(struct in6_addr);
	MALLOC(rthdr2, struct ip6_rthdr2 *, len, M_IP6OPT, M_NOWAIT);
	if (rthdr2 == NULL) {
		return (NULL);
	}
	bzero(rthdr2, len);

	/* rthdr2->ip6r2_nxt = will be filled later in ip6_output */
	rthdr2->ip6r2_len = 2;
	rthdr2->ip6r2_type = 2;
	rthdr2->ip6r2_segleft = 1;
	rthdr2->ip6r2_reserved = 0;
	bcopy((caddr_t)coa, (caddr_t)(rthdr2 + 1), sizeof(struct in6_addr));

	mip6stat.mip6s_orthdr2++;

	return (rthdr2);
}

#if NMIP > 0
struct in6_ifaddr *
mip6_ifa_ifwithin6addr(in6, mipsc)
	const struct in6_addr *in6;
	struct mip_softc *mipsc;
{
	struct sockaddr_in6 sin6;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = *in6;
	if (in6_recoverscope(&sin6, in6, (struct ifnet *)mipsc))
		return (NULL);
	return ((struct in6_ifaddr *)ifa_ifwithaddr((struct sockaddr *)&sin6));
}

static struct mip6_bul_internal *
#ifndef MIP6_MCOA
mip6_bul_create(peeraddr, hoa, coa, flags, state, sc)
	const struct in6_addr *peeraddr, *hoa, *coa;
	u_int16_t flags;
	u_int8_t state;
	struct mip_softc *sc;
#else
mip6_bul_create(peeraddr, hoa, coa, flags, state, sc, bid)
	const struct in6_addr *peeraddr, *hoa, *coa;
	u_int16_t flags;
	u_int8_t state;
	struct mip_softc *sc;
	u_int16_t bid;
#endif
{
	struct mip6_bul_internal *mbul;

	MALLOC(mbul, struct mip6_bul_internal *,
	    sizeof(struct mip6_bul_internal), M_TEMP, M_NOWAIT);
	if (mbul == NULL) {
		return (NULL);
	}

	bzero(mbul, sizeof(*mbul));
	mbul->mbul_peeraddr = *peeraddr;
	mbul->mbul_hoa = *hoa;
	mbul->mbul_coa = *coa;
	mbul->mbul_mip = sc;
	mbul->mbul_flags = flags;
	mbul->mbul_state = state;
#ifdef MIP6_MCOA 
	mbul->mbul_bid = bid;
#endif /* MIP6_MCOA */

	return (mbul);
}

int
#ifndef MIP6_MCOA
mip6_bul_add(peeraddr, hoa, coa, hoa_ifindex, flags, state)
	const struct in6_addr *peeraddr, *hoa, *coa;
	u_short hoa_ifindex;
	u_int16_t flags;
	u_int8_t state;
#else
mip6_bul_add(peeraddr, hoa, coa, hoa_ifindex, flags, state, bid)
	const struct in6_addr *peeraddr, *hoa, *coa;
	u_short hoa_ifindex;
	u_int16_t flags;
	u_int8_t state;
	u_int16_t bid;
#endif /* MIP6_MCOA */
{
	int error = 0;
	struct in6_ifaddr *ia6_hoa;
	struct mip6_bul_internal *mbul;

#if 0
#if defined(__FreeBSD__) && __FreeBSD__ >= 5
	mipsc = (struct mip_softc *)ifnet_byindex(hoa_ifindex);
#else
	mipsc = (struct mip_softc *)ifindex2ifnet[hoa_ifindex];
#endif
	if (mipsc == NULL)
		return (-1);

	ia6_hoa = mip6_ifa_ifwithin6addr(hoa, mipsc);
#endif
	ia6_hoa = mip6_ifa_ifwithin6addr(hoa, NULL);
	if (ia6_hoa == NULL)
		return (-1);

	if (IN6_IS_ADDR_MULTICAST(peeraddr) ||
	    IN6_IS_ADDR_UNSPECIFIED(peeraddr))
		return (EINVAL);

	if (IN6_IS_ADDR_MULTICAST(coa) ||
	    IN6_IS_ADDR_UNSPECIFIED(coa))
		return (EINVAL);
		
	/* 
	 * If the correspondent entry exists, the entry is removed
	 * first. Then, the requested bul will be added right after
	 * this deletion.  
	 */
#ifndef MIP6_MCOA
	mbul = mip6_bul_get(hoa, peeraddr);
#else
	mbul = mip6_bul_get(hoa, peeraddr, bid);
#endif /* MIP6_MCOA */
	if (mbul) 
		mip6_bul_remove(mbul);

	/* binding update list is created here */
#ifndef MIP6_MCOA
	mbul = mip6_bul_create(peeraddr, hoa, coa, 
			       flags, state, (struct mip_softc *)ia6_hoa->ia_ifp);
#else
	mbul = mip6_bul_create(peeraddr, hoa, coa, 
			       flags, state, (struct mip_softc *)ia6_hoa->ia_ifp, bid);
#endif /* MIP6_MCOA */
	if (mbul == NULL)
		return (-1);
	LIST_INSERT_HEAD(&ia6_hoa->ia6_mbul_list, mbul, mbul_entry);

	/* 
	 * tunnel setup only when the requested bul is for home
	 * registration. (and not for the basic NEMO protocol)
	 */
	if ((mbul->mbul_flags & IP6_MH_BU_HOME) && 
		(mbul->mbul_flags & IP6_MH_BU_ROUTER) == 0) {
		mbul->mbul_encap =
			encap_attach_func(AF_INET6, IPPROTO_IPV6,
					  mip6_bu_encapcheck,
					  (struct protosw *)&mip6_tunnel_protosw,
					  mbul);
		if (error) {
			mip6log((LOG_ERR, "tunnel move failed.\n"));
			/* XXX notifiy to upper XXX */
			mip6_bul_remove(mbul); 
			
			return (error);
		}
	}
	
	return (error);
}

void
mip6_bul_remove(mbul)
	struct mip6_bul_internal *mbul;
{
	int error;

	LIST_REMOVE(mbul, mbul_entry);

	/* close encaptab */
	error = encap_detach(mbul->mbul_encap);
	mbul->mbul_encap = NULL;
	if (error) {
		mip6log((LOG_ERR, "tunnel delete failed.\n"));
	}

	FREE(mbul, M_TEMP);
}

void
mip6_bul_remove_all()
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;
	struct in6_ifaddr *ia6;
	struct mip6_bul_internal *mbul, *nmbul;

#ifdef __FreeBSD__
	TAILQ_FOREACH(ifp, &ifnet, if_link)
#elif defined(__NetBSD__)
	TAILQ_FOREACH(ifp, &ifnet, if_list)
#endif
	 {
#ifdef __FreeBSD__
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
#elif defined(__NetBSD__)
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) 
#endif
		{
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			ia6 = (struct in6_ifaddr *)ifa;
			
			if (LIST_EMPTY(&ia6->ia6_mbul_list))
				continue;

			for (mbul = LIST_FIRST(&ia6->ia6_mbul_list); mbul;
			     mbul = nmbul) {
				nmbul = LIST_NEXT(mbul, mbul_entry);
				mip6_bul_remove(mbul);
			}
		}
	}
}

struct mip6_bul_internal *
#ifndef MIP6_MCOA
mip6_bul_get(src, dst)
	const struct in6_addr *src, *dst;
#else  
mip6_bul_get(src, dst, bid)
	const struct in6_addr *src, *dst;
	u_int16_t bid;
#endif /* MIP6_MCOA */
{
	struct in6_ifaddr *ia6_src;
	struct mip6_bul_internal *mbul;

	ia6_src = mip6_ifa_ifwithin6addr(src, NULL);
	if (ia6_src == NULL)
		return (NULL);
	
	for (mbul = LIST_FIRST(&ia6_src->ia6_mbul_list); mbul;
	     mbul = LIST_NEXT(mbul, mbul_entry)) {

#ifdef MIP6_MCOA
		if (bid && (bid =! mbul->mbul_bid))
			continue;
#endif /* MIP6_MCOA */

		if (IN6_ARE_ADDR_EQUAL(src, &mbul->mbul_hoa)
		    && IN6_ARE_ADDR_EQUAL(dst, &mbul->mbul_peeraddr))
			return (mbul);
	}

	/* not found. */
	return (NULL);
}


struct mip6_bul_internal *
mip6_bul_get_home_agent(src)
	const struct in6_addr *src;
{
	struct in6_ifaddr *ia6_src;
	struct mip6_bul_internal *mbul;

	ia6_src = mip6_ifa_ifwithin6addr(src, NULL);
	if (ia6_src == NULL)
		return (NULL);

	for (mbul = LIST_FIRST(&ia6_src->ia6_mbul_list); mbul;
	     mbul = LIST_NEXT(mbul, mbul_entry)) {
		if (IN6_ARE_ADDR_EQUAL(src, &mbul->mbul_hoa)
		    && (mbul->mbul_flags & IP6_MH_BU_HOME) != 0)
			return (mbul);
	}

	/* not found. */
	return (NULL);
}

struct ip6_opt_home_address *
mip6_search_hoa_in_destopt(u_int8_t *optbuf)
{
	int optlen = 0;
	int destoptlen = (((struct ip6_dest *)optbuf)->ip6d_len + 1) << 3;
	optbuf += sizeof(struct ip6_dest);
	destoptlen -= sizeof(struct ip6_dest);

	/* search hoa destination option */
	for (optlen = 0; destoptlen > 0; 
		destoptlen -= optlen, optbuf += optlen) {

		switch (*optbuf) {
		case IP6OPT_PAD1:
			optlen = 1;
			break;
		case IP6OPT_HOME_ADDRESS:
			return (struct ip6_opt_home_address *)optbuf;
		case IP6OPT_PADN:
		default:
			optlen = *(optbuf + 1) + 2;
			break;
		}
	}

	return (NULL);
}

/* Preparing a Home Address option for outgoing packets  */
/* 	The caller must free the returned buffer */
u_int8_t *
mip6_create_hoa_opt(coa)
	struct in6_addr *coa;
{
	struct ip6_opt_home_address *ha_opt;
	struct ip6_dest *ip6dest;
	register char *optbuf;
	size_t pad, optlen, buflen;

	optlen = sizeof(struct ip6_dest);

	/*
	 * calculate the padding size for a home address destination
	 * option (8n + 6).
	 */
	pad = MIP6_PADLEN(optlen, 8, 6);

	/* allocating a new buffer space for a home address option. */
	buflen = optlen + pad + sizeof(struct ip6_opt_home_address);
	optbuf = (char *)malloc(buflen, M_IP6OPT, M_NOWAIT);
	if (optbuf == NULL)
		return (NULL);
	bzero(optbuf, buflen);

	ip6dest = (struct ip6_dest *)optbuf;
		
	/* filling zero for padding. */
	MIP6_FILL_PADDING(optbuf + optlen, pad);
	optlen += pad;

	/* filing a home address destination option fields. */
	ha_opt = (struct ip6_opt_home_address *)(optbuf + optlen);
	ha_opt->ip6oh_type = IP6OPT_HOME_ADDRESS;
	ha_opt->ip6oh_len = IP6OPT_HALEN;
	bcopy(coa, ha_opt->ip6oh_addr , sizeof(struct in6_addr));
	optlen += sizeof(struct ip6_opt_home_address);

	ip6dest->ip6d_nxt = 0;
	ip6dest->ip6d_len = ((optlen) >> 3) - 1;

	return (optbuf);
}

#if 0
/* Append an outer IPv6 header for outgoing packets  */
struct mbuf *
mip6_append_ip6_hdr(mp, osrc, odst)
	struct mbuf **mp;
	struct in6_addr *osrc;
	struct in6_addr *odst;
{
	struct ip6_hdr *iip6 = NULL, *oip6 = NULL;
	struct mbuf *m = *mp;

#ifdef  __NetBSD__
	int sp = splsoftnet();
#endif 
	/* Packet size check */
	if (m->m_len < sizeof(struct ip6_hdr)) {
		m = m_pullup(m, sizeof(struct ip6_hdr));
		if (!m) 
			goto bad;
	}

	/* Allocate mbuf for outer IPv6 headre */
	if (m && m->m_next != NULL && m->m_pkthdr.len < MCLBYTES) {
		struct mbuf *n;

		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (!n) {
			m_freem(m);
			goto bad;
		}
		MCLGET(n, M_DONTWAIT);
		if (!(n->m_flags & M_EXT)) {
			m_freem(m);
			goto bad;
		}

		m_copydata(m, 0, m->m_pkthdr.len, mtod(n, caddr_t));
		n->m_pkthdr = m->m_pkthdr;
		n->m_len = m->m_pkthdr.len;
		m_freem(m);
		m = n;
	}

	/* Get inner IPv6 header */
	iip6 = mtod(m, struct ip6_hdr *);
	iip6->ip6_plen = 
		htons((u_short)m->m_pkthdr.len - sizeof(struct ip6_hdr));

	/* prepend new IP header */
	M_PREPEND(m, sizeof(struct ip6_hdr), M_DONTWAIT);
	if (m && (m->m_len < sizeof(struct ip6_hdr)))
		m = m_pullup(m, sizeof(struct ip6_hdr));
	if (m == NULL) 
		goto bad;

	oip6 = mtod(m, struct ip6_hdr *);
	bzero(oip6, sizeof(struct ip6_hdr));

	/* Filling outer IPv6 header fields */
	oip6->ip6_flow = 0;
	oip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	oip6->ip6_vfc |= IPV6_VERSION;
	oip6->ip6_plen = 
		htons((u_short)m->m_pkthdr.len - sizeof(struct ip6_hdr));
	oip6->ip6_nxt = IPPROTO_IPV6;
	oip6->ip6_hlim = IPV6_DEFHLIM;

	bcopy(osrc, &oip6->ip6_src, sizeof(struct in6_addr));
	bcopy(odst, &oip6->ip6_dst, sizeof(struct in6_addr));
	m->m_pkthdr.rcvif = NULL;

#ifdef  __NetBSD__
	splx(sp);
#endif 

	return (m);

  bad:
#ifdef  __NetBSD__
	splx(sp);
#endif
	return (NULL);
}
#endif

/* if the prefix is equal to one of home prefixes, return TRUE */
int
mip6_are_homeprefix(ndprctl)
	struct nd_prefixctl *ndprctl;
{
	struct in6_ifaddr *ia6;

	for (ia6 = in6_ifaddr; ia6; ia6 = ia6->ia_next) {
		if (ia6->ia_ifp == NULL)
			continue;

		if (ia6->ia_ifp->if_type != IFT_MIP)
			continue;

		if (in6_are_prefix_equal(&ndprctl->ndpr_prefix.sin6_addr, &ia6->ia_addr.sin6_addr, ndprctl->ndpr_plen))
			return (TRUE);
	}
	return (FALSE);
}

int
mip6_bu_encapcheck(m, off, proto, arg)
	const struct mbuf *m;
	int off;
	int proto;
	void *arg;
{
	struct mip6_bul_internal *mbul = (struct mip6_bul_internal *)arg;
	struct ip6_hdr *ip6;

	if (mbul == NULL) {
		return (0);
	}
	if (mbul->mbul_mip == NULL)
		return (0);

	ip6 = mtod(m, struct ip6_hdr*);

	/*
	 * check whether this packet is from a correct sender (our
	 * home agent) to the CoA of the mobile node.
	 */
	if (!IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &mbul->mbul_peeraddr)
	    || !(IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &mbul->mbul_coa)))
		return (0);

	/*
	 * XXX: should we compare the ifid of the inner dstaddr of the
	 * incoming packet and the ifid of the mobile node's?  these
	 * check will be done in the ip6_input and later.
	 */

	return (128);
}
#endif /* NMIP > 0*/

int
mip6_bc_proxy_control(target, local, cmd)
	struct in6_addr *target;
	struct in6_addr *local;
	int cmd;
{
	struct sockaddr_in6 target_sa, local_sa, mask_sa;
	struct sockaddr_dl *sdl;
        struct rtentry *rt, *nrt;
	struct ifaddr *ifa;
	struct ifnet *ifp;
	int flags, error = 0;

	/* create a sockaddr_in6 structure for my address. */
	bzero(&local_sa, sizeof(local_sa));
	local_sa.sin6_len = sizeof(local_sa);
	local_sa.sin6_family = AF_INET6;
	/* XXX */ in6_recoverscope(&local_sa, local, NULL);
	/* XXX */ in6_embedscope(&local_sa.sin6_addr, &local_sa);

	ifa = ifa_ifwithaddr((struct sockaddr *)&local_sa);
	if (ifa == NULL)
		return (EINVAL);
	ifp = ifa->ifa_ifp;

	bzero(&target_sa, sizeof(target_sa));
	target_sa.sin6_len = sizeof(target_sa);
	target_sa.sin6_family = AF_INET6;
	target_sa.sin6_addr = *target;
	if (in6_addr2zoneid(ifp, &target_sa.sin6_addr,
		&target_sa.sin6_scope_id)) {
		mip6log((LOG_ERR, "in6_addr2zoneid failed\n"));
		return(EIO);
	}
	error = in6_embedscope(&target_sa.sin6_addr, &target_sa);
	if (error != 0) {
		return(error);
	}
	/* clear sin6_scope_id before looking up a routing table. */
	target_sa.sin6_scope_id = 0;

	switch (cmd) {
	case RTM_DELETE:
#ifdef __FreeBSD__
		rt = rtalloc1((struct sockaddr *)&target_sa, 0, 0UL);
#else /* __FreeBSD__ */
		rt = rtalloc1((struct sockaddr *)&target_sa, 0);
#endif /* __FreeBSD__ */
		if (rt)
			rt->rt_refcnt--;
		if (rt == NULL)
			return (0);
		if ((rt->rt_flags & RTF_HOST) == 0 ||
		    (rt->rt_flags & RTF_ANNOUNCE) == 0) {
			/*
			 * there is a rtentry, but is not a host nor
			 * a proxy entry.
			 */
			return (0);
		}
		error = rtrequest(RTM_DELETE, rt_key(rt), (struct sockaddr *)0,
		    rt_mask(rt), 0, (struct rtentry **)0);
		if (error) {
			mip6log((LOG_ERR,
				 "RTM_DELETE for %s returned error = %d\n",
				 ip6_sprintf(target), error));
		}
		rt = NULL;

		break;

	case RTM_ADD:
		/* Don't check exsiting routing entry */
#ifdef __NetBSD__
		sdl = ifp->if_sadl;
#else
		/* sdl search */
	{
		struct ifaddr *ifa_dl;

#if defined(__bsdi__) || (defined(__FreeBSD__) && __FreeBSD__ < 3)
		for (ifa_dl = ifp->if_addrlist; ifa_dl; ifa_dl = ifa->ifa_next)
#else
		for (ifa_dl = ifp->if_addrlist.tqh_first; ifa_dl;
		     ifa_dl = ifa_dl->ifa_list.tqe_next)
#endif
			if (ifa_dl->ifa_addr->sa_family == AF_LINK)
				break;

		if (!ifa_dl)
			return (EINVAL);

		sdl = (struct sockaddr_dl *)ifa_dl->ifa_addr;
	}
#endif /* __NetBSD__ */

		/* create a mask. */
		bzero(&mask_sa, sizeof(mask_sa));
		mask_sa.sin6_family = AF_INET6;
		mask_sa.sin6_len = sizeof(mask_sa);

		in6_prefixlen2mask(&mask_sa.sin6_addr, 128);
		flags = (RTF_STATIC | RTF_HOST | RTF_ANNOUNCE);

		error = rtrequest(RTM_ADD, (struct sockaddr *)&target_sa,
		    (struct sockaddr *)sdl, (struct sockaddr *)&mask_sa, flags,
		    &nrt);

		if (error == 0) {
			/* Avoid expiration */
			if (nrt) {
				nrt->rt_rmx.rmx_expire = 0;
				nrt->rt_refcnt--;
			} else
				error = EINVAL;
		} else {
			mip6log((LOG_ERR,
			    "RTM_ADD for %s returned error = %d\n",
			    ip6_sprintf(target), error));
		}

		{
			/* very XXX */
			struct sockaddr_in6 daddr_sa;

			bzero(&daddr_sa, sizeof(daddr_sa));
			daddr_sa.sin6_family = AF_INET6;
			daddr_sa.sin6_len = sizeof(daddr_sa);
			daddr_sa.sin6_addr = in6addr_linklocal_allnodes;
			if (in6_addr2zoneid(ifp, &daddr_sa.sin6_addr,
			    &daddr_sa.sin6_scope_id)) {
				/* XXX: should not happen */
				mip6log((LOG_ERR, "in6_addr2zoneid failed\n"));
				error = EIO; /* XXX */
			}
			if (error == 0) {
				error = in6_embedscope(&daddr_sa.sin6_addr,
				    &daddr_sa);
			}
			if (error == 0) {
				nd6_na_output(ifp, &daddr_sa.sin6_addr,
				    &target_sa.sin6_addr, ND_NA_FLAG_OVERRIDE,
				    1, (struct sockaddr *)sdl);
			}
		}

		break;

	default:
		mip6log((LOG_ERR, "Invalid command %d in mip6control\n", cmd));
		error = -1;
		break;
	}

	return (error);
}

/*
 *	For Home agent
 *	Reverse tunnel is allowed in following case:
 *	|Outer src|Outer Dest|Inner Src|Inner Dest|
 *	|  CoA    |Home agent|   HoA   |  xxxx    | Payload
 */
int
mip6_rev_encapcheck(m, off, proto, arg)
	const struct mbuf *m;
	int off;
	int proto;
	void *arg;
{
	struct ip6_hdr *ip6;
	struct mip6_bc_internal *bce = (struct mip6_bc_internal *)arg;

	ip6 = mtod(m, struct ip6_hdr *);
	
	/* check outer source */
	if (!IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &bce->mbc_coa)) {
		printf("outer IPheader check failed\n");
		return (0);
	}
	return (128);
}

int
mip6_encapsulate(mm, osrc, odst)
	struct mbuf **mm;
	struct in6_addr *osrc, *odst;
{
	struct mbuf *m = *mm;
	struct ip6_hdr *ip6;
	struct ip6_mh mh;

	/* check whether this packet can be tunneled or not */

	ip6 = mtod(m, struct ip6_hdr *);
	/* If packet is MH and BE originated by this node   */
	if (ip6->ip6_nxt == IPPROTO_MH) {
		m_copydata(m, sizeof(struct ip6_hdr),
			sizeof(struct ip6_mh), (caddr_t)&mh);
		if (mh.ip6mh_type == IP6_MH_TYPE_BERROR) {
			printf("skip encapsulation for BE\n");
			goto done;
		}
	}



	M_PREPEND(m, sizeof(struct ip6_hdr), M_DONTWAIT);
	if (m && m->m_len < sizeof(struct ip6_hdr))
		m = m_pullup(m, sizeof(struct ip6_hdr));
	if (m == NULL)
		return (0);

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	ip6->ip6_plen = htons((u_short)m->m_pkthdr.len - sizeof(*ip6));
	ip6->ip6_nxt = IPPROTO_IPV6;
	ip6->ip6_hlim = ip6_defhlim;
	ip6->ip6_src = *osrc;
	ip6->ip6_dst = *odst;
	mip6stat.mip6s_orevtunnel++;

   done:
#ifdef IPV6_MINMTU
	/* XXX */
	return (ip6_output(m, 0, 0, IPV6_MINMTU, 0, /*&ifp*/NULL
#if defined(__FreeBSD__) && __FreeBSD_version >= 480000
			   , NULL
#endif
			));
#else
	return (ip6_output(m, 0, 0, 0, 0, /*&ifp*/NULL
#if defined(__FreeBSD__) && __FreeBSD_version >= 480000
			   , NULL
#endif
			));
#endif
}

#if NMIP > 0
void
mip6_probe_routers(void)
{
        struct llinfo_nd6 *ln;
 
        ln = llinfo_nd6.ln_next;
        while (ln && ln != &llinfo_nd6) {
                if ((ln->ln_router) &&
                    ((ln->ln_state == ND6_LLINFO_REACHABLE) ||
                     (ln->ln_state == ND6_LLINFO_STALE))) {
                        ln->ln_asked = 0;
                        ln->ln_state = ND6_LLINFO_DELAY;
                        nd6_llinfo_settimer(ln, 0);
                }
                ln = ln->ln_next;
        }
}
#endif /* NMIP > 0 */

#if NMIP > 0
void
mip6_notify_rr_hint(dst, src)
	struct in6_addr *dst;
	struct in6_addr *src;
{
	if (mip6_rr_hint_ratelimit(dst, src)) {
		/* rate limited. */
		return;
	}
	mips_notify_rr_hint(dst, src);
}
#endif /* NMIP > 0 */

#if NMIP > 0
static int
mip6_rr_hint_ratelimit(dst, src)
	const struct in6_addr *dst;	/* not used at this moment */
	const struct in6_addr *src;	/* not used at this moment */
{
	int ret;

	ret = 0;	/* okay to send */

	/* PPS limit XXX 1 per 1 second. */
	if (!ppsratecheck(&mip6_rr_hint_ppslim_last, &mip6_rr_hint_pps_count,
	    1)) {
		/* The packet is subject to rate limit */
		ret++;
	}

	return ret;
}
#endif /* NMIP > 0 */
