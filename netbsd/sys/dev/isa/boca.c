/*	$NetBSD: boca.c,v 1.36 2002/01/07 21:47:04 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 *
 * This code is derived from public-domain software written by
 * Roland McGrath, and information provided by David Muir Sharnoff.
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
 *	This product includes software developed by Charles M. Hannum.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: boca.c,v 1.36 2002/01/07 21:47:04 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>
#include <sys/kernel.h>
#include <sys/callout.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/isa/isavar.h>
#include <dev/isa/com_multi.h>

#define	NSLAVES	8

struct boca_softc {
	struct device sc_dev;
	void *sc_ih;

	bus_space_tag_t sc_iot;
	int sc_iobase;

	int sc_alive;			/* mask of slave units attached */
	void *sc_slaves[NSLAVES];	/* com device unit numbers */
	bus_space_handle_t sc_slaveioh[NSLAVES];
	struct callout fixup;
};

int bocaprobe __P((struct device *, struct cfdata *, void *));
void bocaattach __P((struct device *, struct device *, void *));
int bocaintr __P((void *));
void boca_fixup __P((void *));
int bocaprint __P((void *, const char *));

struct cfattach boca_ca = {
	sizeof(struct boca_softc), bocaprobe, bocaattach,
};

int
bocaprobe(parent, self, aux)
	struct device *parent;
	struct cfdata *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int i, iobase, rv = 1;

	/*
	 * Do the normal com probe for the first UART and assume
	 * its presence, and the ability to map the other UARTS,
	 * means there is a multiport board there.
	 * XXX Needs more robustness.
	 */

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISACF_PORT_DEFAULT)
		return (0);
	if (ia->ia_irq[0].ir_irq == ISACF_IRQ_DEFAULT)
		return (0);

	/* if the first port is in use as console, then it. */
	if (com_is_console(iot, iobase, 0))
		goto checkmappings;

	if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
		rv = 0;
		goto out;
	}
	rv = comprobe1(iot, ioh);
	bus_space_unmap(iot, ioh, COM_NPORTS);
	if (rv == 0)
		goto out;

checkmappings:
	for (i = 1, iobase = ia->ia_io[0].ir_addr; i < NSLAVES; i++) {
		iobase += COM_NPORTS;

		if (com_is_console(iot, iobase, 0))
			continue;

		if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
			rv = 0;
			goto out;
		}
		bus_space_unmap(iot, ioh, COM_NPORTS);
	}

out:
	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = NSLAVES * COM_NPORTS;

		ia->ia_nirq = 1;

		ia->ia_niomem = 0;
		ia->ia_ndrq = 0;
	}
	return (rv);
}

int
bocaprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct commulti_attach_args *ca = aux;

	if (pnp)
		printf("com at %s", pnp);
	printf(" slave %d", ca->ca_slave);
	return (UNCONF);
}

void
bocaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct boca_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct commulti_attach_args ca;
	bus_space_tag_t iot = ia->ia_iot;
	int i, iobase;

	printf("\n");

	sc->sc_iot = ia->ia_iot;
	sc->sc_iobase = ia->ia_io[0].ir_addr;

	for (i = 0; i < NSLAVES; i++) {
		iobase = sc->sc_iobase + i * COM_NPORTS;
		if (!com_is_console(iot, iobase, &sc->sc_slaveioh[i]) &&
		    bus_space_map(iot, iobase, COM_NPORTS, 0,
			&sc->sc_slaveioh[i])) {
			printf("%s: can't map i/o space for slave %d\n",
			     sc->sc_dev.dv_xname, i);
			return;
		}
	}

	for (i = 0; i < NSLAVES; i++) {

		ca.ca_slave = i;
		ca.ca_iot = sc->sc_iot;
		ca.ca_ioh = sc->sc_slaveioh[i];
		ca.ca_iobase = sc->sc_iobase + i * COM_NPORTS;
		ca.ca_noien = 0;

		sc->sc_slaves[i] = config_found(self, &ca, bocaprint);
		if (sc->sc_slaves[i] != NULL)
			sc->sc_alive |= 1 << i;
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_SERIAL, bocaintr, sc);
	callout_init(&sc->fixup);
	callout_reset(&sc->fixup, hz/10, boca_fixup, sc);
}

int
bocaintr(arg)
	void *arg;
{
	struct boca_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	int alive = sc->sc_alive;
	int bits;

	bits = bus_space_read_1(iot, sc->sc_slaveioh[0], com_scratch) & alive;
	if (bits == 0)
		return (0);

	for (;;) {
#define	TRY(n) \
		if (bits & (1 << (n))) { \
			if (comintr(sc->sc_slaves[n]) == 0) { \
				printf("%s: bogus intr for port %d\n", \
				    sc->sc_dev.dv_xname, n); \
			} \
		}
		TRY(0);
		TRY(1);
		TRY(2);
		TRY(3);
		TRY(4);
		TRY(5);
		TRY(6);
		TRY(7);
#undef TRY
		bits = bus_space_read_1(iot, sc->sc_slaveioh[0],
		    com_scratch) & alive;
		if (bits == 0) {
			return (1);
		}
 	}
}

void
boca_fixup(v)
	void *v;
{
	struct boca_softc *sc = v;
	int alive = sc->sc_alive;
	int i, s;

	s = splserial();

	for (i = 0; i < 8; i++) {
		if (alive & (1 << (i)))
			comintr(sc->sc_slaves[i]);
	}
	callout_reset(&sc->fixup, hz/10, boca_fixup, sc);
	splx(s);
}
