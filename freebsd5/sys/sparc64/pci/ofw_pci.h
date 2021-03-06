/*
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001, 2003 by Thomas Moestl <tmm@FreeBSD.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: psychoreg.h,v 1.8 2001/09/10 16:17:06 eeh Exp
 *
 * $FreeBSD: src/sys/sparc64/pci/ofw_pci.h,v 1.6 2003/07/01 14:52:47 tmm Exp $
 */

#ifndef _SPARC64_PCI_OFW_PCI_H_
#define _SPARC64_PCI_OFW_PCI_H_

#include <machine/ofw_bus.h>

typedef u_int32_t ofw_pci_intr_t;

#include "ofw_pci_if.h"

/* PCI range child spaces. XXX: are these MI? */
#define	PCI_CS_CONFIG	0x00
#define	PCI_CS_IO	0x01
#define	PCI_CS_MEM32	0x02
#define	PCI_CS_MEM64	0x03

struct ofw_pci_imap {
	u_int32_t	phys_hi;
	u_int32_t	phys_mid;
	u_int32_t	phys_lo;
	ofw_pci_intr_t	intr;
	phandle_t	child_node;
	phandle_t	child_intr;
};

struct ofw_pci_imap_msk {
	u_int32_t	phys_hi;
	u_int32_t	phys_mid;
	u_int32_t	phys_lo;
	ofw_pci_intr_t	intr;
};

u_int8_t ofw_pci_alloc_busno(phandle_t);

#ifdef OFW_NEWPCI

static __inline phandle_t
ofw_pci_get_node(device_t dev)
{

	return (OFW_PCI_GET_NODE(device_get_parent(dev), dev));
}

#else

struct ofw_pci_bdesc;
typedef void ofw_pci_binit_t(device_t, struct ofw_pci_bdesc *);

struct ofw_pci_bdesc {
	u_int	obd_bus;
	u_int	obd_slot;
	u_int	obd_func;
	u_int	obd_secbus;
	u_int	obd_subbus;
	ofw_pci_binit_t	*obd_init;
	struct ofw_pci_bdesc	*obd_super;
};

obr_callback_t ofw_pci_orb_callback;
ofw_pci_binit_t ofw_pci_binit;
void ofw_pci_init(device_t, phandle_t, ofw_pci_intr_t, struct ofw_pci_bdesc *);
phandle_t ofw_pci_find_node(int, int, int);
phandle_t ofw_pci_node(device_t);
#endif

#endif /* ! _SPARC64_PCI_OFW_PCI_H_ */
