/*	$KAME: in6_src.c,v 1.148 2004/08/12 03:54:30 keiichi Exp $	*/

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

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#ifdef __FreeBSD__
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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#ifndef __FreeBSD__
#include <sys/ioctl.h>
#else
#include <sys/sockio.h>
#endif
#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#ifdef RADIX_MPATH
#include <net/radix_mpath.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#ifndef __OpenBSD__
#include <netinet6/in6_pcb.h>
#endif
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/scope6_var.h>

#include <net/net_osdep.h>

#ifdef MIP6
#include <netinet6/mip6.h>
#include <netinet6/mip6_var.h>
#include "mip.h"
#if NMIP > 0
#include <net/if_mip.h>
#endif /* NMIP > 0 */
#endif /* MIP6 */

#ifndef __OpenBSD__
#include "loop.h"
#endif
#ifdef __NetBSD__
extern struct ifnet loif[NLOOP];
#endif

#if defined(__FreeBSD__) && __FreeBSD_version >= 502010
static struct mtx addrsel_lock;
#define ADDRSEL_LOCK_INIT()     mtx_init(&addrsel_lock, "addrsel_lock", NULL, MTX_DEF)
#define ADDRSEL_LOCK()          mtx_lock(&addrsel_lock)
#define ADDRSEL_UNLOCK()        mtx_unlock(&addrsel_lock)
#define ADDRSEL_LOCK_ASSERT()   mtx_assert(&addrsel_lock, MA_OWNED)
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */

#define ADDR_LABEL_NOTAPP (-1)
struct in6_addrpolicy defaultaddrpolicy;

int ip6_prefer_tempaddr = 0;

#ifdef NEW_STRUCT_ROUTE
static int in6_selectif __P((struct sockaddr_in6 *, struct ip6_pktopts *,
	struct ip6_moptions *, struct route *ro, struct ifnet **));
#else
static int in6_selectif __P((struct sockaddr_in6 *, struct ip6_pktopts *,
	struct ip6_moptions *, struct route_in6 *ro, struct ifnet **));
#endif

static struct in6_addrpolicy *lookup_addrsel_policy __P((struct sockaddr_in6 *));

static void init_policy_queue __P((void));
static int add_addrsel_policyent __P((struct in6_addrpolicy *));
static int delete_addrsel_policyent __P((struct in6_addrpolicy *));
static int walk_addrsel_policy __P((int (*)(struct in6_addrpolicy *, void *),
				    void *));
static int dump_addrsel_policyent __P((struct in6_addrpolicy *, void *));
static struct in6_addrpolicy *match_addrsel_policy __P((struct sockaddr_in6 *));

/*
 * Return an IPv6 address, which is the most appropriate for a given
 * destination and user specified options.
 * If necessary, this function lookups the routing table and returns
 * an entry to the caller for later use.
 */
#define REPLACE(r) do {\
	if ((r) < sizeof(ip6stat.ip6s_sources_rule) / \
		sizeof(ip6stat.ip6s_sources_rule[0])) /* check for safety */ \
		ip6stat.ip6s_sources_rule[(r)]++; \
	/* printf("in6_selectsrc: replace %s with %s by %d\n", ia_best ? ip6_sprintf(&ia_best->ia_addr.sin6_addr) : "none", ip6_sprintf(&ia->ia_addr.sin6_addr), (r)); */ \
	goto replace; \
} while(0)
#define NEXT(r) do {\
	if ((r) < sizeof(ip6stat.ip6s_sources_rule) / \
		sizeof(ip6stat.ip6s_sources_rule[0])) /* check for safety */ \
		ip6stat.ip6s_sources_rule[(r)]++; \
	/* printf("in6_selectsrc: keep %s against %s by %d\n", ia_best ? ip6_sprintf(&ia_best->ia_addr.sin6_addr) : "none", ip6_sprintf(&ia->ia_addr.sin6_addr), (r)); */ \
	goto next; 		/* XXX: we can't use 'continue' here */ \
} while(0)
#define BREAK(r) do { \
	if ((r) < sizeof(ip6stat.ip6s_sources_rule) / \
		sizeof(ip6stat.ip6s_sources_rule[0])) /* check for safety */ \
		ip6stat.ip6s_sources_rule[(r)]++; \
	goto out; 		/* XXX: we can't use 'break' here */ \
} while(0)

struct in6_addr *
in6_selectsrc(dstsock, opts, mopts, ro, laddr, ifpp, errorp)
	struct sockaddr_in6 *dstsock;
	struct ip6_pktopts *opts;
	struct ip6_moptions *mopts;
#ifdef NEW_STRUCT_ROUTE
	struct route *ro;
#else
	struct route_in6 *ro;
#endif
	struct in6_addr *laddr;
	struct ifnet **ifpp;
	int *errorp;
{
	struct in6_addr *dst;
	struct ifnet *ifp = NULL;
	struct in6_ifaddr *ia = NULL, *ia_best = NULL;
	struct in6_pktinfo *pi = NULL;
	int dst_scope = -1, best_scope = -1, best_matchlen = -1;
	struct in6_addrpolicy *dst_policy = NULL, *best_policy = NULL;
	u_int32_t odstzone;
	int prefer_tempaddr;
#if defined(MIP6) && NMIP > 0
	u_int8_t ip6po_usecoa = 0;
#endif /* MIP6 && NMIP > 0 */

	dst = &dstsock->sin6_addr;
	*errorp = 0;
	if (ifpp)
		*ifpp = NULL;

	/*
	 * If the source address is explicitly specified by the caller,
	 * check if the requested source address is indeed a unicast address
	 * assigned to the node, and can be used as the packet's source
	 * address.  If everything is okay, use the address as source.
	 */
	if (opts && (pi = opts->ip6po_pktinfo) &&
	    !IN6_IS_ADDR_UNSPECIFIED(&pi->ipi6_addr)) {
		struct sockaddr_in6 srcsock;
		struct in6_ifaddr *ia6;

		/* get the outgoing interface */
		if ((*errorp = in6_selectif(dstsock, opts, mopts, ro, &ifp))
		    != 0) {
			return (NULL);
		}

		/*
		 * determine the appropriate zone id of the source based on
		 * the zone of the destination and the outgoing interface.
		 */
		bzero(&srcsock, sizeof(srcsock));
		srcsock.sin6_family = AF_INET6;
		srcsock.sin6_len = sizeof(srcsock);
		srcsock.sin6_addr = pi->ipi6_addr;
		if (ifp) {
			if (in6_addr2zoneid(ifp, &pi->ipi6_addr,
					    &srcsock.sin6_scope_id)) {
				*errorp = EINVAL; /* XXX */
				return (NULL);
			}
		}
		if ((*errorp = in6_embedscope(&srcsock.sin6_addr, &srcsock))
		    != 0) {
			return (NULL);
		}
#ifndef SCOPEDROUTING
		srcsock.sin6_scope_id = 0; /* XXX: ifa_ifwithaddr expects 0 */
#endif
		ia6 = (struct in6_ifaddr *)ifa_ifwithaddr((struct sockaddr *)(&srcsock));
		if (ia6 == NULL ||
		    (ia6->ia6_flags & (IN6_IFF_ANYCAST | IN6_IFF_NOTREADY))) {
			*errorp = EADDRNOTAVAIL;
			return (NULL);
		}
		pi->ipi6_addr = srcsock.sin6_addr; /* XXX: this overrides pi */
		if (ifpp)
			*ifpp = ifp;
		return (&ia6->ia_addr.sin6_addr);
	}

	/*
	 * Otherwise, if the socket has already bound the source, just use it.
	 */
	if (laddr && !IN6_IS_ADDR_UNSPECIFIED(laddr))
		return (laddr);

	/*
	 * If the address is not specified, choose the best one based on
	 * the outgoing interface and the destination address.
	 */
	/* get the outgoing interface */
	if ((*errorp = in6_selectif(dstsock, opts, mopts, ro, &ifp)) != 0)
		return (NULL);

#if defined(MIP6) && NMIP > 0
	/*
	 * a caller can specify IP6PO_USECOA to not to use a home
	 * address.  for example, the case that the neighbour
	 * unreachability detection to the global address.
	 */
	if (opts != NULL &&
	    (opts->ip6po_flags & IP6PO_USECOA) != 0) {
		ip6po_usecoa = 1;
	}
#endif /* MIP6 && NMIP > 0 */

#ifdef DIAGNOSTIC
	if (ifp == NULL)	/* this should not happen */
		panic("in6_selectsrc: NULL ifp");
#endif
	if (in6_addr2zoneid(ifp, dst, &odstzone)) { /* impossible */
		*errorp = EIO;	/* XXX */
		return (NULL);
	}
	for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
		int new_scope = -1, new_matchlen = -1;
		struct in6_addrpolicy *new_policy = NULL;
		u_int32_t srczone, osrczone, dstzone;
		struct ifnet *ifp1 = ia->ia_ifp;

		/*
		 * We'll never take an address that breaks the scope zone
		 * of the destination.  We also skip an address if its zone
		 * does not contain the outgoing interface.
		 * XXX: we should probably use sin6_scope_id here.
		 */
		if (in6_addr2zoneid(ifp1, dst, &dstzone) ||
		    odstzone != dstzone) {
			continue;
		}
		if (in6_addr2zoneid(ifp, &ia->ia_addr.sin6_addr, &osrczone) ||
		    in6_addr2zoneid(ifp1, &ia->ia_addr.sin6_addr, &srczone) ||
		    osrczone != srczone) {
			continue;
		}

		/* avoid unusable addresses */
		if ((ia->ia6_flags &
		     (IN6_IFF_NOTREADY | IN6_IFF_ANYCAST | IN6_IFF_DETACHED))) {
				continue;
		}
		if (!ip6_use_deprecated && IFA6_IS_DEPRECATED(ia))
			continue;

#if defined(MIP6) && NMIP > 0
		/* avoid unusable home addresses. */
		if ((ia->ia6_flags & IN6_IFF_HOME) &&
		    !mip6_ifa6_is_addr_valid_hoa(ia))
			continue;
#endif /* MIP6 && NMIP > 0 */

		/* Rule 1: Prefer same address */
		if (IN6_ARE_ADDR_EQUAL(dst, &ia->ia_addr.sin6_addr)) {
			ia_best = ia;
			BREAK(1); /* there should be no better candidate */
		}

		if (ia_best == NULL)
			REPLACE(0);

		/* Rule 2: Prefer appropriate scope */
		if (dst_scope < 0)
			dst_scope = in6_addrscope(dst);
		new_scope = in6_addrscope(&ia->ia_addr.sin6_addr);
		if (IN6_ARE_SCOPE_CMP(best_scope, new_scope) < 0) {
			if (IN6_ARE_SCOPE_CMP(best_scope, dst_scope) < 0)
				REPLACE(2);
			NEXT(2);
		} else if (IN6_ARE_SCOPE_CMP(new_scope, best_scope) < 0) {
			if (IN6_ARE_SCOPE_CMP(new_scope, dst_scope) < 0)
				NEXT(2);
			REPLACE(2);
		}

		/*
		 * Rule 3: Avoid deprecated addresses.  Note that the case of
		 * !ip6_use_deprecated is already rejected above.
		 */
		if (!IFA6_IS_DEPRECATED(ia_best) && IFA6_IS_DEPRECATED(ia))
			NEXT(3);
		if (IFA6_IS_DEPRECATED(ia_best) && !IFA6_IS_DEPRECATED(ia))
			REPLACE(3);

		/* Rule 4: Prefer home addresses */
#if defined(MIP6) && NMIP > 0
		if (!MIP6_IS_MN)
			goto skip_rule4;

		if ((ia_best->ia6_flags & IN6_IFF_HOME) == 0 &&
		    (ia->ia6_flags & IN6_IFF_HOME) == 0) {
			/* both address are not home addresses. */
			goto skip_rule4;
		}

		/*
		 * If SA is simultaneously a home address and care-of
		 * address and SB is not, then prefer SA. Similarly,
		 * if SB is simultaneously a home address and care-of
		 * address and SA is not, then prefer SB.
		 */
		if (((ia_best->ia6_flags & IN6_IFF_HOME) != 0 &&
			ia_best->ia_ifp->if_type != IFT_MIP)
		    &&
		    ((ia->ia6_flags & IN6_IFF_HOME) != 0 &&
			ia->ia_ifp->if_type == IFT_MIP))
			NEXT(4);
		if (((ia_best->ia6_flags & IN6_IFF_HOME) != 0 &&
			ia_best->ia_ifp->if_type == IFT_MIP)
		    &&
		    ((ia->ia6_flags & IN6_IFF_HOME) != 0 &&
			ia->ia_ifp->if_type != IFT_MIP))
			REPLACE(4);
		if (ip6po_usecoa == 0) {
			/*
			 * If SA is just a home address and SB is just
			 * a care-of address, then prefer
			 * SA. Similarly, if SB is just a home address
			 * and SA is just a care-of address, then
			 * prefer SB.
			 */
			if ((ia_best->ia6_flags & IN6_IFF_HOME) != 0 &&
			    (ia->ia6_flags & IN6_IFF_HOME) == 0) {
				NEXT(4);
			}
			if ((ia_best->ia6_flags & IN6_IFF_HOME) == 0 &&
			    (ia->ia6_flags & IN6_IFF_HOME) != 0) {
				REPLACE(4);
			}
		} else {
			/*
			 * a sender don't want to use a home address
			 * because:
			 *
			 * 1) we cannot use.  (ex. NS or NA to global
			 * addresses.)
			 *
			 * 2) a user specified not to use.
			 * (ex. mip6control -u)
			 */
			if ((ia_best->ia6_flags & IN6_IFF_HOME) == 0 &&
			    (ia->ia6_flags & IN6_IFF_HOME) != 0) {
				/* XXX breaks stat */
				NEXT(0);
			}
			if ((ia_best->ia6_flags & IN6_IFF_HOME) != 0 &&
			    (ia->ia6_flags & IN6_IFF_HOME) == 0) {
				/* XXX breaks stat */
				REPLACE(0);
			}
		}			
	skip_rule4:
#endif /* MIP6 && NMIP > 0 */

		/* Rule 5: Prefer outgoing interface */
		if (ia_best->ia_ifp == ifp && ia->ia_ifp != ifp)
			NEXT(5);
		if (ia_best->ia_ifp != ifp && ia->ia_ifp == ifp)
			REPLACE(5);

		/*
		 * Rule 6: Prefer matching label
		 * Note that best_policy should be non-NULL here.
		 */
		if (dst_policy == NULL)
			dst_policy = lookup_addrsel_policy(dstsock);
		if (dst_policy->label != ADDR_LABEL_NOTAPP) {
			new_policy = lookup_addrsel_policy(&ia->ia_addr);
			if (dst_policy->label == best_policy->label &&
			    dst_policy->label != new_policy->label)
				NEXT(6);
			if (dst_policy->label != best_policy->label &&
			    dst_policy->label == new_policy->label)
				REPLACE(6);
		}

		/*
		 * Rule 7: Prefer public addresses.
		 * We allow users to reverse the logic by configuring
		 * a sysctl variable, so that privacy conscious users can
		 * always prefer temporary addresses.
		 */
		if (opts == NULL ||
		    opts->ip6po_prefer_tempaddr == IP6PO_TEMPADDR_SYSTEM) {
			prefer_tempaddr = ip6_prefer_tempaddr;
		} else if (opts->ip6po_prefer_tempaddr ==
		    IP6PO_TEMPADDR_NOTPREFER) {
			prefer_tempaddr = 0;
		} else
			prefer_tempaddr = 1;
		if (!(ia_best->ia6_flags & IN6_IFF_TEMPORARY) &&
		    (ia->ia6_flags & IN6_IFF_TEMPORARY)) {
			if (prefer_tempaddr)
				REPLACE(7);
			else
				NEXT(7);
		}
		if ((ia_best->ia6_flags & IN6_IFF_TEMPORARY) &&
		    !(ia->ia6_flags & IN6_IFF_TEMPORARY)) {
			if (prefer_tempaddr)
				NEXT(7);
			else
				REPLACE(7);
		}

		/*
		 * Rule 8: prefer addresses on alive interfaces.
		 * This is a KAME specific rule.
		 */
		if ((ia_best->ia_ifp->if_flags & IFF_UP) &&
		    !(ia->ia_ifp->if_flags & IFF_UP))
			NEXT(8);
		if (!(ia_best->ia_ifp->if_flags & IFF_UP) &&
		    (ia->ia_ifp->if_flags & IFF_UP))
			REPLACE(8);

		/*
		 * Rule 9: prefer addresses on "preferred" interfaces.
		 * This is a KAME specific rule.
		 */
#define NDI_BEST ND_IFINFO(ia_best->ia_ifp)
#define NDI_NEW  ND_IFINFO(ia->ia_ifp)
		if ((NDI_BEST->flags & ND6_IFF_PREFER_SOURCE) &&
		    !(NDI_NEW->flags & ND6_IFF_PREFER_SOURCE))
			NEXT(9);
		if (!(NDI_BEST->flags & ND6_IFF_PREFER_SOURCE) &&
		    (NDI_NEW->flags & ND6_IFF_PREFER_SOURCE))
			REPLACE(9);
#undef NDI_BEST
#undef NDI_NEW

		/*
		 * Rule 14: Use longest matching prefix.
		 * Note: in the address selection draft, this rule is
		 * documented as "Rule 8".  However, since it is also
		 * documented that this rule can be overridden, we assign
		 * a large number so that it is easy to assign smaller numbers
		 * to more preferred rules.
		 */
		new_matchlen = in6_matchlen(&ia->ia_addr.sin6_addr, dst);
		if (best_matchlen < new_matchlen)
			REPLACE(14);
		if (new_matchlen < best_matchlen)
			NEXT(14);

		/* Rule 15 is reserved. */

		/*
		 * Last resort: just keep the current candidate.
		 * Or, do we need more rules?
		 */
		continue;

	  replace:
		ia_best = ia;
		best_scope = (new_scope >= 0 ? new_scope :
			      in6_addrscope(&ia_best->ia_addr.sin6_addr));
		best_policy = (new_policy ? new_policy :
			       lookup_addrsel_policy(&ia_best->ia_addr));
		best_matchlen = (new_matchlen >= 0 ? new_matchlen :
				 in6_matchlen(&ia_best->ia_addr.sin6_addr,
					      dst));

	  next:
		continue;

	  out:
		break;
	}

	if ((ia = ia_best) == NULL) {
		*errorp = EADDRNOTAVAIL;
		return (NULL);
	}

	if (ifpp)
		*ifpp = ifp;
	return (&ia->ia_addr.sin6_addr);
}
#undef REPLACE
#undef BREAK
#undef NEXT

static int
in6_selectif(dstsock, opts, mopts, ro, retifp)
	struct sockaddr_in6 *dstsock;
	struct ip6_pktopts *opts;
	struct ip6_moptions *mopts;
#ifdef NEW_STRUCT_ROUTE
	struct route *ro;
#else
	struct route_in6 *ro;
#endif
	struct ifnet **retifp;
{
	int error, clone;
	struct rtentry *rt = NULL;

	clone = IN6_IS_ADDR_MULTICAST(&dstsock->sin6_addr) ? 0 : 1;
	if ((error = in6_selectroute(dstsock, opts, mopts, ro, retifp,
	    &rt, clone)) != 0) {
		return (error);
	}

	/*
	 * do not use a rejected or black hole route.
	 * XXX: this check should be done in the L2 output routine.
	 * However, if we skipped this check here, we'd see the following
	 * scenario:
	 * - install a rejected route for a scoped address prefix
	 *   (like fe80::/10)
	 * - send a packet to a destination that matches the scoped prefix,
	 *   with ambiguity about the scope zone.
	 * - pick the outgoing interface from the route, and disambiguate the
	 *   scope zone with the interface.
	 * - ip6_output() would try to get another route with the "new"
	 *   destination, which may be valid.
	 * - we'd see no error on output.
	 * Although this may not be very harmful, it should still be confusing.
	 * We thus reject the case here.
	 */
	if (rt && (rt->rt_flags & (RTF_REJECT | RTF_BLACKHOLE)))
		return (rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);

	/*
	 * Adjust the "outgoing" interface.  If we're going to loop the packet
	 * back to ourselves, the ifp would be the loopback interface.
	 * However, we'd rather know the interface associated to the
	 * destination address (which should probably be one of our own
	 * addresses.)
	 */
	if (rt && rt->rt_ifa && rt->rt_ifa->ifa_ifp)
		*retifp = rt->rt_ifa->ifa_ifp;

	return (0);
}

int
in6_selectroute(dstsock, opts, mopts, ro, retifp, retrt, clone)
	struct sockaddr_in6 *dstsock;
	struct ip6_pktopts *opts;
	struct ip6_moptions *mopts;
#ifdef NEW_STRUCT_ROUTE
	struct route *ro;
#else
	struct route_in6 *ro;
#endif
	struct ifnet **retifp;
	struct rtentry **retrt;
	int clone;		/* meaningful only for bsdi and freebsd. */
{
	int error = 0;
	struct ifnet *ifp = NULL;
	struct rtentry *rt = NULL;
	struct sockaddr_in6 *sin6_next;
	struct in6_pktinfo *pi = NULL;
	struct in6_addr *dst;

	dst = &dstsock->sin6_addr;

#if 0
	if (dstsock->sin6_addr.s6_addr32[0] == 0 &&
	    dstsock->sin6_addr.s6_addr32[1] == 0 &&
	    !IN6_IS_ADDR_LOOPBACK(&dstsock->sin6_addr)) {
		printf("in6_selectroute: strange destination %s\n",
		       ip6_sprintf(&dstsock->sin6_addr));
	} else {
		printf("in6_selectroute: destination = %s%%%d\n",
		       ip6_sprintf(&dstsock->sin6_addr),
		       dstsock->sin6_scope_id); /* for debug */
	}
#endif

	/* If the caller specify the outgoing interface explicitly, use it. */
	if (opts && (pi = opts->ip6po_pktinfo) != NULL && pi->ipi6_ifindex) {
		/* XXX boundary check is assumed to be already done. */
#if defined(__FreeBSD__) && __FreeBSD__ >= 5
		ifp = ifnet_byindex(pi->ipi6_ifindex);
#else
		ifp = ifindex2ifnet[pi->ipi6_ifindex];
#endif
		if (ifp != NULL &&
		    (retrt == NULL || IN6_IS_ADDR_MULTICAST(dst))) {
			/*
			 * we do not have to check or get the route for
			 * multicast.
			 */
			goto done;
		} else
			goto getroute;
	}

	/*
	 * If the destination address is a multicast address and the outgoing
	 * interface for the address is specified by the caller, use it.
	 */
	if (IN6_IS_ADDR_MULTICAST(dst) &&
	    mopts != NULL && (ifp = mopts->im6o_multicast_ifp) != NULL) {
		goto done; /* we do not need a route for multicast. */
	}

  getroute:
	/*
	 * If the next hop address for the packet is specified by the caller,
	 * use it as the gateway.
	 */
	if (opts && opts->ip6po_nexthop) {
#ifdef NEW_STRUCT_ROUTE
		struct route *ron;
#else
		struct route_in6 *ron;
#endif

		sin6_next = satosin6(opts->ip6po_nexthop);

		/* at this moment, we only support AF_INET6 next hops */
		if (sin6_next->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT; /* or should we proceed? */
			goto done;
		}

		/*
		 * If the next hop is an IPv6 address, then the node identified
		 * by that address must be a neighbor of the sending host.
		 */
		ron = &opts->ip6po_nextroute;
		if ((ron->ro_rt &&
		    (ron->ro_rt->rt_flags & (RTF_UP | RTF_GATEWAY)) !=
		    RTF_UP) ||
		    !IN6_ARE_ADDR_EQUAL(&satosin6(&ron->ro_dst)->sin6_addr,
		    &sin6_next->sin6_addr)) {
			if (ron->ro_rt) {
				RTFREE(ron->ro_rt);
				ron->ro_rt = NULL;
			}
			*satosin6(&ron->ro_dst) = *sin6_next;
		}
		if (ron->ro_rt == NULL) {
			rtalloc((struct route *)ron); /* multi path case? */
			if (ron->ro_rt == NULL ||
			    (ron->ro_rt->rt_flags & RTF_GATEWAY)) {
				if (ron->ro_rt) {
					RTFREE(ron->ro_rt);
					ron->ro_rt = NULL;
				}
				error = EHOSTUNREACH;
				goto done;
			}
		}
		if (!nd6_is_addr_neighbor(sin6_next, ron->ro_rt->rt_ifp)) {
			RTFREE(ron->ro_rt);
			ron->ro_rt = NULL;
			error = EHOSTUNREACH;
			goto done;
		}
		rt = ron->ro_rt;
		ifp = rt->rt_ifp;

		/*
		 * When cloning is required, try to allocate a route to the
		 * destination so that the caller can store path MTU
		 * information.
		 */
		if (!clone)
			goto done;
	}

	/*
	 * Use a cached route if it exists and is valid, else try to allocate
	 * a new one.  Note that we should check the address family of the
	 * cached destination, in case of sharing the cache with IPv4.
	 */
	if (ro) {
		if (ro->ro_rt &&
		    (!(ro->ro_rt->rt_flags & RTF_UP) ||
		     ((struct sockaddr *)(&ro->ro_dst))->sa_family != AF_INET6 ||
		     !IN6_ARE_ADDR_EQUAL(&satosin6(&ro->ro_dst)->sin6_addr,
		     dst))) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)NULL;
		}
		if (ro->ro_rt == (struct rtentry *)NULL) {
			struct sockaddr_in6 *sa6;

			/* No route yet, so try to acquire one */
			bzero(&ro->ro_dst, sizeof(struct sockaddr_in6));
			sa6 = (struct sockaddr_in6 *)&ro->ro_dst;
			*sa6 = *dstsock;
#ifndef SCOPEDROUTING		/* XXX */
			sa6->sin6_scope_id = 0;
#endif
			if (clone) {
#ifdef RADIX_MPATH
				rtalloc_mpath((struct route *)ro,
				    ntohl(sa6->sin6_addr.s6_addr32[3]));
#else
				rtalloc((struct route *)ro);
#endif /* RADIX_MPATH */
			} else {
#ifdef __FreeBSD__
				ro->ro_rt = rtalloc1(&((struct route *)ro)
						     ->ro_dst, 0, 0UL);
#if __FreeBSD_version >= 502010
				if (ro->ro_rt)
					RT_UNLOCK(ro->ro_rt);
#endif
#else
#ifdef RADIX_MPATH
				rtalloc_mpath((struct route *)ro,
				    ntohl(sa6->sin6_addr.s6_addr32[3]));
#else
				ro->ro_rt = rtalloc1(&((struct route *)ro)
						     ->ro_dst, 0);
#endif /* RADIX_MPATH */
#endif /* __FreeBSD__ */
			}
		}

		/*
		 * do not care about the result if we have the nexthop
		 * explicitly specified.
		 */
		if (opts && opts->ip6po_nexthop)
			goto done;

		if (ro->ro_rt) {
			ifp = ro->ro_rt->rt_ifp;

			if (ifp == NULL) { /* can this really happen? */
				RTFREE(ro->ro_rt);
				ro->ro_rt = NULL;
			}
		}
		if (ro->ro_rt == NULL)
			error = EHOSTUNREACH;
		rt = ro->ro_rt;

		/*
		 * Check if the outgoing interface conflicts with
		 * the interface specified by ipi6_ifindex (if specified).
		 * Note that loopback interface is always okay.
		 * (this may happen when we are sending a packet to one of
		 *  our own addresses.)
		 */
		if (opts && opts->ip6po_pktinfo &&
		    opts->ip6po_pktinfo->ipi6_ifindex) {
			if (!(ifp->if_flags & IFF_LOOPBACK) &&
			    ifp->if_index !=
			    opts->ip6po_pktinfo->ipi6_ifindex) {
				error = EHOSTUNREACH;
				goto done;
			}
		}
	}

  done:
	if (ifp == NULL && rt == NULL) {
		/*
		 * This can happen if the caller did not pass a cached route
		 * nor any other hints.  We treat this case an error.
		 */
		error = EHOSTUNREACH;
	}
	if (error == EHOSTUNREACH)
		ip6stat.ip6s_noroute++;

	if (retifp != NULL)
		*retifp = ifp;
	if (retrt != NULL)
		*retrt = rt;	/* rt may be NULL */

	return (error);
}

/*
 * Default hop limit selection. The precedence is as follows:
 * 1. Hoplimit value specified via ioctl.
 * 2. (If the outgoing interface is detected) the current
 *     hop limit of the interface specified by router advertisement.
 * 3. The system default hoplimit.
*/
#ifdef HAVE_NRL_INPCB
#define in6pcb		inpcb
#define in6p_hops	inp_hops
#endif
int
in6_selecthlim(in6p, ifp)
	struct in6pcb *in6p;
	struct ifnet *ifp;
{
	if (in6p && in6p->in6p_hops >= 0)
		return (in6p->in6p_hops);
	else if (ifp)
		return (ND_IFINFO(ifp)->chlim);
	else
		return (ip6_defhlim);
}
#ifdef HAVE_NRL_INPCB
#undef in6pcb
#undef in6p_hops
#endif

#ifdef __NetBSD__
/*
 * Find an empty port and set it to the specified PCB.
 */
int
in6_pcbsetport(laddr, in6p, p)
	struct in6_addr *laddr;
	struct in6pcb *in6p;
	struct proc *p;
{
	struct socket *so = in6p->in6p_socket;
	struct in6pcb *head = in6p->in6p_head;
	u_int16_t last_port, lport = 0;
	int wild = 0;
	void *t;
	u_int16_t min, max;

	/* XXX: this is redundant when called from in6_pcbbind */
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
	   ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
	    (so->so_options & SO_ACCEPTCONN) == 0))
		wild = IN6PLOOKUP_WILDCARD;

	if (in6p->in6p_flags & IN6P_LOWPORT) {
#ifndef IPNOPRIVPORTS
		if (p == 0 || (suser(p->p_ucred, &p->p_acflag) != 0))
			return (EACCES);
#endif
		min = ip6_lowportmin;
		max = ip6_lowportmax;
	} else {
		min = ip6_anonportmin;
		max = ip6_anonportmax;
	}

	/* value out of range */
	if (head->in6p_lport < min)
		head->in6p_lport = min;
	else if (head->in6p_lport > max)
		head->in6p_lport = min;
	last_port = head->in6p_lport;
	goto startover;	/*to randomize*/
	for (;;) {
		lport = htons(head->in6p_lport);
		if (IN6_IS_ADDR_V4MAPPED(laddr)) {
#if 0
			t = in_pcblookup_bind(&tcbtable,
			    (struct in_addr *)&laddr->s6_addr32[3], lport);
#else
			t = NULL;
#endif
		} else {
			t = in6_pcblookup(head, &zeroin6_addr, 0, laddr, lport,
			    wild);
		}
		if (t == 0)
			break;
	  startover:
		if (head->in6p_lport >= max)
			head->in6p_lport = min;
		else
			head->in6p_lport++;
		if (head->in6p_lport == last_port)
			return (EADDRINUSE);
	}

	in6p->in6p_lport = lport;
	return (0);		/* success */
}
#endif

#ifdef __FreeBSD__
/*
 * XXX: this is borrowed from in6_pcbbind(). If possible, we should
 * share this function by all *bsd*...
 */
int
in6_pcbsetport(laddr, inp, p)
	struct in6_addr *laddr;
	struct inpcb *inp;
#if __FreeBSD_version >= 500000
	struct thread *p;
#else
	struct proc *p;
#endif
{
	struct socket *so = inp->inp_socket;
	u_int16_t lport = 0, first, last, *lastport;
	int count, error = 0, wild = 0;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;

	/* XXX: this is redundant when called from in6_pcbbind */
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0)
		wild = INPLOOKUP_WILDCARD;

	inp->inp_flags |= INP_ANONPORT;

	if (inp->inp_flags & INP_HIGHPORT) {
		first = ipport_hifirstauto;	/* sysctl */
		last  = ipport_hilastauto;
		lastport = &pcbinfo->lasthi;
	} else if (inp->inp_flags & INP_LOWPORT) {
#ifdef __FreeBSD__
		if (p && (error = suser(p)))
#else
		if (p && (error = suser(p->p_ucred, &p->p_acflag)))
#endif
			return error;
		first = ipport_lowfirstauto;	/* 1023 */
		last  = ipport_lowlastauto;	/* 600 */
		lastport = &pcbinfo->lastlow;
	} else {
		first = ipport_firstauto;	/* sysctl */
		last  = ipport_lastauto;
		lastport = &pcbinfo->lastport;
	}
	/*
	 * Simple check to ensure all ports are not used up causing
	 * a deadlock here.
	 *
	 * We split the two cases (up and down) so that the direction
	 * is not being tested on each round of the loop.
	 */
	if (first > last) {
		/*
		 * counting down
		 */
		count = first - last;

		do {
			if (count-- < 0) {	/* completely used? */
				/*
				 * Undo any address bind that may have
				 * occurred above.
				 */
				inp->in6p_laddr = in6addr_any;
				return (EAGAIN);
			}
			--*lastport;
			if (*lastport > first || *lastport < last)
				*lastport = first;
			lport = htons(*lastport);
		} while (in6_pcblookup_local(pcbinfo, &inp->in6p_laddr,
					     lport, wild));
	} else {
		/*
			 * counting up
			 */
		count = last - first;

		do {
			if (count-- < 0) {	/* completely used? */
				/*
				 * Undo any address bind that may have
				 * occurred above.
				 */
				inp->in6p_laddr = in6addr_any;
				return (EAGAIN);
			}
			++*lastport;
			if (*lastport < first || *lastport > last)
				*lastport = first;
			lport = htons(*lastport);
		} while (in6_pcblookup_local(pcbinfo,
					     &inp->in6p_laddr, lport, wild));
	}

	inp->inp_lport = lport;
	if (in_pcbinshash(inp) != 0) {
		inp->in6p_laddr = in6addr_any;
		inp->inp_lport = 0;
		return (EAGAIN);
	}

	return (0);
}
#endif

/*
 * Generate kernel-internal form (scopeid embedded into s6_addr16[1]).
 * If the address scope of is interface-local or link-local, embed the
 * interface index in the address.
 *
 * This function should be nuked in the future, when we get rid of embedded
 * scopeid thing.
 */
int
in6_embedscope(in6, sin6)
	struct in6_addr *in6;
	const struct sockaddr_in6 *sin6;
{
#ifdef SCOPEDROUTING
	/*
	 * XXX: the SCOPEDROUTING code path is NOT expected to work at this
	 * moment (20011112).  We added this just in case.
	 */
	return (0);		/* do nothing */
#else
	struct ifnet *ifp;
	u_int32_t zoneid = sin6->sin6_scope_id;

	*in6 = sin6->sin6_addr;

	/*
	 * don't try to read sin6->sin6_addr beyond here, since the caller may
	 * ask us to overwrite existing sockaddr_in6
	 */

	if (IN6_IS_SCOPE_LINKLOCAL(in6) || IN6_IS_ADDR_MC_INTFACELOCAL(in6)) {
		/* KAME assumption: link id == interface id */
		if (zoneid) {
			if (if_indexlim <= zoneid)
				return (ENXIO);  /* XXX EINVAL? */
#if defined(__FreeBSD__) && __FreeBSD__ >= 5
			ifp = ifnet_byindex(zoneid);
#else
			ifp = ifindex2ifnet[zoneid];
#endif
			if (ifp == NULL) /* XXX: this can happen for some OS */
				return (ENXIO);

			/* XXX assignment to 16bit from 32bit variable */
			in6->s6_addr16[1] = htons(zoneid & 0xffff);
		}
	}

	return 0;
#endif
}

/*
 * generate standard sockaddr_in6 from embedded form.
 * touches sin6_addr and sin6_scope_id only.
 *
 * this function should be nuked in the future, when we get rid of
 * embedded scopeid thing.
 */
int
in6_recoverscope(sin6, in6, ifp)
	struct sockaddr_in6 *sin6;
	const struct in6_addr *in6;
	struct ifnet *ifp;
{
	u_int32_t zoneid;

	sin6->sin6_addr = *in6;

	/*
	 * don't try to read *in6 beyond here, since the caller may
	 * ask us to overwrite existing sockaddr_in6
	 */

	sin6->sin6_scope_id = 0;
	if (IN6_IS_SCOPE_LINKLOCAL(in6) || IN6_IS_ADDR_MC_INTFACELOCAL(in6)) {
		/*
		 * KAME assumption: link id == interface id
		 */
		zoneid = ntohs(sin6->sin6_addr.s6_addr16[1]);
		if (zoneid) {
			/* sanity check */
			if (zoneid < 0 || if_indexlim <= zoneid)
				return ENXIO;
#if defined(__FreeBSD__) && __FreeBSD__ >= 5
			if (!ifnet_byindex(zoneid))
#else
			if (!ifindex2ifnet[zoneid])
#endif
				return ENXIO;
			if (ifp && ifp->if_index != zoneid)
				return ENXIO;
			sin6->sin6_addr.s6_addr16[1] = 0;
			sin6->sin6_scope_id = zoneid;
		}
	}

	return 0;
}

/*
 * just clear the embedded scope identifier.
 */
void
in6_clearscope(addr)
	struct in6_addr *addr;
{
	if (IN6_IS_SCOPE_LINKLOCAL(addr) || IN6_IS_ADDR_MC_INTFACELOCAL(addr))
		addr->s6_addr16[1] = 0;
}

void
addrsel_policy_init()
{
#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_LOCK_INIT();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */

	init_policy_queue();

	/* initialize the "last resort" policy */
	bzero(&defaultaddrpolicy, sizeof(defaultaddrpolicy));
	defaultaddrpolicy.label = ADDR_LABEL_NOTAPP;
}

static struct in6_addrpolicy *
lookup_addrsel_policy(key)
	struct sockaddr_in6 *key;
{
	struct in6_addrpolicy *match = NULL;

#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_LOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */
	match = match_addrsel_policy(key);

	if (match == NULL)
		match = &defaultaddrpolicy;
	else
		match->use++;
#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_UNLOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */

	return (match);
}

/*
 * Subroutines to manage the address selection policy table via sysctl.
 */
#ifdef __FreeBSD__
struct walkarg {
	struct sysctl_req *w_req;
};
#else
struct walkarg {
	size_t	w_total;
	size_t	w_given;
	caddr_t	w_where;
	caddr_t w_limit;
};
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__)
int
in6_src_sysctl(oldp, oldlenp, newp, newlen)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	int error = 0;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif

	if (newp) {
		error = EPERM;
		goto end;
	}
	if (oldp && oldlenp == NULL) {
		error = EINVAL;
		goto end;
	}
	if (oldp || oldlenp) {
		struct walkarg w;
		size_t oldlen = (oldlenp ? *oldlenp : 0);

		bzero(&w, sizeof(w));
		w.w_given = oldlen;
		w.w_where = oldp;
		if (oldp)
			w.w_limit = (caddr_t)oldp + oldlen;

		error = walk_addrsel_policy(dump_addrsel_policyent, &w);

		*oldlenp = w.w_total;
		if (oldp && w.w_total > oldlen && error == 0)
			error = ENOMEM;
	}

  end:
	splx(s);

	return (error);
}
#else
static int in6_src_sysctl(SYSCTL_HANDLER_ARGS);
#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_inet6_ip6);
#endif
SYSCTL_NODE(_net_inet6_ip6, IPV6CTL_ADDRCTLPOLICY, addrctlpolicy,
	CTLFLAG_RD, in6_src_sysctl, "");

static int
in6_src_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct walkarg w;

	if (req->newptr)
		return EPERM;

	bzero(&w, sizeof(w));
	w.w_req = req;

	return (walk_addrsel_policy(dump_addrsel_policyent, &w));
}
#endif /* FreeBSD */

int
in6_src_ioctl(cmd, data)
	u_long cmd;
	caddr_t data;
{
	int i;
	struct in6_addrpolicy ent0;

	if (cmd != SIOCAADDRCTL_POLICY && cmd != SIOCDADDRCTL_POLICY)
		return (EOPNOTSUPP); /* check for safety */

	ent0 = *(struct in6_addrpolicy *)data;

	if (ent0.label == ADDR_LABEL_NOTAPP)
		return (EINVAL);
	/* check if the prefix mask is consecutive. */
	if (in6_mask2len(&ent0.addrmask.sin6_addr, NULL) < 0)
		return (EINVAL);
	/* clear trailing garbages (if any) of the prefix address. */
	for (i = 0; i < 4; i++) {
		ent0.addr.sin6_addr.s6_addr32[i] &=
			ent0.addrmask.sin6_addr.s6_addr32[i];
	}
	ent0.use = 0;

	switch (cmd) {
	case SIOCAADDRCTL_POLICY:
		return (add_addrsel_policyent(&ent0));
	case SIOCDADDRCTL_POLICY:
		return (delete_addrsel_policyent(&ent0));
	}

	return (0);		/* XXX: compromise compilers */
}

/*
 * The followings are implementation of the policy table using a
 * simple tail queue.
 * XXX such details should be hidden.
 * XXX implementation using binary tree should be more efficient.
 */
struct addrsel_policyent {
	TAILQ_ENTRY(addrsel_policyent) ape_entry;
	struct in6_addrpolicy ape_policy;
};

TAILQ_HEAD(addrsel_policyhead, addrsel_policyent);

struct addrsel_policyhead addrsel_policytab;

static void
init_policy_queue()
{
	TAILQ_INIT(&addrsel_policytab);
}

static int
add_addrsel_policyent(newpolicy)
	struct in6_addrpolicy *newpolicy;
{
	struct addrsel_policyent *new, *pol;

#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_LOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */

	/* duplication check */
	for (pol = TAILQ_FIRST(&addrsel_policytab); pol;
	     pol = TAILQ_NEXT(pol, ape_entry)) {
		if (IN6_ARE_ADDR_EQUAL(&newpolicy->addr.sin6_addr,
		    &pol->ape_policy.addr.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&newpolicy->addrmask.sin6_addr,
		    &pol->ape_policy.addrmask.sin6_addr)) {
#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
			ADDRSEL_UNLOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */
			return (EEXIST);	/* or override it? */
		}
	}

	MALLOC(new, struct addrsel_policyent *, sizeof(*new), M_IFADDR,
	       M_WAITOK);
	bzero(new, sizeof(*new));

	/* XXX: should validate entry */
	new->ape_policy = *newpolicy;

	TAILQ_INSERT_TAIL(&addrsel_policytab, new, ape_entry);
#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_UNLOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */

	return (0);
}

static int
delete_addrsel_policyent(key)
	struct in6_addrpolicy *key;
{
	struct addrsel_policyent *pol;

#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_LOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */

	/* search for the entry in the table */
	for (pol = TAILQ_FIRST(&addrsel_policytab); pol;
	     pol = TAILQ_NEXT(pol, ape_entry)) {
		if (IN6_ARE_ADDR_EQUAL(&key->addr.sin6_addr,
		    &pol->ape_policy.addr.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&key->addrmask.sin6_addr,
		    &pol->ape_policy.addrmask.sin6_addr)) {
			break;
		}
	}
	if (pol == NULL) {
#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
		ADDRSEL_UNLOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */
		return (ESRCH);
	}

	TAILQ_REMOVE(&addrsel_policytab, pol, ape_entry);
#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_UNLOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */

	return (0);
}

static int
walk_addrsel_policy(callback, w)
	int (*callback) __P((struct in6_addrpolicy *, void *));
	void *w;
{
	struct addrsel_policyent *pol;
	int error = 0;

#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_LOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */
	for (pol = TAILQ_FIRST(&addrsel_policytab); pol;
	     pol = TAILQ_NEXT(pol, ape_entry)) {
		if ((error = (*callback)(&pol->ape_policy, w)) != 0) {
#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
			ADDRSEL_UNLOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */
			return (error);
		}
	}
#if defined(__FreeBSD__) && (__FreeBSD_version >= 502010)
	ADDRSEL_UNLOCK();
#endif /* __FreeBSD__ && __FreeBSD_version >= 502010 */

	return (error);
}

static int
dump_addrsel_policyent(pol, arg)
	struct in6_addrpolicy *pol;
	void *arg;
{
	int error = 0;
	struct walkarg *w = arg;

#ifdef __FreeBSD__
	error = SYSCTL_OUT(w->w_req, pol, sizeof(*pol));
#else
	if (w->w_where && w->w_where + sizeof(*pol) <= w->w_limit) {
		if ((error = copyout(pol, w->w_where, sizeof(*pol))) != 0)
			return (error);
		w->w_where += sizeof(*pol);
	}
	w->w_total += sizeof(*pol);
#endif

	return (error);
}

static struct in6_addrpolicy *
match_addrsel_policy(key)
	struct sockaddr_in6 *key;
{
	struct addrsel_policyent *pent;
	struct in6_addrpolicy *bestpol = NULL, *pol;
	int matchlen, bestmatchlen = -1;
	u_char *mp, *ep, *k, *p, m;

	for (pent = TAILQ_FIRST(&addrsel_policytab); pent;
	     pent = TAILQ_NEXT(pent, ape_entry)) {
		matchlen = 0;

		pol = &pent->ape_policy;
		mp = (u_char *)&pol->addrmask.sin6_addr;
		ep = mp + 16;	/* XXX: scope field? */
		k = (u_char *)&key->sin6_addr;
		p = (u_char *)&pol->addr.sin6_addr;
		for (; mp < ep && *mp; mp++, k++, p++) {
			m = *mp;
			if ((*k & m) != *p)
				goto next; /* not match */
			if (m == 0xff) /* short cut for a typical case */
				matchlen += 8;
			else {
				while (m >= 0x80) {
					matchlen++;
					m <<= 1;
				}
			}
		}

		/* matched.  check if this is better than the current best. */
		if (bestpol == NULL ||
		    matchlen > bestmatchlen) {
			bestpol = pol;
			bestmatchlen = matchlen;
		}

	  next:
		continue;
	}

	return (bestpol);
}
