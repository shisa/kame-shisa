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

#if defined(YIPS_DEBUG)
#define YIPSDEBUG(lev,arg) if ((debug & (lev)) == (lev)) { arg; }
#else
#define YIPSDEBUG(lev,arg)
#endif /* defined(YIPS_DEBUG) */

#define LOG_ASPATH	0x00000001
#define LOG_BGPSTATE	0x00000002
#define LOG_BGPCONNECT	0x00000004
#define LOG_BGPINPUT	0x00000008
#define LOG_BGPOUTPUT	0x00000010
#define LOG_BGPROUTE	0x00000020
#define LOG_INTERFACE	0x00000040
#define LOG_INET6	0x00000080
#define LOG_RIP		0x00000100
#define LOG_ROUTE	0x00000200
#define LOG_FILTER	0x00000400
#define LOG_TIMER	0x00000800

#define LOG_BGP (LOG_ASPATH|LOG_BGPSTATE|LOG_BGPCONNECT|LOG_BGPINPUT|\
		 LOG_BGPOUTPUT|LOG_BGPROUTE)
#define LOG_ALL 0xffffffff

#define DEBUG_CONF 0x01000000

#define IFLOG(l) if ((logflags & (l)))

extern unsigned long logflags;
