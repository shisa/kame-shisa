/*      $Id: binding.c,v 1.5 2004/10/08 11:11:02 keiichi Exp $  */
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */
#include <net/mipsock.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/ip6mh.h>
#include <netinet/in_var.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef MIP_MN
#include <sys/sockio.h>
#include <net/if_mip.h>
#endif /* MIP_MN */

#include "callout.h"
#include "shisad.h"
#include "fsm.h"
#include "stat.h"

#ifdef MIP_MN
void bul_flush(struct mip6_hoainfo *);
static struct binding_update_list *bul_create(struct in6_addr *,
    struct in6_addr *, u_int16_t, struct mip6_hoainfo *);
#endif /* MIP_MN */

#ifndef MIP_MN
struct binding_cache_head bchead;
static void mip6_bc_set_refresh_timer(struct binding_cache *, int);
static void mip6_bc_stop_refresh_timer(struct binding_cache *);
#endif /* MIP_MN */


#ifndef MIP_MN
/*
 * Binding Cache State Change
 *
 *         receive BU
 *       +---------------------+
 *      \|/                    |      no BU and expire
 * [Registering] -> [Requesting BU if needed] -> [Remove]
 *          soon be expired                
 */
void
mip6_bc_init()
{

	LIST_INIT(&bchead);
	mip6_flush_kernel_bc();
}

/* flush all bce registered in a kernel. */
void
mip6_flush_kernel_bc()
{
	struct mip_msghdr mipmsg;

	memset(&mipmsg, 0, sizeof(struct mip_msghdr));
	mipmsg.miph_msglen = sizeof(struct mip_msghdr);
	mipmsg.miph_type = MIPM_BC_FLUSH;
	if (write(mipsock, &mipmsg, sizeof(struct mip_msghdr)) == -1) {
		syslog(LOG_ERR,
		    "removing all bul entries failed.\n");
	}
}

#ifndef MIP_MCOA
struct binding_cache *
mip6_bc_add(hoa, coa, recvaddr, lifetime, flags, seqno)
	struct in6_addr *hoa, *coa, *recvaddr;
        u_int32_t lifetime;
	u_int8_t flags;
        u_int16_t seqno;
#else
struct binding_cache *
mip6_bc_add(hoa, coa, recvaddr, lifetime, flags, seqno, bid)
	struct in6_addr *hoa, *coa, *recvaddr;
        u_int32_t lifetime;
	u_int8_t flags;
        u_int16_t seqno, bid;
#endif /* MIP_MCOA */
{
	struct binding_cache *bc;
	time_t now;

	now = time(0);
	/* 
	 * Check BC availability. If an entry exists, its sequence
	 * number is compared with the requested binding cache
	 * entry. If the requesting entry has larger sequence number,
	 * add_bc() updates the exisitng entry with the bcreq.
	 */
#ifndef MIP_MCOA
	bc = mip6_bc_lookup(hoa, recvaddr);
#else
	bc = mip6_bc_lookup(hoa, recvaddr, bid);
#endif /* MIP_MCOA */
	if (bc) {
		bc->bc_myaddr = *recvaddr;
		bc->bc_lifetime = lifetime;
		bc->bc_flags = flags;
		bc->bc_seqno = seqno;
		/* update BC in the kernel via mipsock */
		bc->bc_coa = *coa;
		mipscok_bc_request(bc, MIPM_BC_CHANGE);

		goto done;
	} 

	/*
	 * If there is any entry related to the requesting bc entry, 
	 * allocate new entry for the requesting entry.
	 */
	bc = (struct binding_cache *)malloc(sizeof(struct binding_cache));
	if (bc == NULL)
		return (bc);
	memset(bc, 0, sizeof(*bc));
	bc->bc_hoa = *hoa;
	bc->bc_coa = *coa;
	bc->bc_myaddr = *recvaddr;
	bc->bc_lifetime = lifetime;
	bc->bc_flags = flags;
	bc->bc_seqno = seqno;
#ifdef MIP_MCOA
	bc->bc_bid = bid;
#endif /* MIP_MCOA */

	/* insert BC into the kernel via mipsock */
	mipscok_bc_request(bc, MIPM_BC_ADD);
	
        LIST_INSERT_HEAD(&bchead, bc, bc_entry);
 done:

	bc->bc_expire = now + bc->bc_lifetime;
	
	/* refreshment is called after the half of BC's lifetime */
	mip6_bc_set_refresh_timer(bc, bc->bc_lifetime / 2); 

	return (bc);
};

void
mip6_bc_delete(bcreq)
	struct binding_cache *bcreq;
{
	struct binding_cache *bc = NULL;

#if 1	/* Why does it try to re-find the same bce ? */
	/* if no BC is found, nothing need to be done here */
#ifndef MIP_MCOA
	bc = mip6_bc_lookup(&bcreq->bc_hoa, &bcreq->bc_myaddr);
#else
	bc = mip6_bc_lookup(&bcreq->bc_hoa, &bcreq->bc_myaddr, bcreq->bc_bid);
#endif /* MIP_MCOA */
	if (bc == NULL)
		return;
#endif /* 1 */
	/* Fist stop timer */ /* XXX Fist ?? */
	mip6_bc_stop_refresh_timer(bc);

	/* delete BC into the kernel via mipsock */
	mipscok_bc_request(bc, MIPM_BC_REMOVE);

	LIST_REMOVE(bc, bc_entry); 
	free(bc);

	return;
};



/* src can be wildcard */
struct binding_cache *
#ifndef MIP_MCOA
mip6_bc_lookup(hoa, src) 
	struct in6_addr *hoa;
	struct in6_addr *src;
#else
mip6_bc_lookup(hoa, src, bid) 
	struct in6_addr *hoa;
	struct in6_addr *src;
	u_int16_t bid;
#endif /* MIP_MCOA */
{
	struct binding_cache *bc, *bc_nxt = NULL;

        for (bc = LIST_FIRST(&bchead); bc; bc = bc_nxt) {
		bc_nxt =  LIST_NEXT(bc, bc_entry);

#ifdef MIP_MCOA
		if (bid && bid != bc->bc_bid)
			continue;
#endif /* MIP_MCOA */
		if (src && !IN6_ARE_ADDR_EQUAL(src, &bc->bc_myaddr))
			continue;
		
		if (IN6_ARE_ADDR_EQUAL(hoa, &bc->bc_hoa)) 
			return (bc);
	}

	return (NULL);
};

void
command_show_bc(s)
	int s;
{
	char buff[2048];
	struct binding_cache *bc;

        for (bc = LIST_FIRST(&bchead); bc; bc = LIST_NEXT(bc, bc_entry)) {
		sprintf(buff, "%s ", ip6_sprintf(&bc->bc_hoa));
		sprintf(buff + strlen(buff), "%s ", ip6_sprintf(&bc->bc_coa));
		sprintf(buff + strlen(buff), "%s ", ip6_sprintf(&bc->bc_myaddr));
		sprintf(buff + strlen(buff), "%d %c%c%c%c %d\n",
			bc->bc_lifetime,
			(bc->bc_flags & IP6_MH_BU_ACK)  ? 'A' : '-',
			(bc->bc_flags & IP6_MH_BU_HOME) ? 'H' : '-',
			(bc->bc_flags & IP6_MH_BU_LLOCAL) ? 'L' : '-',
			(bc->bc_flags & IP6_MH_BU_KEYM)  ? 'K' : '-',
			bc->bc_seqno);
		write(s, buff, strlen(buff));
	}
}

void
command_show_kbc(s)
	int s;
{
	char buff[2048];

	sprintf(buff, "Not Supported yet\n");
	write(s, buff, strlen(buff));
}


void
flush_bc()
{
	struct binding_cache *bc, *bc_nxt = NULL;

        for (bc = LIST_FIRST(&bchead); bc; bc = bc_nxt) {
		bc_nxt =  LIST_NEXT(bc, bc_entry);
		mip6_bc_delete(bc);
	}
}

static void
mip6_bc_set_refresh_timer(bc, tick)
	struct binding_cache *bc;
	int tick;
{
	remove_callout_entry(bc->bc_refresh);
	bc->bc_refresh = new_callout_entry(tick, mip6_bc_refresh_timer, (void *)bc);
}


static void
mip6_bc_stop_refresh_timer(bc)
	struct binding_cache *bc;
{

	remove_callout_entry(bc->bc_refresh);
}


void
mip6_bc_refresh_timer(arg)
	void *arg;
{
	struct binding_cache *bc = (struct binding_cache *)arg;
	time_t now = time(0);

#ifdef MIP_CN
	/* Sending BRR with backoff timer till renewing this BC  */
	send_brr(&bc->bc_myaddr, &bc->bc_hoa);
	bc->bc_refresh_count++;
#endif /* MIP_CN */

	/* lifetime is expired, let's delete it */
	if (bc->bc_expire <= now) {
		mip6_bc_delete(bc);
		return;
	}

	/* 
	 * Before next BRR, this BC's lifetime will be expired. Thus,
	 * set timer with the rest of lifetime for this BC entry.  
	 */
	now = time(0);
	if (bc->bc_expire < (now +  MIP6_BRR_INTERVAL)) 
		mip6_bc_set_refresh_timer(bc, (bc->bc_expire) - now);
	else  /* set timer with the refresh backoff interval */
		mip6_bc_set_refresh_timer(bc, MIP6_BRR_INTERVAL);
	
	return;
}

void
mipscok_bc_request(bc, command) 
	struct binding_cache *bc;
        u_char command;
{
	char buf[1024];
	int err = 0;
	struct mipm_bc_info *bcinfo;
	struct sockaddr_in6 hoa_s6, coa_s6, cn_s6;
	
	if (command < MIPM_BC_ADD || command > MIPM_BC_REMOVE)
		return;
	
	memset(&hoa_s6, 0, sizeof(hoa_s6));
	memset(&coa_s6, 0, sizeof(coa_s6));
	memset(&cn_s6, 0, sizeof(cn_s6));
	
        hoa_s6.sin6_len = coa_s6.sin6_len = 
                cn_s6.sin6_len = sizeof(struct sockaddr_in6);
        hoa_s6.sin6_family = coa_s6.sin6_family =
                cn_s6.sin6_family = AF_INET6;
        
        hoa_s6.sin6_addr = bc->bc_hoa;
        coa_s6.sin6_addr = bc->bc_coa;
        cn_s6.sin6_addr = bc->bc_myaddr;

        memset(buf, 0, sizeof(buf));
        bcinfo = (struct mipm_bc_info *)buf;

        bcinfo->mipc_msglen = sizeof(struct mipm_bc_info) 
		+ sizeof(struct sockaddr_in6) * 3;
        bcinfo->mipc_version = MIP_VERSION;
        bcinfo->mipc_type = command;
        bcinfo->mipc_seq = random();
        bcinfo->mipc_flags = bc->bc_flags;
        bcinfo->mipc_seqno = bc->bc_seqno;
        bcinfo->mipc_lifetime = bc->bc_lifetime;
#ifdef MIP_MCOA
	bcinfo->mipc_bid = bc->bc_bid;
#endif /* MIP_MCOA */

        /* bcinfo->mipc_coa_ifname xxx */
        memcpy(MIPC_HOA(bcinfo), &hoa_s6, hoa_s6.sin6_len);
        memcpy(MIPC_COA(bcinfo), &coa_s6, coa_s6.sin6_len);
        memcpy(MIPC_CNADDR(bcinfo), &cn_s6, cn_s6.sin6_len);

        err = write(mipsock, bcinfo, bcinfo->mipc_msglen);
	if (err < 0)
		perror("mipsock_bc_request:write");

	if (debug) {
		switch (command) {
		case MIPM_BC_ADD:
			syslog(LOG_INFO, "binding cache add request\n");
			break;
		case MIPM_BC_CHANGE:
			syslog(LOG_INFO, "binding cache change request\n");
			break;
		case MIPM_BC_REMOVE:
			syslog(LOG_INFO, "binding cache remove request\n");
			break;
		default:
			break;
		}
		syslog(LOG_INFO, "[BC info] HoA  %s", ip6_sprintf(&bc->bc_hoa));
		syslog(LOG_INFO, "\tCoA  %s\n", ip6_sprintf(&bc->bc_coa));
		syslog(LOG_INFO, "\tPeer %s\n", ip6_sprintf(&bc->bc_myaddr));
#ifdef MIP_MCOA
		syslog(LOG_INFO, "\tBID %d\n", bc->bc_bid);
#endif /* MIP_MCOA */

		syslog(LOG_INFO, "\tSeq %d, Lifetime %d\n", 
		       bc->bc_seqno, bc->bc_lifetime);

	}
        
        return;
}
#endif /* MIP_MN */

#ifdef MIP_MN 


/* 
 * functions for hoainfo structure 
 */
struct mip6_hoainfo *
hoainfo_insert(hoa, ifindex)
	struct in6_addr *hoa;
	u_int16_t ifindex;
{
	struct mip6_hoainfo *hoainfo = NULL;

	hoainfo = hoainfo_find_withhoa(hoa);
	if (hoainfo)
		return (hoainfo);

	hoainfo = (struct mip6_hoainfo *)malloc(sizeof(struct mip6_hoainfo)); 
	if (hoainfo == NULL)
		return (NULL);

	memset(hoainfo, 0, sizeof(*hoainfo));

	memcpy(&hoainfo->hinfo_hoa, hoa, sizeof(*hoa));
	hoainfo->hinfo_ifindex = ifindex;

	/* Binding Update List Initialization */
	LIST_INIT(&hoainfo->hinfo_bul_head);
#ifdef MIP_NEMO
	LIST_INIT(&hoainfo->hinfo_mpt_head);
#endif /* MIP_NEMO */

	LIST_INSERT_HEAD(&hoa_head, hoainfo, hinfo_entry);

	if (debug)
		syslog(LOG_INFO, "hoainfo entry (HoA %s ifindex %d) is added\n", 
		       ip6_sprintf(hoa), ifindex); 

	return (hoainfo);
};

int
hoainfo_remove(hoa) 
	struct in6_addr *hoa;
{
	struct mip6_hoainfo *hoainfo = NULL;

	hoainfo = hoainfo_find_withhoa(hoa);
	if (hoainfo == NULL)
		return (ENOENT);

	/* remove all BUL entries */
	bul_flush(hoainfo);

	LIST_REMOVE(hoainfo, hinfo_entry);
	free(hoainfo);
	hoainfo = NULL;

	return (0);
};

struct mip6_hoainfo *
hoainfo_find_withhoa(hoa)
	struct in6_addr *hoa;
{
	struct mip6_hoainfo *hoainfo = NULL;

        for (hoainfo = LIST_FIRST(&hoa_head); hoainfo;
		     hoainfo = LIST_NEXT(hoainfo, hinfo_entry)) {
		if (IN6_ARE_ADDR_EQUAL(hoa, &hoainfo->hinfo_hoa))
			return (hoainfo);
	}

	return (NULL);
};


struct mip6_hoainfo *
hoainfo_get_withdhaadid (id)
	u_int16_t id;
{
	struct mip6_hoainfo *hoainfo = NULL;

	for (hoainfo = LIST_FIRST(&hoa_head); hoainfo;
	     hoainfo = LIST_NEXT(hoainfo, hinfo_entry)) {
		if (id == hoainfo->hinfo_dhaad_id)
			return (hoainfo);
	}

	return (NULL);
};

/* 
 * functions for bul structure 
 */
struct binding_update_list *
#ifndef MIP_MCOA
bul_insert(hoainfo, peeraddr, coa, flags)
	struct mip6_hoainfo *hoainfo;
        struct in6_addr *peeraddr;
        struct in6_addr *coa;
        u_int16_t flags;
#else
bul_insert(hoainfo, peeraddr, coa, flags, bid)
	struct mip6_hoainfo *hoainfo;
        struct in6_addr *peeraddr;
        struct in6_addr *coa;
        u_int16_t flags, bid;
#endif /* MIP_MCOA */
{

	struct binding_update_list *bul;
#ifdef MIP_MCOA
	struct binding_update_list *bul2;
#endif /* MIP_MCOA */

	if (hoainfo == NULL)
		return (NULL);
	
        bul = bul_get(&hoainfo->hinfo_hoa, peeraddr);
        if (bul != NULL) {
#ifndef MIP_MCOA
		return (bul);
#else
		if (bid == 0) 
			return (bul);
		else {
			/* if primary bul is active and bul matched with bid is also active */
			bul2 = bul_mcoa_get(&hoainfo->hinfo_hoa, peeraddr, bid);
			if (bul2)
				return (bul2);
			
			bul2 = bul_create(peeraddr, coa, flags, hoainfo);
			if (bul2 == NULL)
				return (NULL);
			bul2->bul_bid = bid;
			LIST_INSERT_HEAD(&bul->bul_mcoa_head, bul2, bul_entry);
			
			if (debug)
				syslog(LOG_INFO, "insert bul %s w/ %d into hoainfo\n", 
				       ip6_sprintf(&hoainfo->hinfo_hoa), bul2->bul_bid);
			return (bul2);
		}
#endif /* MIP_MCOA */
	}

	bul = bul_create(peeraddr, coa, flags, hoainfo);
	if (bul == NULL)
		return (NULL);
	LIST_INSERT_HEAD(&hoainfo->hinfo_bul_head, bul, bul_entry);

#ifdef MIP_MCOA
	if (bid) {
		bul2 = bul_create(peeraddr, coa, flags, hoainfo);
		if (bul2 == NULL)
			return (NULL);
		bul2->bul_bid = bid;
		LIST_INSERT_HEAD(&bul->bul_mcoa_head, bul2, bul_entry);
		if (debug)
			syslog(LOG_ERR, "insert bul %s w/ %d into hoainfo\n",
			       ip6_sprintf(&hoainfo->hinfo_hoa), bul2->bul_bid);
		return (bul2);
	}
#endif /* MIP_MCOA */

	if (debug)
		syslog(LOG_ERR, "insert bul %s into hoainfo\n", 
		       ip6_sprintf(&hoainfo->hinfo_hoa));

	return (bul);
}

static struct binding_update_list *
bul_create(peeraddr, coa, flags, hoainfo) 
        struct in6_addr *peeraddr, *coa;
        u_int16_t flags;
	struct mip6_hoainfo *hoainfo;
{
	struct binding_update_list *bul = NULL;

	bul = (struct binding_update_list *)malloc(sizeof(struct binding_update_list));
	if (bul == NULL) {
		perror("malloc");
		return (NULL);
	}

	memset(bul, 0, sizeof(*bul));
	if (peeraddr)
		memcpy(&bul->bul_peeraddr, peeraddr, sizeof(*peeraddr));
	if (coa)
		memcpy(&bul->bul_coa, coa, sizeof(*coa));
	bul->bul_seqno = random();
	bul->bul_flags = flags;
	bul->bul_hoainfo = hoainfo;

#ifdef MIP_MCOA
	LIST_INIT(&bul->bul_mcoa_head);
#endif /* MIP_MCOA */

	return (bul);
}


void
bul_remove(bul)
	struct binding_update_list *bul;
{
	if (bul == NULL)
		return;
#ifdef MIP_MCOA 
	if (!LIST_EMPTY(&bul->bul_mcoa_head)) {
		struct binding_update_list *mbul, *mbuln;

		for (mbul = LIST_FIRST(&bul->bul_mcoa_head); mbul;
			     mbul = mbuln) {
			mbuln = LIST_NEXT(mbul, bul_entry);

			LIST_REMOVE(mbul, bul_entry);
			free(mbul);
			mbul = NULL;
		};
	};
#endif /* MIP_MCOA */

	LIST_REMOVE(bul, bul_entry);
	free(bul);
}

/* get BUL entry for Home Agent */
struct binding_update_list *
bul_get_homeflag(hoa)
	struct in6_addr *hoa;
{
	struct binding_update_list *bul;
	struct mip6_hoainfo *hoainfo = NULL;

	hoainfo = hoainfo_find_withhoa(hoa);
	if (hoainfo == NULL)
		return (NULL);

	if (LIST_EMPTY(&hoainfo->hinfo_bul_head))
		return (NULL);

        for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); bul;
		     bul = LIST_NEXT(bul, bul_entry)) {

		if (bul->bul_flags & IP6_MH_BU_HOME) 
			return (bul);
	}

	return (NULL);
};


#ifdef MIP_MCOA
struct binding_update_list *
bul_mcoa_get(hoa, peer, bid) 
	struct in6_addr *hoa;
	struct in6_addr *peer;
	u_int16_t bid;
{
	struct binding_update_list *bul, *mbul;
	struct mip6_hoainfo *hoainfo = NULL;

	hoainfo = hoainfo_find_withhoa(hoa);
	if (hoainfo == NULL)
		return (NULL);

	if (LIST_EMPTY(&hoainfo->hinfo_bul_head))
		return (NULL);

        for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); bul;
		     bul = LIST_NEXT(bul, bul_entry)) {

		if (IN6_ARE_ADDR_EQUAL(peer, &bul->bul_peeraddr)) {
			break;
		} 
	}

	if (bul == NULL)
		return (NULL);

	/* if bid is zero, return normal BU */
	if (bid <= 0) 
		return (bul);

	/* search mcoa bul */
	if (LIST_EMPTY(&bul->bul_mcoa_head))
		return (NULL);

        for (mbul = LIST_FIRST(&bul->bul_mcoa_head); mbul;
	     mbul = LIST_NEXT(mbul, bul_entry)) {

		if (bid && bid == mbul->bul_bid)
			return (mbul);
	}

	return (NULL);
}
#endif /* MIP_MCOA */

/* get BUL for the set of hoa and peer */
struct binding_update_list *
bul_get(hoa, peer) 
	struct in6_addr *hoa;
	struct in6_addr *peer;
{
	struct binding_update_list *bul;
	struct mip6_hoainfo *hoainfo = NULL;

	hoainfo = hoainfo_find_withhoa(hoa);
	if (hoainfo == NULL)
		return (NULL);

	if (LIST_EMPTY(&hoainfo->hinfo_bul_head))
		return (NULL);

        for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); bul;
		     bul = LIST_NEXT(bul, bul_entry)) {

		if (IN6_ARE_ADDR_EQUAL(peer, &bul->bul_peeraddr))
			return (bul);
	}

	return (NULL);
};

void
bul_flush(hoainfo)
	struct mip6_hoainfo *hoainfo;
{
	struct binding_update_list *bul, *buln;

        for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); 
		bul; bul = buln) {
		buln = LIST_NEXT(bul, bul_entry);

#ifdef TODO
		/* before removing the entry, MUST remove bu entry in the kernel */
#endif
		LIST_REMOVE(bul, bul_entry); 
		free(bul);
		bul = NULL;
	}
};


struct binding_update_list *
bul_get_nohoa(cookie, coa, peer) 
	char *cookie;
	struct in6_addr *coa;
	struct in6_addr *peer;
{
	struct mip6_hoainfo *hoainfo = NULL;
	struct binding_update_list *bul;

        for (hoainfo = LIST_FIRST(&hoa_head); hoainfo;
		     hoainfo = LIST_NEXT(hoainfo, hinfo_entry)) {

		if (LIST_EMPTY(&hoainfo->hinfo_bul_head))
			continue;

        	for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); 
			bul; bul = LIST_NEXT(bul, bul_entry)) {

			if (IN6_ARE_ADDR_EQUAL(peer, &bul->bul_peeraddr) &&
				IN6_ARE_ADDR_EQUAL(coa, &bul->bul_coa) &&
				(bcmp(cookie, &bul->bul_careof_cookie, 
					sizeof(bul->bul_careof_cookie)) == 0)) 

				return (bul);
		}
	}

	return (NULL);
};

void
command_show_bul(s)
	int s;
{
	char buff[2048];
	struct mip6_hoainfo *hoainfo = NULL;
	struct binding_update_list *bul = NULL;
        struct timeval now;

        gettimeofday(&now, NULL);

	for (hoainfo = LIST_FIRST(&hoa_head); hoainfo;
	     hoainfo = LIST_NEXT(hoainfo, hinfo_entry)) {
		
		for (bul = LIST_FIRST(&hoainfo->hinfo_bul_head); bul;
		     bul = LIST_NEXT(bul, bul_entry)) {

			sprintf(buff, "%s ", ip6_sprintf(&bul->bul_peeraddr));
#ifndef MIP_MCOA
			sprintf(buff + strlen(buff), "%s ", 
				ip6_sprintf(&hoainfo->hinfo_hoa));
#else
			if (bul->bul_bid)
				sprintf(buff + strlen(buff), "%s$%d ", 
					ip6_sprintf(&hoainfo->hinfo_hoa), bul->bul_bid);
			else
				sprintf(buff + strlen(buff), "%s ", 
					ip6_sprintf(&hoainfo->hinfo_hoa));
#endif /* MIP_MCOA */
			sprintf(buff + strlen(buff), "%s\n", 
				ip6_sprintf(&bul->bul_coa));
			write(s, buff, strlen(buff));
			
			sprintf(buff,
				"     lif=%d, ref=%d, seq=%d, %c%c%c%c%c%c, %c, ", 
				bul->bul_lifetime,
				bul->bul_refresh,
				bul->bul_seqno,
				(bul->bul_flags & IP6_MH_BU_ACK)  ? 'A' : '-',
				(bul->bul_flags & IP6_MH_BU_HOME) ? 'H' : '-',
				(bul->bul_flags & IP6_MH_BU_LLOCAL) ? 'L' : '-',
				(bul->bul_flags & IP6_MH_BU_KEYM)  ? 'K' : '-',
				(bul->bul_flags & IP6_MH_BU_ROUTER)  ? 'R' : '-',
				(bul->bul_flags & IP6_MH_BU_MCOA)  ? 'M' : '-',
				(bul->bul_state & MIP6_BUL_STATE_DISABLE) ? 'D' : '-');

			sprintf(buff + strlen(buff),
				"reg=%d, rr=%d, ret=%ld, exp=%ld\n",
				bul->bul_reg_fsm_state,
				bul->bul_rr_fsm_state,
				(bul->bul_retrans) ? 
				(bul->bul_retrans->exptime.tv_sec - now.tv_sec) : -1,
				(bul->bul_expire) ? 
				(bul->bul_expire->exptime.tv_sec - now.tv_sec) : -1);
			write(s, buff, strlen(buff));
		}
	}
} 

void
command_show_kbul(s)
	int s;
{
        struct if_bulreq bulreq;
        struct bul6info *bul6;
        int sock, i;

	char buff[2048];
	struct mip6_hoainfo *hoainfo = NULL;
	char ifname[IFNAMSIZ];

        sock = socket(AF_INET6, SOCK_DGRAM, 0);
        if (sock < 0) {
                perror("socket");
                return;
        }

	for (hoainfo = LIST_FIRST(&hoa_head); hoainfo;
	     hoainfo = LIST_NEXT(hoainfo, hinfo_entry)) {
		
		memset(&bulreq, 0, sizeof(bulreq));
		bulreq.ifbu_count = 0;
		bulreq.ifbu_len = sizeof(struct if_bulreq) + sizeof(struct bul6info) * 10;
		bulreq.ifbu_info = (struct bul6info *)malloc(sizeof(struct bul6info) * 10);
		
		memset(ifname, 0, sizeof(ifname));
		if (if_indextoname(hoainfo->hinfo_ifindex, ifname) == NULL) 
			continue;

		strncpy(bulreq.ifbu_ifname, 
			ifname, strlen(ifname));
		
		if (ioctl(sock, SIOGBULIST, &bulreq) < 0) { 
			perror("ioctl");
			syslog(LOG_INFO, "ioctl is failed for %s\n", ifname);
			free(bulreq.ifbu_info);
			close(sock);
			return;
		} 
        
		/* dump bul */
		for (i = 0; i < bulreq.ifbu_count; i ++) {
			bul6 = bulreq.ifbu_info + i * sizeof(struct bul6info);
			sprintf(buff, "%s ", ip6_sprintf(&bul6->bul_peeraddr));

#ifndef MIP_MCOA
			sprintf(buff + strlen(buff), "%s ", 
				ip6_sprintf(&bul6->bul_hoa));
#else
			if (bul6->bul_bid)
				sprintf(buff + strlen(buff), "%s$%d ", 
					ip6_sprintf(&bul6->bul_hoa), bul6->bul_bid);
			else
				sprintf(buff + strlen(buff), "%s ", 
					ip6_sprintf(&bul6->bul_hoa));
#endif /* MIP_MCOA */

			sprintf(buff + strlen(buff), "%s\n", 
				ip6_sprintf(&bul6->bul_coa));
			write(s, buff, strlen(buff));

			sprintf(buff,
				"     %s, %c%c%c%c%c%c\n", 
				if_indextoname(bul6->bul_ifindex, ifname), 
				(bul6->bul_flags & IP6_MH_BU_ACK)  ? 'A' : '-',
				(bul6->bul_flags & IP6_MH_BU_HOME) ? 'H' : '-',
				(bul6->bul_flags & IP6_MH_BU_LLOCAL) ? 'L' : '-',
				(bul6->bul_flags & IP6_MH_BU_KEYM)  ? 'K' : '-',
				(bul6->bul_flags & IP6_MH_BU_ROUTER)  ? 'R' : '-',
				(bul6->bul_flags & IP6_MH_BU_MCOA)  ? 'M' : '-');

			write(s, buff, strlen(buff));
		}
        }

	free(bulreq.ifbu_info);

	close(sock);
        
        return;
}

#endif /* MIP_MN */
