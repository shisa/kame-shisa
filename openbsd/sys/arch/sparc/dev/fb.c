/*	$OpenBSD: fb.c,v 1.28 2004/02/29 21:24:36 miod Exp $	*/
/*	$NetBSD: fb.c,v 1.23 1997/07/07 23:30:22 pk Exp $ */

/*
 * Copyright (c) 2002 Miodrag Vallat
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *
 *	@(#)fb.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Common wsdisplay framebuffer drivers helpers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#if defined(SUN4)
#include <machine/eeprom.h>
#include <sparc/dev/pfourreg.h>
#endif

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include "wsdisplay.h"

/*
 * emergency unblank code
 * XXX should be somewhat moved to wscons MI code
 */

void (*fb_burner)(void *, u_int, u_int);
void *fb_cookie;

void
fb_unblank()
{
	if (fb_burner != NULL)
		(*fb_burner)(fb_cookie, 1, 0);
}

#if NWSDISPLAY > 0

void
fb_setsize(struct sunfb *sf, int def_depth, int def_width, int def_height,
    int node, int bustype)
{
	int def_linebytes;

	switch (bustype) {
	case BUS_VME16:
	case BUS_VME32:
	case BUS_OBIO:
#if defined(SUN4M)
		/* 4m may have SBus-like framebuffer on obio */
		if (CPU_ISSUN4M) {
			goto obpsize;
		}
#endif
		/* Set up some defaults. */
		sf->sf_width = def_width;
		sf->sf_height = def_height;
		sf->sf_depth = def_depth;

		/*
		 * This is not particularly useful on Sun 4 VME framebuffers.
		 * The EEPROM only contains info about the built-in.
		 */
		if (CPU_ISSUN4 && (bustype == BUS_VME16 ||
		    bustype == BUS_VME32))
			goto donesize;

#if defined(SUN4)
		if (CPU_ISSUN4) {
			struct eeprom *eep = (struct eeprom *)eeprom_va;

			if (ISSET(sf->sf_flags, FB_PFOUR)) {
				volatile u_int32_t pfour;

				/*
				 * Some pfour framebuffers, e.g. the
				 * cgsix, don't encode resolution the
				 * same, so the driver handles that.
				 * The driver can let us know that it
				 * needs to do this by not mapping in
				 * the pfour register by the time this
				 * routine is called.
				 */
				if (sf->sf_pfour == NULL)
					goto donesize;

				pfour = *sf->sf_pfour;

				/*
				 * Use the pfour register to determine
				 * the size.  Note that the cgsix and
				 * cgeight don't use this size encoding.
				 * In this case, we have to settle
				 * for the defaults we were provided
				 * with.
				 */
				if ((PFOUR_ID(pfour) == PFOUR_ID_COLOR24) ||
				    (PFOUR_ID(pfour) == PFOUR_ID_FASTCOLOR))
					goto donesize;

				switch (PFOUR_SIZE(pfour)) {
				case PFOUR_SIZE_1152X900:
					sf->sf_width = 1152;
					sf->sf_height = 900;
					break;

				case PFOUR_SIZE_1024X1024:
					sf->sf_width = 1024;
					sf->sf_height = 1024;
					break;

				case PFOUR_SIZE_1280X1024:
					sf->sf_width = 1280;
					sf->sf_height = 1024;
					break;

				case PFOUR_SIZE_1600X1280:
					sf->sf_width = 1600;
					sf->sf_height = 1280;
					break;

				case PFOUR_SIZE_1440X1440:
					sf->sf_width = 1440;
					sf->sf_height = 1440;
					break;

				case PFOUR_SIZE_640X480:
					sf->sf_width = 640;
					sf->sf_height = 480;
					break;

				default:
					/*
					 * XXX: Do nothing, I guess.
					 * Should we print a warning about
					 * an unknown value? --thorpej
					 */
					break;
				}
			} else if (eep != NULL) {
				switch (eep->eeScreenSize) {
				case EE_SCR_1152X900:
					sf->sf_width = 1152;
					sf->sf_height = 900;
					break;

				case EE_SCR_1024X1024:
					sf->sf_width = 1024;
					sf->sf_height = 1024;
					break;

				case EE_SCR_1600X1280:
					sf->sf_width = 1600;
					sf->sf_height = 1280;
					break;

				case EE_SCR_1440X1440:
					sf->sf_width = 1440;
					sf->sf_height = 1440;
					break;

				default:
					/*
					 * XXX: Do nothing, I guess.
					 * Should we print a warning about
					 * an unknown value? --thorpej
					 */
					break;
				}
			}
		}
#endif /* SUN4 */
#if defined(SUN4M)
		if (CPU_ISSUN4M) {
			/* XXX: need code to find 4/600 vme screen size */
		}
#endif /* SUN4M */

donesize:
		sf->sf_linebytes = (sf->sf_width * sf->sf_depth) / 8;
		break;

	case BUS_SBUS:
#if defined(SUN4M)
obpsize:
#endif
		sf->sf_depth = getpropint(node, "depth", def_depth);
		sf->sf_width = getpropint(node, "width", def_width);
		sf->sf_height = getpropint(node, "height", def_height);

		def_linebytes =
		    roundup(sf->sf_width, sf->sf_depth) * sf->sf_depth / 8;
		sf->sf_linebytes = getpropint(node, "linebytes", def_linebytes);
		/*
		 * XXX If we are configuring a board in a wider depth level
		 * than the mode it is currently operating in, the PROM will
		 * return a linebytes property tied to the current depth value,
		 * which is NOT what we are relying upon!
		 */
		if (sf->sf_linebytes < (sf->sf_width * sf->sf_depth) / 8) {
			sf->sf_linebytes = def_linebytes;
		}
		break;

	default:
		panic("fb_setsize: inappropriate bustype");
		/* NOTREACHED */
	}

	sf->sf_fbsize = sf->sf_height * sf->sf_linebytes;
}

int a2int(char *, int);

int
a2int(char *cp, int deflt)
{
	int i = 0;

	if (*cp == '\0')
		return (deflt);
	while (*cp != '\0')
		i = i * 10 + *cp++ - '0';
	return (i);
}

void
fbwscons_init(struct sunfb *sf, int flags)
{
	int cols, rows;

	/* ri_hw and ri_bits must have already been setup by caller */
	sf->sf_ro.ri_flg = RI_CENTER | RI_FULLCLEAR | flags;
	sf->sf_ro.ri_depth = sf->sf_depth;
	sf->sf_ro.ri_stride = sf->sf_linebytes;
	sf->sf_ro.ri_width = sf->sf_width;
	sf->sf_ro.ri_height = sf->sf_height;

#if defined(SUN4C) || defined(SUN4M)
	if (CPU_ISSUN4COR4M) {
		rows = a2int(getpropstring(optionsnode, "screen-#rows"), 34);
		cols = a2int(getpropstring(optionsnode, "screen-#columns"), 80);
	}
#endif
#if defined(SUN4)
	if (CPU_ISSUN4) {
		struct eeprom *ep = (struct eeprom *)eeprom_va;

		if (ep != NULL) {
			rows = (u_short)ep->eeTtyRows;
			cols = (u_short)ep->eeTtyCols;
			/* deal with broken nvram contents... */
			if (rows <= 0)
				rows = 34;
			if (cols <= 0)
				cols = 80;
		} else {
			rows = 34;
			cols = 80;
		}
	}
#endif

	rasops_init(&sf->sf_ro, rows, cols);
}

void
fbwscons_console_init(struct sunfb *sf, struct wsscreen_descr *wsc, int row,
    void (*burner)(void *, u_int, u_int))
{
	long defattr;

	if (CPU_ISSUN4 || romgetcursoraddr(&sf->sf_crowp, &sf->sf_ccolp))
		sf->sf_ccolp = sf->sf_crowp = NULL;
	if (sf->sf_ccolp != NULL)
		sf->sf_ro.ri_ccol = *sf->sf_ccolp;

	if (row < 0) {
		if (sf->sf_crowp != NULL)
			sf->sf_ro.ri_crow = *sf->sf_crowp;
		else
			/* assume last row */
			sf->sf_ro.ri_crow = sf->sf_ro.ri_rows - 1;
	} else {
		/*
		 * If we force the display row, this is because the screen
		 * has been cleared or the font has been changed.
		 * In this case, choose not to keep pointers to the PROM
		 * cursor position, as the values are likely to be inaccurate
		 * upon shutdown...
		 */
		sf->sf_crowp = sf->sf_ccolp = NULL;
		sf->sf_ro.ri_crow = row;
	}

	/*
	 * Scale back rows and columns if the font would not otherwise
	 * fit on this display. Without this we would panic later.
	 */
	if (sf->sf_ro.ri_crow >= wsc->nrows)
		sf->sf_ro.ri_crow = wsc->nrows - 1;
	if (sf->sf_ro.ri_ccol >= wsc->ncols)
		sf->sf_ro.ri_ccol = wsc->ncols - 1;

	/*
	 * Select appropriate color settings to mimic a
	 * black on white Sun console.
	 */
	if (sf->sf_depth > 8) {
		wscol_white = 0;
		wscol_black = 255;
		wskernel_bg = 0;
		wskernel_fg = 255;
	}

	if (ISSET(wsc->capabilities, WSSCREEN_WSCOLORS) &&
	    sf->sf_depth == 8) {
		sf->sf_ro.ri_ops.alloc_attr(&sf->sf_ro,
		    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, &defattr);
	} else {
		sf->sf_ro.ri_ops.alloc_attr(&sf->sf_ro, 0, 0, 0, &defattr);
	}

	wsdisplay_cnattach(wsc, &sf->sf_ro,
	    sf->sf_ro.ri_ccol, sf->sf_ro.ri_crow, defattr);

	/* remember screen burner routine */
	fb_burner = burner;
	fb_cookie = sf;
}

void
fbwscons_setcolormap(struct sunfb *sf,
    void (*setcolor)(void *, u_int, u_int8_t, u_int8_t, u_int8_t))
{
	int i;
	u_char *color;

	if (sf->sf_depth <= 8 && setcolor != NULL) {
		for (i = 0; i < 16; i++) {
			color = (u_char *)&rasops_cmap[i * 3];
			setcolor(sf, i, color[0], color[1], color[2]);
		}
		for (i = 240; i < 256; i++) {
			color = (u_char *)&rasops_cmap[i * 3];
			setcolor(sf, i, color[0], color[1], color[2]);
		}
		/* compensate for BoW palette */
		setcolor(sf, WSCOL_BLACK, 0, 0, 0);
		setcolor(sf, 0xff ^ WSCOL_BLACK, 255, 255, 255);
		setcolor(sf, WSCOL_WHITE, 255, 255, 255);
		setcolor(sf, 0xff ^ WSCOL_WHITE, 0, 0, 0);
	}
}

#if defined(SUN4)
/*
 * Support routines for pfour framebuffers.
 */

/*
 * Probe for a pfour framebuffer.  Return values:
 *
 *	PFOUR_NOTPFOUR		framebuffer is not a pfour
 *				framebuffer
 *
 *	otherwise returns pfour ID
 */
int
fb_pfour_id(void *va)
{
	volatile u_int32_t val, save, *pfour = va;

	/* Read the pfour register. */
	save = *pfour;

	/*
	 * Try to modify the type code.  If it changes, put the
	 * original value back, and notify the caller that it's
	 * not a pfour framebuffer.
	 */
	val = save & ~PFOUR_REG_RESET;
	*pfour = (val ^ PFOUR_FBTYPE_MASK);
	if ((*pfour ^ val) & PFOUR_FBTYPE_MASK) {
		*pfour = save;
		return (PFOUR_NOTPFOUR);
	}

	return (PFOUR_ID(val));
}


/*
 * Return the status of the video enable.
 */
int
fb_pfour_get_video(struct sunfb *sf)
{

	return ((*sf->sf_pfour & PFOUR_REG_VIDEO) != 0);
}

/*
 * Enable or disable the framebuffer.
 */
void
fb_pfour_set_video(struct sunfb *sf, int enable)
{
	volatile u_int32_t pfour;

	pfour = *sf->sf_pfour & ~(PFOUR_REG_INTCLR | PFOUR_REG_VIDEO);
	*sf->sf_pfour = pfour | (enable ? PFOUR_REG_VIDEO : 0);
}

#endif /* SUN4 */

#endif	/* NWSDISPLAY */
