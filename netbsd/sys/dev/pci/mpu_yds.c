/*	$NetBSD: mpu_yds.c,v 1.2 2001/11/13 07:48:46 lukem Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
__KERNEL_RCSID(0, "$NetBSD: mpu_yds.c,v 1.2 2001/11/13 07:48:46 lukem Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/midiio.h>

#include <machine/bus.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/mpuvar.h>
#include <dev/ic/ac97var.h>
#include <dev/pci/ydsreg.h>
#include <dev/pci/ydsvar.h>

static int	mpu_yds_match __P((struct device *, struct cfdata *, void *));
static void	mpu_yds_attach __P((struct device *, struct device *, void *));

struct cfattach mpu_yds_ca = {
	sizeof (struct mpu_softc), mpu_yds_match, mpu_yds_attach
};

static int
mpu_yds_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct audio_attach_args *aa = (struct audio_attach_args *)aux;
	struct yds_softc *ysc = (struct yds_softc *)parent;
	struct mpu_softc sc;

	if (aa->type != AUDIODEV_TYPE_MPU)
		return (0);
	memset(&sc, 0, sizeof sc);
	sc.ioh = ysc->sc_mpu_ioh;
	sc.iot = ysc->sc_mpu_iot;
	return (mpu_find(&sc));
}

static void
mpu_yds_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct yds_softc *ysc = (struct yds_softc *)parent;
	struct mpu_softc *sc = (struct mpu_softc *)self;

	sc->ioh = ysc->sc_mpu_ioh;
	sc->iot = ysc->sc_mpu_iot;
	sc->model = "Yamaha DS-1 MIDI UART";
#ifndef AUDIO_NO_POWER_CTL
	sc->powerctl = 0;
#endif

	mpu_attach(sc);
}