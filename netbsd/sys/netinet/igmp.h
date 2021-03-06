/*	$NetBSD: igmp.h,v 1.8 1999/11/20 00:37:58 thorpej Exp $	*/

/*
 * Copyright (c) 2002 INRIA. All rights reserved.
 *
 * Implementation of Internet Group Management Protocol, Version 3.
 * Developed by Hitoshi Asaeda, INRIA, February 2002.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of INRIA nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988 Stephen Deering.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	@(#)igmp.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IGMP_H_
#define _NETINET_IGMP_H_

/*
 * Internet Group Management Protocol (IGMP) definitions.
 *
 * MULTICAST 1.3
 */
/*
 * Implementation of Internet Group Management Protocol, Version 3.
 *
 * Developed by Hitoshi Asaeda, INRIA, February 2002.
 */

#define	IGMP_MINLEN	8

/*
 * IGMPv1/v2 packet and v3 Query packet format.
 */
struct igmp {
	u_int8_t	igmp_type;	/* version & type of IGMP message  */
	u_int8_t	igmp_code;	/* max resp code		   */
#define	IGMP_EXP(x)	(((x) >> 4) & 0x07)
#define	IGMP_MANT(x)	((x) & 0x0f)
	u_int16_t	igmp_cksum;	/* IP-style checksum		   */
	struct in_addr	igmp_group;	/* group address being specified   */
					/* (zero for general query)	   */
	u_int8_t	igmp_rtval;	/* Reserved, S Flag and QRV	   */
#define	IGMP_QRESV(x)	(((x) >> 4) & 0x0f)
#define	IGMP_SFLAG(x)	(((x) >> 3) & 0x01)
#define	IGMP_QRV(x)	((x) & 0x07)	/* querier's robustness variable   */
	u_int8_t	igmp_qqi;	/* querier's query interval (QQI)  */
	u_int16_t	igmp_numsrc;	/* number of sources (zero for general*/
					/* and group-specific queries)	   */
	struct in_addr	src[1];		/* source address list		   */
} __attribute__((__packed__));

/*
 * IGMPv3 Membership Report Message header.
 */
struct igmp_report_hdr {
	u_int8_t	igmp_type;	/* version & type of IGMP message  */
	u_int8_t	igmp_reserved1;	/* reserved (set to zero)	   */
	u_int16_t	igmp_cksum;	/* IP-style checksum		   */
	u_int16_t	igmp_reserved2;	/* reserved (set to zero)	   */
	u_int16_t	igmp_grpnum;	/* number of group record	   */
} __attribute__((__packed__));

/*
 * IGMPv3 Group Record header.
 */
struct igmp_group_record_hdr {
	u_int8_t	record_type;	/* record types for membership report */
	u_int8_t	auxlen;		/* aux data length (must be zero)  */
	u_int16_t	numsrc;		/* number of sources		   */
	struct in_addr	group;		/* group address		   */
	struct in_addr	src[1];		/* source address list		   */
} __attribute__((__packed__));

#define	IGMP_HOST_MEMBERSHIP_QUERY	0x11  /* membership query      */
#define	IGMP_v1_HOST_MEMBERSHIP_REPORT	0x12  /* v1 membership report  */
#define	IGMP_DVMRP			0x13  /* DVMRP routing message */
#define	IGMP_PIM			0x14  /* PIM routing message   */
#define	IGMP_v2_HOST_MEMBERSHIP_REPORT	0x16  /* v2 membership report  */
#define	IGMP_HOST_LEAVE_MESSAGE		0x17  /* v2 leave-group message*/
#define	IGMP_MTRACE_REPLY		0x1e  /* traceroute reply      */
#define	IGMP_MTRACE_QUERY		0x1f  /* traceroute query      */
#define	IGMP_v3_HOST_MEMBERSHIP_REPORT	0x22  /* v3 membership report  */

#define	IGMP_v3_GENERAL_QUERY		1
#define	IGMP_v3_GROUP_QUERY		2
#define	IGMP_v3_GROUP_SOURCE_QUERY	3

#define	IGMP_MAX_HOST_REPORT_DELAY	10    /* max delay for response to */
					      /* query (in seconds)	   */

#define	IGMP_TIMER_SCALE		10    /* denominator for igmp_timer */

#define IGMP_v3_QUERY_MINLEN		12

/*
 * States for the IGMP state table.
 */
#define	IGMP_DELAYING_MEMBER		1
#define	IGMP_IDLE_MEMBER		2
#define	IGMP_LAZY_MEMBER		3
#define	IGMP_SLEEPING_MEMBER		4
#define	IGMP_AWAKENING_MEMBER		5
#define	IGMP_QUERY_PENDING_MEMBER	6	/* pending General Query */
#define	IGMP_G_QUERY_PENDING_MEMBER	7	/* pending Grp-specific Query */
#define	IGMP_SG_QUERY_PENDING_MEMBER	8	/* pending Grp-Src-specific Q.*/

/*
 * Group Record Types in the IGMP Membership Report
 */
#define	MODE_IS_INCLUDE			1
#define	MODE_IS_EXCLUDE			2
#define	CHANGE_TO_INCLUDE_MODE		3
#define	CHANGE_TO_EXCLUDE_MODE		4
#define	ALLOW_NEW_SOURCES		5
#define	BLOCK_OLD_SOURCES		6

/*
 * States for IGMP router version cache.
 */
#define	IGMP_v1_ROUTER		1
#define	IGMP_v2_ROUTER		2
#define	IGMP_v3_ROUTER		3

/*
 * IGMP Query version
 */
#define	IGMP_v1_QUERY		IGMP_v1_ROUTER
#define	IGMP_v2_QUERY		IGMP_v2_ROUTER
#define	IGMP_v3_QUERY		IGMP_v3_ROUTER

#endif /* _NETINET_IGMP_H_ */
