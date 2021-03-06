/*	$KAME: sctp_usrreq.c,v 1.42 2004/08/17 06:28:02 t-momose Exp $	*/

/*
 * Copyright (c) 2001, 2002, 2003, 2004 Cisco Systems, Inc.
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
 *      This product includes software developed by Cisco Systems, Inc.
 * 4. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CISCO SYSTEMS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CISCO SYSTEMS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#if !(defined(__OpenBSD__) || defined(__APPLE__))
#include "opt_ipsec.h"
#endif
#if defined(__FreeBSD__)
#include "opt_inet6.h"
#include "opt_inet.h"
#endif
#if defined(__NetBSD__)
#include "opt_inet.h"
#endif

#ifdef __APPLE__
#include <sctp.h>
#elif !defined(__OpenBSD__)
#include "opt_sctp.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/if_types.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
#include <net/if_var.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>

#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_asconf.h>
#ifdef IPSEC
#ifndef __OpenBSD__
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#else
#undef IPSEC
#endif
#endif /* IPSEC */

#include <net/net_osdep.h>

#if defined(HAVE_NRL_INPCB) || defined(__FreeBSD__)
#ifndef in6pcb
#define in6pcb		inpcb
#endif
#ifndef sotoin6pcb
#define sotoin6pcb      sotoinpcb
#endif
#endif

#ifdef SCTP_DEBUG
extern u_int32_t sctp_debug_on;
#endif /* SCTP_DEBUG */

/*
 * sysctl tunable variables
 */
/*
 * we use the number of mbufs and clusters to tune our initial send
 * and receive windows and the limit of chunks allocated.
 */
int sctp_max_burst_default = SCTP_DEF_MAX_BURST;
int sctp_peer_chunk_oh = sizeof(struct mbuf);
int sctp_strict_init = 1;
int sctp_no_csum_on_loopback = 1;
int sctp_max_chunks_on_queue = SCTP_ASOC_MAX_CHUNKS_ON_QUEUE;
int sctp_sendspace = (128 * 1024);	
int sctp_recvspace = 128 * (1024 +
#ifdef INET6
				sizeof(struct sockaddr_in6)
#else
				sizeof(struct sockaddr_in);
#endif
	);
int sctp_strict_sacks = 0;
int sctp_ecn = 1;
int sctp_ecn_nonce = 0;

void
sctp_init(void)
{
#ifdef __OpenBSD__
#define nmbclusters	nmbclust
#endif
	/* Init the SCTP pcb in sctp_pcb.c */
	u_long sb_max_adj;

	sctp_pcb_init();

#ifndef __OpenBSD__
	if (nmbclusters > SCTP_ASOC_MAX_CHUNKS_ON_QUEUE)
		sctp_max_chunks_on_queue = nmbclusters;
#else
/*	if (nmbclust > SCTP_ASOC_MAX_CHUNKS_ON_QUEUE)
	sctp_max_chunks_on_queue = nmbclust; FIX ME */
	sctp_max_chunks_on_queue = nmbclust * 2;
#endif
	/*
	 * Allow a user to take no more than 1/2 the number of clusters
	 * or the SB_MAX whichever is smaller for the send window.
	 */
	sb_max_adj = (u_long)((u_quad_t)(SB_MAX) * MCLBYTES / (MSIZE + MCLBYTES));
	sctp_sendspace = min((min(SB_MAX, sb_max_adj)),
#ifndef __OpenBSD__
			     ((nmbclusters/2) * SCTP_DEFAULT_MAXSEGMENT));
#else
			     ((nmbclust/2) * SCTP_DEFAULT_MAXSEGMENT));
#endif
	/*
	 * Now for the recv window, should we take the same amount?
	 * or should I do 1/2 the SB_MAX instead in the SB_MAX min above.
	 * For now I will just copy.
	 */
	sctp_recvspace = sctp_sendspace;
#ifdef __OpenBSD__
#undef nmbclusters
#endif
}

#ifdef INET6
void
ip_2_ip6_hdr(struct ip6_hdr *ip6, struct ip *ip)
{
	bzero(ip6, sizeof(*ip6));

	ip6->ip6_vfc = IPV6_VERSION;
	ip6->ip6_plen = ip->ip_len;
	ip6->ip6_nxt = ip->ip_p;
	ip6->ip6_hlim = ip->ip_ttl;
	ip6->ip6_src.s6_addr32[2] = ip6->ip6_dst.s6_addr32[2] =
		IPV6_ADDR_INT32_SMP;
	ip6->ip6_src.s6_addr32[3] = ip->ip_src.s_addr;
	ip6->ip6_dst.s6_addr32[3] = ip->ip_dst.s_addr;
}
#endif /* INET6 */

static void
sctp_split_chunks(struct sctp_association *asoc, 
		  struct sctp_stream_out *strm, 
		  struct sctp_tmit_chunk *chk)
{
	struct sctp_tmit_chunk *new_chk;

	/* First we need a chunk */
	new_chk = (struct sctp_tmit_chunk *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_chunk);
	if (new_chk == NULL) {
		chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		return;
	}
	sctppcbinfo.ipi_count_chunk++;
	sctppcbinfo.ipi_gencnt_chunk++;
	/* Copy it all */
	*new_chk = *chk;
	/*  split the data */
	new_chk->data = m_split(chk->data, (chk->send_size>>1), M_DONTWAIT);
	if (new_chk->data == NULL) {
		/* Can't split */
		chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, new_chk);
		sctppcbinfo.ipi_count_chunk--;
		if ((int)sctppcbinfo.ipi_count_chunk < 0) {
			panic("Chunk count is negative");
		}
		sctppcbinfo.ipi_gencnt_chunk++;
		return;

	}
	/* Data is now split adjust sizes */
	chk->send_size >>= 1;
	new_chk->send_size >>= 1;

	chk->book_size >>= 1;
	new_chk->book_size >>= 1;

	/* now adjust the marks */
	chk->rec.data.rcv_flags |= SCTP_DATA_FIRST_FRAG;
	chk->rec.data.rcv_flags &= ~SCTP_DATA_LAST_FRAG;

	new_chk->rec.data.rcv_flags &= ~SCTP_DATA_FIRST_FRAG;
	new_chk->rec.data.rcv_flags |= SCTP_DATA_LAST_FRAG;
	
	/* Increase ref count if dest is set */
	if (chk->whoTo) {
		new_chk->whoTo->ref_count++;
	}
	/* now drop it on the end of the list*/
	asoc->stream_queue_cnt++;
	TAILQ_INSERT_AFTER(&strm->outqueue, chk, new_chk, sctp_next);
}

static void
sctp_notify_mbuf(struct sctp_inpcb *inp,
		 struct sctp_tcb *stcb,
		 struct sctp_nets *net,
		 struct ip *ip,
		 struct sctphdr *sh)

{
	struct icmp *icmph;
	int totsz;
	uint16_t nxtsz;

	/* protection */
	if ((inp == NULL) || (stcb == NULL) || (net == NULL) ||
	    (ip == NULL) || (sh == NULL)) {
		return;
	}
	/* First job is to verify the vtag matches what I would send */
	if (ntohl(sh->v_tag) != (stcb->asoc.peer_vtag)) {
		return;
	}
	icmph = (struct icmp *)((caddr_t)ip - (sizeof(struct icmp) -
					       sizeof(struct ip)));
	if (icmph->icmp_type != ICMP_UNREACH) {
		/* We only care about unreachable */
		return;
	}
	if (icmph->icmp_code != ICMP_UNREACH_NEEDFRAG) {
		/* not a unreachable message due to frag. */
		return;
	}
	totsz = ip->ip_len;
	nxtsz = ntohs(icmph->icmp_seq);
	if (nxtsz == 0) {
		/*
		 * old type router that does not tell us what the next size
		 * mtu is. Rats we will have to guess (in a educated fashion
		 * of course)
		 */
		nxtsz = find_next_best_mtu(totsz);
	}
	/* Stop any PMTU timer */
	sctp_timer_stop(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, NULL);

	/* Adjust destination size limit */
	if (net->mtu > nxtsz) {
		net->mtu = nxtsz;
	}

	/* now what about the ep? */
	if (stcb->asoc.smallest_mtu > nxtsz) {
		struct sctp_tmit_chunk *chk,*nchk;
		struct sctp_stream_out *strm;
		/* Adjust that too */
		stcb->asoc.smallest_mtu = nxtsz;
		/* now off to subtract IP_DF flag if needed */

		TAILQ_FOREACH(chk, &stcb->asoc.send_queue, sctp_next) {
			if ((chk->send_size+IP_HDR_SIZE) > nxtsz) {
				chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
			}
		}
		TAILQ_FOREACH(chk, &stcb->asoc.sent_queue, sctp_next) {
			if ((chk->send_size+IP_HDR_SIZE) > nxtsz) {
				/*
				 * For this guy we also mark for immediate
				 * resend since we sent to big of chunk
				 */
				chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
				if (chk->sent != SCTP_DATAGRAM_RESEND) {
					stcb->asoc.sent_queue_retran_cnt++;
				}
				chk->sent = SCTP_DATAGRAM_RESEND;
				chk->rec.data.doing_fast_retransmit = 0;

				/* Clear any time so NO RTT is being done */
				chk->do_rtt = 0;
				stcb->asoc.total_flight -= chk->send_size;
				if (stcb->asoc.total_flight < 0) {
					stcb->asoc.total_flight = 0;
				}
				stcb->asoc.total_flight_book -= chk->book_size;
				if (stcb->asoc.total_flight_book < 0) {
					stcb->asoc.total_flight_book = 0;
				}
				stcb->asoc.total_flight_count--;
				if (stcb->asoc.total_flight_count < 0) {
					stcb->asoc.total_flight_count = 0;
				}
				net->flight_size -= chk->send_size;
				if (net->flight_size < 0) {
					net->flight_size = 0;
				}
			}
		}
		TAILQ_FOREACH(strm, &stcb->asoc.out_wheel, next_spoke) {
			chk = TAILQ_FIRST(&strm->outqueue);
			while (chk) {
				nchk = TAILQ_NEXT(chk, sctp_next);
				if ((chk->send_size+SCTP_MED_OVERHEAD) > nxtsz) {
					sctp_split_chunks(&stcb->asoc, strm, chk);
				}
				chk = nchk;
			}
		}
	}
	sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, NULL);
}


void
sctp_notify(struct sctp_inpcb *inp,
	    int errno,
	    struct sctphdr *sh,
	    struct sockaddr *to,
	    struct sctp_tcb *stcb,
	    struct sctp_nets *net)
{
	/* protection */
	if ((inp == NULL) || (stcb == NULL) || (net == NULL) ||
	    (sh == NULL) || (to == NULL)) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("sctp-notify, bad call\n");
		}
#endif /* SCTP_DEBUG */
		return;
	}
	/* First job is to verify the vtag matches what I would send */
	if (ntohl(sh->v_tag) != (stcb->asoc.peer_vtag)) {
		return;
	}

/* FIX ME FIX ME PROTOPT i.e. no SCTP should ALWAYS be an ABORT */

	if ((errno == EHOSTUNREACH) ||  /* Host is not reachable */
	    (errno == EHOSTDOWN) ||	/* Host is down */
	    (errno == ECONNREFUSED) ||	/* Host refused the connection, (not an abort?) */
	    (errno == ENOPROTOOPT)	/* SCTP is not present on host */
		) {
		/*
		 * Hmm reachablity problems we must examine closely.
		 * If its not reachable, we may have lost a network.
		 * Or if there is NO protocol at the other end named SCTP.
		 * well we consider it a OOTB abort.
		 */
		if ((errno == EHOSTUNREACH) || (errno == EHOSTDOWN)) {
			if (net->dest_state & SCTP_ADDR_REACHABLE) {
				/* Ok that destination is NOT reachable */
				net->dest_state &= ~SCTP_ADDR_REACHABLE;
				net->dest_state |= SCTP_ADDR_NOT_REACHABLE;
				net->error_count = net->failure_threshold + 1;
				sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_DOWN,
						stcb, SCTP_FAILED_THRESHOLD,
						(void *)net);
			}
		} else {
			/*
			 * Here the peer is either playing tricks on us,
			 * including an address that belongs to someone who
			 * does not support SCTP OR was a userland
			 * implementation that shutdown and now is dead. In
			 * either case treat it like a OOTB abort with no TCB
			 */
			sctp_abort_notification(stcb, SCTP_PEER_FAULTY);
			sctp_free_assoc(inp, stcb);
		}
	} else {
		/* Send all others to the app */
		if (inp->sctp_socket) {
			inp->sctp_socket->so_error = errno;
			sctp_sowwakeup(inp, inp->sctp_socket);
		}
	}
}

#if defined(__FreeBSD__) || defined(__APPLE__)
void
#else
void *
#endif
sctp_ctlinput(cmd, sa, vip)
	int cmd;
	struct sockaddr *sa;
	void *vip;
{
	struct ip *ip = vip;
	struct sctphdr *sh;
	int s;


	if (sa->sa_family != AF_INET ||
	    ((struct sockaddr_in *)sa)->sin_addr.s_addr == INADDR_ANY) {
#if defined(__FreeBSD__) || defined(__APPLE__)
		return;
#else
		return (NULL);
#endif
	}

	if (PRC_IS_REDIRECT(cmd)) {
		ip = 0;
	} else if ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0) {
#if defined(__FreeBSD__) || defined(__APPLE__)
		return;
#else
		return (NULL);
#endif
	}
	if (ip) {
		struct sctp_inpcb *inp;
		struct sctp_tcb *stcb;
		struct sctp_nets *net;
		struct sockaddr_in to, from;

		sh = (struct sctphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		bzero(&to, sizeof(to));
		bzero(&from, sizeof(from));
		from.sin_family = to.sin_family = AF_INET;
		from.sin_len = to.sin_len = sizeof(to);
		from.sin_port = sh->src_port;
		from.sin_addr = ip->ip_src;
		to.sin_port = sh->dest_port;
		to.sin_addr = ip->ip_dst;

		/*
		 * 'to' holds the dest of the packet that failed to be sent.
		 * 'from' holds our local endpoint address.
		 * Thus we reverse the to and the from in the lookup.
		 */
#if defined(__NetBSD__) || defined(__OpenBSD__)
		s = splsoftnet();
#else
		s = splnet();
#endif
		stcb = sctp_findassociation_addr_sa((struct sockaddr *)&from,
						    (struct sockaddr *)&to,
						    &inp, &net, 1);
		if (stcb != NULL && inp && (inp->sctp_socket != NULL)) {
			if (cmd != PRC_MSGSIZE) {
				int cm;
				if (cmd == PRC_HOSTDEAD) {
					cm = EHOSTUNREACH;
				} else {
					cm = inetctlerrmap[cmd];
				}
				sctp_notify(inp, cm, sh,
					    (struct sockaddr *)&to, stcb,
					    net);
			} else {
				/* handle possible ICMP size messages */
				sctp_notify_mbuf(inp, stcb, net, ip, sh);
			}
		} else {
			if (PRC_IS_REDIRECT(cmd) && inp) {
				in_rtchange((struct inpcb *)inp,
					    inetctlerrmap[cmd]);
			}
		}
		splx(s);
	}
#if defined(__FreeBSD__) || defined(__APPLE__)
	return;
#else
	return (NULL);
#endif
}

#if defined(__FreeBSD__)
static int
sctp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct sockaddr_in addrs[2];
	struct sctp_inpcb *inp;
	struct sctp_nets *net;
	struct sctp_tcb *stcb;
	int error, s;

#if __FreeBSD_version >= 500000
	error = suser(req->td);
#else
	error = suser(req->p);
#endif
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);

	s = splnet();
	stcb = sctp_findassociation_addr_sa(sintosa(&addrs[0]),
					   sintosa(&addrs[1]),
					   &inp, &net, 1);
	if (stcb == NULL || inp == NULL || inp->sctp_socket == NULL) {
		error = ENOENT;
		goto out;
	}
	error = SYSCTL_OUT(req, inp->sctp_socket->so_cred, sizeof(struct ucred));
 out:
	splx(s);
	return (error);
}

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, getcred, CTLTYPE_OPAQUE|CTLFLAG_RW,
	    0, 0, sctp_getcred, "S,ucred", "Get the ucred of a SCTP connection");
#endif /* #if defined(__FreeBSD__) */

/*
 * sysctl definitions
 */
#if defined(__FreeBSD__) || defined(__APPLE__)
SYSCTL_INT(_net_inet_sctp, SCTPCTL_MAXDGRAM, maxdgram, CTLFLAG_RW,
	   &sctp_sendspace, 0, "Maximum outgoing SCTP buffer size");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_RECVSPACE, recvspace, CTLFLAG_RW,
	   &sctp_recvspace, 0, "Maximum incoming SCTP buffer size");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_AUTOASCONF, auto_asconf, CTLFLAG_RW,
	   &sctp_auto_asconf, 0, "Enable SCTP Auto-ASCONF");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_ECN_ENABLE, ecn_enable, CTLFLAG_RW,
	   &sctp_ecn, 0, "Enable SCTP ECN");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_ECN_NONCE, ecn_nonce, CTLFLAG_RW,
	   &sctp_ecn_nonce, 0, "Enable SCTP ECN Nonce");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_STRICT_SACK, strict_sacks, CTLFLAG_RW,
	   &sctp_strict_sacks, 0, "Enable SCTP Strict SACK checking");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_NOCSUM_LO, loopback_nocsum, CTLFLAG_RW,
	   &sctp_no_csum_on_loopback, 0, "Enable NO Csum on packets sent on loopback");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_STRICT_INIT, strict_init, CTLFLAG_RW,
	   &sctp_strict_init, 0, "Enable strict INIT/INIT-ACK singleton enforcement");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_PEER_CHK_OH, peer_chkoh, CTLFLAG_RW,
	   &sctp_peer_chunk_oh, 0, "Amount to debit peers rwnd per chunk sent");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_MAXBURST, maxburst, CTLFLAG_RW,
	   &sctp_max_burst_default, 0, "Default max burst for sctp endpoints");

SYSCTL_INT(_net_inet_sctp, SCTPCTL_MAXCHUNKONQ, maxchunks, CTLFLAG_RW,
	   &sctp_max_chunks_on_queue, 0, "Default max chunks on queue per asoc");

#endif

static int
sctp_abort(struct socket *so)
{
	struct sctp_inpcb *inp;
	int s;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return EINVAL;	/* ??? possible? panic instead? */

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	sctp_inpcb_free(inp,1);
	splx(s);
	return 0;
}

static int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_attach(struct socket *so, int proto, struct thread *p)
#else
sctp_attach(struct socket *so, int proto, struct proc *p)
#endif
{
	struct sctp_inpcb *inp;
	struct inpcb *ip_inp;
	int s, error;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp != 0) {
		splx(s);
		return EINVAL;
	}
	error = soreserve(so, sctp_sendspace, sctp_recvspace);
	if (error) {
		splx(s);
		return error;
	}
	error = sctp_inpcb_alloc(so);
	if (error) {
		splx(s);
		return error;
	}

	inp = (struct sctp_inpcb *)so->so_pcb;
	inp->sctp_flags &= ~SCTP_PCB_FLAGS_BOUND_V6;	/* I'm not v6! */
	ip_inp = &inp->ip_inp.inp;
#if defined(__FreeBSD__) || defined(__APPLE__)
	ip_inp->inp_vflag |= INP_IPV4;
	ip_inp->inp_ip_ttl = ip_defttl;
#else
	inp->inp_vflag |= INP_IPV4;
	inp->inp_ip_ttl = ip_defttl;
#endif

#ifdef IPSEC
#if !(defined(__OpenBSD__) || defined(__APPLE__))
	error = ipsec_init_pcbpolicy(so, &ip_inp->inp_sp);
	if (error != 0) {
		sctp_inpcb_free(inp,1);
		return error;
	}
#endif
#endif /*IPSEC*/

#if defined(__NetBSD__)
	so->so_send = sctp_sosend;
#endif
	splx(s);
	return 0;
}

static int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_bind(struct socket *so, struct sockaddr *addr, struct thread *p)
{
#elif defined(__FreeBSD__) || defined(__APPLE__)
sctp_bind(struct socket *so, struct sockaddr *addr, struct proc *p)
{
#else
sctp_bind(struct socket *so, struct mbuf *nam, struct proc *p)
{
	struct sockaddr *addr = nam ? mtod(nam, struct sockaddr *) : NULL;
#endif
	struct sctp_inpcb *inp;
	int s, error;

#ifdef INET6
	if (addr && addr->sa_family != AF_INET)
		/* must be a v4 address! */
		return EINVAL;
#endif /* INET6 */

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return EINVAL;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	error = sctp_inpcb_bind(so, addr, p);
	splx(s);
	return error;
}


static int
sctp_detach(struct socket *so)
{
	struct sctp_inpcb *inp;
	int s;
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return EINVAL;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	if (((so->so_options & SO_LINGER) && (so->so_linger == 0)) ||
	    (so->so_rcv.sb_cc > 0)) {
		sctp_inpcb_free(inp,1);
	} else {
		sctp_inpcb_free(inp,0);
	}
	splx(s);
	return 0;
}

int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
	  struct mbuf *control, struct thread *p);
#else
sctp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
	  struct mbuf *control, struct proc *p);
#endif

int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
	  struct mbuf *control, struct thread *p)
{
#else
sctp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
	  struct mbuf *control, struct proc *p)
{
#endif
	struct sctp_inpcb *inp;
	int error;
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		sctp_m_freem(m);
		return EINVAL;
	}
	/* Got to have an to address if we are NOT a connected socket */
	if ((addr == NULL) &&
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) ||
	     (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE))
		) {
		goto connected_type;
	} else if (addr == NULL) {
		error = EDESTADDRREQ;
		sctp_m_freem(m);
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		return (error);
	}
#ifdef INET6
	if (addr->sa_family != AF_INET) {
		/* must be a v4 address! */
		sctp_m_freem(m);
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		error = EDESTADDRREQ;
		return EINVAL;
	}
#endif /* INET6 */
 connected_type:
	/* now what about control */
	if (control) {
		if (inp->control) {
			printf("huh? control set?\n");
			sctp_m_freem(inp->control);
			inp->control = NULL;
		}
		inp->control = control;
	}
	/* add it in possibly */
	if ((inp->pkt) && (inp->pkt->m_flags & M_PKTHDR)) {
		struct mbuf *x;
		int c_len;

		c_len = 0;
		/* How big is it */
		for (x=m;x;x = x->m_next) {
			c_len += x->m_len;
		}
		inp->pkt->m_pkthdr.len += c_len;
	}
	/* Place the data */
	if (inp->pkt) {
		inp->pkt_last->m_next = m;
		inp->pkt_last = m;
	} else {
		inp->pkt_last = inp->pkt = m;
	}
	if (
#if defined (__FreeBSD__) || defined(__APPLE__)
	    /* FreeBSD uses a flag passed */
	    ((flags & PRUS_MORETOCOME) == 0)
#elif defined( __NetBSD__)
	    /* NetBSD uses the so_state field */
	    ((so->so_state & SS_MORETOCOME) == 0)
#else
	    1   /* Open BSD does not have any "more to come" indication */
#endif
	    ) {
		/*
		 * note with the current version this code will only be used
		 * by OpenBSD-- NetBSD, FreeBSD, and MacOS have methods for
		 * re-defining sosend to use the sctp_sosend. One can
		 * optionally switch back to this code (by changing back the
		 * definitions) but this is not advisable.
	     */
		int ret;
		ret = sctp_output(inp, inp->pkt, addr, inp->control, p);
		inp->pkt = NULL;
		inp->control = NULL;
		return (ret);
	} else {
		return (0);
	}
}

static int
sctp_disconnect(struct socket *so)
{
	struct sctp_inpcb *inp;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == NULL) {
		splx(s);
		return (ENOTCONN);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		if (LIST_EMPTY(&inp->sctp_asoc_list)) {
			/* No connection */
			splx(s);
			return (0);
		} else {
			int some_on_streamwheel = 0;
			struct sctp_association *asoc;
			struct sctp_tcb *stcb;

			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				splx(s);
				return (EINVAL);
			}
			asoc = &stcb->asoc;
			if (((so->so_options & SO_LINGER) &&
			     (so->so_linger == 0)) ||
			    (so->so_rcv.sb_cc > 0)) {
				if (SCTP_GET_STATE(asoc) !=
				    SCTP_STATE_COOKIE_WAIT) {
					/* Left with Data unread */
					struct mbuf *err;
					err = NULL;
					MGET(err, M_DONTWAIT, MT_DATA);
					if (err) {
						/* Fill in the user initiated abort */
						struct sctp_paramhdr *ph;
						ph = mtod(err, struct sctp_paramhdr *);
						err->m_len = sizeof(struct sctp_paramhdr);
						ph->param_type = htons(SCTP_CAUSE_USER_INITIATED_ABT);
						ph->param_length = htons(err->m_len);
					}
					sctp_send_abort_tcb(stcb, err);
				}
				sctp_free_assoc(inp, stcb);
				splx(s);
				return (0);
			}
			if (!TAILQ_EMPTY(&asoc->out_wheel)) {
				/* Check to see if some data queued */
				struct sctp_stream_out *outs;
				TAILQ_FOREACH(outs, &asoc->out_wheel,
					      next_spoke) {
					if (!TAILQ_EMPTY(&outs->outqueue)) {
						some_on_streamwheel = 1;
						break;
					}
				}
			}

			if (TAILQ_EMPTY(&asoc->send_queue) &&
			    TAILQ_EMPTY(&asoc->sent_queue) &&
			    (some_on_streamwheel == 0)) {
				/* there is nothing queued to send, so done */
				if ((SCTP_GET_STATE(asoc) != 
				     SCTP_STATE_SHUTDOWN_SENT) &&
				    (SCTP_GET_STATE(asoc) != 
				     SCTP_STATE_SHUTDOWN_ACK_SENT)) {
					/* only send SHUTDOWN 1st time thru */
					sctp_send_shutdown(stcb,
							   stcb->asoc.primary_destination);
					sctp_chunk_output(stcb->sctp_ep, stcb, 1);
					asoc->state = SCTP_STATE_SHUTDOWN_SENT;
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
							 stcb->sctp_ep, stcb,
							 asoc->primary_destination);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
							 stcb->sctp_ep, stcb,
							 asoc->primary_destination);
				}
			} else {
				/*
				 * we still got (or just got) data to send,
				 * so set SHUTDOWN_PENDING
				 */
				/*
				 * XXX sockets draft says that MSG_EOF should
				 * be sent with no data.
				 * currently, we will allow user data to be
				 * sent first and move to SHUTDOWN-PENDING
				 */
				asoc->state |= SCTP_STATE_SHUTDOWN_PENDING;
			}
			splx(s);
			return (0);
		}
	} else {
		/* UDP model does not support this */
		splx(s);
		return EOPNOTSUPP;
	}
}

int
sctp_shutdown(struct socket *so)
{
	struct sctp_inpcb *inp;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		splx(s);
		return EINVAL;
	}

	/* For UDP model this is a invalid call */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
		/* Restore the flags that the soshutdown took away. */
		so->so_state &= ~SS_CANTRCVMORE;
		/* This proc will wakeup for read and do nothing (I hope) */
		splx(s);
		return (EOPNOTSUPP);
	}
#ifdef SCTP_TCP_MODEL_SUPPORT
	/*
	 * Ok if we reach here its the TCP model and it is either a SHUT_WR
	 * or SHUT_RDWR. This means we put the shutdown flag against it.
	 */
	{
		int some_on_streamwheel = 0;
		struct sctp_tcb *stcb;
		struct sctp_association *asoc;
		socantsendmore(so);

		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb == NULL) {
			/*
			 * Ok we hit the case that the shutdown call was made
			 * after an abort or something. Nothing to do now.
			 */
			splx(s);
			return (0);
		}
		asoc = &stcb->asoc;

		if (!TAILQ_EMPTY(&asoc->out_wheel)) {
			/* Check to see if some data queued */
			struct sctp_stream_out *outs;
			TAILQ_FOREACH(outs, &asoc->out_wheel, next_spoke) {
				if (!TAILQ_EMPTY(&outs->outqueue)) {
					some_on_streamwheel = 1;
					break;
				}
			}
		}
		if (TAILQ_EMPTY(&asoc->send_queue) &&
		    TAILQ_EMPTY(&asoc->sent_queue) &&
		    (some_on_streamwheel == 0)) {
			/* there is nothing queued to send, so I'm done... */
			if (SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_SENT) {
				/* only send SHUTDOWN the first time through */
				sctp_send_shutdown(stcb,
						   stcb->asoc.primary_destination);
				sctp_chunk_output(stcb->sctp_ep, stcb, 1);
				asoc->state = SCTP_STATE_SHUTDOWN_SENT;
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
						 stcb->sctp_ep, stcb,
						 asoc->primary_destination);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
						 stcb->sctp_ep, stcb,
						 asoc->primary_destination);
			}
		} else {
			/*
			 * we still got (or just got) data to send, so
			 * set SHUTDOWN_PENDING
			 */
			asoc->state |= SCTP_STATE_SHUTDOWN_PENDING;
		}
	}
	splx(s);
	return 0;
#endif
	splx(s);
	return (EOPNOTSUPP);
}

/*
 * copies a "user" presentable address and removes embedded scope, etc.
 * returns 0 on success, 1 on error
 */
static uint32_t
sctp_fill_user_address(struct sockaddr_storage *ss, struct sockaddr *sa)
{
	struct sockaddr_in6 lsa6;
	sa = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)sa,
						   &lsa6);
	memcpy(ss, sa, sa->sa_len);
	return (0);
}


#if defined(__NetBSD__) || defined(__OpenBSD__)
/*
 * On NetBSD and OpenBSD in6_sin_2_v4mapsin6() not used and not exported,
 * so we have to export it here.
 */
void    in6_sin_2_v4mapsin6 __P((struct sockaddr_in *sin,
                                 struct sockaddr_in6 *sin6));
#endif

static int
sctp_fill_up_addresses(struct sctp_inpcb *inp,
		       struct sctp_tcb *stcb,
		       int limit,
		       struct sockaddr_storage *sas)
{
	struct ifnet *ifn;
	struct ifaddr *ifa;
	int loopback_scope, ipv4_local_scope, local_scope, site_scope, actual;
	int ipv4_addr_legal, ipv6_addr_legal;
	actual = 0;
	if (limit <= 0)
		return (actual);

	if (stcb) {
		/* Turn on all the appropriate scope */
		loopback_scope = stcb->asoc.loopback_scope;
		ipv4_local_scope = stcb->asoc.ipv4_local_scope;
		local_scope = stcb->asoc.local_scope;
		site_scope = stcb->asoc.site_scope;
	} else {
		/* Turn on ALL scope, since we look at the EP */
		loopback_scope = ipv4_local_scope = local_scope =
			site_scope = 1;
	}
	ipv4_addr_legal = ipv6_addr_legal = 0;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
		ipv6_addr_legal = 1;
		if (
#if defined(__OpenBSD__)
		(0) /* we always do dual bind */
#elif defined (__NetBSD__)
		(((struct in6pcb *)inp)->in6p_flags & IN6P_IPV6_V6ONLY)
#else
		(((struct in6pcb *)inp)->inp_flags & IN6P_IPV6_V6ONLY)
#endif
		== 0) {
			ipv4_addr_legal = 1;
		}
	} else {
		ipv4_addr_legal = 1;
	}

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		TAILQ_FOREACH(ifn, &ifnet, if_list) {
			if ((loopback_scope == 0) &&
			    (ifn->if_type == IFT_LOOP)) {
				/* Skip loopback if loopback_scope not set */
				continue;
			}
			TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
				if (stcb) {
				/*
				 * For the BOUND-ALL case, the list
				 * associated with a TCB is Always
				 * considered a reverse list.. i.e.
				 * it lists addresses that are NOT
				 * part of the association. If this
				 * is one of those we must skip it.
				 */
					if (sctp_is_addr_restricted(stcb,
								    ifa->ifa_addr)) {
						continue;
					}
				}
				if ((ifa->ifa_addr->sa_family == AF_INET) &&
				    (ipv4_addr_legal)) {
					struct sockaddr_in *sin;
					sin = (struct sockaddr_in *)ifa->ifa_addr;
					if (sin->sin_addr.s_addr == 0) {
						/* we skip unspecifed addresses */
						continue;
					}
					if ((ipv4_local_scope == 0) &&
					    (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr))) {
						continue;
					}
					if (inp->sctp_flags & SCTP_I_WANT_MAPPED_V4_ADDR) {
						in6_sin_2_v4mapsin6(sin,(struct sockaddr_in6 *)sas);
						((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
						sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(struct sockaddr_in6));
						actual += sizeof(sizeof(struct sockaddr_in6));
					} else {
						memcpy(sas, sin, sizeof(*sin));
						((struct sockaddr_in *)sas)->sin_port = inp->sctp_lport;
						sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(*sin));
						actual += sizeof(*sin);
					}
					if (actual >= limit) {
						return (actual);
					}
				} else if ((ifa->ifa_addr->sa_family == AF_INET6) &&
					   (ipv6_addr_legal)) {
					struct sockaddr_in6 *sin6, lsa6;
					sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
					if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
						/* we skip unspecifed addresses */
						continue;
					}
					if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
						if (local_scope == 0)
							continue;
						if (sin6->sin6_scope_id == 0) {
							lsa6 = *sin6;
							if (in6_recoverscope(&lsa6,
									     &lsa6.sin6_addr,
									     NULL))
								/* bad link local address */
								continue;
							sin6 = &lsa6;
						}
					}
					if ((site_scope == 0) &&
					    (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))) {
						continue;
					}
					memcpy(sas, sin6, sizeof(*sin6));
					((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
					sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(*sin6));
					actual += sizeof(*sin6);
					if (actual >= limit) {
						return (actual);
					}
				}
			}
		}
	} else {
		struct sctp_laddr *laddr;
		/*
		 * If we have a TCB and we do NOT support ASCONF (it's
		 * turned off or otherwise) then the list is always the
		 * true list of addresses (the else case below).  Otherwise
		 * the list on the association is a list of addresses that
		 * are NOT part of the association.
		 */
		if (inp->sctp_flags & SCTP_PCB_FLAGS_DO_ASCONF) {
			/* The list is a NEGATIVE list */
			LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
				if (stcb) {
					if (sctp_is_addr_restricted(stcb, laddr->ifa->ifa_addr)) {
						continue;
					}
				}
				if (sctp_fill_user_address(sas, laddr->ifa->ifa_addr))
					continue;

				((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
				sas = (struct sockaddr_storage *)((caddr_t)sas +
								  laddr->ifa->ifa_addr->sa_len);
				actual += laddr->ifa->ifa_addr->sa_len;
				if (actual >= limit) {
					return (actual);
				}
			}
		} else {
			/* The list is a positive list if present */
			if (stcb) {
				/* Must use the specific association list */
				LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
					     sctp_nxt_addr) {
					if (sctp_fill_user_address(sas,
								   laddr->ifa->ifa_addr))
						continue;
					((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
					sas = (struct sockaddr_storage *)((caddr_t)sas +
									  laddr->ifa->ifa_addr->sa_len);
					actual += laddr->ifa->ifa_addr->sa_len;
					if (actual >= limit) {
						return (actual);
					}
				}
			} else {
				/* No endpoint so use the endpoints individual list */
				LIST_FOREACH(laddr, &inp->sctp_addr_list,
					     sctp_nxt_addr) {
					if (sctp_fill_user_address(sas,
								   laddr->ifa->ifa_addr))
						continue;
					((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
					sas = (struct sockaddr_storage *)((caddr_t)sas +
									  laddr->ifa->ifa_addr->sa_len);
					actual += laddr->ifa->ifa_addr->sa_len;
					if (actual >= limit) {
						return (actual);
					}
				}
			}
		}
	}
	return (actual);
}

static int
sctp_count_max_addresses(struct sctp_inpcb *inp)
{
	int cnt = 0;
	/*
	 * In both sub-set bound an bound_all cases we return the MAXIMUM
	 * number of addresses that you COULD get. In reality the sub-set
	 * bound may have an exclusion list for a given TCB OR in the
	 * bound-all case a TCB may NOT include the loopback or other
	 * addresses as well.
	 */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		struct ifnet *ifn;
		struct ifaddr *ifa;

		TAILQ_FOREACH(ifn, &ifnet, if_list) {
			TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
				/* Count them if they are the right type */
				if (ifa->ifa_addr->sa_family == AF_INET) {
					if (inp->sctp_flags & SCTP_I_WANT_MAPPED_V4_ADDR) 
						cnt += sizeof(struct sockaddr_in6);
					else
						cnt += sizeof(struct sockaddr_in);
					
				} else if (ifa->ifa_addr->sa_family == AF_INET6)
					cnt += sizeof(struct sockaddr_in6);
			}
		}
	} else {
		struct sctp_laddr *laddr;
		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa->ifa_addr->sa_family == AF_INET) {
				if (inp->sctp_flags & SCTP_I_WANT_MAPPED_V4_ADDR) 
					cnt += sizeof(struct sockaddr_in6);
				else
					cnt += sizeof(struct sockaddr_in);
				
			} else if (laddr->ifa->ifa_addr->sa_family == AF_INET6)
				cnt += sizeof(struct sockaddr_in6);
		}
	}
	return (cnt);
}

static int
sctp_do_connect_x(struct socket *so,
		  struct sctp_inpcb *inp,
		  struct mbuf *m,
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
		  struct thread *p
#else
  	          struct proc *p
#endif
	)
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
        int s = splsoftnet();
#else
	int s = splnet();
#endif
        int error = 0;
	struct sctp_tcb *stcb = NULL;
	struct sockaddr *sa;
	int num_v6=0, num_v4=0, *totaddrp, totaddr, i, incr, at;
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Connectx called\n");
	}
#endif /* SCTP_DEBUG */
#ifdef SCTP_TCP_MODEL_SUPPORT
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		splx(s);
		return (EADDRINUSE);
	}
#endif /* SCTP_TCP_MODEL_SUPPORT */
#ifdef SCTP_TCP_MODEL_SUPPORT
	if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
	if (stcb) {
		splx(s);
		return (EALREADY);

	}
#endif /* SCTP_TCP_MODEL_SUPPORT */

	totaddrp = mtod(m, int *);
	totaddr = *totaddrp;
	sa = (struct sockaddr *)(totaddrp + 1);
	at = incr = 0;
	/* account and validate addresses */
	for (i = 0; i < totaddr; i++) {
		if (sa->sa_family == AF_INET) {
			num_v4++;
			incr = sizeof(struct sockaddr_in);
		} else if (sa->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin6;
			sin6 = (struct sockaddr_in6 *)sa;
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				/* Must be non-mapped for connectx */
				splx(s);
				return EINVAL;
			}
			num_v6++;
			incr = sizeof(struct sockaddr_in6);
		} else {
			totaddr = i;
			break;
		}
		stcb = sctp_findassociation_ep_addr(&inp, sa, NULL, NULL);
		if (stcb != NULL) {
			/* Already have or am bring up an association */
			splx(s);
			return (EALREADY);
		}
		if ((at + incr) > m->m_len) {
			totaddr = i;
			break;
		}
		sa = (struct sockaddr *)((caddr_t)sa + incr);
	}
	sa = (struct sockaddr *)(totaddrp + 1);
#ifdef INET6
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) &&
	    (num_v6 > 0)) {
		splx(s);
		return (EINVAL);
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
	    (num_v4 > 0)) {
		struct in6pcb *inp6;
		inp6 = (struct in6pcb *)inp;
		if (
#if defined(__OpenBSD__)
			(0) /* we always do dual bind */
#elif defined (__NetBSD__)
			(inp6->in6p_flags & IN6P_IPV6_V6ONLY)
#else
			(inp6->inp_flags & IN6P_IPV6_V6ONLY)
#endif
			) {
			/*
			 * if IPV6_V6ONLY flag, ignore connections
			 * destined to a v4 addr or v4-mapped addr
			 */
			splx(s);
			return EINVAL;
		}
	}
#endif /* INET6 */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) ==
	    SCTP_PCB_FLAGS_UNBOUND) {
		/* Bind a ephemeral port */
		error = sctp_inpcb_bind(so, NULL, p);
		if (error) {
			splx(s);
			return (error);
		}
	}
        /* We are GOOD to go */
	stcb = sctp_aloc_assoc(inp, sa, 1, &error);
	if (stcb == NULL) {
		/* Gak! no memory */
		splx(s);
		return (error);
	}
	/* move to second address */
	if (sa->sa_family == AF_INET)
		sa = (struct sockaddr *)((caddr_t)sa + sizeof(struct sockaddr_in));
	else
		sa = (struct sockaddr *)((caddr_t)sa + sizeof(struct sockaddr_in6));

	for (i = 1; i < totaddr; i++) {
		if (sa->sa_family == AF_INET) {
			incr = sizeof(struct sockaddr_in);
			if (sctp_add_remote_addr(stcb, sa, 0, 8)) {
				sctp_free_assoc(inp, stcb);
				splx(s);
				return (ENOBUFS);
			}

		} else if (sa->sa_family == AF_INET6) {
			incr = sizeof(struct sockaddr_in6);
			if (sctp_add_remote_addr(stcb, sa, 0, 8)) {
				sctp_free_assoc(inp, stcb);
				splx(s);
				return (ENOBUFS);
			}
		}
		sa = (struct sockaddr *)((caddr_t)sa + incr);
	}
#ifdef SCTP_TCP_MODEL_SUPPORT
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		/* Set the connected flag so we can queue data */
		soisconnecting(so);
	}
#endif
	stcb->asoc.state = SCTP_STATE_COOKIE_WAIT;
	SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);
	sctp_send_initiate(inp, stcb);
	splx(s);
	return error;
}


static int
sctp_optsget(struct socket *so,
	     int opt,
	     struct mbuf **mp,
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	     struct thread *p
#else
	     struct proc *p
#endif
	)
{
	struct sctp_inpcb *inp;
	struct mbuf *m;
	int error, optval=0;
	struct sctp_tcb *stcb = NULL;

        inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return EINVAL;
	error = 0;

	if (mp == NULL) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("optsget:MP is NULL EINVAL\n");
		}
#endif /* SCTP_DEBUG */
		return (EINVAL);
	}
	m = *mp;
	if (m == NULL) {
		/* Got to have a mbuf */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("Huh no mbuf\n");
		}
#endif /* SCTP_DEBUG */
		return (EINVAL);
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
		printf("optsget opt:%lxx sz:%u\n", (unsigned long)opt,
		       m->m_len);
	}
#endif /* SCTP_DEBUG */

	switch (opt) {
	case SCTP_NODELAY:
	case SCTP_AUTOCLOSE:
	case SCTP_AUTO_ASCONF:
	case SCTP_DISABLE_FRAGMENTS:
	case SCTP_I_WANT_MAPPED_V4_ADDR:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
			printf("other stuff\n");
		}
#endif /* SCTP_DEBUG */
		switch (opt) {
		case SCTP_DISABLE_FRAGMENTS:
			optval = inp->sctp_flags & SCTP_PCB_FLAGS_NO_FRAGMENT;
			break;
		case SCTP_I_WANT_MAPPED_V4_ADDR:
			optval = inp->sctp_flags & SCTP_I_WANT_MAPPED_V4_ADDR;
			break;
		case SCTP_AUTO_ASCONF:
			optval = inp->sctp_flags & SCTP_PCB_FLAGS_AUTO_ASCONF;
			break;
		case SCTP_NODELAY:
			optval = inp->sctp_flags & SCTP_PCB_FLAGS_NODELAY;
			break;
		case SCTP_AUTOCLOSE:
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_AUTOCLOSE) ==
			    SCTP_PCB_FLAGS_AUTOCLOSE)
				optval = inp->sctp_ep.auto_close_time;
			else
				optval = 0;
			break;

		default:
			error = ENOPROTOOPT;
		} /* end switch (sopt->sopt_name) */
		if (opt != SCTP_AUTOCLOSE) {
			/* make it an "on/off" value */
			optval = (optval != 0);
		}
		if (m->m_len < sizeof(int)) {
			error = EINVAL;
		}
		if (error == 0) {
			/* return the option value */
			*mtod(m, int *) = optval;
			m->m_len = sizeof(optval);
		}
		break;
	case SCTP_GET_NONCE_VALUES:
	{
		struct sctp_get_nonce_values *gnv;
		if (m->m_len < sizeof(struct sctp_get_nonce_values)) {
			error = EINVAL;
			break;
		}
		gnv = mtod(m, struct sctp_get_nonce_values *);
		stcb = sctp_findassociation_associd(gnv->gn_assoc_id);
		if (stcb == NULL) {
			error = ENOTCONN;
		} else {
			gnv->gn_peers_tag = stcb->asoc.peer_vtag;
			gnv->gn_local_tag = stcb->asoc.my_vtag;
		}
		
	}
		break;
	case SCTP_PEER_PUBLIC_KEY:
	case SCTP_MY_PUBLIC_KEY:
	case SCTP_SET_AUTH_CHUNKS:
	case SCTP_SET_AUTH_SECRET:
		/* not supported yet and until we refine the draft */
		error = EOPNOTSUPP;
		break;


	case SCTP_GET_SNDBUF_USE:
		if (m->m_len < sizeof(struct sctp_sockstat)) {
			error = EINVAL;
		} else {
			struct sctp_sockstat *ss;
			struct sctp_tcb *stcb;
			struct sctp_association *asoc;
			ss = mtod(m, struct sctp_sockstat *);

			stcb = sctp_findassociation_associd(ss->ss_assoc_id);
			if (stcb == NULL) {
				error = ENOTCONN;
			} else {
				asoc = &stcb->asoc;
				ss->ss_total_sndbuf = (u_int32_t)asoc->total_output_queue_size;
				ss->ss_total_mbuf_sndbuf = (u_int32_t)asoc->total_output_mbuf_queue_size;
				ss->ss_total_recv_buf = (u_int32_t)(asoc->size_on_delivery_queue +
								    asoc->size_on_reasm_queue +
								    asoc->size_on_all_streams);
				error = 0;
				m->m_len = sizeof(struct sctp_sockstat);
			}
		}
		break;
	case SCTP_MAXBURST:
	{
		u_int8_t *burst;
		burst = mtod(m, u_int8_t *);
		*burst = inp->sctp_ep.max_burst;
		m->m_len = sizeof(u_int8_t);
	}
	break;
	case SCTP_MAXSEG:
	{
		u_int32_t *segsize;
		int ovh;
		segsize = mtod(m, u_int32_t *);
		m->m_len = sizeof(u_int32_t);
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
		     (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) ||
		    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
			struct sctp_tcb *stcb;
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb)
				*segsize = sctp_get_frag_point(stcb,&stcb->asoc);
			else
				goto skipit;
		} else {
		skipit:
#endif

			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				ovh = SCTP_MED_OVERHEAD;
			} else {
				ovh = SCTP_MED_V4_OVERHEAD;
			}
			*segsize = inp->sctp_frag_point - ovh;
#ifdef SCTP_TCP_MODEL_SUPPORT
		}
#endif
	}
	break;

	case SCTP_SET_DEBUG_LEVEL:
#ifdef SCTP_DEBUG
	{
		u_int32_t *level;
		if (m->m_len < sizeof(u_int32_t)) {
			error = EINVAL;
			break;
		}
		level = mtod(m, u_int32_t *);
		error = 0;
		*level = sctp_debug_on;
		m->m_len = sizeof(u_int32_t);
		printf("Returning DEBUG LEVEL %x is set\n",
		       (u_int)sctp_debug_on);
	}
#else /* SCTP_DEBUG */
	error = EOPNOTSUPP;
#endif
	break;
	case SCTP_GET_STAT_LOG:
#ifdef SCTP_STAT_LOGGING
		error = sctp_fill_stat_log(m);
#else /* SCTP_DEBUG */
		error = EOPNOTSUPP;
#endif
		break;
	case SCTP_GET_PEGS:
	{
		u_int32_t *pt;
		if (m->m_len < sizeof(sctp_pegs)) {
			error = EINVAL;
			break;
		}
		pt = mtod(m, u_int32_t *);
		memcpy(pt, sctp_pegs, sizeof(sctp_pegs));
		m->m_len = sizeof(sctp_pegs);
	}
	break;
	case SCTP_EVENTS:
	{
		struct sctp_event_subscribe *events;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
			printf("get events\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_event_subscribe)) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
				printf("M->M_LEN is %d not %d\n",
				       (int)m->m_len,
				       (int)sizeof(struct sctp_event_subscribe));
			}
#endif /* SCTP_DEBUG */
			error = EINVAL;
			break;
		}
		events = mtod(m, struct sctp_event_subscribe *);
		memset(events, 0, sizeof(events));

		if (inp->sctp_flags & SCTP_PCB_FLAGS_RECVDATAIOEVNT)
			events->sctp_data_io_event = 1;

		if (inp->sctp_flags & SCTP_PCB_FLAGS_RECVASSOCEVNT)
			events->sctp_association_event = 1;

		if (inp->sctp_flags & SCTP_PCB_FLAGS_RECVPADDREVNT)
			events->sctp_address_event = 1;

		if (inp->sctp_flags & SCTP_PCB_FLAGS_RECVSENDFAILEVNT)
			events->sctp_send_failure_event = 1;

		if (inp->sctp_flags & SCTP_PCB_FLAGS_RECVPEERERR)
			events->sctp_peer_error_event = 1;

		if (inp->sctp_flags & SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT)
			events->sctp_shutdown_event = 1;

		if (inp->sctp_flags & SCTP_PCB_FLAGS_PDAPIEVNT)
			events->sctp_partial_delivery_event = 1;

		if (inp->sctp_flags & SCTP_PCB_FLAGS_ADAPTIONEVNT)
			events->sctp_adaption_layer_event = 1;

		if (inp->sctp_flags & SCTP_PCB_FLAGS_STREAM_RESETEVNT)
			events->sctp_stream_reset_events = 1;

		m->m_len = sizeof(struct sctp_event_subscribe);

	}
	break;

	case SCTP_ADAPTION_LAYER:
		if (m->m_len < sizeof(int)) {
			error = EINVAL;
			break;
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("getadaption ind\n");
		}
#endif /* SCTP_DEBUG */
		*mtod(m, int *) = inp->sctp_ep.adaption_layer_indicator;
		m->m_len = sizeof(int);
		break;
	case SCTP_SET_INITIAL_DBG_SEQ:
		if (m->m_len < sizeof(int)) {
			error = EINVAL;
			break;
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("get initial dbg seq\n");
		}
#endif /* SCTP_DEBUG */
		*mtod(m, int *) = inp->sctp_ep.initial_sequence_debug;
		m->m_len = sizeof(int);
		break;
	case SCTP_GET_LOCAL_ADDR_SIZE:
		if (m->m_len < sizeof(int)) {
			error = EINVAL;
			break;
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("get local sizes\n");
		}
#endif /* SCTP_DEBUG */
		*mtod(m, int *) = sctp_count_max_addresses(inp);
		m->m_len = sizeof(int);
		break;
	case SCTP_GET_REMOTE_ADDR_SIZE:
	{
		sctp_assoc_t *assoc_id;
		u_int32_t *val, sz;
		struct sctp_nets *net;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("get remote size\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(sctp_assoc_t)) {
			error = EINVAL;
			break;
		}
		stcb = NULL;
		val = mtod(m, u_int32_t *);
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		}
#endif /* SCTP_TCP_MODEL_SUPPORT */
		if (stcb == NULL) {
			assoc_id = mtod(m, sctp_assoc_t *);
			stcb = sctp_findassociation_ep_asocid(inp, *assoc_id);
		}
		if (stcb == NULL) {
			error = EINVAL;
			break;
		}
		*val = 0;
		sz = 0;
		/* Count the sizes */
		TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
			if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) ||
			    (((struct sockaddr *)&net->ra._l_addr)->sa_family == AF_INET6)) {
				sz += sizeof(struct sockaddr_in6);
			} else if (((struct sockaddr *)&net->ra._l_addr)->sa_family == AF_INET) {
				sz += sizeof(struct sockaddr_in);
			} else {
				/* huh */
				break;
			}
		}
		*val = sz;
		m->m_len = sizeof(u_int32_t);
	}
	break;
	case SCTP_GET_PEER_ADDRESSES:
		/*
		 * Get the address information, an array
		 * is passed in to fill up we pack it.
		 */
	{
		int cpsz, left;
		struct sockaddr_storage *sas;
		struct sctp_nets *net;
		struct sctp_getaddresses *saddr;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("get peer addresses\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_getaddresses)) {
			error = EINVAL;
			break;
		}
		left = m->m_len - sizeof(struct sctp_getaddresses);
		saddr = mtod(m, struct sctp_getaddresses *);
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		}
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp,
							     saddr->sget_assoc_id);
		if (stcb == NULL) {
			error = ENOENT;
			break;
		}
		m->m_len = sizeof(struct sctp_getaddresses);
		sas = (struct sockaddr_storage *)&saddr->addr[0];
		TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
			if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) ||
			    (((struct sockaddr *)&net->ra._l_addr)->sa_family == AF_INET6)) {
				cpsz = sizeof(struct sockaddr_in6);
			} else if (((struct sockaddr *)&net->ra._l_addr)->sa_family == AF_INET) {
				cpsz = sizeof(struct sockaddr_in);
			} else {
				/* huh */
				break;
			}
			if (left < cpsz) {
				/* not enough room. */
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
					printf("Out of room\n");
				}
#endif /* SCTP_DEBUG */
				break;
			}
			if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
			   (((struct sockaddr *)&net->ra._l_addr)->sa_family == AF_INET)) {
				/* Must map the address */
				in6_sin_2_v4mapsin6((struct sockaddr_in *)&net->ra._l_addr, 
						    (struct sockaddr_in6 *)sas);
			} else {
				memcpy(sas, &net->ra._l_addr, cpsz);
			}
			((struct sockaddr_in *)sas)->sin_port = stcb->rport;

			sas = (struct sockaddr_storage *)((caddr_t)sas + cpsz);
			left -= cpsz;
			m->m_len += cpsz;
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
				printf("left now:%d mlen:%d\n",
				       left, m->m_len);
			}
#endif /* SCTP_DEBUG */
		}
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
		printf("All done\n");
	}
#endif /* SCTP_DEBUG */
	break;
	case SCTP_GET_LOCAL_ADDRESSES:
	{
		int limit, actual;
		struct sockaddr_storage *sas;
		struct sctp_getaddresses *saddr;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("get local addresses\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_getaddresses)) {
			error = EINVAL;
			break;
		}
		saddr = mtod(m, struct sctp_getaddresses *);

		if (saddr->sget_assoc_id) {
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
			else
#endif /* SCTP_TCP_MODEL_SUPPORT */
				stcb = sctp_findassociation_ep_asocid(inp, saddr->sget_assoc_id);
		} else {
			stcb = NULL;
		}
		/*
		 * assure that the TCP model does not need a assoc id 
		 * once connected.
		 */
#ifdef SCTP_TCP_MODEL_SUPPORT
		if ( (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) &&
		     (stcb == NULL) ) {
			stcb = LIST_FIRST(&inp->sctp_asoc_list);			
		}
#endif /* SCTP_TCP_MODEL_SUPPORT */				
		sas = (struct sockaddr_storage *)&saddr->addr[0];
		limit = m->m_len - sizeof(sctp_assoc_t);
		actual = sctp_fill_up_addresses(inp, stcb, limit, sas);
		m->m_len = sizeof(struct sockaddr_storage) + actual;
	}
	break;
	case SCTP_PEER_ADDR_PARAMS:
	{
		struct sctp_paddrparams *paddrp;
		struct sctp_nets *net;

#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("Getting peer_addr_params\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_paddrparams)) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
				printf("Hmm m->m_len:%d is to small\n",
				       m->m_len);
			}
#endif /* SCTP_DEBUG */
			error = EINVAL;
			break;
		}
		paddrp = mtod(m, struct sctp_paddrparams *);
		
		net = NULL;
		if (paddrp->spp_assoc_id) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("In spp_assoc_id find type\n");
			}
#endif /* SCTP_DEBUG */
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					net = sctp_findnet(stcb, (struct sockaddr *)&paddrp->spp_address);
			} else
#endif /* SCTP_TCP_MODEL_SUPPORT */
				stcb = sctp_findassociation_ep_asocid(inp, paddrp->spp_assoc_id);
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
		}
		if (	(stcb == NULL) &&
			((((struct sockaddr *)&paddrp->spp_address)->sa_family == AF_INET) ||
			 (((struct sockaddr *)&paddrp->spp_address)->sa_family == AF_INET6))) {
			/* Lookup via address */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("Ok we need to lookup a param\n");
			}
#endif /* SCTP_DEBUG */
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					net = sctp_findnet(stcb, (struct sockaddr *)&paddrp->spp_address);
			} else
#endif /* SCTP_TCP_MODEL_SUPPORT */
				stcb = sctp_findassociation_ep_addr(&inp,
				    (struct sockaddr *)&paddrp->spp_address,
				    &net, NULL);
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
		} else {
			/* Effects the Endpoint */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("User wants EP level info\n");
			}
#endif /* SCTP_DEBUG */
			stcb = NULL;
		}
		if (stcb) {
			/* Applys to the specific association */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("In TCB side\n");
			}
#endif /* SCTP_DEBUG */
			if (net) {
				paddrp->spp_pathmaxrxt = net->failure_threshold;
			} else {
				/* No destination so return default value */
				paddrp->spp_pathmaxrxt = stcb->asoc.def_net_failure;
			}
			paddrp->spp_hbinterval = stcb->asoc.heart_beat_delay;
			paddrp->spp_assoc_id = sctp_get_associd(stcb);
		} else {
			/* Use endpoint defaults */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("In EP levle info\n");
			}
#endif /* SCTP_DEBUG */
			paddrp->spp_pathmaxrxt = inp->sctp_ep.def_net_failure;
			paddrp->spp_hbinterval = inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_HEARTBEAT];
			paddrp->spp_assoc_id = (sctp_assoc_t)0;
		}
		m->m_len = sizeof(struct sctp_paddrparams);
	}
	break;
	case SCTP_GET_PEER_ADDR_INFO:
	{
		struct sctp_paddrinfo *paddri;
		struct sctp_nets *net;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("GetPEER ADDR_INFO\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_paddrinfo)) {
			error = EINVAL;
			break;
		}
		paddri = mtod(m, struct sctp_paddrinfo *);
		net = NULL;
		if ((((struct sockaddr *)&paddri->spinfo_address)->sa_family == AF_INET) ||
		    (((struct sockaddr *)&paddri->spinfo_address)->sa_family == AF_INET6)) {
			/* Lookup via address */
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					net = sctp_findnet(stcb, 
							    (struct sockaddr *)&paddri->spinfo_address);
			} else {
#endif /* SCTP_TCP_MODEL_SUPPORT */
				stcb = sctp_findassociation_ep_addr(&inp,
				    (struct sockaddr *)&paddri->spinfo_address,
				    &net, NULL);
			}
		} else {
			stcb = NULL;
		}
		if ((stcb == NULL) || (net == NULL)) {
			error = ENOENT;
			break;
		}
		
		m->m_len = sizeof(struct sctp_paddrinfo);
		paddri->spinfo_state = net->dest_state & (SCTP_REACHABLE_MASK|SCTP_ADDR_NOHB);
		paddri->spinfo_cwnd = net->cwnd;
		paddri->spinfo_srtt = ((net->lastsa >> 2) + net->lastsv) >> 1;
		paddri->spinfo_rto = net->RTO;
		paddri->spinfo_assoc_id = sctp_get_associd(stcb);
	}
	break;
	case SCTP_PCB_STATUS:
	{
		struct sctp_pcbinfo *spcb;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("PCB status\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_pcbinfo)) {
			error = EINVAL;
			break;
		}
		spcb = mtod(m, struct sctp_pcbinfo *);
		sctp_fill_pcbinfo(spcb);
		m->m_len = sizeof(struct sctp_pcbinfo);
	}
	break;
	case SCTP_STATUS:
	{
		struct sctp_nets *net;
		struct sctp_status *sstat;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("SCTP status\n");
		}
#endif /* SCTP_DEBUG */

		if (m->m_len < sizeof(struct sctp_status)) {
			error = EINVAL;
			break;
		}
		sstat = mtod(m, struct sctp_status *);
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, sstat->sstat_assoc_id);
		if (stcb == NULL) {
			error = EINVAL;
			break;
		}
		/*
		 * I think passing the state is fine since
		 * sctp_constants.h will be available to the user
		 * land.
		 */
		sstat->sstat_state = stcb->asoc.state;
		sstat->sstat_rwnd = stcb->asoc.peers_rwnd;
		sstat->sstat_unackdata = stcb->asoc.sent_queue_cnt;
		/*
		 * We can't include chunks that have been passed
		 * to the socket layer. Only things in queue.
		 */
		sstat->sstat_penddata = (stcb->asoc.cnt_on_delivery_queue +
					 stcb->asoc.cnt_on_reasm_queue +
					 stcb->asoc.cnt_on_all_streams);


		sstat->sstat_instrms = stcb->asoc.streamincnt;
		sstat->sstat_outstrms = stcb->asoc.streamoutcnt;
		sstat->sstat_fragmentation_point = sctp_get_frag_point(stcb,&stcb->asoc);
		memcpy(&sstat->sstat_primary.spinfo_address,
		       &stcb->asoc.primary_destination->ra._l_addr,
		       ((struct sockaddr *)(&stcb->asoc.primary_destination->ra._l_addr))->sa_len);
		net = stcb->asoc.primary_destination;
		((struct sockaddr_in *)&sstat->sstat_primary.spinfo_address)->sin_port = stcb->rport;
		/*
		 * Again the user can get info from sctp_constants.h
		 * for what the state of the network is.
		 */
		sstat->sstat_primary.spinfo_state = net->dest_state & SCTP_REACHABLE_MASK;
		sstat->sstat_primary.spinfo_cwnd = net->cwnd;
		sstat->sstat_primary.spinfo_srtt = net->lastsa;
		sstat->sstat_primary.spinfo_rto = net->RTO;
		sstat->sstat_primary.spinfo_mtu = net->mtu;
		sstat->sstat_primary.spinfo_assoc_id = sctp_get_associd(stcb);
		m->m_len = sizeof(*sstat);
	}
	break;
	case SCTP_RTOINFO:
	{
		struct sctp_rtoinfo *srto;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("RTO Info\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_rtoinfo)) {
			error = EINVAL;
			break;
		}
		srto = mtod(m, struct sctp_rtoinfo *);
		if (srto->srto_assoc_id == 0) {
			/* Endpoint only please */
			srto->srto_initial = inp->sctp_ep.initial_rto;
			srto->srto_max = inp->sctp_ep.sctp_maxrto;
			srto->srto_min = inp->sctp_ep.sctp_minrto;
			break;
		}
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, srto->srto_assoc_id);
		if (stcb == NULL) {
			error = EINVAL;
			break;
		}
		srto->srto_initial = stcb->asoc.initial_rto;
		srto->srto_max = inp->sctp_ep.sctp_maxrto;
		srto->srto_min = inp->sctp_ep.sctp_minrto;
		m->m_len = sizeof(*srto);
	}
	break;
	case SCTP_ASSOCINFO:
	{
		struct sctp_assocparams *sasoc;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("Associnfo\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_assocparams)) {
			error = EINVAL;
			break;
		}
		sasoc = mtod(m, struct sctp_assocparams *);
		stcb = NULL;
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
#endif /* SCTP_MODEL_SUPPORT */
		if ((sasoc->sasoc_assoc_id) && (stcb == NULL)) {
			stcb = sctp_findassociation_ep_asocid(inp,
							     sasoc->sasoc_assoc_id);
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
		} else {
			stcb = NULL;
		}
		if (stcb) {
			sasoc->sasoc_asocmaxrxt = stcb->asoc.max_send_times;
			sasoc->sasoc_number_peer_destinations = stcb->asoc.numnets;
			sasoc->sasoc_peer_rwnd = stcb->asoc.peers_rwnd;
			sasoc->sasoc_local_rwnd = stcb->asoc.my_rwnd;
			sasoc->sasoc_cookie_life = stcb->asoc.cookie_life;
		} else {
			sasoc->sasoc_asocmaxrxt = inp->sctp_ep.max_send_times;
			sasoc->sasoc_number_peer_destinations = 0;
			sasoc->sasoc_peer_rwnd = 0;
			sasoc->sasoc_local_rwnd = sbspace(&inp->sctp_socket->so_rcv);
			sasoc->sasoc_cookie_life = inp->sctp_ep.def_cookie_life;
		}
		m->m_len = sizeof(*sasoc);
	}
	break;
	case SCTP_DEFAULT_SEND_PARAM:
	{
		struct sctp_sndrcvinfo *s_info;

		if (m->m_len != sizeof(struct sctp_sndrcvinfo)) {
			error = EINVAL;
			break;
		}
		s_info = mtod(m, struct sctp_sndrcvinfo *);
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, s_info->sinfo_assoc_id);
		if (stcb == NULL) {
			error = ENOENT;
			break;
		}
		/* Copy it out */
		*s_info = stcb->asoc.def_send;
		m->m_len = sizeof(*s_info);
	}
	case SCTP_INITMSG:
	{
		struct sctp_initmsg *sinit;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("initmsg\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_initmsg)) {
			error = EINVAL;
			break;
		}
		sinit = mtod(m, struct sctp_initmsg *);
		sinit->sinit_num_ostreams = inp->sctp_ep.pre_open_stream_count;
		sinit->sinit_max_instreams = inp->sctp_ep.max_open_streams_intome;
		sinit->sinit_max_attempts = inp->sctp_ep.max_init_times;
		sinit->sinit_max_init_timeo = inp->sctp_ep.initial_init_rto_max;
		m->m_len = sizeof(*sinit);
	}
	break;
	case SCTP_PRIMARY_ADDR:
		/* we allow a "get" operation on this */
	{
		struct sctp_setprim *ssp;

#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("setprimary\n");
		}
#endif /* SCTP_DEBUG */
		if (m->m_len < sizeof(struct sctp_setprim)) {
			error = EINVAL;
			break;
		}
		ssp = mtod(m, struct sctp_setprim *);
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, ssp->ssp_assoc_id);
		if (stcb == NULL) {
			/* one last shot, try it by the address in */
			struct sctp_nets *net;
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
			else
#endif
				stcb = sctp_findassociation_ep_addr(&inp,
				    (struct sockaddr *)&ssp->ssp_addr,
				    &net, NULL);
			if (stcb == NULL) {
				error = EINVAL;
				break;
			}
		}
		/* simply copy out the sockaddr_storage... */
		memcpy(&ssp->ssp_addr,
		       &stcb->asoc.primary_destination->ra._l_addr,
		       ((struct sockaddr *)&stcb->asoc.primary_destination->ra._l_addr)->sa_len);

		m->m_len = sizeof(*ssp);
	}
	break;
	default:
		error = ENOPROTOOPT;
		m->m_len = 0;
		break;
	} /* end switch (sopt->sopt_name) */
        return (error);
}

static int
sctp_optsset(struct socket *so,
	     int opt,
	     struct mbuf **mp,
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	     struct thread *p
#else
	     struct proc *p
#endif
	)
{
	int error, *mopt, set_opt, s;
	struct mbuf *m;
	struct sctp_tcb *stcb = NULL;
        struct sctp_inpcb *inp;

	if (mp == NULL) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("optsset:MP is NULL EINVAL\n");
		}
#endif /* SCTP_DEBUG */
		return (EINVAL);
	}
	m = *mp;
	if (m == NULL)
		return (EINVAL);

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return EINVAL;

	error = 0;
	switch (opt) {
	case SCTP_NODELAY:
	case SCTP_AUTOCLOSE:
	case SCTP_AUTO_ASCONF:
	case SCTP_DISABLE_FRAGMENTS:
	case SCTP_I_WANT_MAPPED_V4_ADDR:
		/* copy in the option value */
		if (m->m_len < sizeof(int)) {
			error = EINVAL;
			break;
		}
		mopt = mtod(m, int *);
		set_opt = 0;
		if (error)
			break;
		switch (opt) {
		case SCTP_DISABLE_FRAGMENTS:
			set_opt = SCTP_PCB_FLAGS_NO_FRAGMENT;
			break;
		case SCTP_AUTO_ASCONF:
			set_opt = SCTP_PCB_FLAGS_AUTO_ASCONF;
			break;

		case SCTP_I_WANT_MAPPED_V4_ADDR:
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				set_opt = SCTP_PCB_FLAGS_NEEDS_MAPPED_V4;
			} else {
				return (EINVAL);
			}
			break;
		case SCTP_NODELAY:
			set_opt = SCTP_PCB_FLAGS_NODELAY;
			break;
		case SCTP_AUTOCLOSE:
			set_opt = SCTP_PCB_FLAGS_AUTOCLOSE;
			/*
			 * The value is in ticks.
			 * Note this does not effect old associations, only
			 * new ones.
			 */
			inp->sctp_ep.auto_close_time = (*mopt * hz);
			break;
		}
		if (*mopt != 0) {
			inp->sctp_flags |= set_opt;
		} else {
			inp->sctp_flags &= ~set_opt;
		}
		break;
	case SCTP_MY_PUBLIC_KEY:    /* set my public key */
	case SCTP_SET_AUTH_CHUNKS:  /* set the authenticated chunks required */
	case SCTP_SET_AUTH_SECRET:  /* set the actual secret for the endpoint */
		/* not supported yet and until we refine the draft */
		error = EOPNOTSUPP;
		break;

	case SCTP_CLR_STAT_LOG:
#ifdef SCTP_STAT_LOGGING
		sctp_clr_stat_log();
#else
		error = EOPNOTSUPP;
#endif
		break;
	case SCTP_RESET_STREAMS:
	{
		struct sctp_stream_reset *strrst;
		uint8_t two_way, not_peer;

		if (m->m_len < sizeof(struct sctp_stream_reset)) {
			error = EINVAL;
			break;
		}
		strrst = mtod(m, struct sctp_stream_reset *);
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		} else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, strrst->strrst_assoc_id);
		if (stcb == NULL) {
			error = ENOENT;
			break;
		}
		if (stcb->asoc.peer_supports_strreset == 0) {
			/* Peer does not support it,
			 * we return protocol not supported since
			 * this is true for this feature and this
			 * peer, not the socket request in general.
			 */
			error = EPROTONOSUPPORT;
			break;
		}

		if (stcb->asoc.stream_reset_outstanding) {
			error = EALREADY;
			break;
		}
		if (strrst->strrst_flags == SCTP_RESET_LOCAL_RECV) {
			two_way = 0;
			not_peer = 0;
		} else if (strrst->strrst_flags == SCTP_RESET_LOCAL_SEND) {
			two_way = 1;
			not_peer = 1;
		} else if (strrst->strrst_flags == SCTP_RESET_BOTH) {
			two_way = 1;
			not_peer = 0;
		} else {
			error = EINVAL;
			break;
		}
		sctp_send_str_reset_req(stcb, strrst->strrst_num_streams,
		    strrst->strrst_list, two_way, not_peer);
#if defined(__NetBSD__) || defined(__OpenBSD__)
		s = splsoftnet();
#else
		s = splnet();
#endif
		sctp_chunk_output(inp, stcb, 12);
		splx(s);
		
	}
	break;
	case SCTP_RESET_PEGS:
		memset(sctp_pegs,0,sizeof(sctp_pegs));
		error = 0;
		break;
	case SCTP_CONNECT_X:
		if (m->m_len < (sizeof(int) + sizeof(struct sockaddr_in))) {
			error = EINVAL;
			break;
		}
		error = sctp_do_connect_x(so, inp, m, p);
		break;

	case SCTP_MAXBURST:
	{
		u_int8_t *burst;
		burst = mtod(m, u_int8_t *);
		if (*burst) {
			inp->sctp_ep.max_burst = *burst;
		}
	}
	break;
	case SCTP_MAXSEG:
	{
		u_int32_t *segsize;
		int ovh;
		if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
			ovh = SCTP_MED_OVERHEAD;
		} else {
			ovh = SCTP_MED_V4_OVERHEAD;
		}
		segsize = mtod(m, u_int32_t *);
		if (*segsize < 1) {
			error = EINVAL;
			break;
		}
		inp->sctp_frag_point = (*segsize+ovh);
		if (inp->sctp_frag_point < MHLEN) {
			inp->sctp_frag_point = MHLEN;
		}
	}
	break;
	case SCTP_SET_DEBUG_LEVEL:
#ifdef SCTP_DEBUG
	{
		u_int32_t *level;
		if (m->m_len < sizeof(u_int32_t)) {
			error = EINVAL;
			break;
		}
		level = mtod(m, u_int32_t *);
		error = 0;
		sctp_debug_on = (*level & (SCTP_DEBUG_ALL |
					   SCTP_DEBUG_NOISY));
		printf("SETTING DEBUG LEVEL to %x\n",
		       (u_int)sctp_debug_on);

	}
#else
	error = EOPNOTSUPP;
#endif /* SCTP_DEBUG */
	break;
	case SCTP_EVENTS:
	{
		struct sctp_event_subscribe *events;
		if (m->m_len < sizeof(struct sctp_event_subscribe)) {
			error = EINVAL;
			break;
		}
		events = mtod(m, struct sctp_event_subscribe *);
		if (events->sctp_data_io_event) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_RECVDATAIOEVNT;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_RECVDATAIOEVNT;
		}

		if (events->sctp_association_event) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_RECVASSOCEVNT;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_RECVASSOCEVNT;
		}

		if (events->sctp_address_event) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_RECVPADDREVNT;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_RECVPADDREVNT;
		}

		if (events->sctp_send_failure_event) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_RECVSENDFAILEVNT;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_RECVSENDFAILEVNT;
		}

		if (events->sctp_peer_error_event) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_RECVPEERERR;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_RECVPEERERR;
		}

		if (events->sctp_shutdown_event) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT;
		}

		if (events->sctp_partial_delivery_event) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_PDAPIEVNT;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_PDAPIEVNT;
		}

		if (events->sctp_adaption_layer_event) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_ADAPTIONEVNT;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_ADAPTIONEVNT;
		}

		if (events->sctp_stream_reset_events) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_STREAM_RESETEVNT;
		} else {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_STREAM_RESETEVNT;
		}
	}
	break;

	case SCTP_ADAPTION_LAYER:
	{
		struct sctp_setadaption *adap_bits;
		if (m->m_len < sizeof(struct sctp_setadaption)) {
			error = EINVAL;
			break;
		}
		adap_bits = mtod(m, struct sctp_setadaption *);
		inp->sctp_ep.adaption_layer_indicator = adap_bits->ssb_adaption_ind;
	}
	break;
	case SCTP_SET_INITIAL_DBG_SEQ:
	{
		u_int32_t *vvv;
		if (m->m_len < sizeof(u_int32_t)) {
			error = EINVAL;
			break;
		}
		vvv = mtod(m, u_int32_t *);
		inp->sctp_ep.initial_sequence_debug = *vvv;
	}
	break;
	case SCTP_DEFAULT_SEND_PARAM:
	{
		struct sctp_sndrcvinfo *s_info;

		if (m->m_len != sizeof(struct sctp_sndrcvinfo)) {
			error = EINVAL;
			break;
		}
		s_info = mtod(m, struct sctp_sndrcvinfo *);
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, s_info->sinfo_assoc_id);
		if (stcb == NULL) {
			error = ENOENT;
			break;
		}
		/* Validate things */
		if (s_info->sinfo_stream > stcb->asoc.streamoutcnt) {
			error = EINVAL;
			break;
		}
		/* Mask off the flags that are allowed */
		s_info->sinfo_flags = (s_info->sinfo_flags &
				       (MSG_UNORDERED | MSG_ADDR_OVER |
					MSG_PR_SCTP_TTL | MSG_PR_SCTP_BUF));
		/* Copy it in */
		stcb->asoc.def_send = *s_info;
	}
	break;
	case SCTP_PEER_ADDR_PARAMS:
	{
		struct sctp_paddrparams *paddrp;
		struct sctp_nets *net;
		if (m->m_len < sizeof(struct sctp_paddrparams)) {
			error = EINVAL;
			break;
		}
		paddrp = mtod(m, struct sctp_paddrparams *);
		net = NULL;
		if (paddrp->spp_assoc_id) {
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					net = sctp_findnet(stcb, (struct sockaddr *)&paddrp->spp_address);
			} else
#endif /* SCTP_TCP_MODEL_SUPPORT */
				stcb = sctp_findassociation_ep_asocid(inp, paddrp->spp_assoc_id);
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
		}
		if ((stcb == NULL) && 
		    ((((struct sockaddr *)&paddrp->spp_address)->sa_family == AF_INET) ||
		     (((struct sockaddr *)&paddrp->spp_address)->sa_family == AF_INET6))) {
			/* Lookup via address */
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb) {
					net = sctp_findnet(stcb, 
							    (struct sockaddr *)&paddrp->spp_address);
				}
			} else
#endif
				stcb = sctp_findassociation_ep_addr(&inp,
				    (struct sockaddr *)&paddrp->spp_address,
				    &net, NULL);
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
		} else {
			/* Effects the Endpoint */
			stcb = NULL;
		}
		if (stcb) {
			/* Applies to the specific association */
			if (paddrp->spp_pathmaxrxt) {
				if (net) {
					if (paddrp->spp_pathmaxrxt)
						net->failure_threshold = paddrp->spp_pathmaxrxt;
				} else {
					if (paddrp->spp_pathmaxrxt)
						stcb->asoc.def_net_failure = paddrp->spp_pathmaxrxt;
				}
			}
			if ((paddrp->spp_hbinterval != 0) && (paddrp->spp_hbinterval != 0xffffffff)) {
				/* Just a set */
				int old;
				if (net) {
					net->dest_state &= ~SCTP_ADDR_NOHB;
				} else {
					old = stcb->asoc.heart_beat_delay;
					stcb->asoc.heart_beat_delay = paddrp->spp_hbinterval;
					if (old == 0) {
						/* Turn back on the timer */
						sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
					}
				}
			} else if (paddrp->spp_hbinterval == 0xffffffff) {
				/* on demand HB */
				sctp_send_hb(stcb, 1, net);
			} else {
				if (net == NULL) {
					/* off on association */
					if (stcb->asoc.heart_beat_delay) {
						int cnt_of_unconf = 0;
						struct sctp_nets *lnet;
						TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {
							if (lnet->dest_state & SCTP_ADDR_UNCONFIRMED){
								cnt_of_unconf++;
							}
						}
						/* stop the timer ONLY if we have no unconfirmed addresses
						 */
						if (cnt_of_unconf == 0) 
							sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
					}
					stcb->asoc.heart_beat_delay = 0;
				} else {
					net->dest_state |= SCTP_ADDR_NOHB;
				}
			}
		} else {
			/* Use endpoint defaults */
			if (paddrp->spp_pathmaxrxt)
				inp->sctp_ep.def_net_failure = paddrp->spp_pathmaxrxt;
			if (paddrp->spp_hbinterval != SCTP_ISSUE_HB)
				inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_HEARTBEAT] = paddrp->spp_hbinterval;
		}
	}
	break;
	case SCTP_RTOINFO:
	{
		struct sctp_rtoinfo *srto;
		if (m->m_len < sizeof(struct sctp_rtoinfo)) {
			error = EINVAL;
			break;
		}
		srto = mtod(m, struct sctp_rtoinfo *);
		if (srto->srto_assoc_id == 0) {
			/* If we have a null asoc, its for the endpoint */
			if (srto->srto_initial > 10)
				inp->sctp_ep.initial_rto = srto->srto_initial;
			if (srto->srto_max > 10)
				inp->sctp_ep.sctp_maxrto = srto->srto_max;
			if (srto->srto_min > 10)
				inp->sctp_ep.sctp_minrto = srto->srto_min;
			break;
		}
#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, srto->srto_assoc_id);
		if (stcb == NULL) {
			error = EINVAL;
			break;
		}
		/* Set in ms we hope :-) */
		if (srto->srto_initial > 10)
			stcb->asoc.initial_rto = srto->srto_initial;
		if (srto->srto_max > 10)
			inp->sctp_ep.sctp_maxrto = srto->srto_max;
		if (srto->srto_min > 10)
			inp->sctp_ep.sctp_minrto = srto->srto_min;
	}
	break;
	case SCTP_ASSOCINFO:
	{
		struct sctp_assocparams *sasoc;

		if (m->m_len < sizeof(struct sctp_assocparams)) {
			error = EINVAL;
			break;
		}
		sasoc = mtod(m, struct sctp_assocparams *);
		if ((sasoc->sasoc_assoc_id) && (stcb == NULL)){
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
			else
#endif /* SCTP_TCP_MODEL_SUPPORT */
				stcb = sctp_findassociation_ep_asocid(inp,
								     sasoc->sasoc_assoc_id);
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}

		} else {
			stcb = NULL;
		}
		if (stcb) {
			if (sasoc->sasoc_asocmaxrxt)
				stcb->asoc.max_send_times = sasoc->sasoc_asocmaxrxt;
			sasoc->sasoc_number_peer_destinations = stcb->asoc.numnets;
			sasoc->sasoc_peer_rwnd = 0;
			sasoc->sasoc_local_rwnd = 0;
			if (stcb->asoc.cookie_life)
				stcb->asoc.cookie_life = sasoc->sasoc_cookie_life;
		} else {
                        if (sasoc->sasoc_asocmaxrxt)
				inp->sctp_ep.max_send_times = sasoc->sasoc_asocmaxrxt;
			sasoc->sasoc_number_peer_destinations = 0;
			sasoc->sasoc_peer_rwnd = 0;
			sasoc->sasoc_local_rwnd = 0;
			if (sasoc->sasoc_cookie_life)
				inp->sctp_ep.def_cookie_life = sasoc->sasoc_cookie_life;
		}
	}
	break;
	case SCTP_INITMSG:
	{
                struct sctp_initmsg *sinit;

		if (m->m_len < sizeof(struct sctp_initmsg)) {
			error = EINVAL;
			break;
		}
		sinit = mtod(m, struct sctp_initmsg *);
		if (sinit->sinit_num_ostreams)
			inp->sctp_ep.pre_open_stream_count = sinit->sinit_num_ostreams;

		if (sinit->sinit_max_instreams)
			inp->sctp_ep.max_open_streams_intome = sinit->sinit_max_instreams;

		if (sinit->sinit_max_attempts)
			inp->sctp_ep.max_init_times = sinit->sinit_max_attempts;

		if (sinit->sinit_max_init_timeo > 10)
				/* We must be at least a 100ms (we set in ticks) */
			inp->sctp_ep.initial_init_rto_max = sinit->sinit_max_init_timeo;
	}
	break;
	case SCTP_PRIMARY_ADDR:
	{
		struct sctp_setprim *spa;
		struct sctp_nets *net, *lnet;
		if (m->m_len < sizeof(struct sctp_setprim)) {
			error = EINVAL;
			break;
		}
		spa = mtod(m, struct sctp_setprim *);

#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, spa->ssp_assoc_id);
		if (stcb == NULL) {
			/* One last shot */
#ifdef SCTP_TCP_MODEL_SUPPORT
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
			else
#endif /* SCTP_TCP_MODEL_SUPPORT */
				stcb = sctp_findassociation_ep_addr(&inp,
				    (struct sockaddr *)&spa->ssp_addr,
				    &net, NULL);
			if (stcb == NULL) {
				error = EINVAL;
				break;
			}
		} else {
			net = sctp_findnet(stcb,(struct sockaddr *)&spa->ssp_addr);
			if (net == NULL) {
				error = EINVAL;
				break;
			}
                }
                if ((net != stcb->asoc.primary_destination) &&
  		    (!(net->dest_state & SCTP_ADDR_UNCONFIRMED))) {
			/* Ok we need to set it */
			lnet = stcb->asoc.primary_destination;
                        lnet->next_tsn_at_change = net->next_tsn_at_change = stcb->asoc.sending_seq;
		        if (sctp_set_primary_addr(stcb, 
			                         (struct sockaddr *)NULL, 
			                         net) == 0) {
			        if (net->dest_state & SCTP_ADDR_SWITCH_PRIMARY) {
   				        net->dest_state |= SCTP_ADDR_DOUBLE_SWITCH;
                                }
                                net->dest_state |= SCTP_ADDR_SWITCH_PRIMARY;
                        }

               }
        }
	break;

	case SCTP_SET_PEER_PRIMARY_ADDR:
	{
		struct sctp_setpeerprim *sspp;
		if (m->m_len < sizeof(struct sctp_setpeerprim)) {
			error = EINVAL;
			break;
		}
		sspp = mtod(m, struct sctp_setpeerprim *);

#ifdef SCTP_TCP_MODEL_SUPPORT
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
		else
#endif /* SCTP_TCP_MODEL_SUPPORT */
			stcb = sctp_findassociation_ep_asocid(inp, sspp->sspp_assoc_id);
		if (stcb == NULL) {
			error = EINVAL;
			break;
		}
		if (sctp_set_primary_ip_address_sa(stcb, (struct sockaddr *)&sspp->sspp_addr) != 0) {
			error = EINVAL;
			break;
		}
	}
	break;
	case SCTP_BINDX_ADD_ADDR:
	{
		struct sctp_getaddresses *addrs;

		/* see if we're bound all already! */
		if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
			error = EINVAL;
			break;
		}
		if (m->m_len < sizeof(struct sctp_getaddresses)) {
			error = EINVAL;
			break;
		}
		addrs = mtod(m, struct sctp_getaddresses *);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) {
				if (p == NULL) {
					/* Can't get proc for Net/Open BSD */
					error = EINVAL;
					break;
				}
				error = sctp_inpcb_bind(so,
							(struct sockaddr *)addrs->addr,
							p);
				break;
		}
		if (addrs->sget_assoc_id == 0) {
			/* add the address */
			struct sctp_inpcb  *lep;

			((struct sockaddr_in *)addrs->addr)->sin_port = inp->sctp_lport;
			lep = sctp_pcb_findep(addrs->addr, 1);
			if (lep == inp) {
				/* already bound to it.. ok */
				break;
			} else if (lep == NULL) {
				((struct sockaddr_in *)addrs->addr)->sin_port = 0;
				error = sctp_addr_mgmt_ep_sa(inp, (struct sockaddr *)addrs->addr,
							     SCTP_ADD_IP_ADDRESS);
			} else {
				error = EADDRNOTAVAIL;
			}
			if (error)
				break;
			
		} else {
			/* FIX: decide whether we allow assoc based bindx */
		}
	}
	break;
	case SCTP_BINDX_REM_ADDR:
	{
		struct sctp_getaddresses *addrs;
		/* see if we're bound all already! */
		if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
			error = EINVAL;
			break;
		}
		if (m->m_len < sizeof(struct sctp_getaddresses)) {
			error = EINVAL;
			break;
		}
		addrs = mtod(m, struct sctp_getaddresses *);
		if (addrs->sget_assoc_id == 0) {
				/* delete the address */
			sctp_addr_mgmt_ep_sa(inp, addrs->addr,
					     SCTP_DEL_IP_ADDRESS);
		} else {
			/* FIX: decide whether we allow assoc based bindx */
		}
	}
	break;
	default:
		error = ENOPROTOOPT;
		break;
	} /* end switch (opt) */
	return (error);
}


#if defined(__FreeBSD__) || defined(__APPLE__)
int
sctp_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct mbuf *m = NULL;
	struct sctp_inpcb *inp;
	int s, error;

	inp = (struct sctp_inpcb *)so->so_pcb;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	if (inp == 0) {
		splx(s);
		/* I made the same as TCP since we are not setup? */
		return (ECONNRESET);
	}
	if (sopt->sopt_level != IPPROTO_SCTP) {
		/* wrong proto level... send back up to IP */
#ifdef INET6
		if (INP_CHECK_SOCKAF(so, AF_INET6))
			error = ip6_ctloutput(so, sopt);
		else
#endif /* INET6 */
			error = ip_ctloutput(so, sopt);
		splx(s);
		return (error);
	}
	if (sopt->sopt_valsize > MCLBYTES) {
		/*
		 * Restrict us down to a cluster size, that's all we can
		 * pass either way...
		 */
		sopt->sopt_valsize = MCLBYTES;
	}
	if (sopt->sopt_valsize) {

		m = m_get(M_WAIT, MT_DATA);
		if (sopt->sopt_valsize > MLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				sctp_m_freem(m);
				splx(s);
				return (ENOBUFS);
			}
		}
		error = sooptcopyin(sopt, mtod(m, caddr_t), sopt->sopt_valsize,
				    sopt->sopt_valsize);
		if (error) {
			(void) m_free(m);
			goto out;
		}
		m->m_len = sopt->sopt_valsize;
	}
	if (sopt->sopt_dir == SOPT_SET) {
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
		error = sctp_optsset(so, sopt->sopt_name, &m, sopt->sopt_td);
#else
		error = sctp_optsset(so, sopt->sopt_name, &m, sopt->sopt_p);
#endif
	} else if (sopt->sopt_dir == SOPT_GET) {
#if defined (__FreeBSD__) && __FreeBSD_version >= 500000
		error = sctp_optsget(so, sopt->sopt_name, &m, sopt->sopt_td);
#else
		error = sctp_optsget(so, sopt->sopt_name, &m, sopt->sopt_p);
#endif
	} else {
		error = EINVAL;
	}
	if ( (error == 0) && (m != NULL)) {
		error = sooptcopyout(sopt, mtod(m, caddr_t), m->m_len);
		sctp_m_freem(m);
	}
 out:
	splx(s);
	return (error);
}

#else
/* NetBSD and OpenBSD */
int
sctp_ctloutput(op, so, level, optname, mp)
     int op;
     struct socket *so;
     int level, optname;
     struct mbuf **mp;
{
	int s, error;
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
#endif
	int family;	/* family of the socket */

	family = so->so_proto->pr_domain->dom_family;
	error = 0;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	switch (family) {
	case PF_INET:
		inp = sotoinpcb(so);
#ifdef INET6
		in6p = NULL;
#endif
		break;
#ifdef INET6
	case PF_INET6:
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		splx(s);
		return EAFNOSUPPORT;
	}
#ifndef INET6
	if (inp == NULL)
#else
		if (inp == NULL && in6p == NULL)
#endif
		{
			splx(s);
			if (op == PRCO_SETOPT && *mp)
				(void) m_free(*mp);
			return (ECONNRESET);
		}
	if (level != IPPROTO_SCTP) {
		switch (family) {
		case PF_INET:
			error = ip_ctloutput(op, so, level, optname, mp);
			break;
#ifdef INET6
		case PF_INET6:
			error = ip6_ctloutput(op, so, level, optname, mp);
			break;
#endif
		}
		splx(s);
		return (error);
	}
	/* Ok if we reach here it is a SCTP option we hope */
	if (op == PRCO_SETOPT) {
		error = sctp_optsset(so, optname, mp,(struct proc *)NULL);
		if (*mp)
			(void) m_free(*mp);
	} else if (op ==  PRCO_GETOPT) {
		error = sctp_optsget(so, optname, mp,(struct proc *)NULL);
	} else {
		error = EINVAL;
	}
	splx(s);
	return (error);
}

#endif

static int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_connect(struct socket *so, struct sockaddr *addr, struct thread *p)
{
#else
#if defined(__FreeBSD__) || defined(__APPLE__)
sctp_connect(struct socket *so, struct sockaddr *addr, struct proc *p)
{
#else
sctp_connect(struct socket *so, struct mbuf *nam, struct proc *p)
{
	struct sockaddr *addr = mtod(nam, struct sockaddr *);
#endif
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int s = splsoftnet();
#else
	int s = splnet();
#endif
	int error = 0;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Connect called in SCTP to ");
		sctp_print_address(addr);
		printf("Port %d\n", ntohs(((struct sockaddr_in *)addr)->sin_port));
	}
#endif /* SCTP_DEBUG */

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		splx(s);
		/* I made the same as TCP since we are not setup? */
		return (ECONNRESET);
	}
#ifdef INET6
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) &&
	    (addr->sa_family == AF_INET6)) {
		splx(s);
		return (EINVAL);
	}
#endif /* INET6 */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) ==
	    SCTP_PCB_FLAGS_UNBOUND) {
		/* Bind a ephemeral port */
		error = sctp_inpcb_bind(so, NULL, p);
		if (error) {
			splx(s);
			return (error);
		}
	}

	/* Now do we connect? */
#ifdef SCTP_TCP_MODEL_SUPPORT
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		splx(s);
		return (EADDRINUSE);
	}
#endif /* SCTP_TCP_MODEL_SUPPORT */
#ifdef SCTP_TCP_MODEL_SUPPORT
	if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
	else
#endif /* SCTP_TCP_MODEL_SUPPORT */
		stcb = sctp_findassociation_ep_addr(&inp, addr, NULL, NULL);

	if (stcb != NULL) {
		/* Already have or am bring up an association */
		splx(s);
		return (EALREADY);
	}
	/* We are GOOD to go */
	stcb = sctp_aloc_assoc(inp, addr, 1, &error);
	if (stcb == NULL) {
		/* Gak! no memory */
		splx(s);
		return (error);
	}
#ifdef SCTP_TCP_MODEL_SUPPORT
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		/* Set the connected flag so we can queue data */
		soisconnecting(so);
	}
#endif
	stcb->asoc.state = SCTP_STATE_COOKIE_WAIT;
	SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);
	sctp_send_initiate(inp, stcb);
	splx(s);
	return error;
}

int
sctp_usr_recvd(struct socket *so, int flags)
{
	int s;
	struct sctp_socket_q_list *sq;
	/*
	 * The user has received some data, we may be able to stuff more
	 * up the socket. And we need to possibly update the rwnd.
	 */
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	
	inp = (struct sctp_inpcb *)so->so_pcb;
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_USRREQ2)
		printf("Read for so:%x inp:%x Flags:%x\n",
		       (u_int)so, (u_int)inp, (u_int)flags);
#endif

	if (inp == 0) {
		/* I made the same as TCP since we are not setup? */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ2)
			printf("Nope, connection reset\n");
#endif
		return (ECONNRESET);
	}
#if defined(__NetBSD__) || defined(__OpenBSD__)
		s = splsoftnet();
#else
		s = splnet();
#endif
	/*
	 * Grab the first one on the list. It will re-insert itself if
	 * it runs out of room
	 */
#ifdef SCTP_TCP_MODEL_SUPPORT
	if (flags & MSG_EOR) {
		/* Ok the other part of our grubby tracking
		 * stuff for our horrible layer violation that
		 * the tsvwg thinks is ok for sctp_peeloff.. gak!
		 * We must update the next vtag pending on the
		 * socket buffer (if any).
		 */
		inp->sctp_vtag_last = sctp_get_last_vtag_from_sb(so);
	}
#endif
	sq = TAILQ_FIRST(&inp->sctp_queue_list);
	if (sq) {
		stcb = sq->tcb;
	} else {
		stcb = NULL;
	}
	if (stcb) {
		long incr;
		if (flags & MSG_EOR) {
			stcb = sctp_remove_from_socket_q(inp);
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ2)
				printf("remove from socket queue for inp:%x tcbret:%x\n",
				       (u_int)inp, (u_int)stcb);
#endif

 			stcb->asoc.my_rwnd_control_len = sctp_sbspace_sub(stcb->asoc.my_rwnd_control_len,
 									  sizeof(struct mbuf));
			if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_RECVDATAIOEVNT) {
 				stcb->asoc.my_rwnd_control_len = sctp_sbspace_sub(stcb->asoc.my_rwnd_control_len,
 										  CMSG_LEN(sizeof(struct sctp_sndrcvinfo)));
			}
		} 
		if ((TAILQ_EMPTY(&stcb->asoc.delivery_queue) == 0) ||
		    (TAILQ_EMPTY(&stcb->asoc.reasmqueue) == 0)) {
			/* Deliver if there is something to be delivered */
			sctp_service_queues(stcb, &stcb->asoc);
		}
		sctp_set_rwnd(stcb, &stcb->asoc);
		/* if we increase by 1 or more MTU's (smallest MTUs of all
		 * nets) we send a window update sack
		 */
		incr = stcb->asoc.my_rwnd - stcb->asoc.my_last_reported_rwnd;
		if (incr < 0) {
			incr = 0;
		}
		if ((incr >= (stcb->asoc.smallest_mtu * SCTP_SEG_TO_RWND_UPD)) ||
		    ((incr*SCTP_SCALE_OF_RWND_TO_UPD) >= so->so_rcv.sb_hiwat)) {
			if (callout_pending(&stcb->asoc.dack_timer.timer)) {
				/* If the timer is up, stop it */
				sctp_timer_stop(SCTP_TIMER_TYPE_RECV,
						stcb->sctp_ep, stcb, NULL);
			}
			/* Send the sack, with the new rwnd */
			sctp_send_sack(stcb);
			/* Now do the output */
			sctp_chunk_output(inp, stcb, 10);
		}
	} else {
		if (( sq ) && (flags & MSG_EOR)) {
			stcb = sctp_remove_from_socket_q(inp);
		}
	}
	if (( so->so_rcv.sb_mb == NULL ) &&
	    (TAILQ_EMPTY(&inp->sctp_queue_list) == 0)) {
		int sq_cnt=0;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ2)
			printf("Something off, inp:%x so->so_rcv->sb_mb is empty and sockq is not.. cleaning\n",
			       (u_int)inp);
#endif
		while (TAILQ_EMPTY(&inp->sctp_queue_list) == 0) {
			sq_cnt++;
			(void)sctp_remove_from_socket_q(inp);
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ2)
			printf("Cleaned up %d sockq's\n", sq_cnt);
#endif
	}

	splx(s);
	return (0);
}

int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_listen(struct socket *so, struct thread *p)
#else
sctp_listen(struct socket *so, struct proc *p)
#endif
{
	/*
	 * Note this module depends on the protocol processing being
	 * called AFTER any socket level flags and backlog are applied
	 * to the socket. The traditional way that the socket flags are
	 * applied is AFTER protocol processing. We have made a change
	 * to the sys/kern/uipc_socket.c module to reverse this but this
	 * MUST be in place if the socket API for SCTP is to work properly.
	 */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int s = splsoftnet();
#else
	int s = splnet();
#endif
	int error = 0;
	struct sctp_inpcb *inp;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		splx(s);
		/* I made the same as TCP since we are not setup? */
		return (ECONNRESET);
	}
#ifdef SCTP_TCP_MODEL_SUPPORT
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		splx(s);
		return (EADDRINUSE);
	}
#endif /* SCTP_TCP_MODEL_SUPPORT */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) {
		/* We must do a bind. */
		if ((error = sctp_inpcb_bind(so, NULL, p))) {
			/* bind error, probably perm */
			splx(s);
			return (error);
		}
	}
	if (inp->sctp_socket->so_qlimit) {
		if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
			/*
			 * For the UDP model we must TURN OFF the ACCEPT
			 * flags since we do NOT allow the accept() call.
			 * The TCP model (when present) will do accept which
			 * then prohibits connect().
			 */
			inp->sctp_socket->so_options &= ~SO_ACCEPTCONN;
		}
		inp->sctp_flags |= SCTP_PCB_FLAGS_ACCEPTING;
	} else {
		if (inp->sctp_flags & SCTP_PCB_FLAGS_ACCEPTING) {
			/*
			 * Turning off the listen flags if the backlog is
			 * set to 0 (i.e. qlimit is 0).
			 */
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_ACCEPTING;
			inp->sctp_socket->so_options &= ~SO_ACCEPTCONN;
		}
	}
	splx(s);
	return (error);
}

int 
#if defined(__FreeBSD__) || defined(__APPLE__)
sctp_accept(struct socket *so, struct sockaddr **addr)
{
#else
sctp_accept(struct socket *so, struct mbuf *nam)
{
	struct sockaddr *addr = mtod(nam, struct sockaddr *);
#endif
#ifdef SCTP_TCP_MODEL_SUPPORT
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int s = splsoftnet();
#else
	int s = splnet();
#endif
	struct sctp_tcb *stcb;
	struct sockaddr *prim;
	struct sctp_inpcb *inp;

	inp = (struct sctp_inpcb *)so->so_pcb;

	if (inp == 0) {
		splx(s);
		return (ECONNRESET);
	}
	if (so->so_state & SS_ISDISCONNECTED) {
		splx(s);
		return (ECONNABORTED);
	}
	if (inp == 0) {
		splx(s);
		return (EINVAL);
	}
	stcb = LIST_FIRST(&inp->sctp_asoc_list);
	if (stcb == NULL) {
		splx(s);
		return (ECONNRESET);
	}
	prim = (struct sockaddr *)&stcb->asoc.primary_destination->ra._l_addr;
	if (prim->sa_family == AF_INET) {
		struct sockaddr_in *sin;
#if defined(__FreeBSD__) || defined(__APPLE__)
		MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME,
		       M_WAITOK | M_ZERO);
#else
		sin = (struct sockaddr_in *)addr;
		bzero((caddr_t)sin, sizeof (*sin));
#endif
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_port = ((struct sockaddr_in *)prim)->sin_port;
		sin->sin_addr = ((struct sockaddr_in *)prim)->sin_addr;
#if defined(__FreeBSD__) || defined(__APPLE__)
		*addr = (struct sockaddr *)sin;
#else
		nam->m_len = sizeof(*sin);
#endif
	} else {
		struct sockaddr_in6 *sin6;
#if defined(__FreeBSD__) || defined(__APPLE__)
		MALLOC(sin6, struct sockaddr_in6 *, sizeof *sin6, M_SONAME,
		       M_WAITOK | M_ZERO);
#else
		sin6 = (struct sockaddr_in6 *)addr;
#endif
		bzero((caddr_t)sin6, sizeof (*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_port = ((struct sockaddr_in6 *)prim)->sin6_port;

		sin6->sin6_addr = ((struct sockaddr_in6 *)prim)->sin6_addr;
		if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr))
			/*      sin6->sin6_scope_id = ntohs(sin6->sin6_addr.s6_addr16[1]);*/
			in6_recoverscope(sin6, &sin6->sin6_addr, NULL);  /* skip ifp check */
		else
			sin6->sin6_scope_id = 0;	/*XXX*/
#if defined(__FreeBSD__) || defined (__APPLE__)
		*addr= (struct sockaddr *)sin6;
#else
		nam->m_len = sizeof(*sin6);
#endif
	}
	/* Wake any delayed sleep action */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) {
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_DONT_WAKE;
		if (inp->sctp_flags & SCTP_PCB_FLAGS_WAKEOUTPUT) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_WAKEOUTPUT;
			if (sowriteable(inp->sctp_socket)) {
				sowwakeup(inp->sctp_socket);
			}
		}
		if (inp->sctp_flags & SCTP_PCB_FLAGS_WAKEINPUT) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_WAKEINPUT;
			if (soreadable(inp->sctp_socket))
				sorwakeup(inp->sctp_socket);
		}
	}
	splx(s);
	return (0);
#else
	/* No TCP model, we don't support accept then */
	return (EOPNOTSUPP);
#endif
}

int
#if defined(__FreeBSD__) || defined(__APPLE__)
sctp_ingetaddr(struct socket *so, struct sockaddr **addr)
{
	struct sockaddr_in *sin;
#else
sctp_ingetaddr(struct socket *so, struct mbuf *nam)
{
	struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);
#endif
	int s;
	struct sctp_inpcb *inp;
	/*
	 * Do the malloc first in case it blocks.
	 */
#if defined(__FreeBSD__) || defined(__APPLE__)
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME, M_WAITOK |
	       M_ZERO);
#else
	nam->m_len = sizeof(*sin);
	memset(sin, 0, sizeof(*sin));
#endif
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (!inp) {
		splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
		FREE(sin, M_SONAME);
#endif
		return ECONNRESET;
	}
	sin->sin_port = inp->sctp_lport;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
	    if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
		struct sctp_tcb *stcb;
		struct sockaddr_in *sin_a;
		struct route *rtp;
		struct sctp_nets *net;
		int fnd;

		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb == NULL) {
		    goto notConn;
		}
		fnd = 0;
		sin_a = NULL;
		TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		    sin_a = (struct sockaddr_in *)&net->ra._l_addr;
		    if (sin_a->sin_family == AF_INET) {
			fnd = 1;
			break;
		    }
		}
		if ((!fnd) || (sin_a == NULL)) {
		    /* punt */
		    goto notConn;
		}
		rtp = (struct route *)&net->ra;
		sin->sin_addr = sctp_ipv4_source_address_selection(inp,
								   stcb,
								   sin_a,
								   rtp,
								   net,
								   0);
	    } else {
		/* For the bound all case you get back 0 */
	    notConn:
		sin->sin_addr.s_addr = 0;
	    }
	} else {
		/* Take the first IPv4 address in the list */
		struct sctp_laddr *laddr;
		int fnd = 0;
		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa->ifa_addr->sa_family == AF_INET) {
				struct sockaddr_in *sin_a;
				sin_a = (struct sockaddr_in *)laddr->ifa->ifa_addr;
				sin->sin_addr = sin_a->sin_addr;
				fnd = 1;
				break;
			}
		}
		if (!fnd) {
			splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
			FREE(sin, M_SONAME);
#endif
			return ENOENT;
		}
	}
	splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
	(*addr) = (struct sockaddr *)sin;
#endif
	return (0);
}

int
#if defined(__FreeBSD__) || defined(__APPLE__)
sctp_peeraddr(struct socket *so, struct sockaddr **addr)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)*addr;
#else
sctp_peeraddr(struct socket *so, struct mbuf *nam)
{
	struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);
#endif
	int s, fnd;
	struct sockaddr_in *sin_a;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;

	/* Do the malloc first in case it blocks. */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) == 0) {
		/* UDP type and listeners will drop out here */
		return (ENOTCONN);
	}
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME, M_WAITOK |
	       M_ZERO);
#else
	nam->m_len = sizeof(*sin);
	memset(sin, 0, sizeof(*sin));
#endif
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);

	/* We must recapture incase we blocked */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (!inp) {
		splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
		FREE(sin, M_SONAME);
#endif
		return ECONNRESET;
	}
	stcb = LIST_FIRST(&inp->sctp_asoc_list);
	if (stcb == NULL) {
		splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
		FREE(sin, M_SONAME);
#endif
		return ECONNRESET;
	}
	fnd = 0;
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		sin_a = (struct sockaddr_in *)&net->ra._l_addr;
		if (sin_a->sin_family == AF_INET) {
			fnd = 1;
			sin->sin_port = stcb->rport;
			sin->sin_addr = sin_a->sin_addr;
			break;
		}
	}
	if (!fnd) {
		/* No IPv4 address */
		splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
		FREE(sin, M_SONAME);
#endif
		return ENOENT;
	}
	splx(s);
	return (0);
}

#if defined(__FreeBSD__) || defined(__APPLE__)
struct pr_usrreqs sctp_usrreqs = {
	sctp_abort, 
	sctp_accept, 
	sctp_attach, 
	sctp_bind, 
	sctp_connect,
	pru_connect2_notsupp, 
	in_control, 
	sctp_detach, 
	sctp_disconnect,
	sctp_listen, 
	sctp_peeraddr, 
	sctp_usr_recvd, 
	pru_rcvoob_notsupp,
	sctp_send, 
	pru_sense_null, 
	sctp_shutdown, 
	sctp_ingetaddr, 
	sctp_sosend,
	soreceive, 
	sopoll
};

#else
#if defined(__NetBSD__)
int
sctp_usrreq(so, req, m, nam, control, p)
     struct socket *so;
     int req;
     struct mbuf *m, *nam, *control;
     struct proc *p;
{
#else
int
sctp_usrreq(so, req, m, nam, control)
     struct socket *so;
     int req;
     struct mbuf *m, *nam, *control;
{
	struct proc *p = curproc;
#endif
	int s;
	int error = 0;
	int family;

	family = so->so_proto->pr_domain->dom_family;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	if (req == PRU_CONTROL) {
		switch (family) {
		case PF_INET:
			error = in_control(so, (long)m, (caddr_t)nam,
			    (struct ifnet *)control
#if defined(__NetBSD__)
			    , p
#endif
			    );
			break;
#ifdef INET6
		case PF_INET6:
			error = in6_control(so, (long)m, (caddr_t)nam,
			    (struct ifnet *)control, p);
			break;
#endif
		default:
			error =  EAFNOSUPPORT;
		}
		splx(s);
		return (error);
	}
#ifdef __NetBSD__
	if (req == PRU_PURGEIF) {
		struct ifnet *ifn;
		struct ifaddr *ifa;
		ifn = (struct ifnet *)control;
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == family) {
				sctp_delete_ip_address(ifa);
			}
		}
		switch (family) {
		case PF_INET:
			in_purgeif (ifn);
			break;
#ifdef INET6
		case PF_INET6:
			in6_purgeif (ifn);
			break;
#endif /* INET6 */
		default:
			splx(s);
			return (EAFNOSUPPORT);
		}
		splx(s);
		return (0);
	}
#endif
	switch (req) {
	case PRU_ATTACH:
		error = sctp_attach(so, family, p);
		break;
	case PRU_DETACH:
		error = sctp_detach(so);
		break;
	case PRU_BIND:
		if (nam == NULL) {
			splx(s);
			return (EINVAL);
		}
		error  = sctp_bind(so, nam, p);
		break;
	case PRU_LISTEN:
		error = sctp_listen(so, p);
		break;
	case PRU_CONNECT:
		if (nam == NULL) {
			splx(s);
			return (EINVAL);
		}
		error = sctp_connect(so, nam, p);
		break;
	case PRU_DISCONNECT:
		error = sctp_disconnect(so);
		break;
	case PRU_ACCEPT:
		if (nam == NULL) {
			splx(s);
			return (EINVAL);
		}
		error = sctp_accept(so, nam);
	break;
	case PRU_SHUTDOWN:
		error = sctp_shutdown(so);
		break;

	case PRU_RCVD:
		/*
		 * For Open and Net BSD, this is real
		 * ugly. The mbuf *nam that is passed
		 * (by soreceive()) is the int flags c
		 * ast as a (mbuf *) yuck!
		 */
 		error = sctp_usr_recvd(so, (int)((long)nam));
		break;

	case PRU_SEND:
		/* Flags are ignored */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("Send called on V4 side\n");
		}
#endif
		{
		    struct sockaddr *addr;
		    if (nam == NULL)
			addr = NULL;
		    else
			addr = mtod(nam, struct sockaddr *);

		    error = sctp_send(so, 0, m, addr, control, p);
		}
		break;
	case PRU_ABORT:
		error = sctp_abort(so);
		break;

	case PRU_SENSE:
		error = 0;
		break;
	case PRU_RCVOOB:
		error = EAFNOSUPPORT;
		break;
	case PRU_SENDOOB:
		error = EAFNOSUPPORT;
		break;
	case PRU_PEERADDR:
		error = sctp_peeraddr(so, nam);
		break;
	case PRU_SOCKADDR:
		error = sctp_ingetaddr(so, nam);
		break;
	case PRU_SLOWTIMO:
		error = 0;
		break;
	default:
		break;
	}
	splx(s);
	return (error);
}
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
/*
 * Sysctl for sctp variables.
 */
int
sctp_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case SCTPCTL_MAXDGRAM:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		     &sctp_sendspace));
	case SCTPCTL_RECVSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_recvspace));
	case SCTPCTL_AUTOASCONF:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_auto_asconf));
	case SCTPCTL_ECN_ENABLE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &sctp_ecn));
	case SCTPCTL_ECN_NONCE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &sctp_ecn_nonce));
	case SCTPCTL_STRICT_SACK:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &sctp_strict_sacks));
	case SCTPCTL_NOCSUM_LO:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &sctp_no_csum_on_loopback));
	case SCTPCTL_STRICT_INIT:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &sctp_strict_init));
 	case SCTPCTL_PEER_CHK_OH:
 		return (sysctl_int(oldp, oldlenp, newp, newlen,
 				   &sctp_peer_chunk_oh));
 	case SCTPCTL_MAXBURST:
 		return (sysctl_int(oldp, oldlenp, newp, newlen,
 				   &sctp_max_burst_default));
 	case SCTPCTL_MAXCHUNKONQ:
 		return (sysctl_int(oldp, oldlenp, newp, newlen,
 				   &sctp_max_chunks_on_queue));
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
#endif
