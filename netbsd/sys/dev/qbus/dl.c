/*	$NetBSD: dl.c,v 1.20 2002/03/17 19:41:01 atatat Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * Copyright (c) 1997  Ben Harris.  All rights reserved.
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
 * Copyright (c) 1982, 1986, 1990, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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

/*
 * dl.c -- Device driver for the DL11 and DLV11 serial cards.
 *
 * OS-interface code derived from the dz and dca (hp300) drivers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dl.c,v 1.20 2002/03/17 19:41:01 atatat Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/qbus/ubavar.h>

#include <dev/qbus/dlreg.h>

#include "ioconf.h"

struct dl_softc {
	struct device	sc_dev;
	struct evcnt	sc_rintrcnt;
	struct evcnt	sc_tintrcnt;
	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;
	struct tty	*sc_tty;
};

static	int	dl_match (struct device *, struct cfdata *, void *);
static	void	dl_attach (struct device *, struct device *, void *);
static	void	dlrint (void *);
static	void	dlxint (void *);
static	void	dlstart (struct tty *);
static	int	dlparam (struct tty *, struct termios *);
static	void	dlbrk (struct dl_softc *, int);
struct	tty *	dltty (dev_t);

cdev_decl(dl);

struct cfattach dl_ca = {
	sizeof(struct dl_softc), dl_match, dl_attach
};

#define	DL_READ_WORD(reg) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, reg)
#define	DL_WRITE_WORD(reg, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, reg, val)
#define	DL_WRITE_BYTE(reg, val) \
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, reg, val)

/* Autoconfig handles: setup the controller to interrupt, */
/* then complete the housecleaning for full operation */

static int
dl_match (struct device *parent, struct cfdata *cf, void *aux)
{
	struct uba_attach_args *ua = aux;

#ifdef DL_DEBUG
	printf("Probing for dl at %lo ... ", (long)ua->ua_iaddr);
#endif

	bus_space_write_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR, DL_XCSR_TXIE);
	if (bus_space_read_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR) !=
	    (DL_XCSR_TXIE | DL_XCSR_TX_READY)) {
#ifdef DL_DEBUG
	        printf("failed (step 1; XCSR = %.4b)\n", 
		    bus_space_read_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR), 
		    DL_XCSR_BITS);
#endif
		return 0;
	}
	
	/*
	 * We have to force an interrupt so the uba driver can work
	 * out where we are.  Unfortunately, the only way to make a
	 * DL11 interrupt is to get it to send or receive a
	 * character.  We'll send a NUL and hope it doesn't hurt
	 * anything.
	 */

	bus_space_write_1(ua->ua_iot, ua->ua_ioh, DL_UBA_XBUFL, '\0');
#if 0 /* This test seems to fail 2/3 of the time :-( */
	if (dladdr->dl_xcsr != (DL_XCSR_TXIE)) {
#ifdef DL_DEBUG
	        printf("failed (step 2; XCSR = %.4b)\n", dladdr->dl_xcsr,
		       DL_XCSR_BITS);
#endif
		return 0;
	}
#endif
	DELAY(100000); /* delay 1/10 s for character to transmit */
	if (bus_space_read_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR) !=
	    (DL_XCSR_TXIE | DL_XCSR_TX_READY)) {
#ifdef DL_DEBUG
	        printf("failed (step 3; XCSR = %.4b)\n", 
		    bus_space_read_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR),
		    DL_XCSR_BITS);
#endif
		return 0;
	}


        /* What else do I need to do? */

	return 1;

}

static void
dl_attach (struct device *parent, struct device *self, void *aux)
{
	struct dl_softc *sc = (void *)self;
	struct uba_attach_args *ua = aux;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	
	/* Tidy up the device */

	DL_WRITE_WORD(DL_UBA_RCSR, DL_RCSR_RXIE);
	DL_WRITE_WORD(DL_UBA_XCSR, DL_XCSR_TXIE);

	/* Initialize our softc structure. Should be done in open? */
	
	sc->sc_tty = ttymalloc();
	tty_attach(sc->sc_tty);

	/* Now register the TX & RX interrupt handlers */
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec,
		dlxint, sc, &sc->sc_tintrcnt);
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec - 4,
		dlrint, sc, &sc->sc_rintrcnt);
	evcnt_attach_dynamic(&sc->sc_rintrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
		sc->sc_dev.dv_xname, "rintr");
	evcnt_attach_dynamic(&sc->sc_tintrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
		sc->sc_dev.dv_xname, "tintr");

	printf("\n");
}

/* Receiver Interrupt Handler */

static void
dlrint(void *arg)
{
	struct dl_softc *sc = arg;

	if (DL_READ_WORD(DL_UBA_RCSR) & DL_RCSR_RX_DONE) {
		struct tty *tp = sc->sc_tty;
		unsigned c;
		int cc;

	        c = DL_READ_WORD(DL_UBA_RBUF);
		cc = c & 0xFF;

		if (!(tp->t_state & TS_ISOPEN)) {
			wakeup((caddr_t)&tp->t_rawq);
			return;
		}

		if (c & DL_RBUF_OVERRUN_ERR) {
			/*
			 * XXX: This should really be logged somwhere
			 * else where we can afford the time.
			 */
			log(LOG_WARNING, "%s: rx overrun\n",
			    sc->sc_dev.dv_xname);
		}
		if (c & DL_RBUF_FRAMING_ERR)
			cc |= TTY_FE;
		if (c & DL_RBUF_PARITY_ERR)
			cc |= TTY_PE;

		(*tp->t_linesw->l_rint)(cc, tp);
#if defined(DIAGNOSTIC)
	} else {
		log(LOG_WARNING, "%s: stray rx interrupt\n",
		    sc->sc_dev.dv_xname);
#endif
	}
}

/* Transmitter Interrupt Handler */

static void
dlxint(void *arg)
{
	struct dl_softc *sc = arg;
	struct tty *tp = sc->sc_tty;

	tp->t_state &= ~(TS_BUSY | TS_FLUSH);
	(*tp->t_linesw->l_start)(tp);
       
	return;
}

int
dlopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp;
	struct dl_softc *sc;
	int unit;

	unit = minor(dev);

	if (unit >= dl_cd.cd_ndevs || dl_cd.cd_devs[unit] == NULL)
		return ENXIO;
	sc = dl_cd.cd_devs[unit];

	tp = sc->sc_tty;
	if (tp == NULL)
		return ENODEV;
	tp->t_oproc = dlstart;
	tp->t_param = dlparam;
	tp->t_dev = dev;

	if (!(tp->t_state & TS_ISOPEN)) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		/* No modem control, so set CLOCAL. */
		tp->t_cflag = TTYDEF_CFLAG | CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		
		dlparam(tp, &tp->t_termios);
		ttsetwater(tp);
		
	} else if ((tp->t_state & TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return EBUSY;

	return ((*tp->t_linesw->l_open)(dev, tp));
}

/*ARGSUSED*/
int
dlclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct dl_softc *sc = dl_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;

	(*tp->t_linesw->l_close)(tp, flag);

	/* Make sure a BREAK state is not left enabled. */
	dlbrk(sc, 0);

	return (ttyclose(tp));
}

int
dlread(dev_t dev, struct uio *uio, int flag)
{
	struct dl_softc *sc = dl_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

int
dlwrite(dev_t dev, struct uio *uio, int flag)
{
	struct dl_softc *sc = dl_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
dlpoll(dev_t dev, int events, struct proc *p)
{
	struct dl_softc *sc = dl_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;
 
	return ((*tp->t_linesw->l_poll)(tp, events, p));
}

int
dlioctl(dev_t dev, unsigned long cmd, caddr_t data, int flag, struct proc *p)
{
	struct dl_softc *sc = dl_cd.cd_devs[minor(dev)];
        struct tty *tp = sc->sc_tty;
        int error;


        error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, p);
        if (error != EPASSTHROUGH)
                return (error);

        error = ttioctl(tp, cmd, data, flag, p);
        if (error != EPASSTHROUGH)
                return (error);

	switch (cmd) {
	       
        case TIOCSBRK:
                dlbrk(sc, 1);
                break;

        case TIOCCBRK:
                dlbrk(sc, 0);
                break;

        case TIOCMGET:
		/* No modem control, assume they're all low. */
                *(int *)data = 0;
                break;

        default:
                return (EPASSTHROUGH);
        }
        return (0);
}

struct tty *
dltty(dev_t dev)
{
	struct dl_softc *sc = dl_cd.cd_devs[minor(dev)];

	return sc->sc_tty;
}

void
dlstop(struct tty *tp, int flag)
{
	int s = spltty();

	if ((tp->t_state & (TS_BUSY|TS_TTSTOP)) == TS_BUSY)
		tp->t_state |= TS_FLUSH;
        splx(s);
}

static void
dlstart(struct tty *tp)
{
	struct dl_softc *sc = dl_cd.cd_devs[minor(tp->t_dev)];
	int s = spltty();

        if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
                goto out;
        if (tp->t_outq.c_cc <= tp->t_lowat) {
                if (tp->t_state & TS_ASLEEP) {
                        tp->t_state &= ~TS_ASLEEP;
                        wakeup((caddr_t)&tp->t_outq);
                }
                selwakeup(&tp->t_wsel);
        }
        if (tp->t_outq.c_cc == 0)
                goto out;


	if (DL_READ_WORD(DL_UBA_XCSR) & DL_XCSR_TX_READY) {
		tp->t_state |= TS_BUSY;
		DL_WRITE_BYTE(DL_UBA_XBUFL, getc(&tp->t_outq));
	}
out:
	splx(s);
	return;
}

/*ARGSUSED*/
static int
dlparam(struct tty *tp, struct termios *t)
{
	/*
	 * All this kind of stuff (speed, character format, whatever)
	 * is set by jumpers on the card.  Changing it is thus rather
	 * tricky for a mere device driver.
	 */
	return 0;
}

static void
dlbrk(struct dl_softc *sc, int state)
{
	int s = spltty();

	if (state) {
		DL_WRITE_WORD(DL_UBA_XCSR, DL_READ_WORD(DL_UBA_XCSR) |
		    DL_XCSR_TX_BREAK);
	} else {
		DL_WRITE_WORD(DL_UBA_XCSR, DL_READ_WORD(DL_UBA_XCSR) &
		    ~DL_XCSR_TX_BREAK);
	}
	splx(s);
}
