/*	$NetBSD: ofnet.c,v 1.24.8.1 2003/01/27 04:57:40 jmc Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ofnet.c,v 1.24.8.1 2003/01/27 04:57:40 jmc Exp $");

#include "ofnet.h"
#include "opt_inet.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <dev/ofw/openfirm.h>

#if NIPKDB_OFN > 0
#include <ipkdb/ipkdb.h>
#include <machine/ipkdb.h>

struct cfattach ipkdb_ofn_ca = {
	0, ipkdb_probe, ipkdb_attach
};

static struct ipkdb_if *kifp;
static struct ofnet_softc *ipkdb_of;

static int ipkdbprobe (struct cfdata *, void *);
#endif

struct ofnet_softc {
	struct device sc_dev;
	int sc_phandle;
	int sc_ihandle;
	struct ethercom sc_ethercom;
	struct callout sc_callout;
};

static int ofnet_match (struct device *, struct cfdata *, void *);
static void ofnet_attach (struct device *, struct device *, void *);

struct cfattach ofnet_ca = {
	sizeof(struct ofnet_softc), ofnet_match, ofnet_attach
};

static void ofnet_read (struct ofnet_softc *);
static void ofnet_timer (void *);
static void ofnet_init (struct ofnet_softc *);
static void ofnet_stop (struct ofnet_softc *);

static void ofnet_start (struct ifnet *);
static int ofnet_ioctl (struct ifnet *, u_long, caddr_t);
static void ofnet_watchdog (struct ifnet *);

static int
ofnet_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct ofbus_attach_args *oba = aux;
	char type[32];
	int l;
	
#if NIPKDB_OFN > 0
	if (!parent)
		return ipkdbprobe(match, aux);
#endif
	if (strcmp(oba->oba_busname, "ofw"))
		return (0);
	if ((l = OF_getprop(oba->oba_phandle, "device_type", type,
	    sizeof type - 1)) < 0)
		return 0;
	if (l >= sizeof type)
		return 0;
	type[l] = 0;
	if (strcmp(type, "network"))
		return 0;
	return 1;
}

static void
ofnet_attach(struct device *parent, struct device *self, void *aux)
{
	struct ofnet_softc *of = (void *)self;
	struct ifnet *ifp = &of->sc_ethercom.ec_if;
	struct ofbus_attach_args *oba = aux;
	char path[256];
	int l;
	u_int8_t myaddr[ETHER_ADDR_LEN];
	
	of->sc_phandle = oba->oba_phandle;
#if NIPKDB_OFN > 0
	if (kifp &&
	    kifp->unit - 1 == of->sc_dev.dv_unit &&
	    OF_instance_to_package(kifp->port) == oba->oba_phandle)  {
		ipkdb_of = of;
		of->sc_ihandle = kifp->port;
	} else
#endif
	if ((l = OF_package_to_path(oba->oba_phandle, path,
	    sizeof path - 1)) < 0 ||
	    l >= sizeof path ||
	    (path[l] = 0, !(of->sc_ihandle = OF_open(path))))
		panic("ofnet_attach: unable to open");
	if (OF_getprop(oba->oba_phandle, "mac-address", myaddr,
	    sizeof myaddr) < 0)
		panic("ofnet_attach: no mac-address");
	printf(": address %s\n", ether_sprintf(myaddr));

	callout_init(&of->sc_callout);

	bcopy(of->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = of;
	ifp->if_start = ofnet_start;
	ifp->if_ioctl = ofnet_ioctl;
	ifp->if_watchdog = ofnet_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	ether_ifattach(ifp, myaddr);

#ifdef __BROKEN_DK_ESTABLISH
	dk_establish(0, self);					/* XXX */
#endif
}

static char buf[ETHERMTU + sizeof(struct ether_header)];

static void
ofnet_read(struct ofnet_softc *of)
{
	struct ifnet *ifp = &of->sc_ethercom.ec_if;
	struct mbuf *m, **mp, *head;
	int l, len;
	char *bufp;

#if NIPKDB_OFN > 0
	ipkdbrint(kifp, ifp);
#endif	
	while (1) {
		if ((len = OF_read(of->sc_ihandle, buf, sizeof buf)) < 0) {
			if (len == -2 || len == 0)
				return;
			ifp->if_ierrors++;
			continue;
		}
		if (len < sizeof(struct ether_header)) {
			ifp->if_ierrors++;
			continue;
		}
		bufp = buf;

		/* Allocate a header mbuf */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0) {
			ifp->if_ierrors++;
			continue;
		}
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;

		/*
		 * We don't know if the interface included the FCS
		 * or not.  For now, assume that it did if we got
		 * a packet length that looks like it could include
		 * the FCS.
 		 *
		 * XXX Yuck.
		 */

		if (len > (ETHER_MAX_LEN - ETHER_CRC_LEN))
			m->m_flags |= M_HASFCS;

		l = MHLEN;
		head = 0;
		mp = &head;
		
		while (len > 0) {
			if (head) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == 0) {
					ifp->if_ierrors++;
					m_freem(head);
					head = 0;
					break;
				}
				l = MLEN;
			}
			if (len >= MINCLSIZE) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					ifp->if_ierrors++;
					m_free(m);
					m_freem(head);
					head = 0;
					break;
				}
				l = MCLBYTES;
			}

			/*
			 * Make sure the data after the Ethernet header
			 * is aligned.
			 *
			 * XXX Assumes the device is an ethernet, but
			 * XXX then so does other code in this driver.
			 */
			if (head == NULL) {
				caddr_t newdata = (caddr_t)ALIGN(m->m_data +
				      sizeof(struct ether_header)) -
				    sizeof(struct ether_header);
				l -= newdata - m->m_data;
				m->m_data = newdata;
			}

			m->m_len = l = min(len, l);
			bcopy(bufp, mtod(m, char *), l);
			bufp += l;
			len -= l;
			*mp = m;
			mp = &m->m_next;
		}
		if (head == 0)
			continue;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
		ifp->if_ipackets++;
		(*ifp->if_input)(ifp, head);
	}
}

static void
ofnet_timer(arg)
	void *arg;
{
	struct ofnet_softc *of = arg;

	ofnet_read(of);
	callout_reset(&of->sc_callout, 1, ofnet_timer, of);
}

static void
ofnet_init(struct ofnet_softc *of)
{
	struct ifnet *ifp = &of->sc_ethercom.ec_if;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	ifp->if_flags |= IFF_RUNNING;
	/* Start reading from interface */
	ofnet_timer(of);
	/* Attempt to start output */
	ofnet_start(ifp);
}

static void
ofnet_stop(struct ofnet_softc *of)
{
	callout_stop(&of->sc_callout);
	of->sc_ethercom.ec_if.if_flags &= ~IFF_RUNNING;
}

static void
ofnet_start(struct ifnet *ifp)
{
	struct ofnet_softc *of = ifp->if_softc;
	struct mbuf *m, *m0;
	char *bufp;
	int len;
	
	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	for (;;) {
		/* First try reading any packets */
		ofnet_read(of);
		
		/* Now get the first packet on the queue */
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (!m0)
			return;
		
		if (!(m0->m_flags & M_PKTHDR))
			panic("ofnet_start: no header mbuf");
		len = m0->m_pkthdr.len;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif

		if (len > ETHERMTU + sizeof(struct ether_header)) {
			/* packet too large, toss it */
			ifp->if_oerrors++;
			m_freem(m0);
			continue;
		}

		for (bufp = buf; (m = m0) != NULL;) {
			bcopy(mtod(m, char *), bufp, m->m_len);
			bufp += m->m_len;
			MFREE(m, m0);
		}

		/*
		 * We don't know if the interface will auto-pad for
		 * us, so make sure it's at least as large as a
		 * minimum size Ethernet packet.
		 */

		if (len < (ETHER_MIN_LEN - ETHER_CRC_LEN)) {
			memset(bufp, 0, ETHER_MIN_LEN - ETHER_CRC_LEN - len);
			bufp += ETHER_MIN_LEN - ETHER_CRC_LEN - len;
		} else
			len = bufp - buf;

		if (OF_write(of->sc_ihandle, buf, len) != len)
			ifp->if_oerrors++;
		else
			ifp->if_opackets++;
	}
}

static int
ofnet_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ofnet_softc *of = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	/* struct ifreq *ifr = (struct ifreq *)data; */
	int error = 0;
	
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		
		switch (ifa->ifa_addr->sa_family) {
#ifdef	INET
		case AF_INET:
			arp_ifinit(ifp, ifa);
			break;
#endif
		default:
			break;
		}
		ofnet_init(of);
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/* If interface is down, but running, stop it. */
			ofnet_stop(of);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/* If interface is up, but not running, start it. */
			ofnet_init(of);
		} else {
			/* Other flags are ignored. */
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

static void
ofnet_watchdog(struct ifnet *ifp)
{
	struct ofnet_softc *of = ifp->if_softc;
	
	log(LOG_ERR, "%s: device timeout\n", of->sc_dev.dv_xname);
	ifp->if_oerrors++;
	ofnet_stop(of);
	ofnet_init(of);
}

#if NIPKDB_OFN > 0
static void
ipkdbofstart(struct ipkdb_if *kip)
{
	int unit = kip->unit - 1;
	
	if (ipkdb_of)
		ipkdbattach(kip, &ipkdb_of->sc_ethercom);
}

static void
ipkdbofleave(struct ipkdb_if *kip)
{
}

static int
ipkdbofrcv(struct ipkdb_if *kip, u_char *buf, int poll)
{
	int l;
	
	do {
		l = OF_read(kip->port, buf, ETHERMTU);
		if (l < 0)
			l = 0;
	} while (!poll && !l);
	return l;
}

static void
ipkdbofsend(struct ipkdb_if *kip, u_char *buf, int l)
{
	OF_write(kip->port, buf, l);
}

static int
ipkdbprobe(struct cfdata *match, void *aux)
{
	struct ipkdb_if *kip = aux;
	static char name[256];
	int len;
	int phandle;
	
	kip->unit = match->cf_unit + 1;

	if (!(kip->port = OF_open("net")))
		return -1;
	if ((len = OF_instance_to_path(kip->port, name, sizeof name - 1)) < 0 ||
	    len >= sizeof name)
		return -1;
	name[len] = 0;
	if ((phandle = OF_instance_to_package(kip->port)) == -1)
		return -1;
	if (OF_getprop(phandle, "mac-address", kip->myenetaddr,
	    sizeof kip->myenetaddr) < 0)
		return -1;
	
	kip->flags |= IPKDB_MYHW;
	kip->name = name;
	kip->start = ipkdbofstart;
	kip->leave = ipkdbofleave;
	kip->receive = ipkdbofrcv;
	kip->send = ipkdbofsend;

	kifp = kip;
	
	return 0;
}
#endif
