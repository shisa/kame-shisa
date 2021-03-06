/*	$OpenBSD: grfvar.h,v 1.13 2003/09/23 16:51:11 millert Exp $	*/
/*	$NetBSD: grfvar.h,v 1.11 1996/08/04 06:03:58 scottr Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: grfvar.h 1.9 91/01/21$
 *
 *	@(#)grfvar.h	7.3 (Berkeley) 5/7/91
 */

#define CARD_NAME_LEN	64

/*
 * State info, per hardware instance.
 */
struct grfbus_softc {
	struct	device	sc_dev;
	nubus_slot	sc_slot;

	bus_space_tag_t		sc_tag;
	bus_space_handle_t	sc_regh;
	bus_space_handle_t	sc_fbh;

	struct	grfmode curr_mode;	/* hardware desc(for ioctl)	*/
	u_int32_t	card_id;	/* DrHW value for nubus cards	*/
	u_int32_t	cli_offset;	/* Offset of byte to clear intr */
					/* for cards where that's suff.  */
	unsigned char	cli_value;	/* Value to write at cli_offset */
	nubus_dir	board_dir;	/* Nubus dir for curr board	*/
};

/*
 * State info, per grf instance.
 */
struct grf_softc {
	struct	device sc_dev;		/* device glue */

	int	sc_flags;		/* software flags */
	struct	grfmode *sc_grfmode;	/* forwarded ... */
	nubus_slot	*sc_slot;
					/* mode-change on/off/mode function */
	int	(*sc_mode)(struct grf_softc *, int, void *);
					/* map virtual addr to physical addr */
	caddr_t	(*sc_phys)(struct grf_softc *, vm_offset_t);
};

/*
 * Attach grf and ite semantics to Mac video hardware.
 */
struct grfbus_attach_args {
	char	*ga_name;		/* name of semantics to attach */
	struct	grfmode *ga_grfmode;	/* forwarded ... */
	nubus_slot	*ga_slot;
	int	(*ga_mode)(struct grf_softc *, int, void *);
	caddr_t	(*ga_phys)(struct grf_softc *, vm_offset_t);
};

typedef	caddr_t (*grf_phys_t)(struct grf_softc *gp, vm_offset_t addr);

/* flags */
#define	GF_ALIVE	0x01
#define GF_OPEN		0x02
#define GF_EXCLUDE	0x04
#define GF_WANTED	0x08
#define GF_BSDOPEN	0x10
#define GF_HPUXOPEN	0x20

/* requests to mode routine */
#define GM_GRFON	1
#define GM_GRFOFF	2
#define GM_CURRMODE	3
#define GM_LISTMODES	4
#define GM_NEWMODE	5

/* minor device interpretation */
#define GRFUNIT(d)	((d) & 0x7)

/*
 * Nubus image data structure.  This is the equivalent of a PixMap in
 * MacOS programming parlance.  One of these structures exists for each
 * video mode that a quickdraw compatible card can fit in.
 */
struct image_data {
	u_int32_t	size;
	u_int32_t	offset;
	u_int16_t	rowbytes;
	u_int16_t	top;
	u_int16_t	left;
	u_int16_t	bottom;
	u_int16_t	right;
	u_int16_t	version;
	u_int16_t	packType;
	u_int32_t	packSize;
	u_int32_t	hRes;
	u_int32_t	vRes;
	u_int16_t	pixelType;
	u_int16_t	pixelSize;	
	u_int16_t	cmpCount;
	u_int16_t	cmpSize;
	u_int32_t	planeBytes;
};

#define VID_PARAMS		1
#define VID_TABLE_OFFSET	2
#define VID_PAGE_CNT		3
#define VID_DEV_TYPE		4

int	grfopen(dev_t dev, int flag, int mode, struct proc *p);
int	grfclose(dev_t dev, int flag, int mode, struct proc *p);
int	grfioctl(dev_t, int, caddr_t, int, struct proc *p);
int	grfpoll(dev_t dev, int rw, struct proc *p);
paddr_t	grfmmap(dev_t dev, off_t off, int prot);
int	grfon(dev_t dev);
int	grfoff(dev_t dev);
int	grfaddr(struct grf_softc *gp, register int off);
int	grfmap(dev_t dev, caddr_t *addrp, struct proc *p);
int	grfunmap(dev_t dev, caddr_t addr, struct proc *p);

void	grf_establish(struct grfbus_softc *, nubus_slot *,
	    int (*)(struct grf_softc *, int, void *),
	    caddr_t (*)(struct grf_softc *, vm_offset_t));
int	grfbusprint(void *, const char *);
