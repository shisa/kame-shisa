/* $OpenBSD: pcdisplayvar.h,v 1.5 2002/03/14 01:26:55 millert Exp $ */
/* $NetBSD: pcdisplayvar.h,v 1.8 2000/01/25 02:44:03 ad Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#if 0
#include "opt_pcdisplay.h"
#endif

struct pcdisplayscreen {
	struct pcdisplay_handle *hdl;

	const struct wsscreen_descr *type;

	int active;		/* currently displayed */
	u_int16_t *mem;		/* backing store for contents */

	int cursoron;		/* cursor displayed? */
#ifdef PCDISPLAY_SOFTCURSOR
	int cursortmp;		/* glyph & attribute behind software cursor */
#endif
	int vc_ccol, vc_crow;	/* current cursor position */

	int dispoffset;		/* offset of displayed area in video mem */
	int visibleoffset;
};

struct pcdisplay_handle {
	bus_space_tag_t	ph_iot, ph_memt;
	bus_space_handle_t ph_ioh_6845, ph_memh;
};

static inline u_int8_t _pcdisplay_6845_read(struct pcdisplay_handle *,
						 int);
static inline void _pcdisplay_6845_write(struct pcdisplay_handle *,
					      int, u_int8_t);

static inline u_int8_t _pcdisplay_6845_read(ph, reg)
	struct pcdisplay_handle *ph;
	int reg;
{
	bus_space_write_1(ph->ph_iot, ph->ph_ioh_6845, MC6845_INDEX, reg);
	return (bus_space_read_1(ph->ph_iot, ph->ph_ioh_6845, MC6845_DATA));
}

static inline void _pcdisplay_6845_write(ph, reg, val)
	struct pcdisplay_handle *ph;
	int reg;
	u_int8_t val;
{
	bus_space_write_1(ph->ph_iot, ph->ph_ioh_6845, MC6845_INDEX, reg);
	bus_space_write_1(ph->ph_iot, ph->ph_ioh_6845, MC6845_DATA, val);
}

#define pcdisplay_6845_read(ph, reg) \
	_pcdisplay_6845_read(ph, offsetof(struct reg_mc6845, reg))
#define pcdisplay_6845_write(ph, reg, val) \
	_pcdisplay_6845_write(ph, offsetof(struct reg_mc6845, reg), val)

void	pcdisplay_cursor_init(struct pcdisplayscreen *, int);
void	pcdisplay_cursor(void *, int, int, int);
#if 0
unsigned int pcdisplay_mapchar_simple(void *, int);
#endif
int pcdisplay_mapchar(void *, int, unsigned int *);
void	pcdisplay_putchar(void *, int, int, u_int, long);
u_int16_t pcdisplay_getchar(void *, int, int);
void	pcdisplay_copycols(void *, int, int, int,int);
void	pcdisplay_erasecols(void *, int, int, int, long);
void	pcdisplay_copyrows(void *, int, int, int);
void	pcdisplay_eraserows(void *, int, int, long);
