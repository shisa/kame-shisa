/*      $NetBSD: if_atmsubr.c,v 1.31 2001/11/12 23:49:36 lukem Exp $       */

/*
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and 
 *	Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * if_atmsubr.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_atmsubr.c,v 1.31 2001/11/12 23:49:36 lukem Exp $");

#include "opt_inet.h"
#include "opt_gateway.h"
#include "opt_natm.h"

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_atm.h>
#include <net/ethertypes.h> /* XXX: for ETHERTYPE_* */

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_atm.h>

#if defined(INET) || defined(INET6)
#include <netinet/in_var.h>
#endif
#ifdef NATM
#include <netnatm/natm.h>
#endif

#define senderr(e) { error = (e); goto bad;}

/*
 * atm_output: ATM output routine
 *   inputs:
 *     "ifp" = ATM interface to output to
 *     "m0" = the packet to output
 *     "dst" = the sockaddr to send to (either IP addr, or raw VPI/VCI)
 *     "rt0" = the route to use
 *   returns: error code   [0 == ok]
 *
 *   note: special semantic: if (dst == NULL) then we assume "m" already
 *		has an atm_pseudohdr on it and just send it directly.
 *		[for native mode ATM output]   if dst is null, then
 *		rt0 must also be NULL.
 */

int
atm_output(ifp, m0, dst, rt0)
	struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	u_int16_t etype = 0;			/* if using LLC/SNAP */
	int s, error = 0, sz, len;
	struct atm_pseudohdr atmdst, *ad;
	struct mbuf *m = m0;
	struct rtentry *rt;
	struct atmllc *atmllc;
	struct atmllc *llc_hdr = NULL;
	u_int32_t atm_flags;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);

	/*
	 * If the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m,
	     (dst != NULL ? dst->sa_family : AF_UNSPEC), &pktattr);

	/*
	 * check route
	 */
	if ((rt = rt0) != NULL) {

		if ((rt->rt_flags & RTF_UP) == 0) { /* route went down! */
			if ((rt0 = rt = RTALLOC1(dst, 0)) != NULL)
				rt->rt_refcnt--;
			else 
				senderr(EHOSTUNREACH);
		}

		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = RTALLOC1(rt->rt_gateway, 0);
				if ((rt = rt->rt_gwroute) == 0)
					senderr(EHOSTUNREACH);
			}
		}

		/* XXX: put RTF_REJECT code here if doing ATMARP */

	}

	/*
	 * check for non-native ATM traffic   (dst != NULL)
	 */
	if (dst) {
		switch (dst->sa_family) {
#ifdef INET
		case AF_INET:
#endif
#ifdef INET6
		case AF_INET6:
#endif
#if defined(INET) || defined(INET6)
			if (dst->sa_family == AF_INET)
				etype = ETHERTYPE_IP;
			else
				etype = ETHERTYPE_IPV6;
# ifdef ATM_PVCEXT
			if (ifp->if_flags & IFF_POINTOPOINT) {
				/* pvc subinterface */
				struct pvcsif *pvcsif = (struct pvcsif *)ifp;
				atmdst = pvcsif->sif_aph;
				break;
			}
# endif
			if (!atmresolve(rt, m, dst, &atmdst)) {
				m = NULL; 
				/* XXX: atmresolve already free'd it */
				senderr(EHOSTUNREACH);
				/* XXX: put ATMARP stuff here */
				/* XXX: watch who frees m on failure */
			}
			break;
#endif

		case AF_UNSPEC:
			/*
			 * XXX: bpfwrite or output from a pvc shadow if.
			 * assuming dst contains 12 bytes (atm pseudo
			 * header (4) + LLC/SNAP (8))
			 */
			if (dst->sa_len + 2 < sizeof(atmdst) + sizeof(*llc_hdr))
				senderr(EINVAL);
			bcopy(dst->sa_data, &atmdst, sizeof(atmdst));
			llc_hdr = (struct atmllc *)(dst->sa_data + sizeof(atmdst));
			break;
			
		default:
#if defined(__NetBSD__) || defined(__OpenBSD__)
			printf("%s: can't handle af%d\n", ifp->if_xname, 
			    dst->sa_family);
#elif defined(__FreeBSD__) || defined(__bsdi__)
			printf("%s%d: can't handle af%d\n", ifp->if_name, 
			    ifp->if_unit, dst->sa_family);
#endif
			senderr(EAFNOSUPPORT);
		}

		/*
		 * must add atm_pseudohdr to data
		 */
		sz = sizeof(atmdst);
		atm_flags = ATM_PH_FLAGS(&atmdst);
		if (atm_flags & ATM_PH_LLCSNAP) sz += 8; /* sizeof snap == 8 */
		M_PREPEND(m, sz, M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		ad = mtod(m, struct atm_pseudohdr *);
		*ad = atmdst;
		if (atm_flags & ATM_PH_LLCSNAP) {
			atmllc = (struct atmllc *)(ad + 1);
			bcopy(ATMLLC_HDR, atmllc->llchdr, 
						sizeof(atmllc->llchdr));
			ATM_LLC_SETTYPE(atmllc, etype); 
		}
	}

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */

	len = m->m_pkthdr.len;
	s = splnet();
	IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
	if (error) {
		splx(s);
		return (error);
	}
	ifp->if_obytes += len;
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received ATM packet;
 * the packet is in the mbuf chain m.
 */
void
atm_input(ifp, ah, m, rxhand)
	struct ifnet *ifp;
	struct atm_pseudohdr *ah;
	struct mbuf *m;
	void *rxhand;
{
	struct ifqueue *inq;
	u_int16_t etype = ETHERTYPE_IP; /* default */
	int s;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	ifp->if_ibytes += m->m_pkthdr.len;

	if (rxhand) {
#ifdef NATM
	  struct natmpcb *npcb = rxhand;
	  s = splnet();			/* in case 2 atm cards @ diff lvls */
	  npcb->npcb_inq++;			/* count # in queue */
	  splx(s);
	  schednetisr(NETISR_NATM);
	  inq = &natmintrq;
	  m->m_pkthdr.rcvif = rxhand; /* XXX: overload */
#else
	  printf("atm_input: NATM detected but not configured in kernel\n");
	  m_freem(m);
	  return;
#endif
	} else {
	  /*
	   * handle LLC/SNAP header, if present
	   */
	  if (ATM_PH_FLAGS(ah) & ATM_PH_LLCSNAP) {
	    struct atmllc *alc;
	    if (m->m_len < sizeof(*alc) && (m = m_pullup(m, sizeof(*alc))) == 0)
		  return; /* failed */
	    alc = mtod(m, struct atmllc *);
	    if (bcmp(alc, ATMLLC_HDR, 6)) {
#if defined(__NetBSD__) || defined(__OpenBSD__)
	      printf("%s: recv'd invalid LLC/SNAP frame [vp=%d,vc=%d]\n",
		  ifp->if_xname, ATM_PH_VPI(ah), ATM_PH_VCI(ah));
#elif defined(__FreeBSD__) || defined(__bsdi__)
	      printf("%s%d: recv'd invalid LLC/SNAP frame [vp=%d,vc=%d]\n",
		  ifp->if_name, ifp->if_unit, ATM_PH_VPI(ah), ATM_PH_VCI(ah));
#endif
	      m_freem(m);
              return;
	    }
	    etype = ATM_LLC_TYPE(alc);
	    m_adj(m, sizeof(*alc));
	  }

#ifdef ATM_PVCEXT
	/* atm bridging support */
	if ((ifp->if_flags & (IFF_POINTOPOINT|IFF_LINK2)) ==
		(IFF_POINTOPOINT|IFF_LINK2)) {
		struct pvcsif *pvcsif = (struct pvcsif *)ifp;

		if (pvcsif->sif_fwdifp != NULL) {
			struct sockaddr dst;

			/* set address family to dummy dst addr */
			switch (etype) {
			case ETHERTYPE_IP:
				dst.sa_family = AF_INET;
				break;
			case ETHERTYPE_IPV6:
				dst.sa_family = AF_INET6;
				break;
			default:
				m_freem(m);
				return;
			}
			atm_output(pvcsif->sif_fwdifp, m, &dst, NULL);
			return;
		}
	}
#endif /* ATM_PVCEXT */

	  switch (etype) {
#ifdef INET
	  case ETHERTYPE_IP:
#ifdef GATEWAY
		  if (ipflow_fastforward(m))
			return;
#endif
		  schednetisr(NETISR_IP);
		  inq = &ipintrq;
		  break;
#endif /* INET */
#ifdef INET6
	  case ETHERTYPE_IPV6:
		  schednetisr(NETISR_IPV6);
		  inq = &ip6intrq;
		  break;
#endif
	  default:
	      m_freem(m);
	      return;
	  }
	}

	s = splnet();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
}

/*
 * Perform common duties while attaching to interface list
 */
void
atm_ifattach(ifp)
	struct ifnet *ifp;
{

	ifp->if_type = IFT_ATM;
	ifp->if_addrlen = 0;
	ifp->if_hdrlen = 0;
	ifp->if_dlt = DLT_ATM_RFC1483;
	ifp->if_mtu = ATMMTU;
	ifp->if_output = atm_output;
#if 0 /* XXX XXX XXX */
	ifp->if_input = atm_input;
#endif

	if_alloc_sadl(ifp);
	/* XXX Store LLADDR for ATMARP. */

#if NBPFILTER > 0
	bpfattach(ifp, DLT_ATM_RFC1483, sizeof(struct atmllc));
#endif
}

#ifdef ATM_PVCEXT

static int pvc_max_number = 16;	/* max number of PVCs */
static int pvc_number = 0;	/* pvc unit number */

struct ifnet *
pvcsif_alloc()
{
	struct pvcsif *pvcsif;

	if (pvc_number >= pvc_max_number)
		return (NULL);
	MALLOC(pvcsif, struct pvcsif *, sizeof(struct pvcsif),
	       M_DEVBUF, M_WAITOK);
	if (pvcsif == NULL)
		return (NULL);
	memset(pvcsif, 0, sizeof(struct pvcsif));

#ifdef __NetBSD__
	sprintf(pvcsif->sif_if.if_xname, "pvc%d", pvc_number++);
#else
	pvcsif->sif_if.if_name = "pvc";
	pvcsif->sif_if.if_unit = pvc_number++;
#endif
	return (&pvcsif->sif_if);
}

/*
 * pvc bridging support:
 * add or delete brigding between 2 pvc interfaces.
 */
int
pvc_set_fwd(if_name, if_name2, op)
	char *if_name, *if_name2;
	int op;		/* 0:delete 1:add 2:get */
{
	struct ifnet *ifp, *ifp2;
	struct pvcsif *pvcsif, *pvcsif2;

	if (strncmp(if_name, "pvc", 3) != 0
	    || (ifp = ifunit(if_name)) == NULL)
		return (EINVAL);
	pvcsif = (struct pvcsif *)ifp;

	if (op == 2) {
		/* get bridging info */
		if ((ifp2 = pvcsif->sif_fwdifp) == NULL)
			*if_name2 = '\0';
		else
#ifdef __NetBSD__
			sprintf(if_name2, "%s", ifp2->if_xname);
#else
			sprintf(if_name2, "%s%d",
				ifp2->if_name, ifp2->if_unit);
#endif
		return (0);
	}

	if (strncmp(if_name2, "pvc", 3) != 0
	    || (ifp2 = ifunit(if_name2)) == NULL)
		return (EINVAL);
	pvcsif2 = (struct pvcsif *)ifp2;

	if (op) {
		/* set up bridging */
		pvcsif->sif_fwdifp = ifp2;
		pvcsif2->sif_fwdifp = ifp;
		ifp->if_flags |= IFF_LINK2;	/* use IFF_LINK2 to show */
		ifp2->if_flags |= IFF_LINK2;	/* bridging is enabled   */
	}
	else {
		/* delete bridging */
		if (pvcsif->sif_fwdifp != ifp2 || pvcsif2->sif_fwdifp != ifp)
			return (EINVAL);
		pvcsif->sif_fwdifp = NULL;
		pvcsif2->sif_fwdifp = NULL;
		ifp->if_flags &= ~IFF_LINK2;
		ifp2->if_flags &= ~IFF_LINK2;
	}
	return (0);
}
#endif /* ATM_PVCEXT */
