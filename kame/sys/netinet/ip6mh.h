/*	$KAME: ip6mh.h,v 1.1 2004/02/13 02:52:09 keiichi Exp $	*/

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

#ifndef _NETINET_IP6MH_H_
#define _NETINET_IP6MH_H_

/* Mobility header */
struct ip6_mh {
	u_int8_t  ip6mh_proto;	  /* following payload protocol (for PG) */
	u_int8_t  ip6mh_len;	  /* length in units of 8 octets */
	u_int8_t  ip6mh_type;	  /* message type */
	u_int8_t  ip6mh_reserved;
	u_int16_t ip6mh_cksum;    /* sum of IPv6 pseudo-header and MH */
	/* followed by type specific data */
} __attribute__((__packed__));
#ifdef _KERNEL
/* Mobility header minimum length */
#define IP6M_MINLEN	8
#endif /* _KERNEL */

/* Mobility header message types */
#define IP6_MH_TYPE_BRR		0
#define IP6_MH_TYPE_HOTI	1
#define IP6_MH_TYPE_COTI	2
#define IP6_MH_TYPE_HOT		3
#define IP6_MH_TYPE_COT		4
#define IP6_MH_TYPE_BU		5
#define IP6_MH_TYPE_BACK	6
#define IP6_MH_TYPE_BERROR	7
#define IP6_MH_TYPE_MAX		7

/* Binding Refresh Request (BRR) message */
struct ip6_mh_binding_request {
	struct ip6_mh ip6mhbr_hdr;
	u_int16_t     ip6mhbr_reserved;
	/* followed by mobility options */
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6mhbr_proto ip6mhbr_hdr.ip6mh_proto
#define ip6mhbr_len ip6mhbr_hdr.ip6mh_len
#define ip6mhbr_type ip6mhbr_hdr.ip6mh_type
#define ip6mhbr_reserved0 ip6mhbr_hdr.ip6mh_reserved
#define ip6mhbr_cksum ip6mhbr_hdr.ip6mh_cksum
#endif /* _KERNEL */

/* Home Test Init (HoTI) message */
struct ip6_mh_home_test_init {
	struct ip6_mh ip6mhhti_hdr;
	u_int16_t     ip6mhhti_reserved;
	union {
		u_int8_t  __cookie8[8];
		u_int32_t __cookie32[2];
	} __ip6mhhti_cookie;
	/* followed by mobility options */
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6mhhti_proto ip6mhhti_hdr.ip6mh_proto
#define ip6mhhti_len ip6mhhti_hdr.ip6mh_len
#define ip6mhhti_type ip6mhhti_hdr.ip6mh_type
#define ip6mhhti_reserved0 ip6mhhti_hdr.ip6mh_reserved
#define ip6mhhti_cksum ip6mhhti_hdr.ip6mh_cksum
#define ip6mhhti_cookie8 __ip6mhhti_cookie.__cookie8
#endif /* _KERNEL */
#define ip6mhhti_cookie __ip6mhhti_cookie.__cookie32

/* Care-of Test Init (CoTI) message */
struct ip6_mh_careof_test_init {
	struct ip6_mh ip6mhcti_hdr;
	u_int16_t     ip6mhcti_reserved;
	union {
		u_int8_t  __cookie8[8];
		u_int32_t __cookie32[2];
	} __ip6mhcti_cookie;
	/* followed by mobility options */
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6mhcti_proto ip6mhcti_hdr.ip6mh_proto
#define ip6mhcti_len ip6mhcti_hdr.ip6mh_len
#define ip6mhcti_type ip6mhcti_hdr.ip6mh_type
#define ip6mhcti_reserved0 ip6mhcti_hdr.ip6mh_reserved
#define ip6mhcti_cksum ip6mhcti_hdr.ip6mh_cksum
#define ip6mhcti_cookie8 __ip6mhcti_cookie.__cookie8
#endif /* _KERNEL */
#define ip6mhcti_cookie __ip6mhcti_cookie.__cookie32

/* Home Test (HoT) message */
struct ip6_mh_home_test {
	struct ip6_mh ip6mhht_hdr;
	u_int16_t     ip6mhht_nonce_index; /* idx of the CN nonce list array */
	union {
		u_int8_t  __cookie8[8];
		u_int32_t __cookie32[2];
	} __ip6mhht_cookie;
	union {
		u_int8_t  __keygen8[8];
		u_int32_t __keygen32[2];
	} __ip6mhht_keygen;
	/* followed by mobility options */
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6mhht_proto ip6mhht_hdr.ip6mh_proto
#define ip6mhht_len ip6mhht_hdr.ip6mh_len
#define ip6mhht_type ip6mhht_hdr.ip6mh_type
#define ip6mhht_reserved0 ip6mhht_hdr.ip6mh_reserved
#define ip6mhht_cksum ip6mhht_hdr.ip6mh_cksum
#define ip6mhht_cookie8 __ip6mhht_cookie.__cookie8
#define ip6mhht_keygen8 __ip6mhht_keygen.__keygen8
#endif /* _KERNEL */
#define ip6mhht_cookie __ip6mhht_cookie.__cookie32
#define ip6mhht_keygen __ip6mhht_keygen.__keygen32

/* Care-of Test (CoT) message */
struct ip6_mh_careof_test {
	struct ip6_mh ip6mhct_hdr;
	u_int16_t     ip6mhct_nonce_index; /* idx of the CN nonce list array */
	union {
		u_int8_t  __cookie8[8];
		u_int32_t __cookie32[2];
	} __ip6mhct_cookie;
	union {
		u_int8_t  __keygen8[8];
		u_int32_t __keygen32[2];
	} __ip6mhct_keygen;
	/* followed by mobility options */
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6mhct_proto ip6mhct_hdr.ip6mh_proto
#define ip6mhct_len ip6mhct_hdr.ip6mh_len
#define ip6mhct_type ip6mhct_hdr.ip6mh_type
#define ip6mhct_reserved0 ip6mhct_hdr.ip6mh_reserved
#define ip6mhct_cksum ip6mhct_hdr.ip6mh_cksum
#define ip6mhct_cookie8 __ip6mhct_cookie.__cookie8
#define ip6mhct_keygen8 __ip6mhct_keygen.__keygen8
#endif /* _KERNEL */
#define ip6mhct_cookie __ip6mhct_cookie.__cookie32
#define ip6mhct_keygen __ip6mhct_keygen.__keygen32

/* Binding Update (BU) message */
struct ip6_mh_binding_update {
	struct ip6_mh ip6mhbu_hdr;
	u_int16_t     ip6mhbu_seqno;	/* sequence number */
	u_int16_t     ip6mhbu_flags;	/* IP6MU_* flags */
	u_int16_t     ip6mhbu_lifetime;	/* in units of 4 seconds */
	/* followed by mobility options */
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6mhbu_proto ip6mhbu_hdr.ip6mh_proto
#define ip6mhbu_len ip6mhbu_hdr.ip6mh_len
#define ip6mhbu_type ip6mhbu_hdr.ip6mh_type
#define ip6mhbu_reserved0 ip6mhbu_hdr.ip6mh_reserved
#define ip6mhbu_cksum ip6mhbu_hdr.ip6mh_cksum
#endif /* _KERNEL */

/* Binding Update flags */
#if BYTE_ORDER == BIG_ENDIAN
#define IP6MU_ACK	0x8000	/* request a binding ack */
#define IP6MU_HOME	0x4000	/* home registration */
#define IP6MU_LINK	0x2000	/* link-local address compatibility */
#define IP6MU_KEY	0x1000	/* key management mobility compatibility */
#define IP6MU_CLONED	0x0100	/* KAME: internal use */
#endif /* BIG_ENDIAN */
#if BYTE_ORDER == LITTLE_ENDIAN
#define IP6MU_ACK	0x0080	/* request a binding ack */
#define IP6MU_HOME	0x0040	/* home registration */
#define IP6MU_LINK	0x0020	/* link-local address compatibility */
#define IP6MU_KEY	0x0010	/* key management mobility compatibility */
#define IP6MU_CLONED	0x0001	/* KAME: internal use */
#endif /* LITTLE_ENDIAN */

/* Binding Acknowledgement (BA) message */
struct ip6_mh_binding_ack {
	struct ip6_mh ip6mhba_hdr;
	u_int8_t      ip6mhba_status;	/* status code */
	u_int8_t      ip6mhba_flags;
	u_int16_t     ip6mhba_seqno;	/* sequence number */
	u_int16_t     ip6mhba_lifetime;	/* in units of 4 seconds */
	/* followed by mobility options */
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6mhba_proto ip6mhba_hdr.ip6mh_proto
#define ip6mhba_len ip6mhba_hdr.ip6mh_len
#define ip6mhba_type ip6mhba_hdr.ip6mh_type
#define ip6mhba_reserved0 ip6mhba_hdr.ip6mh_reserved
#define ip6mhba_cksum ip6mhba_hdr.ip6mh_cksum
#endif /* _KERNEL */

/* Binding Acknowledgement Flags */
#define IP6_MH_BA_KEYM	0x80	/* key management mobility */

/* Binding Ack status codes */
#define IP6_MH_BAS_ACCEPTED		0   /* Binding Update accepted */
#define IP6_MH_BAS_PRFX_DISCOV		1   /* Accepted, but prefix discovery required */
#define IP6_MH_BAS_ERRORBASE		128 /* ERROR BASE */
#define IP6_MH_BAS_UNSPECIFIED		128 /* Reason unspecified */
#define IP6_MH_BAS_PROHIBIT		129 /* Administratively prohibited */
#define IP6_MH_BAS_INSUFFICIENT		130 /* Insufficient resources */
#define IP6_MH_BAS_HA_NOT_SUPPORTED	131 /* Home registration not supported */
#define IP6_MH_BAS_NOT_HOME_SUBNET	132 /* Not home subnet */
#define IP6_MH_BAS_NOT_HA		133 /* Not home agent for this mobile node */
#define IP6_MH_BAS_DAD_FAILED		134 /* Duplicate Address Detection failed */
#define IP6_MH_BAS_SEQNO_BAD		135 /* Sequence number out of window */
#define IP6_MH_BAS_HOME_NI_EXPIRED	136 /* Expired Home Nonce Index */
#define IP6_MH_BAS_COA_NI_EXPIRED	137 /* Expired Care-of Nonce Index */
#define IP6_MH_BAS_NI_EXPIRED		138 /* Expired Nonces */
#define IP6_MH_BAS_REG_NOT_ALLOWED	139 /* Registration type change disallowed */

/* Binding Error (BE) message */
struct ip6_mh_binding_error {
	struct ip6_mh   ip6mhbe_hdr;
	u_int8_t        ip6mhbe_status;		/* status code */
	u_int8_t        ip6mhbe_reserved;
	struct in6_addr ip6mhbe_homeaddr;
	/* followed by mobility options */
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6mhbe_proto ip6mhbe_hdr.ip6mh_proto
#define ip6mhbe_len ip6mhbe_hdr.ip6mh_len
#define ip6mhbe_type ip6mhbe_hdr.ip6mh_type
#define ip6mhbe_reserved0 ip6mhbe_hdr.ip6mh_reserved
#define ip6mhbe_cksum ip6mhbe_hdr.ip6mh_cksum
#endif /* _KERNEL */

/* Binding Error status codes */
#define IP6_MH_BES_UNKNOWN_HAO		1
#define IP6_MH_BES_UNKNOWN_MH		2

/* Mobility options */
struct ip6_mh_opt {
	u_int8_t ip6mhopt_type;
	u_int8_t ip6mhopt_len;
	/* followed by option data */
} __attribute__((__packed__));

/* Mobility option type */
#define IP6_MHOPT_PAD1		0	/* Pad1 */
#define IP6_MHOPT_PADN		1	/* PadN */
#define IP6_MHOPT_BREFRESH	2	/* Binding Refresh Advice */
#define IP6_MHOPT_ALTCOA	3	/* Alternate Care-of Address */
#define IP6_MHOPT_NONCEID	4	/* Nonce Indices */
#define IP6_MHOPT_BAUTH		5	/* Binding Authorization Data */

/* Binding Refresh Advice */
struct ip6_mh_opt_refresh_advice {
	u_int8_t ip6mora_type;
	u_int8_t ip6mora_len;
	u_int8_t ip6mora_interval[2];	/* Refresh Interval (units of 4 sec) */
} __attribute__((__packed__));

/* Alternate Care-of Address */
struct ip6_mh_opt_altcoa {
	u_int8_t ip6moa_type;
	u_int8_t ip6moa_len;
	u_int8_t ip6moa_addr[16];	/* Alternate Care-of Address */
} __attribute__((__packed__));

/* Nonce Indices */
struct ip6_mh_opt_nonce_index {
	u_int8_t ip6moni_type;
	u_int8_t ip6moni_len;
	union {
		u_int8_t __nonce8[2];
		u_int16_t __nonce16;
	} __ip6moni_home_nonce;
	union {
		u_int8_t __nonce8[2];
		u_int16_t __nonce16;
	} __ip6moni_coa_nonce;
} __attribute__((__packed__));
#ifdef _KERNEL
#define ip6moni_home_nonce8 __ip6moni_home_nonce.__nonce8
#define ip6moni_coa_nonce8 __ip6moni_coa_nonce.__nonce8
#endif /* _KERNEL */
#define ip6moni_home_nonce __ip6moni_home_nonce.__nonce16
#define ip6moni_coa_nonce __ip6moni_coa_nonce.__nonce16

/* Binding Authorization Data */
struct ip6_mh_opt_auth_data {
	u_int8_t ip6moad_type;
	u_int8_t ip6moad_len;
	/* followed by authenticator data */
} __attribute__((__packed__));
#ifdef _KERNEL
#define IP6MOPT_AUTHDATA_SIZE (sizeof(struct ip6_mh_opt_auth_data) + MIP6_AUTHENTICATOR_LEN)
#endif /* _KERNEL */

#endif /* not _NETINET_IP6MH_H_ */
