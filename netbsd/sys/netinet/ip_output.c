/*	$NetBSD: ip_output.c,v 1.95.10.3 2003/06/30 03:25:22 grant Exp $	*/

/*
 * Copyright (c) 2002 INRIA. All rights reserved.
 *
 * Implementation of Internet Group Management Protocol, Version 3.
 * Developed by Hitoshi Asaeda, INRIA, February 2002.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of INRIA nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Public Access Networks Corporation ("Panix").  It was developed under
 * contract to Panix by Eric Haszlakiewicz and Thor Lancelot Simon.
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

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)ip_output.c	8.3 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip_output.c,v 1.95.10.3 2003/06/30 03:25:22 grant Exp $");

#include "opt_pfil_hooks.h"
#include "opt_ipsec.h"
#include "opt_mrouting.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef IGMPV3
#include <netinet/in_msf.h>
#endif
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif

#include <machine/stdarg.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#include <netkey/key_debug.h>
#endif /*IPSEC*/

#include "pf.h"

#if NPF > 0
#include <net/pfvar.h>
#endif

static struct mbuf *ip_insertoptions __P((struct mbuf *, struct mbuf *, int *));
static int ip_getmopt_ifargs
	__P((int, struct ifnet **, struct in_addr *, struct in_addr *));
#ifdef IGMPV3
static int in_getmopt_ifargs
	__P((int, struct ifnet **, struct in_addr *, u_int32_t));
static int ip_getmopt_sgaddr
	__P((struct mbuf *, int, struct ifnet **, struct sockaddr_storage *,
		struct sockaddr_storage *));
#endif
static void ip_mloopback
	__P((struct ifnet *, struct mbuf *, struct sockaddr_in *));

#ifdef IGMPV3
#define SIN(x)	((struct sockaddr_in *)(x))
#endif

#ifdef PFIL_HOOKS
extern struct pfil_head inet_pfil_hook;			/* XXX */
#endif

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int
#if __STDC__
ip_output(struct mbuf *m0, ...)
#else
ip_output(m0, va_alist)
	struct mbuf *m0;
	va_dcl
#endif
{
	struct ip *ip;
	struct ifnet *ifp;
	struct mbuf *m = m0;
	int hlen = sizeof (struct ip);
	int len, error = 0;
	struct route iproute;
	struct sockaddr_in *dst;
	struct in_ifaddr *ia;
	struct mbuf *opt;
	struct route *ro;
	int flags, sw_csum;
	int *mtu_p;
	int mtu;
	struct ip_moptions *imo;
	va_list ap;
#ifdef IPSEC
	struct socket *so;
	struct secpolicy *sp = NULL;
#endif /*IPSEC*/
	u_int16_t ip_len;

	len = 0;
	va_start(ap, m0);
	opt = va_arg(ap, struct mbuf *);
	ro = va_arg(ap, struct route *);
	flags = va_arg(ap, int);
	imo = va_arg(ap, struct ip_moptions *);
	if (flags & IP_RETURNMTU)
		mtu_p = va_arg(ap, int *);
	else
		mtu_p = NULL;
	va_end(ap);

#ifdef IPSEC
	so = ipsec_getsocket(m);
	(void)ipsec_setsocket(m, NULL);
#endif /*IPSEC*/

#ifdef	DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("ip_output no HDR");
#endif
	if (opt) {
		m = ip_insertoptions(m, opt, &len);
		if (len >= sizeof(struct ip))
			hlen = len;
	}
	ip = mtod(m, struct ip *);
	/*
	 * Fill in IP header.
	 */
	if ((flags & (IP_FORWARDING|IP_RAWOUTPUT)) == 0) {
		ip->ip_v = IPVERSION;
		ip->ip_off = 0;
		ip->ip_id = htons(ip_randomid());
		ip->ip_hl = hlen >> 2;
		ipstat.ips_localout++;
	} else {
		hlen = ip->ip_hl << 2;
	}
	/*
	 * Route packet.
	 */
	if (ro == 0) {
		ro = &iproute;
		bzero((caddr_t)ro, sizeof (*ro));
	}
	dst = satosin(&ro->ro_dst);
	/*
	 * If there is a cached route,
	 * check that it is to the same destination
	 * and is still up.  If not, free it and try again.
	 * The address family should also be checked in case of sharing the
	 * cache with IPv6.
	 */
	if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
	    dst->sin_family != AF_INET ||
	    !in_hosteq(dst->sin_addr, ip->ip_dst))) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)0;
	}
	if (ro->ro_rt == 0) {
		bzero(dst, sizeof(*dst));
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = ip->ip_dst;
	}
	/*
	 * If routing to interface only,
	 * short circuit routing lookup.
	 */
	if (flags & IP_ROUTETOIF) {
		if ((ia = ifatoia(ifa_ifwithladdr(sintosa(dst)))) == 0) {
			ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}
		ifp = ia->ia_ifp;
		mtu = ifp->if_mtu;
		ip->ip_ttl = 1;
	} else if ((IN_MULTICAST(ip->ip_dst.s_addr) ||
	    ip->ip_dst.s_addr == INADDR_BROADCAST) &&
	    imo != NULL && imo->imo_multicast_ifp != NULL) {
		ifp = imo->imo_multicast_ifp;
		mtu = ifp->if_mtu;
		IFP_TO_IA(ifp, ia);
	} else {
		if (ro->ro_rt == 0) {
#ifdef RADIX_MPATH
			rtalloc_mpath(ro,
			    ntohl(ip->ip_src.s_addr ^ ip->ip_dst.s_addr));
#else
			rtalloc(ro);
#endif
		}
		if (ro->ro_rt == 0) {
			ipstat.ips_noroute++;
			error = EHOSTUNREACH;
			goto bad;
		}
		ia = ifatoia(ro->ro_rt->rt_ifa);
		ifp = ro->ro_rt->rt_ifp;
		if ((mtu = ro->ro_rt->rt_rmx.rmx_mtu) == 0)
			mtu = ifp->if_mtu;
		ro->ro_rt->rt_use++;
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = satosin(ro->ro_rt->rt_gateway);
	}
	if (IN_MULTICAST(ip->ip_dst.s_addr) ||
	    (ip->ip_dst.s_addr == INADDR_BROADCAST)) {
		struct in_multi *inm;

		m->m_flags |= (ip->ip_dst.s_addr == INADDR_BROADCAST) ?
			M_BCAST : M_MCAST;
		/*
		 * IP destination address is multicast.  Make sure "dst"
		 * still points to the address in "ro".  (It may have been
		 * changed to point to a gateway address, above.)
		 */
		dst = satosin(&ro->ro_dst);
		/*
		 * See if the caller provided any multicast options
		 */
		if (imo != NULL)
			ip->ip_ttl = imo->imo_multicast_ttl;
		else
			ip->ip_ttl = IP_DEFAULT_MULTICAST_TTL;

		/*
		 * if we don't know the outgoing ifp yet, we can't generate
		 * output
		 */
		if (!ifp) {
			ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}

		/*
		 * If the packet is multicast or broadcast, confirm that
		 * the outgoing interface can transmit it.
		 */
		if (((m->m_flags & M_MCAST) &&
		     (ifp->if_flags & IFF_MULTICAST) == 0) ||
		    ((m->m_flags & M_BCAST) && 
		     (ifp->if_flags & (IFF_BROADCAST|IFF_POINTOPOINT)) == 0))  {
			ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}
		/*
		 * If source address not specified yet, use an address
		 * of outgoing interface.
		 */
		if (in_nullhost(ip->ip_src)) {
			struct in_ifaddr *ia;

			IFP_TO_IA(ifp, ia);
			if (!ia) {
				error = EADDRNOTAVAIL;
				goto bad;
			}
			ip->ip_src = ia->ia_addr.sin_addr;
		}

		IN_LOOKUP_MULTI(ip->ip_dst, ifp, inm);
		if (inm != NULL &&
		   (imo == NULL || imo->imo_multicast_loop)) {
			/*
			 * If we belong to the destination multicast group
			 * on the outgoing interface, and the caller did not
			 * forbid loopback, loop back a copy.
			 */
			ip_mloopback(ifp, m, dst);
		}
#ifdef MROUTING
		else {
			/*
			 * If we are acting as a multicast router, perform
			 * multicast forwarding as if the packet had just
			 * arrived on the interface to which we are about
			 * to send.  The multicast forwarding function
			 * recursively calls this function, using the
			 * IP_FORWARDING flag to prevent infinite recursion.
			 *
			 * Multicasts that are looped back by ip_mloopback(),
			 * above, will be forwarded by the ip_input() routine,
			 * if necessary.
			 */
			extern struct socket *ip_mrouter;

			if (ip_mrouter && (flags & IP_FORWARDING) == 0) {
				if (ip_mforward(m, ifp) != 0) {
					m_freem(m);
					goto done;
				}
			}
		}
#endif
		/*
		 * Multicasts with a time-to-live of zero may be looped-
		 * back, above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip->ip_ttl == 0 || (ifp->if_flags & IFF_LOOPBACK) != 0) {
			m_freem(m);
			goto done;
		}

		goto sendit;
	}
#ifndef notdef
	/*
	 * If source address not specified yet, use address
	 * of outgoing interface.
	 */
	if (in_nullhost(ip->ip_src))
		ip->ip_src = ia->ia_addr.sin_addr;
#endif

	/*
	 * packets with Class-D address as source are not valid per 
	 * RFC 1112
	 */
	if (IN_MULTICAST(ip->ip_src.s_addr)) {
		ipstat.ips_odropped++;
		error = EADDRNOTAVAIL;
		goto bad;
	}

	/*
	 * Look for broadcast address and
	 * and verify user is allowed to send
	 * such a packet.
	 */
	if (in_broadcast(dst->sin_addr, ifp)) {
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if ((flags & IP_ALLOWBROADCAST) == 0) {
			error = EACCES;
			goto bad;
		}
		/* don't allow broadcast messages to be fragmented */
		if ((u_int16_t)ip->ip_len > ifp->if_mtu) {
			error = EMSGSIZE;
			goto bad;
		}
		m->m_flags |= M_BCAST;
	} else
		m->m_flags &= ~M_BCAST;

sendit:
	/*
	 * If we're doing Path MTU Discovery, we need to set DF unless
	 * the route's MTU is locked.
	 */
	if ((flags & IP_MTUDISC) != 0 && ro->ro_rt != NULL &&
	    (ro->ro_rt->rt_rmx.rmx_locks & RTV_MTU) == 0)
		ip->ip_off |= IP_DF;

	/*
	 * Remember the current ip_len and ip_off, and swap them into
	 * network order.
	 */
	ip_len = ip->ip_len;

	HTONS(ip->ip_len);
	HTONS(ip->ip_off);

#ifdef IPSEC
	/* get SP for this packet */
	if (so == NULL)
		sp = ipsec4_getpolicybyaddr(m, IPSEC_DIR_OUTBOUND,
		    flags, &error);
	else
		sp = ipsec4_getpolicybysock(m, IPSEC_DIR_OUTBOUND, so, &error);

	if (sp == NULL) {
		ipsecstat.out_inval++;
		goto bad;
	}

	error = 0;

	/* check policy */
	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
		/*
		 * This packet is just discarded.
		 */
		ipsecstat.out_polvio++;
		goto bad;

	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		/* no need to do IPsec. */
		goto skip_ipsec;
	
	case IPSEC_POLICY_IPSEC:
		if (sp->req == NULL) {
			/* acquire a policy */
			error = key_spdacquire(sp);
			goto bad;
		}
		break;

	case IPSEC_POLICY_ENTRUST:
	default:
		printf("ip_output: Invalid policy found. %d\n", sp->policy);
	}

	/*
	 * ipsec4_output() expects ip_len and ip_off in network
	 * order.  They have been set to network order above.
	 */

    {
	struct ipsec_output_state state;
	bzero(&state, sizeof(state));
	state.m = m;
	if (flags & IP_ROUTETOIF) {
		state.ro = &iproute;
		bzero(&iproute, sizeof(iproute));
	} else
		state.ro = ro;
	state.dst = (struct sockaddr *)dst;

	/*
	 * We can't defer the checksum of payload data if
	 * we're about to encrypt/authenticate it.
	 *
	 * XXX When we support crypto offloading functions of
	 * XXX network interfaces, we need to reconsider this,
	 * XXX since it's likely that they'll support checksumming,
	 * XXX as well.
	 */
	if (m->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~(M_CSUM_TCPv4|M_CSUM_UDPv4);
	}

	error = ipsec4_output(&state, sp, flags);

	m = state.m;
	if (flags & IP_ROUTETOIF) {
		/*
		 * if we have tunnel mode SA, we may need to ignore
		 * IP_ROUTETOIF.
		 */
		if (state.ro != &iproute || state.ro->ro_rt != NULL) {
			flags &= ~IP_ROUTETOIF;
			ro = state.ro;
		}
	} else
		ro = state.ro;
	dst = (struct sockaddr_in *)state.dst;
	if (error) {
		/* mbuf is already reclaimed in ipsec4_output. */
		m0 = NULL;
		switch (error) {
		case EHOSTUNREACH:
		case ENETUNREACH:
		case EMSGSIZE:
		case ENOBUFS:
		case ENOMEM:
			break;
		default:
			printf("ip4_output (ipsec): error code %d\n", error);
			/*fall through*/
		case ENOENT:
			/* don't show these error codes to the user */
			error = 0;
			break;
		}
		goto bad;
	}

	/* be sure to update variables that are affected by ipsec4_output() */
	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
	ip_len = ntohs(ip->ip_len);

	if (ro->ro_rt == NULL) {
		if ((flags & IP_ROUTETOIF) == 0) {
			printf("ip_output: "
				"can't update route after IPsec processing\n");
			error = EHOSTUNREACH;	/*XXX*/
			goto bad;
		}
	} else {
		/* nobody uses ia beyond here */
		if (state.encap) {
			ifp = ro->ro_rt->rt_ifp;
			if ((mtu = ro->ro_rt->rt_rmx.rmx_mtu) == 0)
				mtu = ifp->if_mtu;
		}
	}
    }

skip_ipsec:
#endif /*IPSEC*/

#ifdef PFIL_HOOKS
	/*
	 * Run through list of hooks for output packets.
	 */
	if ((error = pfil_run_hooks(&inet_pfil_hook, &m, ifp, PFIL_OUT)) != 0)
		goto done;
	if (m == NULL)
		goto done;

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
#endif /* PFIL_HOOKS */

#if NPF > 0
	if (pf_test(PF_OUT, ifp, &m) != PF_PASS) {
		error = EHOSTUNREACH;
		m_freem(m);
		goto done;
	}
	if (m == NULL)
		goto done;

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
#endif

	/*
	 * If small enough for mtu of path, can just send directly.
	 */
	if (ip_len <= mtu) {
#if IFA_STATS
		/*
		 * search for the source address structure to
		 * maintain output statistics.
		 */
		INADDR_TO_IA(ip->ip_src, ia);
		if (ia)
			ia->ia_ifa.ifa_data.ifad_outbytes += ip_len;
#endif
		/*
		 * Always initialize the sum to 0!  Some HW assisted
		 * checksumming requires this.
		 */
		ip->ip_sum = 0;
		m->m_pkthdr.csum_flags |= M_CSUM_IPv4;

		sw_csum = m->m_pkthdr.csum_flags & ~ifp->if_csum_flags_tx;

		/*
		 * Perform any checksums that the hardware can't do
		 * for us.
		 *
		 * XXX Does any hardware require the {th,uh}_sum
		 * XXX fields to be 0?
		 */
		if (sw_csum & M_CSUM_IPv4)
			ip->ip_sum = in_cksum(m, hlen);
		if (sw_csum & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
			in_delayed_cksum(m);
			sw_csum &= ~(M_CSUM_TCPv4|M_CSUM_UDPv4);
		}
		m->m_pkthdr.csum_flags &= ifp->if_csum_flags_tx;

#ifdef IPSEC
		/* clean ipsec history once it goes out of the node */
		ipsec_delaux(m);
#endif
		error = (*ifp->if_output)(ifp, m, sintosa(dst), ro->ro_rt);
		goto done;
	}

	/*
	 * We can't use HW checksumming if we're about to
	 * to fragment the packet.
	 *
	 * XXX Some hardware can do this.
	 */
	if (m->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~(M_CSUM_TCPv4|M_CSUM_UDPv4);
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 *
	 * Note we swap ip_len and ip_off into host order to make
	 * the logic below a little simpler.
	 */

	NTOHS(ip->ip_len);
	NTOHS(ip->ip_off);

	if (ip->ip_off & IP_DF) {
		if (flags & IP_RETURNMTU)
			*mtu_p = mtu;
		error = EMSGSIZE;
		ipstat.ips_cantfrag++;
		goto bad;
	}

	error = ip_fragment(m, ifp, mtu);
	if (error) {
		m = NULL;
		goto bad;
	}

	for (; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = 0;
		if (error == 0) {
#if IFA_STATS
			/*
			 * search for the source address structure to
			 * maintain output statistics.
			 */
			INADDR_TO_IA(ip->ip_src, ia);
			if (ia) {
				ia->ia_ifa.ifa_data.ifad_outbytes +=
					ntohs(ip->ip_len);
			}
#endif
#ifdef IPSEC
			/* clean ipsec history once it goes out of the node */
			ipsec_delaux(m);
#endif
			error = (*ifp->if_output)(ifp, m, sintosa(dst),
			    ro->ro_rt);
		} else
			m_freem(m);
	}

	if (error == 0)
		ipstat.ips_fragmented++;
done:
	if (ro == &iproute && (flags & IP_ROUTETOIF) == 0 && ro->ro_rt) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = 0;
	}

#ifdef IPSEC
	if (sp != NULL) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ip_output call free SP:%p\n", sp));
		key_freesp(sp);
	}
#endif /* IPSEC */

	return (error);
bad:
	m_freem(m);
	goto done;
}

int
ip_fragment(m, ifp, mtu)
	struct mbuf *m;
	struct ifnet *ifp;
	u_long mtu;
{
	struct ip *ip, *mhip;
	struct mbuf *m0;
	int len, hlen, off;
	int mhlen, firstlen;
	struct mbuf **mnext;
	int fragments = 0;
	int s;
	int error = 0;

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;

	len = (mtu - hlen) &~ 7;
	if (len < 8)
		return (EMSGSIZE);

	firstlen = len;
	mnext = &m->m_nextpkt;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < (u_int16_t)ip->ip_len; off += len) {
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			error = ENOBUFS;
			ipstat.ips_odropped++;
			goto sendorfree;
		}
		*mnext = m;
		mnext = &m->m_nextpkt;
		m->m_data += max_linkhdr;
		mhip = mtod(m, struct ip *);
		*mhip = *ip;
		/* we must inherit MCAST and BCAST flags */
		m->m_flags |= m0->m_flags & (M_MCAST|M_BCAST);
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_hl = mhlen >> 2;
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		if (ip->ip_off & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= (u_int16_t)ip->ip_len)
			len = (u_int16_t)ip->ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_int16_t)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			ipstat.ips_odropped++;
			goto sendorfree;
		}
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = (struct ifnet *)0;
		HTONS(mhip->ip_off);
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		ipstat.ips_ofragments++;
		fragments++;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m = m0;
	m_adj(m, hlen + firstlen - (u_int16_t)ip->ip_len);
	m->m_pkthdr.len = hlen + firstlen;
	ip->ip_len = htons((u_int16_t)m->m_pkthdr.len);
	ip->ip_off |= IP_MF;
	HTONS(ip->ip_off);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m, hlen);
sendorfree:
	/*
	 * If there is no room for all the fragments, don't queue
	 * any of them.
	 */
	s = splnet();
	if (ifp->if_snd.ifq_maxlen - ifp->if_snd.ifq_len < fragments &&
	    error == 0) {
		error = ENOBUFS;
		ipstat.ips_odropped++;
		IFQ_INC_DROPS(&ifp->if_snd);
	}
	splx(s);
	if (error) {
		for (m = m0; m; m = m0) {
			m0 = m->m_nextpkt;
			m->m_nextpkt = NULL;
			m_freem(m);
		}
	}

	return (error);
}

/*
 * Process a delayed payload checksum calculation.
 */
void
in_delayed_cksum(struct mbuf *m)
{
	struct ip *ip;
	u_int16_t csum, offset;

	ip = mtod(m, struct ip *);
	offset = ip->ip_hl << 2;
	csum = in4_cksum(m, 0, offset, ntohs(ip->ip_len) - offset);
	if (csum == 0 && (m->m_pkthdr.csum_flags & M_CSUM_UDPv4) != 0)
		csum = 0xffff;

	offset += m->m_pkthdr.csum_data;	/* checksum offset */

	if ((offset + sizeof(u_int16_t)) > m->m_len) {
		/* This happen when ip options were inserted
		printf("in_delayed_cksum: pullup len %d off %d proto %d\n",
		    m->m_len, offset, ip->ip_p);
		 */
		m_copyback(m, offset, sizeof(csum), (caddr_t) &csum);
	} else
		*(u_int16_t *)(mtod(m, caddr_t) + offset) = csum;
}

/*
 * Determine the maximum length of the options to be inserted;
 * we would far rather allocate too much space rather than too little.
 */

u_int
ip_optlen(inp)
	struct inpcb *inp;
{
	struct mbuf *m = inp->inp_options;

	if (m && m->m_len > offsetof(struct ipoption, ipopt_dst))
		return(m->m_len - offsetof(struct ipoption, ipopt_dst));
	else
		return 0;
}


/*
 * Insert IP options into preformed packet.
 * Adjust IP destination as required for IP source routing,
 * as indicated by a non-zero in_addr at the start of the options.
 */
static struct mbuf *
ip_insertoptions(m, opt, phlen)
	struct mbuf *m;
	struct mbuf *opt;
	int *phlen;
{
	struct ipoption *p = mtod(opt, struct ipoption *);
	struct mbuf *n;
	struct ip *ip = mtod(m, struct ip *);
	unsigned optlen;

	optlen = opt->m_len - sizeof(p->ipopt_dst);
	if (optlen + (u_int16_t)ip->ip_len > IP_MAXPACKET)
		return (m);		/* XXX should fail */
	if (!in_nullhost(p->ipopt_dst))
		ip->ip_dst = p->ipopt_dst;
	if (m->m_flags & M_EXT || m->m_data - optlen < m->m_pktdat) {
		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n == 0)
			return (m);
		M_MOVE_PKTHDR(n, m);
		m->m_flags &= ~M_PKTHDR;
		m->m_len -= sizeof(struct ip);
		m->m_data += sizeof(struct ip);
		n->m_next = m;
		m = n;
		m->m_len = optlen + sizeof(struct ip);
		m->m_data += max_linkhdr;
		bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
	} else {
		m->m_data -= optlen;
		m->m_len += optlen;
		memmove(mtod(m, caddr_t), ip, sizeof(struct ip));
	}
	m->m_pkthdr.len += optlen;
	ip = mtod(m, struct ip *);
	bcopy((caddr_t)p->ipopt_list, (caddr_t)(ip + 1), (unsigned)optlen);
	*phlen = sizeof(struct ip) + optlen;
	ip->ip_len += optlen;
	return (m);
}

/*
 * Copy options from ip to jp,
 * omitting those not copied during fragmentation.
 */
int
ip_optcopy(ip, jp)
	struct ip *ip, *jp;
{
	u_char *cp, *dp;
	int opt, optlen, cnt;

	cp = (u_char *)(ip + 1);
	dp = (u_char *)(jp + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP) {
			/* Preserve for IP mcast tunnel's LSRR alignment. */
			*dp++ = IPOPT_NOP;
			optlen = 1;
			continue;
		}
#ifdef DIAGNOSTIC
		if (cnt < IPOPT_OLEN + sizeof(*cp))
			panic("malformed IPv4 option passed to ip_optcopy");
#endif
		optlen = cp[IPOPT_OLEN];
#ifdef DIAGNOSTIC
		if (optlen < IPOPT_OLEN + sizeof(*cp) || optlen > cnt)
			panic("malformed IPv4 option passed to ip_optcopy");
#endif
		/* bogus lengths should have been caught by ip_dooptions */
		if (optlen > cnt)
			optlen = cnt;
		if (IPOPT_COPIED(opt)) {
			bcopy((caddr_t)cp, (caddr_t)dp, (unsigned)optlen);
			dp += optlen;
		}
	}
	for (optlen = dp - (u_char *)(jp+1); optlen & 0x3; optlen++)
		*dp++ = IPOPT_EOL;
	return (optlen);
}

/*
 * IP socket option processing.
 */
int
ip_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	struct inpcb *inp = sotoinpcb(so);
	struct mbuf *m = *mp;
	int optval = 0;
	int error = 0;
#ifdef IPSEC
#ifdef __NetBSD__
	struct proc *p = curproc;	/*XXX*/
#endif
#endif

	if (level != IPPROTO_IP) {
		error = EINVAL;
		if (op == PRCO_SETOPT && *mp)
			(void) m_free(*mp);
	} else switch (op) {

	case PRCO_SETOPT:
		switch (optname) {
		case IP_OPTIONS:
#ifdef notyet
		case IP_RETOPTS:
			return (ip_pcbopts(optname, &inp->inp_options, m));
#else
			return (ip_pcbopts(&inp->inp_options, m));
#endif

		case IP_TOS:
		case IP_TTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVIF:
			if (m == NULL || m->m_len != sizeof(int))
				error = EINVAL;
			else {
				optval = *mtod(m, int *);
				switch (optname) {

				case IP_TOS:
					inp->inp_ip.ip_tos = optval;
					break;

				case IP_TTL:
					inp->inp_ip.ip_ttl = optval;
					break;
#define	OPTSET(bit) \
	if (optval) \
		inp->inp_flags |= bit; \
	else \
		inp->inp_flags &= ~bit;

				case IP_RECVOPTS:
					OPTSET(INP_RECVOPTS);
					break;

				case IP_RECVRETOPTS:
					OPTSET(INP_RECVRETOPTS);
					break;

				case IP_RECVDSTADDR:
					OPTSET(INP_RECVDSTADDR);
					break;

				case IP_RECVIF:
					OPTSET(INP_RECVIF);
					break;
				}
			}
			break;
#undef OPTSET

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
#ifdef IGMPV3
		case IP_BLOCK_SOURCE:
		case IP_UNBLOCK_SOURCE:
		case IP_ADD_SOURCE_MEMBERSHIP:
		case IP_DROP_SOURCE_MEMBERSHIP:
		case MCAST_JOIN_GROUP:
		case MCAST_BLOCK_SOURCE:
		case MCAST_UNBLOCK_SOURCE:
		case MCAST_LEAVE_GROUP:
		case MCAST_JOIN_SOURCE_GROUP:
		case MCAST_LEAVE_SOURCE_GROUP:
#endif
			error = ip_setmoptions(optname, &inp->inp_moptions, m);
			break;

		case IP_PORTRANGE:
			if (m == 0 || m->m_len != sizeof(int))
				error = EINVAL;
			else {
				optval = *mtod(m, int *);

				switch (optval) {

				case IP_PORTRANGE_DEFAULT:
				case IP_PORTRANGE_HIGH:
					inp->inp_flags &= ~(INP_LOWPORT);
					break;

				case IP_PORTRANGE_LOW:
					inp->inp_flags |= INP_LOWPORT;
					break;

				default:
					error = EINVAL;
					break;
				}
			}
			break;

#ifdef IPSEC
		case IP_IPSEC_POLICY:
		{
			caddr_t req = NULL;
			size_t len = 0;
			int priv = 0;

#ifdef __NetBSD__
			if (p == 0 || suser(p->p_ucred, &p->p_acflag))
				priv = 0;
			else
				priv = 1;
#else
			priv = (in6p->in6p_socket->so_state & SS_PRIV);
#endif
			if (m) {
				req = mtod(m, caddr_t);
				len = m->m_len;
			}
			error = ipsec4_set_policy(inp, optname, req, len, priv);
			break;
		    }
#endif /*IPSEC*/

		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			(void)m_free(m);
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case IP_OPTIONS:
		case IP_RETOPTS:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			if (inp->inp_options) {
				m->m_len = inp->inp_options->m_len;
				bcopy(mtod(inp->inp_options, caddr_t),
				    mtod(m, caddr_t), (unsigned)m->m_len);
			} else
				m->m_len = 0;
			break;

		case IP_TOS:
		case IP_TTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVIF:
		case IP_ERRORMTU:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);
			switch (optname) {

			case IP_TOS:
				optval = inp->inp_ip.ip_tos;
				break;

			case IP_TTL:
				optval = inp->inp_ip.ip_ttl;
				break;

			case IP_ERRORMTU:
				optval = inp->inp_errormtu;
				break;

#define	OPTBIT(bit)	(inp->inp_flags & bit ? 1 : 0)

			case IP_RECVOPTS:
				optval = OPTBIT(INP_RECVOPTS);
				break;

			case IP_RECVRETOPTS:
				optval = OPTBIT(INP_RECVRETOPTS);
				break;

			case IP_RECVDSTADDR:
				optval = OPTBIT(INP_RECVDSTADDR);
				break;

			case IP_RECVIF:
				optval = OPTBIT(INP_RECVIF);
				break;
			}
			*mtod(m, int *) = optval;
			break;

#ifdef IPSEC
		case IP_IPSEC_POLICY:
		{
			caddr_t req = NULL;
			size_t len = 0;

			if (m) {
				req = mtod(m, caddr_t);
				len = m->m_len;
			}
			error = ipsec4_get_policy(inp, req, len, mp);
			break;
		}
#endif /*IPSEC*/

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_getmoptions(optname, inp->inp_moptions, mp);
			break;

		case IP_PORTRANGE:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);

			if (inp->inp_flags & INP_LOWPORT)
				optval = IP_PORTRANGE_LOW;
			else
				optval = IP_PORTRANGE_DEFAULT;

			*mtod(m, int *) = optval;
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}

/*
 * Set up IP options in pcb for insertion in output packets.
 * Store in mbuf with pointer in pcbopt, adding pseudo-option
 * with destination address if source routed.
 */
int
#ifdef notyet
ip_pcbopts(optname, pcbopt, m)
	int optname;
#else
ip_pcbopts(pcbopt, m)
#endif
	struct mbuf **pcbopt;
	struct mbuf *m;
{
	int cnt, optlen;
	u_char *cp;
	u_char opt;

	/* turn off any old options */
	if (*pcbopt)
		(void)m_free(*pcbopt);
	*pcbopt = 0;
	if (m == (struct mbuf *)0 || m->m_len == 0) {
		/*
		 * Only turning off any previous options.
		 */
		if (m)
			(void)m_free(m);
		return (0);
	}

#ifndef	__vax__
	if (m->m_len % sizeof(int32_t))
		goto bad;
#endif
	/*
	 * IP first-hop destination address will be stored before
	 * actual options; move other options back
	 * and clear it when none present.
	 */
	if (m->m_data + m->m_len + sizeof(struct in_addr) >= &m->m_dat[MLEN])
		goto bad;
	cnt = m->m_len;
	m->m_len += sizeof(struct in_addr);
	cp = mtod(m, u_char *) + sizeof(struct in_addr);
	memmove(cp, mtod(m, caddr_t), (unsigned)cnt);
	bzero(mtod(m, caddr_t), sizeof(struct in_addr));

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < IPOPT_OLEN + sizeof(*cp))
				goto bad;
			optlen = cp[IPOPT_OLEN];
			if (optlen < IPOPT_OLEN  + sizeof(*cp) || optlen > cnt)
				goto bad;
		}
		switch (opt) {

		default:
			break;

		case IPOPT_LSRR:
		case IPOPT_SSRR:
			/*
			 * user process specifies route as:
			 *	->A->B->C->D
			 * D must be our final destination (but we can't
			 * check that since we may not have connected yet).
			 * A is first hop destination, which doesn't appear in
			 * actual IP option, but is stored before the options.
			 */
			if (optlen < IPOPT_MINOFF - 1 + sizeof(struct in_addr))
				goto bad;
			m->m_len -= sizeof(struct in_addr);
			cnt -= sizeof(struct in_addr);
			optlen -= sizeof(struct in_addr);
			cp[IPOPT_OLEN] = optlen;
			/*
			 * Move first hop before start of options.
			 */
			bcopy((caddr_t)&cp[IPOPT_OFFSET+1], mtod(m, caddr_t),
			    sizeof(struct in_addr));
			/*
			 * Then copy rest of options back
			 * to close up the deleted entry.
			 */
			memmove(&cp[IPOPT_OFFSET+1],
                            (caddr_t)(&cp[IPOPT_OFFSET+1] + sizeof(struct in_addr)),
			    (unsigned)cnt + sizeof(struct in_addr));
			break;
		}
	}
	if (m->m_len > MAX_IPOPTLEN + sizeof(struct in_addr))
		goto bad;
	*pcbopt = m;
	return (0);

bad:
	(void)m_free(m);
	return (EINVAL);
}

/*
 * following RFC1724 section 3.3, 0.0.0.0/8 is interpreted as interface index.
 */
struct ifnet *
ip_multicast_if(a, ifindexp)
	struct in_addr *a;
	int *ifindexp;
{
	int ifindex;
	struct ifnet *ifp;

	if (ifindexp)
		*ifindexp = 0;
	if (ntohl(a->s_addr) >> 24 == 0) {
		ifindex = ntohl(a->s_addr) & 0xffffff;
		if (ifindex < 0 || if_indexlim <= ifindex)
			return NULL;
		ifp = ifindex2ifnet[ifindex];
		if (ifindexp)
			*ifindexp = ifindex;
	} else {
		INADDR_TO_IFP(*a, ifp);
	}
	return ifp;
}

/*
 * Set the IP multicast options in response to user setsockopt().
 */
int
ip_setmoptions(optname, imop, m)
	int optname;
	struct ip_moptions **imop;
	struct mbuf *m;
{
	int error = 0;
	u_char loop;
	int i;
	struct in_addr addr;
	struct ip_mreq *mreq;
	struct ifnet *ifp;
	struct ip_moptions *imo = *imop;
	int ifindex;
#ifdef IGMPV3
	struct sockaddr_storage ss_src, ss_grp;
	struct sockaddr_storage *del_ss;
	u_int16_t numsrc;
	u_int mode;
	int init;		/* indicate initial group join */
	int final;		/* indicate final group leave */
#endif

	if (imo == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		imo = (struct ip_moptions *)malloc(sizeof(*imo), M_IPMOPTS,
		    M_WAITOK);

		if (imo == NULL)
			return (ENOBUFS);
		*imop = imo;
		imo->imo_multicast_ifp = NULL;
		imo->imo_multicast_addr.s_addr = INADDR_ANY;
		imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
		imo->imo_num_memberships = 0;
	}

	switch (optname) {

	case IP_MULTICAST_IF:
		/*
		 * Select the interface for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != sizeof(struct in_addr)) {
			error = EINVAL;
			break;
		}
		addr = *(mtod(m, struct in_addr *));
		/*
		 * INADDR_ANY is used to remove a previous selection.
		 * When no interface is selected, a default one is
		 * chosen every time a multicast packet is sent.
		 */
		if (in_nullhost(addr)) {
			imo->imo_multicast_ifp = NULL;
			break;
		}
		/*
		 * The selected interface is identified by its local
		 * IP address.  Find the interface and confirm that
		 * it supports multicasting.
		 */
		ifp = ip_multicast_if(&addr, &ifindex);
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		imo->imo_multicast_ifp = ifp;
		if (ifindex)
			imo->imo_multicast_addr = addr;
		else
			imo->imo_multicast_addr.s_addr = INADDR_ANY;
		break;

	case IP_MULTICAST_TTL:
		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_ttl = *(mtod(m, u_char *));
		break;

	case IP_MULTICAST_LOOP:
		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.
		 */
		if (m == NULL || m->m_len != 1 ||
		   (loop = *(mtod(m, u_char *))) > 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_loop = loop;
		break;

	case IP_ADD_MEMBERSHIP:
		/*
		 * Add a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(mreq->imr_multiaddr.s_addr)) {
			error = EINVAL;
			break;
		}

		if ((error = ip_getmopt_ifargs(optname, &ifp,
						&mreq->imr_multiaddr,
						&mreq->imr_interface)) != 0)
			break;

		/*
		 * See if the membership already exists or if all the
		 * membership slots are full.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifp == ifp &&
			    in_hosteq(imo->imo_membership[i]->inm_addr,
				      mreq->imr_multiaddr))
				break;
		}
		if (i < imo->imo_num_memberships) {
			error = EADDRINUSE;
			break;
		}
		if (i == IP_MAX_MEMBERSHIPS) {
			error = ETOOMANYREFS;
			break;
		}

		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
#ifdef IGMPV3
		/*
		 * Even this request doesn't add any source filter, create
		 * msf entry list. This is needed to indicate current msf state.
		 */
		IMO_MSF_ALLOC(imo->imo_msf[i]);
		if (error != 0)
			break;
		init = 1;
		if ((imo->imo_membership[i] =
		    in_addmulti(&mreq->imr_multiaddr, ifp, 0, NULL,
				MCAST_EXCLUDE, init, &error)) == NULL) {
			IMO_MSF_FREE(imo->imo_msf[i]);
			break;
		}
#else
		if ((imo->imo_membership[i] =
		    in_addmulti(&mreq->imr_multiaddr, ifp)) == NULL) {
			error = ENOBUFS;
			break;
		}
#endif
#ifdef IGMPV3
		imo->imo_msf[i]->msf_grpjoin = 1;
#endif
		++imo->imo_num_memberships;
		break;

	case IP_DROP_MEMBERSHIP:
		/*
		 * Drop a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(mreq->imr_multiaddr.s_addr)) {
			error = EINVAL;
			break;
		}

		if ((error = ip_getmopt_ifargs(optname, &ifp, NULL,
						&mreq->imr_interface)) != 0)
			break;

		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if ((ifp == NULL ||
			     imo->imo_membership[i]->inm_ifp == ifp) &&
			     in_hosteq(imo->imo_membership[i]->inm_addr,
				       mreq->imr_multiaddr))
				break;
		}
		if (i == imo->imo_num_memberships) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * Give up the multicast address record to which the
		 * membership points if there is no member.
		 */
#ifdef IGMPV3
		error = in_getmopt_source_list(imo->imo_msf[i], &numsrc,
					   &del_ss, &mode);
		if (error != 0) {
			in_undomopt_source_addr(imo->imo_msf[i], optname);
			if (del_ss != NULL)
				FREE(del_ss, M_IPMOPTS);
			break;
		}

		final = 1;
		in_delmulti(imo->imo_membership[i], numsrc, del_ss, mode,
			    final, &error);
		if (del_ss != NULL)
			FREE(del_ss, M_IPMOPTS);
		in_freemopt_source_list(imo->imo_msf[i],
					imo->imo_msf[i]->msf_head,
					imo->imo_msf[i]->msf_blkhead);
		IMO_MSF_FREE(imo->imo_msf[i]);
#else
		in_delmulti(imo->imo_membership[i]);
#endif

		/*
		 * Remove the gap in the membership array.
		 */
		for (++i; i < imo->imo_num_memberships; ++i) {
			imo->imo_membership[i-1] = imo->imo_membership[i];
			imo->imo_msf[i-1] = imo->imo_msf[i];
		}
		--imo->imo_num_memberships;
		break;

#ifdef IGMPV3
	case MCAST_JOIN_GROUP:
		error = ip_getmopt_sgaddr(m, optname, &ifp, &ss_grp, NULL);
		if (error != 0)
			break;
		/*
		 * See if all the membership slots are full.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifp == ifp &&
			    in_hosteq(imo->imo_membership[i]->inm_addr,
			    	      SIN(&ss_grp)->sin_addr))
				break;
		}
		if (i < imo->imo_num_memberships) {
			error = EADDRINUSE;
			break;
		}
		if (i == IP_MAX_MEMBERSHIPS) {
			error = ETOOMANYREFS;
			break;
		}

		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
		IMO_MSF_ALLOC(imo->imo_msf[i]);
		if (error != 0)
			break;
		init = 1;
		if ((imo->imo_membership[i] =
		    in_addmulti(&SIN(&ss_grp)->sin_addr, ifp, 0, NULL,
		    		MCAST_EXCLUDE, init, &error)) == NULL) {
			IMO_MSF_FREE(imo->imo_msf[i]);
			break;
		}

		imo->imo_msf[i]->msf_grpjoin = 1;
		++imo->imo_num_memberships;
		break;

	case MCAST_LEAVE_GROUP:
		error = ip_getmopt_sgaddr(m, optname, &ifp, &ss_grp, NULL);
		if (error != 0)
			break;
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if ((ifp == NULL ||
			     imo->imo_membership[i]->inm_ifp == ifp) &&
			     in_hosteq(imo->imo_membership[i]->inm_addr,
				       SIN(&ss_grp)->sin_addr))
				break;
		}
		if (i == imo->imo_num_memberships) {
			error = EADDRNOTAVAIL;
			break;
		}

		error = in_getmopt_source_list(imo->imo_msf[i], &numsrc,
					   &del_ss, &mode);
		if (error != 0) {
			in_undomopt_source_addr(imo->imo_msf[i], optname);
			if (del_ss != NULL)
				FREE(del_ss, M_IPMOPTS);
			break;
		}

		final = 1;
		in_delmulti(imo->imo_membership[i], numsrc, del_ss, mode,
			    final, &error);
		if (del_ss != NULL)
			FREE(del_ss, M_IPMOPTS);
		in_freemopt_source_list(imo->imo_msf[i],
					imo->imo_msf[i]->msf_head,
					imo->imo_msf[i]->msf_blkhead);
		IMO_MSF_FREE(imo->imo_msf[i]);

		/*
		 * Remove the gap in the membership array.
		 */
		for (++i; i < imo->imo_num_memberships; ++i) {
			imo->imo_membership[i-1] = imo->imo_membership[i];
			imo->imo_msf[i-1] = imo->imo_msf[i];
		}
		--imo->imo_num_memberships;
		break;

	case IP_ADD_SOURCE_MEMBERSHIP:
	case MCAST_JOIN_SOURCE_GROUP:
		error = ip_getmopt_sgaddr(m, optname, &ifp, &ss_grp, &ss_src);
		if (error != 0)
			break;
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifp == ifp &&
			    in_hosteq(imo->imo_membership[i]->inm_addr,
			    	      SIN(&ss_grp)->sin_addr))
				break;
		}
		if (i == IP_MAX_MEMBERSHIPS) {
			error = ETOOMANYREFS;
			break;
		}
		if (i < imo->imo_num_memberships) {
			/*
			 * If Any-Source join was already requested, return
			 * EINVAL.
			 */
			if (imo->imo_msf[i]->msf_grpjoin != 0) {
				error = EINVAL;
				break;
			}
			/*
			 * If there is EXCLUDE msf state, return EINVAL.
			 */
			if (imo->imo_msf[i]->msf_blknumsrc != 0) {
				error = EINVAL;
				break;
			}
			/*
			 * If the implementation imposes a limit on the
			 * maximum number of sources in a source filter,
			 * ENOBUFS is generated.
			 */
			if (imo->imo_msf[i]->msf_numsrc >= igmpsomaxsrc) {
				error = ENOBUFS;
				break;
			}
			init = 0;
		} else {
			IMO_MSF_ALLOC(imo->imo_msf[i]);
			if (error != 0)
				break;
			init = 1;
		}

		/*
		 * Set source address to the msf.
		 * If requested source address was already in the socket list,
		 * return EADDRNOTAVAIL. 
		 * If there is not enough memory, return ENOBUFS.
		 * Otherwise, 0 will be returned, which means okay.
		 */
		error = in_setmopt_source_addr(&ss_src, imo->imo_msf[i],
					       optname);
		if (error != 0) {
			if (init)
				IMO_MSF_FREE(imo->imo_msf[i]);
			break;
		}

		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 * But if some error occurs when source list is added to
		 * the list, undo added msf list from the socket.
		 */
		numsrc = 1;
		imo->imo_membership[i] =
			in_addmulti(&SIN(&ss_grp)->sin_addr, ifp, numsrc,
				    &ss_src, MCAST_INCLUDE, init, &error);
		if (imo->imo_membership[i] == NULL) {
			in_undomopt_source_addr(imo->imo_msf[i], optname);
			break;
		}
		in_cleanmopt_source_addr(imo->imo_msf[i], optname);

		if (init)
			++imo->imo_num_memberships;
		break;

	case IP_DROP_SOURCE_MEMBERSHIP:
	case MCAST_LEAVE_SOURCE_GROUP:
		error = ip_getmopt_sgaddr(m, optname, &ifp, &ss_grp, &ss_src);
		if (error != 0)
			break;
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if ((ifp == NULL ||
			     imo->imo_membership[i]->inm_ifp == ifp) &&
			     in_hosteq(imo->imo_membership[i]->inm_addr,
				       SIN(&ss_grp)->sin_addr))
				break;
		}
		if (i == imo->imo_num_memberships) {
			error = EADDRNOTAVAIL;
			break;
		}

		/*
		 * Remove source address from the msf.
		 * If (*,G) join or EXCLUDE join was requested previously,
		 * return EINVAL.
		 * If requested source address was not in the socket list,
		 * return EADDRNOTAVAIL. 
		 * If there is not enough memory, return ENOBUFS.
		 * Otherwise, 0 will be returned, which means okay.
		 */
		if ((imo->imo_msf[i]->msf_grpjoin != 0) ||
				(imo->imo_msf[i]->msf_blknumsrc != 0)) {
			error = EINVAL;
			break;
		}
		error = in_setmopt_source_addr(&ss_src, imo->imo_msf[i],
					       optname);
		if (error != 0)
			break;
		if (imo->imo_msf[i]->msf_numsrc == 0)
			final = 1;
		else
			final = 0;

		/*
		 * Give up the multicast address record to which the
		 * membership points.
		 */
		numsrc = 1;
		in_delmulti(imo->imo_membership[i], numsrc, &ss_src,
				MCAST_INCLUDE, final, &error);
		if (error != 0) {
			printf("ip_setmoptions: error must be 0! panic!\n");
			in_undomopt_source_addr(imo->imo_msf[i], optname);
			break; /* strange... */
		}
		in_cleanmopt_source_addr(imo->imo_msf[i], optname);

		/*
		 * Remove the gap in the membership array if there is no
		 * msf member.
		 */
		if (final) {
			for (++i; i < imo->imo_num_memberships; ++i) {
				imo->imo_membership[i-1]
						= imo->imo_membership[i];
				imo->imo_msf[i-1] = imo->imo_msf[i];
			}
			--imo->imo_num_memberships;
		}
		break;

	case IP_BLOCK_SOURCE:
	case MCAST_BLOCK_SOURCE:
		error = ip_getmopt_sgaddr(m, optname, &ifp, &ss_grp, &ss_src);
		if (error != 0)
			break;
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifp == ifp &&
			    in_hosteq(imo->imo_membership[i]->inm_addr,
				      SIN(&ss_grp)->sin_addr))
				break;
		}
		if (i == IP_MAX_MEMBERSHIPS) {
			error = ETOOMANYREFS;
			break;
		}
		if (i < imo->imo_num_memberships) {
			/*
			 * If there is INCLUDE msf state, return EINVAL.
			 */
			if (imo->imo_msf[i]->msf_numsrc != 0) {
				error = EINVAL;
				break;
			}
			if (imo->imo_msf[i]->msf_blknumsrc >= igmpsomaxsrc) {
				error = ENOBUFS;
				break;
			}
			init = 0;
		} else {
			IMO_MSF_ALLOC(imo->imo_msf[i]);
			if (error != 0)
				break;
			init = 1;
		}

		/*
		 * Set source address to the msf.
		 * If requested source address was already in the socket list,
		 * return EADDRNOTAVAIL. 
		 * If there is not enough memory, return ENOBUFS.
		 * Otherwise, 0 will be returned, which means okay.
		 */
		error = in_setmopt_source_addr(&ss_src, imo->imo_msf[i],
					       optname);
		if (error != 0) {
			if (init)
				IMO_MSF_FREE(imo->imo_msf[i]);
			break;
		}

		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 * But if some error occurs when source list is added to
		 * the list, undo added msf list from the socket.
		 */
		numsrc = 1;
		if (imo->imo_msf[i]->msf_grpjoin == 0) {
			/* IN{NULL}/EX{non NULL} -> EX{non NULL} */
			imo->imo_membership[i] =
				in_addmulti(&SIN(&ss_grp)->sin_addr, ifp,
					    numsrc, &ss_src, MCAST_EXCLUDE,
					    init, &error);
			if (imo->imo_membership[i] == NULL) {
				in_undomopt_source_addr
					(imo->imo_msf[i], optname);
				break;
			}
		} else {
			/* EX{NULL} -> EX{non NULL} */
			imo->imo_membership[i] =
				in_modmulti(&SIN(&ss_grp)->sin_addr,
					    ifp, numsrc, &ss_src, MCAST_EXCLUDE,
					    0, NULL, MCAST_EXCLUDE, init,
					    imo->imo_msf[i]->msf_grpjoin, &error);
			if (imo->imo_membership[i] == NULL) {
				in_undomopt_source_addr
					(imo->imo_msf[i], optname);
				break;
			}
			imo->imo_msf[i]->msf_grpjoin = 0;
		}
		in_cleanmopt_source_addr(imo->imo_msf[i], optname);

		if (init)
			++imo->imo_num_memberships;
		break;

	case IP_UNBLOCK_SOURCE:
	case MCAST_UNBLOCK_SOURCE:
		error = ip_getmopt_sgaddr(m, optname, &ifp, &ss_grp, &ss_src);
		if (error != 0)
			break;
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if ((ifp == NULL ||
			     imo->imo_membership[i]->inm_ifp == ifp) &&
			     in_hosteq(imo->imo_membership[i]->inm_addr,
				       SIN(&ss_grp)->sin_addr))
				break;
		}
		if (i == imo->imo_num_memberships) {
			error = EADDRNOTAVAIL;
			break;
		}

		/*
		 * Remove source address from the msf.
		 * If (*,G) join or INCLUDE join was requested previously,
		 * return EINVAL.
		 * If requested source address was not in the socket list,
		 * return EADDRNOTAVAIL. 
		 * If there is not enough memory, return ENOBUFS.
		 * Otherwise, 0 will be returned, which means okay.
		 */
		if ((imo->imo_msf[i]->msf_grpjoin != 0) ||
				(imo->imo_msf[i]->msf_numsrc != 0)) {
			error = EINVAL;
			break;
		}
		error = in_setmopt_source_addr(&ss_src, imo->imo_msf[i],
					       optname);
		if (error != 0)
			break;
		if (imo->imo_msf[i]->msf_blknumsrc == 0)
			final = 1;
		else
			final = 0;

		/*
		 * Give up the multicast address record to which the
		 * membership points.
		 */
		numsrc = 1;
		in_delmulti(imo->imo_membership[i], numsrc, &ss_src,
				MCAST_EXCLUDE, final, &error);
		if (error != 0) {
			printf("ip_setmoptions: error must be 0! panic!\n");
			in_undomopt_source_addr(imo->imo_msf[i], optname);
			break; /* strange... */
		}
		in_cleanmopt_source_addr(imo->imo_msf[i], optname);

		/*
		 * Remove the gap in the membership array if there is no
		 * msf member.
		 */
		if (final) {
			for (++i; i < imo->imo_num_memberships; ++i) {
				imo->imo_membership[i-1]
						= imo->imo_membership[i];
				imo->imo_msf[i-1] = imo->imo_msf[i];
			}
			--imo->imo_num_memberships;
		}
		break;
#endif /* IGMPV3 */

	default:
		error = EOPNOTSUPP;
		break;
	}

	/*
	 * If all options have default values, no need to keep the mbuf.
	 */
	if (imo->imo_multicast_ifp == NULL &&
	    imo->imo_multicast_ttl == IP_DEFAULT_MULTICAST_TTL &&
	    imo->imo_multicast_loop == IP_DEFAULT_MULTICAST_LOOP &&
	    imo->imo_num_memberships == 0) {
		free(*imop, M_IPMOPTS);
		*imop = NULL;
	}

	return (error);
}

/*
 * Return the IP multicast options in response to user getsockopt().
 */
int
ip_getmoptions(optname, imo, mp)
	int optname;
	struct ip_moptions *imo;
	struct mbuf **mp;
{
	u_char *ttl;
	u_char *loop;
	struct in_addr *addr;
	struct in_ifaddr *ia;

	*mp = m_get(M_WAIT, MT_SOOPTS);

	switch (optname) {

	case IP_MULTICAST_IF:
		addr = mtod(*mp, struct in_addr *);
		(*mp)->m_len = sizeof(struct in_addr);
		if (imo == NULL || imo->imo_multicast_ifp == NULL)
			*addr = zeroin_addr;
		else if (imo->imo_multicast_addr.s_addr) {
			/* return the value user has set */
			*addr = imo->imo_multicast_addr;
		} else {
			IFP_TO_IA(imo->imo_multicast_ifp, ia);
			*addr = ia ? ia->ia_addr.sin_addr : zeroin_addr;
		}
		return (0);

	case IP_MULTICAST_TTL:
		ttl = mtod(*mp, u_char *);
		(*mp)->m_len = 1;
		*ttl = imo ? imo->imo_multicast_ttl
			   : IP_DEFAULT_MULTICAST_TTL;
		return (0);

	case IP_MULTICAST_LOOP:
		loop = mtod(*mp, u_char *);
		(*mp)->m_len = 1;
		*loop = imo ? imo->imo_multicast_loop
			    : IP_DEFAULT_MULTICAST_LOOP;
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Discard the IP multicast options.
 */
void
ip_freemoptions(imo)
	struct ip_moptions *imo;
{
	int i;
#ifdef IGMPV3
	struct sockaddr_storage *del_ss;
	u_int16_t numsrc;
	u_int mode;
	int final;
	int error;
#endif

	if (imo != NULL) {
		for (i = 0; i < imo->imo_num_memberships; ++i) {
#ifdef IGMPV3
			error = in_getmopt_source_list(imo->imo_msf[i], &numsrc,
						   &del_ss, &mode);
			if (error != 0) {
				/* XXX strange... panic or skip ? */
				/*
				in_undomopt_source_addr(imo->imo_msf[i],
							optname);
				 */
				if (del_ss != NULL)
					FREE(del_ss, M_IPMOPTS);
				printf("ip_freemoptions: error must be 0! panic!\n");
				continue; /* XXX */
			}

			final = 1;
			in_delmulti(imo->imo_membership[i], numsrc, del_ss,
				    mode, final, &error);
			if (del_ss != NULL)
				FREE(del_ss, M_IPMOPTS);
			in_freemopt_source_list(imo->imo_msf[i],
						imo->imo_msf[i]->msf_head,
						imo->imo_msf[i]->msf_blkhead);
			IMO_MSF_FREE(imo->imo_msf[i]);
#else
			in_delmulti(imo->imo_membership[i]);
#endif
		}
		free(imo, M_IPMOPTS);
	}
}

static int
ip_getmopt_ifargs(optname, ifp, ia_grp, ia_ifa)
	int optname;
	struct ifnet **ifp;
	struct in_addr *ia_grp, *ia_ifa;
{
	struct route ro;
	struct sockaddr_in *dst;
	int error = 0;

	switch (optname) {
	case IP_ADD_MEMBERSHIP:
#ifdef IGMPV3
	case IP_ADD_SOURCE_MEMBERSHIP:
	case IP_BLOCK_SOURCE:
#endif
		/*
		 * If no interface address was provided, use the interface of
		 * the route to the given multicast address.
		 */
		if (in_nullhost(*ia_ifa)) {
			bzero((caddr_t)&ro, sizeof(ro));
			ro.ro_rt = NULL;
			dst = satosin(&ro.ro_dst);
			dst->sin_len = sizeof(struct sockaddr_in);
			dst->sin_family = AF_INET;
			dst->sin_addr = *ia_grp;
			rtalloc(&ro);
			if (ro.ro_rt == NULL) {
				error = EADDRNOTAVAIL;
				return error;
			}
			*ifp = ro.ro_rt->rt_ifp;
			rtfree(ro.ro_rt);
		} else {
			*ifp = ip_multicast_if(ia_ifa, NULL);
		}
		/*
		 * See if we found an interface, and confirm that it supports
		 * multicast.
		 */
		if (*ifp == NULL || ((*ifp)->if_flags & IFF_MULTICAST) == 0)
			error = EADDRNOTAVAIL;
		break;

	case IP_DROP_MEMBERSHIP:
#ifdef IGMPV3
	case IP_DROP_SOURCE_MEMBERSHIP:
	case IP_UNBLOCK_SOURCE:
#endif
		/*
		 * If an interface address was specified, get a pointer
		 * to its ifnet structure.
		 */
		if (in_nullhost(*ia_ifa))
			*ifp = NULL;
		else {
			*ifp = ip_multicast_if(ia_ifa, NULL);
			if (*ifp == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
		}
		break;
	}
	return error;
}

#ifdef IGMPV3
static int
in_getmopt_ifargs(optname, ifp, ia_grp, index)
	int optname;
	struct ifnet **ifp;
	struct in_addr *ia_grp;
	u_int32_t index;
{
	struct route ro;
	struct sockaddr_in *dst;
	int error = 0;

	/*
	 * If the interface is specified, validate it.
	 */
	if (index < 0 || if_indexlim <= index)
		return ENXIO;	/* XXX EINVAL? */

	switch (optname) {
	case MCAST_JOIN_GROUP:
	case MCAST_BLOCK_SOURCE:
	case MCAST_JOIN_SOURCE_GROUP:
		/*
		 * If no interface was explicitly specified, choose an
		 * appropriate one according to the given multicast address.
		 */
		if (index == 0) {
			bzero((caddr_t)&ro, sizeof(ro));
			ro.ro_rt = NULL;
			dst = satosin(&ro.ro_dst);
			dst->sin_len = sizeof(struct sockaddr_in);
			dst->sin_family = AF_INET;
			dst->sin_addr = *ia_grp;
			rtalloc((struct route *)&ro);
			if (ro.ro_rt == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
			*ifp = ro.ro_rt->rt_ifp;
			rtfree(ro.ro_rt);
		} else
			*ifp = ifindex2ifnet[index];

		if (*ifp == NULL || ((*ifp)->if_flags & IFF_MULTICAST) == 0) {
#ifdef IGMPV3_DEBUG
			printf("invalid interface (#%d) specified", index);
#endif
			error = EINVAL;
		}
		break;

	case MCAST_LEAVE_GROUP:
	case MCAST_UNBLOCK_SOURCE:
	case MCAST_LEAVE_SOURCE_GROUP:
		/*
		 * If an interface address was specified, get a pointer
		 * to its ifnet structure.
		 */
		if (index == 0)
			*ifp = NULL;
		else {
			*ifp = ifindex2ifnet[index];
			if (*ifp == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
		}
		break;
	}
	return error;
}

static int
ip_getmopt_sgaddr(m, optname, ifp, ss_grp, ss_src)
	struct mbuf *m;
	int optname;
	struct ifnet **ifp;
	struct sockaddr_storage *ss_grp;
	struct sockaddr_storage *ss_src;
{
	struct sockaddr_in *sin_src, *sin_grp;
	int error = 0;

	switch (optname) {
	case MCAST_JOIN_GROUP:
	case MCAST_LEAVE_GROUP:
	    {
		struct group_req *greq;

		if (m == NULL || m->m_len != sizeof(struct group_req)) {
			error = EINVAL;
			break;
		}

		greq = mtod(m, struct group_req *);
		if (greq->gr_group.ss_family != AF_INET) {
			error = EPFNOSUPPORT;
			break;
		}

		sin_grp = SIN(ss_grp);
		sin_grp->sin_addr = SIN(&greq->gr_group)->sin_addr;
		sin_grp->sin_len = sizeof(*sin_grp);
		sin_grp->sin_family = AF_INET;

		if (!IN_MULTICAST(sin_grp->sin_addr.s_addr)) {
			error = EINVAL;
			break;
		}

		/*
		 * Get a pointer to the ifnet structure.
		 */
		error = in_getmopt_ifargs(optname, ifp, &sin_grp->sin_addr,
					  greq->gr_interface);

		break;
	    }

	case IP_BLOCK_SOURCE:
	case IP_UNBLOCK_SOURCE:
	case IP_ADD_SOURCE_MEMBERSHIP:
	case IP_DROP_SOURCE_MEMBERSHIP:
	    {
		struct ip_mreq_source *mreqsrc;
		struct sockaddr_in sin_ifa;

		if (m == NULL || m->m_len != sizeof(struct ip_mreq_source)) {
			error = EINVAL;
			break;
		}

		mreqsrc = mtod(m, struct ip_mreq_source *);

		sin_src = SIN(ss_src);
		sin_src->sin_addr = mreqsrc->imr_sourceaddr;
		sin_src->sin_len = sizeof(*sin_src);
		sin_src->sin_family = AF_INET;
		sin_grp = SIN(ss_grp);
		sin_grp->sin_addr = mreqsrc->imr_multiaddr;
		sin_grp->sin_len = sizeof(*sin_grp);
		sin_grp->sin_family = AF_INET;
		sin_ifa.sin_addr = mreqsrc->imr_interface;

		/*
		 * Group must be a valid IP multicast address.
		 */
		if (!IN_MULTICAST(sin_grp->sin_addr.s_addr) ||
				IN_LOCAL_GROUP(sin_grp->sin_addr.s_addr)) {
#ifdef IGMPV3_DEBUG
			printf("invalid group %s specified\n", inet_ntoa(sin_grp->sin_addr));
#endif
			error = EINVAL;
			break;
		}
		/*
		 * If no source address was provided or was class-d, bad
		 * class, return error.
		 */
		if (IN_MULTICAST(sin_src->sin_addr.s_addr) ||
			    IN_BADCLASS(sin_src->sin_addr.s_addr) ||
			    (sin_src->sin_addr.s_addr & IN_CLASSA_NET) == 0) {
#ifdef IGMPV3_DEBUG
			printf("invalid source %s specified\n", inet_ntoa(sin_src->sin_addr));
#endif
			error = EINVAL;
			break;
		}

		/*
		 * Get a pointer to the ifnet structure.
		 */
		error = ip_getmopt_ifargs(optname, ifp, &sin_grp->sin_addr,
					  &sin_ifa.sin_addr);

		break;
	    }

	case MCAST_BLOCK_SOURCE:
	case MCAST_UNBLOCK_SOURCE:
	case MCAST_JOIN_SOURCE_GROUP:
	case MCAST_LEAVE_SOURCE_GROUP:
	    {
		struct group_source_req *gsreq;

		if (ss_src == NULL || ss_grp == NULL || m == NULL ||
				m->m_len != sizeof(struct group_source_req)) {
			error = EINVAL;
			break;
		}

		gsreq = mtod(m, struct group_source_req *);
		if ((gsreq->gsr_group.ss_family != AF_INET) ||
				(gsreq->gsr_source.ss_family != AF_INET)) {
			error = EPFNOSUPPORT;
			break;
		}

		sin_src = SIN(ss_src);
		sin_src->sin_addr = SIN(&gsreq->gsr_source)->sin_addr;
		sin_src->sin_len = sizeof(*sin_src);
		sin_src->sin_family = AF_INET;
		sin_grp = SIN(ss_grp);
		sin_grp->sin_addr = SIN(&gsreq->gsr_group)->sin_addr;
		sin_grp->sin_len = sizeof(*sin_grp);
		sin_grp->sin_family = AF_INET;

		if (!IN_MULTICAST(sin_grp->sin_addr.s_addr) ||
				IN_LOCAL_GROUP(sin_grp->sin_addr.s_addr)) {
			error = EINVAL;
			break;
		}
		if (IN_MULTICAST(sin_src->sin_addr.s_addr) ||
			    IN_BADCLASS(sin_src->sin_addr.s_addr) ||
			    (sin_src->sin_addr.s_addr & IN_CLASSA_NET) == 0) {
			error = EINVAL;
			break;
		}

		error = in_getmopt_ifargs(optname, ifp, &sin_grp->sin_addr,
					  gsreq->gsr_interface);

		break;
	    }
	}

	return error;
}
#endif /* IGMPV3 */

/*
 * Routine called from ip_output() to loop back a copy of an IP multicast
 * packet to the input queue of a specified interface.  Note that this
 * calls the output routine of the loopback "driver", but with an interface
 * pointer that might NOT be &loif -- easier than replicating that code here.
 */
static void
ip_mloopback(ifp, m, dst)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr_in *dst;
{
	struct ip *ip;
	struct mbuf *copym;

	copym = m_copy(m, 0, M_COPYALL);
	if (copym != NULL
	 && (copym->m_flags & M_EXT || copym->m_len < sizeof(struct ip)))
		copym = m_pullup(copym, sizeof(struct ip));
	if (copym != NULL) {
		/*
		 * We don't bother to fragment if the IP length is greater
		 * than the interface's MTU.  Can this possibly matter?
		 */
		ip = mtod(copym, struct ip *);
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);

		if (copym->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
			in_delayed_cksum(copym);
			copym->m_pkthdr.csum_flags &=
			    ~(M_CSUM_TCPv4|M_CSUM_UDPv4);
		}

		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(copym, ip->ip_hl << 2);
		(void) looutput(ifp, copym, sintosa(dst), NULL);
	}
}
