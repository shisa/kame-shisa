/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/pc98/libpc98/time.c,v 1.5 2003/09/08 09:11:20 obrien Exp $");

#include <stand.h>
#include <btxv86.h>
#ifdef PC98
#include <machine/cpufunc.h>
#endif
#include "bootstrap.h"
#include "libi386.h"

/*
 * Return the time in seconds since the beginning of the day.
 *
 * If we pass midnight, don't wrap back to 0.
 *
 * XXX uses undocumented BCD support from libstand.
 */

time_t
time(time_t *t)
{
    static time_t	lasttime, now;
    int			hr, minute, sec;
    
#ifdef PC98
    unsigned char	bios_time[6];
#endif

    v86.ctl = 0;
#ifdef PC98
    v86.addr = 0x1c;            /* int 0x1c, function 0 */
    v86.eax = 0x0000;
    v86.es  = VTOPSEG(bios_time);
    v86.ebx = VTOPOFF(bios_time);
#else
    v86.addr = 0x1a;		/* int 0x1a, function 2 */
    v86.eax = 0x0200;
#endif
    v86int();

#ifdef PC98
    hr = bcd2bin(bios_time[3]);
    minute = bcd2bin(bios_time[4]);
    sec = bcd2bin(bios_time[5]);
#else
    hr = bcd2bin((v86.ecx & 0xff00) >> 8);	/* hour in %ch */
    minute = bcd2bin(v86.ecx & 0xff);		/* minute in %cl */
    sec = bcd2bin((v86.edx & 0xff00) >> 8);	/* second in %dh */
#endif
    
    now = hr * 3600 + minute * 60 + sec;
    if (now < lasttime)
	now += 24 * 3600;
    lasttime = now;
    
    if (t != NULL)
	*t = now;
    return(now);
}

/*
 * Use the BIOS Wait function to pause for (period) microseconds.
 *
 * Resolution of this function is variable, but typically around
 * 1ms.
 */
void
delay(int period)
{
#ifdef PC98
    int i;
    period = (period + 500) / 1000;
    for( ; period != 0 ; period--)
	for(i=800;i != 0; i--)
	    outb(0x5f,0);       /* wait 600ns */
#else
    v86.ctl = 0;
    v86.addr = 0x15;		/* int 0x15, function 0x86 */
    v86.eax = 0x8600;
    v86.ecx = period >> 16;
    v86.edx = period & 0xffff;
    v86int();
#endif
}
