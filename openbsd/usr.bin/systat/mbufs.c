/*	$OpenBSD: mbufs.c,v 1.13 2003/06/03 02:56:17 millert Exp $	*/
/*	$NetBSD: mbufs.c,v 1.2 1995/01/20 08:52:02 jtc Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)mbufs.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: mbufs.c,v 1.13 2003/06/03 02:56:17 millert Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <stdlib.h>
#include <string.h>
#include <nlist.h>
#include <err.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"

static struct mbstat *mb;

char *mtnames[] = {
	"free",
	"data",
	"headers",
	"sockets",
	"pcbs",
	"routes",
	"hosts",
	"arps",
	"socknames",
	"zombies",
	"sockopts",
	"frags",
	"rights",
	"ifaddrs",
};

#define	NNAMES	(sizeof (mtnames) / sizeof (mtnames[0]))

WINDOW *
openmbufs(void)
{
	return (subwin(stdscr, LINES-5-1, 0, 5, 0));
}

void
closembufs(WINDOW *w)
{
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

void
labelmbufs(void)
{
	wmove(wnd, 0, 0); wclrtoeol(wnd);
	mvwaddstr(wnd, 0, 10,
	    "/0   /5   /10  /15  /20  /25  /30  /35  /40  /45  /50  /55  /60");
}

void
showmbufs(void)
{
	int i, j, max, index;
	char buf[13];

	if (mb == 0)
		return;
	for (j = 0; j < wnd->_maxy; j++) {
		max = 0, index = -1;
		for (i = 0; i < wnd->_maxy; i++)
			if (mb->m_mtypes[i] > max) {
				max = mb->m_mtypes[i];
				index = i;
			}
		if (max == 0)
			break;
		if (j > NNAMES)
			mvwprintw(wnd, 1+j, 0, "%10d", index);
		else
			mvwprintw(wnd, 1+j, 0, "%-10.10s", mtnames[index]);
		wmove(wnd, 1 + j, 10);
		if (max > 60) {
			snprintf(buf, sizeof buf, " %d", max);
			max = 60;
			while (max--)
				waddch(wnd, 'X');
			waddstr(wnd, buf);
		} else {
			while (max--)
				waddch(wnd, 'X');
			wclrtoeol(wnd);
		}
		mb->m_mtypes[index] = 0;
	}
	wmove(wnd, 1+j, 0); wclrtobot(wnd);
}

static struct nlist namelist[] = {
#define	X_MBSTAT	0
	{ "_mbstat" },			/* sysctl */
	{ "" }
};

int
initmbufs(void)
{
	int ret;

	if (kd != NULL) {
		if (namelist[X_MBSTAT].n_type == 0) {
			if ((ret = kvm_nlist(kd, namelist)) == -1)
				errx(1, "%s", kvm_geterr(kd));
			else if (ret)
				nlisterr(namelist);
			if (namelist[X_MBSTAT].n_type == 0) {
				error("namelist on %s failed", _PATH_UNIX);
				return(0);
			}
		}
	}
	if (mb == 0)
		mb = (struct mbstat *)calloc(1, sizeof (*mb));
	return(1);
}

void
fetchmbufs(void)
{
	int mib[2];
	size_t size = sizeof (*mb);

	if (kd == NULL) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_MBSTAT;
		if (sysctl(mib, 2, mb, &size, NULL, 0) < 0)
			err(1, "sysctl(KERN_MBSTAT) failed");
	} else {
		if (namelist[X_MBSTAT].n_type == 0)
			return;
		NREAD(X_MBSTAT, mb, size);
	}
}
