/*	$KAME: connect.c,v 1.14 2003/06/12 09:58:48 suz Exp $ */
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
#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <arpa/inet.h>

#include "common.h"

void setopthdr __P((int, int));
void usage __P(());

int s;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, ret_ga, hlim = -1, ngate = 0, rthlen;
	struct addrinfo hints, *res;
	struct sockaddr_in6 dst;
	char readbuf[1024], *port = DEFAULTPORT;
	char **gate, *src = NULL;
	struct ip6_rthdr *rthdr;

	if ((gate = malloc((sizeof(char *)) * argc)) == NULL)
		err(1, "malloc");

	while((ch = getopt(argc, argv, "h:p:g:s:")) != -1)
		switch(ch) {
		case 'g':
			gate[ngate++] = optarg;
			break;
		case 'h':
			hlim = atoi(optarg);
			break;
		case 'p':
			port = optarg;
			break;
		case 's':
			src = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	ret_ga = getaddrinfo(argv[0], port, &hints, &res);
	if (ret_ga)
		errx(1, "getaddrinfo: %s\n", gai_strerror(ret_ga));

	if ((s = socket(res->ai_family, res->ai_socktype, 0)) < 0)
		err(1, "socket");

	if (hlim > 0 &&
	    setsockopt(s, IPPROTO_IPV6, IPV6_HOPLIMIT, &hlim, sizeof(hlim))) {
		warn("setsockopt(IPV6_HOPLIMIT %d)", hlim); /* assert? */
	}
	if (hlim > 0 &&
	    setsockopt(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
		       &hlim, sizeof(hlim))) {
		err(1, "setsockopt(IPV6_UNICAST_HOPS %d)", hlim);
	}
	if (src) {
		struct addrinfo shints, *sres;
		struct in6_pktinfo pktinfo;

		memset(&hints, 0, sizeof(hints));
		shints.ai_family = PF_INET6;
		shints.ai_socktype = SOCK_STREAM;
		shints.ai_protocol = IPPROTO_TCP;
		ret_ga = getaddrinfo(src, port, &shints, &sres);
		if (ret_ga) {
			errx(1, "getaddrinfo for %s: %s\n", src,
			     gai_strerror(ret_ga));
		}

		memset(&pktinfo, 0, sizeof(pktinfo));
		memcpy(&pktinfo.ipi6_addr,
		       &((struct sockaddr_in6 *)sres->ai_addr)->sin6_addr,
		       sizeof(pktinfo.ipi6_addr));

		if (setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO, &pktinfo,
			       sizeof(pktinfo)) == 0) {	/* should fail */
			warnx("IPV6_PKTINFO(%s) has succeeded for a "
			      "TCP socket", src);
		} else
			warn("IPV6_PKTINFO(%s) failed (correct)", src);

		freeaddrinfo(sres);
	}

	if (ngate > 0) {
		int i;

		rthlen = inet6_rth_space(IPV6_RTHDR_TYPE_0, ngate);
		if ((rthdr = malloc(rthlen)) == NULL)
			err(1, "malloc (%d) failed", rthlen);
		inet6_rth_init((void *)rthdr, rthlen,
			       IPV6_RTHDR_TYPE_0, ngate);

		for (i = 0; i < ngate; i++) {
			struct in6_addr g;
			int e;

			if ((e = inet_pton(AF_INET6, gate[i], &g)) != 1) {
				if (e)
					warn("inet_pton(%s) failed", gate[i]);
				else
					warnx("inet_pton(%s) failed", gate[i]);
				exit(1);
			}

			if (inet6_rth_add(rthdr, &g))
				errx(1, "inet6_rth_add failed");
		}
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RTHDR,
			       (void *)rthdr, rthlen))
			err(1, "setsockopt(IPV6_RTHDR)");

		free(rthdr);
		free(gate);
	}

	if (connect(s, res->ai_addr, res->ai_addrlen) < 0)
		err(1, "connect");
	memcpy(&dst, res->ai_addr, sizeof(dst));
	freeaddrinfo(res);

	printf("connect OK\n");

	while(1) {
		if (fgets(readbuf, sizeof(readbuf), stdin) == NULL)
			break;

		/* handle special control messages */
		if (strncasecmp(readbuf, "quit", 4) == 0)
			break;
#ifdef IPV6_HOPLIMIT
		if (strncasecmp(readbuf, "hlim", 4) == 0) {
			hlim = atoi(&readbuf[4]);

			if (setsockopt(s, IPPROTO_IPV6, IPV6_HOPLIMIT,
				       &hlim, sizeof(hlim)))
				warn("setsockopt(IPV6_HOPLIMIT %d)", hlim);
		}
#endif
#ifdef IPV6_HOPOPTS
		if (strncasecmp(readbuf, "hbh", 3) == 0)
			setopthdr(atoi(&readbuf[3]), IPV6_HOPOPTS);
#endif
#ifdef IPV6_RTHDRDSTOPTS
		if (strncasecmp(readbuf, "rdst", 4) == 0)
			setopthdr(atoi(&readbuf[4]), IPV6_RTHDRDSTOPTS);
#endif
#ifdef IPV6_DSTOPTS
		if (strncasecmp(readbuf, "dst", 3) == 0)
			setopthdr(atoi(&readbuf[3]), IPV6_DSTOPTS);
#endif
#ifdef IPV6_RTHDR_TYPE_0
		if (strncasecmp(readbuf, "rthdr", 5) == 0) {
			int i, hops = atoi(&readbuf[5]);

			if (hops == 0)
				goto setrth; /* remove the header */

			rthlen = inet6_rth_space(IPV6_RTHDR_TYPE_0, hops);
			if ((rthdr = malloc(rthlen)) == NULL) {
				warnx("malloc (%d) failed", rthlen);
				goto sendbuf;
			}
			inet6_rth_init((void *)rthdr, rthlen,
				       IPV6_RTHDR_TYPE_0, hops);
			
			for (i = 0; i < hops; i++) {
				if (inet6_rth_add(rthdr, &dst.sin6_addr)) {
					warnx("inet6_rth_add failed");
					free(rthdr);
					goto sendbuf;
				}
			}

		  setrth:
			if (setsockopt(s, IPPROTO_IPV6, IPV6_RTHDR,
				       (void *)rthdr, rthlen))
				warn("setsockopt(IPV6_RTHDR)");
			free(rthdr);
		}
#endif

	  sendbuf:
		if (write(s, readbuf, strlen(readbuf)) < 0)
			warn("write");
	}

	exit(0);
}

void
setopthdr(optlen, hdrtype)
	int optlen, hdrtype;
{
	struct ip6_hbh *hbh = NULL;
	int opthlen = 0, curlen;
	void *optp = NULL;
	static char optbuf[128]; /* XXX */

	if (optlen == 0)
		goto sethbh;

	if ((opthlen = inet6_opt_init(NULL, 0)) == -1) {
		warnx("inet6_opt_init(NULL) failed");
		return;
	}
	if ((opthlen = inet6_opt_append(NULL, 0, opthlen,
				       10, /* dummy opt */
				       optlen, 1,
				       NULL)) == -1) {
		warnx("inet6_opt_append(NULL, %d)", optlen);
		return;
	}
	if ((opthlen = inet6_opt_finish(NULL, 0, opthlen)) == -1) {
		warnx("inet6_opt_finish(NULL, %d)", opthlen);
		return;
	}
	if ((hbh = malloc(opthlen)) == NULL) {
		warnx("malloc %d bytes for hbhopt failed",
		      opthlen);
		return;
	}
	if ((curlen = inet6_opt_init(hbh, opthlen)) == -1) {
		warnx("inet6_opt_init(hbh, %d)", opthlen);
		free(hbh);
		return;
	}
	if ((curlen = inet6_opt_append(hbh, opthlen, curlen,
				       10, /* dummy */
				       optlen, 1,
				       &optp)) == -1) {
		warnx("inet6_opt_append"
		      "(cur=%d, optlen=%d, opthlen=%d)",
		      curlen, optlen, opthlen);
		free(hbh);
		return;
	}
	(void)inet6_opt_set_val(optp, 0, (void *)optbuf,
				optlen);
	if (inet6_opt_finish(hbh, opthlen, curlen) == -1) {
		warnx("inet6_opt_finish(opthlen=%d, curlen=%d)",
		      opthlen, curlen);
		free(hbh);
		return;
	}

  sethbh:
	if (setsockopt(s, IPPROTO_IPV6, hdrtype,
		       (void *)hbh, opthlen)) {
		switch(hdrtype) {
#ifdef IPV6_HOPOPTS
		case IPV6_HOPOPTS:
			warn("setsockopt(IPV6_HOPOPTS)");
			break;
#endif
#ifdef IPV6_RTHDRDSTOPTS
		case IPV6_RTHDRDSTOPTS:
			warn("setsockopt(IPV6_RTHDRDSTOPTS)");
			break;
#endif
#ifdef IPV6_DSTOPTS
		case IPV6_DSTOPTS:
			warn("setsockopt(IPV6_DSTOPTS)");
			break;
#endif
		}
	}
	free(hbh);
}

void
usage()
{
	fprintf(stderr, "usage: connect [-h hoplimit] [-p port] [-s dummysrc] "
		"[-g hop1 [-g hop2...]] addr\n");

	exit(1);
}
