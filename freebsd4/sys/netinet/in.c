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
 *	@(#)in.c	8.4 (Berkeley) 1/9/95
 * $FreeBSD: src/sys/netinet/in.c,v 1.44.2.14 2002/11/08 00:45:50 suz Exp $
 */


#include "opt_bootp.h"
#include "opt_inet.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>

#include <netinet/igmp_var.h>

static MALLOC_DEFINE(M_IPMADDR, "in_multi", "internet multicast address");

static int in_mask2len __P((struct in_addr *));
static void in_len2mask __P((struct in_addr *, int));
static int in_lifaddr_ioctl __P((struct socket *, u_long, caddr_t,
	struct ifnet *, struct proc *));

static void	in_socktrim __P((struct sockaddr_in *));
static int	in_ifinit __P((struct ifnet *,
	    struct in_ifaddr *, struct sockaddr_in *, int));

static int subnetsarelocal = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, subnets_are_local, CTLFLAG_RW, 
	&subnetsarelocal, 0, "");

struct in_multihead in_multihead; /* XXX BSS initialization */

extern struct inpcbinfo	ripcbinfo;
extern struct inpcbinfo udbinfo;

/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net.
 * Otherwise, it includes only the directly-connected (sub)nets.
 */
int
in_localaddr(in)
	struct in_addr in;
{
	register u_long i = ntohl(in.s_addr);
	register struct in_ifaddr *ia;

	if (subnetsarelocal) {
		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link)
			if ((i & ia->ia_netmask) == ia->ia_net)
				return (1);
	} else {
		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link)
			if ((i & ia->ia_subnetmask) == ia->ia_subnet)
				return (1);
	}
	return (0);
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(in)
	struct in_addr in;
{
	register u_long i = ntohl(in.s_addr);
	register u_long net;

	if (IN_EXPERIMENTAL(i) || IN_MULTICAST(i))
		return (0);
	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		if (net == 0 || net == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * Trim a mask in a sockaddr
 */
static void
in_socktrim(ap)
struct sockaddr_in *ap;
{
    register char *cplim = (char *) &ap->sin_addr;
    register char *cp = (char *) (&ap->sin_addr + 1);

    ap->sin_len = 0;
    while (--cp >= cplim)
        if (*cp) {
	    (ap)->sin_len = cp - (char *) (ap) + 1;
	    break;
	}
}

static int
in_mask2len(mask)
	struct in_addr *mask;
{
	int x, y;
	u_char *p;

	p = (u_char *)mask;
	for (x = 0; x < sizeof(*mask); x++) {
		if (p[x] != 0xff)
			break;
	}
	y = 0;
	if (x < sizeof(*mask)) {
		for (y = 0; y < 8; y++) {
			if ((p[x] & (0x80 >> y)) == 0)
				break;
		}
	}
	return x * 8 + y;
}

static void
in_len2mask(mask, len)
	struct in_addr *mask;
	int len;
{
	int i;
	u_char *p;

	p = (u_char *)mask;
	bzero(mask, sizeof(*mask));
	for (i = 0; i < len / 8; i++)
		p[i] = 0xff;
	if (len % 8)
		p[i] = (0xff00 >> (len % 8)) & 0xff;
}

static int in_interfaces;	/* number of external internet interfaces */

/*
 * Generic internet control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
in_control(so, cmd, data, ifp, p)
	struct socket *so;
	u_long cmd;
	caddr_t data;
	register struct ifnet *ifp;
	struct proc *p;
{
	register struct ifreq *ifr = (struct ifreq *)data;
	register struct in_ifaddr *ia = 0, *iap;
	register struct ifaddr *ifa;
	struct in_addr dst;
	struct in_ifaddr *oia;
	struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	struct sockaddr_in oldaddr;
	int error, hostIsNew, iaIsNew, maskIsNew, s;

	iaIsNew = 0;

	switch (cmd) {
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		if (p && (error = suser(p)) != 0)
			return error;
		/*fall through*/
	case SIOCGLIFADDR:
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp, p);
	}

	/*
	 * Find address for this interface, if it exists.
	 *
	 * If an alias address was specified, find that one instead of
	 * the first one on the interface, if possible
	 */
	if (ifp) {
		dst = ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;
		LIST_FOREACH(iap, INADDR_HASH(dst.s_addr), ia_hash)
			if (iap->ia_ifp == ifp &&
			    iap->ia_addr.sin_addr.s_addr == dst.s_addr) {
				ia = iap;
				break;
			}
		if (ia == NULL)
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				iap = ifatoia(ifa);
				if (iap->ia_addr.sin_family == AF_INET) {
					ia = iap;
					break;
				}
			}
	}

	switch (cmd) {

	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifp == 0)
			return (EADDRNOTAVAIL);
		if (ifra->ifra_addr.sin_family == AF_INET) {
			for (oia = ia; ia; ia = TAILQ_NEXT(ia, ia_link)) {
				if (ia->ia_ifp == ifp  &&
				    ia->ia_addr.sin_addr.s_addr ==
				    ifra->ifra_addr.sin_addr.s_addr)
					break;
			}
			if ((ifp->if_flags & IFF_POINTOPOINT)
			    && (cmd == SIOCAIFADDR)
			    && (ifra->ifra_dstaddr.sin_addr.s_addr
				== INADDR_ANY)) {
				return EDESTADDRREQ;
			}
		}
		if (cmd == SIOCDIFADDR && ia == 0)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */
	case SIOCSIFADDR:
	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
		if (p && (error = suser(p)) != 0)
			return error;

		if (ifp == 0)
			return (EADDRNOTAVAIL);
		if (ia == (struct in_ifaddr *)0) {
			ia = (struct in_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK);
			if (ia == (struct in_ifaddr *)NULL)
				return (ENOBUFS);
			bzero((caddr_t)ia, sizeof *ia);
			/*
			 * Protect from ipintr() traversing address list
			 * while we're modifying it.
			 */
			s = splnet();
			
			TAILQ_INSERT_TAIL(&in_ifaddrhead, ia, ia_link);
			ifa = &ia->ia_ifa;
			TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);

			ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
			ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
			ifa->ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;
			ia->ia_sockmask.sin_len = 8;
			ia->ia_sockmask.sin_family = AF_INET;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;
			if (!(ifp->if_flags & IFF_LOOPBACK))
				in_interfaces++;
			iaIsNew = 1;
			splx(s);
		}
		break;

	case SIOCSIFBRDADDR:
		if (p && (error = suser(p)) != 0)
			return error;
		/* FALLTHROUGH */

	case SIOCGIFADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
		if (ia == (struct in_ifaddr *)0)
			return (EADDRNOTAVAIL);
		break;
	}
	switch (cmd) {

	case SIOCGIFADDR:
		*((struct sockaddr_in *)&ifr->ifr_addr) = ia->ia_addr;
		return (0);

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		*((struct sockaddr_in *)&ifr->ifr_dstaddr) = ia->ia_broadaddr;
		return (0);

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		*((struct sockaddr_in *)&ifr->ifr_dstaddr) = ia->ia_dstaddr;
		return (0);

	case SIOCGIFNETMASK:
		*((struct sockaddr_in *)&ifr->ifr_addr) = ia->ia_sockmask;
		return (0);

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = *(struct sockaddr_in *)&ifr->ifr_dstaddr;
		if (ifp->if_ioctl && (error = (*ifp->if_ioctl)
					(ifp, SIOCSIFDSTADDR, (caddr_t)ia))) {
			ia->ia_dstaddr = oldaddr;
			return (error);
		}
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = (struct sockaddr *)&oldaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr =
					(struct sockaddr *)&ia->ia_dstaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
		}
		return (0);

	case SIOCSIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		ia->ia_broadaddr = *(struct sockaddr_in *)&ifr->ifr_broadaddr;
		return (0);

	case SIOCSIFADDR:
		error = in_ifinit(ifp, ia,
		    (struct sockaddr_in *) &ifr->ifr_addr, 1);
		if (error != 0 && iaIsNew)
			break;
		return (0);

	case SIOCSIFNETMASK:
		ia->ia_sockmask.sin_addr = ifra->ifra_addr.sin_addr;
		ia->ia_subnetmask = ntohl(ia->ia_sockmask.sin_addr.s_addr);
		return (0);

	case SIOCAIFADDR:
		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;
		if (ia->ia_addr.sin_family == AF_INET) {
			if (ifra->ifra_addr.sin_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ifra->ifra_addr.sin_addr.s_addr ==
					       ia->ia_addr.sin_addr.s_addr)
				hostIsNew = 0;
		}
		if (ifra->ifra_mask.sin_len) {
			in_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			ia->ia_sockmask.sin_family = AF_INET;
			ia->ia_subnetmask =
			     ntohl(ia->ia_sockmask.sin_addr.s_addr);
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin_family == AF_INET)) {
			in_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskIsNew  = 1; /* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.sin_family == AF_INET &&
		    (hostIsNew || maskIsNew))
			error = in_ifinit(ifp, ia, &ifra->ifra_addr, 0);

		if (error != 0 && iaIsNew)
			break;

		if ((ifp->if_flags & IFF_BROADCAST) &&
		    (ifra->ifra_broadaddr.sin_family == AF_INET))
			ia->ia_broadaddr = ifra->ifra_broadaddr;
		return (error);

	case SIOCDIFADDR:
		/*
		 * in_ifscrub kills the interface route.
		 */
		in_ifscrub(ifp, ia);
		/*
		 * in_ifadown gets rid of all the rest of
		 * the routes.  This is not quite the right
		 * thing to do, but at least if we are running
		 * a routing process they will come back.
		 */
		in_ifadown(&ia->ia_ifa, 1);
		/*
		 * XXX horrible hack to detect that we are being called
		 * from if_detach()
		 */
		if (!ifnet_addrs[ifp->if_index - 1]) {
			in_pcbpurgeif0(LIST_FIRST(ripcbinfo.listhead), ifp);
			in_pcbpurgeif0(LIST_FIRST(udbinfo.listhead), ifp);
		}
		error = 0;
		break;

#ifdef IGMPV3
	case SIOCSIPMSFILTER:
		/* Set IPv4 Multicast Source Filter */
		return (ip_setmopt_srcfilter(so, (struct ip_msfilter **)data));
	case SIOCGIPMSFILTER:
		/* Get IPv4 Multicast Source Filter */
		return (ip_getmopt_srcfilter(so, (struct ip_msfilter **)data));
	case SIOCSMSFILTER:
		/* Set Protocol-Independent Multicast Source Filter */
		return (sock_setmopt_srcfilter
				(so, (struct group_filter **)data));
	case SIOCGMSFILTER:
		/* Get Protocol-Independent Multicast Source Filter */
		return (sock_getmopt_srcfilter
				(so, (struct group_filter **)data));
#ifdef IGMPV3_DEBUG
	case SIOCGMSFILTERDUMP:
		return (dump_in_multisrc());
#endif
#endif /* IGMPV3 */

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}

	/*
	 * Protect from ipintr() traversing address list while we're modifying
	 * it.
	 */
	s = splnet();
	TAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifa_link);
	TAILQ_REMOVE(&in_ifaddrhead, ia, ia_link);
	LIST_REMOVE(ia, ia_hash);
	IFAFREE(&ia->ia_ifa);
	splx(s);

	return (error);
}

/*
 * SIOC[GAD]LIFADDR.
 *	SIOCGLIFADDR: get first address. (?!?)
 *	SIOCGLIFADDR with IFLR_PREFIX:
 *		get first address that matches the specified prefix.
 *	SIOCALIFADDR: add the specified address.
 *	SIOCALIFADDR with IFLR_PREFIX:
 *		EINVAL since we can't deduce hostid part of the address.
 *	SIOCDLIFADDR: delete the specified address.
 *	SIOCDLIFADDR with IFLR_PREFIX:
 *		delete the first address that matches the specified prefix.
 * return values:
 *	EINVAL on invalid parameters
 *	EADDRNOTAVAIL on prefix match failed/specified address not found
 *	other values may be returned from in_ioctl()
 */
static int
in_lifaddr_ioctl(so, cmd, data, ifp, p)
	struct socket *so;
	u_long cmd;
	caddr_t	data;
	struct ifnet *ifp;
	struct proc *p;
{
	struct if_laddrreq *iflr = (struct if_laddrreq *)data;
	struct ifaddr *ifa;

	/* sanity checks */
	if (!data || !ifp) {
		panic("invalid argument to in_lifaddr_ioctl");
		/*NOTRECHED*/
	}

	switch (cmd) {
	case SIOCGLIFADDR:
		/* address must be specified on GET with IFLR_PREFIX */
		if ((iflr->flags & IFLR_PREFIX) == 0)
			break;
		/*FALLTHROUGH*/
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		/* address must be specified on ADD and DELETE */
		if (iflr->addr.ss_family != AF_INET)
			return EINVAL;
		if (iflr->addr.ss_len != sizeof(struct sockaddr_in))
			return EINVAL;
		/* XXX need improvement */
		if (iflr->dstaddr.ss_family
		 && iflr->dstaddr.ss_family != AF_INET)
			return EINVAL;
		if (iflr->dstaddr.ss_family
		 && iflr->dstaddr.ss_len != sizeof(struct sockaddr_in))
			return EINVAL;
		break;
	default: /*shouldn't happen*/
		return EOPNOTSUPP;
	}
	if (sizeof(struct in_addr) * 8 < iflr->prefixlen)
		return EINVAL;

	switch (cmd) {
	case SIOCALIFADDR:
	    {
		struct in_aliasreq ifra;

		if (iflr->flags & IFLR_PREFIX)
			return EINVAL;

		/* copy args to in_aliasreq, perform ioctl(SIOCAIFADDR_IN). */
		bzero(&ifra, sizeof(ifra));
		bcopy(iflr->iflr_name, ifra.ifra_name,
			sizeof(ifra.ifra_name));

		bcopy(&iflr->addr, &ifra.ifra_addr, iflr->addr.ss_len);

		if (iflr->dstaddr.ss_family) {	/*XXX*/
			bcopy(&iflr->dstaddr, &ifra.ifra_dstaddr,
				iflr->dstaddr.ss_len);
		}

		ifra.ifra_mask.sin_family = AF_INET;
		ifra.ifra_mask.sin_len = sizeof(struct sockaddr_in);
		in_len2mask(&ifra.ifra_mask.sin_addr, iflr->prefixlen);

		return in_control(so, SIOCAIFADDR, (caddr_t)&ifra, ifp, p);
	    }
	case SIOCGLIFADDR:
	case SIOCDLIFADDR:
	    {
		struct in_ifaddr *ia;
		struct in_addr mask, candidate, match;
		struct sockaddr_in *sin;
		int cmp;

		bzero(&mask, sizeof(mask));
		if (iflr->flags & IFLR_PREFIX) {
			/* lookup a prefix rather than address. */
			in_len2mask(&mask, iflr->prefixlen);

			sin = (struct sockaddr_in *)&iflr->addr;
			match.s_addr = sin->sin_addr.s_addr;
			match.s_addr &= mask.s_addr;

			/* if you set extra bits, that's wrong */
			if (match.s_addr != sin->sin_addr.s_addr)
				return EINVAL;

			cmp = 1;
		} else {
			if (cmd == SIOCGLIFADDR) {
				/* on getting an address, take the 1st match */
				cmp = 0;	/*XXX*/
			} else {
				/* on deleting an address, do exact match */
				in_len2mask(&mask, 32);
				sin = (struct sockaddr_in *)&iflr->addr;
				match.s_addr = sin->sin_addr.s_addr;

				cmp = 1;
			}
		}

		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)	{
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			if (!cmp)
				break;
			candidate.s_addr = ((struct sockaddr_in *)&ifa->ifa_addr)->sin_addr.s_addr;
			candidate.s_addr &= mask.s_addr;
			if (candidate.s_addr == match.s_addr)
				break;
		}
		if (!ifa)
			return EADDRNOTAVAIL;
		ia = (struct in_ifaddr *)ifa;

		if (cmd == SIOCGLIFADDR) {
			/* fill in the if_laddrreq structure */
			bcopy(&ia->ia_addr, &iflr->addr, ia->ia_addr.sin_len);

			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia->ia_dstaddr, &iflr->dstaddr,
					ia->ia_dstaddr.sin_len);
			} else
				bzero(&iflr->dstaddr, sizeof(iflr->dstaddr));

			iflr->prefixlen =
				in_mask2len(&ia->ia_sockmask.sin_addr);

			iflr->flags = 0;	/*XXX*/

			return 0;
		} else {
			struct in_aliasreq ifra;

			/* fill in_aliasreq and do ioctl(SIOCDIFADDR_IN) */
			bzero(&ifra, sizeof(ifra));
			bcopy(iflr->iflr_name, ifra.ifra_name,
				sizeof(ifra.ifra_name));

			bcopy(&ia->ia_addr, &ifra.ifra_addr,
				ia->ia_addr.sin_len);
			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia->ia_dstaddr, &ifra.ifra_dstaddr,
					ia->ia_dstaddr.sin_len);
			}
			bcopy(&ia->ia_sockmask, &ifra.ifra_dstaddr,
				ia->ia_sockmask.sin_len);

			return in_control(so, SIOCDIFADDR, (caddr_t)&ifra,
					  ifp, p);
		}
	    }
	}

	return EOPNOTSUPP;	/*just for safety*/
}

/*
 * Delete any existing route for an interface.
 */
void
in_ifscrub(ifp, ia)
	register struct ifnet *ifp;
	register struct in_ifaddr *ia;
{

	if ((ia->ia_flags & IFA_ROUTE) == 0)
		return;
	if (ifp->if_flags & (IFF_LOOPBACK|IFF_POINTOPOINT))
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
	else
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
	ia->ia_flags &= ~IFA_ROUTE;
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
static int
in_ifinit(ifp, ia, sin, scrub)
	register struct ifnet *ifp;
	register struct in_ifaddr *ia;
	struct sockaddr_in *sin;
	int scrub;
{
	register u_long i = ntohl(sin->sin_addr.s_addr);
	struct sockaddr_in oldaddr;
	int s = splimp(), flags = RTF_UP, error = 0;

	oldaddr = ia->ia_addr;
	if (oldaddr.sin_family == AF_INET)
		LIST_REMOVE(ia, ia_hash);
	ia->ia_addr = *sin;
	if (ia->ia_addr.sin_family == AF_INET)
		LIST_INSERT_HEAD(INADDR_HASH(ia->ia_addr.sin_addr.s_addr),
		    ia, ia_hash);
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl &&
	    (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia))) {
		splx(s);
		/* LIST_REMOVE(ia, ia_hash) is done in in_control */
		ia->ia_addr = oldaddr;
		if (ia->ia_addr.sin_family == AF_INET)
			LIST_INSERT_HEAD(INADDR_HASH(ia->ia_addr.sin_addr.s_addr),
			    ia, ia_hash);
		return (error);
	}
	splx(s);
	if (scrub) {
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
		in_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	}
	if (IN_CLASSA(i))
		ia->ia_netmask = IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		ia->ia_netmask = IN_CLASSB_NET;
	else
		ia->ia_netmask = IN_CLASSC_NET;
	/*
	 * The subnet mask usually includes at least the standard network part,
	 * but may may be smaller in the case of supernetting.
	 * If it is set, we believe it.
	 */
	if (ia->ia_subnetmask == 0) {
		ia->ia_subnetmask = ia->ia_netmask;
		ia->ia_sockmask.sin_addr.s_addr = htonl(ia->ia_subnetmask);
	} else
		ia->ia_netmask &= ia->ia_subnetmask;
	ia->ia_net = i & ia->ia_netmask;
	ia->ia_subnet = i & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_BROADCAST) {
		ia->ia_broadaddr.sin_addr.s_addr =
			htonl(ia->ia_subnet | ~ia->ia_subnetmask);
		ia->ia_netbroadcast.s_addr =
			htonl(ia->ia_net | ~ ia->ia_netmask);
	} else if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin_family != AF_INET)
			return (0);
		flags |= RTF_HOST;
	}

	/*-
	 * Don't add host routes for interface addresses of
	 * 0.0.0.0 --> 0.255.255.255 netmask 255.0.0.0.  This makes it
	 * possible to assign several such address pairs with consistent
	 * results (no host route) and is required by BOOTP.
	 *
	 * XXX: This is ugly !  There should be a way for the caller to
	 *      say that they don't want a host route.
	 */
	if (ia->ia_addr.sin_addr.s_addr != INADDR_ANY ||
	    ia->ia_netmask != IN_CLASSA_NET ||
	    ia->ia_dstaddr.sin_addr.s_addr != htonl(IN_CLASSA_HOST)) {
		if ((error = rtinit(&ia->ia_ifa, (int)RTM_ADD, flags)) != 0) {
			ia->ia_addr = oldaddr;
			return (error);
		}
		ia->ia_flags |= IFA_ROUTE;
	}

	/*
	 * If the interface supports multicast, join the "all hosts"
	 * multicast group on that interface.
	 */
	if (ifp->if_flags & IFF_MULTICAST) {
		struct in_addr addr;

		addr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);
#ifdef IGMPV3
		in_addmulti(&addr, ifp, 0, NULL, MCAST_EXCLUDE, 0, &error);
#else
		in_addmulti(&addr, ifp);
#endif
	}
	return (error);
}


/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(in, ifp)
	struct in_addr in;
        struct ifnet *ifp;
{
	register struct ifaddr *ifa;
	u_long t;

	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return 1;
	if ((ifp->if_flags & IFF_BROADCAST) == 0)
		return 0;
	t = ntohl(in.s_addr);
	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 */
#define ia ((struct in_ifaddr *)ifa)
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    (in.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
		     in.s_addr == ia->ia_netbroadcast.s_addr ||
		     /*
		      * Check for old-style (host 0) broadcast.
		      */
		     t == ia->ia_subnet || t == ia->ia_net) &&
		     /*
		      * Check for an all one subnetmask. These
		      * only exist when an interface gets a secondary
		      * address.
		      */
		     ia->ia_subnetmask != (u_long)0xffffffff)
			    return 1;
	return (0);
#undef ia
}
/*
 * Add an address to the list of IP multicast addresses for a given interface.
 * Add source addresses to the list also, if upstream router is IGMPv3 capable
 * and the number of source is not 0.
 */
struct in_multi *
#ifdef IGMPV3
in_addmulti(ap, ifp, numsrc, ss, mode, init, error)
#else
in_addmulti(ap, ifp)
#endif
	register struct in_addr *ap;
	register struct ifnet *ifp;
#ifdef IGMPV3
	u_int16_t numsrc;
	struct sockaddr_storage *ss;
	u_int mode;			/* requested filter mode by socket */
	int init;			/* indicate initial join by socket */
	int *error;			/* return code of each sub routine */
#endif
{
	register struct in_multi *inm;
	struct sockaddr_in sin;
	struct ifmultiaddr *ifma;
#ifdef IGMPV3
	struct mbuf *m = NULL;
	struct ias_head *newhead = NULL;/* this may become new ims_cur->head */
	u_int curmode;			/* current filter mode */
	u_int newmode;			/* newly calculated filter mode */
	u_int16_t curnumsrc;		/* current ims_cur->numsrc */
	u_int16_t newnumsrc;		/* new ims_cur->numsrc */
	int timer_init = 1;		/* indicate timer initialization */
	int buflen = 0;
	u_int8_t type = 0;		/* State-Change report type */
	struct router_info *rti;
#else
	int dummy;			/* dummy */
	int *error = &dummy;		/* dummy */
#endif
	int s = splnet();

#ifdef IGMPV3
	/*
	 * MCAST_INCLUDE with empty source list means (*,G) leave.
	 */
	if ((mode == MCAST_INCLUDE) && (numsrc == 0)) {
	    *error = EINVAL;
	    splx(s);
	    return NULL;
	}
#endif

	/*
	 * Call generic routine to add membership or increment
	 * refcount.  It wants addresses in the form of a sockaddr,
	 * so we build one here (being careful to zero the unused bytes).
	 */
	bzero(&sin, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof sin;
	sin.sin_addr = *ap;
	*error = if_addmulti(ifp, (struct sockaddr *)&sin, &ifma);
	if (*error) {
		splx(s);
		return 0;
	}

	/*
	 * If ifma->ifma_protospec is null, then if_addmulti() created
	 * a new record.  Otherwise, we are done.
	 */
	if (ifma->ifma_protospec != 0) {
#ifdef IGMPV3
		inm = (struct in_multi *) ifma->ifma_protospec;
		/*
		 * Found it; merge source addresses in inm_source and send
		 * State-Change Report if needed, and increment the reference
		 * count. just return if group address is not the target of 
		 * IGMPv3 (i.e. 224.0.0.0/24)
		 */
		if (IN_LOCAL_GROUP(SIN_ADDR(ifma->ifma_addr))) {
			splx(s);
			return inm;
		}

		/* inm_source is already allocated. */
		curmode = inm->inm_source->ims_mode;
		curnumsrc = inm->inm_source->ims_cur->numsrc;

		/*
	 	 * Add each source address to inm_source and get new source
		 * filter mode and its calculated source list.
		 */
		if ((*error = in_addmultisrc(inm, numsrc, ss, mode, init,
			 	    &newhead, &newmode, &newnumsrc)) != 0) {
			splx(s);
			return NULL;
		}
		if (newhead != NULL) {
			/*
			 * Merge new source list to current pending report's 
			 * source list.
			 */
			if ((*error = in_merge_msf_state
					(inm, newhead, newmode, newnumsrc)) > 0) {
				/* 
				 * State-Change Report will not be sent. Just 
				 * return immediately. 
				 * Each ias linked from newhead is used by new 
				 * curhead, so only newhead is freed. 
				 */
				FREE(newhead, M_MSFILTER);
				*error = 0; /* to make caller behave as normal */
				splx(s);
				return inm;
			}
		} else {
			/* Only newhead was merged in a former function. */
			inm->inm_source->ims_mode = newmode;
			inm->inm_source->ims_cur->numsrc = newnumsrc;
		}

		/*
	 	 * Let IGMP know that we have joined an IP multicast group with
		 * source list if upstream router is IGMPv3 capable.
		 * If there was no pending source list change, an ALLOW or a
		 * BLOCK State-Change Report will not be sent, but a TO_IN or a
		 * TO_EX State-Change Report will be sent in any case.
		 */
		if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
			if (curmode != newmode || curnumsrc != newnumsrc) {
				if (curmode != newmode) {
					if (newmode == MCAST_INCLUDE)
						type = CHANGE_TO_INCLUDE_MODE;
					else
						type = CHANGE_TO_EXCLUDE_MODE;
				}
				igmp_send_state_change_report
					(&m, &buflen, inm, type, timer_init);
			}
		} else {
			/*
			 * If MSF's pending records exist, they must be deleted.
			 * Otherwise, ALW or BLK record will be blocked or pending
			 * list will never be cleaned when upstream router 
			 * switches to IGMPv3. XXX
			 */
			 in_clear_all_pending_report(inm);
		 }
		 *error = 0;
#endif
		splx(s);
		return ifma->ifma_protospec;
	}

	/* XXX - if_addmulti uses M_WAITOK.  Can this really be called
	   at interrupt time?  If so, need to fix if_addmulti. XXX */
	inm = (struct in_multi *)malloc(sizeof(*inm), M_IPMADDR, M_NOWAIT);
	if (inm == NULL) {
		*error = ENOBUFS;
		splx(s);
		return (NULL);
	}

	bzero(inm, sizeof *inm);
	inm->inm_addr = *ap;
	inm->inm_ifp = ifp;
	inm->inm_ifma = ifma;
	ifma->ifma_refcount = 1;
	ifma->ifma_protospec = inm;
	LIST_INSERT_HEAD(&in_multihead, inm, inm_link);

	/*
	 * Let IGMP know that we have joined a new IP multicast group.
	 */
#ifdef IGMPV3
	    for (rti = Head; rti != 0; rti = rti->rti_next) {
		if (rti->rti_ifp == inm->inm_ifp) {
		    inm->inm_rti = rti;
		    break;
		}
	    }
	    if (rti == 0) {
		if ((rti = rti_init(inm->inm_ifp)) == NULL) {
		    LIST_REMOVE(inm, inm_list);
		    if_delmulti(ifma->ifma_ifp, ifma->ifma_addr);
		    free(inm, M_IPMADDR);
		    *error = ENOBUFS;
		    splx(s);
		    return NULL;
	    	} else
		    inm->inm_rti = rti;
	    }

	    inm->inm_source = NULL;
	    if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		splx(s);
		return inm;
	    }

	    if ((*error = in_addmultisrc(inm, numsrc, ss, mode, init,
					&newhead, &newmode, &newnumsrc)) != 0) {
		in_free_all_msf_source_list(inm);
		LIST_REMOVE(inm, inm_list);
		if_delmulti(ifma->ifma_ifp, ifma->ifma_addr);
		free(inm, M_IPMADDR);
		splx(s);
		return NULL;
	    }
	    /* Only newhead was merged in a former function. */
	    curmode = inm->inm_source->ims_mode;
	    inm->inm_source->ims_mode = newmode;
	    inm->inm_source->ims_cur->numsrc = newnumsrc;

	    /*
	     * Let IGMP know that we have joined a new IP multicast group
	     * with source list if upstream router is IGMPv3 capable.
	     * If the router doesn't speak IGMPv3, then send Report message
	     * with no source address since it is a first join request.
	     */
	    if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode) {
		    if (newmode == MCAST_INCLUDE)
			type = CHANGE_TO_INCLUDE_MODE; /* never happen? */
		    else
			type = CHANGE_TO_EXCLUDE_MODE;
		}
		igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
	    } else {
		/*
		 * If MSF's pending records exist, they must be deleted.
		 */
		in_clear_all_pending_report(inm);
		igmp_joingroup(inm);
	    }
	    *error = 0;
#else
	igmp_joingroup(inm);
#endif
#ifdef IGMPV3
	if (newhead != NULL)
	    /* Each ias is linked from new curhead, so only newhead (not
	     * ias_list) is freed */
	    FREE(newhead, M_MSFILTER);
#endif

	splx(s);
	return (inm);
}

/*
 * Delete a multicast address record.
 */
void
#ifdef IGMPV3
in_delmulti(inm, numsrc, ss, mode, final, error)
#else
in_delmulti(inm)
#endif
	register struct in_multi *inm;
#ifdef IGMPV3
	u_int16_t numsrc;
	struct sockaddr_storage *ss;
	u_int mode;			/* requested filter mode by socket */
	int final;			/* indicate complete leave by socket */
	int *error;			/* return code of each sub routine */
#endif
{
	struct ifmultiaddr *ifma = inm->inm_ifma;
#ifdef IGMPV3
	struct mbuf *m = NULL;
	struct ias_head *newhead = NULL;/* this may become new ims_cur->head */
	u_int curmode;			/* current filter mode */
	u_int newmode;			/* newly calculated filter mode */
	u_int16_t curnumsrc;		/* current ims_cur->numsrc */
	u_int16_t newnumsrc;		/* new ims_cur->numsrc */
	int timer_init = 1;		/* indicate timer initialization */
	int buflen = 0;
	u_int8_t type = 0;		/* State-Change report type */
	struct ifreq ifr;
#else
	struct in_multi my_inm;
#endif
	int s = splnet();

#ifdef IGMPV3
	bzero(&ifr, sizeof(ifr));
	if ((mode == MCAST_INCLUDE) && (numsrc == 0)) {
		*error = EINVAL;
		splx(s);
		return;
	}
	if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		if (--ifma->ifma_refcount == 0) {

			/*
			 * Unlink from list.
			 */
			ifma->ifma_protospec = 0;
			LIST_REMOVE(inm, inm_list);
			/*
			 * Notify the network driver to update its multicast
			 * reception filter.
			 */
			satosin(&ifr.ifr_addr)->sin_family = AF_INET;
			satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
			(*inm->inm_ifp->if_ioctl)(inm->inm_ifp, SIOCDELMULTI,
							     (caddr_t)&ifr);
			free(inm, M_IPMADDR);
			if_delmulti(ifma->ifma_ifp, ifma->ifma_addr);
		}
		splx(s);
		return;
	}

	/* inm_source is already allocated */
	curmode = inm->inm_source->ims_mode;
	curnumsrc = inm->inm_source->ims_cur->numsrc;
	/*
	 * Delete each source address from inm_source and get new source
	 * filter mode and its calculated source list, and send State-Change
	 * Report if needed.
	 */
	if ((*error = in_delmultisrc(inm, numsrc, ss, mode, final,
				&newhead, &newmode, &newnumsrc)) != 0) {
		splx(s);
		return;
	}
	if (newhead != NULL) {
		if ((*error = in_merge_msf_state
				(inm, newhead, newmode, newnumsrc)) > 0) {
			/* State-Change Report will not be sent. Just return 
			 * immediately. */
			FREE(newhead, M_MSFILTER);
			splx(s);
			return;
		}
	} else {
		/* Only newhead was merged in a former function. */
		inm->inm_source->ims_mode = newmode;
		inm->inm_source->ims_cur->numsrc = newnumsrc;
	}

	/*
	 * If this is a final leave request by the socket, decrease
	 * refcount.
	 */
	if (final)
		--ifma->ifma_refcount;

	if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode || curnumsrc != newnumsrc) {
			if (curmode != newmode) {
				if (newmode == MCAST_INCLUDE)
					type = CHANGE_TO_INCLUDE_MODE;
				else
					type = CHANGE_TO_EXCLUDE_MODE;
			}
			igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
		}
	} else {
		/*
		 * If MSF's pending records exist, they must be deleted.
		 * Otherwise, ALW or BLK record will be blocked or pending
		 * list will never be cleaned when upstream router switches
		 * to IGMPv3. XXX
		 */
		in_clear_all_pending_report(inm);
		if (ifma->ifma_refcount == 0) {
			inm->inm_source->ims_robvar = 0;
			igmp_leavegroup(inm);
		}
	}

	if (ifma->ifma_refcount == 0) {
		/*
		 * We cannot use timer for robstness times report
		 * transmission when ifma->ifma_refcount becomes 0, since inm
		 * itself will be removed here. So, in this case, report
		 * retransmission will be done quickly. XXX my spec.
		 */
		timer_init = 0;
		while (inm->inm_source->ims_robvar > 0) {
			m = NULL;
			buflen = 0;
			igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
			if (m != NULL)
				igmp_sendbuf(m, inm->inm_ifp);
		}
		/*
		 * Unlink from list.
		 */
		in_free_all_msf_source_list(inm);
		LIST_REMOVE(inm, inm_list);
		ifma->ifma_protospec = 0;

		/*
		 * Notify the network driver to update its multicast
		 * reception filter.
		 */
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
		(*inm->inm_ifp->if_ioctl)
				(inm->inm_ifp, SIOCDELMULTI, (caddr_t)&ifr);
		free(inm, M_IPMADDR);
		if_delmulti(ifma->ifma_ifp, ifma->ifma_addr);
	}
	*error = 0;
	if (newhead != NULL)
		FREE(newhead, M_MSFILTER);
#else
	my_inm.inm_ifp = NULL ; /* don't send the leave msg */
	if (ifma->ifma_refcount == 1) {
		/*
		 * No remaining claims to this record; let IGMP know that
		 * we are leaving the multicast group.
		 * But do it after the if_delmulti() which might reset
		 * the interface and nuke the packet.
		 */
		my_inm = *inm ;
		ifma->ifma_protospec = 0;
		LIST_REMOVE(inm, inm_link);
		free(inm, M_IPMADDR);
	}
	/* XXX - should be separate API for when we have an ifma? */
	if_delmulti(ifma->ifma_ifp, ifma->ifma_addr);
	if (my_inm.inm_ifp != NULL)
		igmp_leavegroup(&my_inm);
#endif
	splx(s);
}

#ifdef IGMPV3
/*
 * Add an address to the list of IP multicast addresses for a given interface.
 * Add source addresses to the list also, if upstream router is IGMPv3 capable
 * and the number of source is not 0.
 */
struct in_multi *
in_modmulti(ap, ifp, numsrc, ss, mode,
		old_num, old_ss, old_mode, init, grpjoin, error)
	struct in_addr *ap;
	struct ifnet *ifp;
	u_int16_t numsrc, old_num;
	struct sockaddr_storage *ss, *old_ss;
	u_int mode, old_mode;		/* requested/current filter mode */
	int init;			/* indicate initial join by socket */
	u_int grpjoin;			/* on/off of (*,G) join by socket */
	int *error;			/* return code of each sub routine */
{
	struct mbuf *m = NULL;
	struct in_multi *inm;
	struct ifmultiaddr *ifma = NULL;
	struct ifreq ifr;
	struct ias_head *newhead = NULL;/* this becomes new ims_cur->head */
	u_int curmode;			/* current filter mode */
	u_int newmode;			/* newly calculated filter mode */
	u_int16_t curnumsrc;		/* current ims_cur->numsrc */
	u_int16_t newnumsrc;		/* new ims_cur->numsrc */
	int timer_init = 1;		/* indicate timer initialization */
	int buflen = 0;
	u_int8_t type = 0;		/* State-Change report type */
	struct router_info *rti;
	int s;

	*error = 0; /* initialize */

	if ((mode != MCAST_INCLUDE && mode != MCAST_EXCLUDE) ||
		(old_mode != MCAST_INCLUDE && old_mode != MCAST_EXCLUDE)) {
	    *error = EINVAL;
	    return NULL;
	}

	s = splnet();

	/*
	 * See if address already in list.
	 */
	IN_LOOKUP_MULTI(*ap, ifp, inm);

	if (inm != NULL) {
	    /*
	     * If requested multicast address is local address, update
	     * the condition, join or leave, based on a requested filter.
	     */
	    if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		if (numsrc != 0) {
		    splx(s);
		    *error = EINVAL;
		    return NULL; /* source filter is not supported for
				    local group address. */
		}
		if (mode == MCAST_INCLUDE) {
		    if (--inm->inm_ifma->ifma_refcount == 0) {
			/*
			 * Unlink from list.
			 */
			LIST_REMOVE(inm, inm_list);
			/*
			 * Notify the network driver to update its multicast
			 * reception filter.
			 */
			satosin(&ifr.ifr_addr)->sin_family = AF_INET;
			satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
			(*inm->inm_ifp->if_ioctl)(inm->inm_ifp, SIOCDELMULTI,
								(caddr_t)&ifr);
			free(inm, M_IPMADDR);
		    }
		    splx(s);
		    return NULL; /* not an error! */
		} else if (mode == MCAST_EXCLUDE) {
		    ++inm->inm_ifma->ifma_refcount;
		    splx(s);
		    return inm;
		}
	    }

	    /* inm_source is already allocated. */
	    curmode = inm->inm_source->ims_mode;
	    curnumsrc = inm->inm_source->ims_cur->numsrc;
	    if ((*error = in_modmultisrc(inm, numsrc, ss, mode,
					old_num, old_ss, old_mode, grpjoin,
					&newhead, &newmode, &newnumsrc)) != 0) {
		splx(s);
		return NULL;
	    }
	    if (newhead != NULL) {
		/*
		 * Merge new source list to current pending report's source
		 * list.
		 */
		if ((*error = in_merge_msf_state
				(inm, newhead, newmode, newnumsrc)) > 0) {
		    /* State-Change Report will not be sent. Just return
		     * immediately. */
		    FREE(newhead, M_MSFILTER);
		    splx(s);
		    return inm;
		}
	    } else {
		/* Only newhead was merged. */
		inm->inm_source->ims_mode = newmode;
		inm->inm_source->ims_cur->numsrc = newnumsrc;
	    }

	    /*
	     * Let IGMP know that we have joined an IP multicast group with
	     * source list if upstream router is IGMPv3 capable.
	     * If there was no pending source list change, an ALLOW or a
	     * BLOCK State-Change Report will not be sent, but a TO_IN or a
	     * TO_EX State-Change Report will be sent in any case.
	     */
	    if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode || curnumsrc != newnumsrc || old_num) {
			if (curmode != newmode) {
			    if (newmode == MCAST_INCLUDE)
				type = CHANGE_TO_INCLUDE_MODE;
			    else
				type = CHANGE_TO_EXCLUDE_MODE;
			}
			igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
		}
	    } else {
		/*
		 * If MSF's pending records exist, they must be deleted.
		 */
		in_clear_all_pending_report(inm);
	    }
	    *error = 0;
	    /* for this group address, initial join request by the socket. */
	    if (init)
		++inm->inm_ifma->ifma_refcount;

	} else {
	    struct sockaddr_in sa;

	    /*
	     * If there is some sources to be deleted, or if the request is
	     * join a local group address with some filtered address, return.
	     */
	    if ((old_num != 0) ||
	    		(IN_LOCAL_GROUP(ap->s_addr) && numsrc != 0)) {
		*error = EINVAL;
		splx(s);
		return NULL;
	    }

	    /*
	     * New address; allocate a new multicast record and link it into
	     * the interface's multicast list.
	     */
	    inm = (struct in_multi *)malloc(sizeof(*inm), M_IPMADDR, M_NOWAIT);
	    if (inm == NULL) {
		*error = ENOBUFS;
		splx(s);
		return NULL;
	    }

	    bzero(&sa, sizeof(sa));
	    sa.sin_family = AF_INET;
	    sa.sin_len = sizeof(struct sockaddr_in);
	    sa.sin_addr = *ap;
	    *error = if_addmulti(ifp, (struct sockaddr *)&sa, &ifma);
	    if (*error) {
		splx(s);
		return NULL;
	    }
	    if (ifma->ifma_protospec != NULL) {
#ifdef IGMPV3_DEBUG
		printf("in_modmulti: there's a corresponding if_multiaddr although IN_LOOKUP_MULTI fails \n");
#endif
		splx(s);
		return NULL;
	    }

	    bzero(inm, sizeof(*inm));
	    inm->inm_addr = *ap;
	    inm->inm_ifp = ifp;
	    inm->inm_ifma = ifma;
	    ifma->ifma_protospec = inm;
	    LIST_INSERT_HEAD(&in_multihead, inm, inm_link);

	    for (rti = Head; rti != 0; rti = rti->rti_next) {
		if (rti->rti_ifp == inm->inm_ifp) {
		    inm->inm_rti = rti;
		    break;
		}
	    }
	    if (rti == NULL) {
		if ((rti = rti_init(inm->inm_ifp)) == NULL) {
		    LIST_REMOVE(inm, inm_list);
		    free(inm, M_IPMADDR);
		    *error = ENOBUFS;
		    splx(s);
		    return NULL;
	    	}
		inm->inm_rti = rti;
	    }

	    inm->inm_source = NULL;
	    if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		splx(s);
		return inm;
	    }

	    if ((*error = in_modmultisrc(inm, numsrc, ss, mode, 0, NULL,
					MCAST_INCLUDE, grpjoin, &newhead,
					&newmode, &newnumsrc)) != 0) {
		in_free_all_msf_source_list(inm);
		LIST_REMOVE(inm, inm_list);
		free(inm, M_IPMADDR);
		splx(s);
		return NULL;
	    }
	    /* Only newhead was merged in a former function. */
	    curmode = inm->inm_source->ims_mode;
	    inm->inm_source->ims_mode = newmode;
	    inm->inm_source->ims_cur->numsrc = newnumsrc;

	    if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode) {
		    if (newmode == MCAST_INCLUDE)
			type = CHANGE_TO_INCLUDE_MODE;/* never happen??? */
		    else
			type = CHANGE_TO_EXCLUDE_MODE;
		}
		igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
	    } else {
		/*
		 * If MSF's pending records exist, they must be deleted.
		 */
		in_clear_all_pending_report(inm);
		igmp_joingroup(inm);
	    }
	    *error = 0;
	}
	if (newhead != NULL)
	    FREE(newhead, M_MSFILTER);

	splx(s);
	return inm;
}
#endif /* IGMPV3 */
