/* 	$NetBSD: rasops8.c,v 1.16 2002/02/11 20:50:58 uwe Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rasops8.c,v 1.16 2002/02/11 20:50:58 uwe Exp $");

#include "opt_rasops.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/bswap.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>

static void 	rasops8_putchar __P((void *, int, int, u_int, long attr));
#ifndef RASOPS_SMALL
static void 	rasops8_putchar8 __P((void *, int, int, u_int, long attr));
static void 	rasops8_putchar12 __P((void *, int, int, u_int, long attr));
static void 	rasops8_putchar16 __P((void *, int, int, u_int, long attr));
static void	rasops8_makestamp __P((struct rasops_info *ri, long));

/*
 * 4x1 stamp for optimized character blitting
 */
static int32_t	stamp[16];
static long	stamp_attr;
static int	stamp_mutex;	/* XXX see note in README */
#endif

/*
 * XXX this confuses the hell out of gcc2 (not egcs) which always insists
 * that the shift count is negative.
 *
 * offset = STAMP_SHIFT(fontbits, nibble #) & STAMP_MASK
 * destination = STAMP_READ(offset)
 */
#define STAMP_SHIFT(fb,n)	((n*4-2) >= 0 ? (fb)>>(n*4-2):(fb)<<-(n*4-2))
#define STAMP_MASK		(0xf << 2)
#define STAMP_READ(o)		(*(int32_t *)((caddr_t)stamp + (o)))

/*
 * Initialize a 'rasops_info' descriptor for this depth.
 */
void
rasops8_init(ri)
	struct rasops_info *ri;
{

	switch (ri->ri_font->fontwidth) {
#ifndef RASOPS_SMALL
	case 8:
		ri->ri_ops.putchar = rasops8_putchar8;
		break;
	case 12:
		ri->ri_ops.putchar = rasops8_putchar12;
		break;
	case 16:
		ri->ri_ops.putchar = rasops8_putchar16;
		break;
#endif /* !RASOPS_SMALL */
	default:
		ri->ri_ops.putchar = rasops8_putchar;
		break;
	}
}

/*
 * Put a single character.
 */
static void
rasops8_putchar(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	int width, height, cnt, fs, fb;
	u_char *dp, *rp, *fr, clr[2];
	struct rasops_info *ri;

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif
	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;

	height = ri->ri_font->fontheight;
	width = ri->ri_font->fontwidth;
	clr[0] = (u_char)ri->ri_devcmap[(attr >> 16) & 0xf];
	clr[1] = (u_char)ri->ri_devcmap[(attr >> 24) & 0xf];

	if (uc == ' ') {
		u_char c = clr[0];

		while (height--) {
			dp = rp;
			rp += ri->ri_stride;

			for (cnt = width; cnt; cnt--)
				*dp++ = c;
		}
	} else {
		uc -= ri->ri_font->firstchar;
		fr = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;
		fs = ri->ri_font->stride;

		while (height--) {
			dp = rp;
			fb = fr[3] | (fr[2] << 8) | (fr[1] << 16) | (fr[0] << 24);
			fr += fs;
			rp += ri->ri_stride;

			for (cnt = width; cnt; cnt--) {
				*dp++ = clr[(fb >> 31) & 1];
				fb <<= 1;
			}
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		u_char c = clr[1];

		rp -= (ri->ri_stride << 1);

		while (width--)
			*rp++ = c;
	}
}

#ifndef RASOPS_SMALL
/*
 * Recompute the 4x1 blitting stamp.
 */
static void
rasops8_makestamp(ri, attr)
	struct rasops_info *ri;
	long attr;
{
	int32_t fg, bg;
	int i;

	fg = ri->ri_devcmap[(attr >> 24) & 0xf] & 0xff;
	bg = ri->ri_devcmap[(attr >> 16) & 0xf] & 0xff;
	stamp_attr = attr;

	for (i = 0; i < 16; i++) {
#if BYTE_ORDER == BIG_ENDIAN
#define NEED_LITTLE_ENDIAN_STAMP RI_BSWAP
#else
#define NEED_LITTLE_ENDIAN_STAMP 0
#endif
		if ((ri->ri_flg & RI_BSWAP) == NEED_LITTLE_ENDIAN_STAMP) {
			/* little endian */
			stamp[i] = (i & 8 ? fg : bg);
			stamp[i] |= ((i & 4 ? fg : bg) << 8);
			stamp[i] |= ((i & 2 ? fg : bg) << 16);
			stamp[i] |= ((i & 1 ? fg : bg) << 24);
		} else {
			/* big endian */
			stamp[i] = (i & 1 ? fg : bg);
			stamp[i] |= ((i & 2 ? fg : bg) << 8);
			stamp[i] |= ((i & 4 ? fg : bg) << 16);
			stamp[i] |= ((i & 8 ? fg : bg) << 24);
		}
	}
}

/*
 * Put a single character. This is for 8-pixel wide fonts.
 */
static void
rasops8_putchar8(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	struct rasops_info *ri;
	int height, fs;
	int32_t *rp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops8_putchar(cookie, row, col, uc, attr);
		return;
	}

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops8_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	height = ri->ri_font->fontheight;

	if (uc == ' ') {
		while (height--) {
			rp[0] = rp[1] = stamp[0];
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	} else {
		uc -= ri->ri_font->firstchar;
		fr = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;
		fs = ri->ri_font->stride;

		while (height--) {
			rp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) & STAMP_MASK);
			rp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) & STAMP_MASK);

			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = stamp[15];
	}

	stamp_mutex--;
}

/*
 * Put a single character. This is for 12-pixel wide fonts.
 */
static void
rasops8_putchar12(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	struct rasops_info *ri;
	int height, fs;
	int32_t *rp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops8_putchar(cookie, row, col, uc, attr);
		return;
	}

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops8_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	height = ri->ri_font->fontheight;

	if (uc == ' ') {
		while (height--) {
			int32_t c = stamp[0];

			rp[0] = rp[1] = rp[2] = c;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	} else {
		uc -= ri->ri_font->firstchar;
		fr = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;
		fs = ri->ri_font->stride;

		while (height--) {
			rp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) & STAMP_MASK);
			rp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) & STAMP_MASK);
			rp[2] = STAMP_READ(STAMP_SHIFT(fr[1], 1) & STAMP_MASK);

			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = rp[2] = stamp[15];
	}

	stamp_mutex--;
}

/*
 * Put a single character. This is for 16-pixel wide fonts.
 */
static void
rasops8_putchar16(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	struct rasops_info *ri;
	int height, fs;
	int32_t *rp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops8_putchar(cookie, row, col, uc, attr);
		return;
	}

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops8_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	height = ri->ri_font->fontheight;

	if (uc == ' ') {
		while (height--)
			rp[0] = rp[1] = rp[2] = rp[3] = stamp[0];
	} else {
		uc -= ri->ri_font->firstchar;
		fr = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;
		fs = ri->ri_font->stride;

		while (height--) {
			rp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) & STAMP_MASK);
			rp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) & STAMP_MASK);
			rp[2] = STAMP_READ(STAMP_SHIFT(fr[1], 1) & STAMP_MASK);
			rp[3] = STAMP_READ(STAMP_SHIFT(fr[1], 0) & STAMP_MASK);

			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = rp[2] = rp[3] = stamp[15];
	}

	stamp_mutex--;
}
#endif /* !RASOPS_SMALL */
