/*	$KAME: mdnsd.h,v 1.24 2002/09/10 02:04:08 itojun Exp $	*/

/*
 * Copyright (C) 2000 WIDE Project.
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

#define DNS_PORT	"53"
#define MDNS_PORT	"5353"
#define MDNS_GROUP4	"224.0.0.251"
#define MDNS_GROUP6	"ff02::1"	/* XXX not declared in i-d */
#define MDNS_LOCALDOM	"local."
#define MDNS_TIMEO	5		/* XXX better value? */

extern u_int16_t dnsid;
extern const char *srcport;
extern const char *dstport;
extern const char *intface;
extern int family;
extern const char *hostname;
extern int dflag;
extern int fflag;
extern int lflag;
extern int Nflag;
extern struct timeval hz;
extern const int niflags;
extern int signo;
extern int dormantcount;
extern int dormanttime;

/* mdnsd.c */
extern int addserv __P((const char *, int, const char *));
extern int ismyaddr __P((const struct sockaddr *, int));
extern int dprintf __P((const char *, ...));
extern void status __P((void));

/* mainloop.c */
extern void mainloop __P((void));
extern int send_updates __P((void));
