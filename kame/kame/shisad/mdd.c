/*      $Id: mdd.c,v 1.1 2004/09/27 04:06:02 t-momose Exp $  */
/*
 * Copyright (C) 2004 WIDE Project.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/mipsock.h>
#include <netinet/ip6mh.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef MIP_MCOA
#include <sys/sysctl.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* defined(__FreeBSD__) && __FreeBSD__ >= 3 */
#include <netinet6/in6_var.h>
#endif /* MIP_MCOA */

#include "mdd.h"

void reload(int);
void terminate(int);

static char *cmd;
int ver_major = 0;
int ver_minor = 1;
int debug = 0;
int numerichost = 0;
int cflag = 0;
int mflag = 0;
int hflag = 0;
int pflag = 0;
struct bl	bl_head;
struct cifl	cifl_head;
struct coacl	coacl_head;
int sock_rt, sock_m, sock_dg6, sock_dg, poll_time = -1;


#ifdef MIP_MCOA
static void dereg_detach_coa(struct ifa_msghdr *);
static int mipsock_deregforeign(struct sockaddr_in6 *, struct sockaddr_in6 *, 
				struct sockaddr_in6 *, int, u_int16_t);
extern void get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
extern int get_ifmsg(void);
#endif /* MIP_MCOA */

void
usage(void)
{
	fprintf(stderr, "%s\n", cmd);
	fprintf(stderr, "movement detection daemon for mobile router\n");
	fprintf(stderr, "\t[-h HoA]        HoA\n", cmd);
	fprintf(stderr, "\t[-i IF_for_CoA] Interface for watching\n", cmd);
	fprintf(stderr, "\t[-d]            Debug mode\n", cmd);
	fprintf(stderr, "\t[-n]            Don't resolve names\n", cmd);
	fprintf(stderr, "\t[-m]            Don\'t use mipsock\n", cmd);
	fprintf(stderr, "\t[-p interval]   polling link status per interval(sec)\n", cmd);
#ifdef MIP_MCOA
	fprintf(stderr, "\t[-b bid]   set Binding Unique Identifier", cmd);
	fprintf(stderr, "\t           -b must be used with -i and - h option.\n");
	fprintf(stderr, "\t           multiple mdds must be executed.\n");
#endif /* MIP6_MCOA */
}

int
main(int argc, char *argv[], char *env[])
{
	int ch;
	struct binding *bp = NULL;
#ifdef MIP_MCOA
	u_int16_t bid = 0;
#endif /* MIP_MCOA */

	/*  Clear all parameters */
	LIST_INIT(&bl_head);
	LIST_INIT(&cifl_head);
	LIST_INIT(&coacl_head);

	cmd = argv[0];

	/*  Option processing */
#ifndef MIP_MCOA
	while ((ch=getopt(argc, argv, "i:dnh:mp:")) != -1) {
#else
	while ((ch=getopt(argc, argv, "b:i:dnh:mp:")) != -1) {
#endif /* MIP_MCOA */
		switch (ch) {
#ifdef MIP_MCOA
		case 'b':
			bid = atoi(optarg);
			if (bid <= 0) {
				fprintf(stderr, "Please specify non zero value\n");
				usage();
				exit(0);
			}
			break;
#endif /* MIP_MCOA */
		case 'i':
			set_coaif(optarg);
			cflag += 1;
			break;
		case 'h':
#ifdef MIP_MCOA
			if (bp) {
				fprintf(stderr, "You can specify multiple HoA here\n");
				usage();
				exit(0);
			}
#endif /* MIP_MCOA */
			bp = set_hoa_str(optarg);
			hflag += 1;
			break;

		case 'd':
			debug += 1;
			break;

		case 'n':
			numerichost += 1;
			break;

		case 'm':
			mflag += 1;
			break;
		case 'p':
			printf("pflag %s\n", optarg);
			pflag += 1; 
			poll_time = atoi(optarg);
			break;
		default:
			usage();
			exit(0);
		}
	}
	argc -= optind;
	argv += optind;

#ifdef MIP_MCOA
	if (bid) {
		if (bp == NULL) {
			fprintf(stderr, "You must specify a HoA with -b option\n");
			usage();
			exit(0);
		}
		bp->bid = bid;
	} 
#endif /* MIP_MCOA */

	/*
	 *  Prepare sockets
	 */
	sock_rt = socket(PF_ROUTE, SOCK_RAW, 0);
	if (sock_rt < 0) {
		perror("socket(PF_ROUTE)");
		exit (-1);
	}

	if (!mflag) {
		sock_m = socket(PF_MOBILITY, SOCK_RAW, 0);
		if (sock_m < 0) {
			perror("socket(PF_MOBILITY)");
			exit (-1);
		}
	}

	sock_dg6 = socket(PF_INET6, SOCK_DGRAM, 0);
	if (sock_dg6 < 0) {
		perror("socket(PF_INET6)");
		exit (-1);
	}

	sock_dg = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock_dg < 0) {
		perror("socket(PF_INET)");
		exit (-1);
	}

	/* Get static parameters */
	if (!cflag) {
		get_coaiflist();
		if (debug > 1) {
			print_coaiflist(stderr);
			fprintf(stderr, "\n");
		}
	}
	if (!hflag) {
		get_hoalist();
		if (debug > 1) {
			print_bl(stderr);
			fprintf(stderr, "\n");
		}
	}


	/* Get dynamic parameters, at this moment */
	get_coacandidate();
	set_coa();

	/*
	 *  Show starting parameters, if started as debug mode
	 */
	if (debug) {
		fprintf(stderr, "# Command:	%s\n", cmd);
		fprintf(stderr, "# Version: 	%d.%d\n", ver_major, ver_minor);
		fprintf(stderr, "# Debug level:	%d\n", debug);
		fprintf(stderr, "\n");

		print_coaiflist(stderr);
		fprintf(stderr, "\n");

		print_bl(stderr);
		fprintf(stderr, "\n");
	}
	sync_binding();

	/*
	 *  Main loop
	 */
	signal(SIGHUP, reload);
	signal(SIGTERM, terminate);
	signal(SIGINT, terminate);

	if (debug == 0) {
		if (daemon(0, 0) < 0) {
			perror("daemon");
			terminate(0);
			exit(-1);
		}
	}

	mainloop();

	/* not reached */
	return 0;
}

void
reload(int dummy)
{
	struct binding *bp;
	struct cif *cifp;
	struct coac *cp;

	if (debug) {
		fprintf(stderr, "Reload parameters\n");
	}

	while (!LIST_EMPTY(&bl_head)) {
		bp = LIST_FIRST(&bl_head);
		LIST_REMOVE(bp, binding_entries);
		free(bp);
	}
	while (!LIST_EMPTY(&cifl_head)) {
		cifp = LIST_FIRST(&cifl_head);
		LIST_REMOVE(cifp, cif_entries);
		free(cifp);
	}
	while (!LIST_EMPTY(&coacl_head)) {
		cp = LIST_FIRST(&coacl_head);
		LIST_REMOVE(cp, coac_entries);
		free(cp);
	}

	if (!cflag) get_coaiflist();
	if (!hflag) get_hoalist();
	get_coacandidate();
	set_coa();
	sync_binding();
}


void
terminate(int dummy)
{

	if (debug) {
		fprintf(stderr, "Terminate\n");
	}

	close(sock_rt);
	close(sock_dg6);
	close(sock_dg);
	if (!mflag)
		close(sock_m);

	exit(-1);
}

struct binding *
set_hoa(struct in6_addr	*ia6, int prefixlen)
{
	struct binding *bp;

	bp = (struct binding *) malloc(sizeof(struct binding));
	if (bp == NULL) {
		perror("malloc:");
		return NULL;
	}
	memset(bp, 0, sizeof(*bp));
	bp->hoa.sin6_len	= sizeof(bp->hoa);
	bp->hoa.sin6_family	= AF_INET6;
	bp->hoa.sin6_port	= htons(0);;
	memcpy(&bp->hoa.sin6_addr, ia6, sizeof(*ia6));
	bp->hoa.sin6_flowinfo	= htonl(0);;
	bp->hoa_prefixlen 	= prefixlen;
	bp->flags 		= BF_INUSE;
	LIST_INSERT_HEAD(&bl_head, bp, binding_entries);

	return bp;
}

struct binding *
set_hoa_str(char *addr)
{
	struct in6_addr	ia6;
	char *cp;
	int prefixlen = DEFAULT_PREFIXLEN;

	cp = strchr(addr, '/');
	if (cp != NULL) {
		*cp = '\0';
		cp++;
		prefixlen = strtol(cp, NULL, 10);
		if (errno == ERANGE) prefixlen = DEFAULT_PREFIXLEN;
	}

	if (inet_pton(AF_INET6, addr, &ia6) < 0) {
		return NULL;
	}
	return set_hoa(&ia6, prefixlen);
}

void
get_hoalist(void)
{
	_get_hoalist();
}

void
set_coaif(char *ifname)
{
	struct cif *cifp;

	cifp = (struct cif *) malloc(sizeof(struct cif));
	cifp->cif_name = ifname;
	cifp->cif_linkstatus = -1;
	LIST_INSERT_HEAD(&cifl_head, cifp, cif_entries);
}

void
get_coaiflist(void)
{
	struct cif *cifp;
	struct ifreq ifr;

	get_ifl(&cifl_head);
	del_if_from_ifl(&cifl_head, IFT_MIP);

    retry:
	LIST_FOREACH(cifp, &cifl_head, cif_entries) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, cifp->cif_name, IFNAMSIZ);
		if (ioctl(sock_dg, SIOCGIFFLAGS, &ifr) < 0) {
			perror("ioctl(SIOCGIFFLAGS)");
			continue;
		}

		if (ifr.ifr_flags & IFF_LOOPBACK) {
			LIST_REMOVE(cifp, cif_entries);
			free(cifp->cif_name);
			free(cifp);
			goto retry;
		}
	}
}



void
get_coacandidate(void)
{
	struct coac *cp;
	char buf[PA_BUFSIZE];

	/* List Deletion. */
	while (!LIST_EMPTY(&coacl_head)) {
		cp = LIST_FIRST(&coacl_head);
		LIST_REMOVE(cp, coac_entries);
		free(cp);
	}

	get_addr_with_ifl(&coacl_head, &cifl_head);

    retry:
	LIST_FOREACH(cp, &coacl_head, coac_entries) {
		if (in6_addrscope(&cp->coa.sin6_addr)
					!= __IPV6_ADDR_SCOPE_GLOBAL) {
			LIST_REMOVE(cp, coac_entries);
			free(cp);
			goto retry;
		}
	}

	if (debug > 1) {
		fprintf(stderr, "CoA candidate\n");
		LIST_FOREACH(cp, &coacl_head, coac_entries) {
			fprintf(stderr, "\tCoA: %s\n",
				(char *) inet_ntop(AF_INET6, &cp->coa.sin6_addr,
							buf, sizeof(buf)));
		}
	}
}



int
in6_matchlen(struct in6_addr *a1, struct in6_addr *a2)
{
	int bytes = sizeof(struct in6_addr)/sizeof(u_int8_t);
	int i, j;
	u_int8_t mask;

	for (i=0; i<bytes; i++) {
		mask = 0;
		for (j=0; j<8; j++) {
			mask |= 0x80 >> j;
			if ((mask & a1->s6_addr[i]) != (mask & a2->s6_addr[i]))
				return 8 * i + j;
		}
	}

	/* same address */
	return 8*i;
}



void
set_coa(void)
{
	struct coac *cp;
	struct binding *bp;
	int maxmatchlen, matchlen;
	struct sockaddr_in6 sin6;

	LIST_FOREACH(bp, &bl_head, binding_entries) {
		maxmatchlen = -1;
		LIST_FOREACH(cp, &coacl_head, coac_entries) {
			matchlen = in6_matchlen(&bp->hoa.sin6_addr,
							&cp->coa.sin6_addr);
			if (maxmatchlen < matchlen) {
				maxmatchlen = matchlen;
				memcpy(&sin6, &cp->coa, sizeof(sin6));
			}
		}
		if (maxmatchlen < 0) {
			bp->flags &= ~BF_BOUND;
			bp->flags &= ~BF_HOME;
			memcpy(&bp->pcoa, &bp->coa, sizeof(bp->pcoa));
			memset(&bp->coa, 0, sizeof(bp->coa));
		} else
		if (maxmatchlen >= bp->hoa_prefixlen) {
			bp->flags &= ~BF_BOUND;
			bp->flags |= BF_HOME;
			memcpy(&bp->pcoa, &bp->coa, sizeof(bp->pcoa));
			bp->pcoaifindex = bp->coaifindex;
			memcpy(&bp->coa, &sin6, sizeof(bp->coa));
			bp->coaifindex = in6_addr2ifindex(&bp->coa.sin6_addr);
		} else {
			bp->flags |= BF_BOUND;
			bp->flags &= ~BF_HOME;
			memcpy(&bp->pcoa, &bp->coa, sizeof(bp->pcoa));
			memcpy(&bp->coa, &sin6, sizeof(bp->coa));
		}
	}
}

void
sync_binding(void)
{
	struct binding *bp;

	LIST_FOREACH(bp, &bl_head, binding_entries) {
		if (memcmp(&bp->coa, &bp->pcoa, sizeof(bp->coa)) == 0)
				continue;

		if (bp->flags & BF_HOME) {
			/* HOME */
			returntohome(&bp->hoa, &bp->coa, bp->coaifindex);
		} else
		if (bp->flags & BF_BOUND) {
#ifndef MIP_MCOA
			chbinding(&bp->hoa, &bp->coa);
#else
			chbinding(&bp->hoa, &bp->coa, bp->bid);
#endif /* MIP_MCOA */
		}
	}
}

void
print_bl(FILE *fp)
{
	char buf[PA_BUFSIZE];
	struct binding *bp;

	LIST_FOREACH(bp, &bl_head, binding_entries) {
		fprintf(fp, "HoA: %s",
			(char *) inet_ntop(AF_INET6, &bp->hoa.sin6_addr,
							buf, sizeof(buf)));
		if (bp->flags & BF_BOUND) {
			fprintf(fp, "\t-> %s",
				(char *) inet_ntop(AF_INET6, &bp->coa.sin6_addr,
							buf, sizeof(buf)));
		}
		fprintf(stderr, "\n");
	}
}

void
print_coaiflist(FILE *fp)
{
	struct cif *cifp;

	LIST_FOREACH(cifp, &cifl_head, cif_entries) {
		fprintf(fp, "CoA IF: %s\n", cifp->cif_name);
	}
}

void
mainloop(void)
{
	char buf[BUFSIZE];
	struct if_msghdr *ifm = (struct if_msghdr *) buf;
	struct mip_msghdr *mhdr= (struct mip_msghdr *) buf;
	struct mipm_home_hint *m = (struct mipm_home_hint *) buf;
	struct timeval tv;
	fd_set fds, tfds;
	int nfds;
	int cc;

	if (pflag) {
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = poll_time;
		tv.tv_usec = 0;
	}
	FD_ZERO(&fds);
	nfds = -1;

	FD_SET(sock_rt, &fds);
	if (sock_rt >= nfds)	nfds = sock_rt + 1;
	FD_SET(sock_m, &fds);
	if (sock_m >= nfds)	nfds = sock_m + 1;

	for (;;) {
		tfds = fds;
		if (select(nfds, &tfds, NULL, NULL, (pflag) ? &tv : NULL) < 0) {
			exit(-1);
		}

		if (FD_ISSET(sock_m, &tfds)) {
			if ((cc=read(sock_m, buf, sizeof(buf))) < 0) {
				exit(-1);
			}
			if (debug > 1) {
				fprintf(stderr, "SOCK_MOB: type=%d\n",
						mhdr->miph_type);
			}

			switch (mhdr->miph_type) {
			case MIPM_HOME_HINT:
				recv_home_hint((int)m->mipmhh_ifindex,
				    (struct sockaddr_in6 *)m->mipmhh_prefix,
				    (int) m->mipmhh_prefixlen);
				sync_binding();
				break;
			default:
				break;
			}

		}

		if (FD_ISSET(sock_rt, &tfds)) {
			if ((cc=read(sock_rt, buf, sizeof(buf))) < 0) {
				exit(-1);
			}
			if (debug > 1) {
				fprintf(stderr, "SOCK_ROUTE: type=%d\n",
							ifm->ifm_type);
			}

			if (ifm->ifm_type != RTM_NEWADDR &&
			    ifm->ifm_type != RTM_DELADDR &&
			    ifm->ifm_type != RTM_ADDRINFO) continue;

#ifdef MIP_MCOA
			if (ifm->ifm_type == RTM_ADDRINFO) 
				(void) dereg_detach_coa((struct ifa_msghdr *) ifm);
#endif /* MIP_MCOA */

			if (in6_is_on_homenetwork(
					(struct ifa_msghdr *) ifm, &bl_head))
					continue;

			get_coacandidate();
			set_coa();
			sync_binding();

			if (debug > 2) {
				print_bl(stderr);
				fprintf(stderr, "\n");
			}
		}

		if (pflag)
			probe_ifstatus(sock_dg6);
	}
}

#ifdef MIP_MCOA
static void
dereg_detach_coa(difam) 
	struct ifa_msghdr *difam;
{
        struct in6_ifreq ifr6;
        struct sockaddr_in6 dsin6, *sin6;
        struct sockaddr *rti_info[RTAX_MAX];
        char ifname[IFNAMSIZ];
	char *next, *limit;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;

        if (if_indextoname(difam->ifam_index, ifname) == NULL)
                return;

        if (strncmp(ifname, "mip", 3) == 0) 
                return;

        get_rtaddrs(difam->ifam_addrs, (struct sockaddr *) (difam + 1), rti_info);
	if (rti_info[RTAX_IFA] == NULL)
		return;

	memset(&dsin6, 0, sizeof(dsin6));
	memcpy(&dsin6, (struct sockaddr_in6 *) rti_info[RTAX_IFA], sizeof(dsin6));

	memset(&ifr6, 0, sizeof(ifr6));
	ifr6.ifr_addr = dsin6;
	strncpy(ifr6.ifr_name, ifname, strlen(ifname));
	if (ioctl(sock_dg6, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
		perror("ioctl(SIOCGIFAFLAG_IN6)");
		return;
	}


	/* address is now detached from the link, send
         *  deregfromforeign to mnd 
	 */
	if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DETACHED) {
		int mib[6];
		char *ifmsg = NULL;
		int len;

		mib[0] = CTL_NET;
		mib[1] = PF_ROUTE;
		mib[2] = 0;
		mib[3] = AF_INET6;
		mib[4] = NET_RT_IFLIST;
		mib[5] = 0;

		if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
			perror("sysctl");
			return;
		}
		if ((ifmsg = malloc(len)) == NULL) {
			perror("malloc");
			return;
		}
		if (sysctl(mib, 6, ifmsg, &len, NULL, 0) < 0) {
			perror("sysctl");
			free(ifmsg);
			return;
		}
        
		limit = ifmsg +  len;
		for (next = ifmsg; next < limit; next += ifm->ifm_msglen) {
			char buf[1024];
			struct binding *bp;

			ifm = (struct if_msghdr *) next;

			if (ifm->ifm_type == RTM_NEWADDR) {
				ifam = (struct ifa_msghdr *) next;

				get_rtaddrs(ifam->ifam_addrs,
					    (struct sockaddr *) (ifam + 1), rti_info);
				sin6 = (struct sockaddr_in6 *) rti_info[RTAX_IFA];
				memset(&ifr6, 0, sizeof(ifr6));
				ifr6.ifr_addr = *sin6;

				/* unknown interface !? */
				if (if_indextoname(ifm->ifm_index, ifr6.ifr_name) == NULL) 
					continue;

				/* MUST be global */
				if (in6_addrscope(&sin6->sin6_addr) !=  __IPV6_ADDR_SCOPE_GLOBAL) 
					continue;
					
				if (ioctl(sock_dg6, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
					perror("ioctl(SIOCGIFAFLAG_IN6)");
					continue;
				}
				if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_READONLY) 
					continue;
				
				fprintf(stderr, "Detached address is %s\n", 
					inet_ntop(AF_INET6, &dsin6.sin6_addr, buf, sizeof(buf)));
				fprintf(stderr, "send dereg from address is %s\n", 
					inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf)));
				
				LIST_FOREACH(bp, &bl_head, binding_entries) {
					if (memcmp(&bp->coa, &dsin6, sizeof(bp->coa)) == 0)
						break;
				}
				if (bp == NULL)
					break;
				mipsock_deregforeign(&bp->hoa, &dsin6, sin6, 
						     ifm->ifm_index, bp->bid);

				/* send bu to msock */
				free(ifmsg);
				return;
			}
		}
		if (ifmsg)
			free(ifmsg);
        }

	return;
}

static int
mipsock_deregforeign(hoa, deregcoa, newcoa, ifindex, bid)
	struct sockaddr_in6 *hoa, *deregcoa, *newcoa;
	int ifindex;
	u_int16_t bid;
{
	int len;
	struct mipm_md_info *mdinfo;
	char buf[PA_BUFSIZE];

	len = sizeof(*mdinfo) + sizeof(*hoa) + sizeof(*deregcoa) + sizeof(*newcoa);
	mdinfo = (struct mipm_md_info *) malloc(len);
	if (mdinfo == NULL) 
		return -1;

	memset(mdinfo, 0, len);
	mdinfo->mipm_md_hdr.miph_msglen	= len;
	mdinfo->mipm_md_hdr.miph_version = MIP_VERSION;
	mdinfo->mipm_md_hdr.miph_type	= MIPM_MD_INFO;
	mdinfo->mipm_md_hdr.miph_seq	= random();
	mdinfo->mipm_md_hint		= MIPM_MD_ADDR;
	mdinfo->mipm_md_command		= MIPM_MD_DEREGFOREIGN;
	mdinfo->mipm_md_ifindex		= ifindex;
	mdinfo->mipm_md_bid = bid; 

	memcpy(MIPD_HOA(mdinfo), hoa, sizeof(*hoa));
	memcpy(MIPD_COA(mdinfo), deregcoa, sizeof(*deregcoa));
	memcpy(MIPD_COA2(mdinfo), newcoa, sizeof(*newcoa));

	if (!mflag) {
		if (write(sock_m, mdinfo, len) < 0) {
			perror("wirte");
			return -1;
		}
	}

	if (mdinfo)
		free(mdinfo);

	if (debug) {
		fprintf(stderr, "Dereg from foreign: %s\n",
			(char *) inet_ntop(AF_INET6, &deregcoa->sin6_addr,
							buf, sizeof(buf)));
 	}

	return 0;
}
#endif /* MIP_MCOA */

int
#ifndef MIP_MCOA
chbinding(hoa, coa)
	struct sockaddr_in6 *hoa, *coa;
#else
chbinding(hoa, coa, bid)
	struct sockaddr_in6 *hoa, *coa;
	u_int16_t bid;
#endif /* MIP_MCOA */
{
	int len;
	struct mipm_md_info *mdinfo;
	char buf[PA_BUFSIZE];

	len = sizeof(*mdinfo) + sizeof(*hoa) + sizeof(*coa);
	mdinfo = (struct mipm_md_info *) malloc(len);
	if (mdinfo == NULL) return -1;

	memset(mdinfo, 0, len);
	mdinfo->mipm_md_hdr.miph_msglen	= len;
	mdinfo->mipm_md_hdr.miph_version = MIP_VERSION;
	mdinfo->mipm_md_hdr.miph_type	= MIPM_MD_INFO;
	mdinfo->mipm_md_hdr.miph_seq	= random();
	mdinfo->mipm_md_hint		= MIPM_MD_ADDR;
	mdinfo->mipm_md_command		= MIPM_MD_REREG;
#ifdef MIP_MCOA
	mdinfo->mipm_md_bid = bid; 
#endif /* MIP_MCOA */
	memcpy(MIPD_HOA(mdinfo), hoa, sizeof(*hoa));
	memcpy(MIPD_COA(mdinfo), coa, sizeof(*coa));

	if (!mflag) {
		if (write(sock_m, mdinfo, len) < 0) {
			perror("wirte");
			return -1;
		}
	}

	if (debug) {
		fprintf(stderr, "Binding: %s",
			(char *) inet_ntop(AF_INET6, &hoa->sin6_addr,
							buf, sizeof(buf)));
		fprintf(stderr, "\t-> %s\n",
			(char *) inet_ntop(AF_INET6, &coa->sin6_addr,
							buf, sizeof(buf)));
	}

	return 0;
}


int
returntohome(struct sockaddr_in6 *hoa, struct sockaddr_in6 *coa, int ifindex)
{
	int len;
	struct mipm_md_info *mdinfo;
	char buf[PA_BUFSIZE];

	len = sizeof(*mdinfo) + sizeof(*hoa) + sizeof(*coa);
	mdinfo = (struct mipm_md_info *) malloc(len);
	if (mdinfo == NULL) return -1;

	memset(mdinfo, 0, len);
	mdinfo->mipm_md_hdr.miph_msglen	= len;
	mdinfo->mipm_md_hdr.miph_version = MIP_VERSION;
	mdinfo->mipm_md_hdr.miph_type	= MIPM_MD_INFO;
	mdinfo->mipm_md_hdr.miph_seq	= random();
	mdinfo->mipm_md_hint		= MIPM_MD_ADDR;
	mdinfo->mipm_md_command		= MIPM_MD_DEREGHOME;
	mdinfo->mipm_md_ifindex		= ifindex;
	memcpy(MIPD_HOA(mdinfo), hoa, sizeof(*hoa));
	memcpy(MIPD_COA(mdinfo), hoa, sizeof(*hoa));

	if (!mflag) {
		if (write(sock_m, mdinfo, len) < 0) {
			perror("wirte");
			return -1;
		}
	}

	if (debug) {
		fprintf(stderr, "Return to home: %s\n",
			(char *) inet_ntop(AF_INET6, &hoa->sin6_addr,
							buf, sizeof(buf)));
 	}

	return 0;
}

void
recv_home_hint(int ifindex, struct sockaddr_in6 *sin6, int prefixlen)
{
	struct binding *bp;
	int matchlen;

	LIST_FOREACH(bp, &bl_head, binding_entries) {
		matchlen = in6_matchlen(&bp->hoa.sin6_addr, &sin6->sin6_addr);
		if (matchlen >= prefixlen) {
			bp->flags &= ~BF_BOUND;
			bp->flags |= BF_HOME;
			memcpy(&bp->pcoa, &bp->coa, sizeof(bp->pcoa));
			bp->pcoaifindex =bp->coaifindex;
			memcpy(&bp->coa, &bp->hoa, sizeof(bp->coa));
			bp->coaifindex = ifindex;
		}
	}
}
