/*	$KAME: dhcp6.h,v 1.53 2004/11/28 11:59:37 jinmei Exp $	*/
/*
 * Copyright (C) 1998 and 1999 WIDE Project.
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

#ifndef __DHCP6_H_DEFINED
#define __DHCP6_H_DEFINED

/* Error Values */
#define DH6ERR_FAILURE		16
#define DH6ERR_AUTHFAIL		17
#define DH6ERR_POORLYFORMED	18
#define DH6ERR_UNAVAIL		19
#define DH6ERR_OPTUNAVAIL	20

/* Message type */
#define DH6_SOLICIT	1
#define DH6_ADVERTISE	2
#define DH6_REQUEST	3
#define DH6_CONFIRM	4
#define DH6_RENEW	5
#define DH6_REBIND	6
#define DH6_REPLY	7
#define DH6_RELEASE	8
#define DH6_DECLINE	9
#define DH6_RECONFIGURE	10
#define DH6_INFORM_REQ	11
#define DH6_RELAY_FORW	12
#define DH6_RELAY_REPLY	13

/* Predefined addresses */
#define DH6ADDR_ALLAGENT	"ff02::1:2"
#define DH6ADDR_ALLSERVER	"ff05::1:3"
#define DH6PORT_DOWNSTREAM	"546"
#define DH6PORT_UPSTREAM	"547"

/* Protocol constants */

/* timer parameters (msec, unless explicitly commented) */
#define SOL_MAX_DELAY	1000
#define SOL_TIMEOUT	1000
#define SOL_MAX_RT	120000
#define INF_TIMEOUT	1000
#define INF_MAX_RT	120000
#define REQ_TIMEOUT	1000
#define REQ_MAX_RT	30000
#define REQ_MAX_RC	10	/* Max Request retry attempts */
#define REN_TIMEOUT	10000	/* 10secs */
#define REN_MAX_RT	600000	/* 600secs */
#define REB_TIMEOUT	10000	/* 10secs */
#define REB_MAX_RT	600000	/* 600secs */
#define REL_TIMEOUT	1000	/* 1 sec */
#define REL_MAX_RC	5

#define DHCP6_DURATITION_INFINITE 0xffffffff
#define DHCP6_DURATITION_MIN 30

#define DHCP6_RELAY_MULTICAST_HOPS 32
#define DHCP6_RELAY_HOP_COUNT_LIMIT 32

#define DHCP6_IRT_DEFAULT 86400	/* 1 day */
#define DHCP6_IRT_MINIMUM 600

/* DUID: DHCP unique Identifier */
struct duid {
	size_t duid_len;	/* length */
	char *duid_id;		/* variable length ID value (must be opaque) */
};

struct dhcp6_vbuf {		/* generic variable length buffer */
	int dv_len;
	caddr_t dv_buf;
};

/* option information */
struct dhcp6_ia {		/* identity association */
	u_int32_t iaid;
	u_int32_t t1;
	u_int32_t t2;
};

struct dhcp6_prefix {
	u_int32_t pltime;
	u_int32_t vltime;
	int plen;
	struct in6_addr addr;
};

/* Internal data structure */
typedef enum { DHCP6_LISTVAL_NUM = 1,
	       DHCP6_LISTVAL_STCODE, DHCP6_LISTVAL_ADDR6,
	       DHCP6_LISTVAL_IAPD, DHCP6_LISTVAL_PREFIX6,
	       DHCP6_LISTVAL_VBUF
} dhcp6_listval_type_t;
TAILQ_HEAD(dhcp6_list, dhcp6_listval);
struct dhcp6_listval {
	TAILQ_ENTRY(dhcp6_listval) link;

	dhcp6_listval_type_t type;

	union {
		int uv_num;
		u_int16_t uv_num16;
		struct in6_addr uv_addr6;
		struct dhcp6_prefix uv_prefix6;
		struct dhcp6_ia uv_ia;
		struct dhcp6_vbuf uv_vbuf;
	} uv;

	struct dhcp6_list sublist;
};
#define val_num uv.uv_num
#define val_num16 uv.uv_num16
#define val_addr6 uv.uv_addr6
#define val_ia uv.uv_ia
#define val_prefix6 uv.uv_prefix6
#define val_vbuf uv.uv_vbuf

struct dhcp6_optinfo {
	struct duid clientID;	/* DUID */
	struct duid serverID;	/* DUID */

	int rapidcommit;	/* bool */
	int pref;		/* server preference */
	int32_t elapsed_time;	/* elapsed time (from client to server only) */
	int64_t refreshtime;	/* info refresh time for stateless options */

	struct dhcp6_list iapd_list; /* list of IA_PD */
	struct dhcp6_list reqopt_list; /* options in option request */
	struct dhcp6_list stcode_list; /* status code */
	struct dhcp6_list sip_list; /* SIP server list */
	struct dhcp6_list sipname_list; /* SIP server domain list */
	struct dhcp6_list dns_list; /* DNS server list */
	struct dhcp6_list dnsname_list; /* Domain Search list */
	struct dhcp6_list ntp_list; /* NTP server list */
	struct dhcp6_list prefix_list; /* prefix list */

	struct dhcp6_vbuf relay_msg; /* relay message */
#define relaymsg_len relay_msg.dv_len
#define relaymsg_msg relay_msg.dv_buf

	struct dhcp6_vbuf ifidopt; /* Interface-id */
#define ifidopt_len ifidopt.dv_len
#define ifidopt_id ifidopt.dv_buf

	u_int authflags;
#define DHCP6OPT_AUTHFLAG_NOINFO	0x1
	int authproto;
	int authalgorithm;
	int authrdm;
	/* the followings are effective only when NOINFO is unset */
	u_int64_t authrd;
	union {
		struct {
			u_int32_t keyid;
			struct dhcp6_vbuf realm;
			int offset; /* offset to the HMAC field */
		} aiu_delayed;
		struct {
			int type;
			int offset; /* offset to the HMAC field */
			char val[16]; /* key value */
		} aiu_reconfig;
	} authinfo;
#define delayedauth_keyid authinfo.aiu_delayed.keyid
#define delayedauth_realmlen authinfo.aiu_delayed.realm.dv_len
#define delayedauth_realmval authinfo.aiu_delayed.realm.dv_buf
#define delayedauth_offset authinfo.aiu_delayed.offset
#define reconfigauth_type authinfo.aiu_reconfig.type
#define reconfigauth_offset authinfo.aiu_reconfig.offset
#define reconfigauth_val authinfo.aiu_reconfig.val
};

/* DHCP6 base packet format */
struct dhcp6 {
	union {
		u_int8_t m;
		u_int32_t x;
	} dh6_msgtypexid;
	/* options follow */
} __attribute__ ((__packed__));
#define dh6_msgtype	dh6_msgtypexid.m
#define dh6_xid		dh6_msgtypexid.x
#define DH6_XIDMASK	0x00ffffff

/* DHCPv6 relay messages */
struct dhcp6_relay {
	u_int8_t dh6relay_msgtype;
	u_int8_t dh6relay_hcnt;
	struct in6_addr dh6relay_linkaddr; /* XXX: badly aligned */
	struct in6_addr dh6relay_peeraddr; /* ditto */
	/* options follow */
} __attribute__ ((__packed__));

/* options */
#define DH6OPT_CLIENTID	1
#define DH6OPT_SERVERID	2
#define DH6OPT_IA 3
#define DH6OPT_IA_TMP 4
#define DH6OPT_IADDR 5
#define DH6OPT_ORO 6
#define DH6OPT_PREFERENCE 7
#  define DH6OPT_PREF_UNDEF -1
#  define DH6OPT_PREF_MAX 255
#define DH6OPT_ELAPSED_TIME 8
#  define DH6OPT_ELAPSED_TIME_UNDEF -1
#define DH6OPT_RELAY_MSG 9
/* #define DH6OPT_SERVER_MSG 10: deprecated */
#define DH6OPT_AUTH 11
#  define DH6OPT_AUTH_PROTO_DELAYED 2
#  define DH6OPT_AUTH_RRECONFIGURE 3
#  define DH6OPT_AUTH_ALG_HMACMD5 1
#define DH6OPT_UNICAST 12
#define DH6OPT_STATUS_CODE 13
#  define DH6OPT_STCODE_SUCCESS 0
#  define DH6OPT_STCODE_UNSPECFAIL 1
#  define DH6OPT_STCODE_NOADDRAVAIL 2
#  define DH6OPT_STCODE_NOBINDING 3
#  define DH6OPT_STCODE_NOTONLINK 4
#  define DH6OPT_STCODE_USEMULTICAST 5
#  define DH6OPT_STCODE_NOPREFIXAVAIL 6

#define DH6OPT_RAPID_COMMIT 14
#define DH6OPT_USER_CLASS 15
#define DH6OPT_VENDOR_CLASS 16
#define DH6OPT_VENDOR_OPTS 17
#define DH6OPT_INTERFACE_ID 18
#define DH6OPT_RECONF_MSG 19

/*
 * The following values have been assigned by IANA very recently.  In order to
 * ensure interoperability with prior experimental implementation, we still
 * provide a way to configure these values.  It is highly recommended to use
 * the standard values whenever possible, of course.
 */
#ifndef DH6OPT_SIP_SERVER_D
#define DH6OPT_SIP_SERVER_D 21
#endif
#ifndef DH6OPT_SIP_SERVER_A
#define DH6OPT_SIP_SERVER_A 22
#endif
#ifndef DH6OPT_DNS
#define DH6OPT_DNS 23
#endif
#ifndef DH6OPT_DNSNAME
#define DH6OPT_DNSNAME 24
#endif
#ifndef DH6OPT_IA_PD
#define DH6OPT_IA_PD 25
#endif
#ifndef DH6OPT_IA_PD_PREFIX
#define DH6OPT_IA_PD_PREFIX 26
#endif

/*
 * The old prefix delegation option used in the service specification document
 * (200206xx version) by NTT Communications.
 */
#define DH6OPT_PREFIX_DELEGATION CONF_DH6OPT_PREFIX_DELEGATION
#define DH6OPT_PREFIX_INFORMATION CONF_DH6OPT_PREFIX_INFORMATION
#define DH6OPT_PREFIX_REQUEST CONF_DH6OPT_PREFIX_REQUEST

/* The followings are KAME specific. */
#if CONF_DH6OPT_NTP > 0
#define USE_DH6OPT_NTP
#endif
#define DH6OPT_NTP CONF_DH6OPT_NTP
#if CONF_DH6OPT_REFRESHTIME > 0
#define USE_DH6OPT_REFRESHTIME
#endif
#define DH6OPT_REFRESHTIME CONF_DH6OPT_REFRESHTIME
#  define DH6OPT_REFRESHTIME_UNDEF -1

struct dhcp6opt {
	u_int16_t dh6opt_type;
	u_int16_t dh6opt_len;
	/* type-dependent data follows */
} __attribute__ ((__packed__));

/* DUID type 1 */
struct dhcp6opt_duid_type1 {
	u_int16_t dh6_duid1_type;
	u_int16_t dh6_duid1_hwtype;
	u_int32_t dh6_duid1_time;
	/* link-layer address follows */
} __attribute__ ((__packed__));

/* Status Code */
struct dhcp6opt_stcode {
	u_int16_t dh6_stcode_type;
	u_int16_t dh6_stcode_len;
	u_int16_t dh6_stcode_code;
} __attribute__ ((__packed__));

/* Prefix Information */
struct dhcp6opt_prefix_info {
	u_int16_t dh6_pi_type;
	u_int16_t dh6_pi_len;
	u_int32_t dh6_pi_duration;
	u_int8_t dh6_pi_plen;
	struct in6_addr dh6_pi_paddr;
} __attribute__ ((__packed__));

/*
 * General format of Identity Association.
 * This format applies to Prefix Delegation (IA_PD) and Non-temporary Addresses
 * (IA_NA), while our implementation only supports the former.
 */
struct dhcp6opt_ia {
	u_int16_t dh6_ia_type;
	u_int16_t dh6_ia_len;
	u_int32_t dh6_ia_iaid;
	u_int32_t dh6_ia_t1;
	u_int32_t dh6_ia_t2;
	/* sub options follow */
} __attribute__ ((__packed__));

/* IA_PD Prefix */
struct dhcp6opt_ia_pd_prefix {
	u_int16_t dh6_iapd_prefix_type;
	u_int16_t dh6_iapd_prefix_len;
	u_int32_t dh6_iapd_prefix_preferred_time;
	u_int32_t dh6_iapd_prefix_valid_time;
	u_int8_t dh6_iapd_prefix_prefix_len;
	struct in6_addr dh6_iapd_prefix_prefix_addr;
} __attribute__ ((__packed__));

/* Authentication */
struct dhcp6opt_auth {
	u_int16_t dh6_auth_type;
	u_int16_t dh6_auth_len;
	u_int8_t dh6_auth_proto;
	u_int8_t dh6_auth_alg;
	u_int8_t dh6_auth_rdm;
	u_int8_t dh6_auth_rdinfo[8];
	/* authentication information follows */
} __attribute__ ((__packed__));

enum { DHCP6_AUTHPROTO_UNDEF = -1, DHCP6_AUTHPROTO_DELAYED = 2,
       DHCP6_AUTHPROTO_RECONFIG = 3 };
enum { DHCP6_AUTHALG_UNDEF = -1, DHCP6_AUTHALG_HMACMD5 = 1 };
enum { DHCP6_AUTHRDM_UNDEF = -1, DHCP6_AUTHRDM_MONOCOUNTER = 0 };

#endif /*__DHCP6_H_DEFINED*/
