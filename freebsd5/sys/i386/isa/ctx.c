/*
 * CORTEX-I Frame Grabber driver V1.0
 *
 *	Copyright (C) 1994, Paul S. LaFollette, Jr. This software may be used,
 *	modified, copied, distributed, and sold, in both source and binary form
 *	provided that the above copyright and these terms are retained. Under
 *	no circumstances is the author responsible for the proper functioning
 *	of this software, nor does the author assume any responsibility
 *	for damages incurred with its use.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i386/isa/ctx.c,v 1.47 2003/06/02 16:32:54 obrien Exp $");

/*
 *	Device Driver for CORTEX-I Frame Grabber
 *	Made by ImageNation Corporation
 *	1200 N.E. Keyues Road
 *	Vancouver, WA 98684  (206) 944-9131
 *	(I have no ties to this company, just thought you might want
 *	 to know how to get in touch with them.)
 *
 *	In order to understand this device, you really need to consult the
 *	manual which ImageNation provides when you buy the board. (And
 *	what a pleasure it is to buy something for a PC and actually get
 *	programming information along with it.)  I will limit myself here to
 *	a few comments which are specific to this driver.  See also the file
 *	ctxreg.h for definitions of registers and control bits.
 *
 *	1.  Although the hardware supports low resolution (256 x 256)
 *	    acqusition and display, I have not implemented access to
 *	    these modes in this driver.  There are some fairly quirky
 *	    aspects to the way this board works in low resolution mode,
 *	    and I don't want to deal with them.  Maybe later.
 *
 *	2.  Choosing the base address for the video memory:  This is set
 *	    using a combination of hardware and software, using the left
 *	    most dip switch on the board, and the AB_SELECT bit of control
 *	    port 1, according to the chart below:
 *
 *		Left DIP switch ||	DOWN	|	UP	|
 *		=================================================
 *		 AB_SELECT =  0	||    0xA0000	|    0xB0000	|
 *		-------------------------------------------------
 *		 AB_SELECT = 1 	||    0xD0000	|    0xE0000	|
 *		------------------------------------------------
 *
 *	    When the RAM_ENABLE bit of control port 1 is clear (0), the
 *	    video ram is disconnected from the computer bus.  This makes
 *	    it possible, in principle, to share memory space with other
 *	    devices (such as VGA) which can also disconnect themselves
 *	    from the bus.  It also means that multiple CORTEX-I boards
 *	    can share the same video memory space.  Disconnecting from the
 *	    bus does not affect the video display of the video ram contents,
 *	    so that one needs only set the RAM_ENABLE bit when actually
 *	    reading or writing to memory.  The cost of this is low,
 *	    the benefits to me are great (I need more than one board
 *	    in my machine, and 0xE0000 is the only address choice that
 *	    doesn't conflict with anything) so I adopt this strategy here.
 *
 *	    XXX-Note... this driver has only been tested for the
 *	    XXX base = 0xE0000 case!
 *
 *	3)  There is a deficiency in the documentation from ImageNation, I
 *	    think.  In order to successfully load the lookup table, it is
 *	    necessary to clear SEE_STORED_VIDEO in control port 0 as well as
 *	    setting LUT_LOAD_ENABLE in control port 1.
 *
 *	4)  This driver accesses video memory through read or write operations.
 *	    Other functionality is provided through ioctl's, manifest
 *	    constants for which are defined in ioctl_ctx.h. The ioctl's
 *	    include:
 *			CTX_LIVE	Display live video
 *			CTX_GRAB	Grab a frame of video data
 *			CTX_H_ORGANIZE	Set things up so that sequential read
 *					operations access horizontal lines of
 *					pixels.
 *			CTX_V_ORGANIZE	Set things up so that sequential read
 *					operations access vertical lines of
 *					pixels.
 *			CTX_SET_LUT	Set the lookup table from an array
 *					of 256 unsigned chars passed as the
 *					third parameter to ioctl.
 *			CTX_GET_LUT	Return the current lookup table to
 *					the application as an array of 256
 *					unsigned chars.  Again the third
 *					parameter to the ioctl call.
 *
 *	    Thus,
 *		ioctl(fi, CTX_H_ORGANIZE, 0);
 *		lseek(fi, y*512, SEEK_SET);
 *		read(fi, buffer, 512);
 *
 *	    will fill buffer with 512 pixels (unsigned chars) which represent
 *	    the y-th horizontal line of the image.
 *	    Similarly,
 *		ioctl(fi, CTX_V_ORGANIZE, 0:
 *		lseek(fi, x*512+y, SEEK_SET);
 *		read(fi, buffer, 10);
 *
 *	    will read 10 a vertical line of 10 pixels starting at (x,y).
 *
 *	    Obviously, this sort of ugliness needs to be hidden away from
 *	    the casual user, with an appropriate set of higher level
 *	    functions.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/ctxreg.h>
#include <machine/ioctl_ctx.h>
#include <machine/md_var.h>

#ifndef COMPAT_OLDISA
#error "The ctx device requires the old isa compatibility shims"
#endif

static int     waitvb(int port);

/* state flags */
#define   OPEN        (0x01)	/* device is open */

static int	ctxprobe(struct isa_device *devp);
static int	ctxattach(struct isa_device *devp);
struct isa_driver ctxdriver = {
	INTR_TYPE_MISC,
	ctxprobe,
	ctxattach,
	"ctx"
};
COMPAT_ISA_DRIVER(ctx, ctxdriver);

static	d_open_t	ctxopen;
static	d_close_t	ctxclose;
static	d_read_t	ctxread;
static	d_write_t	ctxwrite;
static	d_ioctl_t	ctxioctl;
#define CDEV_MAJOR 40

static struct cdevsw ctx_cdevsw = {
	.d_open =	ctxopen,
	.d_close =	ctxclose,
	.d_read =	ctxread,
	.d_write =	ctxwrite,
	.d_ioctl =	ctxioctl,
	.d_name =	"ctx",
	.d_maj =	CDEV_MAJOR,
};


#define   LUTSIZE     256	/* buffer size for Look Up Table (LUT) */
#define   PAGESIZE    65536	/* size of one video page, 1/4 of the screen */

/*
 *  Per unit shadow registers (because the dumb hardware is RO)
*/

struct ctx_soft_registers {
	u_char *lutp;
	u_char  cp0;
	u_char  cp1;
	u_char  flag;
	int     iobase;
	caddr_t maddr;
	int     msize;
};


static int
ctxprobe(struct isa_device * devp)
{
	int     status;

	if (inb(devp->id_iobase) == 0xff)	/* 0xff only if board absent */
		status = 0;
	else {
		status = 1; /*XXX uses only one port? */
	}
	return (status);
}

static int
ctxattach(struct isa_device * devp)
{
	struct ctx_soft_registers *sr;
	dev_t dev;

	sr = malloc(sizeof *sr, M_DEVBUF, M_WAITOK | M_ZERO);
	sr->cp0 = 0;	/* zero out the shadow registers */
	sr->cp1 = 0;	/* and the open flag.  wait for  */
	sr->flag = 0;	/* open to malloc the LUT space  */
	sr->iobase = devp->id_iobase;
	sr->maddr = devp->id_maddr;
	sr->msize = devp->id_msize;
	dev = make_dev(&ctx_cdevsw, 0, 0, 0, 0600, "ctx%d", devp->id_unit);
	dev->si_drv1 = sr;
	
	return (1);
}

static int
ctxopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct ctx_soft_registers *sr;
	int     i;

	sr = dev->si_drv1;

	if (sr->flag != 0)	/* someone has already opened us */
		return (EBUSY);

	/* get space for the LUT buffer */

	sr->lutp = malloc(LUTSIZE, M_DEVBUF, M_WAITOK);
	if (sr->lutp == NULL)
		return (ENOMEM);

	sr->flag = OPEN;

/*
	Set up the shadow registers.  We don't actually write these
	values to the control ports until after we finish loading the
	lookup table.
*/
	sr->cp0 |= SEE_STORED_VIDEO;
	if ((kvtop(sr->maddr) == 0xB0000) || (kvtop(sr->maddr) == 0xE0000))
		sr->cp1 |= AB_SELECT;	/* map to B or E if necessary */
	/* but don't enable RAM	  */
/*
	Set up the lookup table initially so that it is transparent.
*/

	outb(sr->iobase + ctx_cp0, (u_char) 0);
	outb(sr->iobase + ctx_cp1, (u_char) (LUT_LOAD_ENABLE | BLANK_DISPLAY));
	for (i = 0; i < LUTSIZE; i++) {
		outb(sr->iobase + ctx_lutaddr, (u_char) i);
		sr->lutp[i] = (u_char) i;
		outb(sr->iobase + ctx_lutdata, (u_char) sr->lutp[i]);
	}
/*
	Disable LUT loading, and push the data in the shadow
	registers into the control ports.
*/
	outb(sr->iobase + ctx_cp0, sr->cp0);
	outb(sr->iobase + ctx_cp1, sr->cp1);
	return (0);	/* successful open.  All ready to go. */
}

static int
ctxclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct ctx_soft_registers *sr;

	sr = dev->si_drv1;
	sr->flag = 0;
	free(sr->lutp, M_DEVBUF);
	sr->lutp = NULL;
	return (0);
}

static int
ctxwrite(dev_t dev, struct uio * uio, int ioflag)
{
	int     status = 0;
	int     page, count, offset;
	struct ctx_soft_registers *sr;

	sr = dev->si_drv1;

	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_offset >= 4 * PAGESIZE)
		page = 4;	/* EOF */
	else
		page = (u_int)uio->uio_offset / PAGESIZE;
	offset = (u_int)uio->uio_offset % PAGESIZE;
	count = min(uio->uio_resid, PAGESIZE - offset);
	while ((page >= 0) && (page <= 3) && (count > 0)) {
		sr->cp0 &= ~3;
		sr->cp0 |= page;
		outb(sr->iobase + ctx_cp0, sr->cp0);

/*
	Before doing the uiomove, we need to "connect" the frame buffer
	ram to the machine bus.  This is done here so that we can have
	several different boards installed, all sharing the same memory
	space... each board is only "connected" to the bus when its memory
	is actually being read or written.  All my instincts tell me that
	I should disable interrupts here, so I have done so.
*/

		disable_intr();
		sr->cp1 |= RAM_ENABLE;
		outb(sr->iobase + ctx_cp1, sr->cp1);
		status = uiomove(sr->maddr + offset, count, uio);
		sr->cp1 &= ~RAM_ENABLE;
		outb(sr->iobase + ctx_cp1, sr->cp1);
		enable_intr();

		page = (u_int)uio->uio_offset / PAGESIZE;
		offset = (u_int)uio->uio_offset % PAGESIZE;
		count = min(uio->uio_resid, PAGESIZE - offset);
	}
	if (uio->uio_resid > 0)
		return (ENOSPC);
	else
		return (status);
}

static int
ctxread(dev_t dev, struct uio * uio, int ioflag)
{
	int     status = 0;
	int     page, count, offset;
	struct ctx_soft_registers *sr;

	sr = dev->si_drv1;

	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_offset >= 4 * PAGESIZE)
		page = 4;	/* EOF */
	else
		page = (u_int)uio->uio_offset / PAGESIZE;
	offset = (u_int)uio->uio_offset % PAGESIZE;
	count = min(uio->uio_resid, PAGESIZE - offset);
	while ((page >= 0) && (page <= 3) && (count > 0)) {
		sr->cp0 &= ~3;
		sr->cp0 |= page;
		outb(sr->iobase + ctx_cp0, sr->cp0);
/*
	Before doing the uiomove, we need to "connect" the frame buffer
	ram to the machine bus.  This is done here so that we can have
	several different boards installed, all sharing the same memory
	space... each board is only "connected" to the bus when its memory
	is actually being read or written.  All my instincts tell me that
	I should disable interrupts here, so I have done so.
*/
		disable_intr();
		sr->cp1 |= RAM_ENABLE;
		outb(sr->iobase + ctx_cp1, sr->cp1);
		status = uiomove(sr->maddr + offset, count, uio);
		sr->cp1 &= ~RAM_ENABLE;
		outb(sr->iobase + ctx_cp1, sr->cp1);
		enable_intr();

		page = (u_int)uio->uio_offset / PAGESIZE;
		offset = (u_int)uio->uio_offset % PAGESIZE;
		count = min(uio->uio_resid, PAGESIZE - offset);
	}
	if (uio->uio_resid > 0)
		return (ENOSPC);
	else
		return (status);
}

static int
ctxioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	int     error;
	int     i;
	struct ctx_soft_registers *sr;

	error = 0;
	sr = dev->si_drv1;

	switch (cmd) {
	case CTX_LIVE:
		sr->cp0 &= ~SEE_STORED_VIDEO;
		outb(sr->iobase + ctx_cp0, sr->cp0);
		break;
	case CTX_GRAB:
		sr->cp0 &= ~SEE_STORED_VIDEO;
		outb(sr->iobase + ctx_cp0, sr->cp0);
		sr->cp0 |= ACQUIRE;
		if (waitvb(sr->iobase))	/* wait for vert blank to start
					 * acquire */
			error = ENODEV;
		outb(sr->iobase + ctx_cp0, sr->cp0);
		if (waitvb(sr->iobase))	/* wait for two more to finish acquire */
			error = ENODEV;
		if (waitvb(sr->iobase))
			error = ENODEV;
		sr->cp0 &= ~ACQUIRE;	/* turn off acquire and turn on
					 * display */
		sr->cp0 |= SEE_STORED_VIDEO;
		outb(sr->iobase + ctx_cp0, sr->cp0);
		break;
	case CTX_H_ORGANIZE:
		sr->cp0 &= ~PAGE_ROTATE;
		outb(sr->iobase + ctx_cp0, sr->cp0);
		break;
	case CTX_V_ORGANIZE:
		sr->cp0 |= PAGE_ROTATE;
		outb(sr->iobase + ctx_cp0, sr->cp0);
		break;
	case CTX_SET_LUT:
		bcopy((u_char *) data, sr->lutp, LUTSIZE);
		outb(sr->iobase + ctx_cp0, (u_char) 0);
		outb(sr->iobase + ctx_cp1, (u_char) (LUT_LOAD_ENABLE | BLANK_DISPLAY));
		for (i = 0; i < LUTSIZE; i++) {
			outb(sr->iobase + ctx_lutaddr, i);
			outb(sr->iobase + ctx_lutdata, sr->lutp[i]);
		}
		outb(sr->iobase + ctx_cp0, sr->cp0);	/* restore control
							 * registers */
		outb(sr->iobase + ctx_cp1, sr->cp1);
		break;
	case CTX_GET_LUT:
		bcopy(sr->lutp, (u_char *) data, LUTSIZE);
		break;
	default:
		error = ENODEV;
	}

	return (error);
}

static int
waitvb(int port)
{				/* wait for a vertical blank,  */
	if (inb(port) == 0xff)	/* 0xff means no board present */
		return (1);

	while ((inb(port) & VERTICAL_BLANK) != 0) {
	}
	while ((inb(port) & VERTICAL_BLANK) == 0) {
	}

	return (0);
}
