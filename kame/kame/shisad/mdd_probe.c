#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/route.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */

#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <arpa/inet.h>

#include "mdd.h"

extern struct cifl     cifl_head;

static int send_rs(struct cif  *);
static void defrtrlists_flush(int);

int
probe_ifstatus(int s) {
	struct ifmediareq ifmr;
        struct cif *cifp;

        LIST_FOREACH(cifp, &cifl_head, cif_entries) {
		memset(&ifmr, 0, sizeof(ifmr));
                strncpy(ifmr.ifm_name, cifp->cif_name, IFNAMSIZ);
		
		if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) 
			continue;
		
		if (cifp->cif_linkstatus == ifmr.ifm_status)
			continue;
		
		if ((ifmr.ifm_status & IFM_AVALID) == 0) 
			continue;
		
		switch (IFM_TYPE(ifmr.ifm_active)) {
		case IFM_ETHER:
			if (ifmr.ifm_status & IFM_ACTIVE) {
				printf("active");
				/*defrtrlists_flush(s);*/
				send_rs(cifp);
			} else {
				printf("no carrier");
				
			}
			break;
			
		case IFM_FDDI:
		case IFM_TOKEN:
			break;
		case IFM_IEEE80211:
			if (ifmr.ifm_status & IFM_ACTIVE) {
				defrtrlists_flush(s);
				send_rs(cifp);
			} 
			break;
		}
		printf("\n");
		
		cifp->cif_linkstatus = ifmr.ifm_status;
	}
	
	return 0;
};


static int
send_rs(struct cif  *cifp) {
        struct msghdr msg;
        struct iovec iov;
        struct cmsghdr  *cmsgptr = NULL;
        struct in6_pktinfo *pi = NULL;
        struct sockaddr_in6 to;
        char adata[512], buf[1024];
        struct nd_router_solicit *rs;
        size_t rslen = 0;
/*
	struct nd_opt_hdr *opthdr;
	char *addr;
*/
	int icmpsock = -1, on = 1;

	icmpsock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (icmpsock < 0) {
		perror("socket for ICMPv6");
		return 0;
	}
	if (setsockopt(icmpsock, IPPROTO_IPV6, 
		       IPV6_RECVPKTINFO, &on, sizeof(on)) < 0) {
		perror("setsockopt IPV6_RECVPKTINFO for ICMPv6");
		return 0;
	}


        memset(&to, 0, sizeof(to));
        if (inet_pton(AF_INET6, "ff02::1",&to.sin6_addr) != 1) {
		close (icmpsock);
                return -1;
	}
	to.sin6_family = AF_INET6;
	to.sin6_port = 0;
	to.sin6_scope_id = 0;
	to.sin6_len = sizeof (struct sockaddr_in6);

        msg.msg_name = (void *)&to;
        msg.msg_namelen = sizeof(struct sockaddr_in6);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = (void *) adata;
        msg.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo)) 
		+ CMSG_SPACE(sizeof(int));

	/* Packet Information i.e. Source Address */
	cmsgptr = CMSG_FIRSTHDR(&msg);
	pi = (struct in6_pktinfo *)(CMSG_DATA(cmsgptr));
	memset(pi, 0, sizeof(*pi));
	pi->ipi6_ifindex = if_nametoindex(cifp->cif_name);
	cmsgptr->cmsg_level = IPPROTO_IPV6;
	cmsgptr->cmsg_type = IPV6_PKTINFO;
	cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	cmsgptr = CMSG_NXTHDR(&msg, cmsgptr);

        cmsgptr->cmsg_level = IPPROTO_IPV6;
        cmsgptr->cmsg_type = IPV6_HOPLIMIT;
        cmsgptr->cmsg_len = CMSG_LEN(sizeof(int));
        *(int *)(CMSG_DATA(cmsgptr)) = 255;
        cmsgptr = CMSG_NXTHDR(&msg, cmsgptr);
		
	bzero(buf, sizeof(buf));
	rs = (struct nd_router_solicit *)buf;
        rs->nd_rs_type = ND_ROUTER_SOLICIT;
        rs->nd_rs_code = 0;
        rs->nd_rs_cksum = 0;
        rs->nd_rs_reserved = 0;
	rslen = sizeof(struct nd_router_solicit);


#if 0
	opthdr = (struct nd_opt_hdr *) (buf + rslen);
	opthdr->nd_opt_type = ND_OPT_SOURCE_LINKADDR; 

	switch(mif->sockdl.sdl_type) {
	case IFT_ETHER:
#ifdef IFT_IEEE80211
	case IFT_IEEE80211:
#endif
		opthdr->nd_opt_len = (ROUNDUP8(ETHER_ADDR_LEN + 2)) >> 3;
		addr = (char *)(opthdr + 1);
		memcpy(addr, LLADDR(&mif->sockdl), ETHER_ADDR_LEN);
		rslen += ROUNDUP8(ETHER_ADDR_LEN + 2);
		break;
	default:
		return -1;
	}
#endif

	iov.iov_base = buf;
	iov.iov_len = rslen;
	
	if (sendmsg(icmpsock, &msg, 0) < 0)
		perror ("sendmsg");

	printf("sending -- RS\n");

	close (icmpsock);
	return errno;
}

static void
defrtrlists_flush(int s) {
	char dummyif[IFNAMSIZ+8];

	strncpy(dummyif, "lo0", sizeof(dummyif));

	if (ioctl(s, SIOCSRTRFLUSH_IN6, (caddr_t)&dummyif) < 0)
		perror("ioctl(SIOCSRTRFLUSH_IN6)");

}
