/*	$Id: mnd.c,v 1.7 2004/10/13 16:13:43 keiichi Exp $	*/

/*
 * Copyright (C) 2004 WIDE Project.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <poll.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/if_dl.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */
#include <net/route.h>
#include <net/mipsock.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/ip6mh.h>
#include <netinet/icmp6.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/mip6.h>
#include <arpa/inet.h>

#include "callout.h"
#include "stat.h"
#include "shisad.h"
#include "fsm.h"
#include "fdlist.h"
#include "command.h"

/* Global Variables */
int mipsock, icmp6sock, mhsock, csock;
int debug = 0, numerichost = 0;
struct mip6_mipif_list mipifhead;
struct mip6_hinfo_list hoa_head;
struct no_ro_head noro_head;
struct mip6stat mip6stat;

static int default_lifetime = MIP6_DEFAULT_BINDING_LIFE;

static void command_show_status(int, char *);
static void command_flush(int, char *);
static void command_show_hal(int);

struct command_table command_table[] = {
#ifndef MIP_NEMO
	{"show", command_show_status, "Show stat, bul, hal, kbul, noro"},
#else
	{"show", command_show_status, "Show stat, bul, hal, kbul, noro, pt"},
#endif /* MIP_NEMO */
	{"flush", command_flush, "Flush stat, bul, hal, noro"},
};


static void mn_lists_init(void);
static int mipsock_recv_rr_hint(struct mip_msghdr *);
static void mnd_init_homeprefix(u_int16_t, struct mip6_hpfx_list *);
static struct mip6_mipif *mnd_add_mipif(char *);
static void terminate(int);
#ifndef MIP_MCOA
static int mipsock_md_dereg_bul_fl(struct in6_addr *, struct in6_addr *, 
    struct in6_addr *, u_int16_t);
#else
static int mipsock_md_dereg_bul_fl(struct in6_addr *, struct in6_addr *, 
    struct in6_addr *, u_int16_t, u_int16_t);
#endif /* !MIP_MCOA */

static int add_hal_by_commanline_xxx(char *);

static void noro_show(int);
static void noro_init(void);
static void noro_sync(void);


void
mn_usage()
{
#ifdef MIP_NEMO
	char *banner = "mrd [-dn] -f prefixtable -i mip0 mip1 ...\n";
#else
	char *banner = "mnd [-dn] -i mip0 mip1 ...\n";
#endif /* MIP_NEMO */

	fprintf(stderr, banner);
#ifdef MIP_MCOA
	fprintf(stderr, "Multiple CoA Reg Support version\n");
#endif /* MIP_MCOA */
#ifdef MIP_NEMO
	fprintf(stderr, "Basic NEMO Support version\n");
#endif /* MIP_NEMO */
        return;
}

int
main(int argc, char **argv)
{
	int pfds, ch = 0;
	char *arg_ifname = NULL;
	struct mip6_hoainfo *hoainfo = NULL;
	struct binding_update_list *bul;
	char *homeagent = NULL;
#ifdef MIP_NEMO
	char *nemofile = NULL;
#endif /* MIP_NEMO */

	if (argc < 2) {
		mn_usage();
		exit(-1);
	}

#ifdef MIP_NEMO
        while ((ch = getopt(argc, argv, "dna:if:")) != -1)
#else
        while ((ch = getopt(argc, argv, "dna:il:")) != -1) 
#endif
	{
                switch (ch) {
                case 'd':
                        debug = 1;
                        break;
                case 'n':
                        numerichost = 1;
                        break;
		case 'a':
			homeagent = optarg;
			break;
		case 'i':	/* Is it necessary to use '-i' option? Specifing interface is mandatory for mnd. thus, using option is too lengthy */
			goto startmn;
			break;
		case 'l':
			if (atoi(optarg) > 0)
				default_lifetime = atoi(optarg);
			break;
#ifdef MIP_NEMO
		case 'f':
			nemofile = optarg;
			break;
#endif /* MIP_NEMO */
                default:
                        fprintf(stderr, "unknown option\n");
                        mn_usage();
                        break;
                }
        }
	
	mn_usage();

	return (-1);

 startmn:
	/* open syslog infomation. */
	openlog("shisad(mnd)", 0, LOG_DAEMON);
	syslog(LOG_INFO, "Start Mobile Node/Router\n");

	if (optind >= argc) {
		mn_usage();
		return (-1);
	}

	mhsock_open();
	icmp6sock_open();
	mipsock_open();

	mn_lists_init();

	noro_init();

	callout_init();
	fdlist_init();
	csock = command_init("mn> ", command_table, 
		sizeof(command_table) / sizeof(struct command_table), 7778);
	if (csock < 0) {
		fprintf(stderr, "Unable to open user interface\n");
	}


	/* Initialization of mip virtual interfaces, home address and
	 * binding update list */
	for (; optind < argc; optind ++) {
		arg_ifname = argv[optind];

		if (arg_ifname == NULL)
			break;

		if (mnd_add_mipif(arg_ifname) == NULL) {
			syslog(LOG_ERR, "%s is invalid\n", arg_ifname);
			exit(0);
		}
	}
#ifdef MIP_NEMO
	nemo_parse_conf(nemofile);
#endif /* MIP_NEMO */

#if 1
	/* ETSI 2004.10.12 XXX */
	/* install a home agent address, if specified. */
	if (homeagent != NULL)
		add_hal_by_commanline_xxx(homeagent);
#endif

	/* let's insert NULL binding update list to each binding update list */
	for (hoainfo = LIST_FIRST(&hoa_head); hoainfo;
	     hoainfo = LIST_NEXT(hoainfo, hinfo_entry)) {
#ifndef MIP_NEMO
#ifndef MIP_MCOA 
		bul = bul_insert(hoainfo, NULL, NULL,
		    (IP6_MH_BU_HOME|IP6_MH_BU_ACK));
#else
		bul = bul_insert(hoainfo, NULL, NULL, 
		    (IP6_MH_BU_HOME|IP6_MH_BU_ACK|IP6_MH_BU_MCOA), 0);
#endif /* !MIP_MCOA */
#else /* !MIP_NEMO */
#ifndef MIP_MCOA
		bul = bul_insert(hoainfo, NULL, NULL, 
		    (IP6_MH_BU_HOME|IP6_MH_BU_ACK|IP6_MH_BU_ROUTER));
#else
		/* XXX how should bid be handled here ... */ 
		bul = bul_insert(hoainfo, NULL, NULL, 
		    (IP6_MH_BU_HOME|IP6_MH_BU_ACK|IP6_MH_BU_ROUTER|IP6_MH_BU_MCOA), 0);
#endif /* !MIP_MCOA */
#endif /* !MIP_NEMO */
		if (bul == NULL) {
			syslog(LOG_ERR,
			    "cannot insert bul, something wrong\n");
			 continue;
		}

		syslog(LOG_INFO, "Kick fsm to MOVEMENT\n");
		/* kick the fsm to start its state transition. */
		bul_kick_fsm(bul, MIP6_BUL_FSM_EVENT_MOVEMENT, NULL);
	}

	new_fd_list(mipsock, POLLIN, mipsock_input_common);
	new_fd_list(mhsock, POLLIN, mh_input_common);
	new_fd_list(icmp6sock, POLLIN, icmp6_input_common);

	/* notify a kernel to behave as a mobile node. */
#ifndef MIP_NEMO
	mipsock_nodetype_request(MIP6_NODETYPE_MOBILE_NODE, 1);
#else
	mipsock_nodetype_request(MIP6_NODETYPE_MOBILE_ROUTER, 1);
#endif

	/* register signal handlers. */
	signal(SIGTERM, terminate);
	signal(SIGINT, terminate);

	if (debug == 0)
		daemon(0, 0);

	/* main loop. */
	while (1) {
		clear_revents();
	    
		if ((pfds = poll(fdl_fds, fdl_nfds, get_next_timeout())) < 0) {
			perror("poll");
			continue;
		}
		if (pfds != 0) {
			dispatch_fdfunctions(fdl_fds, fdl_nfds);
		}
		/* Timeout */
		callout_expire_check();
	}

	/* not reach */
	return (0);
}


static void
mn_lists_init()
{

	LIST_INIT(&hoa_head);
	LIST_INIT(&mipifhead);
	LIST_INIT(&noro_head);
}


/* mipsock BUL add and delete functions */
int
mipsock_bul_request (bul, command)
	struct binding_update_list *bul;
	u_char command;
{
	char buf[1024];
	int err = 0;
	struct mipm_bul_info *buinfo;
	struct sockaddr_in6 hoa_s6, coa_s6, peer_s6;

	if (command < MIPM_BUL_ADD || command > MIPM_BUL_REMOVE)
		return (EOPNOTSUPP);

	if (bul->bul_hoainfo == NULL) 
		return (EINVAL);

	memset(&hoa_s6, 0, sizeof(hoa_s6));
	memset(&coa_s6, 0, sizeof(coa_s6));
	memset(&peer_s6, 0, sizeof(peer_s6));

	hoa_s6.sin6_len = coa_s6.sin6_len = 
		peer_s6.sin6_len = sizeof(struct sockaddr_in6);
	hoa_s6.sin6_family = coa_s6.sin6_family =
		peer_s6.sin6_family = AF_INET6;
	
	hoa_s6.sin6_addr = bul->bul_hoainfo->hinfo_hoa;
	coa_s6.sin6_addr = bul->bul_coa;
	peer_s6.sin6_addr = bul->bul_peeraddr;

	memset(buf, 0, sizeof(buf));
	buinfo = (struct mipm_bul_info *)buf;

	buinfo->mipu_msglen = 
		sizeof(struct mipm_bul_info) + sizeof(struct sockaddr_in6) * 3;
	buinfo->mipu_version = MIP_VERSION;
	buinfo->mipu_type = command;
	buinfo->mipu_seq = random();
	buinfo->mipu_flags = bul->bul_flags;
	buinfo->mipu_hoa_ifindex = bul->bul_hoainfo->hinfo_ifindex;
#ifdef MIP_MCOA
	buinfo->mipu_bid = bul->bul_bid;
#endif /* MIP_MCOA */
	/* buinfo->mipu_coa_ifname xxx */
	buinfo->mipu_state = bul->bul_state;
	memcpy(MIPU_HOA(buinfo), &hoa_s6, hoa_s6.sin6_len);
	memcpy(MIPU_COA(buinfo), &coa_s6, coa_s6.sin6_len);
	memcpy(MIPU_PEERADDR(buinfo), &peer_s6, peer_s6.sin6_len);

 	err = write(mipsock, buinfo, buinfo->mipu_msglen);
	
	return (0);
}

int
mipsock_recv_mdinfo(miphdr)
	struct mip_msghdr *miphdr;
{
	struct mipm_md_info *mdinfo;
	struct sockaddr *sin;
	struct in6_addr *hoa, *coa, *acoa;
	int err = 0;

	syslog(LOG_INFO, "mipsock_recv_mdinfo\n");

	mdinfo = (struct mipm_md_info *)miphdr;

	/* Get HoA (if ifindex is specified, HoA could be :: */
	sin = MIPD_HOA(mdinfo); 
	if (sin->sa_family != AF_INET6)
		return (0);
	hoa = &((struct sockaddr_in6 *)sin)->sin6_addr;

	/* Get CoA */
	sin = MIPD_COA(mdinfo); 
	if (sin->sa_family != AF_INET6)
		return (0);
	coa = &((struct sockaddr_in6 *)sin)->sin6_addr;

	/* If new CoA is not global, ignore */
	if (IN6_IS_ADDR_LINKLOCAL(coa)
	    || IN6_IS_ADDR_MULTICAST(coa)
	    || IN6_IS_ADDR_LOOPBACK(coa)
	    || IN6_IS_ADDR_V4MAPPED(coa)
	    || IN6_IS_ADDR_UNSPECIFIED(coa))
		return (EINVAL);

	if (debug) 
		syslog(LOG_INFO, "new coa is %s", ip6_sprintf(coa));

	/* Update bul according to md_hint */
	switch (mdinfo->mipm_md_command) {
	case MIPM_MD_REREG:
		/* XXX do we need MIPM_MD_INDEX?! */
		if (mdinfo->mipm_md_hint == MIPM_MD_INDEX)
			err = mipsock_md_update_bul_byifindex(mdinfo->mipm_md_ifindex, coa);
		else if (mdinfo->mipm_md_hint == MIPM_MD_ADDR)
#ifndef MIP_MCOA
			err = bul_update_by_mipsock_w_hoa(hoa, coa);
#else
			err = bul_update_by_mipsock_w_hoa(hoa, coa, mdinfo->mipm_md_bid);
#endif /* MIP_MCOA */
		break;
	case MIPM_MD_DEREGHOME:
		err = mipsock_md_dereg_bul(hoa, coa, mdinfo->mipm_md_ifindex);
		break;
	case MIPM_MD_DEREGFOREIGN:
		/* Get CoA to send de-reg BU */
		sin = MIPD_COA2(mdinfo); 
		if (sin->sa_family != AF_INET6)
			return (0);
		acoa = &((struct sockaddr_in6 *)sin)->sin6_addr;

#ifndef MIP_MCOA
		err = mipsock_md_dereg_bul_fl(hoa, coa, acoa, mdinfo->mipm_md_ifindex);
#else 
		err = mipsock_md_dereg_bul_fl(hoa, coa, acoa, 
					      mdinfo->mipm_md_ifindex, mdinfo->mipm_md_bid);
#endif /* !MIP_MCOA */
		break;
	default:
		syslog(LOG_ERR, "unsupported md_info command %d\n",
		    mdinfo->mipm_md_command);
		err = EOPNOTSUPP;
		break;
	}

	return (err);
}

int
mipsock_md_update_bul_byifindex(ifindex, coa)
	u_int16_t ifindex;
	struct in6_addr *coa;
{
	syslog(LOG_ERR,
	       "mipsock_md_update_bul_byifindex is not supported yet\n");
	return (0);
}

/* DE-REGISTRATION (i.e. FL to HL movement only) */
int
mipsock_md_dereg_bul(hoa, coa, ifindex)
	struct in6_addr *hoa, *coa;
	u_int16_t ifindex;
{

	struct mip6_hoainfo *hoainfo;
	struct binding_update_list *bul, *bul_next;
	char ifname[IFNAMSIZ], mipifname[IFNAMSIZ];
	int err = 0;

	hoainfo = hoainfo_find_withhoa(hoa);
	if (hoainfo == NULL)
		return (ENETDOWN);

	if (!IN6_ARE_ADDR_EQUAL(hoa, coa)) 
		return (EINVAL);

	/* Remove HoA from viturla interface */
	if (if_indextoname(hoainfo->hinfo_ifindex, mipifname) == NULL) 
		return (EINVAL);
	err = delete_ip6addr(mipifname, &hoainfo->hinfo_hoa, 64);
	if (err) {
		syslog(LOG_ERR,
		    "removing a home address (%s) from %s failed.\n",
		    ip6_sprintf(&hoainfo->hinfo_hoa), mipifname);
		return (err);
	}

	/*
	 * add a home address to the physical interface specified by
	 * the movement detector.
	 */
	if (if_indextoname(ifindex, ifname) == NULL) 
		return (EINVAL);
#if 1
	/* ETSI 2004.10.13 */
{
	int flags;

	bul = bul_get_homeflag(&hoainfo->hinfo_hoa);
	if (bul == NULL) {
		syslog(LOG_ERR, "mipsock_md_dereg_bul: "
		    "received home hint, but there is no bul for %s\n",
		    ip6_sprintf(&hoainfo->hinfo_hoa));
		return (-1);
	}
	flags = IN6_IFF_NODAD|IN6_IFF_HOME|IN6_IFF_AUTOCONF;
	if ((bul->bul_flags & IP6_MH_BU_HOME) &&
	    ((bul->bul_reg_fsm_state == MIP6_BUL_REG_FSM_STATE_WAITAR) ||
		(bul->bul_reg_fsm_state == MIP6_BUL_REG_FSM_STATE_BOUND))) {
		flags |= IN6_IFF_DEREGISTERING;
	}
	err = set_ip6addr(ifname, &hoainfo->hinfo_hoa, 64, flags);
}
#else
	err = set_ip6addr(ifname, &hoainfo->hinfo_hoa, 64,
	    IN6_IFF_NODAD|IN6_IFF_HOME|IN6_IFF_DEREGISTERING);
#endif
	if (err) {
		syslog(LOG_ERR,
		    "assigning a home address (%s) to %s failed.\n",
		    ip6_sprintf(&hoainfo->hinfo_hoa), ifname);
		return (err);
	}

	/* set HOME as mn's location */
	hoainfo->hinfo_location = MNINFO_MN_HOME;

	if (LIST_EMPTY(&hoainfo->hinfo_bul_head))
		return (ENOENT);

	for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); bul;
		bul = bul_next) {
		bul_next = LIST_NEXT(bul, bul_entry);

		bul->bul_coa = *coa;
		bul->bul_lifetime = 0;
		bul->bul_home_ifindex = ifindex;
		/* send de-registration */
		syslog(LOG_INFO, 
		       "change fsm MIP6_BUL_FSM_EVENT_RETURNING_HOME to %s\n",
		       ifname);

		if (bul_kick_fsm(bul,  MIP6_BUL_FSM_EVENT_RETURNING_HOME, NULL) == -1) {
			syslog(LOG_ERR, 
			       "fsm processing of movement detection failed.\n");
		}
	}

	return (0);

}

/* DE-REGISTRATION from FL  */
#ifndef MIP_MCOA
static int
mipsock_md_dereg_bul_fl(hoa, oldcoa, newcoa, ifindex)
	struct in6_addr *hoa, *oldcoa, *newcoa;
	u_int16_t ifindex;
#else
static int
mipsock_md_dereg_bul_fl(hoa, oldcoa, newcoa, ifindex, bid)
	struct in6_addr *hoa, *oldcoa, *newcoa;
	u_int16_t ifindex, bid;
#endif /* !MIP_MCOA */
{
	struct mip6_hoainfo *hoainfo;
	struct binding_update_list *bul, *bul_next;
#ifdef MIP_MCOA
	struct binding_update_list *mbul = NULL;
#endif /* MIP_MCOA */
	char ifname[IFNAMSIZ];


	hoainfo = hoainfo_find_withhoa(hoa);
	if (hoainfo == NULL)
		return (ENETDOWN);

	if (if_indextoname(ifindex, ifname) == NULL) 
		return (EINVAL);

	for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); bul;
		bul = bul_next) {
		bul_next = LIST_NEXT(bul, bul_entry);

#ifdef MIP_MCOA
		/* update bul that matched with the bid */
		if (bid && !LIST_EMPTY(&bul->bul_mcoa_head)) {
			for (mbul = LIST_FIRST(&bul->bul_mcoa_head); mbul;
			     mbul = LIST_NEXT(mbul, bul_entry)) {
				
				if (IN6_ARE_ADDR_EQUAL(&mbul->bul_coa, oldcoa) &&
				    mbul->bul_bid == bid) 
					break;
			}
		}

		/* send de-registration */
		if (bid) {
			if (mbul) {
				mbul->bul_coa = *newcoa;
				mbul->bul_lifetime = 0;
				mbul->bul_home_ifindex = ifindex;
				syslog(LOG_INFO, 
				       "change fsm MIP6_BUL_FSM_EVENT_RETURNING_HOME to %s%d \n",
				       ifname, bid);

				if (bul_kick_fsm(mbul, 
						 MIP6_BUL_FSM_EVENT_RETURNING_HOME, NULL) == -1) {
					syslog(LOG_ERR, 
					       "fsm processing of movement detection failed.\n");
				}
			}
			continue;
		} else {  
#endif /* MIP_MCOA */
		if (!IN6_ARE_ADDR_EQUAL(&bul->bul_coa, oldcoa))
			continue;
#ifdef MIP_MCOA
		}
#endif /* MIP_MCOA */

		bul->bul_coa = *newcoa;
		bul->bul_lifetime = 0;
		bul->bul_home_ifindex = ifindex;

		/* send de-registration */
		syslog(LOG_INFO, 
		       "change fsm MIP6_BUL_FSM_EVENT_RETURNING_HOME to %s\n",
		       ifname);

		if (bul_kick_fsm(bul,  MIP6_BUL_FSM_EVENT_RETURNING_HOME, NULL) == -1) {
			syslog(LOG_ERR, 
			       "fsm processing of movement detection failed.\n");
		}
	}

	return (0);
}



/* re-registration. */
int 
#ifndef MIP_MCOA
bul_update_by_mipsock_w_hoa(hoa, coa)
	struct in6_addr *hoa, *coa;
#else
bul_update_by_mipsock_w_hoa(hoa, coa, bid)
	struct in6_addr *hoa, *coa;
	u_int16_t bid;
#endif /* MIP_MCOA */
{
	struct ifaddrs *ifa, *ifap;
	struct sockaddr *sa;
	char mipname[IFNAMSIZ];
	struct mip6_hoainfo *hoainfo;
	struct binding_update_list *bul;
#ifdef MIP_MCOA
	struct binding_update_list *mbul;
#endif /* MIP_MCOA */

	hoainfo = hoainfo_find_withhoa(hoa);
	if (hoainfo == NULL)
		return (ENETDOWN);

	if (LIST_EMPTY(&hoainfo->hinfo_bul_head))
		return (ENOENT);

	if (getifaddrs(&ifap) != 0) {
		syslog(LOG_ERR, "%s\n", strerror(errno));
		return (-1);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		sa = ifa->ifa_addr;
		
		if (sa->sa_family != AF_INET6)
			continue;

		if (IN6_ARE_ADDR_EQUAL(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr,
				       hoa)) {

			if (if_nametoindex(ifa->ifa_name) != hoainfo->hinfo_ifindex) {

				/* move a home address to a virtual i/f. */
				if (delete_ip6addr(ifa->ifa_name, hoa, 64 /* XXX */)) {
					syslog(LOG_ERR,
					    "removing a home address "
					    "from a physical i/f failed.\n");
					freeifaddrs(ifap);
					return (-1);
				}

				if (set_ip6addr(if_indextoname(hoainfo->hinfo_ifindex, 
					mipname), hoa, 64 /* XXX */,
					IN6_IFF_NODAD|IN6_IFF_HOME)) {

					syslog(LOG_ERR,
					    "adding a home address "
					    "to a mip virtual i/f failed.\n");
					/* XXX recover the old phy addr. */
					freeifaddrs(ifap);
					return (-1);
				}

				/* set FOREIGN as a mobile node's location. */
				hoainfo->hinfo_location = MNINFO_MN_FOREIGN;
			}
		}
	}
	freeifaddrs(ifap);

#ifdef MIP_MCOA
        /* for bootstrap */
	if (bid) {
		bul = bul_get_homeflag(&hoainfo->hinfo_hoa);
		if (bul) {
			mbul = bul_mcoa_get(&hoainfo->hinfo_hoa, &bul->bul_peeraddr, bid);
			if (mbul == NULL) {
				mbul = bul_insert(hoainfo, &bul->bul_peeraddr, 
					   coa, bul->bul_flags, bid);

				mbul->bul_lifetime
					= set_default_bu_lifetime(bul->bul_hoainfo);
				mbul->bul_reg_fsm_state = bul->bul_reg_fsm_state;
			} 
		} else 
			syslog(LOG_INFO," bul unknown with %d\n", bid);
	};
#endif /* MIP_MCOA */

	/* update bul */
	for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); bul;
		bul = LIST_NEXT(bul, bul_entry)) {
#ifdef MIP_MCOA
		/* update bul that matched with the bid */
		if (bid && !LIST_EMPTY(&bul->bul_mcoa_head)) {
			for (mbul = LIST_FIRST(&bul->bul_mcoa_head); mbul;
			     mbul = LIST_NEXT(mbul, bul_entry)) {
			
				if (mbul->bul_bid == bid) {
					/* update CoA */
					memcpy(&mbul->bul_coa, coa, sizeof(*coa));
					
					syslog(LOG_INFO, "change fsm MIP6_BUL_FSM_EVENT_MOVEMENT\n");
					if (bul_kick_fsm(mbul, MIP6_BUL_FSM_EVENT_MOVEMENT,
							 NULL) == -1) {
						syslog(LOG_ERR, 
						       "fsm processing of movement detection "
						       "failed.\n");
					}
				} 
			}
			continue;
		}
#endif /* MIP_MCOA */

		/* update CoA */
		memcpy(&bul->bul_coa, coa, sizeof(*coa));

		syslog(LOG_INFO, "change fsm MIP6_BUL_FSM_EVENT_MOVEMENT\n");
		if (bul_kick_fsm(bul, MIP6_BUL_FSM_EVENT_MOVEMENT,
			NULL) == -1) {
			syslog(LOG_ERR, 
			    "fsm processing of movement detection "
			    "failed.\n");
		}
	}

	return (0);
}

static int
mipsock_recv_rr_hint(miphdr)
	struct mip_msghdr *miphdr;
{
	struct mipm_rr_hint *rr_hint;
	struct sockaddr_in6 *sin6;
	struct fsm_message fsmmsg;
	struct binding_update_list *bulhome = NULL, *bul;
	struct mip6_hoainfo *hoainfo = NULL;
	int error = -1;

	rr_hint = (struct mipm_rr_hint *)miphdr;

	bzero(&fsmmsg, sizeof(struct fsm_message));

	if (MIPMRH_HOA(rr_hint)->sa_family != AF_INET6)
		return (-1);
	sin6 = (struct sockaddr_in6 *)MIPMRH_HOA(rr_hint);
	fsmmsg.fsmm_dst = &sin6->sin6_addr;

	if (MIPMRH_PEERADDR(rr_hint)->sa_family != AF_INET6)
		return (-1);
	sin6 = (struct sockaddr_in6 *)MIPMRH_PEERADDR(rr_hint);
	fsmmsg.fsmm_src = &sin6->sin6_addr;

	/* if the destination address is listed in NoRO list, just ignore */
	if (noro_get(&sin6->sin6_addr)) {
		
		syslog(LOG_INFO, 
		       "MN cannot start RO for %s\n", ip6_sprintf(&sin6->sin6_addr));
		return (0);		
	}

	bul = bul_get(fsmmsg.fsmm_dst, fsmmsg.fsmm_src);
	if (bul == NULL) {
		hoainfo = hoainfo_find_withhoa(fsmmsg.fsmm_dst);
		if (hoainfo == NULL)
			return (-1);
		bulhome = bul_get_homeflag(&hoainfo->hinfo_hoa);
		if (bulhome == NULL)
			return (-1);

#ifndef MIP_MCOA
		bul = bul_insert(hoainfo, fsmmsg.fsmm_src, 
				 &bulhome->bul_coa, 0);
#else
		bul = bul_insert(hoainfo, fsmmsg.fsmm_src, 
				 &bulhome->bul_coa, 0, 0);
#endif /* MIP_MCOA */
		if (bul == NULL)
			return (-1);
		bul->bul_lifetime = bulhome->bul_lifetime; /* XXX */
	}

#ifndef MIP_MCOA
	error = bul_kick_fsm(bul, MIP6_BUL_FSM_EVENT_REVERSE_PACKET, &fsmmsg);
	if (error == -1) {
		syslog(LOG_ERR, "fsm processing failed.\n");
	}
#else
	{
		struct binding_update_list *mbul, *mbuln, *newbul;

		bulhome = bul_get_homeflag(&hoainfo->hinfo_hoa);
		if (bulhome == NULL)
			return (-1);
		
		if (LIST_EMPTY(&bulhome->bul_mcoa_head)) {
			error = bul_kick_fsm(bul, 
			     MIP6_BUL_FSM_EVENT_REVERSE_PACKET, &fsmmsg);
			if (error == -1) {
				syslog(LOG_ERR, "fsm processing failed.\n");
			}
		}

		for (mbul = LIST_FIRST(&bulhome->bul_mcoa_head); mbul;
		     mbul = mbuln) {
			mbuln = LIST_NEXT(mbul, bul_entry);
			
			newbul = bul_insert(hoainfo, fsmmsg.fsmm_src, 
				    &mbul->bul_coa, 0, mbul->bul_bid);
			if (newbul == NULL)
				continue;

			error = bul_kick_fsm(newbul, 
				     MIP6_BUL_FSM_EVENT_REVERSE_PACKET, &fsmmsg);
			if (error == -1) {
				syslog(LOG_ERR, "fsm processing failed.\n");
			}
		}
	}
#endif /* MIP_MCOA */

	return (error);
}

int
mipsock_input(miphdr)
	struct mip_msghdr *miphdr;
{
	int err = 0;

	switch (miphdr->miph_type) {
	case MIPM_BC_ADD:
        case MIPM_BC_CHANGE:
        case MIPM_BC_REMOVE:
        case MIPM_BUL_ADD:
        case MIPM_BUL_CHANGE:
        case MIPM_BUL_REMOVE:
	case MIPM_NODETYPE_INFO:
	case MIPM_BUL_FLUSH:
	case MIPM_HOME_HINT: /* ignore, it's for MD deamon*/
		break;
	case MIPM_MD_INFO:
		/* event trigger: update bul entries */
		err = mipsock_recv_mdinfo(miphdr);
		break;
	case MIPM_RR_HINT:
		err = mipsock_recv_rr_hint(miphdr);
		break;
	default:
		break;
	}
	return (err);
}


int
send_haadreq(hoainfo, hoa_plen, src)
	struct mip6_hoainfo *hoainfo;
	int hoa_plen;
	struct in6_addr *src;
{
        struct msghdr msg;
        struct iovec iov;
        struct cmsghdr  *cmsgptr = NULL;
        struct in6_pktinfo *pi = NULL;
        struct sockaddr_in6 to;
        char adata[512], buf[1024];
	struct mip6_dhaad_req dhreq;
#if defined(MIP_MN) && defined(MIP_NEMO)
	struct sockaddr_in6 *ar_sin6, ar_sin6_orig;
#endif


        memset(&to, 0, sizeof(to));
        if (mip6_icmp6_create_haanyaddr(&to.sin6_addr, 
				&hoainfo->hinfo_hoa, hoa_plen)) 
                return (EINVAL);

	to.sin6_family = AF_INET6;
	to.sin6_port = 0;
	to.sin6_scope_id = 0;
	to.sin6_len = sizeof (struct sockaddr_in6);

        msg.msg_name = (void *)&to;
        msg.msg_namelen = sizeof(struct sockaddr_in6);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = (void *) adata;
        msg.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));
#if defined(MIP_MN) && defined(MIP_NEMO)
	msg.msg_controllen += 
		CMSG_SPACE(sizeof(struct sockaddr_in6));
#endif /*MIP_NEMO */


	/* Packet Information i.e. Source Address */
	cmsgptr = CMSG_FIRSTHDR(&msg);
	pi = (struct in6_pktinfo *)(CMSG_DATA(cmsgptr));
	memset(pi, 0, sizeof(*pi));
        pi->ipi6_addr = *src;
	cmsgptr->cmsg_level = IPPROTO_IPV6;
	cmsgptr->cmsg_type = IPV6_PKTINFO;
	cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	cmsgptr = CMSG_NXTHDR(&msg, cmsgptr);
	
#if defined(MIP_MN) && defined(MIP_NEMO)
	ar_sin6 = nemo_ar_get(src, &ar_sin6_orig);
	if (ar_sin6 == NULL) {
		syslog(LOG_ERR, "can't send message due to no AR\n");
		/* XXX send RS */
		return (-1);
	} else {
		if (debug)
			syslog(LOG_INFO, "send ICMP msg via %s/%d\n", 
				ip6_sprintf(&ar_sin6->sin6_addr), ar_sin6->sin6_scope_id);
	}
	cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct sockaddr_in6));
	cmsgptr->cmsg_level = IPPROTO_IPV6;
	cmsgptr->cmsg_type = IPV6_NEXTHOP;
	memcpy(CMSG_DATA(cmsgptr), ar_sin6, sizeof(struct sockaddr_in6));
	cmsgptr = CMSG_NXTHDR(&msg, cmsgptr);
#endif

	bzero(buf, sizeof(buf));
	iov.iov_base = (char *)&dhreq;
	iov.iov_len = sizeof(dhreq);
	
	dhreq.mip6_dhreq_type = MIP6_HA_DISCOVERY_REQUEST;
	dhreq.mip6_dhreq_code = 0;
	dhreq.mip6_dhreq_cksum = 0;
	dhreq.mip6_dhreq_id = htons(++hoainfo->hinfo_dhaad_id);
#ifndef MIP_NEMO
	dhreq.mip6_dhreq_reserved = 0;
#else
	dhreq.mip6_dhreq_reserved = MIP6_DHREQ_FLAG_MR;
#endif /* MIP_NEMO */
	
	if (sendmsg(icmp6sock, &msg, 0) < 0)
		perror ("sendmsg icmp6 @ haddreq");

	syslog(LOG_INFO, "send DHAAD REQUEST\n");

	return (errno);
}

struct home_agent_list *
mnd_add_hal(hpfx_entry, gladdr, flag)
	struct  mip6_hpfxl *hpfx_entry;
	struct in6_addr *gladdr;
	int flag;
{
	struct home_agent_list *hal = NULL, *haln = NULL, *halnew;

	hal = mip6_get_hal(hpfx_entry, gladdr);
	if (hal && (hal->hal_flag != flag)) { 
		hal->hal_flag = flag;
		return (hal);
	} 

	halnew = NULL;
	halnew = malloc(sizeof(*halnew));
	memset(halnew, 0, sizeof(*halnew));

	halnew->hal_ip6addr = *gladdr;
	halnew->hal_flag = flag;

	if (LIST_EMPTY(&hpfx_entry->hpfx_hal_head)) 
		LIST_INSERT_HEAD(&hpfx_entry->hpfx_hal_head, halnew, hal_entry);
	else {
		for (hal = LIST_FIRST(&hpfx_entry->hpfx_hal_head); hal; hal = haln) {
			haln =  LIST_NEXT(hal, hal_entry);
			if (haln == NULL) {
				LIST_INSERT_AFTER(hal, halnew, hal_entry);
				break;
			}
		}
	}

	if (debug)
		syslog(LOG_INFO, "Home Agent (%s) added into home agent list\n", 
		       ip6_sprintf(gladdr));
		
	return (hal);
}

static int
add_hal_by_commanline_xxx(homeagent)
	char *homeagent;
{
	struct in6_addr homeagent_in6;
	struct mip6_mipif *mif;
	struct mip6_hpfxl *hpfx;

	if (inet_pton(AF_INET6, homeagent, &homeagent_in6) != 1) {
		syslog(LOG_ERR,
		    "the specified home agent addrss (%s) is invalid.\n",
		    homeagent);
		return(-1);
	}

	LIST_FOREACH(mif, &mipifhead, mipif_entry) {
		LIST_FOREACH(hpfx, &mif->mipif_hprefx_head, hpfx_entry) {
			if (mip6_are_prefix_equal(&hpfx->hpfx_prefix,
				&homeagent_in6, hpfx->hpfx_prefixlen)) {
				/* XXXX can we add the same addr to
				   multiple prefixes? */
				mnd_add_hal(hpfx, &homeagent_in6, 0);
			}
		}
	}

	return (0);
}

struct mip6_hpfxl *
mnd_add_hpfxlist(home_prefix, home_prefixlen, hpfx_mnoption, hpfxhead) 
	struct in6_addr *home_prefix;
	u_int16_t home_prefixlen;
	struct mip6_hpfx_mn_exclusive *hpfx_mnoption;
	struct mip6_hpfx_list *hpfxhead;
{
	struct mip6_hpfxl *hpfx = NULL;

	hpfx = mip6_get_hpfxlist(home_prefix, home_prefixlen, hpfxhead);
	if (hpfx) {
		if (hpfx_mnoption)
			memcpy(&hpfx->hpfx_for_mn, 
				hpfx_mnoption, sizeof(hpfx->hpfx_for_mn));
		/* need timer XXX */
		return (hpfx);
	}

	hpfx = malloc(sizeof(*hpfx));
	memset(hpfx, 0, sizeof(*hpfx));

	hpfx->hpfx_prefix = *home_prefix;
	hpfx->hpfx_prefixlen = home_prefixlen;
	if (hpfx_mnoption)
		memcpy(&hpfx->hpfx_for_mn, 
			hpfx_mnoption, sizeof(hpfx->hpfx_for_mn));

	/* need timer XXX */

	LIST_INIT(&hpfx->hpfx_hal_head);

	if (debug)
		syslog(LOG_INFO, "Home Prefix (%s/%d) added into home prefix list\n", 
		       ip6_sprintf(home_prefix), home_prefixlen);
	
	LIST_INSERT_HEAD(hpfxhead, hpfx, hpfx_entry);

	return (hpfx);
}

static struct mip6_mipif *
mnd_add_mipif(ifname)
	char *ifname;
{
	struct mip6_mipif *mif = NULL;
	u_int16_t ifindex;

	ifindex = if_nametoindex(ifname);
	if (ifindex == 0) {
		syslog(LOG_ERR, "%s %s\n", ifname, strerror(errno));
		return (NULL);
	}
	
	mif = mnd_get_mipif(ifindex);
	if (mif)
		return (mif);
	
	mif = malloc(sizeof(*mif));
	memset(mif, 0, sizeof(*mif));
	mif->mipif_ifindex = ifindex;

	/* initialize home prefix head */
	LIST_INIT(&mif->mipif_hprefx_head);

	/* add all global prefixes assigned to this ifindex */
	mnd_init_homeprefix(mif->mipif_ifindex, &mif->mipif_hprefx_head);

	LIST_INSERT_HEAD(&mipifhead, mif, mipif_entry);

	if (debug)
		syslog(LOG_ERR, "%s is added successfully\n", ifname);

	
	return (mif);
}

void
mnd_delete_mipif(u_int16_t ifindex)
{
	/* never delete mipif */
	return; 
}


struct mip6_mipif *
mnd_get_mipif(ifindex)
	u_int16_t ifindex;
{
	struct mip6_mipif *mif;

	LIST_FOREACH(mif, &mipifhead, mipif_entry) {
		if (mif->mipif_ifindex == ifindex) 
			return (mif);
	}

	return (NULL);
}


static void
mnd_init_homeprefix(ifindex, hpfx_head)
	u_int16_t ifindex;
	struct mip6_hpfx_list *hpfx_head;
{
	struct ifaddrs *ifa, *ifap;
	struct sockaddr *sa;
	struct sockaddr_in6 *addr_sin6, *mask_sin6;
	int prefixlen = 0;
	struct mip6_hpfxl *hpfxent = NULL;
	struct mip6_hoainfo *hoa = NULL;
#ifdef MIP_NEMO
	struct nemo_mptable *mpt = NULL;
#endif /* MIP_NEMO */

	if (getifaddrs(&ifap) != 0) {
		syslog(LOG_ERR, "%s\n", strerror(errno));
		return;
	}
	
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		sa = ifa->ifa_addr;
		
		if (sa->sa_family != AF_INET6)
			continue;
		if (if_nametoindex(ifa->ifa_name) != ifindex) 
			continue;

		if (!(ifa->ifa_flags & IFF_UP)) 
			continue;

		/* home prefix must be global scope */
		addr_sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (IN6_IS_ADDR_LINKLOCAL(&addr_sin6->sin6_addr))
			continue;

		/* set Home Address to mip6_hoainfo */
		hoa = hoainfo_insert(&addr_sin6->sin6_addr, ifindex);
		if (hoa == NULL)
			continue;

		mask_sin6 = (struct sockaddr_in6 *)ifa->ifa_netmask;
		prefixlen = in6_mask2len(&mask_sin6->sin6_addr, NULL);

		hpfxent = mip6_get_hpfxlist(&addr_sin6->sin6_addr, prefixlen, hpfx_head);
		if (hpfxent)
			continue;

		hpfxent = mnd_add_hpfxlist(&addr_sin6->sin6_addr, 
			prefixlen, NULL, hpfx_head);
		if (hpfxent == NULL) {
			syslog(LOG_ERR, "fail to add home prefix entry %s\n", 
			       ip6_sprintf(&addr_sin6->sin6_addr));
			continue;
		}
		
#ifdef MIP_NEMO
		LIST_FOREACH(mpt, &hoa->hinfo_mpt_head, mpt_entry) {
			if (mpt->mpt_ha.s6_addr == 0)
				continue;
			if (mnd_add_hal(hpfxent, &mpt->mpt_ha, 0) == NULL) {
				syslog(LOG_ERR, "fail to add home agent entry %s\n",
				       ip6_sprintf(&mpt->mpt_ha));
			}
		}
#endif /* MIP_NEMO */
	}
	
	freeifaddrs(ifap);
	
	if (LIST_EMPTY(hpfx_head)) {
		syslog(LOG_ERR, "please configure at least one global home prefix\n");
		exit(0);
	}
	return;
}


int
set_default_bu_lifetime(hoainfo) 
	struct mip6_hoainfo *hoainfo;
{
	return (default_lifetime);
}

static void
terminate(dummy)
	int dummy;
{
	struct mip_msghdr mipmsg;

	/* stop acting as a mobile node. */
#ifndef MIP_NEMO
	mipsock_nodetype_request(MIP6_NODETYPE_MOBILE_NODE, 0);
#else
	mipsock_nodetype_request(MIP6_NODETYPE_MOBILE_ROUTER, 0);
#endif

	/* flush all bul registered in a kernel. */
	memset(&mipmsg, 0, sizeof(struct mip_msghdr));
	mipmsg.miph_msglen = sizeof(struct mip_msghdr);
	mipmsg.miph_type = MIPM_BUL_FLUSH;
	if (write(mipsock, &mipmsg, sizeof(struct mip_msghdr)) == -1) {
		syslog(LOG_ERR,
		    "removing all bul entries failed.\n");
	}

	close(csock);	
	close(icmp6sock);
	close(mipsock);
	close(mhsock);

	noro_sync();

	exit(-1);
}

/* 
 * Entry for hosts not supporting Route Optimization.  This entry is
 * also used to tell shisad not to run RO for specified host in file.  
 */
static void
noro_init()
{ 
        FILE *file;
	char buf[256], *bl;
	struct in6_addr noro_addr;
	struct noro_host_list *noro;
	
	file = fopen(MND_NORO_FILE, "r");
        if(file == NULL) 
                return;

        memset(buf, 0, sizeof(buf));
        while((fgets(buf, sizeof(buf), file)) != NULL){
		/* ignore comments */
		if (strchr(buf, '#') != NULL) 
			continue;

		bl = strchr(buf, '\n');
		if (bl) 
			*bl = '\0';

                if (inet_pton(AF_INET6, buf, &noro_addr) < 0) {
                        fprintf(stderr, "%s is not correct address\n", buf);
                        continue;
		}

		if (noro_get(&noro_addr)) {
			syslog(LOG_ERR, "%s is duplicated in %s\n", buf, MND_NORO_FILE);
			continue;
		}

		noro = malloc(sizeof(struct noro_host_list));
		if (noro == NULL) {
			perror("malloc");
			return;
		}
		memset(noro, 0, sizeof(struct noro_host_list));
		noro->noro_host = noro_addr;
		LIST_INSERT_HEAD(&noro_head, noro, noro_entry); 
	}
	fclose(file);

	return;
};

void
noro_add(tgt)
	struct in6_addr *tgt;
{ 
	struct noro_host_list *noro = NULL;

	noro = noro_get(tgt);
	if (noro)
		return;

	noro = malloc(sizeof(struct noro_host_list));
	if (noro == NULL) {
		perror("malloc");
		return;
	}
	memset(noro, 0, sizeof(struct noro_host_list));
	noro->noro_host = *tgt;
	LIST_INSERT_HEAD(&noro_head, noro, noro_entry); 
	
	return;
};


struct noro_host_list *
noro_get(tgt)
	struct in6_addr *tgt;
{ 
	struct noro_host_list *noro = NULL;

        for (noro = LIST_FIRST(&noro_head); noro; 
	     noro = LIST_NEXT(noro, noro_entry)) {
		
		if (IN6_ARE_ADDR_EQUAL(tgt, &noro->noro_host)) 
			return (noro);
	}

	return (NULL);
};


static void
noro_show(s)
	int s;
{ 
	char buff[2048];
	struct noro_host_list *noro = NULL;

        for (noro = LIST_FIRST(&noro_head); noro; 
	     noro = LIST_NEXT(noro, noro_entry)) {
		
		sprintf(buff, "%s\n", ip6_sprintf(&noro->noro_host));
		write(s, buff, strlen(buff));
	}
};

static void
noro_sync()
{ 
        FILE *file;
	struct noro_host_list *noro = NULL;
	
	file = fopen(MND_NORO_FILE, "r");
        if(file == NULL) 
                return;

        for (noro = LIST_FIRST(&noro_head); noro; 
	     noro = LIST_NEXT(noro, noro_entry)) {
		fputs(ip6_sprintf(&noro->noro_host), file);
	}

	fclose(file);
};


static void
command_show_hal(s)
	int s;
{
	char buff[2048];
        struct home_agent_list *hal = NULL, *haln = NULL;
        struct mip6_hpfxl *hpfx;
	struct mip6_mipif *mipif = NULL;

        LIST_FOREACH(mipif, &mipifhead, mipif_entry) {
		LIST_FOREACH(hpfx, &mipif->mipif_hprefx_head, hpfx_entry) {
			for (hal = LIST_FIRST(&hpfx->hpfx_hal_head); hal; hal = haln) {
				haln =  LIST_NEXT(hal, hal_entry);
				
				sprintf(buff, "%s ", ip6_sprintf(&hal->hal_ip6addr));
				sprintf(buff + strlen(buff), "%s\n", 
					ip6_sprintf(&hal->hal_lladdr));
#ifdef MIP_HA
				sprintf(buff + strlen(buff), 
					"     lif=%d pref=%d flag=%s%s\n",
					hal->hal_lifetime, hal->hal_preference, 
					(hal->hal_flag & MIP6_HA_OWN)  ? "mine" : ""
					(hal->hal_flag & MIP6_HA_STATIC)  ? "static" : "");
#endif /* MIP_HA */
				write(s, buff, strlen(buff));
				
			}
		}
	}

	return;
}


static void
command_show_status(s, arg)
	int s;
	char *arg;
{

	char msg[1024];

	if (strcmp(arg, "bul") == 0) {
		sprintf(msg, "-- Binding Update List (Shisa) --\n");
		write(s, msg, strlen(msg));

		command_show_bul(s);

	} else if (strcmp(arg, "kbul") == 0) {
		sprintf(msg, "-- Binding Update List (kernel) --\n");
		write(s, msg, strlen(msg));

		command_show_kbul(s);

	} else if (strcmp(arg, "hal") == 0) {
		sprintf(msg, "-- Home Agent List --\n");
		write(s, msg, strlen(msg));

		command_show_hal(s);

	} else if (strcmp(arg, "bc") == 0) {
		sprintf(msg, "Wrong port!\nPlease find Binding Cache at cnd\n");
		write(s, msg, strlen(msg));

 	} else if (strcmp(arg, "stat") == 0) {
		sprintf(msg, "-- Shisa Statistics --\n");
		write(s, msg, strlen(msg));

		command_show_stat(s);

	} else if (strcmp(arg, "noro") == 0) {
		sprintf(msg, "-- No Route Optimization --\n");
		write(s, msg, strlen(msg));

		noro_show(s);
	}
#ifdef MIP_NEMO
	else if (strcmp(arg, "pt") == 0) {
		sprintf(msg, "-- Prefix Table --\n");
		write(s, msg, strlen(msg));
		
		command_show_pt(s);
	}
#endif /* MIP_NEMO */
	else {
		sprintf(msg, "Available options are:\n");
		sprintf(msg + strlen(msg), "\tbul (Binding Update List in Shisa)\n\tkbul (Binding Update List in kernel)\n\thal (Home Agent List)\n\tstat (Statistics)\n\tpt (Prefix Table, MR only)\n\n");
		write(s, msg, strlen(msg));
	}

	return;
}

/* Flush BC should be done by cnd */
static void
command_flush(s, arg)
	int s;
	char *arg;
{
	char msg[1024];

	if (strcmp(arg, "bul") == 0) {
		/*flush_bc();*/
		sprintf(msg, "-- Clear Binding Update List --\n");
		write(s, msg, strlen(msg));

	} else if (strcmp(arg, "stat") == 0) {
		sprintf(msg, "-- Clear Shisa Statistics --\n");
		write(s, msg, strlen(msg));

	} else if (strcmp(arg, "hal") == 0) {
		sprintf(msg, "-- Clear Home Agent List --\n");
		write(s, msg, strlen(msg));

	} else if (strcmp(arg, "noro") == 0) {
		sprintf(msg, "-- Clear No Route Optimization Host --\n");
		write(s, msg, strlen(msg));

	} else {
		sprintf(msg, "Available options are:\n");
		sprintf(msg + strlen(msg), "\tbul (Binding Update List)\n\thal (Home Agent List)\n\tstat (Statistics)\n\tnoro (No Route Optimization Hosts)\n\n");
		write(s, msg, strlen(msg));

	}
}
