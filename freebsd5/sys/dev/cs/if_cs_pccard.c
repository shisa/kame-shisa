/*
 * Copyright (c) 1999 M. Warner Losh <imp@village.org> 
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/cs/if_cs_pccard.c,v 1.8 2003/10/31 18:31:58 brooks Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
 
#include <net/ethernet.h> 
#include <net/if.h> 
#include <net/if_arp.h>

#include <dev/cs/if_csvar.h>
#include <dev/cs/if_csreg.h>
#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccarddevs.h>

#include "card_if.h"

static const struct pccard_product cs_pccard_products[] = {
	PCMCIA_CARD(IBM, ETHERJET, 0),
	{ NULL }
};
static int
cs_pccard_match(device_t dev)
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, cs_pccard_products,
	    sizeof(cs_pccard_products[0]), NULL)) != NULL) {
		if (pp->pp_name != NULL)
			device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return EIO;
}

static int
cs_pccard_probe(device_t dev)
{
	int error;

	error = cs_cs89x0_probe(dev);
        cs_release_resources(dev);
        return (error);
}

static int
cs_pccard_attach(device_t dev)
{
        struct cs_softc *sc = device_get_softc(dev);
        int error;
        
	error = cs_alloc_port(dev, sc->port_rid, CS_89x0_IO_PORTS);
	if (error != 0)
		goto bad;
        error = cs_alloc_irq(dev, sc->irq_rid, 0);
	if (error != 0)
		goto bad;
        error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
	    csintr, sc, &sc->irq_handle);
        if (error != 0)
		goto bad;

        return (cs_attach(dev));
bad:
	cs_release_resources(dev);
	return (error);
}

static device_method_t cs_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
#ifdef CS_HAS_DETACH
	DEVMETHOD(device_detach,	cs_detach),
#endif

	/* Card interface */
	DEVMETHOD(card_compat_match,	cs_pccard_match),
	DEVMETHOD(card_compat_probe,	cs_pccard_probe),
	DEVMETHOD(card_compat_attach,	cs_pccard_attach),

	{ 0, 0 }
};

static driver_t cs_pccard_driver = {
	"cs",
	cs_pccard_methods,
	sizeof(struct cs_softc),
};

extern devclass_t cs_devclass;

DRIVER_MODULE(cs, pccard, cs_pccard_driver, cs_devclass, 0, 0);
MODULE_DEPEND(cs, pccard, 1, 1, 1);
MODULE_DEPEND(cs, ether, 1, 1, 1);
