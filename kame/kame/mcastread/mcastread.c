/*
 * Copyright (C) 1999 WIDE Project.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

FILE *outfp;
int recvsock;
int vflag = 0;
int af = AF_INET6;

void recvloop __P((void));
int get_socket __P((struct addrinfo *, char *, char *, struct addrinfo **));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *ifname, *servname, *address, *srclist = NULL;
	struct addrinfo hints;
	struct addrinfo *ai, *ai_valid;
	int c;
	int error;

	while ((c = getopt(argc, argv, "s:v46")) != -1) {
		switch (c) {
		case 'v':
			vflag++;
			break;
		case '4':
			af = AF_INET;
			break;
		case '6':
			af = AF_INET6;
			break;
		case 's':
			if ((srclist = strdup(optarg)) == 0) {
				err(1, "strdup");
			}
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 3 && argc != 4)
		usage();
	ifname = argv[0];
	address = argv[1];
	servname = argv[2];
	if (argc == 3)
		outfp = stdout;
	else {
		if ((outfp = fopen(argv[3], "w")) == NULL)
			err(1, "fopen(%s)", argv[3]);
	}

	if (if_nametoindex(ifname) == 0)
		errx(1, "invalid interface %s", ifname);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	if ((error = getaddrinfo(address, servname, &hints, &ai)) != 0)
		errx(1, "getaddrinfo: %s\n", gai_strerror(error));

	if ((recvsock = get_socket(ai, ifname, srclist, &ai_valid)) < 0)
		errx(1, "can't allocate socket");

	freeaddrinfo(ai);

	recvloop();
	/*NOTREACHED*/

	exit(0);
	/*NOTREACHED*/
}

void
recvloop(void)
{
	char recvbuf[1024];
	int cc, ccout, fromlen;
	struct sockaddr_in6 from6;

	while (1) {
		cc = recvfrom(recvsock, (void *)recvbuf, sizeof(recvbuf), 0,
			      (struct sockaddr *)&from6, &fromlen);
		if (cc < 0) {
			warn("recvfrom");
			continue;
		}
		if ((ccout = fwrite(recvbuf, cc, 1, outfp)) < 1)
			warn("fwrite"); /* XXX: should be recovered */
		fflush(outfp);
	}
}

int
get_socket(ai0, ifname, srclist, valid)
	struct addrinfo *ai0;
	char *ifname;
	char *srclist;
	struct addrinfo **valid;
{
	int so = -1, on;
	struct addrinfo *ai;
	char *emsg = NULL;
	char hbuf[1024];
	int ifindex = if_nametoindex(ifname);
#ifdef MCAST_JOIN_SOURCE_GROUP
	int error;
#endif

	for (ai = ai0; ai; ai = ai->ai_next) {
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, hbuf, sizeof(hbuf),
				NULL, 0, NI_NUMERICHOST) != 0) {
			emsg = "getnameinfo";
			if (vflag)
				warn("%s", emsg);
			continue;
		}

		if (vflag)
			warnx("trying %s", hbuf);

		so = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (so < 0) {
			emsg = "socket";
			if (vflag)
				warn("%s", emsg);
			continue;
		}

		on = 1;
		if (setsockopt(so, SOL_SOCKET, SO_REUSEPORT, &on,
		    sizeof(on))) {
			err(1, "setsockopt(SO_REUSEPORT)");
		}

		if (bind(so, ai->ai_addr, ai->ai_addrlen) < 0) {
			emsg = "bind";
			if (vflag)
				warn("%s", emsg);
			close(so);
			so = -1;
			continue;
		}

		/* join multicast group */

#ifdef MCAST_JOIN_SOURCE_GROUP
		/* in case of SSM join, you can use protocol-independent API */
		if (srclist != NULL) {
			struct addrinfo hints, *res, *ptr;
			struct group_source_req gsreq;
			int level;

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = ai->ai_family;
			hints.ai_protocol = ai->ai_protocol;
			if ((error = getaddrinfo(srclist, NULL, &hints, &res)) != 0) {
				emsg = gai_strerror(error);
				if (vflag)
					warn("%s", emsg);
				close(so);
				so = -1;
				continue;
			}

			memset(&gsreq, 0, sizeof(gsreq));
			gsreq.gsr_interface = ifindex;
			memcpy(&gsreq.gsr_group, ai->ai_addr, ai->ai_addrlen);

			switch (ai->ai_family) {
			case AF_INET6:
				level = IPPROTO_IPV6;
				break;
			case AF_INET:
				level = IPPROTO_IP;
				break;
			default:
				if (vflag)
					warn("unsupported address family %d\n", ai->ai_family);
				continue;
			}

			for (ptr = res; ptr; ptr = ptr->ai_next) {
				memcpy(&gsreq.gsr_source, ptr->ai_addr, ptr->ai_addrlen);
				if (setsockopt(so, level, MCAST_JOIN_SOURCE_GROUP,
					       &gsreq, sizeof(gsreq))) {
					emsg = "setsockopt(MCAST_JOIN_SOURCE_GROUP)";
					if (vflag)
						warn("%s", emsg);
					close(so);
					so = -1;
					continue;
				}
			}
			freeaddrinfo(res);

			if (vflag)
				warnx("using (S,G)=(%s,%s)", srclist, hbuf);

			*valid = ai;
			return so;
		}
#endif

		/* in case of ASM join, you have to use protocol-dependent API */
		switch (ai->ai_family) {
		case AF_INET6:
		    {
			struct ipv6_mreq mreq6;
			mreq6.ipv6mr_interface = if_nametoindex(ifname);
			if (mreq6.ipv6mr_interface == 0) {
				emsg = "if_nametoindex";
				if (vflag)
					warn("%s", emsg);
				close(so);
				so = -1;
				continue;
			}
			memcpy(&mreq6.ipv6mr_multiaddr,
				&((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr,
				sizeof(mreq6.ipv6mr_multiaddr));
			if (setsockopt(so, IPPROTO_IPV6, IPV6_JOIN_GROUP,
					&mreq6, sizeof(mreq6))) {
				emsg = "setsockopt(IPV6_JOIN_GROUP)";
				if (vflag)
					warn("%s", emsg);
				close(so);
				so = -1;
				continue;
			}
			break;
		    }
		case AF_INET:
		    {
			struct in_addr a;
			struct ip_mreq mreq4;

			/* XXX uses "0.0.0.0/8 for interface index" API */
			a.s_addr = htonl(ifindex);
			if (setsockopt(so, IPPROTO_IP, IP_MULTICAST_IF,
			    &a, sizeof(a)) < 0) {
				emsg = "setsockopt(IP_MULTICAST_IF)";
				if (vflag)
					warn("%s", emsg);
				close(so);
				so = -1;
				continue;
			}
			mreq4.imr_multiaddr =
			    ((struct sockaddr_in *)ai->ai_addr)->sin_addr;
			mreq4.imr_interface = a;
			if (setsockopt(so, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			    &mreq4, sizeof(mreq4))) {
				emsg = "setsockopt(IP_ADD_MEMBERSHIP)";
				if (vflag)
					warn("%s", emsg);
				close(so);
				so = -1;
				continue;
			}
			break;
		    }
		default:
			errno = 0;
			emsg = "unsupported address family";
			if (vflag)
				warnx("%s: %s", hbuf, emsg);
			close(so);
			so = -1;
			continue;
		}

		if (vflag)
			warnx("using %s", hbuf);

		*valid = ai;
		return so;
	}

	if (emsg && !vflag) {
		if (errno)
			warn("%s", emsg);
		else
			warnx("%s", emsg);
	}
	*valid = NULL;
	return -1;
}

void
usage(void)
{
	fprintf(stderr,
		"usage: mcastread [-v46] [-s source] interface multicastaddr port [file]\n");
	exit(1);
	/*NOTREACHED*/
}
