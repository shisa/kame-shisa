/*	$OpenBSD: if_pfsync.c,v 1.6 2003/06/21 09:07:01 djm Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __FreeBSD__
#include "opt_inet.h"
#include "opt_inet6.h"
#endif
#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#ifdef __FreeBSD__
#include "bpf.h"
#define NBPFILTER	NBPF
#else
#include "bpfilter.h"
#endif
#include "pfsync.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#ifndef __FreeBSD__
#include <sys/ioctl.h>
#else
#include <sys/kernel.h>
#include <sys/sockio.h>
#endif
#ifdef __OpenBSD__
#include <sys/timeout.h>
#else
#include <sys/callout.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/bpf.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/nd6.h>
#endif /* INET6 */

#include <net/pfvar.h>
#include <net/if_pfsync.h>

#include <net/net_osdep.h>

#define PFSYNC_MINMTU	\
    (sizeof(struct pfsync_header) + sizeof(struct pf_state))

#ifdef PFSYNCDEBUG
#define DPRINTF(x)    do { if (pfsyncdebug) printf x ; } while (0)
int pfsyncdebug;
#else
#define DPRINTF(x)
#endif

struct pfsync_softc pfsyncif;

#ifdef __FreeBSD__
void	pfsyncattach(void *);
PSEUDO_SET(pfsyncattach, if_pfsync);
#else
void	pfsyncattach(int);
#endif
void	pfsync_setmtu(struct pfsync_softc *sc, int);
int	pfsyncoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	       struct rtentry *);
int	pfsyncioctl(struct ifnet *, u_long, caddr_t);
void	pfsyncstart(struct ifnet *);

struct mbuf *pfsync_get_mbuf(struct pfsync_softc *sc, u_int8_t action);
int	pfsync_sendout(struct pfsync_softc *sc);
void	pfsync_timeout(void *v);

#ifndef __FreeBSD__
extern int ifqmaxlen;
#endif

#ifdef __FreeBSD__
void
pfsyncattach(void *pfsync)
#else
void
pfsyncattach(int npfsync)
#endif
{
	struct ifnet *ifp;

	pfsyncif.sc_mbuf = NULL;
	pfsyncif.sc_ptr = NULL;
	pfsyncif.sc_count = 8;
	ifp = &pfsyncif.sc_if;
#ifndef __FreeBSD__
	strlcpy(ifp->if_xname, "pfsync0", sizeof ifp->if_xname);
#elif defined(__FreeBSD__) && __FreeBSD_version >= 502000
	if_initname(ifp, "pfsync", 0);
#else
	ifp->if_name = "pfsync";
	ifp->if_unit = 0;
#endif
	ifp->if_softc = &pfsyncif;
	ifp->if_ioctl = pfsyncioctl;
	ifp->if_output = pfsyncoutput;
	ifp->if_start = pfsyncstart;
	ifp->if_type = IFT_PFSYNC;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = PFSYNC_HDRLEN;
#ifndef __FreeBSD__
	ifp->if_baudrate = IF_Mbps(100);
#else
	ifp->if_baudrate = 10000000;
#endif
	pfsync_setmtu(&pfsyncif, MCLBYTES);
#ifdef __OpenBSD__
	timeout_set(&pfsyncif.sc_tmo, pfsync_timeout, &pfsyncif);
#elif defined(__FreeBSD__) && __FreeBSD__ >= 5
	callout_init(&pfsyncif.sc_tmo, 0);
#else
	callout_init(&pfsyncif.sc_tmo);
#endif
	if_attach(ifp);
#if defined(__NetBSD__) || defined(__OpenBSD__)
	ifp->if_addrlen = 0;
	if_alloc_sadl(ifp);
#endif

#if NBPFILTER > 0
#ifndef HAVE_NEW_BPFATTACH
	bpfattach(&pfsyncif.sc_if.if_bpf, ifp, DLT_PFSYNC, PFSYNC_HDRLEN);
#else
	bpfattach(ifp, DLT_PFSYNC, PFSYNC_HDRLEN);
#endif
#endif
}

/*
 * Start output on the pfsync interface.
 */
void
pfsyncstart(struct ifnet *ifp)
{
	struct mbuf *m;
	int s;

	for (;;) {
#ifdef __OpenBSD__
		s = splimp();
#else
		s = splnet();
#endif
#if (defined(__FreeBSD__) && __FreeBSD_version >= 500000)
		IFQ_LOCK(&ifp->if_snd);
#else
		IF_DROP(&ifp->if_snd);
#endif
		IF_DEQUEUE(&ifp->if_snd, m);
#if (defined(__FreeBSD__) && __FreeBSD_version >= 500000)
		IFQ_UNLOCK(&ifp->if_snd);
#endif
		splx(s);

		if (m == NULL)
			return;
		else
			m_freem(m);
	}
}

int
pfsyncoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
int
pfsyncioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct pfsync_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s;

	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < PFSYNC_MINMTU)
			return (EINVAL);
		if (ifr->ifr_mtu > MCLBYTES)
			ifr->ifr_mtu = MCLBYTES;
		s = splnet();
		if (ifr->ifr_mtu < ifp->if_mtu)
			pfsync_sendout(sc);
		pfsync_setmtu(sc, ifr->ifr_mtu);
		splx(s);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

void
pfsync_setmtu(sc, mtu)
	struct pfsync_softc *sc;
	int mtu;
{
	sc->sc_count = (mtu - sizeof(struct pfsync_header)) /
	    sizeof(struct pf_state);
	sc->sc_if.if_mtu = sizeof(struct pfsync_header) +
	    sc->sc_count * sizeof(struct pf_state);
}

struct mbuf *
pfsync_get_mbuf(sc, action)
	struct pfsync_softc *sc;
	u_int8_t action;
{
#ifndef __FreeBSD__
	extern int hz;
#endif
	struct pfsync_header *h;
	struct mbuf *m;
	int len;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		sc->sc_if.if_oerrors++;
		return (NULL);
	}

	len = sc->sc_if.if_mtu;
	if (len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			sc->sc_if.if_oerrors++;
			return (NULL);
		}
	}
	m->m_pkthdr.rcvif = NULL;
	m->m_pkthdr.len = m->m_len = len;

	h = mtod(m, struct pfsync_header *);
	h->version = PFSYNC_VERSION;
	h->af = 0;
	h->count = 0;
	h->action = action;

	sc->sc_mbuf = m;
	sc->sc_ptr = (struct pf_state *)((char *)h + PFSYNC_HDRLEN);
#ifdef __OpenBSD__
	timeout_add(&sc->sc_tmo, hz);
#else
	callout_reset(&pfsyncif.sc_tmo, hz, pfsync_timeout, &pfsyncif);
#endif

	return (m);
}

int
pfsync_pack_state(action, st)
	u_int8_t action;
	struct pf_state *st;
{
#ifndef __FreeBSD__
	extern struct timeval time;
#endif
	struct ifnet *ifp = &pfsyncif.sc_if;
	struct pfsync_softc *sc = ifp->if_softc;
	struct pfsync_header *h;
	struct pf_state *sp;
	struct pf_rule *r = st->rule.ptr;
	struct mbuf *m;
	u_long secs;
	int s, ret;

	if (action >= PFSYNC_ACT_MAX)
		return (EINVAL);

	s = splnet();
	m = sc->sc_mbuf;
	if (m == NULL) {
		if ((m = pfsync_get_mbuf(sc, action)) == NULL) {
			splx(s);
			return (ENOMEM);
		}
		h = mtod(m, struct pfsync_header *);
	} else {
		h = mtod(m, struct pfsync_header *);
		if (h->action != action) {
			pfsync_sendout(sc);
			if ((m = pfsync_get_mbuf(sc, action)) == NULL) {
				splx(s);
				return (ENOMEM);
			}
			h = mtod(m, struct pfsync_header *);
		}
	}

	sp = sc->sc_ptr++;
	h->count++;
	bzero(sp, sizeof(*sp));

	bcopy(&st->lan, &sp->lan, sizeof(sp->lan));
	bcopy(&st->gwy, &sp->gwy, sizeof(sp->gwy));
	bcopy(&st->ext, &sp->ext, sizeof(sp->ext));

	pf_state_peer_hton(&st->src, &sp->src);
	pf_state_peer_hton(&st->dst, &sp->dst);

	bcopy(&st->rt_addr, &sp->rt_addr, sizeof(sp->rt_addr));
#ifndef __FreeBSD__
	secs = time.tv_sec;
#else
	secs = time_second;
#endif
	sp->creation = htonl(secs - st->creation);
	if (st->expire <= secs)
		sp->expire = htonl(0);
	else
		sp->expire = htonl(st->expire - secs);
	sp->packets[0] = htonl(st->packets[0]);
	sp->packets[1] = htonl(st->packets[1]);
	sp->bytes[0] = htonl(st->bytes[0]);
	sp->bytes[1] = htonl(st->bytes[1]);
	if (r == NULL)
		sp->rule.nr = htonl(-1);
	else
		sp->rule.nr = htonl(r->nr);
	sp->af = st->af;
	sp->proto = st->proto;
	sp->direction = st->direction;
	sp->log = st->log;
	sp->allow_opts = st->allow_opts;

	ret = 0;
	if (h->count == sc->sc_count)
		ret = pfsync_sendout(sc);

	splx(s);
	return (0);
}

int
pfsync_clear_state(st)
	struct pf_state *st;
{
	struct ifnet *ifp = &pfsyncif.sc_if;
	struct pfsync_softc *sc = ifp->if_softc;
	struct mbuf *m = sc->sc_mbuf;
	int s, ret;

	s = splnet();
	if (m == NULL && (m = pfsync_get_mbuf(sc, PFSYNC_ACT_CLR)) == NULL) {
		splx(s);
		return (ENOMEM);
	}

	ret = (pfsync_sendout(sc));
	splx(s);
	return (ret);
}

void
pfsync_timeout(void *v)
{
	struct pfsync_softc *sc = v;
	int s;

	s = splnet();
	pfsync_sendout(sc);
	splx(s);
}

int
pfsync_sendout(sc)
	struct pfsync_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	struct mbuf *m = sc->sc_mbuf;

#ifdef __OpenBSD__
	timeout_del(&sc->sc_tmo);
#else
	callout_stop(&sc->sc_tmo);
#endif
	sc->sc_mbuf = NULL;
	sc->sc_ptr = NULL;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif

	m_freem(m);

	return (0);
}
