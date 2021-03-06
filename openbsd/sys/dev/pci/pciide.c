/*	$OpenBSD: pciide.c,v 1.163.2.1 2004/05/14 21:32:19 brad Exp $	*/
/*	$NetBSD: pciide.c,v 1.127 2001/08/03 01:31:08 tsutsui Exp $	*/

/*
 * Copyright (c) 1999, 2000, 2001 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * Copyright (c) 1996, 1998 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * PCI IDE controller driver.
 *
 * Author: Christopher G. Demetriou, March 2, 1998 (derived from NetBSD
 * sys/dev/pci/ppb.c, revision 1.16).
 *
 * See "PCI IDE Controller Specification, Revision 1.0 3/4/94" and
 * "Programming Interface for Bus Master IDE Controller, Revision 1.0
 * 5/16/94" from the PCI SIG.
 *
 */

#define DEBUG_DMA   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10

#ifdef WDCDEBUG
#ifndef WDCDEBUG_PCIIDE_MASK
#define WDCDEBUG_PCIIDE_MASK 0x00
#endif
int wdcdebug_pciide_mask = WDCDEBUG_PCIIDE_MASK;
#define WDCDEBUG_PRINT(args, level) do {		\
	if ((wdcdebug_pciide_mask & (level)) != 0)	\
		printf args;				\
} while (0)
#else
#define WDCDEBUG_PRINT(args, level)
#endif
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/endian.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_piix_reg.h>
#include <dev/pci/pciide_amd_reg.h>
#include <dev/pci/pciide_apollo_reg.h>
#include <dev/pci/pciide_cmd_reg.h>
#include <dev/pci/pciide_sii3112_reg.h>
#include <dev/pci/pciide_cy693_reg.h>
#include <dev/pci/pciide_sis_reg.h>
#include <dev/pci/pciide_acer_reg.h>
#include <dev/pci/pciide_pdc202xx_reg.h>
#include <dev/pci/pciide_opti_reg.h>
#include <dev/pci/pciide_hpt_reg.h>
#include <dev/pci/pciide_acard_reg.h>
#include <dev/pci/pciide_natsemi_reg.h>
#include <dev/pci/pciide_nforce_reg.h>
#include <dev/pci/pciide_i31244_reg.h>
#include <dev/pci/pciide_ite_reg.h>
#include <dev/pci/cy82c693var.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

/* inlines for reading/writing 8-bit PCI registers */
static __inline u_int8_t pciide_pci_read(pci_chipset_tag_t, pcitag_t,
					      int);
static __inline void pciide_pci_write(pci_chipset_tag_t, pcitag_t,
					   int, u_int8_t);

static __inline u_int8_t
pciide_pci_read(pc, pa, reg)
	pci_chipset_tag_t pc;
	pcitag_t pa;
	int reg;
{

	return (pci_conf_read(pc, pa, (reg & ~0x03)) >>
	    ((reg & 0x03) * 8) & 0xff);
}

static __inline void
pciide_pci_write(pc, pa, reg, val)
	pci_chipset_tag_t pc;
	pcitag_t pa;
	int reg;
	u_int8_t val;
{
	pcireg_t pcival;

	pcival = pci_conf_read(pc, pa, (reg & ~0x03));
	pcival &= ~(0xff << ((reg & 0x03) * 8));
	pcival |= (val << ((reg & 0x03) * 8));
	pci_conf_write(pc, pa, (reg & ~0x03), pcival);
}

struct pciide_softc {
	struct wdc_softc	sc_wdcdev;	/* common wdc definitions */
	pci_chipset_tag_t	sc_pc;		/* PCI registers info */
	pcitag_t		sc_tag;
	void			*sc_pci_ih;	/* PCI interrupt handle */
	int			sc_dma_ok;	/* bus-master DMA info */
	bus_space_tag_t		sc_dma_iot;
	bus_space_handle_t	sc_dma_ioh;
	bus_dma_tag_t		sc_dmat;

	/*
	 * Some controllers might have DMA restrictions other than
	 * the norm.
	 */
	bus_size_t		sc_dma_maxsegsz;
	bus_size_t		sc_dma_boundary;

	/* For Cypress */
	const struct cy82c693_handle *sc_cy_handle;
	int sc_cy_compatchan;

	/* For SiS */
	u_int8_t sis_type;

	/* Chip description */
	const struct pciide_product_desc *sc_pp;
	/* Chip revision */
	int sc_rev;
	/* common definitions */
	struct channel_softc *wdc_chanarray[PCIIDE_NUM_CHANNELS];
	/* internal bookkeeping */
	struct pciide_channel {			/* per-channel data */
		struct channel_softc wdc_channel; /* generic part */
		char		*name;
		int		hw_ok;		/* hardware mapped & OK? */
		int		compat;		/* is it compat? */
		int             dma_in_progress;
		void		*ih;		/* compat or pci handle */
		bus_space_handle_t ctl_baseioh;	/* ctrl regs blk, native mode */
		/* DMA tables and DMA map for xfer, for each drive */
		struct pciide_dma_maps {
			bus_dmamap_t    dmamap_table;
			struct idedma_table *dma_table;
			bus_dmamap_t    dmamap_xfer;
			int dma_flags;
		} dma_maps[2];
	} pciide_channels[PCIIDE_NUM_CHANNELS];
};

void default_chip_map(struct pciide_softc *, struct pci_attach_args *);

void sata_setup_channel(struct channel_softc *);

void piix_chip_map(struct pciide_softc *, struct pci_attach_args *);
void piix_setup_channel(struct channel_softc *);
void piix3_4_setup_channel(struct channel_softc *);

static u_int32_t piix_setup_idetim_timings(u_int8_t, u_int8_t, u_int8_t);
static u_int32_t piix_setup_idetim_drvs(struct ata_drive_datas *);
static u_int32_t piix_setup_sidetim_timings(u_int8_t, u_int8_t, u_int8_t);

void amd756_chip_map(struct pciide_softc *, struct pci_attach_args *);
void amd756_setup_channel(struct channel_softc *);

void apollo_chip_map(struct pciide_softc *, struct pci_attach_args *);
void apollo_sata_chip_map(struct pciide_softc *, struct pci_attach_args *);
void apollo_setup_channel(struct channel_softc *);

void cmd_chip_map(struct pciide_softc *, struct pci_attach_args *);
void cmd0643_9_chip_map(struct pciide_softc *, struct pci_attach_args *);
void cmd0643_9_setup_channel(struct channel_softc *);
void cmd680_chip_map(struct pciide_softc *, struct pci_attach_args *);
void cmd680_setup_channel(struct channel_softc *);
void cmd680_channel_map(struct pci_attach_args *, struct pciide_softc *, int);
void cmd_channel_map(struct pci_attach_args *,
			struct pciide_softc *, int);
int  cmd_pci_intr(void *);
void cmd646_9_irqack(struct channel_softc *);

void sii3112_chip_map(struct pciide_softc *, struct pci_attach_args *);
void sii3112_setup_channel(struct channel_softc *);

void cy693_chip_map(struct pciide_softc *, struct pci_attach_args *);
void cy693_setup_channel(struct channel_softc *);

void sis_chip_map(struct pciide_softc *, struct pci_attach_args *);
void sis_setup_channel(struct channel_softc *);
void sis96x_setup_channel(struct channel_softc *);
int  sis_hostbr_match(struct pci_attach_args *);
int  sis_south_match(struct pci_attach_args *);

void natsemi_chip_map(struct pciide_softc *, struct pci_attach_args *);
void natsemi_setup_channel(struct channel_softc *);
int  natsemi_pci_intr(void *);
void natsemi_irqack(struct channel_softc *);
void ns_scx200_chip_map(struct pciide_softc *, struct pci_attach_args *);
void ns_scx200_setup_channel(struct channel_softc *);

void acer_chip_map(struct pciide_softc *, struct pci_attach_args *);
void acer_setup_channel(struct channel_softc *);
int  acer_pci_intr(void *);

void pdc202xx_chip_map(struct pciide_softc *, struct pci_attach_args *);
void pdc202xx_setup_channel(struct channel_softc *);
void pdc20268_setup_channel(struct channel_softc *);
int  pdc202xx_pci_intr(void *);
int  pdc20265_pci_intr(void *);
void pdc20262_dma_start(void *, int, int);
int  pdc20262_dma_finish(void *, int, int, int);

void opti_chip_map(struct pciide_softc *, struct pci_attach_args *);
void opti_setup_channel(struct channel_softc *);

void hpt_chip_map(struct pciide_softc *, struct pci_attach_args *);
void hpt_setup_channel(struct channel_softc *);
int  hpt_pci_intr(void *);

void acard_chip_map(struct pciide_softc *, struct pci_attach_args *);
void acard_setup_channel(struct channel_softc *);
int  acard_pci_intr(void *);

void serverworks_chip_map(struct pciide_softc *, struct pci_attach_args *);
void serverworks_setup_channel(struct channel_softc *);
int  serverworks_pci_intr(void *);

void nforce_chip_map(struct pciide_softc *, struct pci_attach_args *);
void nforce_setup_channel(struct channel_softc *);
int  nforce_pci_intr(void *);

void artisea_chip_map(struct pciide_softc *, struct pci_attach_args *);

void ite_chip_map(struct pciide_softc *, struct pci_attach_args *);
void ite_setup_channel(struct channel_softc *);

void pciide_channel_dma_setup(struct pciide_channel *);
int  pciide_dma_table_setup(struct pciide_softc *, int, int);
int  pciide_dma_init(void *, int, int, void *, size_t, int);
void pciide_dma_start(void *, int, int);
int  pciide_dma_finish(void *, int, int, int);
void pciide_irqack(struct channel_softc *);
void pciide_print_modes(struct pciide_channel *);
void pciide_print_channels(int, pcireg_t);

struct pciide_product_desc {
	u_int32_t ide_product;
	u_short ide_flags;
	/* map and setup chip, probe drives */
	void (*chip_map)(struct pciide_softc *, struct pci_attach_args *);
};

/* Flags for ide_flags */
#define IDE_PCI_CLASS_OVERRIDE	0x0001	/* accept even if class != pciide */
#define IDE_16BIT_IOSPACE	0x0002	/* I/O space BARS ignore upper word */

/* Default product description for devices not known from this controller */
const struct pciide_product_desc default_product_desc = {
	0,				/* Generic PCI IDE controller */
	0,
	default_chip_map
};

const struct pciide_product_desc pciide_intel_products[] =  {
	{ PCI_PRODUCT_INTEL_82092AA,	/* Intel 82092AA IDE */
	  0,
	  default_chip_map
	},
	{ PCI_PRODUCT_INTEL_82371FB_IDE, /* Intel 82371FB IDE (PIIX) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82371SB_IDE, /* Intel 82371SB IDE (PIIX3) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82371AB_IDE, /* Intel 82371AB IDE (PIIX4) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82440MX_IDE, /* Intel 82440MX IDE */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801AA_IDE, /* Intel 82801AA IDE (ICH) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801AB_IDE, /* Intel 82801AB IDE (ICH0) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801BAM_IDE, /* Intel 82801BAM IDE (ICH2) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801BA_IDE, /* Intel 82801BA IDE (ICH2) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801CAM_IDE, /* Intel 82801CAM IDE (ICH3) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801CA_IDE, /* Intel 82801CA IDE (ICH3) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801DB_IDE, /* Intel 82801DB IDE (ICH4) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801DBM_IDE, /* Intel 82801DBM IDE (ICH4-M) */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801EB_IDE, /* Intel 82801EB/ER (ICH5/5R) IDE */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801EB_SATA, /* Intel 82801EB (ICH5) SATA */
	  0,
	  piix_chip_map
	},
	{ PCI_PRODUCT_INTEL_82801ER_SATA, /* Intel 82801ER (ICH5R) SATA */
	  IDE_PCI_CLASS_OVERRIDE,
	  piix_chip_map
	},
#ifdef notyet
	{ PCI_PRODUCT_INTEL_31244,	 /* Intel 31244 SATA */
	  0,
	  artisea_chip_map
	}
#endif
};

const struct pciide_product_desc pciide_amd_products[] =  {
	{ PCI_PRODUCT_AMD_PBC756_IDE,	/* AMD 756 */
	  0,
	  amd756_chip_map
	},
	{ PCI_PRODUCT_AMD_766_IDE, /* AMD 766 */
	  0,
	  amd756_chip_map
	},
	{ PCI_PRODUCT_AMD_PBC768_IDE,
	  0,
	  amd756_chip_map
	},
	{ PCI_PRODUCT_AMD_8111_IDE,
	  0,
	  amd756_chip_map
	}
};

#ifdef notyet
const struct pciide_product_desc pciide_opti_products[] = {

	{ PCI_PRODUCT_OPTI_82C621,
	  0,
	  opti_chip_map
	},
	{ PCI_PRODUCT_OPTI_82C568,
	  0,
	  opti_chip_map
	},
	{ PCI_PRODUCT_OPTI_82D568,
	  0,
	  opti_chip_map
	},
};
#endif

const struct pciide_product_desc pciide_cmd_products[] =  {
	{ PCI_PRODUCT_CMDTECH_640,	/* CMD Technology PCI0640 */
	  0,
	  cmd_chip_map
	},
	{ PCI_PRODUCT_CMDTECH_643,	/* CMD Technology PCI0643 */
	  0,
	  cmd0643_9_chip_map
	},
	{ PCI_PRODUCT_CMDTECH_646,	/* CMD Technology PCI0646 */
	  0,
	  cmd0643_9_chip_map
	},
	{ PCI_PRODUCT_CMDTECH_648,	/* CMD Technology PCI0648 */
	  IDE_PCI_CLASS_OVERRIDE,
	  cmd0643_9_chip_map
	},
	{ PCI_PRODUCT_CMDTECH_649,	/* CMD Technology PCI0649 */
	  IDE_PCI_CLASS_OVERRIDE,
	  cmd0643_9_chip_map
	},
	{ PCI_PRODUCT_CMDTECH_680,	/* CMD Technology PCI0680 */
	  IDE_PCI_CLASS_OVERRIDE,
	  cmd680_chip_map
	},
	{ PCI_PRODUCT_CMDTECH_3112,	/* SiI 3112 SATA */
	  IDE_PCI_CLASS_OVERRIDE,	/* XXX: subclass RAID */
	  sii3112_chip_map
	}
};

const struct pciide_product_desc pciide_via_products[] =  {
	{ PCI_PRODUCT_VIATECH_VT82C416, /* VIA VT82C416 IDE */
	  0,
	  apollo_chip_map
	},
	{ PCI_PRODUCT_VIATECH_VT82C571, /* VIA VT82C571 IDE */
	  0,
	  apollo_chip_map
	},
	{ PCI_PRODUCT_VIATECH_VT8237_SATA, /* VIA VT8237 SATA */
	  IDE_PCI_CLASS_OVERRIDE,
	  apollo_sata_chip_map
	}
};

const struct pciide_product_desc pciide_cypress_products[] =  {
	{ PCI_PRODUCT_CONTAQ_82C693,	/* Contaq CY82C693 IDE */
	  IDE_16BIT_IOSPACE,
	  cy693_chip_map
	}
};

const struct pciide_product_desc pciide_sis_products[] =  {
	{ PCI_PRODUCT_SIS_5513,		/* SIS 5513 EIDE */
	  0,
	  sis_chip_map
	}
};

const struct pciide_product_desc pciide_natsemi_products[] =  {
	{ PCI_PRODUCT_NS_PC87415,	/* National Semi PC87415 IDE */
	  0,
	  natsemi_chip_map
	},
	{ PCI_PRODUCT_NS_SCx200_IDE,	/* National Semi SCx200 IDE */
	  0,
	  ns_scx200_chip_map
	}
};

const struct pciide_product_desc pciide_acer_products[] =  {
	{ PCI_PRODUCT_ALI_M5229,	/* Acer Labs M5229 UDMA IDE */
	  0,
	  acer_chip_map
	}
};

const struct pciide_product_desc pciide_triones_products[] =  {
	{ PCI_PRODUCT_TRIONES_HPT366,	/* Highpoint HPT36x/37x IDE */
	  IDE_PCI_CLASS_OVERRIDE,
	  hpt_chip_map,
	},
	{ PCI_PRODUCT_TRIONES_HPT372A,	/* Highpoint HPT372A IDE */
	  IDE_PCI_CLASS_OVERRIDE,
	  hpt_chip_map
	},
	{ PCI_PRODUCT_TRIONES_HPT302,	/* Highpoint HPT302 IDE */
	  IDE_PCI_CLASS_OVERRIDE,
	  hpt_chip_map
	},
	{ PCI_PRODUCT_TRIONES_HPT371,	/* Highpoint HPT371 IDE */
	  IDE_PCI_CLASS_OVERRIDE,
	  hpt_chip_map
	},
	{ PCI_PRODUCT_TRIONES_HPT374,	/* Highpoint HPT374 IDE */
	  IDE_PCI_CLASS_OVERRIDE,
	  hpt_chip_map
	}
};

const struct pciide_product_desc pciide_promise_products[] =  {
	{ PCI_PRODUCT_PROMISE_PDC20246,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20262,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20265,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20267,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20268,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20268R,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20269,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20271,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20275,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20276,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20277,
	  IDE_PCI_CLASS_OVERRIDE,
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20376,	/* PDC20376 SATA */
	  IDE_PCI_CLASS_OVERRIDE,	/* XXX: subclass RAID */
	  pdc202xx_chip_map,
	}
};

const struct pciide_product_desc pciide_acard_products[] =  {
	{ PCI_PRODUCT_ACARD_ATP850U,	/* Acard ATP850U Ultra33 Controller */
	  IDE_PCI_CLASS_OVERRIDE,
	  acard_chip_map,
	},
	{ PCI_PRODUCT_ACARD_ATP860,	/* Acard ATP860 Ultra66 Controller */
	  IDE_PCI_CLASS_OVERRIDE,
	  acard_chip_map,
	},
	{ PCI_PRODUCT_ACARD_ATP860A,	/* Acard ATP860-A Ultra66 Controller */
	  IDE_PCI_CLASS_OVERRIDE,
	  acard_chip_map,
	},
	{ PCI_PRODUCT_ACARD_ATP865A,	/* Acard ATP865-A Ultra133 Controller */
	  IDE_PCI_CLASS_OVERRIDE,
	  acard_chip_map,
	},
	{ PCI_PRODUCT_ACARD_ATP865R,	/* Acard ATP865-R Ultra133 Controller */
	  IDE_PCI_CLASS_OVERRIDE,
	  acard_chip_map,
	}
};

const struct pciide_product_desc pciide_serverworks_products[] =  {
	{ PCI_PRODUCT_RCC_OSB4_IDE,
	  0,
	  serverworks_chip_map,
	},
	{ PCI_PRODUCT_RCC_CSB5_IDE,
	  0,
	  serverworks_chip_map,
	},
	{ PCI_PRODUCT_RCC_CSB6_IDE,
	  0,
	  serverworks_chip_map,
	},
	{ PCI_PRODUCT_RCC_CSB6_IDE2,
	  0,
	  serverworks_chip_map,
	}
};

const struct pciide_product_desc pciide_nvidia_products[] = {
	{ PCI_PRODUCT_NVIDIA_NFORCE_IDE,
	  0,
	  nforce_chip_map
	},
	{ PCI_PRODUCT_NVIDIA_NFORCE2_IDE,
	  0,
	  nforce_chip_map
	}
};

const struct pciide_product_desc pciide_ite_products[] = {
	{ PCI_PRODUCT_ITEXPRESS_IT8212F,
	  IDE_PCI_CLASS_OVERRIDE,
	  ite_chip_map
	}
};


struct pciide_vendor_desc {
	u_int32_t ide_vendor;
	const struct pciide_product_desc *ide_products;
	int ide_nproducts;
};

const struct pciide_vendor_desc pciide_vendors[] = {
	{ PCI_VENDOR_INTEL, pciide_intel_products,
	  sizeof(pciide_intel_products)/sizeof(pciide_intel_products[0]) },
	{ PCI_VENDOR_AMD, pciide_amd_products,
	  sizeof(pciide_amd_products)/sizeof(pciide_amd_products[0]) },
#ifdef notyet
	{ PCI_VENDOR_OPTI, pciide_opti_products,
	  sizeof(pciide_opti_products)/sizeof(pciide_opti_products[0]) },
#endif
	{ PCI_VENDOR_CMDTECH, pciide_cmd_products,
	  sizeof(pciide_cmd_products)/sizeof(pciide_cmd_products[0]) },
	{ PCI_VENDOR_VIATECH, pciide_via_products,
	  sizeof(pciide_via_products)/sizeof(pciide_via_products[0]) },
	{ PCI_VENDOR_CONTAQ, pciide_cypress_products,
	  sizeof(pciide_cypress_products)/sizeof(pciide_cypress_products[0]) },
	{ PCI_VENDOR_SIS, pciide_sis_products,
	  sizeof(pciide_sis_products)/sizeof(pciide_sis_products[0]) },
	{ PCI_VENDOR_NS, pciide_natsemi_products,
	  sizeof(pciide_natsemi_products)/sizeof(pciide_natsemi_products[0]) },
	{ PCI_VENDOR_ALI, pciide_acer_products,
	  sizeof(pciide_acer_products)/sizeof(pciide_acer_products[0]) },
	{ PCI_VENDOR_TRIONES, pciide_triones_products,
	  sizeof(pciide_triones_products)/sizeof(pciide_triones_products[0]) },
	{ PCI_VENDOR_ACARD, pciide_acard_products,
	  sizeof(pciide_acard_products)/sizeof(pciide_acard_products[0]) },
	{ PCI_VENDOR_RCC, pciide_serverworks_products,
	  sizeof(pciide_serverworks_products)/sizeof(pciide_serverworks_products[0]) },
	{ PCI_VENDOR_PROMISE, pciide_promise_products,
	  sizeof(pciide_promise_products)/sizeof(pciide_promise_products[0]) },
	{ PCI_VENDOR_NVIDIA, pciide_nvidia_products,
	  sizeof(pciide_nvidia_products)/sizeof(pciide_nvidia_products[0]) },
	{ PCI_VENDOR_ITEXPRESS, pciide_ite_products,
	  sizeof(pciide_ite_products)/sizeof(pciide_ite_products[0]) }
};

/* options passed via the 'flags' config keyword */
#define PCIIDE_OPTIONS_DMA	0x01

#ifndef __OpenBSD__
int	pciide_match(struct device *, struct cfdata *, void *);
#else
int	pciide_match(struct device *, void *, void *);
#endif
void	pciide_attach(struct device *, struct device *, void *);

struct cfattach pciide_ca = {
	sizeof(struct pciide_softc), pciide_match, pciide_attach
};

#ifdef __OpenBSD__
struct        cfdriver pciide_cd = {
      NULL, "pciide", DV_DULL
};
#endif
int	pciide_chipen(struct pciide_softc *, struct pci_attach_args *);
int	pciide_mapregs_compat( struct pci_attach_args *,
	    struct pciide_channel *, int, bus_size_t *, bus_size_t *);
int	pciide_mapregs_native(struct pci_attach_args *,
	    struct pciide_channel *, bus_size_t *, bus_size_t *,
	    int (*pci_intr)(void *));
void	pciide_mapreg_dma(struct pciide_softc *,
	    struct pci_attach_args *);
int	pciide_chansetup(struct pciide_softc *, int, pcireg_t);
void	pciide_mapchan(struct pci_attach_args *,
	    struct pciide_channel *, pcireg_t, bus_size_t *, bus_size_t *,
	    int (*pci_intr)(void *));
int	pciide_chan_candisable(struct pciide_channel *);
void	pciide_map_compat_intr( struct pci_attach_args *,
	    struct pciide_channel *, int, int);
void	pciide_unmap_compat_intr( struct pci_attach_args *,
	    struct pciide_channel *, int, int);
int	pciide_compat_intr(void *);
int	pciide_pci_intr(void *);
int     pciide_intr_flag(struct pciide_channel *);

const struct pciide_product_desc *pciide_lookup_product(u_int32_t);

const struct pciide_product_desc *
pciide_lookup_product(id)
	u_int32_t id;
{
	const struct pciide_product_desc *pp;
	const struct pciide_vendor_desc *vp;
	int i;

	for (i = 0, vp = pciide_vendors;
	    i < sizeof(pciide_vendors)/sizeof(pciide_vendors[0]);
	    vp++, i++)
		if (PCI_VENDOR(id) == vp->ide_vendor)
			break;

	if (i == sizeof(pciide_vendors)/sizeof(pciide_vendors[0]))
		return NULL;

	for (pp = vp->ide_products, i = 0; i < vp->ide_nproducts; pp++, i++)
		if (PCI_PRODUCT(id) == pp->ide_product)
			break;

	if (i == vp->ide_nproducts)
		return NULL;
	return pp;
}

int
pciide_match(parent, match, aux)
	struct device *parent;
#ifdef __OpenBSD__
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = aux;
	const struct pciide_product_desc *pp;

	/*
 	 * Some IDE controllers have severe bugs when used in PCI mode.
	 * We punt and attach them to the ISA bus instead.
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_PCTECH &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_PCTECH_RZ1000)
		return (0);

	/*
	 * Check the ID register to see that it's a PCI IDE controller.
	 * If it is, we assume that we can deal with it; it _should_
	 * work in a standardized way...
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		return (1);
	}

	/*
 	 * Some controllers (e.g. promise Ultra-33) don't claim to be PCI IDE
	 * controllers. Let see if we can deal with it anyway.
	 */
	pp = pciide_lookup_product(pa->pa_id);
	if (pp  && (pp->ide_flags & IDE_PCI_CLASS_OVERRIDE)) {
		return (1);
	}

	return (0);
}

void
pciide_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	struct pciide_softc *sc = (struct pciide_softc *)self;
	pcireg_t csr;
	char devinfo[256];

	sc->sc_pp = pciide_lookup_product(pa->pa_id);
	if (sc->sc_pp == NULL) {
		sc->sc_pp = &default_product_desc;
		pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo,
		    sizeof devinfo);
	}
	sc->sc_rev = PCI_REVISION(pa->pa_class);

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;

	/* Set up DMA defaults; these might be adjusted by chip_map. */
	sc->sc_dma_maxsegsz = IDEDMA_BYTE_COUNT_MAX;
	sc->sc_dma_boundary = IDEDMA_BYTE_COUNT_ALIGN;

	WDCDEBUG_PRINT((" sc_pc=%p, sc_tag=%p, pa_class=0x%x\n", sc->sc_pc,
	    sc->sc_tag, pa->pa_class), DEBUG_PROBE);

	sc->sc_pp->chip_map(sc, pa);

	if (sc->sc_dma_ok) {
		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		csr |= PCI_COMMAND_MASTER_ENABLE;
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);
	}

	WDCDEBUG_PRINT(("pciide: command/status register=0x%x\n",
	    pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG)), DEBUG_PROBE);
}

/* tell whether the chip is enabled or not */
int
pciide_chipen(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	pcireg_t csr;

	csr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);
	if ((csr & PCI_COMMAND_IO_ENABLE) == 0 ) {
		printf("\n%s: device disabled\n",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		return 0;
	}

	return 1;
}

int
pciide_mapregs_compat(pa, cp, compatchan, cmdsizep, ctlsizep)
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	int compatchan;
	bus_size_t *cmdsizep, *ctlsizep;
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	cp->compat = 1;
	*cmdsizep = PCIIDE_COMPAT_CMD_SIZE;
	*ctlsizep = PCIIDE_COMPAT_CTL_SIZE;

	wdc_cp->cmd_iot = pa->pa_iot;

	if (bus_space_map(wdc_cp->cmd_iot, PCIIDE_COMPAT_CMD_BASE(compatchan),
	    PCIIDE_COMPAT_CMD_SIZE, 0, &wdc_cp->cmd_ioh) != 0) {
		printf("%s: couldn't map %s cmd regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return (0);
	}

	wdc_cp->ctl_iot = pa->pa_iot;

	if (bus_space_map(wdc_cp->ctl_iot, PCIIDE_COMPAT_CTL_BASE(compatchan),
	    PCIIDE_COMPAT_CTL_SIZE, 0, &wdc_cp->ctl_ioh) != 0) {
		printf("%s: couldn't map %s ctl regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh,
		    PCIIDE_COMPAT_CMD_SIZE);
		return (0);
	}

	return (1);
}

int
pciide_mapregs_native(pa, cp, cmdsizep, ctlsizep, pci_intr)
	struct pci_attach_args * pa;
	struct pciide_channel *cp;
	bus_size_t *cmdsizep, *ctlsizep;
	int (*pci_intr)(void *);
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	const char *intrstr;
	pci_intr_handle_t intrhandle;
	pcireg_t maptype;

	cp->compat = 0;

	if (sc->sc_pci_ih == NULL) {
		if (pci_intr_map(pa, &intrhandle) != 0) {
			printf("%s: couldn't map native-PCI interrupt\n",
			    sc->sc_wdcdev.sc_dev.dv_xname);
			return 0;
		}
		intrstr = pci_intr_string(pa->pa_pc, intrhandle);
#ifdef __OpenBSD__
		sc->sc_pci_ih = pci_intr_establish(pa->pa_pc,
		    intrhandle, IPL_BIO, pci_intr, sc,
		    sc->sc_wdcdev.sc_dev.dv_xname);
#ifdef __pegasos__
		/* stupid broken board */
		if (intrhandle == 0xe)
			pci_intr_establish(pa->pa_pc,
			    0xf, IPL_BIO, pci_intr, sc,
			    sc->sc_wdcdev.sc_dev.dv_xname);
#endif
#else
		sc->sc_pci_ih = pci_intr_establish(pa->pa_pc,
		    intrhandle, IPL_BIO, pci_intr, sc);
#endif
		if (sc->sc_pci_ih != NULL) {
			printf("%s: using %s for native-PCI interrupt\n",
			    sc->sc_wdcdev.sc_dev.dv_xname,
			    intrstr ? intrstr : "unknown interrupt");
		} else {
			printf("%s: couldn't establish native-PCI interrupt",
			    sc->sc_wdcdev.sc_dev.dv_xname);
			if (intrstr != NULL)
				printf(" at %s", intrstr);
			printf("\n");
			return 0;
		}
	}
	cp->ih = sc->sc_pci_ih;

	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag,
	    PCIIDE_REG_CMD_BASE(wdc_cp->channel));
	WDCDEBUG_PRINT(("%s: %s cmd regs mapping: %s\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
	    (maptype == PCI_MAPREG_TYPE_IO ? "I/O" : "memory")), DEBUG_PROBE);
	if (pci_mapreg_map(pa, PCIIDE_REG_CMD_BASE(wdc_cp->channel),
	    maptype, 0,
	    &wdc_cp->cmd_iot, &wdc_cp->cmd_ioh, NULL, cmdsizep, 0) != 0) {
		printf("%s: couldn't map %s cmd regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return 0;
	}

	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag,
	    PCIIDE_REG_CTL_BASE(wdc_cp->channel));
	WDCDEBUG_PRINT(("%s: %s ctl regs mapping: %s\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
	    (maptype == PCI_MAPREG_TYPE_IO ? "I/O": "memory")), DEBUG_PROBE);
	if (pci_mapreg_map(pa, PCIIDE_REG_CTL_BASE(wdc_cp->channel),
	    maptype, 0,
	    &wdc_cp->ctl_iot, &cp->ctl_baseioh, NULL, ctlsizep, 0) != 0) {
		printf("%s: couldn't map %s ctl regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh, *cmdsizep);
		return 0;
	}
	/*
	 * In native mode, 4 bytes of I/O space are mapped for the control
	 * register, the control register is at offset 2. Pass the generic
	 * code a handle for only one byte at the right offset.
	 */
	if (bus_space_subregion(wdc_cp->ctl_iot, cp->ctl_baseioh, 2, 1,
	    &wdc_cp->ctl_ioh) != 0) {
		printf("%s: unable to subregion %s ctl regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh, *cmdsizep);
		bus_space_unmap(wdc_cp->cmd_iot, cp->ctl_baseioh, *ctlsizep);
		return 0;
	}
	return (1);
}

void
pciide_mapreg_dma(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	pcireg_t maptype;
	bus_addr_t addr;

	/*
	 * Map DMA registers
	 *
	 * Note that sc_dma_ok is the right variable to test to see if
	 * DMA can be done.  If the interface doesn't support DMA,
	 * sc_dma_ok will never be non-zero.  If the DMA regs couldn't
	 * be mapped, it'll be zero.  I.e., sc_dma_ok will only be
	 * non-zero if the interface supports DMA and the registers
	 * could be mapped.
	 *
	 * XXX Note that despite the fact that the Bus Master IDE specs
	 * XXX say that "The bus master IDE function uses 16 bytes of IO
	 * XXX space," some controllers (at least the United
	 * XXX Microelectronics UM8886BF) place it in memory space.
	 */

	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag,
	    PCIIDE_REG_BUS_MASTER_DMA);

	switch (maptype) {
	case PCI_MAPREG_TYPE_IO:
		sc->sc_dma_ok = (pci_mapreg_info(pa->pa_pc, pa->pa_tag,
		    PCIIDE_REG_BUS_MASTER_DMA, PCI_MAPREG_TYPE_IO,
		    &addr, NULL, NULL) == 0);
		if (sc->sc_dma_ok == 0) {
			printf(", unused (couldn't query registers)");
			break;
		}
		if ((sc->sc_pp->ide_flags & IDE_16BIT_IOSPACE)
		    && addr >= 0x10000) {
			sc->sc_dma_ok = 0;
			printf(", unused (registers at unsafe address %#lx)", addr);
			break;
		}
		/* FALLTHROUGH */

	case PCI_MAPREG_MEM_TYPE_32BIT:
		sc->sc_dma_ok = (pci_mapreg_map(pa,
		    PCIIDE_REG_BUS_MASTER_DMA, maptype, 0,
		    &sc->sc_dma_iot, &sc->sc_dma_ioh, NULL, NULL, 0) == 0);
		sc->sc_dmat = pa->pa_dmat;
		if (sc->sc_dma_ok == 0) {
			printf(", unused (couldn't map registers)");
		} else {
			sc->sc_wdcdev.dma_arg = sc;
			sc->sc_wdcdev.dma_init = pciide_dma_init;
			sc->sc_wdcdev.dma_start = pciide_dma_start;
			sc->sc_wdcdev.dma_finish = pciide_dma_finish;
		}
		break;

	default:
		sc->sc_dma_ok = 0;
		printf(", (unsupported maptype 0x%x)", maptype);
		break;
	}
}

int
pciide_intr_flag(struct pciide_channel *cp)
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	if (cp->dma_in_progress) {
		int retry = 10;
		int status;

		/* Check the status register */
		for (retry = 10; retry > 0; retry--) {
			status = bus_space_read_1(sc->sc_dma_iot,
			    sc->sc_dma_ioh,
			    IDEDMA_CTL(cp->wdc_channel.channel));
			if (status & IDEDMA_CTL_INTR) {
				break;
			}
			DELAY(5);
		}

		/* Not for us.  */
		if (retry == 0)
			return (0);

		return (1);
	}

	return (-1);
}

int
pciide_compat_intr(arg)
	void *arg;
{
	struct pciide_channel *cp = arg;

	if (pciide_intr_flag(cp) == 0)
		return 0;

#ifdef DIAGNOSTIC
	/* should only be called for a compat channel */
	if (cp->compat == 0)
		panic("pciide compat intr called for non-compat chan %p", cp);
#endif
	return (wdcintr(&cp->wdc_channel));
}

int
pciide_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;

	rv = 0;
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;

		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		/* if this channel not waiting for intr, skip */
		if ((wdc_cp->ch_flags & WDCF_IRQ_WAIT) == 0)
			continue;

		if (pciide_intr_flag(cp) == 0)
			continue;

		crv = wdcintr(wdc_cp);
		if (crv == 0)
			;		/* leave rv alone */
		else if (crv == 1)
			rv = 1;		/* claim the intr */
		else if (rv == 0)	/* crv should be -1 in this case */
			rv = crv;	/* if we've done no better, take it */
	}
	return (rv);
}

void
pciide_channel_dma_setup(cp)
	struct pciide_channel *cp;
{
	int drive;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct ata_drive_datas *drvp;

	for (drive = 0; drive < 2; drive++) {
		drvp = &cp->wdc_channel.ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* setup DMA if needed */
		if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0) ||
		    sc->sc_dma_ok == 0) {
			drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
			continue;
		}
		if (pciide_dma_table_setup(sc, cp->wdc_channel.channel, drive)
		    != 0) {
			/* Abort DMA setup */
			drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
			continue;
		}
	}
}

int
pciide_dma_table_setup(sc, channel, drive)
	struct pciide_softc *sc;
	int channel, drive;
{
	bus_dma_segment_t seg;
	int error, rseg;
	const bus_size_t dma_table_size =
	    sizeof(struct idedma_table) * NIDEDMA_TABLES;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	/* If table was already allocated, just return */
	if (dma_maps->dma_table)
		return 0;

	/* Allocate memory for the DMA tables and map it */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, dma_table_size,
	    IDEDMA_TBL_ALIGN, IDEDMA_TBL_ALIGN, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s:%d: unable to allocate table DMA for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    dma_table_size,
	    (caddr_t *)&dma_maps->dma_table,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s:%d: unable to map table DMA for"
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}

	WDCDEBUG_PRINT(("pciide_dma_table_setup: table at %p len %ld, "
	    "phy 0x%lx\n", dma_maps->dma_table, dma_table_size,
	    seg.ds_addr), DEBUG_PROBE);

	/* Create and load table DMA map for this disk */
	if ((error = bus_dmamap_create(sc->sc_dmat, dma_table_size,
	    1, dma_table_size, IDEDMA_TBL_ALIGN, BUS_DMA_NOWAIT,
	    &dma_maps->dmamap_table)) != 0) {
		printf("%s:%d: unable to create table DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat,
	    dma_maps->dmamap_table,
	    dma_maps->dma_table,
	    dma_table_size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s:%d: unable to load table DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	WDCDEBUG_PRINT(("pciide_dma_table_setup: phy addr of table 0x%lx\n",
	    dma_maps->dmamap_table->dm_segs[0].ds_addr), DEBUG_PROBE);
	/* Create a xfer DMA map for this drive */
	if ((error = bus_dmamap_create(sc->sc_dmat, IDEDMA_BYTE_COUNT_MAX,
	    NIDEDMA_TABLES, sc->sc_dma_maxsegsz, sc->sc_dma_boundary,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &dma_maps->dmamap_xfer)) != 0) {
		printf("%s:%d: unable to create xfer DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	return 0;
}

int
pciide_dma_init(v, channel, drive, databuf, datalen, flags)
	void *v;
	int channel, drive;
	void *databuf;
	size_t datalen;
	int flags;
{
	struct pciide_softc *sc = v;
	int error, seg;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];
#ifndef BUS_DMA_RAW
#define BUS_DMA_RAW 0
#endif

	error = bus_dmamap_load(sc->sc_dmat,
	    dma_maps->dmamap_xfer,
	    databuf, datalen, NULL, BUS_DMA_NOWAIT|BUS_DMA_RAW);
	if (error) {
		printf("%s:%d: unable to load xfer DMA map for"
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}

	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_xfer, 0,
	    dma_maps->dmamap_xfer->dm_mapsize,
	    (flags & WDC_DMA_READ) ?
	    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	for (seg = 0; seg < dma_maps->dmamap_xfer->dm_nsegs; seg++) {
#ifdef DIAGNOSTIC
		/* A segment must not cross a 64k boundary */
		{
		u_long phys = dma_maps->dmamap_xfer->dm_segs[seg].ds_addr;
		u_long len = dma_maps->dmamap_xfer->dm_segs[seg].ds_len;
		if ((phys & ~IDEDMA_BYTE_COUNT_MASK) !=
		    ((phys + len - 1) & ~IDEDMA_BYTE_COUNT_MASK)) {
			printf("pciide_dma: segment %d physical addr 0x%lx"
			    " len 0x%lx not properly aligned\n",
			    seg, phys, len);
			panic("pciide_dma: buf align");
		}
		}
#endif
		dma_maps->dma_table[seg].base_addr =
		    htole32(dma_maps->dmamap_xfer->dm_segs[seg].ds_addr);
		dma_maps->dma_table[seg].byte_count =
		    htole32(dma_maps->dmamap_xfer->dm_segs[seg].ds_len &
		    IDEDMA_BYTE_COUNT_MASK);
		WDCDEBUG_PRINT(("\t seg %d len %d addr 0x%x\n",
		   seg, letoh32(dma_maps->dma_table[seg].byte_count),
		   letoh32(dma_maps->dma_table[seg].base_addr)), DEBUG_DMA);

	}
	dma_maps->dma_table[dma_maps->dmamap_xfer->dm_nsegs -1].byte_count |=
	    htole32(IDEDMA_BYTE_COUNT_EOT);

	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_table, 0,
	    dma_maps->dmamap_table->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* Maps are ready. Start DMA function */
#ifdef DIAGNOSTIC
	if (dma_maps->dmamap_table->dm_segs[0].ds_addr & ~IDEDMA_TBL_MASK) {
		printf("pciide_dma_init: addr 0x%lx not properly aligned\n",
		    dma_maps->dmamap_table->dm_segs[0].ds_addr);
		panic("pciide_dma_init: table align");
	}
#endif

	/* Clear status bits */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL(channel),
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CTL(channel)));
	/* Write table addr */
	bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_TBL(channel),
	    dma_maps->dmamap_table->dm_segs[0].ds_addr);
	/* set read/write */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD(channel),
	    (flags & WDC_DMA_READ) ? IDEDMA_CMD_WRITE: 0);
	/* remember flags */
	dma_maps->dma_flags = flags;
	return 0;
}

void
pciide_dma_start(v, channel, drive)
	void *v;
	int channel, drive;
{
	struct pciide_softc *sc = v;

	WDCDEBUG_PRINT(("pciide_dma_start\n"),DEBUG_XFERS);
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD(channel),
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CMD(channel)) | IDEDMA_CMD_START);

	sc->pciide_channels[channel].dma_in_progress = 1;
}

int
pciide_dma_finish(v, channel, drive, force)
	void *v;
	int channel, drive;
	int force;
{
	struct pciide_softc *sc = v;
	u_int8_t status;
	int error = 0;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	sc->pciide_channels[channel].dma_in_progress = 0;

	status = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL(channel));
	WDCDEBUG_PRINT(("pciide_dma_finish: status 0x%x\n", status),
	    DEBUG_XFERS);

	if (force == 0 && (status & IDEDMA_CTL_INTR) == 0)
		return WDC_DMAST_NOIRQ;

	/* stop DMA channel */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD(channel),
	    (dma_maps->dma_flags & WDC_DMA_READ) ?
	    0x00 : IDEDMA_CMD_WRITE);

	/* Unload the map of the data buffer */
	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_xfer, 0,
	    dma_maps->dmamap_xfer->dm_mapsize,
	    (dma_maps->dma_flags & WDC_DMA_READ) ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, dma_maps->dmamap_xfer);

	/* Clear status bits */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL(channel),
	    status);

	if ((status & IDEDMA_CTL_ERR) != 0) {
		printf("%s:%d:%d: bus-master DMA error: status=0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, channel, drive, status);
		error |= WDC_DMAST_ERR;
	}

	if ((status & IDEDMA_CTL_INTR) == 0) {
		printf("%s:%d:%d: bus-master DMA error: missing interrupt, "
		    "status=0x%x\n", sc->sc_wdcdev.sc_dev.dv_xname, channel,
		    drive, status);
		error |= WDC_DMAST_NOIRQ;
	}

	if ((status & IDEDMA_CTL_ACT) != 0) {
		/* data underrun, may be a valid condition for ATAPI */
		error |= WDC_DMAST_UNDER;
	}
	return error;
}

void
pciide_irqack(chp)
	struct channel_softc *chp;
{
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	/* clear status bits in IDE DMA registers */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL(chp->channel),
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CTL(chp->channel)));
}

/* some common code used by several chip_map */
int
pciide_chansetup(sc, channel, interface)
	struct pciide_softc *sc;
	int channel;
	pcireg_t interface;
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	sc->wdc_chanarray[channel] = &cp->wdc_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->wdc_channel.channel = channel;
	cp->wdc_channel.wdc = &sc->sc_wdcdev;
	cp->wdc_channel.ch_queue =
	    malloc(sizeof(struct channel_queue), M_DEVBUF, M_NOWAIT);
	if (cp->wdc_channel.ch_queue == NULL) {
		printf("%s: %s "
		    "cannot allocate memory for command queue",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return 0;
	}
	cp->hw_ok = 1;

	return 1;
}

/* some common code used by several chip channel_map */
void
pciide_mapchan(pa, cp, interface, cmdsizep, ctlsizep, pci_intr)
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	pcireg_t interface;
	bus_size_t *cmdsizep, *ctlsizep;
	int (*pci_intr)(void *);
{
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if (interface & PCIIDE_INTERFACE_PCI(wdc_cp->channel))
		cp->hw_ok = pciide_mapregs_native(pa, cp, cmdsizep, ctlsizep,
		    pci_intr);
	else
		cp->hw_ok = pciide_mapregs_compat(pa, cp,
		    wdc_cp->channel, cmdsizep, ctlsizep);
	if (cp->hw_ok == 0)
		return;
	wdc_cp->data32iot = wdc_cp->cmd_iot;
	wdc_cp->data32ioh = wdc_cp->cmd_ioh;
	wdcattach(wdc_cp);
}

/*
 * Generic code to call to know if a channel can be disabled. Return 1
 * if channel can be disabled, 0 if not
 */
int
pciide_chan_candisable(cp)
	struct pciide_channel *cp;
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if ((wdc_cp->ch_drive[0].drive_flags & DRIVE) == 0 &&
	    (wdc_cp->ch_drive[1].drive_flags & DRIVE) == 0) {
		printf("%s: %s disabled (no drives)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		cp->hw_ok = 0;
		return 1;
	}
	return 0;
}

/*
 * generic code to map the compat intr if hw_ok=1 and it is a compat channel.
 * Set hw_ok=0 on failure
 */
void
pciide_map_compat_intr(pa, cp, compatchan, interface)
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	int compatchan, interface;
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if ((interface & PCIIDE_INTERFACE_PCI(wdc_cp->channel)) != 0)
		return;

	cp->compat = 1;
	cp->ih = pciide_machdep_compat_intr_establish(&sc->sc_wdcdev.sc_dev,
	    pa, compatchan, pciide_compat_intr, cp);
	if (cp->ih == NULL) {
		printf("%s: no compatibility interrupt for use by %s\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		cp->hw_ok = 0;
	}
}

/*
 * generic code to unmap the compat intr if hw_ok=1 and it is a compat channel.
 * Set hw_ok=0 on failure
 */
void
pciide_unmap_compat_intr(pa, cp, compatchan, interface)
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	int compatchan, interface;
{
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if ((interface & PCIIDE_INTERFACE_PCI(wdc_cp->channel)) != 0)
		return;

	pciide_machdep_compat_intr_disestablish(pa->pa_pc, cp->ih);
}

void
pciide_print_channels(nchannels, interface)
	int nchannels;
	pcireg_t interface;
{
	int i;

	for (i = 0; i < nchannels; i++) {
		printf(", %s %s to %s", PCIIDE_CHANNEL_NAME(i),
		    (interface & PCIIDE_INTERFACE_SETTABLE(i)) ?
		    "configured" : "wired",
		    (interface & PCIIDE_INTERFACE_PCI(i)) ? "native-PCI" :
		    "compatibility");
	}

	printf("\n");
}

void
pciide_print_modes(cp)
	struct pciide_channel *cp;
{
	wdc_print_current_modes(&cp->wdc_channel);
}

void
default_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	pcireg_t csr;
	int channel, drive;
	struct ata_drive_datas *drvp;
	u_int8_t idedma_ctl;
	bus_size_t cmdsize, ctlsize;
	char *failreason;

	if (pciide_chipen(sc, pa) == 0)
		return;

	if (interface & PCIIDE_INTERFACE_BUS_MASTER_DMA) {
		printf(": DMA");
		if (sc->sc_pp == &default_product_desc &&
		    (sc->sc_wdcdev.sc_dev.dv_cfdata->cf_flags &
		    PCIIDE_OPTIONS_DMA) == 0) {
			printf(" (unsupported)");
			sc->sc_dma_ok = 0;
		} else {
			pciide_mapreg_dma(sc, pa);
			if (sc->sc_dma_ok != 0)
				printf(", (partial support)");
		}
	} else {
		printf(": no DMA");
		sc->sc_dma_ok = 0;
	}
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 0;
	sc->sc_wdcdev.DMA_cap = 0;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16;

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if (interface & PCIIDE_INTERFACE_PCI(channel)) {
			cp->hw_ok = pciide_mapregs_native(pa, cp, &cmdsize,
			    &ctlsize, pciide_pci_intr);
		} else {
			cp->hw_ok = pciide_mapregs_compat(pa, cp,
			    channel, &cmdsize, &ctlsize);
		}
		if (cp->hw_ok == 0)
			continue;
		/*
		 * Check to see if something appears to be there.
		 */
		failreason = NULL;
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		if (!wdcprobe(&cp->wdc_channel)) {
			failreason = "not responding; disabled or no drives?";
			goto next;
		}
		/*
		 * Now, make sure it's actually attributable to this PCI IDE
		 * channel by trying to access the channel again while the
		 * PCI IDE controller's I/O space is disabled.  (If the
		 * channel no longer appears to be there, it belongs to
		 * this controller.)  YUCK!
		 */
		csr = pci_conf_read(sc->sc_pc, sc->sc_tag,
	  	    PCI_COMMAND_STATUS_REG);
		pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG,
		    csr & ~PCI_COMMAND_IO_ENABLE);
		if (wdcprobe(&cp->wdc_channel))
			failreason = "other hardware responding at addresses";
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    PCI_COMMAND_STATUS_REG, csr);
next:
		if (failreason) {
			printf("%s: %s ignored (%s)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
			    failreason);
			cp->hw_ok = 0;
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			bus_space_unmap(cp->wdc_channel.cmd_iot,
			    cp->wdc_channel.cmd_ioh, cmdsize);
			bus_space_unmap(cp->wdc_channel.ctl_iot,
			    cp->wdc_channel.ctl_ioh, ctlsize);
		}
		if (cp->hw_ok) {
			cp->wdc_channel.data32iot = cp->wdc_channel.cmd_iot;
			cp->wdc_channel.data32ioh = cp->wdc_channel.cmd_ioh;
			wdcattach(&cp->wdc_channel);
		}
	}

	if (sc->sc_dma_ok == 0)
		return;

	/* Allocate DMA maps */
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		idedma_ctl = 0;
		cp = &sc->pciide_channels[channel];
		for (drive = 0; drive < 2; drive++) {
			drvp = &cp->wdc_channel.ch_drive[drive];
			/* If no drive, skip */
			if ((drvp->drive_flags & DRIVE) == 0)
				continue;
			if ((drvp->drive_flags & DRIVE_DMA) == 0)
				continue;
			if (pciide_dma_table_setup(sc, channel, drive) != 0) {
				/* Abort DMA setup */
				printf("%s:%d:%d: cannot allocate DMA maps, "
				    "using PIO transfers\n",
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    channel, drive);
				drvp->drive_flags &= ~DRIVE_DMA;
			}
			printf("%s:%d:%d: using DMA data transfers\n",
			    sc->sc_wdcdev.sc_dev.dv_xname,
			    channel, drive);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
		if (idedma_ctl != 0) {
			/* Add software bits in status register */
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL(channel),
			    idedma_ctl);
		}
	}
}

void
sata_setup_channel(struct channel_softc *chp)
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
	}

	/*
	 * Nothing to do to setup modes; it is meaningless in S-ATA
	 * (but many S-ATA drives still want to get the SET_FEATURE
	 * command).
	 */
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel), idedma_ctl);
	}
	pciide_print_modes(cp);
}

void
piix_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	u_int32_t idetim;
	bus_size_t cmdsize, ctlsize;

	pcireg_t interface = PCI_INTERFACE(pa->pa_class);

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		switch (sc->sc_pp->ide_product) {
		case PCI_PRODUCT_INTEL_82371AB_IDE:
		case PCI_PRODUCT_INTEL_82440MX_IDE:
		case PCI_PRODUCT_INTEL_82801AA_IDE:
		case PCI_PRODUCT_INTEL_82801AB_IDE:
		case PCI_PRODUCT_INTEL_82801BAM_IDE:
		case PCI_PRODUCT_INTEL_82801BA_IDE:
		case PCI_PRODUCT_INTEL_82801CAM_IDE:
		case PCI_PRODUCT_INTEL_82801CA_IDE:
		case PCI_PRODUCT_INTEL_82801DB_IDE:
		case PCI_PRODUCT_INTEL_82801DBM_IDE:
		case PCI_PRODUCT_INTEL_82801EB_IDE:
		case PCI_PRODUCT_INTEL_82801EB_SATA:
		case PCI_PRODUCT_INTEL_82801ER_SATA:
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
			break;
		}
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_INTEL_82801AA_IDE:
		sc->sc_wdcdev.UDMA_cap = 4;
		break;
	case PCI_PRODUCT_INTEL_82801BAM_IDE:
	case PCI_PRODUCT_INTEL_82801BA_IDE:
	case PCI_PRODUCT_INTEL_82801CAM_IDE:
	case PCI_PRODUCT_INTEL_82801CA_IDE:
	case PCI_PRODUCT_INTEL_82801DB_IDE:
	case PCI_PRODUCT_INTEL_82801DBM_IDE:
	case PCI_PRODUCT_INTEL_82801EB_IDE:
	case PCI_PRODUCT_INTEL_82801EB_SATA:
	case PCI_PRODUCT_INTEL_82801ER_SATA:
		sc->sc_wdcdev.UDMA_cap = 5;
		break;
	default:
		sc->sc_wdcdev.UDMA_cap = 2;
		break;
	}
	if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_SATA ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801ER_SATA) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_SATA;
		sc->sc_wdcdev.set_modes = sata_setup_channel;
	} else if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82371FB_IDE) {
		sc->sc_wdcdev.set_modes = piix_setup_channel;
	} else {
		sc->sc_wdcdev.set_modes = piix3_4_setup_channel;
	}
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_SATA ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801ER_SATA)
		goto chansetup;

	WDCDEBUG_PRINT(("piix_setup_chip: old idetim=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM)),
	    DEBUG_PROBE);
	if (sc->sc_pp->ide_product != PCI_PRODUCT_INTEL_82371FB_IDE) {
		WDCDEBUG_PRINT((", sidetim=0x%x",
		    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM)),
		    DEBUG_PROBE);
		if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA) {
			WDCDEBUG_PRINT((", udamreg 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG)),
			    DEBUG_PROBE);
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DBM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_IDE) {
			WDCDEBUG_PRINT((", IDE_CONTROL 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG)),
			    DEBUG_PROBE);
		}

	}
	WDCDEBUG_PRINT(("\n"), DEBUG_PROBE);

chansetup:
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];

		/* SATA setup */
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_SATA ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801ER_SATA) {
			if (pciide_chansetup(sc, channel, interface) == 0)
				continue;
			pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
			    pciide_pci_intr);
			if (cp->hw_ok == 0)
				continue;
			sc->sc_wdcdev.set_modes(&cp->wdc_channel);
			continue;
		}

		/* PIIX is compat-only */
		if (pciide_chansetup(sc, channel, 0) == 0)
			continue;
		idetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
		if ((PIIX_IDETIM_READ(idetim, channel) &
		    PIIX_IDETIM_IDE) == 0) {
			printf("%s: %s ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		/* PIIX are compat-only pciide devices */
		pciide_map_compat_intr(pa, cp, channel, 0);
		if (cp->hw_ok == 0)
			continue;
		pciide_mapchan(pa, cp, 0, &cmdsize, &ctlsize, pciide_pci_intr);
		if (cp->hw_ok == 0)
			goto next;
		if (pciide_chan_candisable(cp)) {
			idetim = PIIX_IDETIM_CLEAR(idetim, PIIX_IDETIM_IDE,
			    channel);
			pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM,
			    idetim);
		}
		if (cp->hw_ok == 0)
			goto next;
		sc->sc_wdcdev.set_modes(&cp->wdc_channel);
next:
		if (cp->hw_ok == 0)
			pciide_unmap_compat_intr(pa, cp, channel, 0);
	}

	if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_SATA ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801ER_SATA)
		return;

	WDCDEBUG_PRINT(("piix_setup_chip: idetim=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM)),
	    DEBUG_PROBE);
	if (sc->sc_pp->ide_product != PCI_PRODUCT_INTEL_82371FB_IDE) {
		WDCDEBUG_PRINT((", sidetim=0x%x",
		    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM)),
		    DEBUG_PROBE);
		if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA) {
			WDCDEBUG_PRINT((", udamreg 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG)),
			    DEBUG_PROBE);
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DBM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_IDE) {
			WDCDEBUG_PRINT((", IDE_CONTROL 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG)),
			    DEBUG_PROBE);
		}
	}
	WDCDEBUG_PRINT(("\n"), DEBUG_PROBE);
}

void
piix_setup_channel(chp)
	struct channel_softc *chp;
{
	u_int8_t mode[2], drive;
	u_int32_t oidetim, idetim, idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct ata_drive_datas *drvp = cp->wdc_channel.ch_drive;

	oidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
	idetim = PIIX_IDETIM_CLEAR(oidetim, 0xffff, chp->channel);
	idedma_ctl = 0;

	/* set up new idetim: Enable IDE registers decode */
	idetim = PIIX_IDETIM_SET(idetim, PIIX_IDETIM_IDE,
	    chp->channel);

	/* setup DMA */
	pciide_channel_dma_setup(cp);

	/*
	 * Here we have to mess up with drives mode: PIIX can't have
	 * different timings for master and slave drives.
	 * We need to find the best combination.
	 */

	/* If both drives supports DMA, take the lower mode */
	if ((drvp[0].drive_flags & DRIVE_DMA) &&
	    (drvp[1].drive_flags & DRIVE_DMA)) {
		mode[0] = mode[1] =
		    min(drvp[0].DMA_mode, drvp[1].DMA_mode);
		    drvp[0].DMA_mode = mode[0];
		    drvp[1].DMA_mode = mode[1];
		goto ok;
	}
	/*
	 * If only one drive supports DMA, use its mode, and
	 * put the other one in PIO mode 0 if mode not compatible
	 */
	if (drvp[0].drive_flags & DRIVE_DMA) {
		mode[0] = drvp[0].DMA_mode;
		mode[1] = drvp[1].PIO_mode;
		if (piix_isp_pio[mode[1]] != piix_isp_dma[mode[0]] ||
		    piix_rtc_pio[mode[1]] != piix_rtc_dma[mode[0]])
			mode[1] = drvp[1].PIO_mode = 0;
		goto ok;
	}
	if (drvp[1].drive_flags & DRIVE_DMA) {
		mode[1] = drvp[1].DMA_mode;
		mode[0] = drvp[0].PIO_mode;
		if (piix_isp_pio[mode[0]] != piix_isp_dma[mode[1]] ||
		    piix_rtc_pio[mode[0]] != piix_rtc_dma[mode[1]])
			mode[0] = drvp[0].PIO_mode = 0;
		goto ok;
	}
	/*
	 * If both drives are not DMA, takes the lower mode, unless
	 * one of them is PIO mode < 2
	 */
	if (drvp[0].PIO_mode < 2) {
		mode[0] = drvp[0].PIO_mode = 0;
		mode[1] = drvp[1].PIO_mode;
	} else if (drvp[1].PIO_mode < 2) {
		mode[1] = drvp[1].PIO_mode = 0;
		mode[0] = drvp[0].PIO_mode;
	} else {
		mode[0] = mode[1] =
		    min(drvp[1].PIO_mode, drvp[0].PIO_mode);
		drvp[0].PIO_mode = mode[0];
		drvp[1].PIO_mode = mode[1];
	}
ok:	/* The modes are setup */
	for (drive = 0; drive < 2; drive++) {
		if (drvp[drive].drive_flags & DRIVE_DMA) {
			idetim |= piix_setup_idetim_timings(
			    mode[drive], 1, chp->channel);
			goto end;
		}
	}
	/* If we are there, none of the drives are DMA */
	if (mode[0] >= 2)
		idetim |= piix_setup_idetim_timings(
		    mode[0], 0, chp->channel);
	else
		idetim |= piix_setup_idetim_timings(
		    mode[1], 0, chp->channel);
end:	/*
	 * timing mode is now set up in the controller. Enable
	 * it per-drive
	 */
	for (drive = 0; drive < 2; drive++) {
		/* If no drive, skip */
		if ((drvp[drive].drive_flags & DRIVE) == 0)
			continue;
		idetim |= piix_setup_idetim_drvs(&drvp[drive]);
		if (drvp[drive].drive_flags & DRIVE_DMA)
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel),
		    idedma_ctl);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM, idetim);
	pciide_print_modes(cp);
}

void
piix3_4_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	u_int32_t oidetim, idetim, sidetim, udmareg, ideconf, idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int drive;
	int channel = chp->channel;

	oidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
	sidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM);
	udmareg = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG);
	ideconf = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG);
	idetim = PIIX_IDETIM_CLEAR(oidetim, 0xffff, channel);
	sidetim &= ~(PIIX_SIDETIM_ISP_MASK(channel) |
	    PIIX_SIDETIM_RTC_MASK(channel));

	idedma_ctl = 0;
	/* If channel disabled, no need to go further */
	if ((PIIX_IDETIM_READ(oidetim, channel) & PIIX_IDETIM_IDE) == 0)
		return;
	/* set up new idetim: Enable IDE registers decode */
	idetim = PIIX_IDETIM_SET(idetim, PIIX_IDETIM_IDE, channel);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		udmareg &= ~(PIIX_UDMACTL_DRV_EN(channel, drive) |
		    PIIX_UDMATIM_SET(0x3, channel, drive));
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0))
			goto pio;

		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DBM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_IDE) {
			ideconf |= PIIX_CONFIG_PINGPONG;
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CAM_IDE||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DBM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_IDE) {
			/* setup Ultra/100 */
			if (drvp->UDMA_mode > 2 &&
			    (ideconf & PIIX_CONFIG_CR(channel, drive)) == 0)
				drvp->UDMA_mode = 2;
			if (drvp->UDMA_mode > 4) {
				ideconf |= PIIX_CONFIG_UDMA100(channel, drive);
			} else {
				ideconf &= ~PIIX_CONFIG_UDMA100(channel, drive);
				if (drvp->UDMA_mode > 2) {
					ideconf |= PIIX_CONFIG_UDMA66(channel,
					    drive);
				} else {
					ideconf &= ~PIIX_CONFIG_UDMA66(channel,
					    drive);
				}
			}
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE) {
			/* setup Ultra/66 */
			if (drvp->UDMA_mode > 2 &&
			    (ideconf & PIIX_CONFIG_CR(channel, drive)) == 0)
				drvp->UDMA_mode = 2;
			if (drvp->UDMA_mode > 2)
				ideconf |= PIIX_CONFIG_UDMA66(channel, drive);
			else
				ideconf &= ~PIIX_CONFIG_UDMA66(channel, drive);
		}

		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			udmareg |= PIIX_UDMACTL_DRV_EN( channel,drive);
			udmareg |= PIIX_UDMATIM_SET(
			    piix4_sct_udma[drvp->UDMA_mode], channel, drive);
		} else {
			/* use Multiword DMA */
			drvp->drive_flags &= ~DRIVE_UDMA;
			if (drive == 0) {
				idetim |= piix_setup_idetim_timings(
				    drvp->DMA_mode, 1, channel);
			} else {
				sidetim |= piix_setup_sidetim_timings(
					drvp->DMA_mode, 1, channel);
				idetim =PIIX_IDETIM_SET(idetim,
				    PIIX_IDETIM_SITRE, channel);
			}
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:		/* use PIO mode */
		idetim |= piix_setup_idetim_drvs(drvp);
		if (drive == 0) {
			idetim |= piix_setup_idetim_timings(
			    drvp->PIO_mode, 0, channel);
		} else {
			sidetim |= piix_setup_sidetim_timings(
				drvp->PIO_mode, 0, channel);
			idetim =PIIX_IDETIM_SET(idetim,
			    PIIX_IDETIM_SITRE, channel);
		}
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(channel),
		    idedma_ctl);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM, idetim);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM, sidetim);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG, udmareg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_CONFIG, ideconf);
	pciide_print_modes(cp);
}


/* setup ISP and RTC fields, based on mode */
static u_int32_t
piix_setup_idetim_timings(mode, dma, channel)
	u_int8_t mode;
	u_int8_t dma;
	u_int8_t channel;
{

	if (dma)
		return PIIX_IDETIM_SET(0,
		    PIIX_IDETIM_ISP_SET(piix_isp_dma[mode]) |
		    PIIX_IDETIM_RTC_SET(piix_rtc_dma[mode]),
		    channel);
	else
		return PIIX_IDETIM_SET(0,
		    PIIX_IDETIM_ISP_SET(piix_isp_pio[mode]) |
		    PIIX_IDETIM_RTC_SET(piix_rtc_pio[mode]),
		    channel);
}

/* setup DTE, PPE, IE and TIME field based on PIO mode */
static u_int32_t
piix_setup_idetim_drvs(drvp)
	struct ata_drive_datas *drvp;
{
	u_int32_t ret = 0;
	struct channel_softc *chp = drvp->chnl_softc;
	u_int8_t channel = chp->channel;
	u_int8_t drive = drvp->drive;

	/*
	 * If drive is using UDMA, timings setups are independant
	 * So just check DMA and PIO here.
	 */
	if (drvp->drive_flags & DRIVE_DMA) {
		/* if mode = DMA mode 0, use compatible timings */
		if ((drvp->drive_flags & DRIVE_DMA) &&
		    drvp->DMA_mode == 0) {
			drvp->PIO_mode = 0;
			return ret;
		}
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_TIME(drive), channel);
		/*
		 * PIO and DMA timings are the same, use fast timings for PIO
		 * too, else use compat timings.
		 */
		if ((piix_isp_pio[drvp->PIO_mode] !=
		    piix_isp_dma[drvp->DMA_mode]) ||
		    (piix_rtc_pio[drvp->PIO_mode] !=
		    piix_rtc_dma[drvp->DMA_mode]))
			drvp->PIO_mode = 0;
		/* if PIO mode <= 2, use compat timings for PIO */
		if (drvp->PIO_mode <= 2) {
			ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_DTE(drive),
			    channel);
			return ret;
		}
	}

	/*
	 * Now setup PIO modes. If mode < 2, use compat timings.
	 * Else enable fast timings. Enable IORDY and prefetch/post
	 * if PIO mode >= 3.
	 */

	if (drvp->PIO_mode < 2)
		return ret;

	ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_TIME(drive), channel);
	if (drvp->PIO_mode >= 3) {
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_IE(drive), channel);
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_PPE(drive), channel);
	}
	return ret;
}

/* setup values in SIDETIM registers, based on mode */
static u_int32_t
piix_setup_sidetim_timings(mode, dma, channel)
	u_int8_t mode;
	u_int8_t dma;
	u_int8_t channel;
{
	if (dma)
		return PIIX_SIDETIM_ISP_SET(piix_isp_dma[mode], channel) |
		    PIIX_SIDETIM_RTC_SET(piix_rtc_dma[mode], channel);
	else
		return PIIX_SIDETIM_ISP_SET(piix_isp_pio[mode], channel) |
		    PIIX_SIDETIM_RTC_SET(piix_rtc_pio[mode], channel);
}

void
amd756_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	int channel;
	pcireg_t chanenable;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_AMD_8111_IDE:
		sc->sc_wdcdev.UDMA_cap = 6;
		break;
	case PCI_PRODUCT_AMD_766_IDE:
	case PCI_PRODUCT_AMD_PBC768_IDE:
		sc->sc_wdcdev.UDMA_cap = 5;
		break;
	default:
		sc->sc_wdcdev.UDMA_cap = 4;
		break;
	}
	sc->sc_wdcdev.set_modes = amd756_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	chanenable = pci_conf_read(sc->sc_pc, sc->sc_tag, AMD756_CHANSTATUS_EN);

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		if ((chanenable & AMD756_CHAN_EN(channel)) == 0) {
			printf("%s: %s ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;

		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);

		if (pciide_chan_candisable(cp)) {
			chanenable &= ~AMD756_CHAN_EN(channel);
		}
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}

		amd756_setup_channel(&cp->wdc_channel);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, AMD756_CHANSTATUS_EN,
	    chanenable);
	return;
}

void
amd756_setup_channel(chp)
	struct channel_softc *chp;
{
	u_int32_t udmatim_reg, datatim_reg;
	u_int8_t idedma_ctl;
	int mode, drive;
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	pcireg_t chanenable;
#ifndef	PCIIDE_AMD756_ENABLEDMA
	int product = sc->sc_pp->ide_product;
	int rev = sc->sc_rev;
#endif

	idedma_ctl = 0;
	datatim_reg = pci_conf_read(sc->sc_pc, sc->sc_tag, AMD756_DATATIM);
	udmatim_reg = pci_conf_read(sc->sc_pc, sc->sc_tag, AMD756_UDMA);
	datatim_reg &= ~AMD756_DATATIM_MASK(chp->channel);
	udmatim_reg &= ~AMD756_UDMA_MASK(chp->channel);
	chanenable = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    AMD756_CHANSTATUS_EN);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0)) {
			mode = drvp->PIO_mode;
			goto pio;
		}
		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;

			/* Check cable */
			if ((chanenable & AMD756_CABLE(chp->channel,
			    drive)) == 0 && drvp->UDMA_mode > 2) {
				WDCDEBUG_PRINT(("%s(%s:%d:%d): 80-wire "
				    "cable not detected\n", drvp->drive_name,
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    chp->channel, drive), DEBUG_PROBE);
				drvp->UDMA_mode = 2;
			}

			udmatim_reg |= AMD756_UDMA_EN(chp->channel, drive) |
			    AMD756_UDMA_EN_MTH(chp->channel, drive) |
			    AMD756_UDMA_TIME(chp->channel, drive,
				amd756_udma_tim[drvp->UDMA_mode]);
			/* can use PIO timings, MW DMA unused */
			mode = drvp->PIO_mode;
		} else {
			/* use Multiword DMA, but only if revision is OK */
			drvp->drive_flags &= ~DRIVE_UDMA;
#ifndef PCIIDE_AMD756_ENABLEDMA
			/*
			 * The workaround doesn't seem to be necessary
			 * with all drives, so it can be disabled by
			 * PCIIDE_AMD756_ENABLEDMA. It causes a hard hang if
			 * triggered.
			 */
			if (AMD756_CHIPREV_DISABLEDMA(product, rev)) {
				printf("%s:%d:%d: multi-word DMA disabled due "
				    "to chip revision\n",
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    chp->channel, drive);
				mode = drvp->PIO_mode;
				drvp->drive_flags &= ~DRIVE_DMA;
				goto pio;
			}
#endif
			/* mode = min(pio, dma+2) */
			if (drvp->PIO_mode <= (drvp->DMA_mode +2))
				mode = drvp->PIO_mode;
			else
				mode = drvp->DMA_mode + 2;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:		/* setup PIO mode */
		if (mode <= 2) {
			drvp->DMA_mode = 0;
			drvp->PIO_mode = 0;
			mode = 0;
		} else {
			drvp->PIO_mode = mode;
			drvp->DMA_mode = mode - 2;
		}
		datatim_reg |=
		    AMD756_DATATIM_PULSE(chp->channel, drive,
			amd756_pio_set[mode]) |
		    AMD756_DATATIM_RECOV(chp->channel, drive,
			amd756_pio_rec[mode]);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel),
		    idedma_ctl);
	}
	pciide_print_modes(cp);
	pci_conf_write(sc->sc_pc, sc->sc_tag, AMD756_DATATIM, datatim_reg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, AMD756_UDMA, udmatim_reg);
}

void
apollo_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	int channel;
	u_int32_t ideconf;
	bus_size_t cmdsize, ctlsize;
	pcitag_t pcib_tag;
	pcireg_t pcib_id, pcib_class;

	if (pciide_chipen(sc, pa) == 0)
		return;
	pcib_tag = pci_make_tag(pa->pa_pc, pa->pa_bus, pa->pa_device, 0);

	pcib_id = pci_conf_read(sc->sc_pc, pcib_tag, PCI_ID_REG);
	pcib_class = pci_conf_read(sc->sc_pc, pcib_tag, PCI_CLASS_REG);

	switch (PCI_PRODUCT(pcib_id)) {
	case PCI_PRODUCT_VIATECH_VT82C586_ISA:
		if (PCI_REVISION(pcib_class) >= 0x02) {
			printf(": ATA33");
			sc->sc_wdcdev.UDMA_cap = 2;
		} else {
			printf(": DMA");
			sc->sc_wdcdev.UDMA_cap = 0;
		}
		break;
	case PCI_PRODUCT_VIATECH_VT82C596A:
		if (PCI_REVISION(pcib_class) >= 0x12) {
			printf(": ATA66");
			sc->sc_wdcdev.UDMA_cap = 4;
		} else {
			printf(": ATA33");
			sc->sc_wdcdev.UDMA_cap = 2;
		}
		break;

	case PCI_PRODUCT_VIATECH_VT82C686A_ISA:
		if (PCI_REVISION(pcib_class) >= 0x40) {
			printf(": ATA100");
			sc->sc_wdcdev.UDMA_cap = 5;
		} else {
			printf(": ATA66");
			sc->sc_wdcdev.UDMA_cap = 4;
		}
		break;
	case PCI_PRODUCT_VIATECH_VT8231_ISA:
		printf(": ATA100");
		sc->sc_wdcdev.UDMA_cap = 5;
		break;
	case PCI_PRODUCT_VIATECH_VT8366_ISA:
		printf(": ATA100");
		sc->sc_wdcdev.UDMA_cap = 5;
		break;
	case PCI_PRODUCT_VIATECH_VT8233_ISA:
		printf(": ATA133");
		sc->sc_wdcdev.UDMA_cap = 6;
		break;
	case PCI_PRODUCT_VIATECH_VT8235_ISA:
		printf(": ATA133");
		sc->sc_wdcdev.UDMA_cap = 6;
		break;
	case PCI_PRODUCT_VIATECH_VT8237_SATA:
		printf(": ATA133");
		sc->sc_wdcdev.UDMA_cap = 6;
		break;
	default:
		printf(": DMA");
		sc->sc_wdcdev.UDMA_cap = 0;
		break;
	}

	pciide_mapreg_dma(sc, pa);
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		if (sc->sc_wdcdev.UDMA_cap > 0)
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = apollo_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	WDCDEBUG_PRINT(("apollo_chip_map: old APO_IDECONF=0x%x, "
	    "APO_CTLMISC=0x%x, APO_DATATIM=0x%x, APO_UDMA=0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_IDECONF),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_CTLMISC),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_DATATIM),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_UDMA)),
	    DEBUG_PROBE);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		ideconf = pci_conf_read(sc->sc_pc, sc->sc_tag, APO_IDECONF);
		if ((ideconf & APO_IDECONF_EN(channel)) == 0) {
			printf("%s: %s ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;

		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0) {
			goto next;
		}
		if (pciide_chan_candisable(cp)) {
			ideconf &= ~APO_IDECONF_EN(channel);
			pci_conf_write(sc->sc_pc, sc->sc_tag, APO_IDECONF,
			    ideconf);
		}

		if (cp->hw_ok == 0)
			goto next;
		apollo_setup_channel(&sc->pciide_channels[channel].wdc_channel);
next:
		if (cp->hw_ok == 0)
			pciide_unmap_compat_intr(pa, cp, channel, interface);
	}
	WDCDEBUG_PRINT(("apollo_chip_map: APO_DATATIM=0x%x, APO_UDMA=0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_DATATIM),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_UDMA)), DEBUG_PROBE);
}

void
apollo_sata_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	int channel;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;

	if (interface == 0) {
		WDCDEBUG_PRINT(("apollo_sata_chip_map interface == 0\n"),
		    DEBUG_PROBE);
		interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
		    PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);
	}

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	printf("\n");

	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA |
		    WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.UDMA_cap = 6;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE | WDC_CAPABILITY_SATA;
	sc->sc_wdcdev.set_modes = sata_setup_channel;

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		     pciide_pci_intr);
		sata_setup_channel(&cp->wdc_channel);
	}
}

void
apollo_setup_channel(chp)
	struct channel_softc *chp;
{
	u_int32_t udmatim_reg, datatim_reg;
	u_int8_t idedma_ctl;
	int mode, drive;
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	idedma_ctl = 0;
	datatim_reg = pci_conf_read(sc->sc_pc, sc->sc_tag, APO_DATATIM);
	udmatim_reg = pci_conf_read(sc->sc_pc, sc->sc_tag, APO_UDMA);
	datatim_reg &= ~APO_DATATIM_MASK(chp->channel);
	udmatim_reg &= ~APO_UDMA_MASK(chp->channel);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	/*
	 * We can't mix Ultra/33 and Ultra/66 on the same channel, so
	 * downgrade to Ultra/33 if needed
	 */
	if ((chp->ch_drive[0].drive_flags & DRIVE_UDMA) &&
	    (chp->ch_drive[1].drive_flags & DRIVE_UDMA)) {
		/* both drives UDMA */
		if (chp->ch_drive[0].UDMA_mode > 2 &&
		    chp->ch_drive[1].UDMA_mode <= 2) {
			/* drive 0 Ultra/66, drive 1 Ultra/33 */
			chp->ch_drive[0].UDMA_mode = 2;
		} else if (chp->ch_drive[1].UDMA_mode > 2 &&
		    chp->ch_drive[0].UDMA_mode <= 2) {
			/* drive 1 Ultra/66, drive 0 Ultra/33 */
			chp->ch_drive[1].UDMA_mode = 2;
		}
	}

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0)) {
			mode = drvp->PIO_mode;
			goto pio;
		}
		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			udmatim_reg |= APO_UDMA_EN(chp->channel, drive) |
			    APO_UDMA_EN_MTH(chp->channel, drive);
			if (sc->sc_wdcdev.UDMA_cap == 6) {
				udmatim_reg |= APO_UDMA_TIME(chp->channel,
				    drive, apollo_udma133_tim[drvp->UDMA_mode]);
			} else if (sc->sc_wdcdev.UDMA_cap == 5) {
				/* 686b */
				udmatim_reg |= APO_UDMA_TIME(chp->channel,
				    drive, apollo_udma100_tim[drvp->UDMA_mode]);
			} else if (sc->sc_wdcdev.UDMA_cap == 4) {
				/* 596b or 686a */
				udmatim_reg |= APO_UDMA_CLK66(chp->channel);
				udmatim_reg |= APO_UDMA_TIME(chp->channel,
				    drive, apollo_udma66_tim[drvp->UDMA_mode]);
			} else {
				/* 596a or 586b */
				udmatim_reg |= APO_UDMA_TIME(chp->channel,
				    drive, apollo_udma33_tim[drvp->UDMA_mode]);
			}
			/* can use PIO timings, MW DMA unused */
			mode = drvp->PIO_mode;
		} else {
			/* use Multiword DMA */
			drvp->drive_flags &= ~DRIVE_UDMA;
			/* mode = min(pio, dma+2) */
			if (drvp->PIO_mode <= (drvp->DMA_mode +2))
				mode = drvp->PIO_mode;
			else
				mode = drvp->DMA_mode + 2;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:		/* setup PIO mode */
		if (mode <= 2) {
			drvp->DMA_mode = 0;
			drvp->PIO_mode = 0;
			mode = 0;
		} else {
			drvp->PIO_mode = mode;
			drvp->DMA_mode = mode - 2;
		}
		datatim_reg |=
		    APO_DATATIM_PULSE(chp->channel, drive,
			apollo_pio_set[mode]) |
		    APO_DATATIM_RECOV(chp->channel, drive,
			apollo_pio_rec[mode]);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel),
		    idedma_ctl);
	}
	pciide_print_modes(cp);
	pci_conf_write(sc->sc_pc, sc->sc_tag, APO_DATATIM, datatim_reg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, APO_UDMA, udmatim_reg);
}

void
cmd_channel_map(pa, sc, channel)
	struct pci_attach_args *pa;
	struct pciide_softc *sc;
	int channel;
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	bus_size_t cmdsize, ctlsize;
	u_int8_t ctrl = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CTRL);
	pcireg_t interface;
	int one_channel;

	/*
	 * The 0648/0649 can be told to identify as a RAID controller.
	 * In this case, we have to fake interface
	 */
	if (PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCIIDE_INTERFACE_SETTABLE(0) |
		    PCIIDE_INTERFACE_SETTABLE(1);
		if (pciide_pci_read(pa->pa_pc, pa->pa_tag, CMD_CONF) &
		    CMD_CONF_DSA1)
			interface |= PCIIDE_INTERFACE_PCI(0) |
			    PCIIDE_INTERFACE_PCI(1);
	} else {
		interface = PCI_INTERFACE(pa->pa_class);
	}

	sc->wdc_chanarray[channel] = &cp->wdc_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->wdc_channel.channel = channel;
	cp->wdc_channel.wdc = &sc->sc_wdcdev;

	/*
	 * Older CMD64X doesn't have independant channels
	 */
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_CMDTECH_649:
		one_channel = 0;
		break;
	default:
		one_channel = 1;
		break;
	}

	if (channel > 0 && one_channel) {
		cp->wdc_channel.ch_queue =
		    sc->pciide_channels[0].wdc_channel.ch_queue;
	} else {
		cp->wdc_channel.ch_queue =
		    malloc(sizeof(struct channel_queue), M_DEVBUF, M_NOWAIT);
	}
	if (cp->wdc_channel.ch_queue == NULL) {
		printf(
		    "%s: %s cannot allocate memory for command queue",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return;
	}

	/*
	 * with a CMD PCI64x, if we get here, the first channel is enabled:
	 * there's no way to disable the first channel without disabling
	 * the whole device
	 */
	 if (channel != 0 && (ctrl & CMD_CTRL_2PORT) == 0) {
		printf("%s: %s ignored (disabled)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return;
	}
	cp->hw_ok = 1;
	pciide_map_compat_intr(pa, cp, channel, interface);
	if (cp->hw_ok == 0)
		return;
	pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize, cmd_pci_intr);
	if (cp->hw_ok == 0) {
		pciide_unmap_compat_intr(pa, cp, channel, interface);
		return;
	}
	if (pciide_chan_candisable(cp)) {
		if (channel == 1) {
			ctrl &= ~CMD_CTRL_2PORT;
			pciide_pci_write(pa->pa_pc, pa->pa_tag,
			    CMD_CTRL, ctrl);
			pciide_unmap_compat_intr(pa, cp, channel, interface);
		}
	}
}

int
cmd_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;
	u_int32_t priirq, secirq;

	rv = 0;
	priirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CONF);
	secirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23);
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		if ((i == 0 && (priirq & CMD_CONF_DRV0_INTR)) ||
		    (i == 1 && (secirq & CMD_ARTTIM23_IRQ))) {
			crv = wdcintr(wdc_cp);
			if (crv == 0) {
#if 0
				printf("%s:%d: bogus intr\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, i);
#endif
			} else
				rv = 1;
		}
	}
	return rv;
}

void
cmd_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	int channel;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	/*
 	 * For a CMD PCI064x, the use of PCI_COMMAND_IO_ENABLE
	 * and base address registers can be disabled at
 	 * hardware level. In this case, the device is wired
	 * in compat mode and its first channel is always enabled,
	 * but we can't rely on PCI_COMMAND_IO_ENABLE.
	 * In fact, it seems that the first channel of the CMD PCI0640
	 * can't be disabled.
 	 */

#ifdef PCIIDE_CMD064x_DISABLE
	if (pciide_chipen(sc, pa) == 0)
		return;
#endif

	printf(": no DMA");
	sc->sc_dma_ok = 0;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16;

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cmd_channel_map(pa, sc, channel);
	}
}

void
cmd0643_9_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	int rev = sc->sc_rev;
	pcireg_t interface;

	/*
	 * The 0648/0649 can be told to identify as a RAID controller.
	 * In this case, we have to fake interface
	 */
	if (PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCIIDE_INTERFACE_SETTABLE(0) |
		    PCIIDE_INTERFACE_SETTABLE(1);
		if (pciide_pci_read(pa->pa_pc, pa->pa_tag, CMD_CONF) &
		    CMD_CONF_DSA1)
			interface |= PCIIDE_INTERFACE_PCI(0) |
			    PCIIDE_INTERFACE_PCI(1);
	} else {
		interface = PCI_INTERFACE(pa->pa_class);
	}

	/*
	 * For a CMD PCI064x, the use of PCI_COMMAND_IO_ENABLE
	 * and base address registers can be disabled at
	 * hardware level. In this case, the device is wired
	 * in compat mode and its first channel is always enabled,
 	 * but we can't rely on PCI_COMMAND_IO_ENABLE.
	 * In fact, it seems that the first channel of the CMD PCI0640
	 * can't be disabled.
	*/

#ifdef PCIIDE_CMD064x_DISABLE
	if (pciide_chipen(sc, pa) == 0)
		return;
#endif
	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		switch (sc->sc_pp->ide_product) {
		case PCI_PRODUCT_CMDTECH_649:
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
			sc->sc_wdcdev.UDMA_cap = 5;
			sc->sc_wdcdev.irqack = cmd646_9_irqack;
			break;
		case PCI_PRODUCT_CMDTECH_648:
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
			sc->sc_wdcdev.UDMA_cap = 4;
			sc->sc_wdcdev.irqack = cmd646_9_irqack;
			break;
		case PCI_PRODUCT_CMDTECH_646:
			if (rev >= CMD0646U2_REV) {
				sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
				sc->sc_wdcdev.UDMA_cap = 2;
			} else if (rev >= CMD0646U_REV) {
			/*
			 * Linux's driver claims that the 646U is broken
			 * with UDMA. Only enable it if we know what we're
			 * doing
			 */
#ifdef PCIIDE_CMD0646U_ENABLEUDMA
				sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
				sc->sc_wdcdev.UDMA_cap = 2;
#endif
				/* explicitly disable UDMA */
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(0), 0);
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(1), 0);
			}
			sc->sc_wdcdev.irqack = cmd646_9_irqack;
			break;
		default:
			sc->sc_wdcdev.irqack = pciide_irqack;
		}
	}

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = cmd0643_9_setup_channel;

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	WDCDEBUG_PRINT(("cmd0643_9_chip_map: old timings reg 0x%x 0x%x\n",
		pci_conf_read(sc->sc_pc, sc->sc_tag, 0x54),
		pci_conf_read(sc->sc_pc, sc->sc_tag, 0x58)),
		DEBUG_PROBE);
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		cmd_channel_map(pa, sc, channel);
		if (cp->hw_ok == 0)
			continue;
		cmd0643_9_setup_channel(&cp->wdc_channel);
	}
	/*
	 * note - this also makes sure we clear the irq disable and reset
	 * bits
	 */
	pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_DMA_MODE, CMD_DMA_MULTIPLE);
	WDCDEBUG_PRINT(("cmd0643_9_chip_map: timings reg now 0x%x 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, 0x54),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, 0x58)),
	    DEBUG_PROBE);
}

void
cmd0643_9_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	u_int8_t tim;
	u_int32_t idedma_ctl, udma_reg;
	int drive;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	idedma_ctl = 0;
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		tim = cmd0643_9_data_tim_pio[drvp->PIO_mode];
		if (drvp->drive_flags & (DRIVE_DMA | DRIVE_UDMA)) {
			if (drvp->drive_flags & DRIVE_UDMA) {
				/* UltraDMA on a 646U2, 0648 or 0649 */
				drvp->drive_flags &= ~DRIVE_DMA;
				udma_reg = pciide_pci_read(sc->sc_pc,
				    sc->sc_tag, CMD_UDMATIM(chp->channel));
				if (drvp->UDMA_mode > 2 &&
				    (pciide_pci_read(sc->sc_pc, sc->sc_tag,
				    CMD_BICSR) &
				    CMD_BICSR_80(chp->channel)) == 0) {
					WDCDEBUG_PRINT(("%s(%s:%d:%d): "
					    "80-wire cable not detected\n",
					    drvp->drive_name,
					    sc->sc_wdcdev.sc_dev.dv_xname,
					    chp->channel, drive), DEBUG_PROBE);
					drvp->UDMA_mode = 2;
				}
				if (drvp->UDMA_mode > 2)
					udma_reg &= ~CMD_UDMATIM_UDMA33(drive);
				else if (sc->sc_wdcdev.UDMA_cap > 2)
					udma_reg |= CMD_UDMATIM_UDMA33(drive);
				udma_reg |= CMD_UDMATIM_UDMA(drive);
				udma_reg &= ~(CMD_UDMATIM_TIM_MASK <<
				    CMD_UDMATIM_TIM_OFF(drive));
				udma_reg |=
				    (cmd0646_9_tim_udma[drvp->UDMA_mode] <<
				    CMD_UDMATIM_TIM_OFF(drive));
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(chp->channel), udma_reg);
			} else {
				/*
				 * use Multiword DMA.
				 * Timings will be used for both PIO and DMA,
				 * so adjust DMA mode if needed
				 * if we have a 0646U2/8/9, turn off UDMA
				 */
				if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA) {
					udma_reg = pciide_pci_read(sc->sc_pc,
					    sc->sc_tag,
					    CMD_UDMATIM(chp->channel));
					udma_reg &= ~CMD_UDMATIM_UDMA(drive);
					pciide_pci_write(sc->sc_pc, sc->sc_tag,
					    CMD_UDMATIM(chp->channel),
					    udma_reg);
				}
				if (drvp->PIO_mode >= 3 &&
				    (drvp->DMA_mode + 2) > drvp->PIO_mode) {
					drvp->DMA_mode = drvp->PIO_mode - 2;
				}
				tim = cmd0643_9_data_tim_dma[drvp->DMA_mode];
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    CMD_DATA_TIM(chp->channel, drive), tim);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel),
		    idedma_ctl);
	}
	pciide_print_modes(cp);
#ifdef __sparc64__
	/*
	 * The Ultra 5 has a tendency to hang during reboot.  This is due
	 * to the PCI0646U asserting a PCI interrupt line when the chip
	 * registers claim that it is not.  Performing a reset at this
	 * point appears to eliminate the symptoms.  It is likely the
	 * real cause is still lurking somewhere in the code.
	 */
	wdcreset(chp, SILENT);
#endif /* __sparc64__ */
}

void
cmd646_9_irqack(chp)
	struct channel_softc *chp;
{
	u_int32_t priirq, secirq;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	if (chp->channel == 0) {
		priirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CONF);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_CONF, priirq);
	} else {
		secirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23, secirq);
	}
	pciide_irqack(chp);
}

void
cmd680_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;
	printf("\n%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.UDMA_cap = 6;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = cmd680_setup_channel;

	pciide_pci_write(sc->sc_pc, sc->sc_tag, 0x80, 0x00);
	pciide_pci_write(sc->sc_pc, sc->sc_tag, 0x84, 0x00);
	pciide_pci_write(sc->sc_pc, sc->sc_tag, 0x8a,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, 0x8a) | 0x01);
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		cmd680_channel_map(pa, sc, channel);
		if (cp->hw_ok == 0)
			continue;
		cmd680_setup_channel(&cp->wdc_channel);
	}
}

void
cmd680_channel_map(pa, sc, channel)
	struct pci_attach_args *pa;
	struct pciide_softc *sc;
	int channel;
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	bus_size_t cmdsize, ctlsize;
	int interface, i, reg;
	static const u_int8_t init_val[] =
	    {             0x8a, 0x32, 0x8a, 0x32, 0x8a, 0x32,
	      0x92, 0x43, 0x92, 0x43, 0x09, 0x40, 0x09, 0x40 };

	if (PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCIIDE_INTERFACE_SETTABLE(0) |
		    PCIIDE_INTERFACE_SETTABLE(1);
		interface |= PCIIDE_INTERFACE_PCI(0) |
		    PCIIDE_INTERFACE_PCI(1);
	} else {
		interface = PCI_INTERFACE(pa->pa_class);
	}

	sc->wdc_chanarray[channel] = &cp->wdc_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->wdc_channel.channel = channel;
	cp->wdc_channel.wdc = &sc->sc_wdcdev;

	cp->wdc_channel.ch_queue =
	    malloc(sizeof(struct channel_queue), M_DEVBUF, M_NOWAIT);
	if (cp->wdc_channel.ch_queue == NULL) {
		printf("%s %s: "
		    "can't allocate memory for command queue",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		    return;
	}

	/* XXX */
	reg = 0xa2 + channel * 16;
	for (i = 0; i < sizeof(init_val); i++)
		pciide_pci_write(sc->sc_pc, sc->sc_tag, reg + i, init_val[i]);

	printf("%s: %s %s to %s mode\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
	    (interface & PCIIDE_INTERFACE_SETTABLE(channel)) ?
	    "configured" : "wired",
	    (interface & PCIIDE_INTERFACE_PCI(channel)) ?
	    "native-PCI" : "compatibility");

	pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize, pciide_pci_intr);
	if (cp->hw_ok == 0)
		return;
	pciide_map_compat_intr(pa, cp, channel, interface);
}

void
cmd680_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	u_int8_t mode, off, scsc;
	u_int16_t val;
	u_int32_t idedma_ctl;
	int drive;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	pci_chipset_tag_t pc = sc->sc_pc;
	pcitag_t pa = sc->sc_tag;
	static const u_int8_t udma2_tbl[] =
	    { 0x0f, 0x0b, 0x07, 0x06, 0x03, 0x02, 0x01 };
	static const u_int8_t udma_tbl[] =
	    { 0x0c, 0x07, 0x05, 0x04, 0x02, 0x01, 0x00 };
	static const u_int16_t dma_tbl[] =
	    { 0x2208, 0x10c2, 0x10c1 };
	static const u_int16_t pio_tbl[] =
	    { 0x328a, 0x2283, 0x1104, 0x10c3, 0x10c1 };

	idedma_ctl = 0;
	pciide_channel_dma_setup(cp);
	mode = pciide_pci_read(pc, pa, 0x80 + chp->channel * 4);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		mode &= ~(0x03 << (drive * 4));
		if (drvp->drive_flags & DRIVE_UDMA) {
			drvp->drive_flags &= ~DRIVE_DMA;
			off = 0xa0 + chp->channel * 16;
			if (drvp->UDMA_mode > 2 &&
			    (pciide_pci_read(pc, pa, off) & 0x01) == 0)
				drvp->UDMA_mode = 2;
			scsc = pciide_pci_read(pc, pa, 0x8a);
			if (drvp->UDMA_mode == 6 && (scsc & 0x30) == 0) {
				pciide_pci_write(pc, pa, 0x8a, scsc | 0x01);
				scsc = pciide_pci_read(pc, pa, 0x8a);
				if ((scsc & 0x30) == 0)
					drvp->UDMA_mode = 5;
			}
			mode |= 0x03 << (drive * 4);
			off = 0xac + chp->channel * 16 + drive * 2;
			val = pciide_pci_read(pc, pa, off) & ~0x3f;
			if (scsc & 0x30)
				val |= udma2_tbl[drvp->UDMA_mode];
			else
				val |= udma_tbl[drvp->UDMA_mode];
			pciide_pci_write(pc, pa, off, val);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & DRIVE_DMA) {
			mode |= 0x02 << (drive * 4);
			off = 0xa8 + chp->channel * 16 + drive * 2;
			val = dma_tbl[drvp->DMA_mode];
			pciide_pci_write(pc, pa, off, val & 0xff);
			pciide_pci_write(pc, pa, off, val >> 8);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			mode |= 0x01 << (drive * 4);
			off = 0xa4 + chp->channel * 16 + drive * 2;
			val = pio_tbl[drvp->PIO_mode];
			pciide_pci_write(pc, pa, off, val & 0xff);
			pciide_pci_write(pc, pa, off, val >> 8);
		}
	}

	pciide_pci_write(pc, pa, 0x80 + chp->channel * 4, mode);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel),
		    idedma_ctl);
	}
	pciide_print_modes(cp);
}

void
sii3112_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	bus_size_t cmdsize, ctlsize;
	pcireg_t interface;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);

	/*
	 * Rev. <= 0x01 of the 3112 have a bug that can cause data
	 * corruption if DMA transfers cross an 8K boundary.  This is
	 * apparently hard to tickle, but we'll go ahead and play it
	 * safe.
	 */
	if (sc->sc_rev <= 0x01) {
		sc->sc_dma_maxsegsz = 8192;
		sc->sc_dma_boundary = 8192;
	}

	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE | WDC_CAPABILITY_SATA;
	sc->sc_wdcdev.PIO_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.DMA_cap = 2;
		sc->sc_wdcdev.UDMA_cap = 6;
	}
	sc->sc_wdcdev.set_modes = sii3112_setup_channel;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	/*
	 * The 3112 can be told to identify as a RAID controller.
	 * In this case, we have to fake interface
	 */
	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCI_INTERFACE(pa->pa_class);
	} else {
		interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
		    PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);
	}

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0)
			continue;
		pciide_map_compat_intr(pa, cp, channel, interface);
		sc->sc_wdcdev.set_modes(&cp->wdc_channel);
	}
}

void
sii3112_setup_channel(struct channel_softc *chp)
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t idedma_ctl, dtm;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;
	dtm = 0;

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			dtm |= DTM_IDEx_DMA;
		} else if (drvp->drive_flags & DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			dtm |= DTM_IDEx_DMA;
		} else {
			dtm |= DTM_IDEx_PIO;
		}
	}

	/*
	 * Nothing to do to setup modes; it is meaningless in S-ATA
	 * (but many S-ATA drives still want to get the SET_FEATURE
	 * command).
	 */
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel), idedma_ctl);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag,
	    chp->channel == 0 ? SII3112_DTM_IDE0 : SII3112_DTM_IDE1, dtm);
	pciide_print_modes(cp);
}

void
cy693_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;
	/*
	 * this chip has 2 PCI IDE functions, one for primary and one for
	 * secondary. So we need to call pciide_mapregs_compat() with
	 * the real channel
	 */
	if (pa->pa_function == 1) {
		sc->sc_cy_compatchan = 0;
	} else if (pa->pa_function == 2) {
		sc->sc_cy_compatchan = 1;
	} else {
		printf(": unexpected PCI function %d\n", pa->pa_function);
		return;
	}

	if (interface & PCIIDE_INTERFACE_BUS_MASTER_DMA) {
		printf(": DMA");
		pciide_mapreg_dma(sc, pa);
	} else {
		printf(": no DMA");
		sc->sc_dma_ok = 0;
	}

	sc->sc_cy_handle = cy82c693_init(pa->pa_iot);
	if (sc->sc_cy_handle == NULL) {
		printf(", (unable to map ctl registers)");
		sc->sc_dma_ok = 0;
	}

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = cy693_setup_channel;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = 1;

	/* Only one channel for this chip; if we are here it's enabled */
	cp = &sc->pciide_channels[0];
	sc->wdc_chanarray[0] = &cp->wdc_channel;
	cp->name = PCIIDE_CHANNEL_NAME(0);
	cp->wdc_channel.channel = 0;
	cp->wdc_channel.wdc = &sc->sc_wdcdev;
	cp->wdc_channel.ch_queue =
	    malloc(sizeof(struct channel_queue), M_DEVBUF, M_NOWAIT);
	if (cp->wdc_channel.ch_queue == NULL) {
		printf(": cannot allocate memory for command queue\n");
		return;
	}
	printf(", %s %s to ", PCIIDE_CHANNEL_NAME(0),
	    (interface & PCIIDE_INTERFACE_SETTABLE(0)) ?
	    "configured" : "wired");
	if (interface & PCIIDE_INTERFACE_PCI(0)) {
		printf("native-PCI\n");
		cp->hw_ok = pciide_mapregs_native(pa, cp, &cmdsize, &ctlsize,
		    pciide_pci_intr);
	} else {
		printf("compatibility\n");
		cp->hw_ok = pciide_mapregs_compat(pa, cp, sc->sc_cy_compatchan,
		    &cmdsize, &ctlsize);
	}

	cp->wdc_channel.data32iot = cp->wdc_channel.cmd_iot;
	cp->wdc_channel.data32ioh = cp->wdc_channel.cmd_ioh;
	pciide_map_compat_intr(pa, cp, sc->sc_cy_compatchan, interface);
	if (cp->hw_ok == 0)
		return;
	wdcattach(&cp->wdc_channel);
	if (pciide_chan_candisable(cp)) {
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    PCI_COMMAND_STATUS_REG, 0);
	}
	if (cp->hw_ok == 0) {
		pciide_unmap_compat_intr(pa, cp, sc->sc_cy_compatchan,
		    interface);
		return;
	}

	WDCDEBUG_PRINT(("cy693_chip_map: old timings reg 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL)),DEBUG_PROBE);
	cy693_setup_channel(&cp->wdc_channel);
	WDCDEBUG_PRINT(("cy693_chip_map: new timings reg 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL)), DEBUG_PROBE);
}

void
cy693_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t cy_cmd_ctrl;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int dma_mode = -1;

	cy_cmd_ctrl = idedma_ctl = 0;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if (drvp->drive_flags & DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			/* use Multiword DMA */
			if (dma_mode == -1 || dma_mode > drvp->DMA_mode)
				dma_mode = drvp->DMA_mode;
		}
		cy_cmd_ctrl |= (cy_pio_pulse[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOW_PULSE_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_rec[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOW_REC_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_pulse[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOR_PULSE_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_rec[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOR_REC_OFF(drive));
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL, cy_cmd_ctrl);
	chp->ch_drive[0].DMA_mode = dma_mode;
	chp->ch_drive[1].DMA_mode = dma_mode;

	if (dma_mode == -1)
		dma_mode = 0;

	if (sc->sc_cy_handle != NULL) {
		/* Note: `multiple' is implied. */
		cy82c693_write(sc->sc_cy_handle,
		    (sc->sc_cy_compatchan == 0) ?
		    CY_DMA_IDX_PRIMARY : CY_DMA_IDX_SECONDARY, dma_mode);
	}

	pciide_print_modes(cp);

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel), idedma_ctl);
	}
}

static struct sis_hostbr_type {
	u_int16_t id;
	u_int8_t rev;
	u_int8_t udma_mode;
	char *name;
	u_int8_t type;
#define SIS_TYPE_NOUDMA	0
#define SIS_TYPE_66	1
#define SIS_TYPE_100OLD	2
#define SIS_TYPE_100NEW 3
#define SIS_TYPE_133OLD 4
#define SIS_TYPE_133NEW 5
#define SIS_TYPE_SOUTH	6
} sis_hostbr_type[] = {
	/* Most infos here are from sos@freebsd.org */
	{PCI_PRODUCT_SIS_530, 0x00, 4, "530", SIS_TYPE_66},
#if 0
	/*
	 * controllers associated to a rev 0x2 530 Host to PCI Bridge
	 * have problems with UDMA (info provided by Christos)
	 */
	{PCI_PRODUCT_SIS_530, 0x02, 0, "530 (buggy)", SIS_TYPE_NOUDMA},
#endif
	{PCI_PRODUCT_SIS_540, 0x00, 4, "540", SIS_TYPE_66},
	{PCI_PRODUCT_SIS_550, 0x00, 4, "550", SIS_TYPE_66},
	{PCI_PRODUCT_SIS_620, 0x00, 4, "620", SIS_TYPE_66},
	{PCI_PRODUCT_SIS_630, 0x00, 4, "630", SIS_TYPE_66},
	{PCI_PRODUCT_SIS_630, 0x30, 5, "630S", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_633, 0x00, 5, "633", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_635, 0x00, 5, "635", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_640, 0x00, 4, "640", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_645, 0x00, 6, "645", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_646, 0x00, 6, "645DX", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_648, 0x00, 6, "648", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_650, 0x00, 6, "650", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_651, 0x00, 6, "651", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_652, 0x00, 6, "652", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_655, 0x00, 6, "655", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_658, 0x00, 6, "658", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_730, 0x00, 5, "730", SIS_TYPE_100OLD},
	{PCI_PRODUCT_SIS_733, 0x00, 5, "733", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_735, 0x00, 5, "735", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_740, 0x00, 5, "740", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_745, 0x00, 5, "745", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_746, 0x00, 6, "746", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_748, 0x00, 6, "748", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_750, 0x00, 6, "750", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_751, 0x00, 6, "751", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_752, 0x00, 6, "752", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_755, 0x00, 6, "755", SIS_TYPE_SOUTH},
	/*
	 * From sos@freebsd.org: the 0x961 ID will never be found in real world
	 * {PCI_PRODUCT_SIS_961, 0x00, 6, "961", SIS_TYPE_133NEW},
	 */
	{PCI_PRODUCT_SIS_962, 0x00, 6, "962", SIS_TYPE_133NEW},
	{PCI_PRODUCT_SIS_963, 0x00, 6, "963", SIS_TYPE_133NEW}
};

static struct sis_hostbr_type *sis_hostbr_type_match;

int
sis_hostbr_match(struct pci_attach_args *pa)
{
	int i;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_SIS)
		return (0);
	sis_hostbr_type_match = NULL;
	for (i = 0;
	    i < sizeof(sis_hostbr_type) / sizeof(sis_hostbr_type[0]);
	    i++) {
		if (PCI_PRODUCT(pa->pa_id) == sis_hostbr_type[i].id &&
		    PCI_REVISION(pa->pa_class) >= sis_hostbr_type[i].rev)
			sis_hostbr_type_match = &sis_hostbr_type[i];
	}
	return (sis_hostbr_type_match != NULL);
}

int
sis_south_match(struct pci_attach_args *pa)
{
	return(PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SIS &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SIS_85C503 &&
	    PCI_REVISION(pa->pa_class) >= 0x10);
}

void
sis_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	u_int8_t sis_ctr0 = pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_CTRL0);
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	int rev = sc->sc_rev;
	bus_size_t cmdsize, ctlsize;
	pcitag_t br_tag;
	struct pci_attach_args br_pa;

	if (pciide_chipen(sc, pa) == 0)
		return;

	/* Find PCI bridge (dev 0 func 0 on the same bus) */
	br_tag = pci_make_tag(pa->pa_pc, pa->pa_bus, 0, 0);
	br_pa.pa_id = pci_conf_read(sc->sc_pc, br_tag, PCI_ID_REG);
	br_pa.pa_class = pci_conf_read(sc->sc_pc, br_tag, PCI_CLASS_REG);
	WDCDEBUG_PRINT(("%s: PCI bridge pa_id=0x%x pa_class=0x%x\n",
	    __func__, br_pa.pa_id, br_pa.pa_class), DEBUG_PROBE);

	if (sis_hostbr_match(&br_pa)) {
		if (sis_hostbr_type_match->type == SIS_TYPE_SOUTH) {
			pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_REG_57,
			    pciide_pci_read(sc->sc_pc, sc->sc_tag,
			    SIS_REG_57) & 0x7f);
			if (sc->sc_pp->ide_product == SIS_PRODUCT_5518) {
				sc->sis_type = SIS_TYPE_133NEW;
				sc->sc_wdcdev.UDMA_cap =
				    sis_hostbr_type_match->udma_mode;
			} else {
				/* Find ISA bridge (func 0 of the same dev) */
				br_tag = pci_make_tag(pa->pa_pc, pa->pa_bus,
				    pa->pa_device, 0);
				br_pa.pa_id = pci_conf_read(sc->sc_pc,
				    br_tag, PCI_ID_REG);
				br_pa.pa_class = pci_conf_read(sc->sc_pc,
				    br_tag, PCI_CLASS_REG);
				WDCDEBUG_PRINT(("%s: ISA bridge "
				    "pa_id=0x%x pa_class=0x%x\n",
				    __func__, br_pa.pa_id, br_pa.pa_class),
				    DEBUG_PROBE);

				if (sis_south_match(&br_pa)) {
					sc->sis_type = SIS_TYPE_133OLD;
					sc->sc_wdcdev.UDMA_cap =
					    sis_hostbr_type_match->udma_mode;
				} else {
					sc->sis_type = SIS_TYPE_100NEW;
					sc->sc_wdcdev.UDMA_cap =
					    sis_hostbr_type_match->udma_mode;
				}
			}
		} else {
			sc->sis_type = sis_hostbr_type_match->type;
			sc->sc_wdcdev.UDMA_cap =
			    sis_hostbr_type_match->udma_mode;
		}
		printf(": %s", sis_hostbr_type_match->name);
	} else {
		printf(": 5597/5598");
		if (rev >= 0xd0) {
			sc->sc_wdcdev.UDMA_cap = 2;
			sc->sis_type = SIS_TYPE_66;
		} else {
			sc->sc_wdcdev.UDMA_cap = 0;
			sc->sis_type = SIS_TYPE_NOUDMA;
		}
	}

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		if (sc->sis_type >= SIS_TYPE_66)
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
	}

	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	switch (sc->sis_type) {
	case SIS_TYPE_NOUDMA:
	case SIS_TYPE_66:
	case SIS_TYPE_100OLD:
		sc->sc_wdcdev.set_modes = sis_setup_channel;
		pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_MISC,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_MISC) |
		    SIS_MISC_TIM_SEL | SIS_MISC_FIFO_SIZE | SIS_MISC_GTC);
		break;
	case SIS_TYPE_100NEW:
	case SIS_TYPE_133OLD:
		sc->sc_wdcdev.set_modes = sis_setup_channel;
		pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_REG_49,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_REG_49) | 0x01);
		break;
	case SIS_TYPE_133NEW:
		sc->sc_wdcdev.set_modes = sis96x_setup_channel;
		pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_REG_50,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_REG_50) & 0xf7);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_REG_52,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_REG_52) & 0xf7);
		break;
	}

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((channel == 0 && (sis_ctr0 & SIS_CTRL0_CHAN0_EN) == 0) ||
		    (channel == 1 && (sis_ctr0 & SIS_CTRL0_CHAN1_EN) == 0)) {
			printf("%s: %s ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}
		if (pciide_chan_candisable(cp)) {
			if (channel == 0)
				sis_ctr0 &= ~SIS_CTRL0_CHAN0_EN;
			else
				sis_ctr0 &= ~SIS_CTRL0_CHAN1_EN;
			pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_CTRL0,
			    sis_ctr0);
		}
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}
		sc->sc_wdcdev.set_modes(&cp->wdc_channel);
	}
}

void
sis96x_setup_channel(struct channel_softc *chp)
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t sis_tim;
	u_int32_t idedma_ctl;
	int regtim;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	sis_tim = 0;
	idedma_ctl = 0;
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		regtim = SIS_TIM133(
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_REG_57),
		    chp->channel, drive);
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			if (pciide_pci_read(sc->sc_pc, sc->sc_tag,
			    SIS96x_REG_CBL(chp->channel)) & SIS96x_REG_CBL_33) {
				if (drvp->UDMA_mode > 2)
					drvp->UDMA_mode = 2;
			}
			sis_tim |= sis_udma133new_tim[drvp->UDMA_mode];
			sis_tim |= sis_pio133new_tim[drvp->PIO_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & DRIVE_DMA) {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			sis_tim |= sis_dma133new_tim[drvp->DMA_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			sis_tim |= sis_pio133new_tim[drvp->PIO_mode];
		}
		WDCDEBUG_PRINT(("sis96x_setup_channel: new timings reg for "
		    "channel %d drive %d: 0x%x (reg 0x%x)\n",
		    chp->channel, drive, sis_tim, regtim), DEBUG_PROBE);
		pci_conf_write(sc->sc_pc, sc->sc_tag, regtim, sis_tim);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel), idedma_ctl);
	}
	pciide_print_modes(cp);
}

void
sis_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t sis_tim;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	WDCDEBUG_PRINT(("sis_setup_channel: old timings reg for "
	    "channel %d 0x%x\n", chp->channel,
	    pci_conf_read(sc->sc_pc, sc->sc_tag, SIS_TIM(chp->channel))),
	    DEBUG_PROBE);
	sis_tim = 0;
	idedma_ctl = 0;
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0)
			goto pio;

		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			if (pciide_pci_read(sc->sc_pc, sc->sc_tag,
			    SIS_REG_CBL) & SIS_REG_CBL_33(chp->channel)) {
				if (drvp->UDMA_mode > 2)
					drvp->UDMA_mode = 2;
			}
			switch (sc->sis_type) {
			case SIS_TYPE_66:
			case SIS_TYPE_100OLD:
				sis_tim |= sis_udma66_tim[drvp->UDMA_mode] <<
				    SIS_TIM66_UDMA_TIME_OFF(drive);
				break;
			case SIS_TYPE_100NEW:
				sis_tim |=
				    sis_udma100new_tim[drvp->UDMA_mode] <<
				    SIS_TIM100_UDMA_TIME_OFF(drive);
			case SIS_TYPE_133OLD:
				sis_tim |=
				    sis_udma133old_tim[drvp->UDMA_mode] <<
				    SIS_TIM100_UDMA_TIME_OFF(drive);
				break;
			default:
				printf("unknown SiS IDE type %d\n",
				    sc->sis_type);
			}
		} else {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			if (drvp->DMA_mode == 0)
				drvp->PIO_mode = 0;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
pio:		switch (sc->sis_type) {
		case SIS_TYPE_NOUDMA:
		case SIS_TYPE_66:
		case SIS_TYPE_100OLD:
			sis_tim |= sis_pio_act[drvp->PIO_mode] <<
			    SIS_TIM66_ACT_OFF(drive);
			sis_tim |= sis_pio_rec[drvp->PIO_mode] <<
			    SIS_TIM66_REC_OFF(drive);
			break;
		case SIS_TYPE_100NEW:
		case SIS_TYPE_133OLD:
			sis_tim |= sis_pio_act[drvp->PIO_mode] <<
			    SIS_TIM100_ACT_OFF(drive);
			sis_tim |= sis_pio_rec[drvp->PIO_mode] <<
			    SIS_TIM100_REC_OFF(drive);
			break;
		default:
			printf("unknown SiS IDE type %d\n",
			    sc->sis_type);
		}
	}
	WDCDEBUG_PRINT(("sis_setup_channel: new timings reg for "
	    "channel %d 0x%x\n", chp->channel, sis_tim), DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, SIS_TIM(chp->channel), sis_tim);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel), idedma_ctl);
	}
	pciide_print_modes(cp);
}

void
natsemi_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface, ctl;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16;

	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = natsemi_irqack;
	}

	pciide_pci_write(sc->sc_pc, sc->sc_tag, NATSEMI_CCBT, 0xb7);

	/*
	 * Mask off interrupts from both channels, appropriate channel(s)
	 * will be unmasked later.
	 */
	pciide_pci_write(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2) |
	    NATSEMI_CHMASK(0) | NATSEMI_CHMASK(1));

	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = natsemi_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	interface = PCI_INTERFACE(pci_conf_read(sc->sc_pc, sc->sc_tag,
	    PCI_CLASS_REG));
	interface &= ~PCIIDE_CHANSTATUS_EN;	/* Reserved on PC87415 */
	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	/* If we're in PCIIDE mode, unmask INTA, otherwise mask it. */
	ctl = pciide_pci_read(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL1);
	if (interface & (PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1)))
		ctl &= ~NATSEMI_CTRL1_INTAMASK;
	else
		ctl |= NATSEMI_CTRL1_INTAMASK;
	pciide_pci_write(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL1, ctl);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;

		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    natsemi_pci_intr);
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}
		natsemi_setup_channel(&cp->wdc_channel);
	}
}

void
natsemi_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive, ndrives = 0;
	u_int32_t idedma_ctl = 0;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	u_int8_t tim;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;

		ndrives++;
		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & DRIVE_DMA) == 0) {
			tim = natsemi_pio_pulse[drvp->PIO_mode] |
			    (natsemi_pio_recover[drvp->PIO_mode] << 4);
		} else {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode >= 3 &&
			    (drvp->DMA_mode + 2) > drvp->PIO_mode) {
				drvp->DMA_mode = drvp->PIO_mode - 2;
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			tim = natsemi_dma_pulse[drvp->DMA_mode] |
			    (natsemi_dma_recover[drvp->DMA_mode] << 4);
		}

		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    NATSEMI_RTREG(chp->channel, drive), tim);
		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    NATSEMI_WTREG(chp->channel, drive), tim);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel), idedma_ctl);
	}
	if (ndrives > 0) {
		/* Unmask the channel if at least one drive is found */
		pciide_pci_write(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2) &
		    ~(NATSEMI_CHMASK(chp->channel)));
	}

	pciide_print_modes(cp);

	/* Go ahead and ack interrupts generated during probe. */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL(chp->channel),
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CTL(chp->channel)));
}

void
natsemi_irqack(chp)
	struct channel_softc *chp;
{
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	u_int8_t clr;

	/* The "clear" bits are in the wrong register *sigh* */
	clr = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD(chp->channel));
	clr |= bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL(chp->channel)) &
	    (IDEDMA_CTL_ERR | IDEDMA_CTL_INTR);
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD(chp->channel), clr);
}

int
natsemi_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;
	u_int8_t ide_dmactl, msk;

	rv = 0;
	msk = pciide_pci_read(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2);
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;

		/* If a compat channel skip. */
		if (cp->compat)
			continue;

		/* If this channel is masked, skip it. */
		if (msk & NATSEMI_CHMASK(i))
			continue;

		/* Get intr status */
		ide_dmactl = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(i));

		if (ide_dmactl & IDEDMA_CTL_ERR)
			printf("%s:%d: error intr\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, i);

		if (ide_dmactl & IDEDMA_CTL_INTR) {
			crv = wdcintr(wdc_cp);
			if (crv == 0)
				printf("%s:%d: bogus intr\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, i);
			else
				rv = 1;
		}
	}
	return (rv);
}

void
ns_scx200_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.UDMA_cap = 2;

	sc->sc_wdcdev.set_modes = ns_scx200_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	/*
	 * Soekris net4801 errata 0003:
	 *
	 * The SC1100 built in busmaster IDE controller is pretty standard,
	 * but have two bugs: data transfers need to be dword aligned and
	 * it cannot do an exact 64Kbyte data transfer.
	 *
	 * Assume that reducing maximum segment size by one page
	 * will be enough, and restrict boundary too for extra certainty.
	 */
	if (sc->sc_pp->ide_product == PCI_PRODUCT_NS_SCx200_IDE) {
		sc->sc_dma_maxsegsz = IDEDMA_BYTE_COUNT_MAX - PAGE_SIZE;
		sc->sc_dma_boundary = IDEDMA_BYTE_COUNT_MAX - PAGE_SIZE;
	}

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}
		sc->sc_wdcdev.set_modes(&cp->wdc_channel);
	}
}

void
ns_scx200_setup_channel(struct channel_softc *chp)
{
	struct ata_drive_datas *drvp;
	int drive, mode;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;
	int pioformat;
	pcireg_t piotim, dmatim;

	/* Setup DMA if needed */
	pciide_channel_dma_setup(cp);

	pioformat = (pci_conf_read(sc->sc_pc, sc->sc_tag,
	    SCx200_TIM_DMA(0, 0)) >> SCx200_PIOFORMAT_SHIFT) & 0x01;
	WDCDEBUG_PRINT(("%s: pio format %d\n", __func__, pioformat),
	    DEBUG_PROBE);

	/* Per channel settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];

		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;

		piotim = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    SCx200_TIM_PIO(channel, drive));
		dmatim = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    SCx200_TIM_DMA(channel, drive));
		WDCDEBUG_PRINT(("%s:%d:%d: piotim=0x%x, dmatim=0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, channel, drive,
		    piotim, dmatim), DEBUG_PROBE);

		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) != 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) != 0) {
			/* Setup UltraDMA mode */
			drvp->drive_flags &= ~DRIVE_DMA;
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			dmatim = scx200_udma33[drvp->UDMA_mode];
			mode = drvp->PIO_mode;
		} else if ((chp->wdc->cap & WDC_CAPABILITY_DMA) != 0 &&
		    (drvp->drive_flags & DRIVE_DMA) != 0) {
			/* Setup multiword DMA mode */
			drvp->drive_flags &= ~DRIVE_UDMA;
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			dmatim = scx200_dma33[drvp->DMA_mode];

			/* mode = min(pio, dma + 2) */
			if (drvp->PIO_mode <= (drvp->DMA_mode + 2))
				mode = drvp->PIO_mode;
			else
				mode = drvp->DMA_mode + 2;
		} else {
			mode = drvp->PIO_mode;
		}

		/* Setup PIO mode */
		drvp->PIO_mode = mode;
		if (mode < 2)
			drvp->DMA_mode = 0;
		else
			drvp->DMA_mode = mode - 2;

		piotim = scx200_pio33[pioformat][drvp->PIO_mode];

		WDCDEBUG_PRINT(("%s:%d:%d: new piotim=0x%x, dmatim=0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, channel, drive,
		    piotim, dmatim), DEBUG_PROBE);

		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    SCx200_TIM_PIO(channel, drive), piotim);
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    SCx200_TIM_DMA(channel, drive), dmatim);
	}

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(channel), idedma_ctl);
	}

	pciide_print_modes(cp);
}

void
acer_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t cr, interface;
	bus_size_t cmdsize, ctlsize;
	int rev = sc->sc_rev;

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;

	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA;
		if (rev >= 0x20) {
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
			if (rev >= 0xC4)
				sc->sc_wdcdev.UDMA_cap = 5;
			else if (rev >= 0xC2)
				sc->sc_wdcdev.UDMA_cap = 4;
			else
				sc->sc_wdcdev.UDMA_cap = 2;
		}
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}

	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = acer_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_CDRC,
	    (pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CDRC) |
		ACER_CDRC_DMA_EN) & ~ACER_CDRC_FIFO_DISABLE);

	/* Enable "microsoft register bits" R/W. */
	pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_CCAR3,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CCAR3) | ACER_CCAR3_PI);
	pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_CCAR1,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CCAR1) &
	    ~(ACER_CHANSTATUS_RO|PCIIDE_CHAN_RO(0)|PCIIDE_CHAN_RO(1)));
	pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_CCAR2,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CCAR2) &
	    ~ACER_CHANSTATUSREGS_RO);
	cr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CLASS_REG);
	cr |= (PCIIDE_CHANSTATUS_EN << PCI_INTERFACE_SHIFT);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_CLASS_REG, cr);
	/* Don't use cr, re-read the real register content instead */
	interface = PCI_INTERFACE(pci_conf_read(sc->sc_pc, sc->sc_tag,
	    PCI_CLASS_REG));

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	/* From linux: enable "Cable Detection" */
	if (rev >= 0xC2)
		pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_0x4B,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_0x4B)
		    | ACER_0x4B_CDETECT);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((interface & PCIIDE_CHAN_EN(channel)) == 0) {
			printf("%s: %s ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    (rev >= 0xC2) ? pciide_pci_intr : acer_pci_intr);
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}
		if (pciide_chan_candisable(cp)) {
			cr &= ~(PCIIDE_CHAN_EN(channel) << PCI_INTERFACE_SHIFT);
			pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PCI_CLASS_REG, cr);
		}
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}
		acer_setup_channel(&cp->wdc_channel);
	}
}

void
acer_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t acer_fifo_udma;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	idedma_ctl = 0;
	acer_fifo_udma = pci_conf_read(sc->sc_pc, sc->sc_tag, ACER_FTH_UDMA);
	WDCDEBUG_PRINT(("acer_setup_channel: old fifo/udma reg 0x%x\n",
	    acer_fifo_udma), DEBUG_PROBE);
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	if ((chp->ch_drive[0].drive_flags | chp->ch_drive[1].drive_flags) &
	    DRIVE_UDMA)	{	/* check 80 pins cable */
		if (pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_0x4A) &
		    ACER_0x4A_80PIN(chp->channel)) {
			WDCDEBUG_PRINT(("%s:%d: 80-wire cable not detected\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, chp->channel),
			    DEBUG_PROBE);
			if (chp->ch_drive[0].UDMA_mode > 2)
				chp->ch_drive[0].UDMA_mode = 2;
			if (chp->ch_drive[1].UDMA_mode > 2)
				chp->ch_drive[1].UDMA_mode = 2;
		}
	}

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		WDCDEBUG_PRINT(("acer_setup_channel: old timings reg for "
		    "channel %d drive %d 0x%x\n", chp->channel, drive,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag,
		    ACER_IDETIM(chp->channel, drive))), DEBUG_PROBE);
		/* clear FIFO/DMA mode */
		acer_fifo_udma &= ~(ACER_FTH_OPL(chp->channel, drive, 0x3) |
		    ACER_UDMA_EN(chp->channel, drive) |
		    ACER_UDMA_TIM(chp->channel, drive, 0x7));

		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0) {
			acer_fifo_udma |=
			    ACER_FTH_OPL(chp->channel, drive, 0x1);
			goto pio;
		}

		acer_fifo_udma |= ACER_FTH_OPL(chp->channel, drive, 0x2);
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			acer_fifo_udma |= ACER_UDMA_EN(chp->channel, drive);
			acer_fifo_udma |=
			    ACER_UDMA_TIM(chp->channel, drive,
				acer_udma[drvp->UDMA_mode]);
			/* XXX disable if one drive < UDMA3 ? */
			if (drvp->UDMA_mode >= 3) {
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    ACER_0x4B,
				    pciide_pci_read(sc->sc_pc, sc->sc_tag,
				    ACER_0x4B) | ACER_0x4B_UDMA66);
			}
		} else {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			if (drvp->DMA_mode == 0)
				drvp->PIO_mode = 0;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
pio:		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    ACER_IDETIM(chp->channel, drive),
		    acer_pio[drvp->PIO_mode]);
	}
	WDCDEBUG_PRINT(("acer_setup_channel: new fifo/udma reg 0x%x\n",
	    acer_fifo_udma), DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, ACER_FTH_UDMA, acer_fifo_udma);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel), idedma_ctl);
	}
	pciide_print_modes(cp);
}

int
acer_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;
	u_int32_t chids;

	rv = 0;
	chids = pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CHIDS);
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		if (chids & ACER_CHIDS_INT(i)) {
			crv = wdcintr(wdc_cp);
			if (crv == 0)
				printf("%s:%d: bogus intr\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, i);
			else
				rv = 1;
		}
	}
	return rv;
}

void
hpt_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int i, compatchan, revision;
	pcireg_t interface;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;
	revision = sc->sc_rev;

	/*
	 * when the chip is in native mode it identifies itself as a
	 * 'misc mass storage'. Fake interface in this case.
	 */
	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCI_INTERFACE(pa->pa_class);
	} else {
		interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
		    PCIIDE_INTERFACE_PCI(0);
		if ((sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
		   (revision == HPT370_REV || revision == HPT370A_REV ||
		    revision == HPT372_REV)) ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT372A ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT302 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT371 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT374)
			interface |= PCIIDE_INTERFACE_PCI(1);
	}

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;

	sc->sc_wdcdev.set_modes = hpt_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	if (sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
	    revision == HPT366_REV) {
		sc->sc_wdcdev.UDMA_cap = 4;
		/*
		 * The 366 has 2 PCI IDE functions, one for primary and one
		 * for secondary. So we need to call pciide_mapregs_compat()
		 * with the real channel
		 */
		if (pa->pa_function == 0) {
			compatchan = 0;
		} else if (pa->pa_function == 1) {
			compatchan = 1;
		} else {
			printf("%s: unexpected PCI function %d\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, pa->pa_function);
			return;
		}
		sc->sc_wdcdev.nchannels = 1;
	} else {
		sc->sc_wdcdev.nchannels = 2;
		if (sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT372A ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT302 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT371 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT374)
			sc->sc_wdcdev.UDMA_cap = 6;
		else if (sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366) {
			if (revision == HPT372_REV)
				sc->sc_wdcdev.UDMA_cap = 6;
			else
				sc->sc_wdcdev.UDMA_cap = 5;
		}
	}
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		if (sc->sc_wdcdev.nchannels > 1) {
			compatchan = i;
			if((pciide_pci_read(sc->sc_pc, sc->sc_tag,
			    HPT370_CTRL1(i)) & HPT370_CTRL1_EN) == 0) {
				printf("%s: %s ignored (disabled)\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
				continue;
			}
		}
		if (pciide_chansetup(sc, i, interface) == 0)
			continue;
		if (interface & PCIIDE_INTERFACE_PCI(i)) {
			cp->hw_ok = pciide_mapregs_native(pa, cp, &cmdsize,
			    &ctlsize, hpt_pci_intr);
		} else {
			cp->hw_ok = pciide_mapregs_compat(pa, cp, compatchan,
			    &cmdsize, &ctlsize);
		}
		if (cp->hw_ok == 0)
			return;
		cp->wdc_channel.data32iot = cp->wdc_channel.cmd_iot;
		cp->wdc_channel.data32ioh = cp->wdc_channel.cmd_ioh;
		wdcattach(&cp->wdc_channel);
		hpt_setup_channel(&cp->wdc_channel);
	}
	if ((sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
	    (revision == HPT370_REV || revision == HPT370A_REV ||
	    revision == HPT372_REV)) ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT372A ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT302 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT371 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT374) {
		/*
		 * Turn off fast interrupts
		 */
		pciide_pci_write(sc->sc_pc, sc->sc_tag, HPT370_CTRL2(0),
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT370_CTRL2(0)) &
		    ~(HPT370_CTRL2_FASTIRQ | HPT370_CTRL2_HIRQ));
		pciide_pci_write(sc->sc_pc, sc->sc_tag, HPT370_CTRL2(1),
		pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT370_CTRL2(1)) &
		~(HPT370_CTRL2_FASTIRQ | HPT370_CTRL2_HIRQ));

		/*
		 * HPT370 and highter has a bit to disable interrupts,
		 * make sure to clear it
		 */
		pciide_pci_write(sc->sc_pc, sc->sc_tag, HPT_CSEL,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT_CSEL) &
		    ~HPT_CSEL_IRQDIS);
	}
	/* set clocks, etc (mandatory on 372/4, optional otherwise) */
	if (sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT372A ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT302 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT371 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT374 ||
	    (sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
	    revision == HPT372_REV))
		pciide_pci_write(sc->sc_pc, sc->sc_tag, HPT_SC2,
		    (pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT_SC2) &
		     HPT_SC2_MAEN) | HPT_SC2_OSC_EN);

	return;
}

void
hpt_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive;
	int cable;
	u_int32_t before, after;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int revision = sc->sc_rev;
	u_int32_t *tim_pio, *tim_dma, *tim_udma;

	cable = pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT_CSEL);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;

	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_TRIONES_HPT366:
		if (revision == HPT370_REV ||
		    revision == HPT370A_REV) {
			tim_pio = hpt370_pio;
			tim_dma = hpt370_dma;
			tim_udma = hpt370_udma;
		} else if (revision == HPT372_REV) {
			tim_pio = hpt372_pio;
			tim_dma = hpt372_dma;
			tim_udma = hpt372_udma;
		} else {
			tim_pio = hpt366_pio;
			tim_dma = hpt366_dma;
			tim_udma = hpt366_udma;
		}
		break;
	case PCI_PRODUCT_TRIONES_HPT372A:
	case PCI_PRODUCT_TRIONES_HPT302:
	case PCI_PRODUCT_TRIONES_HPT371:
		tim_pio = hpt372_pio;
		tim_dma = hpt372_dma;
		tim_udma = hpt372_udma;
		break;
	case PCI_PRODUCT_TRIONES_HPT374:
		tim_pio = hpt374_pio;
		tim_dma = hpt374_dma;
		tim_udma = hpt374_udma;
		break;
	default:
		printf("%s: no known timing values\n",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		goto end;
	}

	/* Per drive settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		before = pci_conf_read(sc->sc_pc, sc->sc_tag,
				       HPT_IDETIM(chp->channel, drive));

		/* add timing values, setup DMA if needed */
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			if ((cable & HPT_CSEL_CBLID(chp->channel)) != 0 &&
			    drvp->UDMA_mode > 2) {
				WDCDEBUG_PRINT(("%s(%s:%d:%d): 80-wire "
				    "cable not detected\n", drvp->drive_name,
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    chp->channel, drive), DEBUG_PROBE);
				drvp->UDMA_mode = 2;
			}
			after = tim_udma[drvp->UDMA_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & DRIVE_DMA) {
			/*
			 * use Multiword DMA.
			 * Timings will be used for both PIO and DMA, so adjust
			 * DMA mode if needed
			 */
			if (drvp->PIO_mode >= 3 &&
			    (drvp->DMA_mode + 2) > drvp->PIO_mode) {
				drvp->DMA_mode = drvp->PIO_mode - 2;
			}
			after = tim_dma[drvp->DMA_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			/* PIO only */
			after = tim_pio[drvp->PIO_mode];
		}
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    HPT_IDETIM(chp->channel, drive), after);
		WDCDEBUG_PRINT(("%s: bus speed register set to 0x%08x "
		    "(BIOS 0x%08x)\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    after, before), DEBUG_PROBE);
	}
end:
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(chp->channel), idedma_ctl);
	}
	pciide_print_modes(cp);
}

int
hpt_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int rv = 0;
	int dmastat, i, crv;

	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		dmastat = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(i));
		if((dmastat & (IDEDMA_CTL_ACT | IDEDMA_CTL_INTR)) !=
		    IDEDMA_CTL_INTR)
		    continue;
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		crv = wdcintr(wdc_cp);
		if (crv == 0) {
			printf("%s:%d: bogus intr\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, i);
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL(i), dmastat);
		} else
			rv = 1;
	}
	return rv;
}

/* Macros to test product */
#define PDC_IS_262(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20262 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20265  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20267)
#define PDC_IS_265(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20265 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20267  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20268  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20268R ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20269  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20271  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20275  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20276  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20277  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20376)
#define PDC_IS_268(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20268 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20268R ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20269  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20271  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20275  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20276  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20277  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20376)
#define PDC_IS_269(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20269 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20271  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20275  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20276  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20277  ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20376)

static __inline u_int8_t
pdc268_config_read(struct channel_softc *chp, int index)
{
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;

	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    PDC268_INDEX(channel), index);
	return (bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    PDC268_DATA(channel)));
}

static __inline void
pdc268_config_write(struct channel_softc *chp, int index, u_int8_t value)
{
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;

	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    PDC268_INDEX(channel), index);
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    PDC268_DATA(channel), value);
}

void
pdc202xx_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface, st, mode;
	bus_size_t cmdsize, ctlsize;

	if (!PDC_IS_268(sc)) {
		st = pci_conf_read(sc->sc_pc, sc->sc_tag, PDC2xx_STATE);
		WDCDEBUG_PRINT(("pdc202xx_setup_chip: controller state 0x%x\n",
		    st), DEBUG_PROBE);
	}
	if (pciide_chipen(sc, pa) == 0)
		return;

	/* turn off  RAID mode */
	if (!PDC_IS_268(sc))
		st &= ~PDC2xx_STATE_IDERAID;

	/*
 	 * can't rely on the PCI_CLASS_REG content if the chip was in raid
	 * mode. We have to fake interface
	 */
	interface = PCIIDE_INTERFACE_SETTABLE(0) | PCIIDE_INTERFACE_SETTABLE(1);
	if (PDC_IS_268(sc) || (st & PDC2xx_STATE_NATIVE))
		interface |= PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20246 ||
	    PDC_IS_262(sc))
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_NO_ATAPI_DMA;
	if (sc->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20376)
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_SATA;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	if (PDC_IS_269(sc))
		sc->sc_wdcdev.UDMA_cap = 6;
	else if (PDC_IS_265(sc))
		sc->sc_wdcdev.UDMA_cap = 5;
	else if (PDC_IS_262(sc))
		sc->sc_wdcdev.UDMA_cap = 4;
	else
		sc->sc_wdcdev.UDMA_cap = 2;
	sc->sc_wdcdev.set_modes = PDC_IS_268(sc) ?
			pdc20268_setup_channel : pdc202xx_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	if (PDC_IS_262(sc)) {
		sc->sc_wdcdev.dma_start = pdc20262_dma_start;
		sc->sc_wdcdev.dma_finish = pdc20262_dma_finish;
	}

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);
	if (!PDC_IS_268(sc)) {
		/* setup failsafe defaults */
		mode = 0;
		mode = PDC2xx_TIM_SET_PA(mode, pdc2xx_pa[0]);
		mode = PDC2xx_TIM_SET_PB(mode, pdc2xx_pb[0]);
		mode = PDC2xx_TIM_SET_MB(mode, pdc2xx_dma_mb[0]);
		mode = PDC2xx_TIM_SET_MC(mode, pdc2xx_dma_mc[0]);
		for (channel = 0;
		     channel < sc->sc_wdcdev.nchannels;
		     channel++) {
			WDCDEBUG_PRINT(("pdc202xx_setup_chip: channel %d "
			    "drive 0 initial timings  0x%x, now 0x%x\n",
			    channel, pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PDC2xx_TIM(channel, 0)), mode | PDC2xx_TIM_IORDYp),
			    DEBUG_PROBE);
			pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PDC2xx_TIM(channel, 0), mode | PDC2xx_TIM_IORDYp);
			WDCDEBUG_PRINT(("pdc202xx_setup_chip: channel %d "
			    "drive 1 initial timings  0x%x, now 0x%x\n",
			    channel, pci_conf_read(sc->sc_pc, sc->sc_tag,
	 		    PDC2xx_TIM(channel, 1)), mode), DEBUG_PROBE);
			pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PDC2xx_TIM(channel, 1), mode);
		}

		mode = PDC2xx_SCR_DMA;
		if (PDC_IS_262(sc)) {
			mode = PDC2xx_SCR_SET_GEN(mode, PDC262_SCR_GEN_LAT);
		} else {
			/* the BIOS set it up this way */
			mode = PDC2xx_SCR_SET_GEN(mode, 0x1);
		}
		mode = PDC2xx_SCR_SET_I2C(mode, 0x3); /* ditto */
		mode = PDC2xx_SCR_SET_POLL(mode, 0x1); /* ditto */
		WDCDEBUG_PRINT(("pdc202xx_setup_chip: initial SCR  0x%x, "
		    "now 0x%x\n",
		    bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh,
			PDC2xx_SCR),
		    mode), DEBUG_PROBE);
		bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC2xx_SCR, mode);

		/* controller initial state register is OK even without BIOS */
		/* Set DMA mode to IDE DMA compatibility */
		mode =
		    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_PM);
		WDCDEBUG_PRINT(("pdc202xx_setup_chip: primary mode 0x%x", mode),
		    DEBUG_PROBE);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_PM,
		    mode | 0x1);
		mode =
		    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SM);
		WDCDEBUG_PRINT((", secondary mode 0x%x\n", mode ), DEBUG_PROBE);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SM,
		    mode | 0x1);
	}

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if (!PDC_IS_268(sc) && (st & (PDC_IS_262(sc) ?
		    PDC262_STATE_EN(channel):PDC246_STATE_EN(channel))) == 0) {
			printf("%s: %s ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		if (PDC_IS_265(sc))
			pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
			    pdc20265_pci_intr);
		else
			pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
			    pdc202xx_pci_intr);
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}
		if (!PDC_IS_268(sc) && pciide_chan_candisable(cp)) {
			st &= ~(PDC_IS_262(sc) ?
			    PDC262_STATE_EN(channel):PDC246_STATE_EN(channel));
			pciide_unmap_compat_intr(pa, cp, channel, interface);
		}
		if (PDC_IS_268(sc))
			pdc20268_setup_channel(&cp->wdc_channel);
		else
			pdc202xx_setup_channel(&cp->wdc_channel);
	}
	if (!PDC_IS_268(sc)) {
		WDCDEBUG_PRINT(("pdc202xx_setup_chip: new controller state "
		    "0x%x\n", st), DEBUG_PROBE);
		pci_conf_write(sc->sc_pc, sc->sc_tag, PDC2xx_STATE, st);
	}
	return;
}

void
pdc202xx_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive;
	pcireg_t mode, st;
	u_int32_t idedma_ctl, scr, atapi;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;
	WDCDEBUG_PRINT(("pdc202xx_setup_channel %s: scr 0x%x\n",
	    sc->sc_wdcdev.sc_dev.dv_xname,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC262_U66)),
	    DEBUG_PROBE);

	/* Per channel settings */
	if (PDC_IS_262(sc)) {
		scr = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66);
		st = pci_conf_read(sc->sc_pc, sc->sc_tag, PDC2xx_STATE);
		/* Check cable */
		if ((st & PDC262_STATE_80P(channel)) != 0 &&
		    ((chp->ch_drive[0].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[0].UDMA_mode > 2) ||
		    (chp->ch_drive[1].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[1].UDMA_mode > 2))) {
			WDCDEBUG_PRINT(("%s:%d: 80-wire cable not detected\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, channel),
			    DEBUG_PROBE);
			if (chp->ch_drive[0].UDMA_mode > 2)
				chp->ch_drive[0].UDMA_mode = 2;
			if (chp->ch_drive[1].UDMA_mode > 2)
				chp->ch_drive[1].UDMA_mode = 2;
		}
		/* Trim UDMA mode */
		if ((chp->ch_drive[0].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[0].UDMA_mode <= 2) ||
		    (chp->ch_drive[1].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[1].UDMA_mode <= 2)) {
			if (chp->ch_drive[0].UDMA_mode > 2)
				chp->ch_drive[0].UDMA_mode = 2;
			if (chp->ch_drive[1].UDMA_mode > 2)
				chp->ch_drive[1].UDMA_mode = 2;
		}
		/* Set U66 if needed */
		if ((chp->ch_drive[0].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[0].UDMA_mode > 2) ||
		    (chp->ch_drive[1].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[1].UDMA_mode > 2))
			scr |= PDC262_U66_EN(channel);
		else
			scr &= ~PDC262_U66_EN(channel);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66, scr);
		WDCDEBUG_PRINT(("pdc202xx_setup_channel %s:%d: ATAPI 0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, channel,
		    bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_ATAPI(channel))), DEBUG_PROBE);
		if (chp->ch_drive[0].drive_flags & DRIVE_ATAPI ||
		    chp->ch_drive[1].drive_flags & DRIVE_ATAPI) {
			if (((chp->ch_drive[0].drive_flags & DRIVE_UDMA) &&
			    !(chp->ch_drive[1].drive_flags & DRIVE_UDMA) &&
			    (chp->ch_drive[1].drive_flags & DRIVE_DMA)) ||
			    ((chp->ch_drive[1].drive_flags & DRIVE_UDMA) &&
			    !(chp->ch_drive[0].drive_flags & DRIVE_UDMA) &&
			    (chp->ch_drive[0].drive_flags & DRIVE_DMA)))
				atapi = 0;
			else
				atapi = PDC262_ATAPI_UDMA;
			bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
			    PDC262_ATAPI(channel), atapi);
		}
	}
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		mode = 0;
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			mode = PDC2xx_TIM_SET_MB(mode,
			   pdc2xx_udma_mb[drvp->UDMA_mode]);
			mode = PDC2xx_TIM_SET_MC(mode,
			   pdc2xx_udma_mc[drvp->UDMA_mode]);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & DRIVE_DMA) {
			mode = PDC2xx_TIM_SET_MB(mode,
			    pdc2xx_dma_mb[drvp->DMA_mode]);
			mode = PDC2xx_TIM_SET_MC(mode,
			   pdc2xx_dma_mc[drvp->DMA_mode]);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			mode = PDC2xx_TIM_SET_MB(mode,
			    pdc2xx_dma_mb[0]);
			mode = PDC2xx_TIM_SET_MC(mode,
			    pdc2xx_dma_mc[0]);
		}
		mode = PDC2xx_TIM_SET_PA(mode, pdc2xx_pa[drvp->PIO_mode]);
		mode = PDC2xx_TIM_SET_PB(mode, pdc2xx_pb[drvp->PIO_mode]);
		if (drvp->drive_flags & DRIVE_ATA)
			mode |= PDC2xx_TIM_PRE;
		mode |= PDC2xx_TIM_SYNC | PDC2xx_TIM_ERRDY;
		if (drvp->PIO_mode >= 3) {
			mode |= PDC2xx_TIM_IORDY;
			if (drive == 0)
				mode |= PDC2xx_TIM_IORDYp;
		}
		WDCDEBUG_PRINT(("pdc202xx_setup_channel: %s:%d:%d "
		    "timings 0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname,
		    chp->channel, drive, mode), DEBUG_PROBE);
		    pci_conf_write(sc->sc_pc, sc->sc_tag,
		    PDC2xx_TIM(chp->channel, drive), mode);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(channel), idedma_ctl);
	}
	pciide_print_modes(cp);
}

void
pdc20268_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive, cable;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;

	/* check 80 pins cable */
	cable = pdc268_config_read(chp, 0x0b) & PDC268_CABLE;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			if (cable && drvp->UDMA_mode > 2) {
				WDCDEBUG_PRINT(("%s(%s:%d:%d): 80-wire "
				    "cable not detected\n", drvp->drive_name,
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    channel, drive), DEBUG_PROBE);
				drvp->UDMA_mode = 2;
			}
		} else if (drvp->drive_flags & DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
	}
	/* nothing to do to setup modes, the controller snoop SET_FEATURE cmd */
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(channel), idedma_ctl);
	}
	pciide_print_modes(cp);
}

int
pdc202xx_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;
	u_int32_t scr;

	rv = 0;
	scr = bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SCR);
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		if (scr & PDC2xx_SCR_INT(i)) {
			crv = wdcintr(wdc_cp);
			if (crv == 0)
				printf("%s:%d: bogus intr (reg 0x%x)\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, i, scr);
			else
				rv = 1;
		}
	}
	return rv;
}

int
pdc20265_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;
	u_int32_t dmastat;

	rv = 0;
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;

		/*
		 * In case of shared IRQ check that the interrupt
		 * was actually generated by this channel.
		 * Only check the channel that is enabled.
		 */
		if (cp->hw_ok && PDC_IS_268(sc)) {
			if ((pdc268_config_read(wdc_cp,
			    0x0b) & PDC268_INTR) == 0)
				continue;
		}

		/*
		 * The Ultra/100 seems to assert PDC2xx_SCR_INT * spuriously,
		 * however it asserts INT in IDEDMA_CTL even for non-DMA ops.
		 * So use it instead (requires 2 reg reads instead of 1,
		 * but we can't do it another way).
		 */
		dmastat = bus_space_read_1(sc->sc_dma_iot,
		    sc->sc_dma_ioh, IDEDMA_CTL(i));
		if ((dmastat & IDEDMA_CTL_INTR) == 0)
			continue;

		crv = wdcintr(wdc_cp);
		if (crv == 0)
			printf("%s:%d: bogus intr\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, i);
		else
			rv = 1;
	}
	return rv;
}

void
pdc20262_dma_start(void *v, int channel, int drive)
{
	struct pciide_softc *sc = v;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];
	u_int8_t clock;
	u_int32_t count;

	if (dma_maps->dma_flags & WDC_DMA_LBA48) {
		clock = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66, clock | PDC262_U66_EN(channel));
		count = dma_maps->dmamap_xfer->dm_mapsize >> 1;
		count |= dma_maps->dma_flags & WDC_DMA_READ ?
		    PDC262_ATAPI_LBA48_READ : PDC262_ATAPI_LBA48_WRITE;
		bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_ATAPI(channel), count);
	}

	pciide_dma_start(v, channel, drive);
}

int
pdc20262_dma_finish(void *v, int channel, int drive, int force)
{
	struct pciide_softc *sc = v;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];
 	u_int8_t clock;

	if (dma_maps->dma_flags & WDC_DMA_LBA48) {
		clock = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66, clock & ~PDC262_U66_EN(channel));
		bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_ATAPI(channel), 0);
	}

	return (pciide_dma_finish(v, channel, drive, force));
}

#ifdef notyet
/*
 * Inline functions for accessing the timing registers of the
 * OPTi controller.
 *
 * These *MUST* disable interrupts as they need atomic access to
 * certain magic registers. Failure to adhere to this *will*
 * break things in subtle ways if the wdc registers are accessed
 * by an interrupt routine while this magic sequence is executing.
 */
static __inline__ u_int8_t
opti_read_config(struct channel_softc *chp, int reg)
{
	u_int8_t rv;
	int s = splhigh();

	/* Two consecutive 16-bit reads from register #1 (0x1f1/0x171) */
	(void) bus_space_read_2(chp->cmd_iot, chp->cmd_ioh, wdr_features);
	(void) bus_space_read_2(chp->cmd_iot, chp->cmd_ioh, wdr_features);

	/* Followed by an 8-bit write of 0x3 to register #2 */
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wdr_seccnt, 0x03u);

	/* Now we can read the required register */
	rv = bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, reg);

	/* Restore the real registers */
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wdr_seccnt, 0x83u);

	splx(s);

	return rv;
}

static __inline__ void
opti_write_config(struct channel_softc *chp, int reg, u_int8_t val)
{
	int s = splhigh();

	/* Two consecutive 16-bit reads from register #1 (0x1f1/0x171) */
	(void) bus_space_read_2(chp->cmd_iot, chp->cmd_ioh, wdr_features);
	(void) bus_space_read_2(chp->cmd_iot, chp->cmd_ioh, wdr_features);

	/* Followed by an 8-bit write of 0x3 to register #2 */
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wdr_seccnt, 0x03u);

	/* Now we can write the required register */
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, reg, val);

	/* Restore the real registers */
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wdr_seccnt, 0x83u);

	splx(s);
}

void
opti_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	bus_size_t cmdsize, ctlsize;
	pcireg_t interface;
	u_int8_t init_ctrl;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;
	printf(": DMA");
	/*
	 * XXXSCW:
	 * There seem to be a couple of buggy revisions/implementations
	 * of the OPTi pciide chipset. This kludge seems to fix one of
	 * the reported problems (NetBSD PR/11644) but still fails for the
	 * other (NetBSD PR/13151), although the latter may be due to other
	 * issues too...
	 */
	if (sc->sc_rev <= 0x12) {
		printf(" (disabled)");
		sc->sc_dma_ok = 0;
		sc->sc_wdcdev.cap = 0;
	} else {
		sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA32;
		pciide_mapreg_dma(sc, pa);
	}

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_MODE;
	sc->sc_wdcdev.PIO_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.DMA_cap = 2;
	}
	sc->sc_wdcdev.set_modes = opti_setup_channel;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	init_ctrl = pciide_pci_read(sc->sc_pc, sc->sc_tag,
	    OPTI_REG_INIT_CONTROL);

	interface = PCI_INTERFACE(pa->pa_class);

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if (channel == 1 &&
		    (init_ctrl & OPTI_INIT_CONTROL_CH2_DISABLE) != 0) {
			printf("%s: %s ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}
		opti_setup_channel(&cp->wdc_channel);
	}
}

void
opti_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int drive,spd;
	int mode[2];
	u_int8_t rv, mr;

	/*
	 * The `Delay' and `Address Setup Time' fields of the
	 * Miscellaneous Register are always zero initially.
	 */
	mr = opti_read_config(chp, OPTI_REG_MISC) & ~OPTI_MISC_INDEX_MASK;
	mr &= ~(OPTI_MISC_DELAY_MASK |
		OPTI_MISC_ADDR_SETUP_MASK |
		OPTI_MISC_INDEX_MASK);

	/* Prime the control register before setting timing values */
	opti_write_config(chp, OPTI_REG_CONTROL, OPTI_CONTROL_DISABLE);

	/* Determine the clockrate of the PCIbus the chip is attached to */
	spd = (int) opti_read_config(chp, OPTI_REG_STRAP);
	spd &= OPTI_STRAP_PCI_SPEED_MASK;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0) {
			mode[drive] = -1;
			continue;
		}

		if ((drvp->drive_flags & DRIVE_DMA)) {
			/*
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			if (drvp->DMA_mode == 0)
				drvp->PIO_mode = 0;

			mode[drive] = drvp->DMA_mode + 5;
		} else
			mode[drive] = drvp->PIO_mode;

		if (drive && mode[0] >= 0 &&
		    (opti_tim_as[spd][mode[0]] != opti_tim_as[spd][mode[1]])) {
			/*
			 * Can't have two drives using different values
			 * for `Address Setup Time'.
			 * Slow down the faster drive to compensate.
			 */
			int d = (opti_tim_as[spd][mode[0]] >
				 opti_tim_as[spd][mode[1]]) ?  0 : 1;

			mode[d] = mode[1-d];
			chp->ch_drive[d].PIO_mode = chp->ch_drive[1-d].PIO_mode;
			chp->ch_drive[d].DMA_mode = 0;
			chp->ch_drive[d].drive_flags &= DRIVE_DMA;
		}
	}

	for (drive = 0; drive < 2; drive++) {
		int m;
		if ((m = mode[drive]) < 0)
			continue;

		/* Set the Address Setup Time and select appropriate index */
		rv = opti_tim_as[spd][m] << OPTI_MISC_ADDR_SETUP_SHIFT;
		rv |= OPTI_MISC_INDEX(drive);
		opti_write_config(chp, OPTI_REG_MISC, mr | rv);

		/* Set the pulse width and recovery timing parameters */
		rv  = opti_tim_cp[spd][m] << OPTI_PULSE_WIDTH_SHIFT;
		rv |= opti_tim_rt[spd][m] << OPTI_RECOVERY_TIME_SHIFT;
		opti_write_config(chp, OPTI_REG_READ_CYCLE_TIMING, rv);
		opti_write_config(chp, OPTI_REG_WRITE_CYCLE_TIMING, rv);

		/* Set the Enhanced Mode register appropriately */
	    	rv = pciide_pci_read(sc->sc_pc, sc->sc_tag, OPTI_REG_ENH_MODE);
		rv &= ~OPTI_ENH_MODE_MASK(chp->channel, drive);
		rv |= OPTI_ENH_MODE(chp->channel, drive, opti_tim_em[m]);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, OPTI_REG_ENH_MODE, rv);
	}

	/* Finally, enable the timings */
	opti_write_config(chp, OPTI_REG_CONTROL, OPTI_CONTROL_ENABLE);

	pciide_print_modes(cp);
}
#endif

void
serverworks_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	pcitag_t pcib_tag;
	int channel;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;

	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_RCC_OSB4_IDE:
		sc->sc_wdcdev.UDMA_cap = 2;
		break;
	case PCI_PRODUCT_RCC_CSB5_IDE:
		if (sc->sc_rev < 0x92)
			sc->sc_wdcdev.UDMA_cap = 4;
		else
			sc->sc_wdcdev.UDMA_cap = 5;
		break;
	case PCI_PRODUCT_RCC_CSB6_IDE:
		sc->sc_wdcdev.UDMA_cap = 5;
		break;
	case PCI_PRODUCT_RCC_CSB6_IDE2:
		sc->sc_wdcdev.UDMA_cap = 4;
		break;
	}

	sc->sc_wdcdev.set_modes = serverworks_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels =
	    (sc->sc_pp->ide_product == PCI_PRODUCT_RCC_CSB6_IDE2 ? 1 : 2);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    serverworks_pci_intr);
		if (cp->hw_ok == 0)
			return;
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			return;
		serverworks_setup_channel(&cp->wdc_channel);
	}

	pcib_tag = pci_make_tag(pa->pa_pc, pa->pa_bus, pa->pa_device, 0);
	pci_conf_write(pa->pa_pc, pcib_tag, 0x64,
	    (pci_conf_read(pa->pa_pc, pcib_tag, 0x64) & ~0x2000) | 0x4000);
}

void
serverworks_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;
	int drive, unit;
	u_int32_t pio_time, dma_time, pio_mode, udma_mode;
	u_int32_t idedma_ctl;
	static const u_int8_t pio_modes[5] = {0x5d, 0x47, 0x34, 0x22, 0x20};
	static const u_int8_t dma_modes[3] = {0x77, 0x21, 0x20};

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	pio_time = pci_conf_read(sc->sc_pc, sc->sc_tag, 0x40);
	dma_time = pci_conf_read(sc->sc_pc, sc->sc_tag, 0x44);
	pio_mode = pci_conf_read(sc->sc_pc, sc->sc_tag, 0x48);
	udma_mode = pci_conf_read(sc->sc_pc, sc->sc_tag, 0x54);

	pio_time &= ~(0xffff << (16 * channel));
	dma_time &= ~(0xffff << (16 * channel));
	pio_mode &= ~(0xff << (8 * channel + 16));
	udma_mode &= ~(0xff << (8 * channel + 16));
	udma_mode &= ~(3 << (2 * channel));

	idedma_ctl = 0;

	/* Per drive settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		unit = drive + 2 * channel;
		/* add timing values, setup DMA if needed */
		pio_time |= pio_modes[drvp->PIO_mode] << (8 * (unit^1));
		pio_mode |= drvp->PIO_mode << (4 * unit + 16);
		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA, check for 80-pin cable */
			if (drvp->UDMA_mode > 2 &&
			    (PCI_PRODUCT(pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PCI_SUBSYS_ID_REG)) &
			    (1 << (14 + channel))) == 0) {
				WDCDEBUG_PRINT(("%s(%s:%d:%d): 80-wire "
				    "cable not detected\n", drvp->drive_name,
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    channel, drive), DEBUG_PROBE);
				drvp->UDMA_mode = 2;
			}
			dma_time |= dma_modes[drvp->DMA_mode] << (8 * (unit^1));
			udma_mode |= drvp->UDMA_mode << (4 * unit + 16);
			udma_mode |= 1 << unit;
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if ((chp->wdc->cap & WDC_CAPABILITY_DMA) &&
		    (drvp->drive_flags & DRIVE_DMA)) {
			/* use Multiword DMA */
			drvp->drive_flags &= ~DRIVE_UDMA;
			dma_time |= dma_modes[drvp->DMA_mode] << (8 * (unit^1));
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			/* PIO only */
			drvp->drive_flags &= ~(DRIVE_UDMA | DRIVE_DMA);
		}
	}

	pci_conf_write(sc->sc_pc, sc->sc_tag, 0x40, pio_time);
	pci_conf_write(sc->sc_pc, sc->sc_tag, 0x44, dma_time);
	if (sc->sc_pp->ide_product != PCI_PRODUCT_RCC_OSB4_IDE)
		pci_conf_write(sc->sc_pc, sc->sc_tag, 0x48, pio_mode);
	pci_conf_write(sc->sc_pc, sc->sc_tag, 0x54, udma_mode);

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(channel), idedma_ctl);
	}
	pciide_print_modes(cp);
}

int
serverworks_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int rv = 0;
	int dmastat, i, crv;

	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		dmastat = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(i));
		if ((dmastat & (IDEDMA_CTL_ACT | IDEDMA_CTL_INTR)) !=
		    IDEDMA_CTL_INTR)
			continue;
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		crv = wdcintr(wdc_cp);
		if (crv == 0) {
			printf("%s:%d: bogus intr\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, i);
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL(i), dmastat);
		} else
			rv = 1;
	}
	return rv;
}


#define	ACARD_IS_850(sc) \
	((sc)->sc_pp->ide_product == PCI_PRODUCT_ACARD_ATP850U)

void
acard_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int i;
	pcireg_t interface;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;

	/*
	 * when the chip is in native mode it identifies itself as a
	 * 'misc mass storage'. Fake interface in this case.
	 */
	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCI_INTERFACE(pa->pa_class);
	} else {
		interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
		    PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);
	}

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;

	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_ACARD_ATP850U:
		sc->sc_wdcdev.UDMA_cap = 2;
		break;
	case PCI_PRODUCT_ACARD_ATP860:
	case PCI_PRODUCT_ACARD_ATP860A:
		sc->sc_wdcdev.UDMA_cap = 4;
		break;
	case PCI_PRODUCT_ACARD_ATP865A:
	case PCI_PRODUCT_ACARD_ATP865R:
		sc->sc_wdcdev.UDMA_cap = 6;
		break;
	}

	sc->sc_wdcdev.set_modes = acard_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = 2;

	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		if (pciide_chansetup(sc, i, interface) == 0)
			continue;
		if (interface & PCIIDE_INTERFACE_PCI(i)) {
			cp->hw_ok = pciide_mapregs_native(pa, cp, &cmdsize,
			    &ctlsize, pciide_pci_intr);
		} else {
			cp->hw_ok = pciide_mapregs_compat(pa, cp, i,
			    &cmdsize, &ctlsize);
		}
		if (cp->hw_ok == 0)
			return;
		cp->wdc_channel.data32iot = cp->wdc_channel.cmd_iot;
		cp->wdc_channel.data32ioh = cp->wdc_channel.cmd_ioh;
		wdcattach(&cp->wdc_channel);
		acard_setup_channel(&cp->wdc_channel);
	}
	if (!ACARD_IS_850(sc)) {
		u_int32_t reg;
		reg = pci_conf_read(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL);
		reg &= ~ATP860_CTRL_INT;
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL, reg);
	}
}

void
acard_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;
	int drive;
	u_int32_t idetime, udma_mode;
	u_int32_t idedma_ctl;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	if (ACARD_IS_850(sc)) {
		idetime = 0;
		udma_mode = pci_conf_read(sc->sc_pc, sc->sc_tag, ATP850_UDMA);
		udma_mode &= ~ATP850_UDMA_MASK(channel);
	} else {
		idetime = pci_conf_read(sc->sc_pc, sc->sc_tag, ATP860_IDETIME);
		idetime &= ~ATP860_SETTIME_MASK(channel);
		udma_mode = pci_conf_read(sc->sc_pc, sc->sc_tag, ATP860_UDMA);
		udma_mode &= ~ATP860_UDMA_MASK(channel);
	}

	idedma_ctl = 0;

	/* Per drive settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA */
			if (ACARD_IS_850(sc)) {
				idetime |= ATP850_SETTIME(drive,
				    acard_act_udma[drvp->UDMA_mode],
				    acard_rec_udma[drvp->UDMA_mode]);
				udma_mode |= ATP850_UDMA_MODE(channel, drive,
				    acard_udma_conf[drvp->UDMA_mode]);
			} else {
				idetime |= ATP860_SETTIME(channel, drive,
				    acard_act_udma[drvp->UDMA_mode],
				    acard_rec_udma[drvp->UDMA_mode]);
				udma_mode |= ATP860_UDMA_MODE(channel, drive,
				    acard_udma_conf[drvp->UDMA_mode]);
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if ((chp->wdc->cap & WDC_CAPABILITY_DMA) &&
		    (drvp->drive_flags & DRIVE_DMA)) {
			/* use Multiword DMA */
			drvp->drive_flags &= ~DRIVE_UDMA;
			if (ACARD_IS_850(sc)) {
				idetime |= ATP850_SETTIME(drive,
				    acard_act_dma[drvp->DMA_mode],
				    acard_rec_dma[drvp->DMA_mode]);
			} else {
				idetime |= ATP860_SETTIME(channel, drive,
				    acard_act_dma[drvp->DMA_mode],
				    acard_rec_dma[drvp->DMA_mode]);
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			/* PIO only */
			drvp->drive_flags &= ~(DRIVE_UDMA | DRIVE_DMA);
			if (ACARD_IS_850(sc)) {
				idetime |= ATP850_SETTIME(drive,
				    acard_act_pio[drvp->PIO_mode],
				    acard_rec_pio[drvp->PIO_mode]);
			} else {
				idetime |= ATP860_SETTIME(channel, drive,
				    acard_act_pio[drvp->PIO_mode],
				    acard_rec_pio[drvp->PIO_mode]);
			}
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL,
		    pci_conf_read(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL)
		    | ATP8x0_CTRL_EN(channel));
		}
	}

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(channel), idedma_ctl);
	}
	pciide_print_modes(cp);

	if (ACARD_IS_850(sc)) {
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    ATP850_IDETIME(channel), idetime);
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP850_UDMA, udma_mode);
	} else {
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP860_IDETIME, idetime);
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP860_UDMA, udma_mode);
	}
}

int
acard_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int rv = 0;
	int dmastat, i, crv;

	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		dmastat = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(i));
		if ((dmastat & IDEDMA_CTL_INTR) == 0)
			continue;
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		if ((wdc_cp->ch_flags & WDCF_IRQ_WAIT) == 0) {
			(void)wdcintr(wdc_cp);
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL(i), dmastat);
			continue;
		}
		crv = wdcintr(wdc_cp);
		if (crv == 0)
			printf("%s:%d: bogus intr\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, i);
		else if (crv == 1)
			rv = 1;
		else if (rv == 0)
			rv = crv;
	}
	return rv;
}

void
nforce_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	bus_size_t cmdsize, ctlsize;
	u_int32_t conf;

	conf = pci_conf_read(sc->sc_pc, sc->sc_tag, NFORCE_CONF);
	WDCDEBUG_PRINT(("%s: conf register 0x%x\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, conf), DEBUG_PROBE);

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_NVIDIA_NFORCE_IDE:
		sc->sc_wdcdev.UDMA_cap = 5;
		break;
	case PCI_PRODUCT_NVIDIA_NFORCE2_IDE:
		sc->sc_wdcdev.UDMA_cap = 6;
		break;
	default:
		sc->sc_wdcdev.UDMA_cap = 0;
	}
	sc->sc_wdcdev.set_modes = nforce_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];

		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		if ((conf & NFORCE_CHAN_EN(channel)) == 0) {
			printf("%s: %s ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}

		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    nforce_pci_intr);
		if (cp->hw_ok == 0) {
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}

		if (pciide_chan_candisable(cp)) {
			conf &= ~NFORCE_CHAN_EN(channel);
			pciide_unmap_compat_intr(pa, cp, channel, interface);
			continue;
		}

		sc->sc_wdcdev.set_modes(&cp->wdc_channel);
	}
	WDCDEBUG_PRINT(("%s: new conf register 0x%x\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, conf), DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, NFORCE_CONF, conf);
}

void
nforce_setup_channel(struct channel_softc *chp)
{
	struct ata_drive_datas *drvp;
	int drive, mode;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;
	u_int32_t conf, piodmatim, piotim, udmatim;

	conf = pci_conf_read(sc->sc_pc, sc->sc_tag, NFORCE_CONF);
	piodmatim = pci_conf_read(sc->sc_pc, sc->sc_tag, NFORCE_PIODMATIM);
	piotim = pci_conf_read(sc->sc_pc, sc->sc_tag, NFORCE_PIOTIM);
	udmatim = pci_conf_read(sc->sc_pc, sc->sc_tag, NFORCE_UDMATIM);
	WDCDEBUG_PRINT(("%s: %s old timing values: piodmatim=0x%x, "
	    "piotim=0x%x, udmatim=0x%x\n", sc->sc_wdcdev.sc_dev.dv_xname,
	    cp->name, piodmatim, piotim, udmatim), DEBUG_PROBE);

	/* Setup DMA if needed */
	pciide_channel_dma_setup(cp);

	/* Clear all bits for this channel */
	idedma_ctl = 0;
	piodmatim &= ~NFORCE_PIODMATIM_MASK(channel);
	udmatim &= ~NFORCE_UDMATIM_MASK(channel);

	/* Per channel settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];

		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;

		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) != 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) != 0) {
			/* Setup UltraDMA mode */
			drvp->drive_flags &= ~DRIVE_DMA;

			udmatim |= NFORCE_UDMATIM_SET(channel, drive,
			    nforce_udma[drvp->UDMA_mode]) |
			    NFORCE_UDMA_EN(channel, drive) |
			    NFORCE_UDMA_ENM(channel, drive);

			mode = drvp->PIO_mode;
		} else if ((chp->wdc->cap & WDC_CAPABILITY_DMA) != 0 &&
		    (drvp->drive_flags & DRIVE_DMA) != 0) {
			/* Setup multiword DMA mode */
			drvp->drive_flags &= ~DRIVE_UDMA;

			/* mode = min(pio, dma + 2) */
			if (drvp->PIO_mode <= (drvp->DMA_mode + 2))
				mode = drvp->PIO_mode;
			else
				mode = drvp->DMA_mode + 2;
		} else {
			goto pio;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:
		/* Setup PIO mode */
		if (mode <= 2) {
			drvp->DMA_mode = 0;
			drvp->PIO_mode = 0;
			mode = 0;
		} else {
			drvp->PIO_mode = mode;
			drvp->DMA_mode = mode - 2;
		}
		piodmatim |= NFORCE_PIODMATIM_SET(channel, drive,
		    nforce_pio[mode]);
	}

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(channel), idedma_ctl);
	}

	WDCDEBUG_PRINT(("%s: %s new timing values: piodmatim=0x%x, "
	    "piotim=0x%x, udmatim=0x%x\n", sc->sc_wdcdev.sc_dev.dv_xname,
	    cp->name, piodmatim, piotim, udmatim), DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, NFORCE_PIODMATIM, piodmatim);
	pci_conf_write(sc->sc_pc, sc->sc_tag, NFORCE_UDMATIM, udmatim);

	pciide_print_modes(cp);
}

int
nforce_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;
	u_int32_t dmastat;

	rv = 0;
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;

		/* Skip compat channel */
		if (cp->compat)
			continue;

		dmastat = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(i));
		if ((dmastat & IDEDMA_CTL_INTR) == 0)
			continue;

		crv = wdcintr(wdc_cp);
		if (crv == 0)
			printf("%s:%d: bogus intr\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, i);
		else
			rv = 1;
	}
	return rv;
}

#ifdef notyet
void
artisea_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	bus_size_t cmdsize, ctlsize;
	pcireg_t interface;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf("%s: DMA",
	    sc->sc_wdcdev.sc_dev.dv_xname);
#ifndef PCIIDE_I31244_ENABLEDMA
	if (sc->sc_rev == 0) {
		printf(" disabled due to rev. 0");
		sc->sc_dma_ok = 0;
	} else
#endif
		pciide_mapreg_dma(sc, pa);
	printf("\n");

	/*
	 * XXX Configure LEDs to show activity.
	 */

	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE | WDC_CAPABILITY_SATA;
	sc->sc_wdcdev.PIO_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.DMA_cap = 2;
		sc->sc_wdcdev.UDMA_cap = 6;
	}
	sc->sc_wdcdev.set_modes = sata_setup_channel;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	interface = PCI_INTERFACE(pa->pa_class);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0)
			continue;
		pciide_map_compat_intr(pa, cp, channel, interface);
		sata_setup_channel(&cp->wdc_channel);
	}
}
#endif

void
ite_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface;
	bus_size_t cmdsize, ctlsize;
	pcireg_t cfg, modectl;

	/*
	 * Fake interface since IT8212F is claimed to be a ``RAID'' device.
	 */
	interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
	    PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);

	cfg = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_CFG);
	modectl = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_MODE);
	WDCDEBUG_PRINT(("%s: cfg=0x%x, modectl=0x%x\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, cfg & IT_CFG_MASK,
	    modectl & IT_MODE_MASK), DEBUG_PROBE);

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf(": DMA");
	pciide_mapreg_dma(sc, pa);

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.UDMA_cap = 6;

	sc->sc_wdcdev.set_modes = ite_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);

	/* Disable RAID */
	modectl &= ~IT_MODE_RAID1;
	/* Disable CPU firmware mode */
	modectl &= ~IT_MODE_CPU;

	pci_conf_write(sc->sc_pc, sc->sc_tag, IT_MODE, modectl);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];

		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		sc->sc_wdcdev.set_modes(&cp->wdc_channel);
	}

	/* Re-read configuration registers after channels setup */
	cfg = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_CFG);
	modectl = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_MODE);
	WDCDEBUG_PRINT(("%s: cfg=0x%x, modectl=0x%x\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, cfg & IT_CFG_MASK,
	    modectl & IT_MODE_MASK), DEBUG_PROBE);
}

void
ite_setup_channel(struct channel_softc *chp)
{
	struct ata_drive_datas *drvp;
	int drive, mode;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel *)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;
	pcireg_t cfg, modectl;
	pcireg_t tim;

	cfg = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_CFG);
	modectl = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_MODE);
	tim = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_TIM(channel));
	WDCDEBUG_PRINT(("%s:%d: tim=0x%x\n", sc->sc_wdcdev.sc_dev.dv_xname,
	    channel, tim), DEBUG_PROBE);

	/* Setup DMA if needed */
	pciide_channel_dma_setup(cp);

	/* Clear all bits for this channel */
	idedma_ctl = 0;

	/* Per channel settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];

		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;

		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) != 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) != 0) {
			/* Setup UltraDMA mode */
			drvp->drive_flags &= ~DRIVE_DMA;
			modectl &= ~IT_MODE_DMA(channel, drive);

#if 0
			/* Check cable, works only in CPU firmware mode */
			if (drvp->UDMA_mode > 2 &&
			    (cfg & IT_CFG_CABLE(channel, drive)) == 0) {
				WDCDEBUG_PRINT(("%s(%s:%d:%d): "
				    "80-wire cable not detected\n",
				    drvp->drive_name,
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    channel, drive), DEBUG_PROBE);
				drvp->UDMA_mode = 2;
			}
#endif

			if (drvp->UDMA_mode >= 5)
				tim |= IT_TIM_UDMA5(drive);
			else
				tim &= ~IT_TIM_UDMA5(drive);

			mode = drvp->PIO_mode;
		} else if ((chp->wdc->cap & WDC_CAPABILITY_DMA) != 0 &&
		    (drvp->drive_flags & DRIVE_DMA) != 0) {
			/* Setup multiword DMA mode */
			drvp->drive_flags &= ~DRIVE_UDMA;
			modectl |= IT_MODE_DMA(channel, drive);

			/* mode = min(pio, dma + 2) */
			if (drvp->PIO_mode <= (drvp->DMA_mode + 2))
				mode = drvp->PIO_mode;
			else
				mode = drvp->DMA_mode + 2;
		} else {
			goto pio;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:
		/* Setup PIO mode */
		if (mode <= 2) {
			drvp->DMA_mode = 0;
			drvp->PIO_mode = 0;
			mode = 0;
		} else {
			drvp->PIO_mode = mode;
			drvp->DMA_mode = mode - 2;
		}

		/* Enable IORDY if PIO mode >= 3 */
		if (drvp->PIO_mode >= 3)
			cfg |= IT_CFG_IORDY(channel);
	}

	WDCDEBUG_PRINT(("%s: tim=0x%x\n", sc->sc_wdcdev.sc_dev.dv_xname,
	    tim), DEBUG_PROBE);

	pci_conf_write(sc->sc_pc, sc->sc_tag, IT_CFG, cfg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, IT_MODE, modectl);
	pci_conf_write(sc->sc_pc, sc->sc_tag, IT_TIM(channel), tim);

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL(channel), idedma_ctl);
	}

	pciide_print_modes(cp);
}
