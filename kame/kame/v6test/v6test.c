/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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

#include "common.h"
#ifdef HAVE_LIBPCAP
#include <pcap.h>
#endif 

u_char buf[65536];
char *conffile = NULL;
char *iface = NULL;
char *optsrc, *optdst, *srceaddr, *dsteaddr;
static struct in6_addr ip6src, ip6dst;
struct in6_addr *optsrcn, *optdstn;
int sflag = 0, dflag = 0, nflag = 0;
static void usage __P((void));
int main __P((int, char *[]));
int bpf_open __P((char *));
static int linkhdrlen __P((int, char *));
static void form __P((int, char *, int));
static void form_ether __P((int));
static void form_null __P((int));

static void
usage()
{
	fprintf(stderr,
		"usage: v6test [-d dstaddr] [-f configfile] [-i interface] "
		"[-n] [-s srcaddr] testname [testname...]\n");
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int size, ch;
	int fd = -1;
	int linkhdr;
	
	while ((ch = getopt(argc, argv, "d:f:i:ns:")) != -1)
		switch(ch) {
		case 's':
			if (inet_pton(AF_INET6, optarg, &ip6src) != 1) {
				optdst = optarg;
			} else
				optsrcn = &ip6src;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'd':
			if(inet_pton(AF_INET6, optarg, &ip6dst) != 1) {
				optdst = optarg;
			} else
				optdstn = &ip6dst;
			break;
		case 'i':
			iface = optarg;
			break;
		case 'n':
			nflag++;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
		/*NOTREACHED*/
	}

	if (nflag == 0)
		fd = bpf_open(iface);
	linkhdr = linkhdrlen(fd, iface);
	if (linkhdr < 0) {
		errx(1, "unsupported interface %s", iface);
		/*NOTREACHED*/
	}
	for (; argc > 0; argv++, argc--) {
		bzero(buf, sizeof(buf));
		size = getconfig(*argv, buf + linkhdr);
		form(fd, iface, size);
		if (size && nflag == 0)
			write(fd, buf, size + linkhdr);
	}
	exit(0);
}

int
bpf_open(char *iface)
{
	/* based on dhcpc_subr.c(dhcp-1.3.9p2) */
	int n = 0, fd;
	char dev[16];
	struct ifreq ifr;
#ifdef HAVE_LIBPCAP
	char pcaperrbuf[PCAP_ERRBUF_SIZE];
#endif 
	
#define NBPFILTER 4
	do {
		sprintf(dev, "/dev/bpf%d", n++);
		fd = open(dev, O_RDWR);
	} while (fd < 0 && n < NBPFILTER);
	if (fd < 0) {
		fprintf(stderr, "Can't open bpf\n");
		exit(1);
	}

	if (ioctl(fd, BIOCIMMEDIATE, &n) < 0) {
		perror("ioctl(BIOCIMMEDIATE)");
		exit(1);
	}

	bzero(&ifr, sizeof(ifr));
#ifdef HAVE_LIBPCAP
	if (iface == NULL &&
	    (iface = pcap_lookupdev(pcaperrbuf)) == NULL) {
		fprintf(stderr, "pcap_lookupdev: %s\n", pcaperrbuf);
		exit(1);
	}
	fprintf(stderr, "outgoing interface is %s\n", iface);
#else
	if (iface == NULL) {
		fprintf(stderr, "interface must be specified\n");
		exit(1);
	}
#endif 
	strcpy(ifr.ifr_name, iface);
	if (ioctl(fd, BIOCSETIF, &ifr) < 0) {
		perror("ioctl(BIOCSETIF)");
		return(-1);
	}
	return(fd);
}

static int
linkhdrlen(fd, iface)
	int fd;
	char *iface;
{
	u_int v;

	if (fd < 0)
		return sizeof(struct ether_header);

	if (ioctl(fd, BIOCGDLT, (caddr_t)&v) < 0) {
		err(1, "ioctl(BIOCGDLT)");
		/*NOTREACHED*/
	}
	switch (v) {
	case DLT_EN10MB:
		return sizeof(struct ether_header);
	case DLT_NULL:
		return sizeof(u_int);
	}

	return -1;
}

static void
form(fd, iface, size)
	int fd, size;
	char *iface;
{
	u_int v;

	if (fd < 0)
		return;

	if (ioctl(fd, BIOCGDLT, (caddr_t)&v) < 0) {
		err(1, "ioctl(BIOCGDLT)");
		/*NOTREACHED*/
	}
	switch (v) {
	case DLT_EN10MB:
		form_ether(size);
		break;
	case DLT_NULL:
		form_null(size);
		break;
	}
}

static void
form_ether(size)
	int size;
{
	struct ether_header *ether;
	struct ip6_hdr *ip;
	
	ether = (struct ether_header *)buf;
	ip = (struct ip6_hdr *)(ether + 1);

#ifndef ETHERTYPE_IPV6
#define	ETHERTYPE_IPV6	0x86dd /* Ether type for IPv6 */
#endif
	ether->ether_type = htons(ETHERTYPE_IPV6);
	if (srceaddr)
		bcopy(srceaddr, ether->ether_shost, 6);
	if (dsteaddr)
		bcopy(dsteaddr, ether->ether_dhost, 6);
	else if (IN6_IS_ADDR_MULTICAST(&(ip->ip6_dst))) {
		ether->ether_dhost[0] = 0x33;
		ether->ether_dhost[1] = 0x33;
		ether->ether_dhost[2] = ip->ip6_dst.s6_addr[12];
		ether->ether_dhost[3] = ip->ip6_dst.s6_addr[13];
		ether->ether_dhost[4] = ip->ip6_dst.s6_addr[14];
		ether->ether_dhost[5] = ip->ip6_dst.s6_addr[15];
	} else {
		ether->ether_dhost[0] = ip->ip6_dst.s6_addr[8] & 0xfd;
		ether->ether_dhost[1] = ip->ip6_dst.s6_addr[9];
		ether->ether_dhost[2] = ip->ip6_dst.s6_addr[10];
		ether->ether_dhost[3] = ip->ip6_dst.s6_addr[13];
		ether->ether_dhost[4] = ip->ip6_dst.s6_addr[14];
		ether->ether_dhost[5] = ip->ip6_dst.s6_addr[15];
	}		

	cksum6(sizeof(struct ether_header), size);
}

static void
form_null(size)
	int size;
{
	u_int *af;

	af = (u_int *)buf;
	*af = AF_INET6;

	cksum6(sizeof(*af), size);
}
