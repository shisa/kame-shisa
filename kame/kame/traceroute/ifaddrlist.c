/*
 * Copyright (c) 1997
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /home/keiichi/tmp/shisa/cvsroot-kame-shisa/kame/kame/kame/traceroute/ifaddrlist.c,v 1.1.1.1 2004/09/22 07:25:24 t-momose Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <sys/time.h>				/* concession to AIX */

#if __STDC__
struct mbuf;
struct rtentry;
#endif

#include <net/if.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "ifaddrlist.h"


/* Not all systems have IFF_LOOPBACK */
#ifdef IFF_LOOPBACK
#define ISLOOPBACK(p) ((p)->ifr_flags & IFF_LOOPBACK)
#else
#define ISLOOPBACK(p) (strcmp((p)->ifr_name, "lo0") == 0)
#endif

static unsigned int if_maxindex __P((void));

static unsigned int
if_maxindex()
{
	struct if_nameindex *p, *p0;
	unsigned int max = 0;

	p0 = if_nameindex();
	for (p = p0; p && p->if_index && p->if_name; p++) {
		if (max < p->if_index)
			max = p->if_index;
	}
	if_freenameindex(p0);
	return max;
}

/*
 * Return the interface list
 */
int
ifaddrlist(register struct ifaddrlist **ipaddrp, register char *errbuf, size_t l)
{
#ifdef HAVE_GETIFADDRS
	struct ifaddrs *ifap, *ifa;
	int nipaddr;
	struct ifaddrlist *al;
	struct ifaddrlist *ifaddrlist;
	unsigned int maxif;
	struct sockaddr_in *sin;

#if 1
	maxif = if_maxindex() * 3; /* 3 is a magic number... */
#else
	maxif = 64;
#endif

	ifaddrlist = (struct ifaddrlist *)malloc(maxif *
		sizeof(struct ifaddrlist));
	if (ifaddrlist == NULL)
		return -1;

	if (getifaddrs(&ifap) != 0)
		return -1;

	al = ifaddrlist;
	nipaddr = 0;
	for (ifa = ifap; ifa && nipaddr < maxif; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if (((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr ==
		    htonl(INADDR_ANY))
			continue;
		if ((ifa->ifa_flags & IFF_UP) == 0)
			continue;
#ifdef IFF_LOOPBACK
		if ((ifa->ifa_flags & IFF_LOOPBACK) != 0)
			continue;
#else
		if (strcmp(ifa->ifa_name, "lo0") == 0)
			continue;
#endif

		sin = (struct sockaddr_in *)ifa->ifa_addr;
		al->addr = sin->sin_addr.s_addr;
		al->device = strdup(ifa->ifa_name);
		++al;
		++nipaddr;
	}

	*ipaddrp = ifaddrlist;
#ifdef HAVE_FREEIFADDRS
	freeifaddrs(ifap);
#else
	free(ifap);
#endif
	return nipaddr;
#else
	register int fd, nipaddr;
#ifdef HAVE_SOCKADDR_SA_LEN
	register int n;
#endif
	register struct ifreq *ifrp, *ifend, *ifnext, *mp;
	register struct sockaddr_in *sin;
	register struct ifaddrlist *al;
	struct ifconf ifc;
	struct ifreq *ibuf, ifr;
	char device[sizeof(ifr.ifr_name) + 1];
	struct ifaddrlist *ifaddrlist;
	unsigned int maxif;

#if 1
	maxif = if_maxindex() * 3; /* 3 is a magic number... */
#else
	maxif = 64;
#endif
	ifaddrlist = (struct ifaddrlist *)malloc(maxif *
		sizeof(struct ifaddrlist));
	if (ifaddrlist == NULL)
		return -1;
	ibuf = (struct ifreq *)malloc(maxif * sizeof(struct ifreq));
	if (ibuf == NULL) {
		free(ifaddrlist);
		return -1;
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, l, "socket: %s", strerror(errno));
		free(ifaddrlist);
		free(ibuf);
		return (-1);
	}
	ifc.ifc_len = maxif * sizeof(struct ifreq);
	ifc.ifc_buf = (caddr_t)ibuf;

	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0 ||
	    ifc.ifc_len < sizeof(struct ifreq)) {
		(void)snprintf(errbuf, l, "SIOCGIFCONF: %s", strerror(errno));
		(void)close(fd);
		free(ifaddrlist);
		free(ibuf);
		return (-1);
	}
	ifrp = ibuf;
	ifend = (struct ifreq *)((char *)ibuf + ifc.ifc_len);

	al = ifaddrlist;
	mp = NULL;
	nipaddr = 0;
	for (; ifrp < ifend; ifrp = ifnext) {
#ifdef HAVE_SOCKADDR_SA_LEN
		n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
			ifnext = ifrp + 1;
		else
			ifnext = (struct ifreq *)((char *)ifrp + n);
		if (ifrp->ifr_addr.sa_family != AF_INET)
			continue;
#else
		ifnext = ifrp + 1;
#endif

		if (((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr.s_addr ==
		    htonl(INADDR_ANY))
			continue;
		/*
		 * Need a template to preserve address info that is
		 * used below to locate the next entry.  (Otherwise,
		 * SIOCGIFFLAGS stomps over it because the requests
		 * are returned in a union.)
		 */
		strncpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifrp->ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifr) < 0) {
			if (errno == ENXIO)
				continue;
			(void)snprintf(errbuf, l, "SIOCGIFFLAGS: %.*s: %s",
			    (int)sizeof(ifr.ifr_name), ifr.ifr_name,
			    strerror(errno));
			(void)close(fd);
			free(ifaddrlist);
			free(ibuf);
			return (-1);
		}

		/* Must be up and not the loopback */
		if ((ifr.ifr_flags & IFF_UP) == 0 || ISLOOPBACK(&ifr))
			continue;

		(void)strncpy(device, ifr.ifr_name, sizeof(ifr.ifr_name));
		device[sizeof(device) - 1] = '\0';
		if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) < 0) {
			(void)snprintf(errbuf, l, "SIOCGIFADDR: %s: %s",
			    device, strerror(errno));
			(void)close(fd);
			free(ifaddrlist);
			free(ibuf);
			return (-1);
		}

		sin = (struct sockaddr_in *)&ifr.ifr_addr;
		al->addr = sin->sin_addr.s_addr;
		al->device = strdup(device);
		++al;
		++nipaddr;
	}
	(void)close(fd);

	*ipaddrp = ifaddrlist;
	free(ibuf);
	return (nipaddr);
#endif
}
