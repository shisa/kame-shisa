/*	$NetBSD: mbuf.c,v 1.17 2002/03/09 23:26:51 sommerfeld Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)mbuf.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: mbuf.c,v 1.17 2002/03/09 23:26:51 sommerfeld Exp $");
#endif
#endif /* not lint */

#define	__POOL_EXPOSE

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/pool.h>

#include <stdio.h>
#include <stdlib.h>
#include "netstat.h"

#define	YES	1
typedef int bool;

struct	mbstat mbstat;
struct pool mbpool, mclpool;
struct pool_allocator mbpa, mclpa;

static struct mbtypes {
	int	mt_type;
	char	*mt_name;
} mbtypes[] = {
	{ MT_DATA,	"data" },
	{ MT_OOBDATA,	"oob data" },
	{ MT_CONTROL,	"ancillary data" },
	{ MT_HEADER,	"packet headers" },
	{ MT_FTABLE,	"fragment reassembly queue headers" },	/* XXX */
	{ MT_SONAME,	"socket names and addresses" },
	{ MT_SOOPTS,	"socket options" },
	{ 0, 0 }
};

int nmbtypes = sizeof(mbstat.m_mtypes) / sizeof(short);
bool seen[256];			/* "have we seen this type yet?" */

/*
 * Print mbuf statistics.
 */
void
mbpr(mbaddr, msizeaddr, mclbaddr, mbpooladdr, mclpooladdr)
	u_long mbaddr;
	u_long msizeaddr, mclbaddr;
	u_long mbpooladdr, mclpooladdr;
{
	int totmem, totused, totmbufs, totpct;
	int i;
	struct mbtypes *mp;

	int	mclbytes,	msize;

	if (nmbtypes != 256) {
		fprintf(stderr,
		    "%s: unexpected change to mbstat; check source\n",
		        getprogname());
		return;
	}
	if (mbaddr == 0) {
		fprintf(stderr, "%s: mbstat: symbol not in namelist\n",
		    getprogname());
		return;
	}
/*XXX*/
	if (msizeaddr != 0)
		kread(msizeaddr, (char *)&msize, sizeof (msize));
	else
		msize = MSIZE;
	if (mclbaddr != 0)
		kread(mclbaddr, (char *)&mclbytes, sizeof (mclbytes));
	else
		mclbytes = MCLBYTES;
/*XXX*/

	if (kread(mbaddr, (char *)&mbstat, sizeof (mbstat)))
		return;

	if (kread(mbpooladdr, (char *)&mbpool, sizeof (mbpool)))
		return;

	if (kread(mclpooladdr, (char *)&mclpool, sizeof (mclpool)))
		return;

	mbpooladdr = (u_long) mbpool.pr_alloc;
	mclpooladdr = (u_long) mclpool.pr_alloc;

	if (kread(mbpooladdr, (char *)&mbpa, sizeof (mbpa)))
		return;

	if (kread(mclpooladdr, (char *)&mclpa, sizeof (mclpa)))
		return;

	totmbufs = 0;
	for (mp = mbtypes; mp->mt_name; mp++)
		totmbufs += mbstat.m_mtypes[mp->mt_type];
	printf("%u mbufs in use:\n", totmbufs);
	for (mp = mbtypes; mp->mt_name; mp++)
		if (mbstat.m_mtypes[mp->mt_type]) {
			seen[mp->mt_type] = YES;
			printf("\t%u mbufs allocated to %s\n",
			    mbstat.m_mtypes[mp->mt_type], mp->mt_name);
		}
	seen[MT_FREE] = YES;
	for (i = 0; i < nmbtypes; i++)
		if (!seen[i] && mbstat.m_mtypes[i]) {
			printf("\t%u mbufs allocated to <mbuf type %d>\n",
			    mbstat.m_mtypes[i], i);
		}

	printf("%lu/%lu mapped pages in use\n",
	       (u_long)(mclpool.pr_nget - mclpool.pr_nput),
	       ((u_long)mclpool.pr_npages * mclpool.pr_itemsperpage));
	totmem = (mbpool.pr_npages << mbpa.pa_pageshift) +
	    (mclpool.pr_npages << mclpa.pa_pageshift);
	totused = (mbpool.pr_nget - mbpool.pr_nput) * mbpool.pr_size +
	    (mclpool.pr_nget - mclpool.pr_nput) * mclpool.pr_size;
	totpct = (totmem == 0)? 0 : ((totused * 100)/totmem);
	printf("%u Kbytes allocated to network (%d%% in use)\n",
	    totmem / 1024, totpct);
	printf("%lu requests for memory denied\n", mbstat.m_drops);
	printf("%lu requests for memory delayed\n", mbstat.m_wait);
	printf("%lu calls to protocol drain routines\n", mbstat.m_drain);

	if (mbstat.m_exthdrget || mbstat.m_exthdrget0) {
#define	p(f, m) if (mbstat.f || sflag <= 1) \
    printf(m, (unsigned long long)mbstat.f, plural(mbstat.f))
#define	p1(f, m) if (mbstat.f || sflag <= 1) \
    printf(m, (unsigned long long)mbstat.f)
		p(m_exthdrget, "%llu use%s of IP6_EXTHDR_GET\n");
		p(m_exthdrget0, "%llu use%s of IP6_EXTHDR_GET0\n");
		p(m_pulldowns, "%llu call%s to m_pulldown\n");
		p(m_pulldown_alloc,
		    "%llu mbuf allocation%s in m_pulldown\n");
		if (mbstat.m_pulldown_copy != 1) {
			p1(m_pulldown_copy,
			    "%llu mbuf copies in m_pulldown\n");
		} else {
			p1(m_pulldown_copy,
			    "%llu mbuf copy in m_pulldown\n");
		}
		p(m_pullups, "%llu call%s to m_pullup\n");
		p(m_pullup_alloc, "%llu mbuf allocation%s in m_pullup\n");
		if (mbstat.m_pullup_copy != 1) {
			p1(m_pullup_copy,
			    "%llu mbuf copies in m_pullup\n");
		} else {
			p1(m_pullup_copy, "%llu mbuf copy in m_pullup\n");
		}
		p(m_pullup_fail, "%llu failure%s in m_pullup\n");
		p(m_pullup2, "%llu call%s to m_pullup2\n");
		p(m_pullup2_alloc,
		    "%llu mbuf allocation%s in m_pullup2\n");
		if (mbstat.m_pullup2_copy != 1) {
			p1(m_pullup2_copy,
			    "%llu mbuf copies in m_pullup2\n");
		} else {
			p1(m_pullup2_copy,
			    "%llu mbuf copy in m_pullup2\n");
		}
		p(m_pullup2_fail, "%llu failure%s in m_pullup2\n");
#undef p
#undef p1
	}
}
