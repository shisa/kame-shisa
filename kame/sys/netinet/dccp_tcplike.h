/*	$KAME: dccp_tcplike.h,v 1.8 2003/11/18 04:55:43 ono Exp $	*/

/*
 * Copyright (c) 2003 Magnus Erixzon
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
 */
/*
 * Headerfile for TCP-like congestion control for DCCP
 */

#ifndef _NETINET_DCCP_TCPLIKE_H_
#define _NETINET_DCCP_TCPLIKE_H_

/* 
 * TCPlike sender 
 */

/* Parameter to decide when a packet is considered lost */
#define TCPLIKE_NUMDUPACK 3
/* Upperbound timeout value */
#define TIMEOUT_UBOUND	(30 * hz)
#define TCPLIKE_MIN_RTT	(hz >> 3)
#define TCPLIKE_INITIAL_CWND 3
#define TCPLIKE_INITIAL_CWNDVECTOR 512

/* TCPlike sender congestion control block (ccb) */
struct tcplike_send_ccb
{
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	struct mtx mutex;
#endif
	struct dccpcb *pcb; /* Pointer to associated dccpcb */
	u_int32_t cwnd; /* congestion window */
	u_int32_t ssthresh;
	u_int32_t oldcwnd_ts; /* old cwnd tail seqnr */
	
	u_int16_t rtt; /* estimated round trip-time */
	u_int16_t rto; /* Timeout value */
	u_int16_t rtt_d;
	
	int16_t outstanding; /* Number of unacked packets sent */
	u_int16_t rcvr_ackratio; /* Receiver ack ratio */

	u_int16_t acked_in_win; /* No of acked packets in the window */
	u_int8_t acked_windows; /* No of acked windows with no lost Acks */

	u_int32_t ack_last; /* Last ok Ack packet */
	u_int32_t ack_miss; /* oldest missing Ack packet */

#ifdef __OpenBSD__	
	struct timeout free_timer;
	struct timeout rto_timer;
#else
	struct callout free_timer;
	struct callout rto_timer;
#endif
	u_int rto_timer_callout;

	u_char *cwndvector;  /* 2 bits per packet */
	u_char *cv_hp;  /* head ptr for cwndvector */
	u_int16_t cv_size;
	u_int32_t cv_hs, cv_ts; /* lowest/highest seq no in cwndvector */

	u_int8_t sample_rtt;
	u_int32_t timestamp;

	u_int32_t rcvd_ack, lost_ack;
};

#ifdef _KERNEL

/* Functions declared in struct dccp_cc_sw */

/*
 * Initialises the sender side
 * args: pcb  - pointer to dccpcb of associated connection
 * returns: pointer to a tcplike_send_ccb struct on success, otherwise 0
 */ 
void *tcplike_send_init(struct dccpcb *); 

/*
 * Free the sender side
 * args: ccb - ccb of sender
 */
void tcplike_send_free(void *);

/*
 * Ask TCPlike wheter one can send a packet or not 
 * args: ccb  -  ccb block for current connection
 * returns: 0 if ok, else <> 0.
 */ 
int tcplike_send_packet(void *, long);
void tcplike_send_packet_sent(void *, int, long);

/*
 * Notify that an ack package was received
 * args: ccb  -  ccb block for current connection
 */
void tcplike_send_packet_recv(void *, char *, int);

#endif

/* 
 * TFRC Receiver 
 */

struct ack_list
{
	u_int32_t localseq, ackthru;
	struct ack_list *next;
};

/* TCPlike receiver congestion control block (ccb) */
struct tcplike_recv_ccb {
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	struct mtx mutex;
#endif
	struct dccpcb *pcb;               /* Pointer to associated dccpcb */
	/* No ack ratio or vector here. it's a global feature */
	struct ack_list *av_list;
	u_int16_t unacked; /* no of unacked packets */
#ifdef __OpenBSD__
	struct timeout free_timer;
#else
	struct callout free_timer;
#endif
};

#ifdef _KERNEL

/* Functions declared in struct dccp_cc_sw */

/*
 * Initialises the receiver side
 * args: pcb  -  pointer to dccpcb of associated connection
 * returns: pointer to a tcplike_recv_ccb struct on success, otherwise 0
 */ 
void *tcplike_recv_init(struct dccpcb *); 

/*
 * Free the receiver side
 * args: ccb - ccb of recevier
 */
void tcplike_recv_free(void *);

/*
 * Tell TCPlike that a packet has been received
 * args: ccb  -  ccb block for current connection 
 */
void tcplike_recv_packet_recv(void *, char *, int);

/*
int tcplike_option_recv(void);
*/
#endif

#endif
