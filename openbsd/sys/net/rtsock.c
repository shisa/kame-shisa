/*	$OpenBSD: rtsock.c,v 1.35 2004/01/15 10:47:55 markus Exp $	*/
/*	$NetBSD: rtsock.c,v 1.18 1996/03/29 00:32:10 cgd Exp $	*/

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
 * Copyright (c) 1988, 1991, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)rtsock.c	8.6 (Berkeley) 2/11/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>

#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <sys/stdarg.h>

#ifdef SCTP
extern void sctp_add_ip_address(struct ifaddr *ifa);
extern void sctp_delete_ip_address(struct ifaddr *ifa);
#endif /* SCTP */

struct	sockaddr route_dst = { 2, PF_ROUTE, };
struct	sockaddr route_src = { 2, PF_ROUTE, };
struct	sockproto route_proto = { PF_ROUTE, };

struct walkarg {
	int	w_op, w_arg, w_given, w_needed, w_tmemsize;
	caddr_t	w_where, w_tmem;
};

static struct mbuf *
		rt_msg1(int, struct rt_addrinfo *);
static int	rt_msg2(int,
		    struct rt_addrinfo *, caddr_t, struct walkarg *);
static void	rt_xaddrs(caddr_t, caddr_t, struct rt_addrinfo *);

/* Sleazy use of local variables throughout file, warning!!!! */
#define dst	info.rti_info[RTAX_DST]
#define gate	info.rti_info[RTAX_GATEWAY]
#define netmask	info.rti_info[RTAX_NETMASK]
#define genmask	info.rti_info[RTAX_GENMASK]
#define ifpaddr	info.rti_info[RTAX_IFP]
#define ifaaddr	info.rti_info[RTAX_IFA]
#define brdaddr	info.rti_info[RTAX_BRD]

/*ARGSUSED*/
int
route_usrreq(so, req, m, nam, control)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	int error = 0;
	struct rawcb *rp = sotorawcb(so);
	int s;

	if (req == PRU_ATTACH) {
		MALLOC(rp, struct rawcb *, sizeof(*rp), M_PCB, M_WAITOK);
		so->so_pcb = rp;
		bzero(so->so_pcb, sizeof(*rp));
	}
	if (req == PRU_DETACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (af == AF_INET)
			route_cb.ip_count--;
		else if (af == AF_INET6)
			route_cb.ip6_count--;
		else if (af == AF_NS)
			route_cb.ns_count--;
		else if (af == AF_ISO)
			route_cb.iso_count--;
		route_cb.any_count--;
	}
	s = splsoftnet();
	/*
	 * Don't call raw_usrreq() in the attach case, because
	 * we want to allow non-privileged processes to listen on
	 * and send "safe" commands to the routing socket.
	 */
	if (req == PRU_ATTACH) {
		if (curproc == 0)
			error = EACCES;
		else
			error = raw_attach(so, (int)(long)nam);
	} else
		error = raw_usrreq(so, req, m, nam, control);

	rp = sotorawcb(so);
	if (req == PRU_ATTACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (error) {
			free((caddr_t)rp, M_PCB);
			splx(s);
			return (error);
		}
		if (af == AF_INET)
			route_cb.ip_count++;
		else if (af == AF_INET6)
			route_cb.ip6_count++;
		else if (af == AF_NS)
			route_cb.ns_count++;
		else if (af == AF_ISO)
			route_cb.iso_count++;
		rp->rcb_faddr = &route_src;
		route_cb.any_count++;
		soisconnected(so);
		so->so_options |= SO_USELOOPBACK;
	}
	splx(s);
	return (error);
}

/*ARGSUSED*/
int
route_output(struct mbuf *m, ...)
{
	struct rt_msghdr *rtm = 0;
	struct radix_node *rn = 0;
	struct rtentry *rt = 0;
	struct rtentry *saved_nrt = 0;
	struct radix_node_head *rnh;
	struct rt_addrinfo info;
	int len, error = 0;
	struct ifnet *ifp = 0;
	struct ifaddr *ifa = 0;
	struct socket *so;
	va_list ap;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	va_end(ap);

#define senderr(e) do { error = e; goto flush;} while (0)
	if (m == 0 || ((m->m_len < sizeof(int32_t)) &&
		       (m = m_pullup(m, sizeof(int32_t))) == 0))
		return (ENOBUFS);
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("route_output");
	len = m->m_pkthdr.len;
	if (len < sizeof(*rtm) ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen) {
		dst = 0;
		senderr(EINVAL);
	}
	R_Malloc(rtm, struct rt_msghdr *, len);
	if (rtm == 0) {
		dst = 0;
		senderr(ENOBUFS);
	}
	m_copydata(m, 0, len, (caddr_t)rtm);
	if (rtm->rtm_version != RTM_VERSION) {
		dst = 0;
		senderr(EPROTONOSUPPORT);
	}
	rtm->rtm_pid = curproc->p_pid;
	bzero(&info, sizeof(info));
	info.rti_addrs = rtm->rtm_addrs;
	rt_xaddrs((caddr_t)(rtm + 1), len + (caddr_t)rtm, &info);
	info.rti_flags = rtm->rtm_flags;
	if (dst == 0 || (dst->sa_family >= AF_MAX))
		senderr(EINVAL);
	if (gate != 0 && (gate->sa_family >= AF_MAX))
		senderr(EINVAL);
	if (genmask) {
		struct radix_node *t;
		t = rn_addmask((caddr_t)genmask, 0, 1);
		if (t && genmask->sa_len >= ((struct sockaddr *)t->rn_key)->sa_len &&
		    Bcmp((caddr_t *)genmask + 1, (caddr_t *)t->rn_key + 1,
		    ((struct sockaddr *)t->rn_key)->sa_len) - 1)
			genmask = (struct sockaddr *)(t->rn_key);
		else
			senderr(ENOBUFS);
	}

	/*
	 * Verify that the caller has the appropriate privilege; RTM_GET
	 * is the only operation the non-superuser is allowed.
	 */
	if (rtm->rtm_type != RTM_GET && suser(curproc, 0) != 0)
		senderr(EACCES);
	switch (rtm->rtm_type) {

	case RTM_DELETE:
		error = rtrequest1(rtm->rtm_type, &info, &saved_nrt);
		if (error == 0) {
			(rt = saved_nrt)->rt_refcnt++;
			goto report;
		}
		break;

	case RTM_ADD:
		if (gate == 0)
			senderr(EINVAL);
		error = rtrequest1(rtm->rtm_type, &info, &saved_nrt);
		if (error == 0 && saved_nrt) {
			rt_setmetrics(rtm->rtm_inits,
				&rtm->rtm_rmx, &saved_nrt->rt_rmx);
			saved_nrt->rt_refcnt--;
			saved_nrt->rt_genmask = genmask;
		}
		/* FALLTHROUGH */
	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		if ((rnh = rt_tables[dst->sa_family]) == 0) {
			senderr(EAFNOSUPPORT);
		}
		rn = rnh->rnh_lookup(dst, netmask, rnh);
		if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0) {
			senderr(ESRCH);
		}
		rt = (struct rtentry *)rn;
#ifdef RADIX_MPATH
		/*
		 * for RTM_CHANGE/LOCK, if we got multipath routes,
		 * we require users to specify a matching RTAX_GATEWAY.
		 *
		 * for RTM_GET, gate is optional even with multipath.
		 * if gate == NULL the first match is returned.
		 * (no need to call rt_mpath_matchgate if gate == NULL)
		 */
		if (rn_mpath_capable(rnh) &&
		    (rtm->rtm_type != RTM_GET || gate)) {
			rt = rt_mpath_matchgate(rt, gate);
			rn = (struct radix_node *)rt;
			if (!rt)
				senderr(ESRCH);
		}
#endif
		rt->rt_refcnt++;
		if (rtm->rtm_type != RTM_GET) {/* XXX: too grotty */
			struct radix_node *rn;
			extern struct radix_node_head *mask_rnhead;

			if (Bcmp(dst, rt_key(rt), dst->sa_len) != 0)
				senderr(ESRCH);
			if (netmask && (rn = rn_search(netmask,
					    mask_rnhead->rnh_treetop)))
				netmask = (struct sockaddr *)rn->rn_key;
			for (rn = rt->rt_nodes; rn; rn = rn->rn_dupedkey)
				if (netmask == (struct sockaddr *)rn->rn_mask)
					break;
			if (rn == 0)
				senderr(ETOOMANYREFS);
			rt = (struct rtentry *)rn;
		}

		switch (rtm->rtm_type) {

		case RTM_GET:
		report:
			dst = rt_key(rt);
			gate = rt->rt_gateway;
			netmask = rt_mask(rt);
			genmask = rt->rt_genmask;
			if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) {
				if ((ifp = rt->rt_ifp) != NULL) {
					ifpaddr =
					    TAILQ_FIRST(&ifp->if_addrlist)->ifa_addr;
					ifaaddr = rt->rt_ifa->ifa_addr;
					if (ifp->if_flags & IFF_POINTOPOINT)
						brdaddr = rt->rt_ifa->ifa_dstaddr;
					else
						brdaddr = 0;
					rtm->rtm_index = ifp->if_index;
				} else {
					ifpaddr = 0;
					ifaaddr = 0;
				}
			}
			len = rt_msg2(rtm->rtm_type, &info, (caddr_t)0,
				(struct walkarg *)0);
			if (len > rtm->rtm_msglen) {
				struct rt_msghdr *new_rtm;
				R_Malloc(new_rtm, struct rt_msghdr *, len);
				if (new_rtm == 0)
					senderr(ENOBUFS);
				Bcopy(rtm, new_rtm, rtm->rtm_msglen);
				Free(rtm); rtm = new_rtm;
			}
			(void)rt_msg2(rtm->rtm_type, &info, (caddr_t)rtm,
				(struct walkarg *)0);
			rtm->rtm_flags = rt->rt_flags;
			rtm->rtm_rmx = rt->rt_rmx;
			rtm->rtm_addrs = info.rti_addrs;
			break;

		case RTM_CHANGE:
			/*
			 * new gateway could require new ifaddr, ifp;
			 * flags may also be different; ifp may be specified
			 * by ll sockaddr when protocol address is ambiguous
			 */
			if ((error = rt_getifa(&info)) != 0)
				senderr(error);
			if (gate && rt_setgate(rt, rt_key(rt), gate))
				senderr(EDQUOT);
			if (ifpaddr && (ifa = ifa_ifwithnet(ifpaddr)) &&
			    (ifp = ifa->ifa_ifp) && (ifaaddr || gate))
				ifa = ifaof_ifpforaddr(ifaaddr ? ifaaddr : gate,
							ifp);
			else if ((ifaaddr && (ifa = ifa_ifwithaddr(ifaaddr))) ||
				 (gate && (ifa = ifa_ifwithroute(rt->rt_flags,
							rt_key(rt), gate))))
				ifp = ifa->ifa_ifp;
			if (ifa) {
				struct ifaddr *oifa = rt->rt_ifa;
				if (oifa != ifa) {
				    if (oifa && oifa->ifa_rtrequest)
					oifa->ifa_rtrequest(RTM_DELETE, rt,
					    &info);
				    IFAFREE(rt->rt_ifa);
				    rt->rt_ifa = ifa;
				    ifa->ifa_refcnt++;
				    rt->rt_ifp = ifp;
				}
			}
			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
					&rt->rt_rmx);
			if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest)
			       rt->rt_ifa->ifa_rtrequest(RTM_ADD, rt, &info);
			if (genmask)
				rt->rt_genmask = genmask;
			/*
			 * Fall into
			 */
		case RTM_ADD:
		case RTM_LOCK:
			rt->rt_rmx.rmx_locks &= ~(rtm->rtm_inits);
			rt->rt_rmx.rmx_locks |=
				(rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
			break;
		}
		break;

	default:
		senderr(EOPNOTSUPP);
	}

flush:
	if (rtm) {
		if (error)
			rtm->rtm_errno = error;
		else 
			rtm->rtm_flags |= RTF_DONE;
	}
	if (rt)
		rtfree(rt);
    {
	struct rawcb *rp = 0;
	/*
	 * Check to see if we don't want our own messages.
	 */
	if ((so->so_options & SO_USELOOPBACK) == 0) {
		if (route_cb.any_count <= 1) {
			if (rtm)
				Free(rtm);
			m_freem(m);
			return (error);
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}
	if (rp)
		rp->rcb_proto.sp_family = 0; /* Avoid us */
	if (dst)
		route_proto.sp_protocol = dst->sa_family;
	if (rtm) {
		m_copyback(m, 0, rtm->rtm_msglen, rtm);
		if (m->m_pkthdr.len < rtm->rtm_msglen) {
			m_freem(m);
			m = NULL;
		} else if (m->m_pkthdr.len > rtm->rtm_msglen)
			m_adj(m, rtm->rtm_msglen - m->m_pkthdr.len);
		Free(rtm);
	}
	if (m)
		raw_input(m, &route_proto, &route_src, &route_dst);
	if (rp)
		rp->rcb_proto.sp_family = PF_ROUTE;
    }
	return (error);
}

void
rt_setmetrics(which, in, out)
	u_long which;
	struct rt_metrics *in, *out;
{
#define metric(f, e) if (which & (f)) out->e = in->e;
	metric(RTV_RPIPE, rmx_recvpipe);
	metric(RTV_SPIPE, rmx_sendpipe);
	metric(RTV_SSTHRESH, rmx_ssthresh);
	metric(RTV_RTT, rmx_rtt);
	metric(RTV_RTTVAR, rmx_rttvar);
	metric(RTV_HOPCOUNT, rmx_hopcount);
	metric(RTV_MTU, rmx_mtu);
	metric(RTV_EXPIRE, rmx_expire);
#undef metric
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static void
rt_xaddrs(cp, cplim, rtinfo)
	caddr_t cp, cplim;
	struct rt_addrinfo *rtinfo;
{
	struct sockaddr *sa;
	int i;

	bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}

static struct mbuf *
rt_msg1(type, rtinfo)
	int type;
	struct rt_addrinfo *rtinfo;
{
	struct rt_msghdr *rtm;
	struct mbuf *m;
	int i;
	struct sockaddr *sa;
	int len, dlen;

	switch (type) {

	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	case RTM_IFANNOUNCE:
		len = sizeof(struct if_announcemsghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	if (len > MCLBYTES)
		panic("rt_msg1");
	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m && len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			m = NULL;
		}
	}
	if (m == 0)
		return (m);
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = 0;
	rtm = mtod(m, struct rt_msghdr *);
	bzero((caddr_t)rtm, len);
	for (i = 0; i < RTAX_MAX; i++) {
		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = ROUNDUP(sa->sa_len);
		m_copyback(m, len, dlen, sa);
		len += dlen;
	}
	if (m->m_pkthdr.len != len) {
		m_freem(m);
		return (NULL);
	}
	rtm->rtm_msglen = len;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	return (m);
}

static int
rt_msg2(type, rtinfo, cp, w)
	int type;
	struct rt_addrinfo *rtinfo;
	caddr_t cp;
	struct walkarg *w;
{
	int i;
	int len, dlen, second_time = 0;
	caddr_t cp0;

	rtinfo->rti_addrs = 0;
again:
	switch (type) {

	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	if ((cp0 = cp) != NULL)
		cp += len;
	for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr *sa;

		if ((sa = rtinfo->rti_info[i]) == 0)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = ROUNDUP(sa->sa_len);
		if (cp) {
			bcopy((caddr_t)sa, cp, (unsigned)dlen);
			cp += dlen;
		}
		len += dlen;
	}
	if (cp == 0 && w != NULL && !second_time) {
		struct walkarg *rw = w;

		rw->w_needed += len;
		if (rw->w_needed <= 0 && rw->w_where) {
			if (rw->w_tmemsize < len) {
				if (rw->w_tmem)
					free(rw->w_tmem, M_RTABLE);
				rw->w_tmem = (caddr_t) malloc(len, M_RTABLE,
							      M_NOWAIT);
				if (rw->w_tmem)
					rw->w_tmemsize = len;
			}
			if (rw->w_tmem) {
				cp = rw->w_tmem;
				second_time = 1;
				goto again;
			} else
				rw->w_where = 0;
		}
	}
	if (cp) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)cp0;

		rtm->rtm_version = RTM_VERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
	}
	return (len);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occurred, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
rt_missmsg(type, rtinfo, flags, error)
	int type, flags, error;
	struct rt_addrinfo *rtinfo;
{
	struct rt_msghdr *rtm;
	struct mbuf *m;
	struct sockaddr *sa = rtinfo->rti_info[RTAX_DST];

	if (route_cb.any_count == 0)
		return;
	m = rt_msg1(type, rtinfo);
	if (m == 0)
		return;
	rtm = mtod(m, struct rt_msghdr *);
	rtm->rtm_flags = RTF_DONE | flags;
	rtm->rtm_errno = error;
	rtm->rtm_addrs = rtinfo->rti_addrs;
	route_proto.sp_protocol = sa ? sa->sa_family : 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
void
rt_ifmsg(ifp)
	struct ifnet *ifp;
{
	struct if_msghdr *ifm;
	struct mbuf *m;
	struct rt_addrinfo info;

	if (route_cb.any_count == 0)
		return;
	bzero((caddr_t)&info, sizeof(info));
	m = rt_msg1(RTM_IFINFO, &info);
	if (m == 0)
		return;
	ifm = mtod(m, struct if_msghdr *);
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_flags = ifp->if_flags;
	ifm->ifm_data = ifp->if_data;
	ifm->ifm_addrs = 0;
	route_proto.sp_protocol = 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 * if we ever reverse the logic and replace messages TO the routing
 * socket indicate a request to configure interfaces, then it will
 * be unnecessary as the routing socket will automatically generate
 * copies of it.
 */
void
rt_newaddrmsg(cmd, ifa, error, rt)
	int cmd, error;
	struct ifaddr *ifa;
	struct rtentry *rt;
{
	struct rt_addrinfo info;
	struct sockaddr *sa = NULL;
	int pass;
	struct mbuf *m = NULL;
	struct ifnet *ifp = ifa->ifa_ifp;

#ifdef SCTP
	/*
	 * notify the SCTP stack
	 * this will only get called when an address is added/deleted
	 * XXX pass the ifaddr struct instead if ifa->ifa_addr...
	 */
	if (cmd == RTM_ADD) {
		sctp_add_ip_address(ifa);
	} else if (cmd == RTM_DELETE) {
		sctp_delete_ip_address(ifa);
	}
#endif /* SCTP */

	if (route_cb.any_count == 0)
		return;
	for (pass = 1; pass < 3; pass++) {
		bzero((caddr_t)&info, sizeof(info));
		if ((cmd == RTM_ADD && pass == 1) ||
		    (cmd == RTM_DELETE && pass == 2)) {
			struct ifa_msghdr *ifam;
			int ncmd = cmd == RTM_ADD ? RTM_NEWADDR : RTM_DELADDR;

			ifaaddr = sa = ifa->ifa_addr;
			ifpaddr = TAILQ_FIRST(&ifp->if_addrlist)->ifa_addr;
			netmask = ifa->ifa_netmask;
			brdaddr = ifa->ifa_dstaddr;
			if ((m = rt_msg1(ncmd, &info)) == NULL)
				continue;
			ifam = mtod(m, struct ifa_msghdr *);
			ifam->ifam_index = ifp->if_index;
			ifam->ifam_metric = ifa->ifa_metric;
			ifam->ifam_flags = ifa->ifa_flags;
			ifam->ifam_addrs = info.rti_addrs;
		}
		if ((cmd == RTM_ADD && pass == 2) ||
		    (cmd == RTM_DELETE && pass == 1)) {
			struct rt_msghdr *rtm;
			
			if (rt == 0)
				continue;
			netmask = rt_mask(rt);
			dst = sa = rt_key(rt);
			gate = rt->rt_gateway;
			if ((m = rt_msg1(cmd, &info)) == NULL)
				continue;
			rtm = mtod(m, struct rt_msghdr *);
			rtm->rtm_index = ifp->if_index;
			rtm->rtm_flags |= rt->rt_flags;
			rtm->rtm_errno = error;
			rtm->rtm_addrs = info.rti_addrs;
		}
		route_proto.sp_protocol = sa ? sa->sa_family : 0;
		raw_input(m, &route_proto, &route_src, &route_dst);
	}
}

/*
 * This is called to generate routing socket messages indicating
 * network interface arrival and departure.
 */
void
rt_ifannouncemsg(ifp, what)
	struct ifnet *ifp;
	int what;
{
	struct if_announcemsghdr *ifan;
	struct mbuf *m;
	struct rt_addrinfo info;

	if (route_cb.any_count == 0)
		return;
	bzero(&info, sizeof(info));
	m = rt_msg1(RTM_IFANNOUNCE, &info);
	if (m == 0)
		return;
	ifan = mtod(m, struct if_announcemsghdr *);
	ifan->ifan_index = ifp->if_index;
	strlcpy(ifan->ifan_name, ifp->if_xname, sizeof(ifan->ifan_name));
	ifan->ifan_what = what;
	route_proto.sp_protocol = 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This is used in dumping the kernel table via sysctl().
 */
int
sysctl_dumpentry(rn, v)
	struct radix_node *rn;
	void *v;
{
	struct walkarg *w = v;
	struct rtentry *rt = (struct rtentry *)rn;
	int error = 0, size;
	struct rt_addrinfo info;

	if (w->w_op == NET_RT_FLAGS && !(rt->rt_flags & w->w_arg))
		return 0;
	bzero((caddr_t)&info, sizeof(info));
	dst = rt_key(rt);
	gate = rt->rt_gateway;
	netmask = rt_mask(rt);
	genmask = rt->rt_genmask;
	if (rt->rt_ifp) {
		ifpaddr = TAILQ_FIRST(&rt->rt_ifp->if_addrlist)->ifa_addr;
		ifaaddr = rt->rt_ifa->ifa_addr;
		if (rt->rt_ifp->if_flags & IFF_POINTOPOINT)
			brdaddr = rt->rt_ifa->ifa_dstaddr;
	}
	size = rt_msg2(RTM_GET, &info, 0, w);
	if (w->w_where && w->w_tmem && w->w_needed <= 0) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)w->w_tmem;

		rtm->rtm_flags = rt->rt_flags;
		rtm->rtm_use = rt->rt_use;
		rtm->rtm_rmx = rt->rt_rmx;
		rtm->rtm_index = rt->rt_ifp->if_index;
		rtm->rtm_errno = rtm->rtm_pid = rtm->rtm_seq = 0;
		rtm->rtm_addrs = info.rti_addrs;
		if ((error = copyout((caddr_t)rtm, w->w_where, size)) != 0)
			w->w_where = NULL;
		else
			w->w_where += size;
	}
	return (error);
}

int
sysctl_iflist(af, w)
	int	af;
	struct	walkarg *w;
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct	rt_addrinfo info;
	int	len, error = 0;

	bzero((caddr_t)&info, sizeof(info));
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (w->w_arg && w->w_arg != ifp->if_index)
			continue;
		ifa = TAILQ_FIRST(&ifp->if_addrlist);
		if (!ifa)
			continue;
		ifpaddr = ifa->ifa_addr;
		len = rt_msg2(RTM_IFINFO, &info, (caddr_t)0, w);
		ifpaddr = 0;
		if (w->w_where && w->w_tmem && w->w_needed <= 0) {
			struct if_msghdr *ifm;

			ifm = (struct if_msghdr *)w->w_tmem;
			ifm->ifm_index = ifp->if_index;
			ifm->ifm_flags = ifp->if_flags;
			ifm->ifm_data = ifp->if_data;
			ifm->ifm_addrs = info.rti_addrs;
			error = copyout((caddr_t)ifm, w->w_where, len);
			if (error)
				return (error);
			w->w_where += len;
		}
		while ((ifa = TAILQ_NEXT(ifa, ifa_list)) !=
		    TAILQ_END(&ifp->if_addrlist)) {
			if (af && af != ifa->ifa_addr->sa_family)
				continue;
			ifaaddr = ifa->ifa_addr;
			netmask = ifa->ifa_netmask;
			brdaddr = ifa->ifa_dstaddr;
			len = rt_msg2(RTM_NEWADDR, &info, 0, w);
			if (w->w_where && w->w_tmem && w->w_needed <= 0) {
				struct ifa_msghdr *ifam;

				ifam = (struct ifa_msghdr *)w->w_tmem;
				ifam->ifam_index = ifa->ifa_ifp->if_index;
				ifam->ifam_flags = ifa->ifa_flags;
				ifam->ifam_metric = ifa->ifa_metric;
				ifam->ifam_addrs = info.rti_addrs;
				error = copyout(w->w_tmem, w->w_where, len);
				if (error)
					return (error);
				w->w_where += len;
			}
		}
		ifaaddr = netmask = brdaddr = 0;
	}
	return (0);
}

int
sysctl_rtable(name, namelen, where, given, new, newlen)
	int	*name;
	u_int	namelen;
	void 	*where;
	size_t	*given;
	void	*new;
	size_t	newlen;
{
	struct radix_node_head *rnh;
	int	i, s, error = EINVAL;
	u_char  af;
	struct	walkarg w;

	if (new)
		return (EPERM);
	if (namelen != 3)
		return (EINVAL);
	af = name[0];
	Bzero(&w, sizeof(w));
	w.w_where = where;
	w.w_given = *given;
	w.w_needed = 0 - w.w_given;
	w.w_op = name[1];
	w.w_arg = name[2];

	s = splsoftnet();
	switch (w.w_op) {

	case NET_RT_DUMP:
	case NET_RT_FLAGS:
		for (i = 1; i <= AF_MAX; i++)
			if ((rnh = rt_tables[i]) && (af == 0 || af == i) &&
			    (error = (*rnh->rnh_walktree)(rnh,
							  sysctl_dumpentry,
							  &w)))
				break;
		break;

	case NET_RT_IFLIST:
		error = sysctl_iflist(af, &w);
	}
	splx(s);
	if (w.w_tmem)
		free(w.w_tmem, M_RTABLE);
	w.w_needed += w.w_given;
	if (where) {
		*given = w.w_where - (caddr_t) where;
		if (*given < w.w_needed)
			return (ENOMEM);
	} else {
		*given = (11 * w.w_needed) / 10;
	}
	return (error);
}

/*
 * Definitions of protocols supported in the ROUTE domain.
 */

extern	struct domain routedomain;		/* or at least forward */

struct protosw routesw[] = {
{ SOCK_RAW,	&routedomain,	0,		PR_ATOMIC|PR_ADDR,
  raw_input,	route_output,	raw_ctlinput,	0,
  route_usrreq,
  raw_init,	0,		0,		0,
  sysctl_rtable,
}
};

struct domain routedomain =
    { PF_ROUTE, "route", route_init, 0, 0,
      routesw, &routesw[sizeof(routesw)/sizeof(routesw[0])] };
