/*
 * Copyright (c) 1982, 1989, 1993
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
 *	@(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/net/if_ethersubr.c,v 1.155 2003/11/14 21:02:22 andre Exp $
 */

#include "opt_atalk.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipx.h"
#include "opt_bdg.h"
#include "opt_mac.h"
#include "opt_netgraph.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/bridge.h>
#include <net/if_vlan_var.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>
#endif
#ifdef INET6
#include <netinet6/nd6.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
int (*ef_inputp)(struct ifnet*, struct ether_header *eh, struct mbuf *m);
int (*ef_outputp)(struct ifnet *ifp, struct mbuf **mp,
		struct sockaddr *dst, short *tp, int *hlen);
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>

#define llc_snap_org_code llc_un.type_snap.org_code
#define llc_snap_ether_type llc_un.type_snap.ether_type

extern u_char	at_org_code[3];
extern u_char	aarp_org_code[3];
#endif /* NETATALK */

/* netgraph node hooks for ng_ether(4) */
void	(*ng_ether_input_p)(struct ifnet *ifp, struct mbuf **mp);
void	(*ng_ether_input_orphan_p)(struct ifnet *ifp, struct mbuf *m);
int	(*ng_ether_output_p)(struct ifnet *ifp, struct mbuf **mp);
void	(*ng_ether_attach_p)(struct ifnet *ifp);
void	(*ng_ether_detach_p)(struct ifnet *ifp);

void	(*vlan_input_p)(struct ifnet *, struct mbuf *);

/* bridge support */
int do_bridge;
bridge_in_t *bridge_in_ptr;
bdg_forward_t *bdg_forward_ptr;
bdgtakeifaces_t *bdgtakeifaces_ptr;
struct bdg_softc *ifp2sc;

static u_char etherbroadcastaddr[ETHER_ADDR_LEN] =
			{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static	int ether_resolvemulti(struct ifnet *, struct sockaddr **,
		struct sockaddr *);

#define senderr(e) do { error = (e); goto bad;} while (0)
#define IFP2AC(IFP) ((struct arpcom *)IFP)

int
ether_ipfw_chk(struct mbuf **m0, struct ifnet *dst,
	struct ip_fw **rule, int shared);
static int ether_ipfw;

/*
 * Ethernet output routine.
 * Encapsulate a packet of type family for the local net.
 * Use trailer local net encapsulation if enough data in first
 * packet leaves a multiple of 512 bytes of data in remainder.
 * Assumes that ifp is actually pointer to arpcom structure.
 */
int
ether_output(struct ifnet *ifp, struct mbuf *m,
	struct sockaddr *dst, struct rtentry *rt0)
{
	short type;
	int error = 0, hdrcmplt = 0;
	u_char esrc[ETHER_ADDR_LEN], edst[ETHER_ADDR_LEN];
	struct rtentry *rt;
	struct ether_header *eh;
	int loop_copy = 0;
	int hlen;	/* link layer header length */
	struct arpcom *ac = IFP2AC(ifp);

#ifdef MAC
	error = mac_check_ifnet_transmit(ifp, m);
	if (error)
		senderr(error);
#endif

	if (ifp->if_flags & IFF_MONITOR)
		senderr(ENETDOWN);
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);

	error = rt_check(&rt, &rt0, dst);
	if (error)
		goto bad;

	hlen = ETHER_HDR_LEN;
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		if (!arpresolve(ifp, rt, m, dst, edst, rt0))
			return (0);	/* if not yet resolved */
		type = htons(ETHERTYPE_IP);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (!nd6_storelladdr(&ac->ac_if, rt, m, dst, (u_char *)edst)) {
			/* Something bad happened */
			return(0);
		}
		type = htons(ETHERTYPE_IPV6);
		break;
#endif
#ifdef IPX
	case AF_IPX:
		if (ef_outputp) {
		    error = ef_outputp(ifp, &m, dst, &type, &hlen);
		    if (error)
			goto bad;
		} else
		    type = htons(ETHERTYPE_IPX);
		bcopy((caddr_t)&(((struct sockaddr_ipx *)dst)->sipx_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
	  {
	    struct at_ifaddr *aa;

	    if ((aa = at_ifawithnet((struct sockaddr_at *)dst)) == NULL) {
		    goto bad;
	    }
	    if (!aarpresolve(ac, m, (struct sockaddr_at *)dst, edst))
		    return (0);
	    /*
	     * In the phase 2 case, need to prepend an mbuf for the llc header.
	     * Since we must preserve the value of m, which is passed to us by
	     * value, we m_copy() the first mbuf, and use it for our llc header.
	     */
	    if ( aa->aa_flags & AFA_PHASE2 ) {
		struct llc llc;

		M_PREPEND(m, LLC_SNAPFRAMELEN, M_TRYWAIT);
		if (m == NULL)
			senderr(ENOBUFS);
		llc.llc_dsap = llc.llc_ssap = LLC_SNAP_LSAP;
		llc.llc_control = LLC_UI;
		bcopy(at_org_code, llc.llc_snap_org_code, sizeof(at_org_code));
		llc.llc_snap_ether_type = htons( ETHERTYPE_AT );
		bcopy(&llc, mtod(m, caddr_t), LLC_SNAPFRAMELEN);
		type = htons(m->m_pkthdr.len);
		hlen = LLC_SNAPFRAMELEN + ETHER_HDR_LEN;
	    } else {
		type = htons(ETHERTYPE_AT);
	    }
	    break;
	  }
#endif /* NETATALK */

	case pseudo_AF_HDRCMPLT:
		hdrcmplt = 1;
		eh = (struct ether_header *)dst->sa_data;
		(void)memcpy(esrc, eh->ether_shost, sizeof (esrc));
		/* FALLTHROUGH */

	case AF_UNSPEC:
		loop_copy = -1; /* if this is for us, don't do it */
		eh = (struct ether_header *)dst->sa_data;
		(void)memcpy(edst, eh->ether_dhost, sizeof (edst));
		type = eh->ether_type;
		break;

	default:
		if_printf(ifp, "can't handle af%d\n", dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
	if (m == NULL)
		senderr(ENOBUFS);
	eh = mtod(m, struct ether_header *);
	(void)memcpy(&eh->ether_type, &type,
		sizeof(eh->ether_type));
	(void)memcpy(eh->ether_dhost, edst, sizeof (edst));
	if (hdrcmplt)
		(void)memcpy(eh->ether_shost, esrc,
			sizeof(eh->ether_shost));
	else
		(void)memcpy(eh->ether_shost, ac->ac_enaddr,
			sizeof(eh->ether_shost));

	/*
	 * If a simplex interface, and the packet is being sent to our
	 * Ethernet address or a broadcast address, loopback a copy.
	 * XXX To make a simplex device behave exactly like a duplex
	 * device, we should copy in the case of sending to our own
	 * ethernet address (thus letting the original actually appear
	 * on the wire). However, we don't do that here for security
	 * reasons and compatibility with the original behavior.
	 */
	if ((ifp->if_flags & IFF_SIMPLEX) && (loop_copy != -1)) {
		int csum_flags = 0;

		if (m->m_pkthdr.csum_flags & CSUM_IP)
			csum_flags |= (CSUM_IP_CHECKED|CSUM_IP_VALID);
		if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA)
			csum_flags |= (CSUM_DATA_VALID|CSUM_PSEUDO_HDR);

		if ((m->m_flags & M_BCAST) || (loop_copy > 0)) {
			struct mbuf *n;

			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				n->m_pkthdr.csum_flags |= csum_flags;
				if (csum_flags & CSUM_DATA_VALID)
					n->m_pkthdr.csum_data = 0xffff;
				(void)if_simloop(ifp, n, dst->sa_family, hlen);
			} else
				ifp->if_iqdrops++;
		} else if (bcmp(eh->ether_dhost, eh->ether_shost,
				ETHER_ADDR_LEN) == 0) {
			m->m_pkthdr.csum_flags |= csum_flags;
			if (csum_flags & CSUM_DATA_VALID)
				m->m_pkthdr.csum_data = 0xffff;
			(void) if_simloop(ifp, m, dst->sa_family, hlen);
			return (0);	/* XXX */
		}
	}

	/* Handle ng_ether(4) processing, if any */
	if (ng_ether_output_p != NULL) {
		if ((error = (*ng_ether_output_p)(ifp, &m)) != 0) {
bad:			if (m != NULL)
				m_freem(m);
			return (error);
		}
		if (m == NULL)
			return (0);
	}

	/* Continue with link-layer output */
	return ether_output_frame(ifp, m);
}

/*
 * Ethernet link layer output routine to send a raw frame to the device.
 *
 * This assumes that the 14 byte Ethernet header is present and contiguous
 * in the first mbuf (if BRIDGE'ing).
 */
int
ether_output_frame(struct ifnet *ifp, struct mbuf *m)
{
	struct ip_fw *rule = NULL;
	int error;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	/* Extract info from dummynet tag, ignore others */
	for (; m->m_type == MT_TAG; m = m->m_next)
		if (m->m_flags == PACKET_TAG_DUMMYNET)
			rule = ((struct dn_pkt *)m)->rule;

	if (rule == NULL && BDG_ACTIVE(ifp)) {
		/*
		 * Beware, the bridge code notices the null rcvif and
		 * uses that identify that it's being called from
		 * ether_output as opposd to ether_input.  Yech.
		 */
		m->m_pkthdr.rcvif = NULL;
		m = bdg_forward_ptr(m, ifp);
		if (m != NULL)
			m_freem(m);
		return (0);
	}
	if (IPFW_LOADED && ether_ipfw != 0) {
		if (ether_ipfw_chk(&m, ifp, &rule, 0) == 0) {
			if (m) {
				m_freem(m);
				return ENOBUFS;	/* pkt dropped */
			} else
				return 0;	/* consumed e.g. in a pipe */
		}
	}

#ifdef ALTQ
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		altq_etherclassify(&ifp->if_snd, m, &pktattr);
#endif

	/*
	 * Queue message on interface, update output statistics if
	 * successful, and start output if interface not yet active.
	 */
	IFQ_HANDOFF(ifp, m, &pktattr, error);
	return (error);
}

/*
 * ipfw processing for ethernet packets (in and out).
 * The second parameter is NULL from ether_demux, and ifp from
 * ether_output_frame. This section of code could be used from
 * bridge.c as well as long as we use some extra info
 * to distinguish that case from ether_output_frame();
 */
int
ether_ipfw_chk(struct mbuf **m0, struct ifnet *dst,
	struct ip_fw **rule, int shared)
{
	struct ether_header *eh;
	struct ether_header save_eh;
	struct mbuf *m;
	int i;
	struct ip_fw_args args;

	if (*rule != NULL && fw_one_pass)
		return 1; /* dummynet packet, already partially processed */

	/*
	 * I need some amt of data to be contiguous, and in case others need
	 * the packet (shared==1) also better be in the first mbuf.
	 */
	m = *m0;
	i = min( m->m_pkthdr.len, max_protohdr);
	if ( shared || m->m_len < i) {
		m = m_pullup(m, i);
		if (m == NULL) {
			*m0 = m;
			return 0;
		}
	}
	eh = mtod(m, struct ether_header *);
	save_eh = *eh;			/* save copy for restore below */
	m_adj(m, ETHER_HDR_LEN);	/* strip ethernet header */

	args.m = m;		/* the packet we are looking at		*/
	args.oif = dst;		/* destination, if any			*/
	args.divert_rule = 0;	/* we do not support divert yet		*/
	args.rule = *rule;	/* matching rule to restart		*/
	args.next_hop = NULL;	/* we do not support forward yet	*/
	args.eh = &save_eh;	/* MAC header for bridged/MAC packets	*/
	i = ip_fw_chk_ptr(&args);
	m = args.m;
	if (m != NULL) {
		/*
		 * Restore Ethernet header, as needed, in case the
		 * mbuf chain was replaced by ipfw.
		 */
		M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
		if (m == NULL) {
			*m0 = m;
			return 0;
		}
		if (eh != mtod(m, struct ether_header *))
			bcopy(&save_eh, mtod(m, struct ether_header *),
				ETHER_HDR_LEN);
	}
	*m0 = m;
	*rule = args.rule;

	if ( (i & IP_FW_PORT_DENY_FLAG) || m == NULL) /* drop */
		return 0;

	if (i == 0) /* a PASS rule.  */
		return 1;

	if (DUMMYNET_LOADED && (i & IP_FW_PORT_DYNT_FLAG)) {
		/*
		 * Pass the pkt to dummynet, which consumes it.
		 * If shared, make a copy and keep the original.
		 */
		if (shared) {
			m = m_copypacket(m, M_DONTWAIT);
			if (m == NULL)
				return 0;
		} else {
			/*
			 * Pass the original to dummynet and
			 * nothing back to the caller
			 */
			*m0 = NULL ;
		}
		ip_dn_io_ptr(m, (i & 0xffff),
			dst ? DN_TO_ETH_OUT: DN_TO_ETH_DEMUX, &args);
		return 0;
	}
	/*
	 * XXX at some point add support for divert/forward actions.
	 * If none of the above matches, we have to drop the pkt.
	 */
	return 0;
}

/*
 * Process a received Ethernet packet; the packet is in the
 * mbuf chain m with the ethernet header at the front.
 */
static void
ether_input(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	u_short etype;

	/*
	 * Do consistency checks to verify assumptions
	 * made by code past this point.
	 */
	if ((m->m_flags & M_PKTHDR) == 0) {
		if_printf(ifp, "discard frame w/o packet header\n");
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}
	if (m->m_len < ETHER_HDR_LEN) {
		/* XXX maybe should pullup? */
		if_printf(ifp, "discard frame w/o leading ethernet "
				"header (len %u pkt len %u)\n",
				m->m_len, m->m_pkthdr.len);
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}
	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);
	if (m->m_pkthdr.len >
	    ETHER_MAX_FRAME(ifp, etype, m->m_flags & M_HASFCS)) {
		if_printf(ifp, "discard oversize frame "
				"(ether type %x flags %x len %u > max %lu)\n",
				etype, m->m_flags, m->m_pkthdr.len,
				ETHER_MAX_FRAME(ifp, etype,
						m->m_flags & M_HASFCS));
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}
	if (m->m_pkthdr.rcvif == NULL) {
		if_printf(ifp, "discard frame w/o interface pointer\n");
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}
#ifdef DIAGNOSTIC
	if (m->m_pkthdr.rcvif != ifp) {
		if_printf(ifp, "Warning, frame marked as received on %s\n",
			m->m_pkthdr.rcvif->if_xname);
	}
#endif

#ifdef MAC
	/*
	 * Tag the mbuf with an appropriate MAC label before any other
	 * consumers can get to it.
	 */
	mac_create_mbuf_from_ifnet(ifp, m);
#endif

	/*
	 * Give bpf a chance at the packet.
	 */
	BPF_MTAP(ifp, m);

	if (ifp->if_flags & IFF_MONITOR) {
		/*
		 * Interface marked for monitoring; discard packet.
		 */
		m_freem(m);
		return;
	}

	/* If the CRC is still on the packet, trim it off. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -ETHER_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}

	ifp->if_ibytes += m->m_pkthdr.len;

	/* Handle ng_ether(4) processing, if any */
	if (ng_ether_input_p != NULL) {
		(*ng_ether_input_p)(ifp, &m);
		if (m == NULL)
			return;
	}


	/* Check for bridging mode */
	if (BDG_ACTIVE(ifp) ) {
		struct ifnet *bif;

		/*
		 * Check with bridging code to see how the packet
		 * should be handled.  Possibilities are:
		 *
		 *    BDG_BCAST		broadcast
		 *    BDG_MCAST		multicast
		 *    BDG_LOCAL		for local address, don't forward
		 *    BDG_DROP		discard
		 *    ifp		forward only to specified interface(s)
		 *
		 * Non-local destinations are handled by passing the
		 * packet back to the bridge code.
		 */
		bif = bridge_in_ptr(ifp, eh);
		if (bif == BDG_DROP) {		/* discard packet */
			m_freem(m);
			return;
		}
		if (bif != BDG_LOCAL) {		/* non-local, forward */
			m = bdg_forward_ptr(m, bif);
			/*
			 * The bridge may consume the packet if it's not
			 * supposed to be passed up or if a problem occurred
			 * while doing its job.  This is reflected by it
			 * returning a NULL mbuf pointer.
			 */
			if (m == NULL) {
				if (bif == BDG_BCAST || bif == BDG_MCAST)
					if_printf(ifp,
						"bridge dropped %s packet\n",
						bif == BDG_BCAST ? "broadcast" :
								   "multicast");
				return;
			}
			/*
			 * But in some cases the bridge may return the
			 * packet for us to free; sigh.
			 */
			if (bif != BDG_BCAST && bif != BDG_MCAST) {
				m_freem(m);
				return;
			}
		}
	}

	ether_demux(ifp, m);
	/* First chunk of an mbuf contains good entropy */
	if (harvest.ethernet)
		random_harvest(m, 16, 3, 0, RANDOM_NET);
}

/*
 * Upper layer processing for a received Ethernet packet.
 */
void
ether_demux(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	int isr;
	u_short ether_type;
#if defined(NETATALK)
	struct llc *l;
#endif
	struct ip_fw *rule = NULL;

	/* Extract info from dummynet tag, ignore others */
	for (;m->m_type == MT_TAG; m = m->m_next)
		if (m->m_flags == PACKET_TAG_DUMMYNET) {
			rule = ((struct dn_pkt *)m)->rule;
			ifp = m->m_next->m_pkthdr.rcvif;
		}

	KASSERT(ifp != NULL, ("ether_demux: NULL interface pointer"));

	eh = mtod(m, struct ether_header *);

	if (rule)	/* packet was already bridged */
		goto post_stats;

	if (!(BDG_ACTIVE(ifp))) {
		/*
		 * Discard packet if upper layers shouldn't see it because it
		 * was unicast to a different Ethernet address. If the driver
		 * is working properly, then this situation can only happen
		 * when the interface is in promiscuous mode.
		 */
		if ((ifp->if_flags & IFF_PROMISC) != 0
		    && (eh->ether_dhost[0] & 1) == 0
		    && bcmp(eh->ether_dhost,
		      IFP2AC(ifp)->ac_enaddr, ETHER_ADDR_LEN) != 0
		    && (ifp->if_flags & IFF_PPROMISC) == 0) {
			    m_freem(m);
			    return;
		}
	}

	/* Discard packet if interface is not up */
	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		if (bcmp(etherbroadcastaddr, eh->ether_dhost,
		    sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	}
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

post_stats:
	if (IPFW_LOADED && ether_ipfw != 0) {
		if (ether_ipfw_chk(&m, NULL, &rule, 0) == 0) {
			if (m)
				m_freem(m);
			return;
		}
	}

	/*
	 * If VLANs are configured on the interface, check to
	 * see if the device performed the decapsulation and
	 * provided us with the tag.
	 */
	if (ifp->if_nvlans &&
	    m_tag_locate(m, MTAG_VLAN, MTAG_VLAN_TAG, NULL) != NULL) {
		/*
		 * vlan_input() will either recursively call ether_input()
		 * or drop the packet.
		 */
		KASSERT(vlan_input_p != NULL,("ether_input: VLAN not loaded!"));
		(*vlan_input_p)(ifp, m);
		return;
	}

	ether_type = ntohs(eh->ether_type);

	/*
	 * Handle protocols that expect to have the Ethernet header
	 * (and possibly FCS) intact.
	 */
	switch (ether_type) {
	case ETHERTYPE_VLAN:
		if (ifp->if_nvlans != 0) {
			KASSERT(vlan_input_p,("ether_input: VLAN not loaded!"));
			(*vlan_input_p)(ifp, m);
		} else {
			ifp->if_noproto++;
			m_freem(m);
		}
		return;
	}

	/* Strip off Ethernet header. */
	m_adj(m, ETHER_HDR_LEN);

	/* If the CRC is still on the packet, trim it off. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -ETHER_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}

	switch (ether_type) {
#ifdef INET
	case ETHERTYPE_IP:
		if (ip_fastforward(m))
			return;
		isr = NETISR_IP;
		break;

	case ETHERTYPE_ARP:
		if (ifp->if_flags & IFF_NOARP) {
			/* Discard packet if ARP is disabled on interface */
			m_freem(m);
			return;
		}
		isr = NETISR_ARP;
		break;
#endif
#ifdef IPX
	case ETHERTYPE_IPX:
		if (ef_inputp && ef_inputp(ifp, eh, m) == 0)
			return;
		isr = NETISR_IPX;
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		isr = NETISR_IPV6;
		break;
#endif
#ifdef NETATALK
	case ETHERTYPE_AT:
		isr = NETISR_ATALK1;
		break;
	case ETHERTYPE_AARP:
		isr = NETISR_AARP;
		break;
#endif /* NETATALK */
	default:
#ifdef IPX
		if (ef_inputp && ef_inputp(ifp, eh, m) == 0)
			return;
#endif /* IPX */
#if defined(NETATALK)
		if (ether_type > ETHERMTU)
			goto discard;
		l = mtod(m, struct llc *);
		if (l->llc_dsap == LLC_SNAP_LSAP &&
		    l->llc_ssap == LLC_SNAP_LSAP &&
		    l->llc_control == LLC_UI) {
			if (Bcmp(&(l->llc_snap_org_code)[0], at_org_code,
			    sizeof(at_org_code)) == 0 &&
			    ntohs(l->llc_snap_ether_type) == ETHERTYPE_AT) {
				m_adj(m, LLC_SNAPFRAMELEN);
				isr = NETISR_ATALK2;
				break;
			}
			if (Bcmp(&(l->llc_snap_org_code)[0], aarp_org_code,
			    sizeof(aarp_org_code)) == 0 &&
			    ntohs(l->llc_snap_ether_type) == ETHERTYPE_AARP) {
				m_adj(m, LLC_SNAPFRAMELEN);
				isr = NETISR_AARP;
				break;
			}
		}
#endif /* NETATALK */
		goto discard;
	}
	netisr_dispatch(isr, m);
	return;

discard:
	/*
	 * Packet is to be discarded.  If netgraph is present,
	 * hand the packet to it for last chance processing;
	 * otherwise dispose of it.
	 */
	if (ng_ether_input_orphan_p != NULL) {
		/*
		 * Put back the ethernet header so netgraph has a
		 * consistent view of inbound packets.
		 */
		M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
		(*ng_ether_input_orphan_p)(ifp, m);
		return;
	}
	m_freem(m);
}

/*
 * Convert Ethernet address to printable (loggable) representation.
 * This routine is for compatibility; it's better to just use
 *
 *	printf("%6D", <pointer to address>, ":");
 *
 * since there's no static buffer involved.
 */
char *
ether_sprintf(const u_char *ap)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof (etherbuf), "%6D", ap, ":");
	return (etherbuf);
}

/*
 * Perform common duties while attaching to interface list
 */
void
ether_ifattach(struct ifnet *ifp, const u_int8_t *llc)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifp->if_type = IFT_ETHER;
	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	if_attach(ifp);
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;
	ifp->if_input = ether_input;
	ifp->if_resolvemulti = ether_resolvemulti;
	if (ifp->if_baudrate == 0)
		ifp->if_baudrate = IF_Mbps(10);		/* just a default */
	ifp->if_broadcastaddr = etherbroadcastaddr;

	ifa = ifaddr_byindex(ifp->if_index);
	KASSERT(ifa != NULL, ("%s: no lladdr!\n", __func__));
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ifp->if_addrlen;
	bcopy(llc, LLADDR(sdl), ifp->if_addrlen);
	/*
	 * XXX: This doesn't belong here; we do it until
	 * XXX:  all drivers are cleaned up
	 */
	if (llc != IFP2AC(ifp)->ac_enaddr)
		bcopy(llc, IFP2AC(ifp)->ac_enaddr, ifp->if_addrlen);

	bpfattach(ifp, DLT_EN10MB, ETHER_HDR_LEN);
	if (ng_ether_attach_p != NULL)
		(*ng_ether_attach_p)(ifp);
	if (BDG_LOADED)
		bdgtakeifaces_ptr();
}

/*
 * Perform common duties while detaching an Ethernet interface
 */
void
ether_ifdetach(struct ifnet *ifp)
{
	if (ng_ether_detach_p != NULL)
		(*ng_ether_detach_p)(ifp);
	bpfdetach(ifp);
	if_detach(ifp);
	if (BDG_LOADED)
		bdgtakeifaces_ptr();
}

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, IFT_ETHER, ether, CTLFLAG_RW, 0, "Ethernet");
SYSCTL_INT(_net_link_ether, OID_AUTO, ipfw, CTLFLAG_RW,
	    &ether_ipfw,0,"Pass ether pkts through firewall");

int
ether_ioctl(struct ifnet *ifp, int command, caddr_t data)
{
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0;

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ifp->if_init(ifp->if_softc);	/* before arpwhohas */
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef IPX
		/*
		 * XXX - This code is probably wrong
		 */
		case AF_IPX:
			{
			struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);
			struct arpcom *ac = IFP2AC(ifp);

			if (ipx_nullhost(*ina))
				ina->x_host =
				    *(union ipx_host *)
				    ac->ac_enaddr;
			else {
				bcopy((caddr_t) ina->x_host.c_host,
				      (caddr_t) ac->ac_enaddr,
				      sizeof(ac->ac_enaddr));
			}

			/*
			 * Set new address
			 */
			ifp->if_init(ifp->if_softc);
			break;
			}
#endif
		default:
			ifp->if_init(ifp->if_softc);
			break;
		}
		break;

	case SIOCGIFADDR:
		{
			struct sockaddr *sa;

			sa = (struct sockaddr *) & ifr->ifr_data;
			bcopy(IFP2AC(ifp)->ac_enaddr,
			      (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
		}
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;
	default:
		error = EINVAL;			/* XXX netbsd has ENOTTY??? */
		break;
	}
	return (error);
}

static int
ether_resolvemulti(struct ifnet *ifp, struct sockaddr **llsa,
	struct sockaddr *sa)
{
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sin;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
	u_char *e_addr;

	switch(sa->sa_family) {
	case AF_LINK:
		/*
		 * No mapping needed. Just check that it's a valid MC address.
		 */
		sdl = (struct sockaddr_dl *)sa;
		e_addr = LLADDR(sdl);
		if ((e_addr[0] & 1) != 1)
			return EADDRNOTAVAIL;
		*llsa = 0;
		return 0;

#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
			return EADDRNOTAVAIL;
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_WAITOK|M_ZERO);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IP_MULTICAST(&sin->sin_addr, e_addr);
		*llsa = (struct sockaddr *)sdl;
		return 0;
#endif
#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			/*
			 * An IP6 address of 0 means listen to all
			 * of the Ethernet multicast address used for IP6.
			 * (This is used for multicast routers.)
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			*llsa = 0;
			return 0;
		}
		if (!IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
			return EADDRNOTAVAIL;
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_WAITOK|M_ZERO);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IPV6_MULTICAST(&sin6->sin6_addr, e_addr);
		*llsa = (struct sockaddr *)sdl;
		return 0;
#endif

	default:
		/*
		 * Well, the text isn't quite right, but it's the name
		 * that counts...
		 */
		return EAFNOSUPPORT;
	}
}

#ifdef ALTQ
/*
 * find the size of ethernet header, and call classifier
 */
void
altq_etherclassify(ifq, m, pktattr)
	struct ifaltq *ifq;
	struct mbuf *m;
	struct altq_pktattr *pktattr;
{
	struct ether_header *eh;
	u_short	ether_type;
	int	hlen, af, hdrsize;
	caddr_t hdr;

	hlen = sizeof(struct ether_header);
	eh = mtod(m, struct ether_header *);

	ether_type = ntohs(eh->ether_type);
	if (ether_type < ETHERMTU) {
		/* ick! LLC/SNAP */
		struct llc *llc = (struct llc *)(eh + 1);
		hlen += 8;

		if (m->m_len < hlen ||
		    llc->llc_dsap != LLC_SNAP_LSAP ||
		    llc->llc_ssap != LLC_SNAP_LSAP ||
		    llc->llc_control != LLC_UI)
			goto bad;  /* not snap! */

		ether_type = ntohs(llc->llc_un.type_snap.ether_type);
	}

	if (ether_type == ETHERTYPE_IP) {
		af = AF_INET;
		hdrsize = 20;  /* sizeof(struct ip) */
#ifdef INET6
	} else if (ether_type == ETHERTYPE_IPV6) {
		af = AF_INET6;
		hdrsize = 40;  /* sizeof(struct ip6_hdr) */
#endif
	} else
		goto bad;

	while (m->m_len <= hlen) {
		hlen -= m->m_len;
		m = m->m_next;
	}
	hdr = m->m_data + hlen;
	if (m->m_len < hlen + hdrsize) {
		/*
		 * ip header is not in a single mbuf.  this should not
		 * happen in the current code.
		 * (todo: use m_pulldown in the future)
		 */
		goto bad;
	}
	m->m_data += hlen;
	m->m_len -= hlen;
	if (ALTQ_NEEDS_CLASSIFY(ifq))
		pktattr->pattr_class =
		    (*ifq->altq_classify)(ifq->altq_clfier, m, af);
	m->m_data -= hlen;
	m->m_len += hlen;

	pktattr->pattr_af = af;
	pktattr->pattr_hdr = hdr;
	return;

bad:
	pktattr->pattr_class = NULL;
	pktattr->pattr_hdr = NULL;
	pktattr->pattr_af = AF_UNSPEC;
}
#endif /* ALTQ */

static moduledata_t ether_mod = {
	"ether",
	NULL,
	0
};

DECLARE_MODULE(ether, ether_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(ether, 1);
