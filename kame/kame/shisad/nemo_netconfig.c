/*      $Id: nemo_netconfig.c,v 1.1 2004/09/27 04:06:04 t-momose Exp $  */
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
#include <syslog.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/mipsock.h>
#include <net/if_dl.h>
#include <netinet/ip6mh.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>

#define MODE_HA 0x01
#define MODE_MR 0x02

#include "callout.h"
#include "shisad.h"

/* Variables */
struct nemo_if {
	LIST_ENTRY(nemo_if) nemo_ifentry;
	char ifname[IFNAMSIZ];
	struct in6_addr hoa;
#ifdef MIP_MCOA
	u_int16_t bid;
#endif /* MIP_MCOA */
};
LIST_HEAD(nemo_if_head, nemo_if) nemo_ifhead;

/* Those info is retrieved from prefix_table */
struct nemo_mnpt {
	LIST_ENTRY(nemo_mnpt) nemo_mnptentry;
	struct in6_addr hoa;
	struct in6_addr nemo_prefix;
	int nemo_prefixlen;
#ifdef MIP_MCOA
	u_int16_t bid;
#endif /* MIP_MCOA */
	struct nemo_if *nemo_if;
};
LIST_HEAD(nemo_mnpt_head, nemo_mnpt) nemo_mnpthead;

int mode;
int debug = 0;
int numerichost = 0;

/* Functions */
static int set_nemo_ifinfo();
static void mainloop();
static void terminate(int);
static int ha_parse_ptconf(char *);
static int mr_parse_ptconf(char *);
#ifndef MIP_MCOA 
static struct nemo_if *nemo_setup_forwarding (struct sockaddr *, struct sockaddr *, 
					      struct in6_addr *);
static struct nemo_if *nemo_destroy_forwarding(struct in6_addr *);
#else
static struct nemo_if *nemo_setup_forwarding (struct sockaddr *, struct sockaddr *, 
					      struct in6_addr *, u_int16_t);
static struct nemo_if *nemo_destroy_forwarding(struct in6_addr *, u_int16_t);
#endif /* MIP_MCOA */

void
usage() {
	fprintf(stderr, "netd -d -h -m -f prefixtable\n");
	fprintf(stderr, "\t-h Home Agent\n");
	fprintf(stderr, "\t-m Mobile Router\n");

	exit(0);
}

int
main (int argc, char **argv) {
	char *pt_filename = NULL;
	int ch = 0;
	int if_number = 0, pt_number = 0;
	struct nemo_if *nif;
	struct nemo_mnpt *npt;
	
	LIST_INIT(&nemo_mnpthead);
	LIST_INIT(&nemo_ifhead);

	mode = 0;
	while ((ch = getopt(argc, argv, "dnhmf:")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'n':
			numerichost = 1;
			break;
		case 'h':
			mode = MODE_HA;
			break;
		case 'm':
			mode = MODE_MR;
			break;
		case 'f':
			pt_filename = optarg;
			break;
		default:
			fprintf(stderr, "unknown option\n");
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (mode == 0)
		usage();

	/* open syslog */
	openlog("shisad(net)", 0, LOG_DAEMON);
	syslog(LOG_INFO, "start nemo network daemon\n");

	// parse prefix table
	if (mode == MODE_HA) {
		ha_parse_ptconf((pt_filename)?pt_filename:NEMO_PTFILE);
	} else if (mode == MODE_MR) {
		mr_parse_ptconf((pt_filename)?pt_filename:NEMO_PTFILE);
	} else /* never reach here */
		usage ();

	//get nemotun from the kernel and flush all states
	if (set_nemo_ifinfo()) {
		perror("set_nemo_ifinfo");
		return -1;
	}

	LIST_FOREACH(nif, &nemo_ifhead, nemo_ifentry) {
		if_number ++;
	}
	LIST_FOREACH(npt, &nemo_mnpthead, nemo_mnptentry) {
		pt_number ++;
	}

	if (if_number < pt_number) {
		syslog(LOG_ERR, "Please create %d of nemo interfaces\n", pt_number);	
		exit(0);
	}

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

	return 0;
};

static int
ha_parse_ptconf(char *filename) {
        FILE *file;
        int i=0;
        char buf[256], *spacer, *head;
	struct nemo_mnpt *pt;
        char *option[NEMO_OPTNUM];
        /*
         * option[0]: HoA 
         * option[1]: Mobile Network Prefix
         * option[2]: Mobile Network Prefix Length
         * option[3]: Registration mode
         * option[4]: Binding Unique Identifier (optional)
         */

	if (filename == NULL)
		return EINVAL;
	file = fopen(filename, "r");
        if(file == NULL) {
                perror("fopen");
                return errno;
        }

        memset(buf, 0, sizeof(buf));
        while((fgets(buf, sizeof(buf), file)) != NULL){
                /* ignore comments */
                if (strchr(buf, '#') != NULL) 
                        continue;
                if (strchr(buf, ' ') == NULL) 
                        continue;
                
                /* parsing all options */
                for (i = 0; i < NEMO_OPTNUM; i++)
                        option[i] = '\0';
                head = buf;
                for (i = 0, head = buf; 
                     (head != NULL) && (i < NEMO_OPTNUM); 
                     head = ++spacer, i ++) {

                        spacer = strchr(head, ' ');
                        if (spacer) {
                                *spacer = '\0';
                                option[i] = head;
                        } else {
                                option[i] = head;
                                break;
                        }
                }

                if (debug) {
                        for (i = 0; i < NEMO_OPTNUM; i ++)  
				if (option[i])
                                	syslog(LOG_INFO, "\t%d=%s\n", i, option[i]);
                }

		pt = malloc(sizeof(*pt));
		if (pt == NULL)
			return ENOMEM;
		memset(pt, 0, sizeof(pt));
                if (inet_pton(AF_INET6, option[0], &pt->hoa) < 0) {
                        fprintf(stderr, "%s is not correct address\n", option[0]);
			free(pt);
                        continue;
                }
                if (inet_pton(AF_INET6, option[1], &pt->nemo_prefix) < 0) {
                        fprintf(stderr, "%s is not correct address\n", option[1]);
			free(pt);
                        continue;
                }
		pt->nemo_prefixlen = atoi(option[2]);  

#ifdef MIP_MCOA 
		if (option[4])
			pt->bid = atoi(option[4]);
		else 
			pt->bid = 0;
#endif /* MIP_MCOA */

		LIST_INSERT_HEAD(&nemo_mnpthead, pt, nemo_mnptentry);

                memset(buf, 0, sizeof(buf));
        }

        fclose(file);
        return 0;

};

static int
mr_parse_ptconf(char *filename) {
        FILE *file;
        int i=0;
        char buf[256], *spacer, *head;
	struct nemo_mnpt *pt;

        char *option[NEMO_OPTNUM];
        /*
         * option[0]: HoA 
         * option[1]: Mobile Network Prefix
         * option[2]: Mobile Network Prefix Length
         * option[3]: Registration mode
         * option[4]: Binding Unique Id (optional)
         */

	if (filename == NULL)
		return EINVAL;

        file = fopen(filename, "r");
        if(file == NULL) {
                perror("fopen");
                return errno;
        }

        memset(buf, 0, sizeof(buf));
        while((fgets(buf, sizeof(buf), file)) != NULL){
                /* ignore comments */
                if (strchr(buf, '#') != NULL) 
                        continue;
                if (strchr(buf, ' ') == NULL) 
                        continue;
                
                /* parsing all options */
                for (i = 0; i < NEMO_OPTNUM; i++)
                        option[i] = '\0';
                head = buf;
                for (i = 0, head = buf; 
                     (head != NULL) && (i < NEMO_OPTNUM); 
                     head = ++spacer, i ++) {

                        spacer = strchr(head, ' ');
                        if (spacer) {
                                *spacer = '\0';
                                option[i] = head;
                        } else {
                                option[i] = head;
                                break;
                        }
                }

                if (debug) {
                        for (i = 0; i < NEMO_OPTNUM; i ++)  
				if (option[i])
                                	syslog(LOG_INFO, "\t%d=%s\n", i, option[i]);
                }

		pt = malloc(sizeof(*pt));
		if (pt == NULL)
			return ENOMEM;
		memset(pt, 0, sizeof(*pt));

                if (inet_pton(AF_INET6, option[0], &pt->hoa) < 0) {
                        fprintf(stderr, "%s is not correct address\n", option[0]);
			free(pt);
                        continue;
                }

                if (inet_pton(AF_INET6, option[1], &pt->nemo_prefix) < 0) {
                        fprintf(stderr, "%s is not correct address\n", option[1]);
			free(pt);
                        continue;
                }
                pt->nemo_prefixlen = atoi(option[2]);

#ifdef MIP_MCOA 
		if (option[4])
			pt->bid = atoi(option[4]);
		else 
			pt->bid = 0;
#endif /* MIP_MCOA */

		LIST_INSERT_HEAD(&nemo_mnpthead, pt, nemo_mnptentry);
                memset(buf, 0, sizeof(buf));
        }

        fclose(file);
        return 0;
};


static int
set_nemo_ifinfo() {
        size_t needed;
        char *buf, *next, name[IFNAMSIZ];
        struct if_msghdr *ifm;
        struct sockaddr_dl *sdl;
        int mib[6];
	struct nemo_if *nif;
        
        mib[0] = CTL_NET;
        mib[1] = PF_ROUTE;
        mib[2] = 0;
        mib[3] = AF_INET6;
        mib[4] = NET_RT_IFLIST;
        mib[5] = 0;
        
        if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
                perror("sysctl");
		return errno;
	}
        if ((buf = malloc(needed)) == NULL) {
                perror("malloc");
		return errno;
	}
        if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
                perror("sysctl");
		return errno;
	}

        for (next = buf; next < buf + needed ; next += ifm->ifm_msglen) {
                ifm = (struct if_msghdr *)next;

                if (ifm->ifm_type == RTM_IFINFO) {
                        sdl = (struct sockaddr_dl *)(ifm + 1);

                        bzero(name, sizeof(name));
                        strncpy(name, &sdl->sdl_data[0], sdl->sdl_nlen);

                        if (strncmp(name, "nemo", strlen("nemo")) == 0) {

				nif = malloc(sizeof(struct nemo_if));
				if (nif == NULL)
					return ENOMEM;

				memset(nif, 0, sizeof(*nif));
				strncpy(nif->ifname, name, strlen(name));

				/* clear tunnel configuration */ 
				nemo_tun_del(nif->ifname); 

				LIST_INSERT_HEAD(&nemo_ifhead, nif, nemo_ifentry);
                                continue;
                        }
                }               

        }
        free(buf); 

	if (debug) {
		LIST_FOREACH(nif, &nemo_ifhead, nemo_ifentry) {
			syslog(LOG_INFO, "add %s\n", nif->ifname);
		}
	}

	return 0;
};

static struct nemo_if *
#ifndef MIP_MCOA
find_nemo_if(struct in6_addr *hoa) 
#else
find_nemo_if(struct in6_addr *hoa, u_int16_t bid) 
#endif /* MIP_MCOA */
{
	struct nemo_if *nif;
	short flags;
	
	LIST_FOREACH(nif, &nemo_ifhead, nemo_ifentry) {
#ifndef MIP_MCOA
		if (hoa && IN6_ARE_ADDR_EQUAL(hoa, &nif->hoa)) {  
				return nif;
		} 
#else
		if (hoa && IN6_ARE_ADDR_EQUAL(hoa, &nif->hoa)) {
			if ((bid > 0) && (bid == nif->bid)) 
				return nif;
		}
#endif /* MIP_MCOA */
	}

	LIST_FOREACH(nif, &nemo_ifhead, nemo_ifentry) {
		flags = nemo_ifflag_get(nif->ifname);
		if (!(flags & IFF_UP))
			return nif;
	}

	return NULL;
}

static void	
mainloop() {
	int msock, n, nfds = 0;
        fd_set fds;
        struct mip_msghdr *mhdr = NULL;
	char buf[256];
	struct in6_addr local_in6, def, *hoa;
	struct sockaddr_in6 src, dst;
	struct nemo_mnpt *npt = NULL, *nptn = NULL;
	struct nemo_if *nif;
	struct mipm_bul_info *mbu;
	struct mipm_bc_info *mbc;
#ifdef MIP_MCOA
	u_int16_t bid = 0;
#endif /* MIP_MCOA */

	memset(&def, 0, sizeof(def));
	memset(&local_in6, 0, sizeof(local_in6));
	inet_pton(AF_INET6, "::1", &local_in6);

        msock = socket(PF_MOBILITY, SOCK_RAW, 0);

        if (msock < 0) {
                perror("socket(PF_MOBILITY)");
                exit(-1);
        }

	while (1) {
		FD_ZERO(&fds);
		nfds = -1;
		FD_SET(msock, &fds);
		nfds = msock + 1;

                if (select(nfds, &fds, NULL, NULL, NULL) < 0) {
			perror("select()");
                        exit(-1);
                }

                if (FD_ISSET(msock, &fds)) {
			n = read(msock, buf, sizeof(buf));
			if (n < 0) {
				perror ("read");
				continue;
			}

			mhdr = (struct mip_msghdr *)buf;
                        switch (mhdr->miph_type) {
                        case MIPM_BUL_ADD:
                        case MIPM_BUL_CHANGE:
                                /* tunnel setup and route add for MNPs */

				if (mode != MODE_MR)
					break;

                                mbu = (struct mipm_bul_info *)buf;
				/* if R flag is not set, ignore the BU */
                                if (!(mbu->mipu_flags & (IP6_MH_BU_HOME | IP6_MH_BU_ROUTER))) 
					break;
				memset(&src, 0, sizeof(src));
				memset(&dst, 0, sizeof(dst));
				src.sin6_family = dst.sin6_family = AF_INET6;
				src.sin6_len = dst.sin6_len =
					sizeof(struct sockaddr_in6);
				src.sin6_addr =
					((struct sockaddr_in6 *)MIPU_COA(mbu))->sin6_addr;
				dst.sin6_addr =
					((struct sockaddr_in6 *)MIPU_PEERADDR(mbu))->sin6_addr;
				hoa = &((struct sockaddr_in6 *)MIPU_HOA(mbu))->sin6_addr;
				if (hoa == NULL)
					break;

				if (debug) {
					syslog(LOG_INFO, "BUL_ADD hoa %s\n", 
					       ip6_sprintf(hoa));
					syslog(LOG_INFO, "BUL_ADD dst %s\n", 
					       ip6_sprintf(&dst.sin6_addr));
					syslog(LOG_INFO, "BUL_ADD src %s\n", 
					       ip6_sprintf(&src.sin6_addr));
				}
#ifndef MIP_MCOA
				nif = nemo_setup_forwarding((struct sockaddr *)&src, 
						(struct sockaddr *)&dst, hoa);
#else

				bid = mbu->mipu_bid;
				syslog(LOG_INFO, "received BID is %d\n", bid);
				nif = nemo_setup_forwarding((struct sockaddr *)&src, 
							    (struct sockaddr *)&dst, hoa, bid);
#endif /* MIP_MCOA */
				if (nif == NULL) 
					break;

				/*
				 * setup routes toward nif for all
				 * associated mobile network prefixes 
				 */
				for (npt = LIST_FIRST(&nemo_mnpthead); npt; npt = nptn) {
					nptn = LIST_NEXT(npt, nemo_mnptentry);

					if (!IN6_ARE_ADDR_EQUAL(hoa, &npt->hoa)) 
						continue;

#ifdef MIP_MCOA
					if (bid <= 0) {
#endif /* MIP_MCOA */
					/* remove default route */
					route_del(0);
					/* add default route */
					route_add(&def, &local_in6, NULL, 0,
						if_nametoindex(nif->ifname));

					syslog(LOG_INFO, "route add default to %s\n", nif->ifname);
#ifdef MIP_MCOA
					}
#endif /* MIP_MCOA */
					
					npt->nemo_if = nif;
				}

                                break;
                        case MIPM_BUL_REMOVE:
				if (mode != MODE_MR)
					break;

                                mbu = (struct mipm_bul_info *)buf;

				/* if R flag is not set, ignore the BU */
                                if (!(mbu->mipu_flags & (IP6_MH_BU_HOME | IP6_MH_BU_ROUTER))) 
					break;
				hoa = &((struct sockaddr_in6 *)MIPU_HOA(mbu))->sin6_addr;
				if (hoa == NULL)
					break;

#ifndef MIP_MCOA
				nif = nemo_destroy_forwarding(hoa);
#else
				bid = mbu->mipu_bid;
				syslog(LOG_INFO, "received BID is %d\n", bid);
				nif = nemo_destroy_forwarding(hoa, bid);
#endif /* MIP_MCOA */

				for (npt = LIST_FIRST(&nemo_mnpthead); npt; npt = nptn) {
					nptn = LIST_NEXT(npt, nemo_mnptentry);

					if (!IN6_ARE_ADDR_EQUAL(hoa, &npt->hoa)) 
						continue;
					
					if (npt->nemo_if) {
						npt->nemo_if = NULL; 
						/* remove default route */
#ifdef MIP_MCOA
						if (bid <= 0) {
#endif /* MIP_MCOA */
						route_del(0);
#ifdef MIP_MCOA						
						}
#endif /* MIP_MCOA */
					}
                                }
                                break;
			case MIPM_BC_ADD:
			case MIPM_BC_CHANGE:
				if (mode != MODE_HA)
					break;

                                mbc = (struct mipm_bc_info *)buf;
				/* if R flag is not set, ignore the BU */
                                if (!(mbc->mipc_flags & (IP6_MH_BU_HOME | IP6_MH_BU_ROUTER))) 
					break;
				hoa = &((struct sockaddr_in6 *)MIPC_HOA(mbc))->sin6_addr;
				if (hoa == NULL)
					break;

				memset(&src, 0, sizeof(src));
				memset(&dst, 0, sizeof(dst));
				src.sin6_family = dst.sin6_family = AF_INET6;
				src.sin6_len = dst.sin6_len = sizeof(struct sockaddr_in6);
				src.sin6_addr = 
					((struct sockaddr_in6 *)MIPC_CNADDR(mbc))->sin6_addr;
				dst.sin6_addr = 
       					((struct sockaddr_in6 *)MIPC_COA(mbc))->sin6_addr;

#ifndef MIP_MCOA
				nif = nemo_setup_forwarding((struct sockaddr *)&src, 
						(struct sockaddr *)&dst, hoa);
#else
				bid = mbc->mipc_bid;
				syslog(LOG_INFO, "received BID is %d\n", bid);
				nif = nemo_setup_forwarding((struct sockaddr *)&src, 
						(struct sockaddr *)&dst, hoa, bid);
#endif /* MIP_MCOA */
				if (nif == NULL) 
					break;

				route_del(if_nametoindex(nif->ifname));

				for (npt = LIST_FIRST(&nemo_mnpthead); npt; npt = nptn) {
					nptn = LIST_NEXT(npt, nemo_mnptentry);

					if (!IN6_ARE_ADDR_EQUAL(hoa, &npt->hoa)) 
						continue;

					npt->nemo_if = nif;
					route_add(&npt->nemo_prefix, &local_in6, NULL, 
						  npt->nemo_prefixlen, if_nametoindex(nif->ifname));
				}
                                break;

			case MIPM_BC_REMOVE:
				if (mode != MODE_HA)
					break;

                                mbc = (struct mipm_bc_info *)buf;
				/* if R flag is not set, ignore the BU */
                                if (!(mbc->mipc_flags & (IP6_MH_BU_HOME | IP6_MH_BU_ROUTER))) 
					break;
				hoa = &((struct sockaddr_in6 *)MIPC_HOA(mbc))->sin6_addr;
				if (hoa == NULL)
					break;

#ifndef MIP_MCOA
				nif = nemo_destroy_forwarding(hoa);
#else
				bid = mbc->mipc_bid;
				syslog(LOG_INFO, "received BID is %d\n", bid);
				nif = nemo_destroy_forwarding(hoa, bid);
#endif /* MIP_MCOA */

				route_del(if_nametoindex(nif->ifname));

				for (npt = LIST_FIRST(&nemo_mnpthead); npt; npt = nptn) {
					nptn = LIST_NEXT(npt, nemo_mnptentry);

					if (!IN6_ARE_ADDR_EQUAL(hoa, &npt->hoa)) 
						continue;

					npt->nemo_if = NULL; 
					/* remove default route */
                                }
                                break;
                        default:
                                break;
                        }

                }
	}
}

/*
 * Setup bi-directional tunnel between HA and CoA
 */ 
static struct nemo_if *
#ifndef MIP_MCOA 
nemo_setup_forwarding (src, dst, hoa)
#else
nemo_setup_forwarding (src, dst, hoa, bid) 
#endif /* MIP_MCOA */
	struct sockaddr *src, *dst;
	struct in6_addr *hoa;
#ifdef MIP_MCOA
	u_int16_t bid;
#endif /* MIP_MCOA */
{
	struct nemo_if *nif = NULL;

#ifndef MIP_MCOA
	nif = find_nemo_if(hoa);
#else
	nif = find_nemo_if(hoa, bid);
#endif /* MIP_MCOA */
	if (nif == NULL) {
		syslog(LOG_ERR, 
		       "No more available nemo interfaces\n");
		return NULL;
	}
	
	nif->hoa = *hoa;
#ifdef MIP_MCOA
	if (bid)
		nif->bid = bid;
#endif /* MIP_MCOA */

	/* tunnel disable (just for safety) */
	nemo_tun_del(nif->ifname);

	if (mode == MODE_HA) {
		/* tunnel activate */
		nemo_tun_set((struct sockaddr *)src,
			     (struct sockaddr *)dst,
			     if_nametoindex(nif->ifname), 0 /* FALSE */);
	} else if (mode == MODE_MR) {
		/* tunnel activate */
		nemo_tun_set((struct sockaddr *)src,
			     (struct sockaddr *)dst,
			     if_nametoindex(nif->ifname), 1 /* TRUE */);
	} 
	
	nemo_gif_ar_set(nif->ifname, &((struct sockaddr_in6 *)src)->sin6_addr);

	return nif;
}

static struct nemo_if *
#ifndef MIP_MCOA 
nemo_destroy_forwarding (hoa)
#else
nemo_destroy_forwarding (hoa, bid) 
#endif /* MIP_MCOA */
	struct in6_addr *hoa;
#ifdef MIP_MCOA
	u_int16_t bid;
#endif /* MIP_MCOA */
{
	struct nemo_if *nif = NULL;
	short flags;

#ifndef MIP_MCOA
	nif = find_nemo_if(hoa);
#else
	nif = find_nemo_if(hoa, bid);
#endif /* MIP_MCOA */
	if (nif == NULL) {
		syslog(LOG_ERR, 
		       "No associated nemo interfaces for %s\n", ip6_sprintf(hoa));
		return NULL;
	}

	nemo_tun_del(nif->ifname);

	flags = nemo_ifflag_get(nif->ifname);

	if (flags & IFF_UP)
		nemo_ifflag_set(nif->ifname, 
				(flags &= ~IFF_UP));

	memset(&nif->hoa, 0, sizeof(*hoa));
#ifdef MIP_MCOA
	nif->bid = 0;
#endif /* MIP_MCOA */

	return nif;
}


static void
terminate(dummy)
	int dummy;
{
	static struct nemo_if *nif;
	short flags;

	LIST_FOREACH(nif, &nemo_ifhead, nemo_ifentry) {
		syslog(LOG_INFO, "destroy %s\n", nif->ifname);
		nemo_tun_del(nif->ifname);

		flags = nemo_ifflag_get(nif->ifname);
		if (flags & IFF_UP)
			nemo_ifflag_set(nif->ifname, 
					(flags &= ~IFF_UP));

#ifndef MIP_MCOA
		if (mode == MODE_HA)
			route_del(if_nametoindex(nif->ifname));
#endif /* MIP_MCOA */
	}

#ifndef MIP_MCOA
	if (mode == MODE_MR)
		route_del(0);
#endif /* MIP_MCOA */

	exit(-1);
}
