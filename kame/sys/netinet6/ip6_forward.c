/*	$KAME: ip6_forward.c,v 1.141 2004/06/24 15:03:32 itojun Exp $	*/

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

#ifdef __FreeBSD__
#include "opt_ip6fw.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_mip6.h"
#endif
#ifdef __NetBSD__
#include "opt_inet.h"
#include "opt_ipsec.h"
#include "opt_mip6.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#ifdef MIP6
#include <netinet/ip6mh.h>
#include <netinet6/mip6.h>
#include <netinet6/mip6_var.h>
#endif

#include <netinet/in_pcb.h>
#ifdef __NetBSD__
#include <netinet6/in6_pcb.h>
#endif

#if !(defined(__FreeBSD__) && __FreeBSD_version >= 503000)
#include "pf.h"
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef __OpenBSD__ /* KAME IPSEC */
#undef IPSEC
#endif

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#endif /* IPSEC */

#if defined(IPV6FIREWALL) || defined(__FreeBSD__)
#include <netinet6/ip6_fw.h>
#endif
#if ((defined(__NetBSD__) && defined(PFIL_HOOKS)) || (defined(__FreeBSD__) && __FreeBSD_version >= 503000))
#include <net/pfil.h>
#endif

#include <net/net_osdep.h>

#if defined(__FreeBSD__) && __FreeBSD__ > 4 && __FreeBSD_version < 500000
extern int (*fr_checkp) __P((struct ip *, int, struct ifnet *, int, struct mbuf **));
#endif

#ifdef NEW_STRUCT_ROUTE
struct	route ip6_forward_rt;
#else
struct	route_in6 ip6_forward_rt;
#endif

#if (defined(__NetBSD__) && defined(PFIL_HOOKS)) || (defined(__FreeBSD__) && __FreeBSD_version >= 503000)
extern struct pfil_head inet6_pfil_hook;	/* XXX */
#endif

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 */

void
ip6_forward(m, srcrt)
	struct mbuf *m;
	int srcrt;
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct sockaddr_in6 *dst = NULL;
	struct rtentry *rt = NULL;
	int error = 0, type = 0, code = 0;
	struct mbuf *mcopy = NULL;
	struct ifnet *origifp;	/* maybe unnecessary */
	u_int32_t dstzone;
	struct sockaddr_in6 sin6;
#ifdef IPSEC
	struct secpolicy *sp = NULL;
	int ipsecrt = 0;
#endif
#ifdef MIP6 
	struct mip6_bc_internal *bce;
#endif
#ifndef __FreeBSD__
	long time_second = time.tv_sec;
#endif

#ifdef IPSEC
	/*
	 * Check AH/ESP integrity.
	 */
	/*
	 * Don't increment ip6s_cantforward because this is the check
	 * before forwarding packet actually.
	 */
	if (ipsec6_in_reject(m, NULL)) {
		ipsec6stat.in_polvio++;
		m_freem(m);
		return;
	}
#endif /* IPSEC */

	/*
	 * Do not forward packets to multicast destination (should be handled
	 * by ip6_mforward().
	 * Do not forward packets with unspecified source.  It was discussed
	 * in July 2000, on the ipngwg mailing list.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) != 0 ||
	    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
		ip6stat.ip6s_cantforward++;
		/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard) */
		if (ip6_log_time + ip6_log_interval < time_second) {
			ip6_log_time = time_second;
			log(LOG_DEBUG,
			    "cannot forward "
			    "from %s to %s nxt %d received on %s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst),
			    ip6->ip6_nxt,
			    if_name(m->m_pkthdr.rcvif));
		}
		m_freem(m);
		return;
	}

	if (ip6->ip6_hlim <= IPV6_HLIMDEC) {
		/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard) */
		icmp6_error(m, ICMP6_TIME_EXCEEDED,
				ICMP6_TIME_EXCEED_TRANSIT, 0);
		return;
	}
	ip6->ip6_hlim -= IPV6_HLIMDEC;

	/*
	 * Save at most ICMPV6_PLD_MAXLEN (= the min IPv6 MTU -
	 * size of IPv6 + ICMPv6 headers) bytes of the packet in case
	 * we need to generate an ICMP6 message to the src.
	 * Thanks to M_EXT, in most cases copy will not occur.
	 *
	 * It is important to save it before IPsec processing as IPsec
	 * processing may modify the mbuf.
	 */
	mcopy = m_copy(m, 0, imin(m->m_pkthdr.len, ICMPV6_PLD_MAXLEN));

#ifdef IPSEC
	/* get a security policy for this packet */
	sp = ipsec6_getpolicybyaddr(m, IPSEC_DIR_OUTBOUND,
	    IP_FORWARDING, &error);
	if (sp == NULL) {
		ipsec6stat.out_inval++;
		ip6stat.ip6s_cantforward++;
		if (mcopy) {
#if 0
			/* XXX: what icmp ? */
#else
			m_freem(mcopy);
#endif
		}
		m_freem(m);
		return;
	}

	error = 0;

	/* check policy */
	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
		/*
		 * This packet is just discarded.
		 */
		ipsec6stat.out_polvio++;
		ip6stat.ip6s_cantforward++;
		key_freesp(sp);
		if (mcopy) {
#if 0
			/* XXX: what icmp ? */
#else
			m_freem(mcopy);
#endif
		}
		m_freem(m);
		return;

	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		/* no need to do IPsec. */
		key_freesp(sp);
		goto skip_ipsec;

	case IPSEC_POLICY_IPSEC:
		if (sp->req == NULL) {
			/* XXX should be panic ? */
			printf("ip6_forward: No IPsec request specified.\n");
			ip6stat.ip6s_cantforward++;
			key_freesp(sp);
			if (mcopy) {
#if 0
				/* XXX: what icmp ? */
#else
				m_freem(mcopy);
#endif
			}
			m_freem(m);
			return;
		}
		/* do IPsec */
		break;

	case IPSEC_POLICY_ENTRUST:
	default:
		/* should be panic ?? */
		printf("ip6_forward: Invalid policy found. %d\n", sp->policy);
		key_freesp(sp);
		goto skip_ipsec;
	}

    {
	struct ipsecrequest *isr = NULL;
	struct ipsec_output_state state;

	/*
	 * when the kernel forwards a packet, it is not proper to apply
	 * IPsec transport mode to the packet is not proper.  this check
	 * avoid from this.
	 * at present, if there is even a transport mode SA request in the
	 * security policy, the kernel does not apply IPsec to the packet.
	 * this check is not enough because the following case is valid.
	 *      ipsec esp/tunnel/xxx-xxx/require esp/transport//require;
	 */
	for (isr = sp->req; isr; isr = isr->next) {
		if (isr->saidx.mode == IPSEC_MODE_ANY)
			goto doipsectunnel;
		if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
			goto doipsectunnel;
	}

	/*
	 * if there's no need for tunnel mode IPsec, skip.
	 */
	if (!isr)
		goto skip_ipsec;
	
    doipsectunnel:
	/*
	 * All the extension headers will become inaccessible
	 * (since they can be encrypted).
	 * Don't panic, we need no more updates to extension headers
	 * on inner IPv6 packet (since they are now encapsulated).
	 *
	 * IPv6 [ESP|AH] IPv6 [extension headers] payload
	 */
	bzero(&state, sizeof(state));
	state.m = m;
	state.ro = NULL;	/* update at ipsec6_output_tunnel() */
	state.dst = NULL;	/* update at ipsec6_output_tunnel() */

	error = ipsec6_output_tunnel(&state, sp, 0);

	m = state.m;
	key_freesp(sp);

	if (error) {
		/* mbuf is already reclaimed in ipsec6_output_tunnel. */
		switch (error) {
		case EHOSTUNREACH:
		case ENETUNREACH:
		case EMSGSIZE:
		case ENOBUFS:
		case ENOMEM:
			break;
		default:
			printf("ip6_output (ipsec): error code %d\n", error);
			/* FALLTHROUGH */
		case ENOENT:
			/* don't show these error codes to the user */
			break;
		}
		ip6stat.ip6s_cantforward++;
		if (mcopy) {
#if 0
			/* XXX: what icmp ? */
#else
			m_freem(mcopy);
#endif
		}
		m_freem(m);
		return;
	}

	if (ip6 != mtod(m, struct ip6_hdr *)) {
		/*
		 * now tunnel mode headers are added.  we are originating
		 * packet instead of forwarding the packet.  
		 */
#if defined(__FreeBSD__) && __FreeBSD_version >= 480000
		ip6_output(m, NULL, NULL, IPV6_FORWARDING/*XXX*/, NULL, NULL,
		    NULL);
#else
		ip6_output(m, NULL, NULL, IPV6_FORWARDING/*XXX*/, NULL, NULL);
#endif
		goto freecopy;
	}

	/* adjust pointer */
	dst = (struct sockaddr_in6 *)state.dst;
	rt = state.ro ? state.ro->ro_rt : NULL;
	if (dst != NULL && rt != NULL)
		ipsecrt = 1;
    }
    skip_ipsec:
#endif /* IPSEC */

#ifdef MIP6
	/* This codes are only for Home Agent */
	if (!MIP6_IS_HA) 
		goto bc_check_done;
	/*
	 * intercept and tunnel packets for home addresses
	 * which we are acting as a home agent for.
	 */

#ifndef MIP6_MCOA
	bce = mip6_bce_get(&ip6->ip6_dst, NULL);
#else
	/* XXX need some policy to determine bid */
	bce = mip6_bce_get(&ip6->ip6_dst, NULL, NULL, 0); 
#endif /* MIP6_MCOA */
	if (bce &&
	     (bce->mbc_flags & IP6_MH_BU_HOME) &&
	     (bce->mbc_encap != NULL)) {
		if (IN6_IS_ADDR_LINKLOCAL(&bce->mbc_hoa)
		    || IN6_IS_ADDR_SITELOCAL(&bce->mbc_hoa)) {
			ip6stat.ip6s_cantforward++;
			if (mcopy) {
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
					    ICMP6_DST_UNREACH_ADDR, 0);
			}
			m_freem(m);
			return;
		}
	
		if (m->m_pkthdr.len > IPV6_MMTU) {
			u_long mtu = IPV6_MMTU;
			/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_toobig); */
			if (mcopy) {
				icmp6_error(mcopy,
					    ICMP6_PACKET_TOO_BIG, 0, mtu);
			}
			m_freem(m);
			return;
		}

		/*
		 * if we have a binding cache entry for the
		 * ip6_dst, we are acting as a home agent for
		 * that node.  before sending a packet as a
		 * tunneled packet, we must make sure that
		 * encaptab is ready.  if dad is enabled and
		 * not completed yet, encaptab will be NULL.
		 */
		if (mip6_encapsulate(&m, IFA_IN6(bce->mbc_ifaddr), 
			&bce->mbc_coa) != 0) {
			ip6stat.ip6s_cantforward++;
		}
		if (mcopy)
			m_freem(mcopy);
		return;
   bc_check_done:
		;
	}
#endif /* MIP6 */

#ifdef IPSEC
	if (ipsecrt)
		goto skip_routing;
#endif

	dst = (struct sockaddr_in6 *)&ip6_forward_rt.ro_dst;
	if (!srcrt) {
		/* ip6_forward_rt.ro_dst.sin6_addr is equal to ip6->ip6_dst */
		if (ip6_forward_rt.ro_rt == 0 ||
		    !(ip6_forward_rt.ro_rt->rt_flags & RTF_UP)
			) {
			if (ip6_forward_rt.ro_rt) {
				RTFREE(ip6_forward_rt.ro_rt);
				ip6_forward_rt.ro_rt = 0;
			}

			/* this probably fails but give it a try again */
#if defined(__FreeBSD__) && __FreeBSD_version < 502000
			rtalloc_ign((struct route *)&ip6_forward_rt,
				    RTF_PRCLONING);
#else
			rtalloc((struct route *)&ip6_forward_rt);
#endif
		}

		if (ip6_forward_rt.ro_rt == 0) {
			ip6stat.ip6s_noroute++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_noroute);
			if (mcopy) {
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
					    ICMP6_DST_UNREACH_NOROUTE, 0);
			}
			m_freem(m);
			return;
		}
	} else if ((rt = ip6_forward_rt.ro_rt) == 0 ||
		    !(ip6_forward_rt.ro_rt->rt_flags & RTF_UP) ||
		   !IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &dst->sin6_addr)) {
		if (ip6_forward_rt.ro_rt) {
			RTFREE(ip6_forward_rt.ro_rt);
			ip6_forward_rt.ro_rt = 0;
		}
		dst->sin6_addr = ip6->ip6_dst;
		dst->sin6_scope_id = 0;	/* XXX */
#if defined(__FreeBSD__) && __FreeBSD_version < 502000
		rtalloc_ign((struct route *)&ip6_forward_rt, RTF_PRCLONING);
#else
		rtalloc((struct route *)&ip6_forward_rt);
#endif
		if (ip6_forward_rt.ro_rt == 0) {
			ip6stat.ip6s_noroute++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_noroute);
			if (mcopy) {
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_NOROUTE, 0);
			}
			m_freem(m);
			return;
		}
	}
	rt = ip6_forward_rt.ro_rt;
#ifdef IPSEC
    skip_routing:;
#endif

	/*
	 * Source scope check: if a packet can't be delivered to its
	 * destination for the reason that the destination is beyond the scope
	 * of the source address, discard the packet and return an icmp6
	 * destination unreachable error with Code 2 (beyond scope of source
	 * address).
	 * [draft-ietf-ipngwg-icmp-v3-02.txt, Section 3.1]
	 */
	if (in6_addr2zoneid(rt->rt_ifp, &ip6->ip6_src, &dstzone)) {
		/* XXX: this should not happen */
		ip6stat.ip6s_cantforward++;
		ip6stat.ip6s_badscope++;
		m_freem(m);
		return;
	}
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	if (in6_recoverscope(&sin6, &ip6->ip6_src, m->m_pkthdr.rcvif) != 0) {
		/* XXX: this should not happen */
		ip6stat.ip6s_cantforward++;
		ip6stat.ip6s_badscope++;
		m_freem(m);
		return;
	}
	if (sin6.sin6_scope_id != dstzone
#ifdef IPSEC
	    && !ipsecrt
#endif
	    ) {
		ip6stat.ip6s_cantforward++;
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard);

		if (ip6_log_time + ip6_log_interval < time_second) {
			ip6_log_time = time_second;
			log(LOG_DEBUG,
			    "cannot forward "
			    "src %s, dst %s, nxt %d, rcvif %s, outif %s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst),
			    ip6->ip6_nxt,
			    if_name(m->m_pkthdr.rcvif), if_name(rt->rt_ifp));
		}
		if (mcopy)
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_BEYONDSCOPE, 0);
		m_freem(m);
		return;
	}

	/*
	 * Destination scope check: if a packet is going to break the scope
	 * zone of packet's destination address, discard it.  This case should
	 * usually be prevented by appropriately-configured routing table, but
	 * we need an explicit check because we may mistakenly forward the
	 * packet to a different zone by (e.g.) a default route.
	 */
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	if (in6_recoverscope(&sin6, &ip6->ip6_dst, m->m_pkthdr.rcvif) != 0) {
		/* XXX: this should not happen */
		ip6stat.ip6s_cantforward++;
		ip6stat.ip6s_badscope++;
		m_freem(m);
		return;
	}
	if (in6_addr2zoneid(rt->rt_ifp, &ip6->ip6_dst, &dstzone) ||
	    sin6.sin6_scope_id != dstzone) {
		ip6stat.ip6s_cantforward++;
		ip6stat.ip6s_badscope++;
		m_freem(m);
		return;
	}

	if (m->m_pkthdr.len > IN6_LINKMTU(rt->rt_ifp)) {
		in6_ifstat_inc(rt->rt_ifp, ifs6_in_toobig);
		if (mcopy) {
			u_long mtu;
#ifdef IPSEC
			struct secpolicy *sp;
			int ipsecerror;
			size_t ipsechdrsiz;
#endif

			mtu = IN6_LINKMTU(rt->rt_ifp);
#ifdef IPSEC
			/*
			 * When we do IPsec tunnel ingress, we need to play
			 * with the link value (decrement IPsec header size
			 * from mtu value).  The code is much simpler than v4
			 * case, as we have the outgoing interface for
			 * encapsulated packet as "rt->rt_ifp".
			 */
			sp = ipsec6_getpolicybyaddr(mcopy, IPSEC_DIR_OUTBOUND,
				IP_FORWARDING, &ipsecerror);
			if (sp) {
				ipsechdrsiz = ipsec6_hdrsiz(mcopy,
					IPSEC_DIR_OUTBOUND, NULL);
				if (ipsechdrsiz < mtu)
					mtu -= ipsechdrsiz;
			}

			/*
			 * if mtu becomes less than minimum MTU,
			 * tell minimum MTU (and I'll need to fragment it).
			 */
			if (mtu < IPV6_MMTU)
				mtu = IPV6_MMTU;
#endif
			icmp6_error(mcopy, ICMP6_PACKET_TOO_BIG, 0, mtu);
		}
		m_freem(m);
		return;
	}

	if (rt->rt_flags & RTF_GATEWAY)
		dst = (struct sockaddr_in6 *)rt->rt_gateway;

	/*
	 * If we are to forward the packet using the same interface
	 * as one we got the packet from, perhaps we should send a redirect
	 * to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a route
	 * modified by a redirect.
	 */
	if (rt->rt_ifp == m->m_pkthdr.rcvif && !srcrt && ip6_sendredirects &&
#ifdef IPSEC
	    !ipsecrt &&
#endif
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0) {
		if ((rt->rt_ifp->if_flags & IFF_POINTOPOINT)) {
			struct sockaddr_in6 sa6_dst;

#ifdef MIP6
			if (MIP6_IS_HA) {
			    /* Should it be take care on home agent case ? */
			    ;
			}
#endif /* defined(MIP6) */
			/*
			 * If the incoming interface is equal to the outgoing
			 * one, the link attached to the interface is
			 * point-to-point, and the IPv6 destination is
			 * regarded as on-link on the link, then it will be
			 * highly probable that the destination address does
			 * not exist on the link and that the packet is going
			 * to loop.  Thus, we immediately drop the packet and
			 * send an ICMPv6 error message.
			 * For other routing loops, we dare to let the packet
			 * go to the loop, so that a remote diagnosing host
			 * can detect the loop by traceroute.
			 * type/code is based on suggestion by Rich Draves.
			 * not sure if it is the best pick.
			 */
			bzero(&sa6_dst, sizeof(sa6_dst));
			sa6_dst.sin6_family = AF_INET6;
			sa6_dst.sin6_len = sizeof(sa6_dst);
			sa6_dst.sin6_addr = ip6->ip6_dst;
			if (nd6_is_addr_neighbor(&sa6_dst, rt->rt_ifp)) {
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_ADDR, 0);
				m_freem(m);
				return;
			}
		}
		type = ND_REDIRECT;
	}

#if defined(IPV6FIREWALL) || defined(__FreeBSD__)
	/*
	 * Check with the firewall...
	 */
#ifdef __FreeBSD__
	if (ip6_fw_enable && ip6_fw_chk_ptr)
#else
	if (ip6_fw_chk_ptr)
#endif
	{
		/* If ipfw says divert, we have to just drop packet */
		if ((*ip6_fw_chk_ptr)(&ip6, rt->rt_ifp, &m)) {
			m_freem(m);
			goto freecopy;
		}
		if (!m)
			goto freecopy;
	}
#endif

	/*
	 * Fake scoped addresses. Note that even link-local source or
	 * destinaion can appear, if the originating node just sends the
	 * packet to us (without address resolution for the destination).
	 * Since both icmp6_error and icmp6_redirect_output fill the embedded
	 * link identifiers, we can do this stuff after making a copy for
	 * returning an error.
	 */
	if ((rt->rt_ifp->if_flags & IFF_LOOPBACK) != 0) {
		/*
		 * See corresponding comments in ip6_output.
		 * XXX: but is it possible that ip6_forward() sends a packet
		 *      to a loopback interface? I don't think so, and thus
		 *      I bark here. (jinmei@kame.net)
		 * XXX: it is common to route invalid packets to loopback.
		 *	also, the codepath will be visited on use of ::1 in
		 *	rthdr. (itojun)
		 */
#if 1
		if (0)
#else
		if ((rt->rt_flags & (RTF_BLACKHOLE|RTF_REJECT)) == 0)
#endif
		{
			printf("ip6_forward: outgoing interface is loopback. "
			       "src %s, dst %s, nxt %d, rcvif %s, outif %s\n",
			       ip6_sprintf(&ip6->ip6_src),
			       ip6_sprintf(&ip6->ip6_dst),
			       ip6->ip6_nxt, if_name(m->m_pkthdr.rcvif),
			       if_name(rt->rt_ifp));
		}

		/* we can just use rcvif in forwarding. */
		origifp = m->m_pkthdr.rcvif;
	}
	else
		origifp = rt->rt_ifp;
#ifndef SCOPEDROUTING
	/*
	 * clear embedded scope identifiers if necessary.
	 * in6_clearscope will touch the addresses only when necessary.
	 */
	in6_clearscope(&ip6->ip6_src);
	in6_clearscope(&ip6->ip6_dst);
#endif

#if defined(__NetBSD__) && defined(PFIL_HOOKS)
	/*
	 * Run through list of hooks for output packets.
	 */
	if ((error = pfil_run_hooks(&inet6_pfil_hook, &m, rt->rt_ifp,
				    PFIL_OUT)) != 0)
		goto senderr;
	if (m == NULL)
		goto freecopy;
	ip6 = mtod(m, struct ip6_hdr *);
#elif (defined(__FreeBSD__) && __FreeBSD_version >= 503000)
	/* Jump over all PFIL processing if hooks are not active. */
	if (inet6_pfil_hook.ph_busy_count == -1)
		goto pass;

	/* Run through list of hooks for output packets. */
	error = pfil_run_hooks(&inet6_pfil_hook, &m, rt->rt_ifp, PFIL_OUT, NULL);
	if (error != 0)
		goto senderr;
	if (m == NULL)
		goto freecopy;
	ip6 = mtod(m, struct ip6_hdr *);
#endif /* PFIL_HOOKS */

#if defined(__FreeBSD__) && __FreeBSD__ > 4 && __FreeBSD_version < 500000
	/* 
	 * Check if we want to allow this packet to be processed.
	 * Consider it to be bad if not.
	 */
	if (fr_checkp) {
		struct mbuf	*m1 = m;

		if ((*fr_checkp)((struct ip *)ip6, sizeof(*ip6),
				 rt->rt_ifp, 1, &m1) != 0)
			goto freecopy;
		m = m1;
		if (m == NULL)
			goto freecopy;
		ip6 = mtod(m, struct ip6_hdr *);
	}
#endif

#if NPF > 0
	if (pf_test6(PF_OUT, rt->rt_ifp, &m) != PF_PASS) {
		m_freem(m);
		goto senderr;
	}
	if (m == NULL)
		goto senderr;

	ip6 = mtod(m, struct ip6_hdr *);
#endif

#if defined(__FreeBSD__) && __FreeBSD_version >= 503000
pass:
#endif
	error = nd6_output(rt->rt_ifp, origifp, m, dst, rt);
	if (error) {
		in6_ifstat_inc(rt->rt_ifp, ifs6_out_discard);
		ip6stat.ip6s_cantforward++;
	} else {
		ip6stat.ip6s_forward++;
		in6_ifstat_inc(rt->rt_ifp, ifs6_out_forward);
		if (type)
			ip6stat.ip6s_redirectsent++;
		else {
			if (mcopy)
				goto freecopy;
		}
	}

#if (defined(__NetBSD__) && defined(PFIL_HOOKS)) || (defined(__FreeBSD__) && __FreeBSD_version >= 503000) || NPF > 0
 senderr:
#endif
	if (mcopy == NULL)
		return;
	switch (error) {
	case 0:
		if (type == ND_REDIRECT) {
			icmp6_redirect_output(mcopy, rt);
			return;
		}
		goto freecopy;

	case EMSGSIZE:
		/* xxx MTU is constant in PPP? */
		goto freecopy;

	case ENOBUFS:
		/* Tell source to slow down like source quench in IP? */
		goto freecopy;

	case ENETUNREACH:	/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP6_DST_UNREACH;
		code = ICMP6_DST_UNREACH_ADDR;
		break;
	}
	icmp6_error(mcopy, type, code, 0);
	return;

 freecopy:
	m_freem(mcopy);
	return;
}
