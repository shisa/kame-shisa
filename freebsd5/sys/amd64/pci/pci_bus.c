/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/amd64/pci/pci_bus.c,v 1.106 2003/12/06 23:19:47 peter Exp $");

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <isa/isavar.h>
#include <machine/legacyvar.h>
#include <machine/pci_cfgreg.h>
#include <machine/segments.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>

#include "pcib_if.h"

int
legacy_pcib_maxslots(device_t dev)
{
	return 31;
}

/* read configuration space register */

u_int32_t
legacy_pcib_read_config(device_t dev, int bus, int slot, int func,
			int reg, int bytes)
{
	return(pci_cfgregread(bus, slot, func, reg, bytes));
}

/* write configuration space register */

void
legacy_pcib_write_config(device_t dev, int bus, int slot, int func,
			 int reg, u_int32_t data, int bytes)
{
	pci_cfgregwrite(bus, slot, func, reg, data, bytes);
}

/* route interrupt */

static int
legacy_pcib_route_interrupt(device_t pcib, device_t dev, int pin)
{

	/* No routing possible */
	return (PCI_INVALID_IRQ);
}

static const char *
legacy_pcib_is_host_bridge(int bus, int slot, int func,
			  u_int32_t id, u_int8_t class, u_int8_t subclass,
			  u_int8_t *busnum)
{
	const char *s = NULL;
	static u_int8_t pxb[4];	/* hack for 450nx */

	*busnum = 0;

	switch (id) {
	case 0x12258086:
		s = "Intel 824?? host to PCI bridge";
		/* XXX This is a guess */
		/* *busnum = legacy_pcib_read_config(0, bus, slot, func, 0x41, 1); */
		*busnum = bus;
		break;
	case 0x71208086:
		s = "Intel 82810 (i810 GMCH) Host To Hub bridge";
		break;
	case 0x71228086:
		s = "Intel 82810-DC100 (i810-DC100 GMCH) Host To Hub bridge";
		break;
	case 0x71248086:
		s = "Intel 82810E (i810E GMCH) Host To Hub bridge";
		break;
	case 0x11308086:
		s = "Intel 82815 (i815 GMCH) Host To Hub bridge";
		break;
	case 0x71808086:
		s = "Intel 82443LX (440 LX) host to PCI bridge";
		break;
	case 0x71908086:
		s = "Intel 82443BX (440 BX) host to PCI bridge";
		break;
	case 0x71928086:
		s = "Intel 82443BX host to PCI bridge (AGP disabled)";
		break;
	case 0x71948086:
		s = "Intel 82443MX host to PCI bridge";
		break;
	case 0x71a08086:
		s = "Intel 82443GX host to PCI bridge";
		break;
	case 0x71a18086:
		s = "Intel 82443GX host to AGP bridge";
		break;
	case 0x71a28086:
		s = "Intel 82443GX host to PCI bridge (AGP disabled)";
		break;
	case 0x84c48086:
		s = "Intel 82454KX/GX (Orion) host to PCI bridge";
		*busnum = legacy_pcib_read_config(0, bus, slot, func, 0x4a, 1);
		break;
	case 0x84ca8086:
		/*
		 * For the 450nx chipset, there is a whole bundle of
		 * things pretending to be host bridges. The MIOC will 
		 * be seen first and isn't really a pci bridge (the
		 * actual busses are attached to the PXB's). We need to 
		 * read the registers of the MIOC to figure out the
		 * bus numbers for the PXB channels.
		 *
		 * Since the MIOC doesn't have a pci bus attached, we
		 * pretend it wasn't there.
		 */
		pxb[0] = legacy_pcib_read_config(0, bus, slot, func,
						0xd0, 1); /* BUSNO[0] */
		pxb[1] = legacy_pcib_read_config(0, bus, slot, func,
						0xd1, 1) + 1;	/* SUBA[0]+1 */
		pxb[2] = legacy_pcib_read_config(0, bus, slot, func,
						0xd3, 1); /* BUSNO[1] */
		pxb[3] = legacy_pcib_read_config(0, bus, slot, func,
						0xd4, 1) + 1;	/* SUBA[1]+1 */
		return NULL;
	case 0x84cb8086:
		switch (slot) {
		case 0x12:
			s = "Intel 82454NX PXB#0, Bus#A";
			*busnum = pxb[0];
			break;
		case 0x13:
			s = "Intel 82454NX PXB#0, Bus#B";
			*busnum = pxb[1];
			break;
		case 0x14:
			s = "Intel 82454NX PXB#1, Bus#A";
			*busnum = pxb[2];
			break;
		case 0x15:
			s = "Intel 82454NX PXB#1, Bus#B";
			*busnum = pxb[3];
			break;
		}
		break;

		/* AMD -- vendor 0x1022 */
	case 0x30001022:
		s = "AMD Elan SC520 host to PCI bridge";
#ifdef CPU_ELAN
		init_AMD_Elan_sc520();
#else
		printf(
"*** WARNING: missing CPU_ELAN -- timekeeping may be wrong\n");
#endif
		break;
	case 0x70061022:
		s = "AMD-751 host to PCI bridge";
		break;
	case 0x700e1022:
		s = "AMD-761 host to PCI bridge";
		break;

		/* SiS -- vendor 0x1039 */
	case 0x04961039:
		s = "SiS 85c496";
		break;
	case 0x04061039:
		s = "SiS 85c501";
		break;
	case 0x06011039:
		s = "SiS 85c601";
		break;
	case 0x55911039:
		s = "SiS 5591 host to PCI bridge";
		break;
	case 0x00011039:
		s = "SiS 5591 host to AGP bridge";
		break;

		/* VLSI -- vendor 0x1004 */
	case 0x00051004:
		s = "VLSI 82C592 Host to PCI bridge";
		break;

		/* XXX Here is MVP3, I got the datasheet but NO M/B to test it  */
		/* totally. Please let me know if anything wrong.            -F */
		/* XXX need info on the MVP3 -- any takers? */
	case 0x05981106:
		s = "VIA 82C598MVP (Apollo MVP3) host bridge";
		break;

		/* AcerLabs -- vendor 0x10b9 */
		/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
		/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x154110b9:
		s = "AcerLabs M1541 (Aladdin-V) PCI host bridge";
		break;

		/* OPTi -- vendor 0x1045 */
	case 0xc7011045:
		s = "OPTi 82C700 host to PCI bridge";
		break;
	case 0xc8221045:
		s = "OPTi 82C822 host to PCI Bridge";
		break;

		/* ServerWorks -- vendor 0x1166 */
	case 0x00051166:
		s = "ServerWorks NB6536 2.0HE host to PCI bridge";
		*busnum = legacy_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;
	
	case 0x00061166:
		/* FALLTHROUGH */
	case 0x00081166:
		/* FALLTHROUGH */
	case 0x02011166:
		/* FALLTHROUGH */
	case 0x010f1014: /* IBM re-badged ServerWorks chipset */
		s = "ServerWorks host to PCI bridge";
		*busnum = legacy_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;

	case 0x00091166:
		s = "ServerWorks NB6635 3.0LE host to PCI bridge";
		*busnum = legacy_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;

	case 0x00101166:
		s = "ServerWorks CIOB30 host to PCI bridge";
		*busnum = legacy_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;

	case 0x00111166:
		/* FALLTHROUGH */
	case 0x03021014: /* IBM re-badged ServerWorks chipset */
		s = "ServerWorks CMIC-HE host to PCI-X bridge";
		*busnum = legacy_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;

		/* XXX unknown chipset, but working */
	case 0x00171166:
		/* FALLTHROUGH */
	case 0x01011166:
		s = "ServerWorks host to PCI bridge(unknown chipset)";
		*busnum = legacy_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;

		/* Integrated Micro Solutions -- vendor 0x10e0 */
	case 0x884910e0:
		s = "Integrated Micro Solutions VL Bridge";
		break;

	default:
		if (class == PCIC_BRIDGE && subclass == PCIS_BRIDGE_HOST)
			s = "Host to PCI bridge";
		break;
	}

	return s;
}

/*
 * Scan the first pci bus for host-pci bridges and add pcib instances
 * to the nexus for each bridge.
 */
static void
legacy_pcib_identify(driver_t *driver, device_t parent)
{
	int bus, slot, func;
	u_int8_t  hdrtype;
	int found = 0;
	int pcifunchigh;
	int found824xx = 0;
	int found_orion = 0;
	device_t child;
	devclass_t pci_devclass;

	if (pci_cfgregopen() == 0)
		return;
	/*
	 * Check to see if we haven't already had a PCI bus added
	 * via some other means.  If we have, bail since otherwise
	 * we're going to end up duplicating it.
	 */
	if ((pci_devclass = devclass_find("pci")) && 
		devclass_get_device(pci_devclass, 0))
		return;


	bus = 0;
 retry:
	for (slot = 0; slot <= PCI_SLOTMAX; slot++) {
		func = 0;
		hdrtype = legacy_pcib_read_config(0, bus, slot, func,
						 PCIR_HDRTYPE, 1);
		/*
		 * When enumerating bus devices, the standard says that
		 * one should check the header type and ignore the slots whose
		 * header types that the software doesn't know about.  We use
		 * this to filter out devices.
		 */
		if ((hdrtype & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
			continue;
		if ((hdrtype & PCIM_MFDEV) && 
		    (!found_orion || hdrtype != 0xff))
			pcifunchigh = PCI_FUNCMAX;
		else
			pcifunchigh = 0;
		for (func = 0; func <= pcifunchigh; func++) {
			/*
			 * Read the IDs and class from the device.
			 */
			u_int32_t id;
			u_int8_t class, subclass, busnum;
			const char *s;
			device_t *devs;
			int ndevs, i;

			id = legacy_pcib_read_config(0, bus, slot, func,
						    PCIR_DEVVENDOR, 4);
			if (id == -1)
				continue;
			class = legacy_pcib_read_config(0, bus, slot, func,
						       PCIR_CLASS, 1);
			subclass = legacy_pcib_read_config(0, bus, slot, func,
							  PCIR_SUBCLASS, 1);

			s = legacy_pcib_is_host_bridge(bus, slot, func,
						      id, class, subclass,
						      &busnum);
			if (s == NULL)
				continue;

			/*
			 * Check to see if the physical bus has already
			 * been seen.  Eg: hybrid 32 and 64 bit host
			 * bridges to the same logical bus.
			 */
			if (device_get_children(parent, &devs, &ndevs) == 0) {
				for (i = 0; s != NULL && i < ndevs; i++) {
					if (strcmp(device_get_name(devs[i]),
					    "pcib") != 0)
						continue;
					if (legacy_get_pcibus(devs[i]) == busnum)
						s = NULL;
				}
				free(devs, M_TEMP);
			}

			if (s == NULL)
				continue;
			/*
			 * Add at priority 100 to make sure we
			 * go after any motherboard resources
			 */
			child = BUS_ADD_CHILD(parent, 100,
					      "pcib", busnum);
			device_set_desc(child, s);
			legacy_set_pcibus(child, busnum);

			found = 1;
			if (id == 0x12258086)
				found824xx = 1;
			if (id == 0x84c48086)
				found_orion = 1;
		}
	}
	if (found824xx && bus == 0) {
		bus++;
		goto retry;
	}

	/*
	 * Make sure we add at least one bridge since some old
	 * hardware doesn't actually have a host-pci bridge device.
	 * Note that pci_cfgregopen() thinks we have PCI devices..
	 */
	if (!found) {
		if (bootverbose)
			printf(
	"legacy_pcib_identify: no bridge found, adding pcib0 anyway\n");
		child = BUS_ADD_CHILD(parent, 100, "pcib", 0);
		legacy_set_pcibus(child, 0);
	}
}

static int
legacy_pcib_probe(device_t dev)
{

	if (pci_cfgregopen() == 0)
		return ENXIO;
	return -100;
}

int
legacy_pcib_attach(device_t dev)
{

	device_add_child(dev, "pci", pcib_get_bus(dev));

	return bus_generic_attach(dev);
}

int
legacy_pcib_read_ivar(device_t dev, device_t child, int which,
    uintptr_t *result)
{

	switch (which) {
	case  PCIB_IVAR_BUS:
		*result = legacy_get_pcibus(dev);
		return 0;
	}
	return ENOENT;
}

int
legacy_pcib_write_ivar(device_t dev, device_t child, int which,
    uintptr_t value)
{

	switch (which) {
	case  PCIB_IVAR_BUS:
		legacy_set_pcibus(dev, value);
		return 0;
	}
	return ENOENT;
}


static device_method_t legacy_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	legacy_pcib_identify),
	DEVMETHOD(device_probe,		legacy_pcib_probe),
	DEVMETHOD(device_attach,	legacy_pcib_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	legacy_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	legacy_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	legacy_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	legacy_pcib_read_config),
	DEVMETHOD(pcib_write_config,	legacy_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	legacy_pcib_route_interrupt),

	{ 0, 0 }
};

static driver_t legacy_pcib_driver = {
	"pcib",
	legacy_pcib_methods,
	1,
};

DRIVER_MODULE(pcib, legacy, legacy_pcib_driver, pcib_devclass, 0, 0);


/*
 * Provide a device to "eat" the host->pci bridges that we dug up above
 * and stop them showing up twice on the probes.  This also stops them
 * showing up as 'none' in pciconf -l.
 */
static int
pci_hostb_probe(device_t dev)
{
	u_int32_t id;

	id = pci_get_devid(dev);

	switch (id) {

	/* VIA VT82C596 Power Managment Function */
	case 0x30501106:
		return ENXIO;

	default:
		break;
	}

	if (pci_get_class(dev) == PCIC_BRIDGE &&
	    pci_get_subclass(dev) == PCIS_BRIDGE_HOST) {
		device_set_desc(dev, "Host to PCI bridge");
		device_quiet(dev);
		return -10000;
	}
	return ENXIO;
}

static int
pci_hostb_attach(device_t dev)
{

	return 0;
}

static device_method_t pci_hostb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pci_hostb_probe),
	DEVMETHOD(device_attach,	pci_hostb_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	{ 0, 0 }
};
static driver_t pci_hostb_driver = {
	"hostb",
	pci_hostb_methods,
	1,
};
static devclass_t pci_hostb_devclass;

DRIVER_MODULE(hostb, pci, pci_hostb_driver, pci_hostb_devclass, 0, 0);


/*
 * Install placeholder to claim the resources owned by the
 * PCI bus interface.  This could be used to extract the 
 * config space registers in the extreme case where the PnP
 * ID is available and the PCI BIOS isn't, but for now we just
 * eat the PnP ID and do nothing else.
 *
 * XXX we should silence this probe, as it will generally confuse 
 * people.
 */
static struct isa_pnp_id pcibus_pnp_ids[] = {
	{ 0x030ad041 /* PNP0A03 */, "PCI Bus" },
	{ 0 }
};

static int
pcibus_pnp_probe(device_t dev)
{
	int result;
	
	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, pcibus_pnp_ids)) <= 0)
		device_quiet(dev);
	return(result);
}

static int
pcibus_pnp_attach(device_t dev)
{
	return(0);
}

static device_method_t pcibus_pnp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcibus_pnp_probe),
	DEVMETHOD(device_attach,	pcibus_pnp_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	{ 0, 0 }
};

static driver_t pcibus_pnp_driver = {
	"pcibus_pnp",
	pcibus_pnp_methods,
	1,		/* no softc */
};

static devclass_t pcibus_pnp_devclass;

DRIVER_MODULE(pcibus_pnp, isa, pcibus_pnp_driver, pcibus_pnp_devclass, 0, 0);
