/*	$KAME: dccp.h,v 1.3 2003/10/18 08:16:17 itojun Exp $	*/

/*
 * Copyright (c) 2003 Joacim H�ggmark, Magnus Erixzon, Nils-Erik Mattsson 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Id: dccp.h,v 1.13 2003/07/31 11:14:41 joahag-9 Exp
 */

#ifndef _NETINET_DCCP_H_
#define _NETINET_DCCP_H_

/*
 * DCCP protocol header
 * draft-ietf-dccp-spec-01.txt 
 */
struct dccphdr {
	u_short		dh_sport;	/* source port */
	u_short		dh_dport;	/* destination port */
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int32_t	dh_res:4,	/* Reserved */
			dh_type:4,	/* Type of message */
			dh_seq:24;	/* Sequence number */
#else
	u_int32_t	dh_type:4,
			dh_res:4,
			dh_seq:24;	
#endif

	u_int8_t	dh_off;		/* Data offset */
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int8_t	dh_cslen:4,	/* Checksum Length */
			dh_ndp:4;	/* Number of non data packets */
#else
	u_int8_t	dh_ndp:4,
			dh_cslen:4;
#endif

	u_short		dh_sum;		/* Checksum */
};

struct dccp_requesthdr {
	u_int32_t	drqh_sname;	/* Service Name */
};

struct dccp_ackhdr {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int32_t	dah_res:8,	/* Reserved */
			dah_ack:24;	/* Acknowledgement number */
#else
	u_int32_t	dah_ack:24,
			dah_res:8;
#endif
};


struct dccp_resethdr {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int32_t	drth_res:8,	/* Reserved */
			drth_ack:24;	/* Acknowledgement number */
#else
	u_int32_t	drth_ack:24,
			drth_res:8;
#endif
	u_int8_t	drth_reason;	/* Reason */
	u_int8_t	drth_data1;	/* Data 1 */
	u_int8_t	drth_data2;	/* Data 2 */
	u_int8_t	drth_data3;	/* Data 3 */
};

#define DCCP_TYPE_REQUEST	0
#define DCCP_TYPE_RESPONSE	1
#define DCCP_TYPE_DATA		2
#define DCCP_TYPE_ACK		3
#define DCCP_TYPE_DATAACK	4
#define DCCP_TYPE_CLOSEREQ	5
#define DCCP_TYPE_CLOSE		6
#define DCCP_TYPE_RESET		7
#define DCCP_TYPE_MOVE		8

#define DCCP_FEATURE_CC		1
#define DCCP_FEATURE_ECN	2
#define DCCP_FEATURE_ACKRATIO	3
#define DCCP_FEATURE_ACKVECTOR	4
#define DCCP_FEATURE_MOBILITY	5
#define DCCP_FEATURE_LOSSWINDOW	6
#define DCCP_FEATURE_CONN_NONCE	8
#define DCCP_FEATURE_IDENTREG	7

#define DCCP_OPT_PADDING	0
#define DCCP_OPT_DATA_DISCARD	1
#define DCCP_OPT_SLOW_RECV	2
#define DCCP_OPT_BUF_CLOSED	3
#define DCCP_OPT_IGNORED	32
#define DCCP_OPT_CHANGE		33
#define DCCP_OPT_PREFER		34
#define DCCP_OPT_CONFIRM	35
#define DCCP_OPT_INIT_COOKIE	36
#define DCCP_OPT_ACK_VECTOR0	37
#define DCCP_OPT_ACK_VECTOR1	38
#define DCCP_OPT_RECV_BUF_DROPS 39
#define DCCP_OPT_TIMESTAMP	40
#define DCCP_OPT_TIMESTAMP_ECHO 41
#define DCCP_OPT_IDENT		42
#define DCCP_OPT_CHALLANGE	44

#define DCCP_REASON_UNSPEC	0
#define DCCP_REASON_CLOSED	1
#define DCCP_REASON_INVALID	2
#define DCCP_REASON_OPTION_ERR	3
#define DCCP_REASON_FEA_ERR	4
#define DCCP_REASON_CONN_REF	5
#define DCCP_REASON_BAD_SNAME	6
#define DCCP_REASON_BAD_COOKIE	7
#define DCCP_REASON_INV_MOVE	8
#define DCCP_REASON_UNANSW_CH	10
#define DCCP_REASON_FRUITLESS_NEG	11

#define DCCP_CCID		0x01
#define DCCP_CSLEN		0x02
#define DCCP_TFRC_AVGPSIZE	0x04
#define DCCP_MAXSEG		0x08

#define DCCP_NDP_LIMIT          16
#define DCCP_SEQ_NUM_LIMIT      16777216
#define DCCP_MAX_OPTIONS	32
#define DCCP_CONNECT_TIMER	(75 * hz)
#define DCCP_CLOSE_TIMER	(75 * hz)
#define DCCP_TIMEWAIT_TIMER	(60 * hz)
#endif
