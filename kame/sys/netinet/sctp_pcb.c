/*	$KAME: sctp_pcb.c,v 1.37 2004/08/17 06:28:02 t-momose Exp $	*/

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
#include "opt_compat.h"
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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/random.h>
#endif
#if defined(__NetBSD__)
#include <sys/rnd.h>
#endif
#if defined(__OpenBSD__)
#include <dev/rndvar.h>
#endif

#if defined(__APPLE__)
#include <netinet/sctp_callout.h>
#elif defined(__OpenBSD__)
#include <sys/timeout.h>
#else
#include <sys/callout.h>
#endif

#if (defined(__FreeBSD__) && __FreeBSD_version >= 500000)
#include <sys/limits.h>
#else
#include <machine/limits.h>
#endif
#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#if defined(__FreeBSD__) || (__NetBSD__)
#include <netinet6/in6_pcb.h>
#elif defined(__OpenBSD__)
#include <netinet/in_pcb.h>
#endif
#endif /* INET6 */

#include "faith.h"

#ifdef IPSEC
#ifndef __OpenBSD__
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#else
#undef IPSEC
#endif
#endif /* IPSEC */

#include <netinet/sctp_var.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctputil.h>
#include <netinet/sctp.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_timer.h>

#ifndef SCTP_PCBHASHSIZE
/* default number of association hash buckets in each endpoint */
#define SCTP_PCBHASHSIZE 256
#endif

#ifdef SCTP_DEBUG
u_int32_t sctp_debug_on = 0;
#endif /* SCTP_DEBUG */

u_int32_t sctp_pegs[SCTP_NUMBER_OF_PEGS];

int sctp_pcbtblsize = SCTP_PCBHASHSIZE;

struct sctp_epinfo sctppcbinfo;

/* FIX: we don't handle multiple link local scopes */
/* "scopeless" replacement IN6_ARE_ADDR_EQUAL */
int
SCTP6_ARE_ADDR_EQUAL(struct in6_addr *a, struct in6_addr *b)
{
	struct in6_addr tmp_a, tmp_b;
	/* use a copy of a and b */
	tmp_a = *a;
	tmp_b = *b;
	in6_clearscope(&tmp_a);
	in6_clearscope(&tmp_b);
	return (IN6_ARE_ADDR_EQUAL(&tmp_a, &tmp_b));
}

#ifdef __OpenBSD__
extern int ipport_firstauto;
extern int ipport_lastauto;
extern int ipport_hifirstauto;
extern int ipport_hilastauto;
#endif

caddr_t sctp_lowest_tcb = (caddr_t)0xffffffff;
caddr_t sctp_highest_tcb = 0;

void
sctp_fill_pcbinfo(struct sctp_pcbinfo *spcb)
{
	spcb->ep_count = sctppcbinfo.ipi_count_ep;
	spcb->asoc_count = sctppcbinfo.ipi_count_asoc;
	spcb->laddr_count = sctppcbinfo.ipi_count_laddr;
	spcb->raddr_count = sctppcbinfo.ipi_count_raddr;
	spcb->chk_count = sctppcbinfo.ipi_count_chunk;
	spcb->sockq_count = sctppcbinfo.ipi_count_sockq;
	spcb->mbuf_track = sctppcbinfo.mbuf_track;
}


/*
 * Given a endpoint, look and find in its association list any association
 * with the "to" address given. This can be a "from" address, too, for
 * inbound packets. For outbound packets it is a true "to" address.
 */
static struct sctp_tcb *
sctp_tcb_special_locate(struct sctp_inpcb **inp_p, struct sockaddr *from,
    struct sockaddr *to, struct sctp_nets **netp)
{
	/*
	 * Note for this module care must be taken when observing what to is
	 * for. In most of the rest of the code the TO field represents my
	 * peer and the FROM field represents my address. For this module it
	 * is reversed of that.
	 */

#ifdef SCTP_TCP_MODEL_SUPPORT
	/*
	 * If we support the TCP model, then we must now dig through to
	 * see if we can find our endpoint in the list of tcp ep's.
	 */
	uint16_t lport, rport;
	struct sctppcbhead *ephead;
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;

	if (to->sa_family == AF_INET && from->sa_family == AF_INET) {
		lport = ((struct sockaddr_in *)to)->sin_port;
		rport = ((struct sockaddr_in *)from)->sin_port;
	} else if (to->sa_family == AF_INET6 && from->sa_family == AF_INET6) {
		lport = ((struct sockaddr_in6 *)to)->sin6_port;
		rport = ((struct sockaddr_in6 *)from)->sin6_port;
	} else {
		return NULL;
	}

	ephead = &sctppcbinfo.sctp_tcpephash[SCTP_PCBHASH_ALLADDR(
	    (lport + rport), sctppcbinfo.hashtcpmark)];
	/*
	 * Ok now for each of the guys in this bucket we must look
	 * and see:
	 *  - Does the remote port match.
	 *  - Does there single association's addresses match this
	 *    address (to).
	 * If so we update p_ep to point to this ep and return the
	 * tcb from it.
	 */
	LIST_FOREACH(inp, ephead, sctp_hash) {
		if (lport != inp->sctp_lport) {
			continue;
		}

		/* check to see if the ep has one of the addresses */
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {
			/* We are NOT bound all, so look further */
			int match = 0;

			LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
				if (laddr->ifa == NULL) {
#ifdef SCTP_DEBUG
					if (sctp_debug_on & SCTP_DEBUG_PCB1) {
						printf("An ounce of prevention is worth a pound of cure\n");
					}
#endif
					continue;
				}
				if (laddr->ifa->ifa_addr == NULL) {
#ifdef SCTP_DEBUG
					if (sctp_debug_on & SCTP_DEBUG_PCB1) {
						printf("ifa with a NULL address\n");
					}
#endif
					continue;
				}
				if (laddr->ifa->ifa_addr->sa_family ==
				    to->sa_family) {
					/* see if it matches */
					struct sockaddr_in *intf_addr, *sin;
					intf_addr = (struct sockaddr_in *)
					    laddr->ifa->ifa_addr;
					sin = (struct sockaddr_in *)to;
					if (from->sa_family == AF_INET) {
						if (sin->sin_addr.s_addr ==
						    intf_addr->sin_addr.s_addr) {
							match = 1;
							break;
						}
					} else {
						struct sockaddr_in6 *intf_addr6;
						struct sockaddr_in6 *sin6;
						sin6 = (struct sockaddr_in6 *)
						    to;
						intf_addr6 = (struct sockaddr_in6 *)
						    laddr->ifa->ifa_addr;
						    
						if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
						    &intf_addr6->sin6_addr)) {
							match = 1;
							break;
						}
					}
				}
			}
			if (match == 0)
				/* This endpoint does not have this address */
				continue;
		}
		/*
		 * Ok if we hit here the ep has the address, does it hold the
		 * 
		 */
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb== NULL)
			continue;

		if (stcb->rport != rport)
			/* remote port does not match. */
			continue;

		/* Does this TCB have a matching address? */
		TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
			if (net->ra._l_addr.sa.sa_family != from->sa_family) {
				/* not the same family, can't be a match */
				continue;
			}
			if (from->sa_family == AF_INET) {
				struct sockaddr_in *sin, *rsin;
				sin = (struct sockaddr_in *)&net->ra._l_addr;
				rsin = (struct sockaddr_in *)from;
				if (sin->sin_addr.s_addr ==
				    rsin->sin_addr.s_addr) {
					/* found it */
					if (netp != NULL) {
						*netp = net;
					}
					/* Update the endpoint pointer */
					*inp_p = inp;
					return (stcb);
				}
			} else {
				struct sockaddr_in6 *sin6, *rsin6;
				sin6 = (struct sockaddr_in6 *)&net->ra._l_addr;
				rsin6 = (struct sockaddr_in6 *)from;
				if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
				    &rsin6->sin6_addr)) {
					/* found it */
					if (netp != NULL) {
						*netp = net;
					}
					/* Update the endpoint pointer */
					*inp_p = inp;
					return (stcb);
				}
			}
		}
	}
#endif /* SCTP_TCP_MODEL_SUPPORT */
	return (NULL);
}

struct sctp_tcb *
sctp_findassociation_ep_asconf(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_inpcb **inp_p, struct sctp_nets **netp)
{
	struct sctp_tcb *stcb;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sockaddr_storage local_store, remote_store;
	struct ip *iph;
	struct sctp_paramhdr parm_buf, *phdr;

	memset(&local_store, 0, sizeof(local_store));
	memset(&remote_store, 0, sizeof(remote_store));

	/* First get the destination address setup too. */
	iph = mtod(m, struct ip *);
	if (iph->ip_v == IPVERSION) {
		/* its IPv4 */
		sin = (struct sockaddr_in *)&local_store;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_port = sh->dest_port;
		sin->sin_addr.s_addr = iph->ip_dst.s_addr ;
	} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
		/* its IPv6 */
		struct ip6_hdr *ip6;
		ip6 = mtod(m, struct ip6_hdr *);
		sin6 = (struct sockaddr_in6 *)&local_store;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_port = sh->dest_port;
		sin6->sin6_addr = ip6->ip6_dst;
	} else {
		return NULL;
	}

	phdr = sctp_get_next_param(m, offset + sizeof(struct sctp_asconf_chunk),
	    &parm_buf, sizeof(struct sctp_paramhdr));
	if (phdr == NULL) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
			printf("sctp_process_control: failed to get asconf lookup addr\n");
		}
#endif /* SCTP_DEBUG */
		return NULL;
	}
	phdr->param_type = ntohs(phdr->param_type);
	phdr->param_length = ntohs(phdr->param_length);

	/* get the correlation address */
	if (phdr->param_type == SCTP_IPV6_ADDRESS) {
		/* ipv6 address param */
		struct sctp_ipv6addr_param *p6, p6_buf;
		if (phdr->param_length != sizeof(struct sctp_ipv6addr_param)) {
			return NULL;
		}

		p6 = (struct sctp_ipv6addr_param *)sctp_get_next_param(m,
		    offset + sizeof(struct sctp_asconf_chunk),
		    &p6_buf.ph, sizeof(*p6));
		if (p6 == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("sctp_process_control: failed to get asconf v6 lookup addr\n");
			}
#endif /* SCTP_DEBUG */
			return (NULL);
		}
		sin6 = (struct sockaddr_in6 *)&remote_store;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_port = sh->src_port;
		memcpy(&sin6->sin6_addr, &p6->addr, sizeof(struct in6_addr));
	} else if (phdr->param_type == SCTP_IPV4_ADDRESS) {
		/* ipv4 address param */
		struct sctp_ipv4addr_param *p4, p4_buf;
		if (phdr->param_length != sizeof(struct sctp_ipv4addr_param)) {
			return NULL;
		}

		p4 = (struct sctp_ipv4addr_param *)sctp_get_next_param(m,
		    offset + sizeof(struct sctp_asconf_chunk),
		    &p4_buf.ph, sizeof(*p4));
		if (p4 == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("sctp_process_control: failed to get asconf v4 lookup addr\n");
			}
#endif /* SCTP_DEBUG */
			return (NULL);
		}
		sin = (struct sockaddr_in *)&remote_store;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_port = sh->src_port;
		memcpy(&sin->sin_addr, &p4->addr, sizeof(struct in_addr));
	} else {
		/* invalid address param type */
		return NULL;
	}

	stcb = sctp_findassociation_ep_addr(inp_p,
	    (struct sockaddr *)&remote_store, netp,
	    (struct sockaddr *)&local_store);
	return (stcb);
}

struct sctp_tcb *
sctp_findassociation_ep_addr(struct sctp_inpcb **inp_p, struct sockaddr *remote,
    struct sctp_nets **netp, struct sockaddr *local)
{
	struct sctpasochead *head;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;
	uint16_t rport;

	inp = *inp_p;
	if (remote->sa_family == AF_INET) {
		rport = (((struct sockaddr_in *)remote)->sin_port);
	} else if (remote->sa_family == AF_INET6) {
		rport = (((struct sockaddr_in6 *)remote)->sin6_port);
	} else {
		return (NULL);
	}
#ifdef SCTP_TCP_MODEL_SUPPORT
	if (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		/*
		 * Now either this guy is our listner or it's the connector.
		 * If it is the one that issued the connect, then it's only
		 * chance is to be the first TCB in the list. If it is the
		 * acceptor, then do the special_lookup to hash and find the
		 * real inp.
		 */
		if (inp->sctp_flags & SCTP_PCB_FLAGS_ACCEPTING) {
			/* to is peer addr, from is my addr */
			stcb = sctp_tcb_special_locate(inp_p, remote, local,
			    netp);
			return (stcb);
		} else {
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				return (NULL);
			}
			if (stcb->rport != rport) {
				/* remote port does not match. */
				return (NULL);
			}
			/* now look at the list of remote addresses */
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				if (net->ra._l_addr.sa.sa_family !=
				    remote->sa_family) {
					/* not the same family */
					continue;
				}
				if (remote->sa_family == AF_INET) {
					struct sockaddr_in *sin, *rsin;
					sin = (struct sockaddr_in *)
					    &net->ra._l_addr;
					rsin = (struct sockaddr_in *)remote;
					if (sin->sin_addr.s_addr ==
					    rsin->sin_addr.s_addr) {
						/* found it */
						if (netp != NULL) {
							*netp = net;
						}
						return (stcb);
					}
				} else if (remote->sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6, *rsin6;
					sin6 = (struct sockaddr_in6 *)&net->ra._l_addr;
					rsin6 = (struct sockaddr_in6 *)remote;
					if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					     &rsin6->sin6_addr)) {
						/* found it */
						if (netp != NULL) {
							*netp = net;
						}
						return (stcb);
					}
				}
			}
		}
	} else 
#endif
	{
		head = &inp->sctp_tcbhash[SCTP_PCBHASH_ALLADDR(rport,
		    inp->sctp_hashmark)];
		if (head == NULL) {
			return (NULL);
		}
		LIST_FOREACH(stcb, head, sctp_tcbhash) {
			if (stcb->rport != rport) {
				/* remote port does not match */
				continue;
			}
			/* now look at the list of remote addresses */
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				if (net->ra._l_addr.sa.sa_family !=
				    remote->sa_family) {
					/* not the same family */
					continue;
				}
				if (remote->sa_family == AF_INET) {
					struct sockaddr_in *sin, *rsin;
					sin = (struct sockaddr_in *)
					    &net->ra._l_addr;
					rsin = (struct sockaddr_in *)remote;
					if (sin->sin_addr.s_addr ==
					    rsin->sin_addr.s_addr) {
						/* found it */
						if (netp != NULL) {
							*netp = net;
						}
						return (stcb);
					}
				} else if (remote->sa_family == AF_INET6){
					struct sockaddr_in6 *sin6, *rsin6;
					sin6 = (struct sockaddr_in6 *)
					    &net->ra._l_addr;
					rsin6 = (struct sockaddr_in6 *)remote;
					if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &rsin6->sin6_addr)) {
						/* found it */
						if (netp != NULL) {
							*netp = net;
						}
						return (stcb);
					}
				}
			}
		}
	}
	/* not found */
	return (NULL);
}

/*
 * Find an association for a specific endpoint using the association id
 * given out in the COMM_UP notification
 */
struct sctp_tcb *
sctp_findassociation_ep_asocid(struct sctp_inpcb *inp, caddr_t asoc_id)
{
	struct sctp_tcb *retval;

	if (asoc_id == 0 || inp == NULL)
		return (NULL);

	if (inp->highest_tcb == 0) {
		/* can't be never allocated a association yet */
		return (NULL);
	}
	if (((u_long)asoc_id % 4) != 0) {
	       /* Must be aligned to 4 byte boundary */
	       return (NULL);
	}

	if ((inp->highest_tcb >= asoc_id) && (inp->lowest_tcb <= asoc_id)) {
		/* it is possible lets have a look */
		retval = (struct sctp_tcb *)asoc_id;
		if (retval->sctp_ep == inp && retval->asoc.state) {
			return (retval);
		}
	}
	return (NULL);
}

struct sctp_tcb *
sctp_findassociation_associd(caddr_t asoc_id)
{
	/* This is allows you to look at another sockets info */
	struct sctp_tcb *retval;
	if ((asoc_id < sctp_lowest_tcb) && (asoc_id > sctp_highest_tcb)) {
		return (NULL);
	}
	retval = (struct sctp_tcb *)asoc_id;
	if (retval->asoc.state != 0)
		return (retval);
	return (NULL);

}

static struct sctp_inpcb *
sctp_endpoint_probe(struct sockaddr *nam, struct sctppcbhead *head,
    uint16_t lport)
{
	struct sctp_inpcb *inp;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sctp_laddr *laddr;

	if (nam->sa_family == AF_INET) {
		sin = (struct sockaddr_in *)nam;
		sin6 = NULL;
	} else if (nam->sa_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)nam;
		sin = NULL;
	} else {
		/* unsupported family */
		return (NULL);
	}
	LIST_FOREACH(inp, head, sctp_hash) {
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) &&
		    (inp->sctp_lport == lport)) {
			/* got it */
			if ((nam->sa_family == AF_INET) &&
			    (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
#if defined(__FreeBSD__) || defined(__APPLE__)
			    (((struct inpcb *)inp)->inp_flags & IN6P_IPV6_V6ONLY)
#else
#if defined(__OpenBSD__)
			    (0)	/* For open bsd we do dual bind only */
#else
			    (((struct in6pcb *)inp)->in6p_flags & IN6P_IPV6_V6ONLY)
#endif
#endif
				) {
				/* IPv4 on a IPv6 socket with ONLY IPv6 set */
				continue;
			}
			/* A V6 address and the endpoint is NOT bound V6 */
			if (nam->sa_family == AF_INET6 &&
			   (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) {
				continue;
			}
			return (inp);
		}
	}

	if ((nam->sa_family == AF_INET) &&
	    (sin->sin_addr.s_addr == INADDR_ANY)) {
		/* Can't hunt for one that has no address specified */
		return (NULL);
	} else if ((nam->sa_family == AF_INET6) &&
		   (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))) {
		/* Can't hunt for one that has no address specified */
		return (NULL);
	}
	/*
	 * ok, not bound to all so see if we can find a EP bound to this
	 * address.
	 */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Ok, there is NO bound-all available for port:%x\n", ntohs(lport));
	}
#endif
	LIST_FOREACH(inp, head, sctp_hash) {
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL)) {
			continue;
		}
		/*
		 * Ok this could be a likely candidate, look at all of
		 * its addresses
		 */
		if (inp->sctp_lport != lport)
			continue;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB1) {
			printf("Ok, found maching local port\n");
		}
#endif
		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_PCB1) { 
					printf("An ounce of prevention is worth a pound of cure\n");
				}
#endif
				continue;
			}
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB1) {
				printf("Ok laddr->ifa:%p is possible, ",
				    laddr->ifa);
			}
#endif
			if (laddr->ifa->ifa_addr == NULL) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_PCB1) {
					printf("Huh IFA as an ifa_addr=NULL, ");
				}
#endif
				continue;
			}
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB1) {
				printf("Ok laddr->ifa:%p is possible, ",
				    laddr->ifa->ifa_addr);
				sctp_print_address(laddr->ifa->ifa_addr);
				printf("looking for ");
				sctp_print_address(nam);
			}
#endif
			if (laddr->ifa->ifa_addr->sa_family == nam->sa_family) {
				/* possible, see if it matches */
				struct sockaddr_in *intf_addr;
				intf_addr = (struct sockaddr_in *)
				    laddr->ifa->ifa_addr;
				if (nam->sa_family == AF_INET) {
					if (sin->sin_addr.s_addr ==
					    intf_addr->sin_addr.s_addr) {
#ifdef SCTP_DEBUG
						if (sctp_debug_on & SCTP_DEBUG_PCB1) {
							printf("YES, return ep:%p\n", inp);
						}
#endif
						return (inp);
					}
				} else if (nam->sa_family == AF_INET6) {
					struct sockaddr_in6 *intf_addr6;
					intf_addr6 = (struct sockaddr_in6 *)
					    laddr->ifa->ifa_addr;
					if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
				 	    &intf_addr6->sin6_addr)) {
#ifdef SCTP_DEBUG
						if (sctp_debug_on & SCTP_DEBUG_PCB1) {
							printf("YES, return ep:%p\n", inp);
						}
#endif
						return (inp);
					}
				}
			}
		}
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("NO, Falls out to NULL\n");
	}
#endif
	return (NULL);
}


struct sctp_inpcb *
sctp_pcb_findep(struct sockaddr *nam, int find_tcp_pool)
{
	/*
	 * First we check the hash table to see if someone has this port
	 * bound with just the port.
	 */
	struct sctp_inpcb *inp;
	struct sctppcbhead *head;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	int lport;
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Looking for endpoint %d :",
		       ntohs(((struct sockaddr_in *)nam)->sin_port));
		sctp_print_address(nam);
	}
#endif
	if (nam->sa_family == AF_INET) {
		sin = (struct sockaddr_in *)nam;
		lport = ((struct sockaddr_in *)nam)->sin_port;
	} else if (nam->sa_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)nam;
		lport = ((struct sockaddr_in6 *)nam)->sin6_port;
	} else {
		/* unsupported family */
		return (NULL);
	}
	/*
	 * I could cheat here and just cast to one of the types but we will
	 * do it right. It also provides the check against an Unsupported
	 * type too.
	 */
	/* Find the head of the ALLADDR chain */
	head = &sctppcbinfo.sctp_ephash[SCTP_PCBHASH_ALLADDR(lport,
	    sctppcbinfo.hashmark)];
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Main hash to lookup at head:%p\n", head);
	}
#endif
 	inp = sctp_endpoint_probe(nam, head, lport);

#ifdef SCTP_TCP_MODEL_SUPPORT
	/*
	 * If the TCP model exists it could be that the main listening
	 * endpoint is gone but there exists a connected socket for this
	 * guy yet. If so we can return the first one that we find. This
	 * may NOT be the correct one but the sctp_findassociation_ep_addr
	 * has further code to look at all TCP models.
	 */
	if (inp == NULL && find_tcp_pool) {
		int i;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB1) {
			printf("EP was NULL and TCP model is supported\n");
		}
#endif
		for (i = 0; i < sctppcbinfo.hashtblsize; i++) {
			/*
			 * This is real gross, but we do NOT have a remote
			 * port at this point depending on who is calling. We
			 * must therefore look for ANY one that matches our
			 * local port :/
			 */
			head = &sctppcbinfo.sctp_tcpephash[i];
			if (LIST_FIRST(head)) {
				inp = sctp_endpoint_probe(nam, head, lport);
				if (inp) {
					/* Found one */
					break;
				}
			}
		}
	}
#endif /* SCTP_TCP_MODEL_SUPPORT */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("EP to return is %p\n", inp);
	}
#endif
	return (inp);
}

/*
 * Find an association for an endpoint with the pointer to whom you want
 * to send to and the endpoint pointer. The address can be IPv4 or IPv6.
 * We may need to change the *to to some other struct like a mbuf...
 */
struct sctp_tcb *
sctp_findassociation_addr_sa(struct sockaddr *to, struct sockaddr *from,
    struct sctp_inpcb **inp_p, struct sctp_nets **netp, int find_tcp_pool)
{
	struct sctp_inpcb *inp;
#ifdef SCTP_TCP_MODEL_SUPPORT
	struct sctp_tcb *retval;

	if (find_tcp_pool) {
		if (inp_p != NULL) {
			retval = sctp_tcb_special_locate(inp_p, from, to, netp);
		} else {
			retval = sctp_tcb_special_locate(&inp, from, to, netp);
		}
		if (retval != NULL) {
			return (retval);
		}
	}
#endif
	inp = sctp_pcb_findep(to, 0);
	if (inp_p != NULL) {
		*inp_p = inp;
	}
	if (inp == NULL) {
		return (NULL);
	}

	/*
	 * ok, we have an endpoint, now lets find the assoc for it (if any)
	 * we now place the source address or from in the to of the find
	 * endpoint call. Since in reality this chain is used from the
	 * inbound packet side.
	 */
	if (inp_p != NULL) {
		return (sctp_findassociation_ep_addr(inp_p, from, netp, to));
	} else {
		return (sctp_findassociation_ep_addr(&inp, from, netp, to));
	}
}


/*
 * This routine will grub through the mbuf that is a INIT or INIT-ACK and
 * find all addresses that the sender has specified in any address list.
 * Each address will be used to lookup the TCB and see if one exits.
 */
static struct sctp_tcb *
sctp_findassociation_special_addr(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_inpcb **inp_p, struct sctp_nets **netp,
    struct sockaddr *dest)
{
	struct sockaddr_in sin4;
	struct sockaddr_in6 sin6;
	struct sctp_paramhdr *phdr, parm_buf;
	struct sctp_tcb *retval;
	u_int32_t ptype, plen;

	memset(&sin4, 0, sizeof(sin4));
	memset(&sin6, 0, sizeof(sin6));
	sin4.sin_len = sizeof(sin4);
	sin4.sin_family = AF_INET;
	sin4.sin_port = sh->src_port;
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = sh->src_port;

	retval = NULL;
	offset += sizeof(struct sctp_init_chunk);

	phdr = sctp_get_next_param(m, offset, &parm_buf, sizeof(parm_buf));
	while (phdr != NULL) {
		/* now we must see if we want the parameter */
		ptype = ntohs(phdr->param_type);
		plen = ntohs(phdr->param_length);
		if (plen == 0) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB1) {
				printf("sctp_findassociation_special_addr: Impossible length in parameter\n");
			}
#endif /* SCTP_DEBUG */
			break;
		}
		if (ptype == SCTP_IPV4_ADDRESS &&
		    plen == sizeof(struct sctp_ipv4addr_param)) {
			/* Get the rest of the address */
			struct sctp_ipv4addr_param ip4_parm, *p4;

			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&ip4_parm, plen);
			if (phdr == NULL) {
				return (NULL);
			}
			p4 = (struct sctp_ipv4addr_param *)phdr;
			memcpy(&sin4.sin_addr, &p4->addr, sizeof(p4->addr));
			/* look it up */
			retval = sctp_findassociation_ep_addr(inp_p,
			    (struct sockaddr *)&sin4, netp, dest);
			if (retval != NULL) {
				return (retval);
			}
		} else if (ptype == SCTP_IPV6_ADDRESS &&
		    plen == sizeof(struct sctp_ipv6addr_param)) {
			/* Get the rest of the address */
			struct sctp_ipv6addr_param ip6_parm, *p6;

			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&ip6_parm, plen);
			if (phdr == NULL) {
				return (NULL);
			}
			p6 = (struct sctp_ipv6addr_param *)phdr;
			memcpy(&sin6.sin6_addr, &p6->addr, sizeof(p6->addr));
			/* look it up */
			retval = sctp_findassociation_ep_addr(inp_p,
			    (struct sockaddr *)&sin6, netp, dest);
			if (retval != NULL) {
				return (retval);
			}
		}
		offset += SCTP_SIZE32(plen);
		phdr = sctp_get_next_param(m, offset, &parm_buf,
		    sizeof(parm_buf));
	}
	return (NULL);
}

static struct sctp_tcb *
sctp_findassoc_by_vtag(struct sockaddr *from, uint32_t vtag, 
    struct sctp_inpcb **inp_p, struct sctp_nets **netp, uint16_t rport,
    uint16_t lport)
{
	/*
	 * Use my vtag to hash. If we find it we then verify the source addr
	 * is in the assoc. If all goes well we save a bit on rec of a packet.
	 */
	struct sctpasochead *head;
	struct sctp_nets *net;
	struct sctp_tcb *stcb;

	head = &sctppcbinfo.sctp_asochash[SCTP_PCBHASH_ASOC(vtag,
	    sctppcbinfo.hashasocmark)];
	if (head == NULL) {
		/* invalid vtag */
		return (NULL);
	}
	LIST_FOREACH(stcb, head, sctp_asocs) {
		if (stcb->asoc.my_vtag == vtag) {
			/* candidate */
			if (stcb->rport != rport) {
				/*
				 * we could remove this if vtags are unique
				 * across the system.
				 */
				continue;
			}
			if (stcb->sctp_ep->sctp_lport != lport) {
				/*
				 * we could remove this if vtags are unique
				 * across the system.
				 */
				continue;
			}
			net = sctp_findnet(stcb, from);
			if (net) {
				/* yep its him. */
				*netp = net;
				sctp_pegs[SCTP_VTAG_EXPR]++;
				*inp_p = stcb->sctp_ep;
				return (stcb);
			} else {
				/* bogus 
				sctp_pegs[SCTP_VTAG_BOGUS]++;
				return (NULL);
				*/
				/* we could uncomment the above
				 * if vtags were unique across
				 * the system.
				 */
				continue;
			}
		}
	}
	return (NULL);
}

/*
 * Find an association with the pointer to the inbound IP packet. This
 * can be a IPv4 or IPv6 packet.
 */
struct sctp_tcb *
sctp_findassociation_addr(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_chunkhdr *ch,
    struct sctp_inpcb **inp_p, struct sctp_nets **netp)
{
	int find_tcp_pool;
	struct ip *iph;
	struct sctp_tcb *retval;
	struct sockaddr_storage to_store, from_store;
	struct sockaddr *to = (struct sockaddr *)&to_store;
	struct sockaddr *from = (struct sockaddr *)&from_store;
	struct sctp_inpcb *inp;


	iph = mtod(m, struct ip *);
	if (iph->ip_v == IPVERSION) {
		/* its IPv4 */
		struct sockaddr_in *to4, *from4;

		to4 = (struct sockaddr_in *)&to_store;
		from4 = (struct sockaddr_in *)&from_store;
		bzero(to4, sizeof(*to4));
		bzero(from4, sizeof(*from4));
		from4->sin_family = to4->sin_family = AF_INET;
		from4->sin_len = to4->sin_len = sizeof(struct sockaddr_in);
		from4->sin_addr.s_addr  = iph->ip_src.s_addr;
		to4->sin_addr.s_addr = iph->ip_dst.s_addr ;
		from4->sin_port = sh->src_port;
		to4->sin_port = sh->dest_port;
	} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
		/* its IPv6 */
		struct ip6_hdr *ip6;
		struct sockaddr_in6 *to6, *from6;

		ip6 = mtod(m, struct ip6_hdr *);
		to6 = (struct sockaddr_in6 *)&to_store;
		from6 = (struct sockaddr_in6 *)&from_store;
		bzero(to6, sizeof(*to6));
		bzero(from6, sizeof(*from6));
		from6->sin6_family = to6->sin6_family = AF_INET6;
		from6->sin6_len = to6->sin6_len = sizeof(struct sockaddr_in6);
		to6->sin6_addr = ip6->ip6_dst;
		from6->sin6_addr = ip6->ip6_src;
		from6->sin6_port = sh->src_port;
		to6->sin6_port = sh->dest_port;
		/* Get the scopes in properly to the sin6 addr's */
		(void)in6_recoverscope(to6, &to6->sin6_addr, NULL);
#if defined(SCTP_BASE_FREEBSD) || defined(__APPLE__)
		(void)in6_embedscope(&to6->sin6_addr, to6, NULL, NULL);
#else
		(void)in6_embedscope(&to6->sin6_addr, to6);
#endif

		(void)in6_recoverscope(from6, &from6->sin6_addr, NULL);
#if defined(SCTP_BASE_FREEBSD) || defined(__APPLE__)
		(void)in6_embedscope(&from6->sin6_addr, from6, NULL, NULL);
#else
		(void)in6_embedscope(&from6->sin6_addr, from6);
#endif
	} else {
		/* Currently not supported. */
		return (NULL);
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Looking for port %d address :",
		       ntohs(((struct sockaddr_in *)to)->sin_port));
		sctp_print_address(to);
		printf("From for port %d address :",
		       ntohs(((struct sockaddr_in *)from)->sin_port));
		sctp_print_address(from);
	}
#endif
	if (sh->v_tag) {
		/* we only go down this path if vtag is non-zero */
		retval = sctp_findassoc_by_vtag(from, ntohl(sh->v_tag),
		    inp_p, netp, sh->src_port, sh->dest_port);
		if (retval) {
			return (retval);
		}
	}
	find_tcp_pool = 0;
#ifdef SCTP_TCP_MODEL_SUPPORT
	if ((ch->chunk_type != SCTP_INITIATION) &&
	    (ch->chunk_type != SCTP_INITIATION_ACK) &&
	    (ch->chunk_type != SCTP_COOKIE_ACK) &&
	    (ch->chunk_type != SCTP_COOKIE_ECHO)) {
		/* Other chunk types go to the tcp pool. */
		find_tcp_pool = 1;
	}
#endif
	if (inp_p) {
		retval = sctp_findassociation_addr_sa(to, from, inp_p, netp,
		    find_tcp_pool);
		inp = *inp_p;
	} else {
		retval = sctp_findassociation_addr_sa(to, from, &inp, netp,
		    find_tcp_pool);
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("retval:%p inp:%p\n", retval, inp);
	}
#endif
	if (retval == NULL && inp) {
		/* Found a EP but not this address */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB1) {
			printf("Found endpoint %p but no asoc - ep state:%x\n",
			    inp, inp->sctp_flags);
		}
#endif
		if ((ch->chunk_type == SCTP_INITIATION) ||
		    (ch->chunk_type == SCTP_INITIATION_ACK)) {
#ifdef SCTP_TCP_MODEL_SUPPORT
			/*
			 * special hook, we do NOT return linp or an
			 * association that is linked to an existing
			 * association that is under the TCP pool (i.e. no
			 * listener exists). The endpoint finding routine
			 * will always find a listner before examining the
			 * TCP pool.
			 */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_PCB1) {
					printf("Gak, its in the TCP pool... return NULL");
				}
#endif
				if (inp_p) {
					*inp_p = NULL;
				}
				return (NULL);
			}
#endif /* SCTP_TCP_MODEL_SUPPORT */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB1) {
				printf("Now doing SPECIAL find\n");
			}
#endif
			retval = sctp_findassociation_special_addr(m, iphlen,
			    offset, sh, inp_p, netp, to);
		}
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
	    printf("retval is %p\n", retval);
	}
#endif
	return (retval);
}

extern int sctp_max_burst_default;
/*
 * allocate a sctp_inpcb and setup a temporary binding to a port/all
 * addresses. This way if we don't get a bind we by default pick a ephemeral
 * port with all addresses bound.
 */
int
sctp_inpcb_alloc(struct socket *so)
{
	/*
	 * we get called when a new endpoint starts up. We need to allocate
	 * the sctp_inpcb structure from the zone and init it. Mark it as
	 * unbound and find a port that we can use as an ephemeral with
	 * INADDR_ANY. If the user binds later no problem we can then add
	 * in the specific addresses. And setup the default parameters for
	 * the EP.
	 */
	int i, error;
	struct sctp_inpcb *inp, *n_inp;
	struct sctp_pcb *m;
	struct timeval time;

	error = 0;

        /* Hack alert */
	inp = LIST_FIRST(&sctppcbinfo.listhead);
	while (inp) {
		n_inp = LIST_NEXT(inp, sctp_list);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
			if (LIST_FIRST(&inp->sctp_asoc_list) == NULL) {
				/* finish the job now */
				printf("Found a GONE on list\n");
				sctp_inpcb_free(inp, 1);
			}
		}
		inp = n_inp;
	}		
	inp = (struct sctp_inpcb *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_ep);
	if (inp == NULL) {
		printf("Out of SCTP-INPCB structures - no resources\n");
		return (ENOBUFS);
	}

	/* zap it */
	bzero(inp, sizeof(*inp));

	/* bump generations */
	inp->ip_inp.inp.inp_socket = so;

	/* setup socket pointers */
	inp->sctp_socket = so;

	/* setup inpcb socket too */
	inp->ip_inp.inp.inp_socket = so;
	inp->sctp_frag_point = SCTP_DEFAULT_MAXSEGMENT;
#ifndef SCTP_VTAG_TIMEWAIT_PER_STACK
	LIST_INIT(inp->vtag_timewait);
#endif

#ifdef IPSEC
#if !(defined(__OpenBSD__) || defined(__APPLE__))
	{
		struct inpcbpolicy *pcb_sp = NULL;
		error = ipsec_init_pcbpolicy(so, &pcb_sp);
		/* Arrange to share the policy */
		inp->ip_inp.inp.inp_sp = pcb_sp;
		((struct in6pcb *)(&inp->ip_inp.inp))->in6p_sp = pcb_sp;
	}
#else
	/* not sure what to do for openbsd here */
	error = 0;
#endif
	if (error != 0) {
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_ep, inp);
		return error;
	}
#endif /* IPSEC */
	sctppcbinfo.ipi_count_ep++;
#if defined(__FreeBSD__) || defined(__APPLE__)
	inp->ip_inp.inp.inp_gencnt = ++sctppcbinfo.ipi_gencnt_ep;
	inp->ip_inp.inp.inp_ip_ttl = ip_defttl;
#else
	inp->inp_ip_ttl = ip_defttl;
	inp->inp_ip_tos = 0;
#endif

	so->so_pcb = (caddr_t)inp;
	inp->lowest_tcb = (caddr_t)0xffffffff;

	if ((so->so_type == SOCK_DGRAM) ||
	    (so->so_type == SOCK_SEQPACKET)) {
		/* UDP style socket */
		inp->sctp_flags = (SCTP_PCB_FLAGS_UDPTYPE |
		    SCTP_PCB_FLAGS_UNBOUND);
		inp->sctp_flags |= (SCTP_PCB_FLAGS_RECVDATAIOEVNT);
		/* Be sure it is NON-BLOCKING IO for UDP */
		/*so->so_state |= SS_NBIO;*/
#ifdef SCTP_TCP_MODEL_SUPPORT
	} else if (so->so_type == SOCK_STREAM) {
		/* TCP style socket */
		inp->sctp_flags = (SCTP_PCB_FLAGS_TCPTYPE |
		    SCTP_PCB_FLAGS_UNBOUND);
		inp->sctp_flags |= (SCTP_PCB_FLAGS_RECVDATAIOEVNT);
		/* Be sure we have blocking IO bu default */
		so->so_state &= ~SS_NBIO;
#endif /* SCTP_TCP_MODEL_SUPPORT */
	} else {
		/*
		 * unsupported socket type (RAW, etc)- in case we missed
		 * it in protosw
		 */
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_ep, inp);
		return (EOPNOTSUPP);
	}
	inp->sctp_tcbhash = hashinit(sctp_pcbtblsize,
#ifdef __NetBSD__
	    HASH_LIST,
#endif
	    M_PCB,
#if defined(__NetBSD__) || defined(__OpenBSD__)
	    M_WAITOK,
#endif
	    &inp->sctp_hashmark);
	if (inp->sctp_tcbhash == NULL) {
		printf("Out of SCTP-INPCB->hashinit - no resources\n");
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_ep, inp);
		return (ENOBUFS);
	}

	LIST_INSERT_HEAD(&sctppcbinfo.listhead, inp, sctp_list);
	LIST_INIT(&inp->sctp_addr_list);
	LIST_INIT(&inp->sctp_asoc_list);
	TAILQ_INIT(&inp->sctp_queue_list);
	/* Init the timer structure for signature change */
#if defined (__FreeBSD__) && __FreeBSD_version >= 500000
	callout_init(&inp->sctp_ep.signature_change.timer, 0);
#else
	callout_init(&inp->sctp_ep.signature_change.timer);
#endif

	/* now init the actual endpoint default data */
	m = &inp->sctp_ep;

	/* setup the base timeout information */
	m->sctp_timeoutticks[SCTP_TIMER_SEND] = SCTP_SEND_SEC;
	m->sctp_timeoutticks[SCTP_TIMER_INIT] = SCTP_INIT_SEC;
	m->sctp_timeoutticks[SCTP_TIMER_RECV] = SCTP_RECV_SEC;
	m->sctp_timeoutticks[SCTP_TIMER_HEARTBEAT] = SCTP_HB_DEFAULT;
	m->sctp_timeoutticks[SCTP_TIMER_PMTU] = SCTP_DEF_PMTU_RAISE;
	m->sctp_timeoutticks[SCTP_TIMER_MAXSHUTDOWN] = SCTP_DEF_MAX_SHUTDOWN;
	m->sctp_timeoutticks[SCTP_TIMER_SIGNATURE] = SCTP_DEFAULT_SECRET_LIFE;
	/* all max/min max are in ms */
	m->sctp_maxrto = SCTP_RTO_UPPER_BOUND;
	m->sctp_minrto = SCTP_RTO_LOWER_BOUND;
	m->initial_rto = SCTP_RTO_INITIAL;
	m->initial_init_rto_max = SCTP_RTO_UPPER_BOUND;

	m->max_open_streams_intome = MAX_SCTP_STREAMS;

	m->max_init_times = SCTP_DEF_MAX_INIT;
	m->max_send_times = SCTP_DEF_MAX_SEND;
	m->def_net_failure = SCTP_DEF_MAX_SEND/2;
	m->sctp_sws_sender = SCTP_SWS_SENDER_DEF;
	m->sctp_sws_receiver = SCTP_SWS_RECEIVER_DEF;
	m->max_burst = sctp_max_burst_default;
	/* number of streams to pre-open on a association */
	m->pre_open_stream_count = SCTP_OSTREAM_INITIAL;

	/* Add adaption cookie */
	m->adaption_layer_indicator = 0x504C5253;

	/* seed random number generator */
	m->random_counter = 1;
	m->store_at = SCTP_SIGNATURE_SIZE;
#if defined(__FreeBSD__) && __FreeBSD_version < 500000
	read_random_unlimited(m->random_numbers, sizeof(m->random_numbers));
#elif defined(__APPLE__)
	read_random(m->random_numbers, sizeof(m->random_numbers));
#elif defined(__OpenBSD__)
	get_random_bytes(m->random_numbers, sizeof(m->random_numbers));
#elif defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD_version >= 500000)
#if !defined(__FreeBSD__) && NRND > 0
	rnd_extract_data(m->random_numbers, sizeof(m->random_numbers),
			 RND_EXTRACT_ANY);
#else
	{
		u_int32_t *ranm, *ranp;
		ranp = (u_int32_t *)&m->random_numbers;
		ranm = ranp + SCTP_SIGNATURE_ALOC_SIZE;
		if ((u_long)ranp % 4) {
			/* not a even boundary? */
			ranp = (u_int32_t *)SCTP_SIZE32((u_long)ranp);
		}
		while (ranp < ranm) {
			*ranp = random();
			ranp++;
		}
	}
#endif
#endif
	sctp_fill_random_store(m);

	/* Minimum cookie size */
	m->size_of_a_cookie = (sizeof(struct sctp_init_msg) * 2) +
		sizeof(struct sctp_state_cookie);
	m->size_of_a_cookie += SCTP_SIGNATURE_SIZE;

	/* Setup the initial secret */
	SCTP_GETTIME_TIMEVAL(&time);
	m->time_of_secret_change = time.tv_sec;

	for (i = 0; i < SCTP_NUMBER_OF_SECRETS; i++) {
		m->secret_key[0][i] = sctp_select_initial_TSN(m);
	}
	sctp_timer_start(SCTP_TIMER_TYPE_NEWCOOKIE, inp, NULL, NULL);

	/* How long is a cookie good for ? */
	m->def_cookie_life = SCTP_DEFAULT_COOKIE_LIFE;
	return (error);
}


#ifdef SCTP_TCP_MODEL_SUPPORT
void
sctp_move_pcb_and_assoc(struct sctp_inpcb *old_inp, struct sctp_inpcb *new_inp,
    struct sctp_tcb *stcb)
{
	uint16_t lport, rport;
	struct sctppcbhead *head;
	struct sctp_laddr *laddr, *oladdr;

	new_inp->sctp_ep.time_of_secret_change =
	    old_inp->sctp_ep.time_of_secret_change;
	memcpy(new_inp->sctp_ep.secret_key, old_inp->sctp_ep.secret_key,
	    sizeof(old_inp->sctp_ep.secret_key));
	new_inp->sctp_ep.current_secret_number =
	    old_inp->sctp_ep.current_secret_number;
	new_inp->sctp_ep.last_secret_number =
	    old_inp->sctp_ep.last_secret_number;
	new_inp->sctp_ep.size_of_a_cookie = old_inp->sctp_ep.size_of_a_cookie;

	/* Copy the port across */
	lport = new_inp->sctp_lport = old_inp->sctp_lport;
	rport = stcb->rport;
	/* Pull the tcb from the old association */
	LIST_REMOVE(stcb, sctp_tcbhash);
	LIST_REMOVE(stcb, sctp_tcblist);
	/* Now insert the new_inp into the TCP connected hash */
	head = &sctppcbinfo.sctp_tcpephash[SCTP_PCBHASH_ALLADDR((lport + rport),
	    sctppcbinfo.hashtcpmark)];
	LIST_INSERT_HEAD(head, new_inp, sctp_hash);

	/* Now move the tcb into the endpoint list */
	LIST_INSERT_HEAD(&new_inp->sctp_asoc_list, stcb, sctp_tcblist);
	/*
	 * Question, do we even need to worry about the ep-hash since
	 * we only have one connection? Probably not :> so lets
	 * get rid of it and not suck up any kernel memory in that.
	 */
	stcb->sctp_socket = new_inp->sctp_socket;
	stcb->sctp_ep = new_inp;
	new_inp->highest_tcb = (caddr_t)stcb;
	new_inp->lowest_tcb = (caddr_t)stcb;
	if (new_inp->sctp_tcbhash != NULL) {
		FREE(new_inp->sctp_tcbhash, M_PCB);
		new_inp->sctp_tcbhash = NULL;
	}
	if ((new_inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {
		/* Subset bound, so copy in the laddr list from the old_inp */
		LIST_FOREACH(oladdr, &old_inp->sctp_addr_list, sctp_nxt_addr) {
			laddr = (struct sctp_laddr *)SCTP_ZONE_GET(
			    sctppcbinfo.ipi_zone_laddr);
			if (laddr == NULL) {
				/*
				 * Gak, what can we do? This assoc is really
				 * HOSED. We probably should send an abort
				 * here.
				 */
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_PCB1) {
					printf("Association hosed in TCP model, out of laddr memory\n");
				}
#endif /* SCTP_DEBUG */
				continue;
			}
			sctppcbinfo.ipi_count_laddr++;
			sctppcbinfo.ipi_gencnt_laddr++;
			bzero(laddr, sizeof(*laddr));
			laddr->ifa = oladdr->ifa;
			LIST_INSERT_HEAD(&new_inp->sctp_addr_list, laddr,
			    sctp_nxt_addr);
			new_inp->laddr_count++;
		}
	}
}
#endif


static int
sctp_isport_inuse(struct sctp_inpcb *inp, uint16_t lport)
{
	struct sctppcbhead *head;
	struct sctp_inpcb *t_inp;
	head = &sctppcbinfo.sctp_ephash[SCTP_PCBHASH_ALLADDR(lport,
	    sctppcbinfo.hashmark)];
	LIST_FOREACH(t_inp, head, sctp_hash) {
		if (t_inp->sctp_lport != lport) {
			continue;
		}
		/* This one is in use. */
		/* check the v6/v4 binding issue */
		if ((t_inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
#if defined(__FreeBSD__) 
		    (((struct inpcb *)t_inp)->inp_flags & IN6P_IPV6_V6ONLY)
#else
#if defined(__OpenBSD__)
		    (0)	/* For open bsd we do dual bind only */
#else
		    (((struct in6pcb *)t_inp)->in6p_flags & IN6P_IPV6_V6ONLY)
#endif
#endif
			) {
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				/* collision in V6 space */
				return (1);
			} else {
				/* inp is BOUND_V4 no conflict */
				continue;
			}
		} else if (t_inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
			/* t_inp is bound v4 and v6, conflict always */
			return (1);
		} else {
			/* t_inp is bound only V4 */
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
#if defined(__FreeBSD__)
			    (((struct inpcb *)inp)->inp_flags & IN6P_IPV6_V6ONLY)
#else
#if defined(__OpenBSD__)
			    (0)	/* For open bsd we do dual bind only */
#else
			    (((struct in6pcb *)inp)->in6p_flags & IN6P_IPV6_V6ONLY)
#endif
#endif
				) {
				/* no conflict */
				continue;
			}
			/* else fall through to conflict */
		}
		return (1);
	}
	return (0);
}

#if !(defined(__FreeBSD__) || defined(__APPLE__))
/*
 * Don't know why, but without this there is an unknown reference when
 * compiling NetBSD... hmm
 */
extern void in6_sin6_2_sin (struct sockaddr_in *, struct sockaddr_in6 *sin6);
#endif


int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_inpcb_bind(struct socket *so, struct sockaddr *addr, struct thread *p)
#else
sctp_inpcb_bind(struct socket *so, struct sockaddr *addr, struct proc *p)
#endif
{
	/* bind a ep to a socket address */
	struct sctppcbhead *head;
	struct sctp_inpcb *inp, *inp_tmp;
	struct inpcb *ip_inp;
	int wild, bindall;
	uint16_t lport;
	int error;

	lport = 0;
	error = wild = 0;
	bindall = 1;
	inp = (struct sctp_inpcb *)so->so_pcb;
	ip_inp = (struct inpcb *)so->so_pcb;
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		if (addr) {
			printf("Bind called port:%d\n",
			       ntohs(((struct sockaddr_in *)addr)->sin_port));
			printf("Addr :");
			sctp_print_address(addr);
		}
	}
#endif /* SCTP_DEBUG */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) == 0) {
		/* already did a bind, subsequent binds NOT allowed ! */
		return (EINVAL);
	}

	/*
	 * do we support address re-use? if so I am not sure how. It will
	 * need to be added ...
	 */
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0)
		wild = 1;

	if (addr != NULL) {
		if (addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;

			/* IPV6_V6ONLY socket? */
			if (
#if defined(__FreeBSD__) || defined(__APPLE__)
				(ip_inp->inp_flags & IN6P_IPV6_V6ONLY)
#else
#if defined(__OpenBSD__)
				(0)	/* For openbsd we do dual bind only */
#else
				(((struct in6pcb *)inp)->in6p_flags & IN6P_IPV6_V6ONLY)
#endif
#endif
				) {
				return (EINVAL);
			}

			if (addr->sa_len != sizeof(*sin))
				return (EINVAL);

			sin = (struct sockaddr_in *)addr;
			lport = sin->sin_port;

			if (sin->sin_addr.s_addr != INADDR_ANY) {
				bindall = 0;
			}
		} else if (addr->sa_family == AF_INET6) {
			/* Only for pure IPv6 Address. (No IPv4 Mapped!) */
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)addr;

			if (addr->sa_len != sizeof(*sin6))
				return (EINVAL);

			lport = sin6->sin6_port;
			if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
				bindall = 0;
				/* KAME hack: embed scopeid */
#if defined(SCTP_BASE_FREEBSD) || defined(__APPLE__)
				if (in6_embedscope(&sin6->sin6_addr, sin6,
				    ip_inp, NULL) != 0)
					return (EINVAL);
#elif defined(__FreeBSD__)
				error = scope6_check_id(sin6, ip6_use_defzone);
				if (error != 0)
					return (error);
#else
				if (in6_embedscope(&sin6->sin6_addr, sin6) != 0) {
					return (EINVAL);
				}
#endif
			}
#ifndef SCOPEDROUTING
			/* this must be cleared for ifa_ifwithaddr() */
			sin6->sin6_scope_id = 0;
#endif /* SCOPEDROUTING */
		} else {
			return (EAFNOSUPPORT);
		}
	}
	if (lport) {
		/*
		 * Did the caller specify a port? if so we must see if a
		 * ep already has this one bound.
		 */
		/* got to be root to get at low ports */
		if (ntohs(lport) < IPPORT_RESERVED) {
			if (p && (error =
#ifdef __FreeBSD__
#if __FreeBSD_version >= 500000
			    suser_cred(p->td_ucred, 0)
#else
			    suser(p)
#endif
#elif defined(__NetBSD__) || defined(__APPLE__)
			    suser(p->p_ucred, &p->p_acflag)
#else
			    suser(p, 0)
#endif
			    )) {
				return (error);
			}
		}
		if (p == NULL)
			return (error);

		inp_tmp = sctp_pcb_findep(addr, 0);
		if (inp_tmp != NULL) {
			return (EADDRNOTAVAIL);
		}
		if (bindall) {
			/* verify that no lport is not used by a singleton */
			if (sctp_isport_inuse(inp, lport)) {
				/* Sorry someone already has this one bound */
				return (EADDRNOTAVAIL);
			}
		}
	} else {
		/*
		 * get any port but lets make sure no one has any address
		 * with this port bound
		 */

		/*
		 * setup the inp to the top (I could use the union but this
		 * is just as easy
		 */
		uint16_t first, last, *lastport;

#ifndef __OpenBSD__
		ip_inp->inp_flags |= INP_ANONPORT;
#endif
		if (ip_inp->inp_flags & INP_LOWPORT) {
			if (p && (error =
#ifdef __FreeBSD__
#if __FreeBSD_version >= 500000
			    suser_cred(p->td_ucred, 0)
#else
			    suser(p)
#endif
#elif defined(__NetBSD__) || defined(__APPLE__)
			    suser(p->p_ucred, &p->p_acflag)
#else
			    suser(p, 0)
#endif
			    )) {
				return (error);
			}
			if (p == NULL)
				return (error);

			lastport = &sctppcbinfo.lastlow;
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)

#if defined(__OpenBSD__)
			first = IPPORT_RESERVED-1; /* 1023 */
			last = 600;		   /* not IPPORT_RESERVED/2 */
#else
			first = ipport_lowfirstauto;
			last = ipport_lowlastauto;
#endif

		} else if (ip_inp->inp_flags & INP_HIGHPORT) {
			lastport = &sctppcbinfo.lasthi;
#if defined(__OpenBSD__)
			first = ipport_hifirstauto;	/* sysctl */
			last = ipport_hilastauto;
#else
			first = ipport_hifirstauto;
			last = ipport_hilastauto;
#endif
#else
			first = lowportmin;
			last = lowportmax;
#endif
		} else {
			lastport = &sctppcbinfo.lastport;
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
			first = ipport_firstauto;
			last = ipport_lastauto;
#else
			first = anonportmin;
			last = anonportmax;
#endif
		}
		if (first > last) {
			/* we are assigning from large towards small */
			int max, cnt;
			max = first - last;
			cnt = 0;
			do {
				cnt++;
				if (cnt > max)
					return (EAGAIN);
				if ((*lastport > first) || (*lastport == 0)) {
					*lastport = first;
				} else {
					(*lastport)--;
				}
				lport = htons(*lastport);
			} while (sctp_isport_inuse(inp, lport));
		} else {
			/* we are assigning from small towards large */
			int max, cnt;
			max = last - first;
			cnt = 0;
			do {
				cnt++;
				if (cnt > max)
					return (EAGAIN);

				if (*lastport > last)
					*lastport = first;

				if (*lastport < first) {
					*lastport = first;
				} else {
					(*lastport)++;
				}
				lport = htons(*lastport);
			} while (sctp_isport_inuse(inp, lport));
		}
	}
	/* ok we look clear to give out this port, so lets setup the binding */
	if (bindall) {
		/* binding to all addresses, so just set in the proper flags */
		inp->sctp_flags |= (SCTP_PCB_FLAGS_BOUNDALL |
		    SCTP_PCB_FLAGS_DO_ASCONF);
		/* set the automatic addr changes from kernel flag */
		if (sctp_auto_asconf == 0) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_AUTO_ASCONF;
		} else {
			inp->sctp_flags |= SCTP_PCB_FLAGS_AUTO_ASCONF;
		}
	} else {
		/*
		 * bind specific, make sure flags is off and add a new address
		 * structure to the sctp_addr_list inside the ep structure.
		 *
		 * We will need to allocate one and insert it at the head.
		 * The socketopt call can just insert new addresses in there
		 * as well. It will also have to do the embed scope kame hack
		 * too (before adding).
		 */
		struct ifaddr *ifa;
		struct sockaddr_storage store_sa;

		memset(&store_sa, 0, sizeof(store_sa));
		if (addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;

			sin = (struct sockaddr_in *)&store_sa;
			memcpy(sin, addr, sizeof(struct sockaddr_in));
			sin->sin_port = 0;
		} else if (addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)&store_sa;
			memcpy(sin6, addr, sizeof(struct sockaddr_in6));
			sin6->sin6_port = 0;
		}
		/*
		 * first find the interface with the bound address
		 * need to zero out the port to find the address! yuck!
		 * can't do this earlier since need port for sctp_pcb_findep()
		 */
		ifa = sctp_find_ifa_by_addr((struct sockaddr *)&store_sa);
		if (ifa == NULL) {
			/* Can't find an interface with that address */
			return (EADDRNOTAVAIL);
		}
		if (addr->sa_family == AF_INET6) {
			struct in6_ifaddr *ifa6;
			ifa6 = (struct in6_ifaddr *)ifa;
			/*
			 * allow binding of deprecated addresses as per
			 * RFC 2462 and ipng discussion
			 */
			if (ifa6->ia6_flags & (IN6_IFF_DETACHED |
			    IN6_IFF_ANYCAST |
			    IN6_IFF_NOTREADY)) {
				/* Can't bind a non-existent addr. */
				return (EINVAL);
			}
		}
		/* we're not bound all */
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_BOUNDALL;
#if 0 /* use sysctl now */
		/* don't allow automatic addr changes from kernel */
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_AUTO_ASCONF;
#endif
		/* set the automatic addr changes from kernel flag */
		if (sctp_auto_asconf == 0) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_AUTO_ASCONF;
		} else {
			inp->sctp_flags |= SCTP_PCB_FLAGS_AUTO_ASCONF;
		}
		/* allow bindx() to send ASCONF's for binding changes */
		inp->sctp_flags |= SCTP_PCB_FLAGS_DO_ASCONF;
		/* add this address to the endpoint list */
		error = sctp_insert_laddr(&inp->sctp_addr_list, ifa);
		if (error != 0)
			return (error);
		inp->laddr_count++;
	}
	/* find the bucket */
	head = &sctppcbinfo.sctp_ephash[SCTP_PCBHASH_ALLADDR(lport,
	    sctppcbinfo.hashmark)];
	/* put it in the bucket */
	LIST_INSERT_HEAD(head, inp, sctp_hash);
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Main hash to bind at head:%p, bound port:%d\n", head, ntohs(lport));
	}
#endif
	/* set in the port */
	inp->sctp_lport = lport;
	/* turn off just the unbound flag */
	inp->sctp_flags &= ~SCTP_PCB_FLAGS_UNBOUND;
	return (0);
}


/* release sctp_inpcb unbind the port */
void
sctp_inpcb_free(struct sctp_inpcb *inp, int immediate)
{
	/*
	 * Here we free a endpoint. We must find it (if it is in the Hash
	 * table) and remove it from there. Then we must also find it in
	 * the overall list and remove it from there. After all removals are
	 * complete then any timer has to be stopped. Then start the actual
	 * freeing.
	 * a) Any local lists.
	 * b) Any associations.
	 * c) The hash of all associations.
	 * d) finally the ep itself.
	 */
	struct sctp_pcb *m;
	struct sctp_tcb *asoc, *nasoc;
	struct sctp_laddr *laddr, *nladdr;
	struct inpcb *ip_pcb;
	struct socket *so;
	struct sctp_socket_q_list *sq;
	struct rtentry *rt;
	int s, cnt;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
		/* been here before */
		splx(s);
		printf("Endpoint was all gone (dup free)?\n");
		return;
	}
	sctp_timer_stop(SCTP_TIMER_TYPE_NEWCOOKIE, inp, NULL, NULL);
	if (inp->control) {
		sctp_m_freem(inp->control);
		inp->control = NULL;
	}
	if (inp->pkt) {
		sctp_m_freem(inp->pkt);
		inp->pkt = NULL;
	}
	so  = inp->sctp_socket;
	m = &inp->sctp_ep;
	ip_pcb = &inp->ip_inp.inp; /* we could just cast the main
				   * pointer here but I will
				   * be nice :> ( i.e. ip_pcb = ep;)
				   */

	if (immediate == 0) {
		int cnt_in_sd;
		cnt_in_sd = 0;
		for ((asoc = LIST_FIRST(&inp->sctp_asoc_list)); asoc != NULL;
		     asoc = nasoc) {
			nasoc = LIST_NEXT(asoc, sctp_tcblist);
			if ((SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_COOKIE_WAIT) ||
			    (SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_COOKIE_ECHOED)) {
				/* Just abandon things in the front states */
				sctp_free_assoc(inp, asoc);
				continue;
			} else {
				asoc->asoc.state |= SCTP_STATE_CLOSED_SOCKET;
			}
			if ((asoc->asoc.size_on_delivery_queue  > 0) ||
			    (asoc->asoc.size_on_reasm_queue > 0) ||
			    (asoc->asoc.size_on_all_streams > 0) ||
			    (so && (so->so_rcv.sb_cc > 0))
				) {
				/* Left with Data unread */
				struct mbuf *op_err;
				MGET(op_err, M_DONTWAIT, MT_DATA);
				if (op_err) {
					/* Fill in the user initiated abort */
					struct sctp_paramhdr *ph;
					op_err->m_len =
					    sizeof(struct sctp_paramhdr);
					ph = mtod(op_err,
					    struct sctp_paramhdr *);
					ph->param_type = htons(
					    SCTP_CAUSE_USER_INITIATED_ABT);
					ph->param_length = htons(op_err->m_len);
				}
				sctp_send_abort_tcb(asoc, op_err);
				sctp_free_assoc(inp, asoc);
				continue;
			} else if (TAILQ_EMPTY(&asoc->asoc.send_queue) &&
			    TAILQ_EMPTY(&asoc->asoc.sent_queue)) {
				if ((SCTP_GET_STATE(&asoc->asoc) != SCTP_STATE_SHUTDOWN_SENT) &&
				    (SCTP_GET_STATE(&asoc->asoc) != SCTP_STATE_SHUTDOWN_ACK_SENT)) {
					/* there is nothing queued to send, so I send shutdown */
					sctp_send_shutdown(asoc, asoc->asoc.primary_destination);
					asoc->asoc.state = SCTP_STATE_SHUTDOWN_SENT;
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN, asoc->sctp_ep, asoc,
							 asoc->asoc.primary_destination);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD, asoc->sctp_ep, asoc,
							 asoc->asoc.primary_destination);
					sctp_chunk_output(inp, asoc, 1);
				}
			} else {
				/* mark into shutdown pending */
				asoc->asoc.state |= SCTP_STATE_SHUTDOWN_PENDING;
			}
			cnt_in_sd++;
		}
		/* now is there some left in our SHUTDOWN state? */ 
		if (cnt_in_sd) {
			inp->sctp_flags |= SCTP_PCB_FLAGS_SOCKET_GONE;
			splx(s);
			return;
		}
	}
	inp->sctp_flags |= SCTP_PCB_FLAGS_SOCKET_ALLGONE;
	rt = ip_pcb->inp_route.ro_rt;
	if (so) {
	/* First take care of socket level things */
#ifdef IPSEC
#ifdef __OpenBSD__
	/* XXX IPsec cleanup here */
	    {
		int s2 = spltdb();
		if (ip_pcb->inp_tdb_in)
		    TAILQ_REMOVE(&ip_pcb->inp_tdb_in->tdb_inp_in,
				 ip_pcb, inp_tdb_in_next);
		if (ip_pcb->inp_tdb_out)
		    TAILQ_REMOVE(&ip_pcb->inp_tdb_out->tdb_inp_out, ip_pcb,
				 inp_tdb_out_next);
		if (ip_pcb->inp_ipsec_localid)
		    ipsp_reffree(ip_pcb->inp_ipsec_localid);
		if (ip_pcb->inp_ipsec_remoteid)
		    ipsp_reffree(ip_pcb->inp_ipsec_remoteid);
		if (ip_pcb->inp_ipsec_localcred)
		    ipsp_reffree(ip_pcb->inp_ipsec_localcred);
		if (ip_pcb->inp_ipsec_remotecred)
		    ipsp_reffree(ip_pcb->inp_ipsec_remotecred);
		if (ip_pcb->inp_ipsec_localauth)
		    ipsp_reffree(ip_pcb->inp_ipsec_localauth);
		if (ip_pcb->inp_ipsec_remoteauth)
		    ipsp_reffree(ip_pcb->inp_ipsec_remoteauth);
		splx(s2);
	    }
#else
	    ipsec4_delete_pcbpolicy(ip_pcb);
#endif
#endif /*IPSEC*/
	    so->so_pcb = 0;
	    sofree(so);
	}

	if (ip_pcb->inp_options) {
		(void)m_free(ip_pcb->inp_options);
		ip_pcb->inp_options = 0;
	}
	if (rt) {
		RTFREE(rt);
		ip_pcb->inp_route.ro_rt = 0;
	}
	if (ip_pcb->inp_moptions) {
		ip_freemoptions(ip_pcb->inp_moptions);
		ip_pcb->inp_moptions = 0;
	}
#if !(defined(__FreeBSD__) || defined(__APPLE__))
	inp->inp_vflag = 0;
#else
	ip_pcb->inp_vflag = 0;
#endif

	/* Now the sctp_pcb things */

	/*
	 * free each asoc if it is not already closed/free. we can't use
	 * the macro here since le_next will get freed as part of the
	 * sctp_free_assoc() call.
	 */
	cnt = 0;
	for ((asoc = LIST_FIRST(&inp->sctp_asoc_list)); asoc != NULL;
	     asoc = nasoc) {
		nasoc = LIST_NEXT(asoc, sctp_tcblist);
		if (SCTP_GET_STATE(&asoc->asoc) != SCTP_STATE_COOKIE_WAIT) {
			struct mbuf *op_err;
			MGET(op_err, M_DONTWAIT, MT_DATA);
			if (op_err) {
				/* Fill in the user initiated abort */
				struct sctp_paramhdr *ph;
				op_err->m_len = sizeof(struct sctp_paramhdr);
				ph = mtod(op_err, struct sctp_paramhdr *);
				ph->param_type = htons(
				    SCTP_CAUSE_USER_INITIATED_ABT);
				ph->param_length = htons(op_err->m_len);
			}
			sctp_send_abort_tcb(asoc, op_err);
		}
		cnt++;
		/*
		 * sctp_free_assoc() will call sctp_inpcb_free(),
		 * if SCTP_PCB_FLAGS_SOCKET_GONE set.
		 * So, we clear it before sctp_free_assoc() making sure
		 * no double sctp_inpcb_free().
		 */
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_SOCKET_GONE;
		sctp_free_assoc(inp, asoc);
	}
	while ((sq = TAILQ_FIRST(&inp->sctp_queue_list)) != NULL) {
		TAILQ_REMOVE(&inp->sctp_queue_list, sq, next_sq);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_sockq, sq);
		sctppcbinfo.ipi_count_sockq--;
		sctppcbinfo.ipi_gencnt_sockq++;
	}
	inp->sctp_socket = 0;
	/* Now first we remove ourselves from the overall list of all EP's */
	LIST_REMOVE(inp, sctp_list);

	/*
	 * Now the question comes as to if this EP was ever bound at all.
	 * If it was, then we must pull it out of the EP hash list.
	 */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) !=
	    SCTP_PCB_FLAGS_UNBOUND) {
		/*
		 * ok, this guy has been bound. It's port is somewhere
		 * in the sctppcbinfo hash table. Remove it!
		 */
		LIST_REMOVE(inp, sctp_hash);
	}
	/*
	 * if we have an address list the following will free the list of
	 * ifaddr's that are set into this ep. Again macro limitations here,
	 * since the LIST_FOREACH could be a bad idea.
	 */
#ifndef SCTP_VTAG_TIMEWAIT_PER_STACK
	{
		int i;
		struct sctp_tagblock *tb, *ntb;
		/* Free anything in the vtag_waitblock */
		for (i = 0; i < SCTP_NUMBER_IN_VTAG_BLOCK; i++) {
			tb = LIST_FIRST(&inp->vtag_timewait[i]);
			while (tb) {
				ntb = LIST_NEXT(tb, sctp_nxt_tagblock);
				LIST_REMOVE(tb, sctp_nxt_tagblock);
				FREE(tb, M_PCB);
				tb = ntb;
			}
		}
	}
#endif /* !SCTP_VTAG_TIMEWAIT_PER_STACK */
	for ((laddr = LIST_FIRST(&inp->sctp_addr_list)); laddr != NULL;
	     laddr = nladdr) {
		nladdr = LIST_NEXT(laddr, sctp_nxt_addr);
		LIST_REMOVE(laddr, sctp_nxt_addr);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_laddr, laddr);
		sctppcbinfo.ipi_gencnt_laddr++;
		sctppcbinfo.ipi_count_laddr--;
	}
	/* Now lets see about freeing the EP hash table. */
	if (inp->sctp_tcbhash != NULL) {
		FREE(inp->sctp_tcbhash, M_PCB);
		inp->sctp_tcbhash = 0;
	}

	/* Now we must put the ep memory back into the zone pool */
	SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_ep, inp);
	sctppcbinfo.ipi_count_ep--;
	splx(s);
}


struct sctp_nets *
sctp_findnet(struct sctp_tcb *stcb, struct sockaddr *addr)
{
	struct sctp_nets *net;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	/* use the peer's/remote port for lookup if unspecified */
	sin = (struct sockaddr_in *)addr;
	sin6 = (struct sockaddr_in6 *)addr;
#if 0 /* why do we need to check the port for a nets list on an assoc? */
	if (stcb->rport != sin->sin_port) {
		/* we cheat and just a sin for this test */
		return (NULL);
	}
#endif
	/* locate the address */
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		if (sctp_cmpaddr(addr, (struct sockaddr *)&net->ra._l_addr))
			return (net);
	}
	return (NULL);
}


/*
 * add's a remote endpoint address, done with the INIT/INIT-ACK
 * as well as when a ASCONF arrives that adds it. It will also
 * initialize all the cwnd stats of stuff.
 */
int
sctp_is_address_on_local_host(struct sockaddr *addr)
{
	struct ifnet *ifn;
	struct ifaddr *ifa;
	TAILQ_FOREACH(ifn, &ifnet, if_list) {
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			if (addr->sa_family == ifa->ifa_addr->sa_family) {
				/* same family */
				if (addr->sa_family == AF_INET) {
					struct sockaddr_in *sin, *sin_c;
					sin = (struct sockaddr_in *)addr;
					sin_c = (struct sockaddr_in *)
					    ifa->ifa_addr;
					if (sin->sin_addr.s_addr ==
					    sin_c->sin_addr.s_addr) {
						/* we are on the same machine */
						return (1);
					}
				} else if (addr->sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6, *sin_c6;
					sin6 = (struct sockaddr_in6 *)addr;
					sin_c6 = (struct sockaddr_in6 *)
					    ifa->ifa_addr;
					if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &sin_c6->sin6_addr)) {
						/* we are on the same machine */
						return (1);
					}
				}
			}
		}
	}
	return (0);
}

int
sctp_add_remote_addr(struct sctp_tcb *stcb, struct sockaddr *newaddr,
    int set_scope, int from)
{
	/*
	 * The following is redundant to the same lines in the
	 * sctp_aloc_assoc() but is needed since other's call the add
	 * address function
	 */
	struct sctp_nets *net, *netfirst;
	int addr_inscope;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Adding an address (from:%d) to the peer: ", from);
		sctp_print_address(newaddr);
	}
#endif
	netfirst = sctp_findnet(stcb, newaddr);
	if (netfirst) {
		/*
		 * Lie and return ok, we don't want to make the association
		 * go away for this behavior. It will happen in the TCP model
		 * in a connected socket. It does not reach the hash table
		 * until after the association is built so it can't be found.
		 * Mark as reachable, since the initial creation will have
		 * been cleared and the NOT_IN_ASSOC flag will have been
		 * added... and we don't want to end up removing it back out.
		 */
		if (netfirst->dest_state & SCTP_ADDR_UNCONFIRMED) {
			netfirst->dest_state = (SCTP_ADDR_REACHABLE|
			    SCTP_ADDR_UNCONFIRMED);
		} else {
			netfirst->dest_state = SCTP_ADDR_REACHABLE;
		}

		return (0);
	}
	addr_inscope = 1;
	if (newaddr->sa_family == AF_INET) {
		struct sockaddr_in *sin;
		sin = (struct sockaddr_in *)newaddr;
		if ((sin->sin_port == 0) || (sin->sin_addr.s_addr == 0)) {
			/* Invalid address */
			return (-1);
		}
		/* zero out the bzero area */
		memset(&sin->sin_zero, 0, sizeof(sin->sin_zero));

		/* assure len is set */
		sin->sin_len = sizeof(struct sockaddr_in);
		if (set_scope) {
#ifdef SCTP_DONT_DO_PRIVADDR_SCOPE
			stcb->ipv4_local_scope = 1;
#else
			if (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)) {
				stcb->asoc.ipv4_local_scope = 1;
			}
#endif /* SCTP_DONT_DO_PRIVADDR_SCOPE */
			
			if (sctp_is_address_on_local_host(newaddr)) {
				stcb->asoc.loopback_scope = 1;
				stcb->asoc.ipv4_local_scope = 1;
				stcb->asoc.local_scope = 1;
				stcb->asoc.site_scope = 1;
			}
		} else {
			if (from == 8) {
				/* From connectx */
				if (sctp_is_address_on_local_host(newaddr)) {
					stcb->asoc.loopback_scope = 1;
					stcb->asoc.ipv4_local_scope = 1;
					stcb->asoc.local_scope = 1;
					stcb->asoc.site_scope = 1;
				}
			}
			/* Validate the address is in scope */
			if ((IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)) &&
			    (stcb->asoc.ipv4_local_scope == 0)) {
				addr_inscope = 0;
			}
		}
	} else if (newaddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)newaddr;
		if ((sin6->sin6_port == 0) ||
		    (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))) {
			/* Invalid address */
			return (-1);
		}
		/* assure len is set */
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		if (set_scope) {
			if (sctp_is_address_on_local_host(newaddr)) {
				stcb->asoc.loopback_scope = 1;
				stcb->asoc.local_scope = 1;
				stcb->asoc.ipv4_local_scope = 1;
				stcb->asoc.site_scope = 1;
			} else if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				/*
				 * If the new destination is a LINK_LOCAL
				 * we must have common site scope. Don't set
				 * the local scope since we may not share all
				 * links, only loopback can do this.
				 */
				stcb->asoc.site_scope = 1;
			} else if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr)) {
				/*
				 * If the new destination is SITE_LOCAL
				 * then we must have site scope in common.
				 */
				stcb->asoc.site_scope = 1;
			}
		} else {
			if (from == 8) {
				/* From connectx */
				if (sctp_is_address_on_local_host(newaddr)) {
					stcb->asoc.loopback_scope = 1;
					stcb->asoc.ipv4_local_scope = 1;
					stcb->asoc.local_scope = 1;
					stcb->asoc.site_scope = 1;
				}
			}
			/* Validate the address is in scope */
			if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr) &&
			    (stcb->asoc.loopback_scope == 0)) {
				addr_inscope = 0;
			} else if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
				   (stcb->asoc.local_scope == 0)) {
				addr_inscope = 0;
			} else if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr) &&
				   (stcb->asoc.site_scope == 0)) {
				addr_inscope = 0;
			}
		}
	} else {
		/* not supported family type */
		return (-1);
	}
	net = (struct sctp_nets *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_raddr);
	if (net == NULL) {
		return (-1);
	}
	sctppcbinfo.ipi_count_raddr++;
	sctppcbinfo.ipi_gencnt_raddr++;
	bzero(net, sizeof(*net));
	memcpy(&net->ra._l_addr, newaddr, newaddr->sa_len);
#if defined(__FreeBSD__) || defined(__APPLE__)
	if (newaddr->sa_family == AF_INET6)
		net->addr_is_local = in6_localaddr(
		    &(((struct sockaddr_in6 *)newaddr)->sin6_addr));
	else if (newaddr->sa_family == AF_INET)
		net->addr_is_local = in_localaddr(
		    ((struct sockaddr_in *)newaddr)->sin_addr);
#else
	net->addr_is_local = 0;
#endif

	net->failure_threshold = stcb->asoc.def_net_failure;
	if (addr_inscope == 0) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB1) {
			printf("Adding an address which is OUT OF SCOPE\n");
		}
#endif /* SCTP_DEBUG */
		net->dest_state = (SCTP_ADDR_REACHABLE |
		    SCTP_ADDR_OUT_OF_SCOPE);
	} else {
		if (from == 8)
			/* 8 is passed by connect_x */
			net->dest_state = SCTP_ADDR_REACHABLE;
		else 
			net->dest_state = SCTP_ADDR_REACHABLE |
			    SCTP_ADDR_UNCONFIRMED;
	}
	net->RTO = 0;
	stcb->asoc.numnets++;
	net->ref_count = 1;

	/* Init the timer structure */
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	callout_init(&net->rxt_timer.timer, 0);
#else
	callout_init(&net->rxt_timer.timer);
#endif

	/* Now generate a route for this guy */
	/* KAME hack: embed scopeid */
	if (newaddr->sa_family == AF_INET6 ) {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)&net->ra._l_addr;
#if defined(SCTP_BASE_FREEBSD) || defined(__APPLE__)
		(void)in6_embedscope(&sin6->sin6_addr, sin6,
		    &stcb->sctp_ep->ip_inp.inp, NULL);
#else
		(void)in6_embedscope(&sin6->sin6_addr, sin6);
#endif
#ifndef SCOPEDROUTING
		sin6->sin6_scope_id = 0;
#endif
	}
#if defined(__FreeBSD__) || defined(__APPLE__)
	net->ra.ro_rt = rtalloc1((struct sockaddr *)&net->ra._l_addr, 1, 0UL);
#else
	net->ra.ro_rt = rtalloc1((struct sockaddr *)&net->ra._l_addr, 1);
#endif
	if (newaddr->sa_family == AF_INET6 ) {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)&net->ra._l_addr;
		(void)in6_recoverscope(sin6, &sin6->sin6_addr, NULL);
	}
	if ((net->ra.ro_rt) && 
	    (net->ra.ro_rt->rt_ifp)) {
		net->mtu = net->ra.ro_rt->rt_ifp->if_mtu;
		if (from == 1) {
			stcb->asoc.smallest_mtu = net->mtu;
		}
		/* start things off to match mtu of interface please. */
		net->ra.ro_rt->rt_rmx.rmx_mtu = net->ra.ro_rt->rt_ifp->if_mtu;
	} else {
		net->mtu = stcb->asoc.smallest_mtu;
	}
	if (stcb->asoc.smallest_mtu > net->mtu) {
		stcb->asoc.smallest_mtu = net->mtu;
	}
	if (net->addr_is_local) {
		net->cwnd = max((net->mtu * 4), SCTP_INITIAL_CWND);
	} else {
		net->cwnd = min((net->mtu * 4), max((2*net->mtu),
		    SCTP_INITIAL_CWND));
	}
	if (net->cwnd < (2 * net->mtu)) {
		net->cwnd = 2 * net->mtu;
	}
	net->ssthresh = stcb->asoc.peers_rwnd;

	net->src_addr_selected = 0;
	netfirst = TAILQ_FIRST(&stcb->asoc.nets);
	if (net->ra.ro_rt == NULL) {
		/* Since we have no route put it at the back */
		TAILQ_INSERT_TAIL(&stcb->asoc.nets, net, sctp_next);
	} else if (netfirst == NULL) {
		/* We are the first one in the pool. */
		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
	} else if (netfirst->ra.ro_rt == NULL) {
		/*
		 * First one has NO route. Place this one ahead of the
		 * first one.
		 */
		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
	} else if (net->ra.ro_rt->rt_ifp != netfirst->ra.ro_rt->rt_ifp) {
		/*
		 * This one has a different interface than the one at the
		 * top of the list. Place it ahead.
		 */
		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
	} else {
		/*
		 * Ok we have the same interface as the first one. Move
		 * forward until we find either
		 *   a) one with a NULL route... insert ahead of that
		 *   b) one with a different ifp.. insert after that.
		 *   c) end of the list.. insert at the tail.
		 */
		struct sctp_nets *netlook;
		do {
			netlook = TAILQ_NEXT(netfirst, sctp_next);
			if (netlook == NULL) {
				/* End of the list */
				TAILQ_INSERT_TAIL(&stcb->asoc.nets, net,
				    sctp_next);
				break;
			} else if (netlook->ra.ro_rt == NULL) {
				/* next one has NO route */
				TAILQ_INSERT_BEFORE(netfirst, net, sctp_next);
				break;
			} else if (netlook->ra.ro_rt->rt_ifp !=
				   net->ra.ro_rt->rt_ifp) {
				TAILQ_INSERT_AFTER(&stcb->asoc.nets, netlook,
				    net, sctp_next);
				break;
			}
			/* Shift forward */
			netfirst = netlook;
		} while (netlook != NULL);
	}
	/* got to have a primary set */
	if (stcb->asoc.primary_destination == 0) {
		stcb->asoc.primary_destination = net;
	} else if ((stcb->asoc.primary_destination->ra.ro_rt == NULL) &&
		   (net->ra.ro_rt)) {
		/* No route to current primary adopt new primary */
		stcb->asoc.primary_destination = net;
	}
	sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, stcb->sctp_ep, stcb,
	    net);

	return (0);
}


/*
 * allocate an association and add it to the endpoint. The caller must
 * be careful to add all additional addresses once they are know right
 * away or else the assoc will be may experience a blackout scenario.
 */
struct sctp_tcb *
sctp_aloc_assoc(struct sctp_inpcb *inp, struct sockaddr *firstaddr,
    int for_a_init, int *error)
{
	struct sctp_tcb *stcb;
	struct sctp_association *asoc;
	struct sctpasochead *head;
	uint16_t rport;
	int err;

	/*
	 * Assumption made here:
	 *  Caller has done a sctp_findassociation_ep_addr(ep, addr's);
	 *  to make sure the address does not exist already.
	 */
	if (sctppcbinfo.ipi_count_asoc >= SCTP_MAX_NUM_OF_ASOC) {
		/* Hit max assoc, sorry no more */
		*error = ENOBUFS;
		return (NULL);
	}

#ifdef SCTP_TCP_MODEL_SUPPORT
	if (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) {
		/*
		 * If its in the TCP pool, its NOT allowed to create an
		 * association. The parent listener needs to call
		 * sctp_alloc_assoc.. or the one-2-many socket. If a
		 * peeled off, or connected one does this.. its an error.
		 */
		*error = EINVAL;
		return (NULL);
 	}
#endif

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB3) {
		printf("Allocate an association for peer:");
		if (firstaddr)
			sctp_print_address(firstaddr);
		else
			printf("None\n");
		printf("Port:%d\n",
		       ntohs(((struct sockaddr_in *)firstaddr)->sin_port));
	}
#endif /* SCTP_DEBUG */
	if (firstaddr->sa_family == AF_INET) {
		struct sockaddr_in *sin;
		sin = (struct sockaddr_in *)firstaddr;
		if ((sin->sin_port == 0) || (sin->sin_addr.s_addr == 0)) {
			/* Invalid address */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB3) {
				printf("peer address invalid\n");
			}
#endif
			*error = EINVAL;
			return (NULL);
		}
		rport = sin->sin_port;
	} else if (firstaddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)firstaddr;
		if ((sin6->sin6_port == 0) ||
		    (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))) {
			/* Invalid address */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB3) {
				printf("peer address invalid\n");
			}
#endif
			*error = EINVAL;
			return (NULL);
		}
		rport = sin6->sin6_port;
	} else {
		/* not supported family type */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB3) {
			printf("BAD family %d\n", firstaddr->sa_family);
		}
#endif
		*error = EINVAL;
		return (NULL);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) {
		/*
		 * If you have not performed a bind, then we need to do
		 * the ephemerial bind for you.
		 */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB3) {
			printf("Doing implicit BIND\n");
		}
#endif
		if ((err = sctp_inpcb_bind(inp->sctp_socket,
		    (struct sockaddr *)NULL,
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
		    (struct thread *)NULL))) {
#else
		    (struct proc *)NULL))) {
#endif
			/* bind error, probably perm */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB3) {
				printf("BIND FAILS ret:%d\n", err);
			}
#endif
			*error = err;
			return (NULL);
		}
	}

	stcb = (struct sctp_tcb *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_asoc);
	if (stcb == NULL) {
		/* out of memory? */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB3) {
			printf("aloc_assoc: no assoc mem left, stcb=NULL\n");
		}
#endif
		*error = ENOMEM;
		return (NULL);
	}
	sctppcbinfo.ipi_count_asoc++;
	sctppcbinfo.ipi_gencnt_asoc++;

	bzero(stcb, sizeof(*stcb));
	asoc = &stcb->asoc;
	if ((err = sctp_init_asoc(inp, asoc, for_a_init))) {
		/* failed */
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, stcb);
		sctppcbinfo.ipi_count_asoc--;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB3) {
			printf("aloc_assoc: couldn't init asoc, out of mem?!\n");
		}
#endif
		*error = err;
		return (NULL);
	}
	/* setup back pointer's */
	stcb->sctp_ep = inp;
	stcb->sctp_socket = inp->sctp_socket;

	/* and the port */
	stcb->rport = rport;

	/* now that my_vtag is set, add it to the  hash */
	head = &sctppcbinfo.sctp_asochash[SCTP_PCBHASH_ASOC(stcb->asoc.my_vtag,
	     sctppcbinfo.hashasocmark)];
	/* put it in the bucket in the vtag hash of assoc's for the system */
	LIST_INSERT_HEAD(head, stcb, sctp_asocs);

	if ((err = sctp_add_remote_addr(stcb, firstaddr, 1, 1))) {
		/* failure.. memory error? */
		if (asoc->strmout)
			FREE(asoc->strmout, M_PCB);
		if (asoc->mapping_array)
			FREE(asoc->mapping_array, M_PCB);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, stcb);
		sctppcbinfo.ipi_count_asoc--;
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_PCB3) {
			printf("aloc_assoc: couldn't add remote addr!\n");
		}
#endif
		*error = ENOBUFS;
		return (NULL);
	}
	if ((caddr_t)stcb < inp->lowest_tcb) {
		inp->lowest_tcb = (caddr_t)stcb;
	}
	if ((caddr_t)stcb > inp->highest_tcb) {
		inp->highest_tcb = (caddr_t)stcb;
	}
	if ((caddr_t)stcb < sctp_lowest_tcb) {
		sctp_lowest_tcb = (caddr_t)stcb;
	}
	if ((caddr_t)stcb > sctp_highest_tcb) {
		sctp_highest_tcb = (caddr_t)stcb;
	}

	/* Init all the timers */
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	callout_init(&asoc->hb_timer.timer, 0);
	callout_init(&asoc->dack_timer.timer, 0);
	callout_init(&asoc->asconf_timer.timer, 0);
	callout_init(&asoc->shut_guard_timer.timer, 0);
	callout_init(&asoc->autoclose_timer.timer, 0);
#else
	callout_init(&asoc->hb_timer.timer);
	callout_init(&asoc->dack_timer.timer);
	callout_init(&asoc->asconf_timer.timer);
	callout_init(&asoc->shut_guard_timer.timer);
	callout_init(&asoc->autoclose_timer.timer);
#endif

	LIST_INSERT_HEAD(&inp->sctp_asoc_list, stcb, sctp_tcblist);

	/* now file the port under the hash as well */
	if (inp->sctp_tcbhash != NULL) {
		head = &inp->sctp_tcbhash[SCTP_PCBHASH_ALLADDR(stcb->rport,
		   inp->sctp_hashmark)];
		LIST_INSERT_HEAD(head, stcb, sctp_tcbhash);
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Association %p now allocated\n", stcb);
	}
#endif
	return (stcb);
}

void
sctp_free_remote_addr(struct sctp_nets *net)
{
	if (net == NULL)
		return;
	net->ref_count--;
	if (net->ref_count <= 0) {
		/* stop timer if running */
		callout_stop(&net->rxt_timer.timer);
		callout_stop(&net->pmtu_timer.timer);
		net->dest_state = SCTP_ADDR_NOT_REACHABLE;
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_raddr, net);
		sctppcbinfo.ipi_count_raddr--;
	}
}

/*
 * remove a remote endpoint address from an association, it
 * will fail if the address does not exist.
 */
int
sctp_del_remote_addr(struct sctp_tcb *stcb, struct sockaddr *remaddr)
{
	/*
	 * Here we need to remove a remote address. This is quite simple, we
	 * first find it in the list of address for the association
	 * (tasoc->asoc.nets) and then if it is there, we do a LIST_REMOVE on
	 * that item.
	 * Note we do not allow it to be removed if there are no other
	 * addresses.
	 */
	struct sctp_association *asoc;
	struct sctp_nets *net, *net_tmp;
	asoc = &stcb->asoc;
	if (asoc->numnets < 2) {
		/* Must have at LEAST two remote addresses */
		return (-1);
	}
	/* locate the address */
	for (net = TAILQ_FIRST(&asoc->nets); net != NULL; net = net_tmp) {
		net_tmp = TAILQ_NEXT(net, sctp_next);
		if (net->ra._l_addr.sa.sa_family != remaddr->sa_family) {
			continue;
		}
		if (sctp_cmpaddr((struct sockaddr *)&net->ra._l_addr,
		    remaddr)) {
			/* we found the guy */
			asoc->numnets--;
			TAILQ_REMOVE(&asoc->nets, net, sctp_next);
			sctp_free_remote_addr(net);
			if (net == asoc->primary_destination) {
				/* Reset primary */
				struct sctp_nets *lnet;
				lnet = TAILQ_FIRST(&asoc->nets);
				/* Try to find a confirmed primary */
				asoc->primary_destination =
				    sctp_find_alternate_net(stcb, lnet);
			}
			if (net == asoc->last_data_chunk_from) {
				/* Reset primary */
				asoc->last_data_chunk_from =
				    TAILQ_FIRST(&asoc->nets);
			}
			if (net == asoc->last_control_chunk_from) {
				/* Reset primary */
				asoc->last_control_chunk_from =
				    TAILQ_FIRST(&asoc->nets);
			}
			if (net == asoc->asconf_last_sent_to) {
				/* Reset primary */
				asoc->asconf_last_sent_to =
				    TAILQ_FIRST(&asoc->nets);
			}
			return (0);
		}
	}
	/* not found. */
	return (-2);
}


static void
sctp_add_vtag_to_timewait(struct sctp_inpcb *inp, u_int32_t tag)
{
	struct sctpvtaghead *chain;
	struct sctp_tagblock *twait_block;
	struct timeval now;
	int set, i;
	SCTP_GETTIME_TIMEVAL(&now);
#ifdef SCTP_VTAG_TIMEWAIT_PER_STACK
	chain = &sctppcbinfo.vtag_timewait[(tag % SCTP_STACK_VTAG_HASH_SIZE)];
#else
	chain = &inp->vtag_timewait[(tag % SCTP_STACK_VTAG_HASH_SIZE)];
#endif
	set = 0;
	if (!LIST_EMPTY(chain)) {
		/* Block(s) present, lets find space, and expire on the fly */
		LIST_FOREACH(twait_block, chain, sctp_nxt_tagblock) {
			for (i = 0; i < SCTP_NUMBER_IN_VTAG_BLOCK; i++) {
				if ((twait_block->vtag_block[i].v_tag == 0) &&
				    !set) {
					twait_block->vtag_block[0].tv_sec_at_expire =
					    now.tv_sec + SCTP_TIME_WAIT;
					twait_block->vtag_block[0].v_tag = tag;
					set = 1;
				} else if ((twait_block->vtag_block[i].v_tag) &&
				    (twait_block->vtag_block[i].tv_sec_at_expire >
				    now.tv_sec)) {
					/* Audit expires this guy */
					twait_block->vtag_block[i].tv_sec_at_expire = 0;
					twait_block->vtag_block[i].v_tag = 0;
					if (set == 0) {
						/* Reuse it for my new tag */
						twait_block->vtag_block[0].tv_sec_at_expire = now.tv_sec + SCTP_TIME_WAIT;
						twait_block->vtag_block[0].v_tag = tag;
						set = 1;
					}
				}
			}
			if (set) {
				/*
				 * We only do up to the block where we can
				 * place our tag for audits
				 */
				break;
			}
		}
	}
	/* Need to add a new block to chain */
	if (!set) {
		MALLOC(twait_block, struct sctp_tagblock *,
		       sizeof(struct sctp_tagblock), M_PCB, M_NOWAIT);
		if (twait_block == NULL) {
			return;
		}
		memset(twait_block, 0, sizeof(struct sctp_timewait));
		LIST_INSERT_HEAD(chain, twait_block, sctp_nxt_tagblock);
		twait_block->vtag_block[0].tv_sec_at_expire = now.tv_sec +
		    SCTP_TIME_WAIT;
		twait_block->vtag_block[0].v_tag = tag;
	}
}


/*
 * Free the association after un-hashing the remote port.
 */
void
sctp_free_assoc(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	struct sctp_association *asoc;
	struct sctp_nets *net, *prev;
	struct sctp_laddr *laddr;
	struct sctp_tmit_chunk *chk;
	struct sctp_asconf_addr *aparam;
	struct sctp_socket_q_list *sq;
	int s;
	
	/* first, lets purge the entry from the hash table. */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	if (stcb->asoc.state == 0) {
		printf("Freeing already free association:%p - huh??\n",
		    stcb);
		splx(s);
		return;
	}
	/* Null all of my entry's on the socket q */
	TAILQ_FOREACH(sq, &inp->sctp_queue_list, next_sq) {
		if (sq->tcb == stcb) {
			sq->tcb = NULL;
		}
	}

	if (inp->sctp_tcb_at_block == (void *)stcb) {
		inp->error_on_block = ECONNRESET;
	}
	if (inp->sctp_tcbhash) {
		LIST_REMOVE(stcb, sctp_tcbhash);
	}
	/* pull it from the vtag hash */
	LIST_REMOVE(stcb, sctp_asocs);

	/* Now lets remove it from the list of ALL associations in the EP */
	LIST_REMOVE(stcb, sctp_tcblist);

	/*
	 * Now before we can free the assoc, we must  remove all of the
	 * networks and any other allocated space.. i.e. add removes here
	 * before the SCTP_ZONE_FREE() of the tasoc entry.
	 */
	asoc = &stcb->asoc;
	asoc->state = 0;

	sctp_add_vtag_to_timewait(inp, asoc->my_vtag);
	/* now clean up any other timers */
	callout_stop(&asoc->hb_timer.timer);
	callout_stop(&asoc->dack_timer.timer);
	callout_stop(&asoc->asconf_timer.timer);
	callout_stop(&asoc->shut_guard_timer.timer);
	callout_stop(&asoc->autoclose_timer.timer);
#ifdef SCTP_TCP_MODEL_SUPPORT
	callout_stop(&asoc->delayed_event_timer.timer);
#endif /* SCTP_TCP_MODEL_SUPPORT */
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		callout_stop(&net->rxt_timer.timer);
		callout_stop(&net->pmtu_timer.timer);
	}
	prev = NULL;
	while (!TAILQ_EMPTY(&asoc->nets)) {
		net = TAILQ_FIRST(&asoc->nets);
		/* pull from list */
		if ((sctppcbinfo.ipi_count_raddr == 0) || (prev == net)) {
			break;
		}
		prev = net;
		TAILQ_REMOVE(&asoc->nets, net, sctp_next);
		/* free it */
		net->ref_count = 0;
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_raddr, net);
		sctppcbinfo.ipi_count_raddr--;
	}
	/*
	 * The chunk lists and such SHOULD be empty but we check them
	 * just in case.
	 */
	/* anything on the wheel needs to be removed */
	while (!TAILQ_EMPTY(&asoc->out_wheel)) {
		struct sctp_stream_out *outs;
		outs = TAILQ_FIRST(&asoc->out_wheel);
		TAILQ_REMOVE(&asoc->out_wheel, outs, next_spoke);
		/* now clean up any chunks here */
		chk = TAILQ_FIRST(&outs->outqueue);
		while (chk) {
			TAILQ_REMOVE(&outs->outqueue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			chk->whoTo = NULL;
			chk->asoc = NULL;
			/* Free the chunk */
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			sctppcbinfo.ipi_count_chunk--;
			sctppcbinfo.ipi_gencnt_chunk++;
			if ((int)sctppcbinfo.ipi_count_chunk < 0) {
				panic("Chunk count is negative");
			}
			chk = TAILQ_FIRST(&outs->outqueue);
		}
		outs = TAILQ_FIRST(&asoc->out_wheel);
	}

	if (asoc->pending_reply) {
		FREE(asoc->pending_reply, M_PCB);
		asoc->pending_reply = NULL;
	}
	chk = TAILQ_FIRST(&asoc->pending_reply_queue);
	while (chk) {
		TAILQ_REMOVE(&asoc->pending_reply_queue, chk, sctp_next);
		if (chk->data) {
			sctp_m_freem(chk->data);
			chk->data = NULL;
		}
		chk->whoTo = NULL;
		chk->asoc = NULL;
		/* Free the chunk */
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
		sctppcbinfo.ipi_count_chunk--;
		sctppcbinfo.ipi_gencnt_chunk++;
		if ((int)sctppcbinfo.ipi_count_chunk < 0) {
			panic("Chunk count is negative");
		}
		chk = TAILQ_FIRST(&asoc->pending_reply_queue);
	}
	/* pending send queue SHOULD be empty */
	if (!TAILQ_EMPTY(&asoc->send_queue)) {
		chk = TAILQ_FIRST(&asoc->send_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->send_queue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			sctppcbinfo.ipi_count_chunk--;
			if ((int)sctppcbinfo.ipi_count_chunk < 0) {
				panic("Chunk count is negative");
			}
			sctppcbinfo.ipi_gencnt_chunk++;
			chk = TAILQ_FIRST(&asoc->send_queue);
		}
	}
	/* sent queue SHOULD be empty */
	if (!TAILQ_EMPTY(&asoc->sent_queue)) {
		chk = TAILQ_FIRST(&asoc->sent_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->sent_queue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			sctppcbinfo.ipi_count_chunk--;
			if ((int)sctppcbinfo.ipi_count_chunk < 0) {
				panic("Chunk count is negative");
			}
			sctppcbinfo.ipi_gencnt_chunk++;
			chk = TAILQ_FIRST(&asoc->sent_queue);
		}
	}
	/* control queue MAY not be empty */
	if (!TAILQ_EMPTY(&asoc->control_send_queue)) {
		chk = TAILQ_FIRST(&asoc->control_send_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->control_send_queue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			sctppcbinfo.ipi_count_chunk--;
			if ((int)sctppcbinfo.ipi_count_chunk < 0) {
				panic("Chunk count is negative");
			}
			sctppcbinfo.ipi_gencnt_chunk++;
			chk = TAILQ_FIRST(&asoc->control_send_queue);
		}
	}
	if (!TAILQ_EMPTY(&asoc->reasmqueue)) {
		chk = TAILQ_FIRST(&asoc->reasmqueue);
		while (chk) {
			TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			sctppcbinfo.ipi_count_chunk--;
			if ((int)sctppcbinfo.ipi_count_chunk < 0) {
				panic("Chunk count is negative");
			}
			sctppcbinfo.ipi_gencnt_chunk++;
			chk = TAILQ_FIRST(&asoc->reasmqueue);
		}
	}
	if (!TAILQ_EMPTY(&asoc->delivery_queue)) {
		chk = TAILQ_FIRST(&asoc->delivery_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->delivery_queue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			sctppcbinfo.ipi_count_chunk--;
			if ((int)sctppcbinfo.ipi_count_chunk < 0) {
				panic("Chunk count is negative");
			}
			sctppcbinfo.ipi_gencnt_chunk++;
			chk = TAILQ_FIRST(&asoc->delivery_queue);
		}
	}
	if (asoc->mapping_array) {
		FREE(asoc->mapping_array, M_PCB);
		asoc->mapping_array = NULL;
	}

	/* the stream outs */
	if (asoc->strmout) {
		FREE(asoc->strmout, M_PCB);
		asoc->strmout = NULL;
	}
	asoc->streamoutcnt = 0;
	if (asoc->strmin) {
		int i;
		for (i = 0; i < asoc->streamincnt; i++) {
			if (!TAILQ_EMPTY(&asoc->strmin[i].inqueue)) {
				/* We have somethings on the streamin queue */
				chk = TAILQ_FIRST(&asoc->strmin[i].inqueue);
				while (chk) {
					TAILQ_REMOVE(&asoc->strmin[i].inqueue,
					    chk, sctp_next);
					if (chk->data) {
						sctp_m_freem(chk->data);
						chk->data = NULL;
					}
					SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk,
					    chk);
					sctppcbinfo.ipi_count_chunk--;
					if ((int)sctppcbinfo.ipi_count_chunk < 0) {
						panic("Chunk count is negative");
					}
					sctppcbinfo.ipi_gencnt_chunk++;
					chk = TAILQ_FIRST(&asoc->strmin[i].inqueue);
				}
			}
		}
		FREE(asoc->strmin, M_PCB);
		asoc->strmin = NULL;
	}
	asoc->streamincnt = 0;
	/* local addresses, if any */
	while (!LIST_EMPTY(&asoc->sctp_local_addr_list)) {
		laddr = LIST_FIRST(&asoc->sctp_local_addr_list);
		LIST_REMOVE(laddr, sctp_nxt_addr);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_laddr, laddr);
		sctppcbinfo.ipi_count_laddr--;
	}
	/* pending asconf (address) parameters */
	while (!TAILQ_EMPTY(&asoc->asconf_queue)) {
		aparam = TAILQ_FIRST(&asoc->asconf_queue);
		TAILQ_REMOVE(&asoc->asconf_queue, aparam, next);
		FREE(aparam, M_PCB);
	}
	if (asoc->last_asconf_ack_sent != NULL) {
		sctp_m_freem(asoc->last_asconf_ack_sent);
		asoc->last_asconf_ack_sent = NULL;
	}
	/* Insert new items here :> */

	/* now clean up the tasoc itself */
	SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, stcb);
	sctppcbinfo.ipi_count_asoc--;
#ifdef SCTP_TCP_MODEL_SUPPORT
	if ((inp->sctp_socket->so_snd.sb_cc) ||
	    (inp->sctp_socket->so_snd.sb_mbcnt)) {
		/* This will happen when a abort is done */
		inp->sctp_socket->so_snd.sb_cc = 0;
		inp->sctp_socket->so_snd.sb_mbcnt = 0;
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) == 0) {
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				/*
				 * For the base fd, that is NOT in TCP pool we
				 * turn off the connected flag. This allows
				 * non-listening endpoints to connect/shutdown/
				 * connect.
				 */
				inp->sctp_flags &= ~SCTP_PCB_FLAGS_CONNECTED;
				soisdisconnected(inp->sctp_socket);
			}
			/*
			 * For those that are in the TCP pool we just leave
			 * so it cannot be used. When they close the fd we
			 * will free it all.
			 */
		}
	}
#endif /* SCTP_TCP_MODEL_SUPPORT */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
		sctp_inpcb_free(inp, 0);
	}
	splx(s);
}


/*
 * determine if a destination is "reachable" based upon the addresses
 * bound to the current endpoint (e.g. only v4 or v6 currently bound)
 */
/*
 * FIX: if we allow assoc-level bindx(), then this needs to be fixed
 * to use assoc level v4/v6 flags, as the assoc *may* not have the
 * same address types bound as its endpoint
 */
int
sctp_destination_is_reachable(struct sctp_tcb *stcb, struct sockaddr *destaddr)
{
	struct sctp_inpcb *inp;

	inp = stcb->sctp_ep;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL)
		/* if bound all, destination is not restricted */
		return (1);

	/* NOTE: all "scope" checks are done when local addresses are added */
	if (destaddr->sa_family == AF_INET6) {
#if !(defined(__FreeBSD__) || defined(__APPLE__))
		return (inp->inp_vflag & INP_IPV6);
#else
		return (inp->ip_inp.inp.inp_vflag & INP_IPV6);
#endif
	} else if (destaddr->sa_family == AF_INET) {
#if !(defined(__FreeBSD__) || defined(__APPLE__))
		return (inp->inp_vflag & INP_IPV4);
#else
		return (inp->ip_inp.inp.inp_vflag & INP_IPV4);
#endif
	} else {
		/* invalid family, so it's unreachable */
		return (0);
	}
}

/*
 * update the inp_vflags on an endpoint
 */
static void
sctp_update_ep_vflag(struct sctp_inpcb *inp) {
	struct sctp_laddr *laddr;

	/* first clear the flag */
#if !(defined(__FreeBSD__) || defined(__APPLE__))
	inp->inp_vflag = 0;
#else
	inp->ip_inp.inp.inp_vflag = 0;
#endif
	/* set the flag based on addresses on the ep list */
	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB1) {
				printf("An ounce of prevention is worth a pound of cure\n");
			}
#endif /* SCTP_DEBUG */
			continue;
		}
		if (laddr->ifa->ifa_addr) {
			continue;
		}
		if (laddr->ifa->ifa_addr->sa_family == AF_INET6) {
#if !(defined(__FreeBSD__) || defined(__APPLE__))
			inp->inp_vflag |= INP_IPV6;
#else
			inp->ip_inp.inp.inp_vflag |= INP_IPV6;
#endif
		} else if (laddr->ifa->ifa_addr->sa_family == AF_INET) {
#if !(defined(__FreeBSD__) || defined(__APPLE__))
			inp->inp_vflag |= INP_IPV4;
#else
			inp->ip_inp.inp.inp_vflag |= INP_IPV4;
#endif
		}
	}
}

/*
 * Add the address to the endpoint local address list
 * There is nothing to be done if we are bound to all addresses
 */
int
sctp_add_local_addr_ep(struct sctp_inpcb *inp, struct ifaddr *ifa)
{
	struct sctp_laddr *laddr;
	int fnd, error;
	fnd = 0;

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/* You are already bound to all. You have it already */
		return (0);
	}
	if (ifa->ifa_addr->sa_family == AF_INET6) {
		struct in6_ifaddr *ifa6;
		ifa6 = (struct in6_ifaddr *)ifa;
		if (ifa6->ia6_flags & (IN6_IFF_DETACHED |
		    IN6_IFF_DEPRECATED | IN6_IFF_ANYCAST | IN6_IFF_NOTREADY))
			/* Can't bind a non-existent addr. */
			return (-1);
	}
	/* first, is it already present? */
	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			fnd = 1;
			break;
		}
	}

	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) && (fnd == 0)) {
		/* Not bound to all */
		error = sctp_insert_laddr(&inp->sctp_addr_list, ifa);
		if (error != 0)
			return (error);
		inp->laddr_count++;
		/* update inp_vflag flags */
		if (ifa->ifa_addr->sa_family == AF_INET6) {
#if !(defined(__FreeBSD__) || defined(__APPLE__))
			inp->inp_vflag |= INP_IPV6;
#else
			inp->ip_inp.inp.inp_vflag |= INP_IPV6;
#endif
		} else if (ifa->ifa_addr->sa_family == AF_INET) {
#if !(defined(__FreeBSD__) || defined(__APPLE__))
			inp->inp_vflag |= INP_IPV4;
#else
			inp->ip_inp.inp.inp_vflag |= INP_IPV4;
#endif
		}
	}
	return (0);
}


/*
 * select a new (hopefully reachable) destination net
 * (should only be used when we deleted an ep addr that is the
 * only usable source address to reach the destination net)
 */
static void
sctp_select_primary_destination(struct sctp_tcb *stcb)
{
	struct sctp_nets *net;

	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		/* for now, we'll just pick the first reachable one we find */
		if (net->dest_state & SCTP_ADDR_UNCONFIRMED)
			continue;
		if (sctp_destination_is_reachable(stcb,
		    (struct sockaddr *)&net->ra._l_addr)) {
			/* found a reachable destination */
			stcb->asoc.primary_destination = net;
		}
	}
	/* I can't there from here! ...we're gonna die shortly... */
}


/*
 * Delete the address from the endpoint local address list
 * There is nothing to be done if we are bound to all addresses
 */
int
sctp_del_local_addr_ep(struct sctp_inpcb *inp, struct ifaddr *ifa)
{
	struct sctp_laddr *laddr;
	int fnd;
	fnd = 0;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/* You are already bound to all. You have it already */
		return (EINVAL);
	}

	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			fnd = 1;
			break;
		}
	}
	if (fnd && (inp->laddr_count < 2)) {
		/* can't delete unless there are at LEAST 2 addresses */
		return (-1);
	}
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) && (fnd)) {
		/*
		 * clean up any use of this address
		 * go through our associations and clear any
		 *  last_used_address that match this one
		 * for each assoc, see if a new primary_destination is needed
		 */
		struct sctp_tcb *stcb;

		/* clean up "next_addr_touse" */
		if (inp->next_addr_touse == laddr)
			/* delete this address */
			inp->next_addr_touse = NULL;

		/* clean up "last_used_address" */
		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			if (stcb->asoc.last_used_address == laddr)
				/* delete this address */
				stcb->asoc.last_used_address = NULL;
		} /* for each tcb */

		/* remove it from the ep list */
		sctp_remove_laddr(laddr);
		inp->laddr_count--;
		/* update inp_vflag flags */
		sctp_update_ep_vflag(inp);
		/* select a new primary destination if needed */
		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			if (sctp_destination_is_reachable(stcb,
			    (struct sockaddr *)&stcb->asoc.primary_destination->ra._l_addr) == 0) {
				sctp_select_primary_destination(stcb);
			}
		} /* for each tcb */
	}
	return (0);
}

/*
 * Add the addr to the TCB local address list
 * For the BOUNDALL or dynamic case, this is a "pending" address list
 * (eg. addresses waiting for an ASCONF-ACK response)
 * For the subset binding, static case, this is a "valid" address list
 */
int
sctp_add_local_addr_assoc(struct sctp_tcb *stcb, struct ifaddr *ifa)
{
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;
	int error;

	inp = stcb->sctp_ep;
	if (ifa->ifa_addr->sa_family == AF_INET6) {
		struct in6_ifaddr *ifa6;
		ifa6 = (struct in6_ifaddr *)ifa;
		if (ifa6->ia6_flags & (IN6_IFF_DETACHED |
		    /* IN6_IFF_DEPRECATED | */
		    IN6_IFF_ANYCAST |
		    IN6_IFF_NOTREADY))
			/* Can't bind a non-existent addr. */
			return (-1);
	}
	/* does the address already exist? */
	LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			return (-1);
		}
	}

	/* add to the list */
	error = sctp_insert_laddr(&stcb->asoc.sctp_local_addr_list, ifa);
	if (error != 0) 
		return (error);
	return (0);
}

/*
 * insert an laddr entry with the given ifa for the desired list
 */
int
sctp_insert_laddr(struct sctpladdr *list, struct ifaddr *ifa) {
	struct sctp_laddr *laddr;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif

	laddr = (struct sctp_laddr *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_laddr);
	if (laddr == NULL) {
		/* out of memory? */
		splx(s);
		return (EINVAL);
	}
	sctppcbinfo.ipi_count_laddr++;
	sctppcbinfo.ipi_gencnt_laddr++;
	bzero(laddr, sizeof(*laddr));
	laddr->ifa = ifa;
	/* insert it */
	LIST_INSERT_HEAD(list, laddr, sctp_nxt_addr);

	splx(s);
	return (0);
}

/*
 * Remove an laddr entry from the local address list (on an assoc)
 */
void
sctp_remove_laddr(struct sctp_laddr *laddr)
{
	int s;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	/* remove from the list */
	LIST_REMOVE(laddr, sctp_nxt_addr);
	SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_laddr, laddr);
	sctppcbinfo.ipi_count_laddr--;
	sctppcbinfo.ipi_gencnt_laddr++;

	splx(s);
}

/*
 * Remove an address from the TCB local address list
 */
int
sctp_del_local_addr_assoc(struct sctp_tcb *stcb, struct ifaddr *ifa)
{
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;

	inp = stcb->sctp_ep;
	/* if subset bound and don't allow ASCONF's, can't delete last */
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) &&
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_DO_ASCONF) == 0)) {
		if (stcb->asoc.numnets < 2) {
			/* can't delete last address */
			return (-1);
		}
	}

	LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list, sctp_nxt_addr) {
		/* remove the address if it exists */
		if (laddr->ifa == NULL)
			continue;
		if (laddr->ifa == ifa) {
			sctp_remove_laddr(laddr);
			return (0);
		}
	}

	/* address not found! */
	return (-1);
}

/*
 * Remove an address from the TCB local address list
 * lookup using a sockaddr addr
 */
int
sctp_del_local_addr_assoc_sa(struct sctp_tcb *stcb, struct sockaddr *sa)
{
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;
	struct sockaddr *l_sa;

	inp = stcb->sctp_ep;
	/* if subset bound and don't allow ASCONF's, can't delete last */
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) &&
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_DO_ASCONF) == 0)) {
		if (stcb->asoc.numnets < 2) {
			/* can't delete last address */
			return (-1);
		}
	}

	LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list, sctp_nxt_addr) {
		/* make sure the address exists */
		if (laddr->ifa == NULL)
			continue;
		if (laddr->ifa->ifa_addr == NULL)
			continue;

		l_sa = laddr->ifa->ifa_addr;
		if (l_sa->sa_family == AF_INET6) {
			/* IPv6 address */
			struct sockaddr_in6 *sin1, *sin2;
			sin1 = (struct sockaddr_in6 *)l_sa;
			sin2 = (struct sockaddr_in6 *)sa;
			if (memcmp(&sin1->sin6_addr, &sin2->sin6_addr,
			    sizeof(struct in6_addr)) == 0) {
				/* matched */
				sctp_remove_laddr(laddr);
				return (0);
			}
		} else if (l_sa->sa_family == AF_INET) {
			/* IPv4 address */
			struct sockaddr_in *sin1, *sin2;
			sin1 = (struct sockaddr_in *)l_sa;
			sin2 = (struct sockaddr_in *)sa;
			if (sin1->sin_addr.s_addr == sin2->sin_addr.s_addr) {
				/* matched */
				sctp_remove_laddr(laddr);
				return (0);
			}
		} else {
			/* invalid family */
			return (-1);
		}
	} /* end foreach */
	/* address not found! */
	return (-1);
}

static char sctp_pcb_initialized = 0;

#if defined(__FreeBSD__) || defined(__APPLE__)
/* sysctl */
static int sctp_max_number_of_assoc = SCTP_MAX_NUM_OF_ASOC;
static int sctp_scale_up_for_address = SCTP_SCALE_FOR_ADDR;

#endif /* FreeBSD || APPLE */

/* sysctl to enable/disable SCTP_PCB_FLAGS_AUTO_ASCONF for new EP's */
int sctp_auto_asconf = SCTP_DEFAULT_AUTO_ASCONF;
#if defined(__FreeBSD__)
SYSCTL_INT(_net_inet_sctp, SCTPCTL_AUTOASCONF, sctp_auto_asconf,
	   CTLFLAG_RW, &sctp_auto_asconf, 0,
	   "auto ASCONF flag enable(1)/disable(0)");
#elif defined(__APPLE__)
SYSCTL_INT(_net_inet_sctp, OID_AUTO, sctp_auto_asconf, CTLFLAG_RW,
	   &sctp_auto_asconf, 0, "auto ASCONF flag enable(1)/disable(0)");
#endif

#ifndef SCTP_TCBHASHSIZE
#define SCTP_TCBHASHSIZE 1024
#endif

#ifndef SCTP_CHUNKQUEUE_SCALE
#define SCTP_CHUNKQUEUE_SCALE 10
#endif

void
sctp_pcb_init()
{
	/*
	 * SCTP initialization for the PCB structures
	 * should be called by the sctp_init() funciton.
	 */
	int i;
	int hashtblsize = SCTP_TCBHASHSIZE;

#if defined(__FreeBSD__) || defined(__APPLE__)
	int sctp_chunkscale = SCTP_CHUNKQUEUE_SCALE;
#endif

	if (sctp_pcb_initialized != 0) {
		/* error I was called twice */
		return;
	}
	sctp_pcb_initialized = 1;

	/* Init all peg counts */
	for (i = 0; i < SCTP_NUMBER_OF_PEGS; i++) {
		sctp_pegs[i] = 0;
	}

	/* init the empty list of (All) Endpoints */
	LIST_INIT(&sctppcbinfo.listhead);

	/* init the hash table of endpoints */
#if defined(__FreeBSD__)
#if defined(__FreeBSD_cc_version) && __FreeBSD_cc_version >= 440000
	TUNABLE_INT_FETCH("net.inet.sctp.tcbhashsize", &hashtblsize);
	TUNABLE_INT_FETCH("net.inet.sctp.pcbhashsize", &sctp_pcbtblsize);
	TUNABLE_INT_FETCH("net.inet.sctp.chunkscale", &sctp_chunkscale);
#else
	TUNABLE_INT_FETCH("net.inet.sctp.tcbhashsize", SCTP_TCBHASHSIZE,
	    hashtblsize);
	TUNABLE_INT_FETCH("net.inet.sctp.pcbhashsize", SCTP_PCBHASHSIZE,
	    sctp_pcbtblsize);
	TUNABLE_INT_FETCH("net.inet.sctp.chunkscale", SCTP_CHUNKQUEUE_SCALE,
	    sctp_chunkscale);
#endif
#endif

	sctppcbinfo.sctp_asochash = hashinit((hashtblsize * 31),
#ifdef __NetBSD__
	    HASH_LIST,
#endif
	    M_PCB,
#if defined(__NetBSD__) || defined(__OpenBSD__)
	    M_WAITOK,
#endif
	    &sctppcbinfo.hashasocmark);

	sctppcbinfo.sctp_ephash = hashinit(hashtblsize,
#ifdef __NetBSD__
	    HASH_LIST,
#endif
	    M_PCB,
#if defined(__NetBSD__) || defined(__OpenBSD__)
	    M_WAITOK,
#endif
	    &sctppcbinfo.hashmark);

#ifdef SCTP_TCP_MODEL_SUPPORT
	sctppcbinfo.sctp_tcpephash = hashinit(hashtblsize,
#ifdef __NetBSD__
	    HASH_LIST,
#endif
	    M_PCB,
#if defined(__NetBSD__) || defined(__OpenBSD__)
	    M_WAITOK,
#endif
	    &sctppcbinfo.hashtcpmark);
#endif /* SCTP_TCP_MODEL_SUPPORT */

	sctppcbinfo.hashtblsize = hashtblsize;

	/* init the zones */
	/*
	 * FIX ME: Should check for NULL returns, but if it does fail we
	 * are doomed to panic anyways... add later maybe.
	 */
	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_ep, "sctp_ep",
	    sizeof(struct sctp_inpcb), maxsockets);

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_asoc, "sctp_asoc",
	    sizeof(struct sctp_tcb), sctp_max_number_of_assoc);

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_laddr, "sctp_laddr",
	    sizeof(struct sctp_laddr),
	    (sctp_max_number_of_assoc * sctp_scale_up_for_address));

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_raddr, "sctp_raddr",
	    sizeof(struct sctp_nets),
	    (sctp_max_number_of_assoc * sctp_scale_up_for_address));

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_chunk, "sctp_chunk",
	    sizeof(struct sctp_tmit_chunk),
	    (sctp_max_number_of_assoc * sctp_scale_up_for_address *
	    sctp_chunkscale));

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_sockq, "sctp_sockq",
	    sizeof(struct sctp_socket_q_list),
	    (sctp_max_number_of_assoc * sctp_scale_up_for_address *
	    sctp_chunkscale));

	/* not sure if we need all the counts */
	sctppcbinfo.ipi_count_ep = 0;
	sctppcbinfo.ipi_gencnt_ep = 0;
	/* assoc/tcb zone info */
	sctppcbinfo.ipi_count_asoc = 0;
	sctppcbinfo.ipi_gencnt_asoc = 0;
	/* local addrlist zone info */
	sctppcbinfo.ipi_count_laddr = 0;
	sctppcbinfo.ipi_gencnt_laddr = 0;
	/* remote addrlist zone info */
	sctppcbinfo.ipi_count_raddr = 0;
	sctppcbinfo.ipi_gencnt_raddr = 0;
	/* chunk info */
	sctppcbinfo.ipi_count_chunk = 0;
	sctppcbinfo.ipi_gencnt_chunk = 0;

	/* socket queue zone info */
	sctppcbinfo.ipi_count_sockq = 0;
	sctppcbinfo.ipi_gencnt_sockq = 0;

	/* mbuf tracker */
	sctppcbinfo.mbuf_track = 0;
	/* port stuff */
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
	sctppcbinfo.lastlow = ipport_firstauto;
#else
	sctppcbinfo.lastlow = anonportmin;
#endif
#ifdef SCTP_VTAG_TIMEWAIT_PER_STACK
	/* Init the TIMEWAIT list */
	for (i = 0; i < SCTP_STACK_VTAG_HASH_SIZE; i++) {
		LIST_INIT(&sctppcbinfo.vtag_timewait[i]);
	}
#endif

#if defined(_SCTP_NEEDS_CALLOUT_) && !defined(__APPLE__)
	TAILQ_INIT(&sctppcbinfo.callqueue);
#endif

}

int
sctp_load_addresses_from_init(struct sctp_tcb *stcb, struct mbuf *m,
    int iphlen, int offset, int limit, struct sctphdr *sh,
    struct sockaddr *altsa)
{
	/*
	 * grub through the INIT pulling addresses and
	 * loading them to the nets structure in the asoc.
	 * The from address in the mbuf should also be loaded
	 * (if it is not already). This routine can be called
	 * with either INIT or INIT-ACK's as long as the
	 * m points to the IP packet and the offset points
	 * to the beginning of the parameters.
	 */
	struct sctp_inpcb *inp;
	struct sctp_nets *net, *net_tmp;
	struct ip *iph;
	struct sctp_paramhdr *phdr, parm_buf;
	struct sctp_tcb *stcb_tmp;
	u_int16_t ptype, plen;
	struct sockaddr *sa;
	struct sockaddr_storage dest_store;
	struct sockaddr *local_sa = (struct sockaddr *)&dest_store;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;

	/* First get the destination address setup too. */
	memset(&sin, 0, sizeof(sin));
	memset(&sin6, 0, sizeof(sin6));

	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = stcb->rport;

	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_port = stcb->rport;
	if (altsa == NULL) {
		iph = mtod(m, struct ip *);
		if (iph->ip_v == IPVERSION) {
			/* its IPv4 */
			struct sockaddr_in *sin_2;
			sin_2 = (struct sockaddr_in *)(local_sa);
			memset(sin_2, 0, sizeof(sin));
			sin_2->sin_family = AF_INET;
			sin_2->sin_len = sizeof(sin);
			sin_2->sin_port = sh->dest_port;
			sin_2->sin_addr.s_addr = iph->ip_dst.s_addr ;
			sin.sin_addr = iph->ip_src;
			sa = (struct sockaddr *)&sin;
		} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
			/* its IPv6 */
			struct ip6_hdr *ip6;
			struct sockaddr_in6 *sin6_2;

			ip6 = mtod(m, struct ip6_hdr *);
			sin6_2 = (struct sockaddr_in6 *)(local_sa);
			memset(sin6_2, 0, sizeof(sin6));
			sin6_2->sin6_family = AF_INET6;
			sin6_2->sin6_len = sizeof(struct sockaddr_in6);
			sin6_2->sin6_port = sh->dest_port;
			sin6.sin6_addr = ip6->ip6_src;
			sa = (struct sockaddr *)&sin6;
		} else {
			sa = NULL;
		}
	} else {
		/*
		 * For cookies we use the src address NOT from the packet
		 * but from the original INIT
		 */
		sa = altsa;
	}
	/* Turn off ECN until we get through all params */
	stcb->asoc.ecn_allowed = 0;

	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		/* mark all addresses that we have currently on the list */
		net->dest_state |= SCTP_ADDR_NOT_IN_ASSOC;
	}
	/* does the source address already exist? if so skip it */
	inp = stcb->sctp_ep;
	stcb_tmp = sctp_findassociation_ep_addr(&inp, sa, &net_tmp, local_sa);
	if ((stcb_tmp == NULL && inp == stcb->sctp_ep) || inp == NULL) {
		/* we must add the source address */
		/* no scope set here since we have a tcb already. */
		if ((sa->sa_family == AF_INET) &&
		    (stcb->asoc.ipv4_addr_legal)) {
			if (sctp_add_remote_addr(stcb, sa, 0, 2)) {
				return (-1);
			}	
		} else if ((sa->sa_family == AF_INET6) &&
		    (stcb->asoc.ipv6_addr_legal)) {
			if (sctp_add_remote_addr(stcb, sa, 0, 3)) {
				return (-1);
			}
		}
	} else {
		if (net_tmp != NULL && stcb_tmp == stcb) {
			net_tmp->dest_state &= ~SCTP_ADDR_NOT_IN_ASSOC;
		} else if (stcb_tmp != stcb) {
			/* It belongs to another association? */
			return (-1);
		}
	}
	/* now we must go through each of the params. */
	phdr = sctp_get_next_param(m, offset, &parm_buf, sizeof(parm_buf));
	while (phdr) {
		ptype = ntohs(phdr->param_type);
		plen = ntohs(phdr->param_length);
		/*printf("ptype => %d, plen => %d\n", ptype, plen);*/
		if (offset + plen > limit) {
			break;
		}
		if (plen == 0) {
			break;
		}
		if ((ptype == SCTP_IPV4_ADDRESS) &&
		    (stcb->asoc.ipv4_addr_legal)) {
			struct sctp_ipv4addr_param *p4, p4_buf;
			/* ok get the v4 address and check/add */
			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&p4_buf, sizeof(p4_buf));
			if (plen != sizeof(struct sctp_ipv4addr_param) ||
			    phdr == NULL) {
				return (-1);
			}
			p4 = (struct sctp_ipv4addr_param *)phdr;
			sin.sin_addr.s_addr = p4->addr;
			sa = (struct sockaddr *)&sin;
			inp = stcb->sctp_ep;
			stcb_tmp = sctp_findassociation_ep_addr(&inp, sa, &net,
			    local_sa);

			if ((stcb_tmp== NULL && inp == stcb->sctp_ep) ||
			    inp == NULL) {
				/* we must add the source address */
				/* no scope set since we have a tcb already */
				if (sctp_add_remote_addr(stcb, sa, 0, 4)) {
					return (-1);
				}
			} else if (stcb_tmp == stcb) {
				if (net != NULL) {
					/* clear flag */
					net->dest_state &=
					    ~SCTP_ADDR_NOT_IN_ASSOC;
				}
			} else {
				/* strange, address is in another assoc? */
				return (-1);
			}
		} else if ((ptype == SCTP_IPV6_ADDRESS) &&
		    (stcb->asoc.ipv6_addr_legal)) {
			/* ok get the v6 address and check/add */
			struct sctp_ipv6addr_param *p6, p6_buf;
			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&p6_buf, sizeof(p6_buf));
			if (plen != sizeof(struct sctp_ipv6addr_param) ||
			    phdr == NULL) {
				return (-1);
			}
			p6 = (struct sctp_ipv6addr_param *)phdr;
			memcpy((caddr_t)&sin6.sin6_addr, p6->addr,
			    sizeof(p6->addr));
			sa = (struct sockaddr *)&sin6;
			inp = stcb->sctp_ep;
			stcb_tmp= sctp_findassociation_ep_addr(&inp, sa, &net,
			    local_sa);
			if (stcb_tmp == NULL && (inp == stcb->sctp_ep ||
			    inp == NULL)) {
				/* we must add the address, no scope set */
				if (sctp_add_remote_addr(stcb, sa, 0, 5)) {
					return (-1);
				}
			} else if (stcb_tmp == stcb) {
				if (net != NULL) {
					/* clear flag */
					net->dest_state &=
					    ~SCTP_ADDR_NOT_IN_ASSOC;
				}
			} else {
				/* strange, address is in another assoc? */
				return (-1);
			}
		} else if (ptype == SCTP_ECN_CAPABLE) {
			stcb->asoc.ecn_allowed = 1;
		} else if (ptype == SCTP_ULP_ADAPTION) {
			if (stcb->asoc.state != SCTP_STATE_OPEN) {
				struct sctp_adaption_layer_indication ai, *aip;

				phdr = sctp_get_next_param(m, offset,
							   (struct sctp_paramhdr *)&ai, sizeof(ai));
				aip = (struct sctp_adaption_layer_indication *)phdr;
				sctp_ulp_notify(SCTP_NOTIFY_ADAPTION_INDICATION,
						stcb, ntohl(aip->indication), NULL);
			}
		} else if (ptype == SCTP_SET_PRIM_ADDR) {
			struct sctp_asconf_addr_param lstore, *fee;
			struct sctp_asconf_addrv4_param *fii;
			int lptype;
			struct sockaddr *lsa = NULL;

			stcb->asoc.peer_supports_asconf = 1;
			stcb->asoc.peer_supports_asconf_setprim = 1;
			if (plen > sizeof(lstore)) {
				return (-1);
			}
			phdr = sctp_get_next_param(m, offset,
    			    (struct sctp_paramhdr *)&lstore, plen);
			if (phdr == NULL) {
				return (-1);
			}

			fee  = (struct sctp_asconf_addr_param *)phdr;
			lptype = ntohs(fee->addrp.ph.param_type);
			if (lptype == SCTP_IPV4_ADDRESS) {
				if (plen !=
				    sizeof(struct sctp_asconf_addrv4_param)) {
					printf("Sizeof setprim in init/init ack not %d but %d - ignored\n",
				           (int)sizeof(struct sctp_asconf_addrv4_param),
				           plen);
				} else {
					fii = (struct sctp_asconf_addrv4_param *)fee;
					sin.sin_addr.s_addr = fii->addrp.addr;
					lsa = (struct sockaddr *)&sin;
				}
			} else if (lptype == SCTP_IPV6_ADDRESS) {
				if (plen !=
				    sizeof(struct sctp_asconf_addr_param)) {
					printf("Sizeof setprim (v6) in init/init ack not %d but %d - ignored\n",
				           (int)sizeof(struct sctp_asconf_addr_param),
				           plen);
				} else {
					memcpy(sin6.sin6_addr.s6_addr,
					    fee->addrp.addr,
					    sizeof(fee->addrp.addr));
					lsa = (struct sockaddr *)&sin6;
				}
			}
			if (lsa) {
				sctp_set_primary_addr(stcb, sa, NULL);
			}

		} else if (ptype == SCTP_PRSCTP_SUPPORTED) {
			/* Peer supports pr-sctp */
			stcb->asoc.peer_supports_prsctp = 1;
		} else if (ptype == SCTP_SUPPORTED_CHUNK_EXT) {
			/* A supported extension chunk */
			struct sctp_supported_chunk_types_param *pr_supported;
			uint8_t local_store[128];
			int num_ent,i;

			phdr = sctp_get_next_param(m, offset,
    			    (struct sctp_paramhdr *)&local_store, plen);
			if (phdr == NULL) {
				return (-1);
			}
			stcb->asoc.peer_supports_asconf = 0;
			stcb->asoc.peer_supports_asconf_setprim = 0;
			stcb->asoc.peer_supports_prsctp = 0;
			stcb->asoc.peer_supports_pktdrop = 0;
			stcb->asoc.peer_supports_strreset = 0;
			pr_supported = (struct sctp_supported_chunk_types_param *)phdr;
			num_ent = plen - sizeof(struct sctp_paramhdr);
			for (i=0; i<num_ent; i++) {
				switch (pr_supported->chunk_types[i]) {
				case SCTP_ASCONF:
					stcb->asoc.peer_supports_asconf = 1;
					stcb->asoc.peer_supports_asconf_setprim = 1;
					break;
				case SCTP_ASCONF_ACK:
					stcb->asoc.peer_supports_asconf = 1;
					stcb->asoc.peer_supports_asconf_setprim = 1;
					break;
				case SCTP_FORWARD_CUM_TSN:
					stcb->asoc.peer_supports_prsctp = 1;
					break;
				case SCTP_PACKET_DROPPED:
					stcb->asoc.peer_supports_pktdrop = 1;
					break;
				case SCTP_STREAM_RESET:
					stcb->asoc.peer_supports_strreset = 1;
					break;
				default:
					/* one I have not learned yet */
					break;

				}
			}
		} else if (ptype == SCTP_ECN_NONCE_SUPPORTED) {
			/* Peer supports ECN-nonce */
			stcb->asoc.peer_supports_ecn_nonce = 1;
			stcb->asoc.ecn_nonce_allowed = 1;
		} else if ((ptype == SCTP_HEARTBEAT_INFO) || 
			   (ptype == SCTP_STATE_COOKIE) ||
			   (ptype == SCTP_UNRECOG_PARAM) ||
			   (ptype == SCTP_COOKIE_PRESERVE) ||
			   (ptype == SCTP_SUPPORTED_ADDRTYPE) ||
			   (ptype == SCTP_ADD_IP_ADDRESS) ||
			   (ptype == SCTP_DEL_IP_ADDRESS) ||
			   (ptype == SCTP_ERROR_CAUSE_IND) ||
			   (ptype == SCTP_SUCCESS_REPORT)) {
			/* don't care */;
		} else {
			if ((ptype & 0x8000) == 0x0000) {
				/* must stop processing the rest of
				 * the param's. Any report bits were
				 * handled with the call to sctp_arethere_unrecognized_parameters()
				 * when the INIT or INIT-ACK was first seen.
				 */
				break;
			}
		}
		offset += SCTP_SIZE32(plen);
		if (offset >= limit) {
			break;
		}
		phdr = sctp_get_next_param(m, offset, &parm_buf,
		    sizeof(parm_buf));
	}
	/* Now check to see if we need to purge any addresses */
	for (net = TAILQ_FIRST(&stcb->asoc.nets); net != NULL; net = net_tmp) {
		net_tmp = TAILQ_NEXT(net, sctp_next);
		if ((net->dest_state & SCTP_ADDR_NOT_IN_ASSOC) ==
		    SCTP_ADDR_NOT_IN_ASSOC) {
			/* This address has been removed from the asoc */
			/* remove and free it */
			stcb->asoc.numnets--;
			TAILQ_REMOVE(&stcb->asoc.nets, net, sctp_next);
			sctp_free_remote_addr(net);
			if (net == stcb->asoc.primary_destination) {
				stcb->asoc.primary_destination = NULL;
				sctp_select_primary_destination(stcb);
			}
		}
	}
	return (0);
}

int
sctp_set_primary_addr(struct sctp_tcb *stcb, struct sockaddr *sa,
    struct sctp_nets *net)
{
	/* make sure the requested primary address exists in the assoc */
	if (net == NULL && sa)
		net = sctp_findnet(stcb, sa);

	if (net == NULL) {
		/* didn't find the requested primary address! */
		return (-1);
	} else {
		/* set the primary address */
		if (net->dest_state & SCTP_ADDR_UNCONFIRMED) {
			/* Must be confirmed */
			return (-1);
		}
		stcb->asoc.primary_destination = net;
		net->dest_state &= ~SCTP_ADDR_WAS_PRIMARY;
		return (0);
	}
}


int
sctp_is_vtag_good(struct sctp_inpcb *inp, u_int32_t tag, struct timeval *now)
{
	/*
	 * This function serves two purposes. It will see if a TAG can be
	 * re-used and return 1 for yes it is ok and 0 for don't use that
	 * tag.
	 * A secondary function it will do is purge out old tags that can
	 * be removed.
	 */
	struct sctpasochead *head;
	struct sctpvtaghead *chain;
	struct sctp_tagblock *twait_block;
	struct sctp_tcb *stcb;

	int i;
#ifdef SCTP_VTAG_TIMEWAIT_PER_STACK
	chain = &sctppcbinfo.vtag_timewait[(tag % SCTP_STACK_VTAG_HASH_SIZE)];
#else
	chain = &inp->vtag_timewait[(tag % SCTP_STACK_VTAG_HASH_SIZE)];
#endif
	/* First is the vtag in use ? */

	head = &sctppcbinfo.sctp_asochash[SCTP_PCBHASH_ASOC(tag,
	    sctppcbinfo.hashasocmark)];
	if (head == NULL) {
		goto good_so_far;
	}
	LIST_FOREACH(stcb, head, sctp_asocs) {
		if (stcb->asoc.my_vtag == tag) {
			/* bad tag, in use */
			return(0);
		}
	}
 good_so_far:
	if (!LIST_EMPTY(chain)) {
		/*
		 * Block(s) are present, lets see if we have this tag in
		 * the list
		 */
		LIST_FOREACH(twait_block, chain, sctp_nxt_tagblock) {
			for (i = 0; i < SCTP_NUMBER_IN_VTAG_BLOCK; i++) {
				if (twait_block->vtag_block[i].v_tag == 0) {
					/* not used */
					continue;
				} else if (twait_block->vtag_block[i].tv_sec_at_expire >
				    now->tv_sec) {
					/* Audit expires this guy */
					twait_block->vtag_block[i].tv_sec_at_expire = 0;
					twait_block->vtag_block[i].v_tag = 0;
				} else if (twait_block->vtag_block[i].v_tag ==
				    tag) {
					/* Bad tag, sorry :< */
					return (0);
				}
			}
		}
	}
	/* Not found, ok to use the tag */
	return (1);
}


/*
 * Delete the address from the endpoint local address list
 * Lookup using a sockaddr address (ie. not an ifaddr)
 */
int
sctp_del_local_addr_ep_sa(struct sctp_inpcb *inp, struct sockaddr *sa)
{
	struct sctp_laddr *laddr;
	struct sockaddr *l_sa;
	int found = 0;

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/* You are already bound to all. You have it already */
		return (EINVAL);
	}

	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		/* make sure the address exists */
		if (laddr->ifa == NULL)
			continue;
		if (laddr->ifa->ifa_addr == NULL)
			continue;

		l_sa = laddr->ifa->ifa_addr;
		if (l_sa->sa_family == AF_INET6) {
			/* IPv6 address */
			struct sockaddr_in6 *sin1, *sin2;
			sin1 = (struct sockaddr_in6 *)l_sa;
			sin2 = (struct sockaddr_in6 *)sa;
			if (memcmp(&sin1->sin6_addr, &sin2->sin6_addr,
			    sizeof(struct in6_addr)) == 0) {
				/* matched */
				found = 1;
				break;
			}
		} else if (l_sa->sa_family == AF_INET) {
			/* IPv4 address */
			struct sockaddr_in *sin1, *sin2;
			sin1 = (struct sockaddr_in *)l_sa;
			sin2 = (struct sockaddr_in *)sa;
			if (sin1->sin_addr.s_addr == sin2->sin_addr.s_addr) {
				/* matched */
				found = 1;
				break;
			}
		} else {
			/* invalid family */
			return (-1);
		}
	}

	if (found && inp->laddr_count < 2) {
		/* can't delete unless there are at LEAST 2 addresses */
		return (-1);
	}

	if (found && (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {
		/*
		 * remove it from the ep list, this should NOT be
		 * done until its really gone from the interface list and
		 * we won't be receiving more of these. Probably right
		 * away. If we do allow a removal of an address from
		 * an association (sub-set bind) than this should NOT
		 * be called until the all ASCONF come back from this
		 * association.
		 */
		sctp_remove_laddr(laddr);
		return (0);
	} else {
		return (-1);
	}
}

static void
sctp_drain_mbufs(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	/*
	 * We must hunt this association for MBUF's past the cumack
	 * (i.e. out of order data that we can renege on).
	 */
	struct sctp_association *asoc;
	struct sctp_tmit_chunk *chk, *nchk;
	u_int32_t cumulative_tsn_p1, tsn;
	int cnt, strmat, gap;
	/* We look for anything larger than the cum-ack + 1 */

	asoc = &stcb->asoc;
	cumulative_tsn_p1 = asoc->cumulative_tsn + 1;
	cnt = 0;
	/* First look in the re-assembly queue */
	chk = TAILQ_FIRST(&asoc->reasmqueue);
	while (chk) {
		/* Get the next one */
		nchk = TAILQ_NEXT(chk, sctp_next);
		if (compare_with_wrap(chk->rec.data.TSN_seq,
		    cumulative_tsn_p1, MAX_TSN)) {
			/* Yep it is above cum-ack */
			cnt++;
			tsn = chk->rec.data.TSN_seq;
			if (tsn >= asoc->mapping_array_base_tsn) {
				gap  = tsn - asoc->mapping_array_base_tsn;
			} else {
				gap = (MAX_TSN - asoc->mapping_array_base_tsn) +
				    tsn + 1;
			}
			asoc->size_on_reasm_queue -= chk->send_size;
			asoc->cnt_on_reasm_queue--;
			SCTP_UNSET_TSN_PRESENT(asoc->mapping_array, gap);
			TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			sctppcbinfo.ipi_count_chunk--;
			if ((int)sctppcbinfo.ipi_count_chunk < 0) {
				panic("Chunk count is negative");
			}
			sctppcbinfo.ipi_gencnt_chunk++;
		}
		chk = nchk;
	}
	/* Ok that was fun, now we will drain all the inbound streams? */
	for (strmat = 0; strmat < asoc->streamincnt; strmat++) {
		chk = TAILQ_FIRST(&asoc->strmin[strmat].inqueue);
		while (chk) {
			nchk = TAILQ_NEXT(chk, sctp_next);
			if (compare_with_wrap(chk->rec.data.TSN_seq,
			    cumulative_tsn_p1, MAX_TSN)) {
				/* Yep it is above cum-ack */
				cnt++;
				tsn = chk->rec.data.TSN_seq;
				if (tsn >= asoc->mapping_array_base_tsn) {
					gap = tsn -
					    asoc->mapping_array_base_tsn;
				} else {
					gap = (MAX_TSN -
					    asoc->mapping_array_base_tsn) +
					    tsn + 1;
				}
				asoc->size_on_all_streams -= chk->send_size;
				asoc->cnt_on_all_streams--;

				SCTP_UNSET_TSN_PRESENT(asoc->mapping_array,
				    gap);
				TAILQ_REMOVE(&asoc->strmin[strmat].inqueue,
				    chk, sctp_next);
				if (chk->data) {
					sctp_m_freem(chk->data);
					chk->data = NULL;
				}
				SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
				sctppcbinfo.ipi_count_chunk--;
				if ((int)sctppcbinfo.ipi_count_chunk < 0) {
					panic("Chunk count is negative");
				}
				sctppcbinfo.ipi_gencnt_chunk++;
			}
			chk = nchk;
		}
	}
	printf("Harvest %d chunks from drain ep:%p - %d left\n",
	       cnt, inp, sctppcbinfo.ipi_count_chunk);
	/*
	 * Question, should we go through the delivery queue?
	 * The only reason things are on here is the app not reading OR a
	 * p-d-api up. An attacker COULD send enough in to initiate the
	 * PD-API and then send a bunch of stuff to other streams... these
	 * would wind up on the delivery queue.. and then we would not get
	 * to them. But in order to do this I then have to back-track and
	 * un-deliver sequence numbers in streams.. el-yucko. I think for
	 * now we will NOT look at the delivery queue and leave it to be
	 * something to consider later. An alternative would be to abort
	 * the P-D-API with a notification and then deliver the data....
	 * Or another method might be to keep track of how many times the
	 * situation occurs and if we see a possible attack underway just
	 * abort the association.
	 */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		if (cnt) {
			printf("Freed %d chunks from reneg harvest\n", cnt);
		}
	}
#endif /* SCTP_DEBUG */

	/*
	 * Another issue, in un-setting the TSN's in the mapping array we
	 * DID NOT adjust the higest_tsn marker.  This will cause one of
	 * two things to occur. It may cause us to do extra work in checking
	 * for our mapping array movement. More importantly it may cause us
	 * to SACK every datagram. This may not be a bad thing though since
	 * we will recover once we get our cum-ack above and all this stuff
	 * we dumped recovered.
	 */
}

void
sctp_drain()
{
	/*
	 * We must walk the PCB lists for ALL associations here. The system
	 * is LOW on MBUF's and needs help. This is where reneging will
	 * occur. We really hope this does NOT happen!
	 */
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;

	printf("SCTP DRAIN called %d chunks out there\n",
	    sctppcbinfo.ipi_count_chunk);
	LIST_FOREACH(inp, &sctppcbinfo.listhead, sctp_list) {
		/* For each endpoint */
		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			/* For each association */
			sctp_drain_mbufs(inp, stcb);
		}
	}
}

int
sctp_add_to_socket_q(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	struct sctp_socket_q_list *sq;

	if ((inp == NULL) || (stcb == NULL)) {
		/* I am paranoid */
		return (0);
	}
	sq = (struct sctp_socket_q_list *)SCTP_ZONE_GET(
	    sctppcbinfo.ipi_zone_sockq);
	if (sq == NULL) {
		/* out of sq structs */
		return (0);
	}
	sctppcbinfo.ipi_count_sockq++;
	sctppcbinfo.ipi_gencnt_sockq++;
	stcb->asoc.cnt_msg_on_sb++;
	sq->tcb = stcb;
	TAILQ_INSERT_TAIL(&inp->sctp_queue_list, sq, next_sq);
	return (1);
}


struct sctp_tcb *
sctp_remove_from_socket_q(struct sctp_inpcb *inp)
{
	struct sctp_tcb *stcb = NULL;
	struct sctp_socket_q_list *sq;

	sq = TAILQ_FIRST(&inp->sctp_queue_list);
	if (sq == NULL)
		return (NULL);

	stcb = sq->tcb;
	TAILQ_REMOVE(&inp->sctp_queue_list, sq, next_sq);
	SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_sockq, sq);
	sctppcbinfo.ipi_count_sockq--;
	sctppcbinfo.ipi_gencnt_sockq++;
	if (stcb) {
		stcb->asoc.cnt_msg_on_sb--;
	}
	return (stcb);
}


/*
 * Callout/Timer routines for OS that doesn't have them
 */
#ifdef _SCTP_NEEDS_CALLOUT_
#ifndef __APPLE__
extern int ticks;
#endif

void
callout_init(struct callout *c)
{
	bzero(c, sizeof(*c));
}

void
callout_reset(struct callout *c, int to_ticks, void (*ftn)(void *), void *arg)
{
	int s;

	s = splhigh();
	if (c->c_flags & CALLOUT_PENDING)
		callout_stop(c);

	/*
	 * We could spl down here and back up at the TAILQ_INSERT_TAIL,
	 * but there's no point since doing this setup doesn't take much
	 * time.
	 */
	if (to_ticks <= 0)
		to_ticks = 1;

	c->c_arg = arg;
	c->c_flags = (CALLOUT_ACTIVE | CALLOUT_PENDING);
	c->c_func = ftn;
#ifdef __APPLE__
	c->c_time = to_ticks;	/* just store the requested timeout */
/*	thread_call_enter_delayed(c->c_call, to_ticks); */
	timeout(ftn, arg, to_ticks);
#else
	c->c_time = ticks + to_ticks;
	TAILQ_INSERT_TAIL(&sctppcbinfo.callqueue, c, tqe);
#endif
	splx(s);
}

int
callout_stop(struct callout *c)
{
	int	s;

	s = splhigh();
	/*
	 * Don't attempt to delete a callout that's not on the queue.
	 */
	if (!(c->c_flags & CALLOUT_PENDING)) {
		c->c_flags &= ~CALLOUT_ACTIVE;
		splx(s);
		return (0);
	}
	c->c_flags &= ~(CALLOUT_ACTIVE | CALLOUT_PENDING| CALLOUT_FIRED);
#ifdef __APPLE__
/*	thread_call_cancel(c->c_call); */
	untimeout(c->c_func, c->c_arg);
#else
	TAILQ_REMOVE(&sctppcbinfo.callqueue, c, tqe);
	c->c_func = NULL;
#endif
	splx(s);
	return (1);
}

#if !defined(__APPLE__)
void
sctp_fasttim(void)
{
	struct callout *c, *n;
	struct calloutlist locallist;
	int inited = 0;
	int s;
	s = splhigh();
	/* run through and subtract and mark all callouts */
	c = TAILQ_FIRST(&sctppcbinfo.callqueue);
	while (c) {
		n = TAILQ_NEXT(c, tqe);
		if (c->c_time <= ticks) {
			c->c_flags |= CALLOUT_FIRED;
			c->c_time = 0;
			TAILQ_REMOVE(&sctppcbinfo.callqueue, c, tqe);
			if (inited == 0) {
				TAILQ_INIT(&locallist);
				inited = 1;
			}
			/* move off of main list */
			TAILQ_INSERT_TAIL(&locallist, c, tqe);
		}
		c = n;
	}
	/* Now all the ones on the locallist must be called */
	if (inited) {
		c = TAILQ_FIRST(&locallist);
		while (c) {
			/* remove it */
			TAILQ_REMOVE(&locallist, c, tqe);
			/* now validate that it did not get canceled */
			if (c->c_flags & CALLOUT_FIRED) {
				c->c_flags &= ~CALLOUT_PENDING;
				splx(s);
				(*c->c_func)(c->c_arg);
				s = splhigh();
			}
			c = TAILQ_FIRST(&locallist);
		}
	}
	splx(s);
}
#endif
#endif /* _SCTP_NEEDS_CALLOUT_ */
