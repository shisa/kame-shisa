/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/pci/if_rl.c,v 1.126 2003/11/28 05:28:29 imp Exp $");

/*
 * RealTek 8129/8139 PCI NIC driver
 *
 * Supports several extremely cheap PCI 10/100 adapters based on
 * the RealTek chipset. Datasheets can be obtained from
 * www.realtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */
/*
 * The RealTek 8139 PCI NIC redefines the meaning of 'low end.' This is
 * probably the worst PCI ethernet controller ever made, with the possible
 * exception of the FEAST chip made by SMC. The 8139 supports bus-master
 * DMA, but it has a terrible interface that nullifies any performance
 * gains that bus-master DMA usually offers.
 *
 * For transmission, the chip offers a series of four TX descriptor
 * registers. Each transmit frame must be in a contiguous buffer, aligned
 * on a longword (32-bit) boundary. This means we almost always have to
 * do mbuf copies in order to transmit a frame, except in the unlikely
 * case where a) the packet fits into a single mbuf, and b) the packet
 * is 32-bit aligned within the mbuf's data area. The presence of only
 * four descriptor registers means that we can never have more than four
 * packets queued for transmission at any one time.
 *
 * Reception is not much better. The driver has to allocate a single large
 * buffer area (up to 64K in size) into which the chip will DMA received
 * frames. Because we don't know where within this region received packets
 * will begin or end, we have no choice but to copy data from the buffer
 * area into mbufs in order to pass the packets up to the higher protocol
 * levels.
 *
 * It's impossible given this rotten design to really achieve decent
 * performance at 100Mbps, unless you happen to have a 400Mhz PII or
 * some equally overmuscled CPU to drive it.
 *
 * On the bright side, the 8139 does have a built-in PHY, although
 * rather than using an MDIO serial interface like most other NICs, the
 * PHY registers are directly accessible through the 8139's register
 * space. The 8139 supports autonegotiation, as well as a 64-bit multicast
 * filter.
 *
 * The 8129 chip is an older version of the 8139 that uses an external PHY
 * chip. The 8129 has a serial MDIO interface for accessing the MII where
 * the 8139 lets you directly access the on-board PHY registers. We need
 * to select which interface to use depending on the chip type.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

MODULE_DEPEND(rl, pci, 1, 1, 1);
MODULE_DEPEND(rl, ether, 1, 1, 1);
MODULE_DEPEND(rl, miibus, 1, 1, 1);

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

/*
 * Default to using PIO access for this driver. On SMP systems,
 * there appear to be problems with memory mapped mode: it looks like
 * doing too many memory mapped access back to back in rapid succession
 * can hang the bus. I'm inclined to blame this on crummy design/construction
 * on the part of RealTek. Memory mapped mode does appear to work on
 * uniprocessor systems though.
 */
#define RL_USEIOSPACE

#include <pci/if_rlreg.h>

/*
 * Various supported device vendors/types and their names.
 */
static struct rl_type rl_devs[] = {
	{ RT_VENDORID, RT_DEVICEID_8129, RL_8129,
		"RealTek 8129 10/100BaseTX" },
	{ RT_VENDORID, RT_DEVICEID_8139, RL_8139,
		"RealTek 8139 10/100BaseTX" },
	{ RT_VENDORID, RT_DEVICEID_8138, RL_8139,
		"RealTek 8139 10/100BaseTX CardBus" },
	{ RT_VENDORID, RT_DEVICEID_8100, RL_8139,
		"RealTek 8100 10/100BaseTX" },
	{ ACCTON_VENDORID, ACCTON_DEVICEID_5030, RL_8139,
		"Accton MPX 5030/5038 10/100BaseTX" },
	{ DELTA_VENDORID, DELTA_DEVICEID_8139, RL_8139,
		"Delta Electronics 8139 10/100BaseTX" },
	{ ADDTRON_VENDORID, ADDTRON_DEVICEID_8139, RL_8139,
		"Addtron Technolgy 8139 10/100BaseTX" },
	{ DLINK_VENDORID, DLINK_DEVICEID_530TXPLUS, RL_8139,
		"D-Link DFE-530TX+ 10/100BaseTX" },
	{ DLINK_VENDORID, DLINK_DEVICEID_690TXD, RL_8139,
		"D-Link DFE-690TXD 10/100BaseTX" },
	{ NORTEL_VENDORID, ACCTON_DEVICEID_5030, RL_8139,
		"Nortel Networks 10/100BaseTX" },
	{ COREGA_VENDORID, COREGA_DEVICEID_FETHERCBTXD, RL_8139,
		"Corega FEther CB-TXD" },
	{ COREGA_VENDORID, COREGA_DEVICEID_FETHERIICBTXD, RL_8139,
		"Corega FEtherII CB-TXD" },
	{ PEPPERCON_VENDORID, PEPPERCON_DEVICEID_ROLF, RL_8139,
		"Peppercon AG ROL-F" },
	{ PLANEX_VENDORID, PLANEX_DEVICEID_FNW3800TX, RL_8139,
		"Planex FNW-3800-TX" },
	{ CP_VENDORID, RT_DEVICEID_8139, RL_8139,
		"Compaq HNE-300" },
	{ LEVEL1_VENDORID, LEVEL1_DEVICEID_FPC0106TX, RL_8139,
		"LevelOne FPC-0106TX" },
	{ EDIMAX_VENDORID, EDIMAX_DEVICEID_EP4103DL, RL_8139,
		"Edimax EP-4103DL CardBus" },
	{ 0, 0, NULL }
};

static int rl_probe		(device_t);
static int rl_attach		(device_t);
static int rl_detach		(device_t);

static int rl_encap		(struct rl_softc *, struct mbuf * );

static void rl_rxeof		(struct rl_softc *);
static void rl_txeof		(struct rl_softc *);
static void rl_intr		(void *);
static void rl_tick		(void *);
static void rl_start		(struct ifnet *);
static int rl_ioctl		(struct ifnet *, u_long, caddr_t);
static void rl_init		(void *);
static void rl_stop		(struct rl_softc *);
static void rl_watchdog		(struct ifnet *);
static int rl_suspend		(device_t);
static int rl_resume		(device_t);
static void rl_shutdown		(device_t);
static int rl_ifmedia_upd	(struct ifnet *);
static void rl_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static void rl_eeprom_putbyte	(struct rl_softc *, int);
static void rl_eeprom_getword	(struct rl_softc *, int, u_int16_t *);
static void rl_read_eeprom	(struct rl_softc *, caddr_t, int, int, int);
static void rl_mii_sync		(struct rl_softc *);
static void rl_mii_send		(struct rl_softc *, u_int32_t, int);
static int rl_mii_readreg	(struct rl_softc *, struct rl_mii_frame *);
static int rl_mii_writereg	(struct rl_softc *, struct rl_mii_frame *);

static int rl_miibus_readreg	(device_t, int, int);
static int rl_miibus_writereg	(device_t, int, int, int);
static void rl_miibus_statchg	(device_t);

static u_int32_t rl_mchash	(caddr_t);
static void rl_setmulti		(struct rl_softc *);
static void rl_reset		(struct rl_softc *);
static int rl_list_tx_init	(struct rl_softc *);

static void rl_dma_map_rxbuf	(void *, bus_dma_segment_t *, int, int);
static void rl_dma_map_txbuf	(void *, bus_dma_segment_t *, int, int);

#ifdef RL_USEIOSPACE
#define RL_RES			SYS_RES_IOPORT
#define RL_RID			RL_PCI_LOIO
#else
#define RL_RES			SYS_RES_MEMORY
#define RL_RID			RL_PCI_LOMEM
#endif

static device_method_t rl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rl_probe),
	DEVMETHOD(device_attach,	rl_attach),
	DEVMETHOD(device_detach,	rl_detach),
	DEVMETHOD(device_suspend,	rl_suspend),
	DEVMETHOD(device_resume,	rl_resume),
	DEVMETHOD(device_shutdown,	rl_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	rl_miibus_readreg),
	DEVMETHOD(miibus_writereg,	rl_miibus_writereg),
	DEVMETHOD(miibus_statchg,	rl_miibus_statchg),

	{ 0, 0 }
};

static driver_t rl_driver = {
	"rl",
	rl_methods,
	sizeof(struct rl_softc)
};

static devclass_t rl_devclass;

DRIVER_MODULE(rl, pci, rl_driver, rl_devclass, 0, 0);
DRIVER_MODULE(rl, cardbus, rl_driver, rl_devclass, 0, 0);
DRIVER_MODULE(miibus, rl, miibus_driver, miibus_devclass, 0, 0);

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) & ~x)

static void
rl_dma_map_rxbuf(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg, error;
{
	struct rl_softc *sc;

	sc = arg;
	CSR_WRITE_4(sc, RL_RXADDR, segs->ds_addr & 0xFFFFFFFF);

	return;
}

static void
rl_dma_map_txbuf(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg, error;
{
	struct rl_softc *sc;

	sc = arg;
	CSR_WRITE_4(sc, RL_CUR_TXADDR(sc), segs->ds_addr & 0xFFFFFFFF);

	return;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void
rl_eeprom_putbyte(sc, addr)
	struct rl_softc		*sc;
	int			addr;
{
	register int		d, i;

	d = addr | sc->rl_eecmd_read;

	/*
	 * Feed in each bit and strobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			EE_SET(RL_EE_DATAIN);
		} else {
			EE_CLR(RL_EE_DATAIN);
		}
		DELAY(100);
		EE_SET(RL_EE_CLK);
		DELAY(150);
		EE_CLR(RL_EE_CLK);
		DELAY(100);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
rl_eeprom_getword(sc, addr, dest)
	struct rl_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Enter EEPROM access mode. */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_PROGRAM|RL_EE_SEL);

	/*
	 * Send address of word we want to read.
	 */
	rl_eeprom_putbyte(sc, addr);

	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_PROGRAM|RL_EE_SEL);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		EE_SET(RL_EE_CLK);
		DELAY(100);
		if (CSR_READ_1(sc, RL_EECMD) & RL_EE_DATAOUT)
			word |= i;
		EE_CLR(RL_EE_CLK);
		DELAY(100);
	}

	/* Turn off EEPROM access mode. */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void
rl_read_eeprom(sc, dest, off, cnt, swap)
	struct rl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		rl_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}


/*
 * MII access routines are provided for the 8129, which
 * doesn't have a built-in PHY. For the 8139, we fake things
 * up by diverting rl_phy_readreg()/rl_phy_writereg() to the
 * direct access PHY registers.
 */
#define MII_SET(x)					\
	CSR_WRITE_1(sc, RL_MII,				\
		CSR_READ_1(sc, RL_MII) | (x))

#define MII_CLR(x)					\
	CSR_WRITE_1(sc, RL_MII,				\
		CSR_READ_1(sc, RL_MII) & ~(x))

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
rl_mii_sync(sc)
	struct rl_softc		*sc;
{
	register int		i;

	MII_SET(RL_MII_DIR|RL_MII_DATAOUT);

	for (i = 0; i < 32; i++) {
		MII_SET(RL_MII_CLK);
		DELAY(1);
		MII_CLR(RL_MII_CLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void
rl_mii_send(sc, bits, cnt)
	struct rl_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	MII_CLR(RL_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
		if (bits & i) {
			MII_SET(RL_MII_DATAOUT);
		} else {
			MII_CLR(RL_MII_DATAOUT);
		}
		DELAY(1);
		MII_CLR(RL_MII_CLK);
		DELAY(1);
		MII_SET(RL_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int
rl_mii_readreg(sc, frame)
	struct rl_softc		*sc;
	struct rl_mii_frame	*frame;

{
	int			i, ack;

	RL_LOCK(sc);

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = RL_MII_STARTDELIM;
	frame->mii_opcode = RL_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;

	CSR_WRITE_2(sc, RL_MII, 0);

	/*
	 * Turn on data xmit.
	 */
	MII_SET(RL_MII_DIR);

	rl_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	rl_mii_send(sc, frame->mii_stdelim, 2);
	rl_mii_send(sc, frame->mii_opcode, 2);
	rl_mii_send(sc, frame->mii_phyaddr, 5);
	rl_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	MII_CLR((RL_MII_CLK|RL_MII_DATAOUT));
	DELAY(1);
	MII_SET(RL_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	MII_CLR(RL_MII_DIR);

	/* Check for ack */
	MII_CLR(RL_MII_CLK);
	DELAY(1);
	ack = CSR_READ_2(sc, RL_MII) & RL_MII_DATAIN;
	MII_SET(RL_MII_CLK);
	DELAY(1);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			MII_CLR(RL_MII_CLK);
			DELAY(1);
			MII_SET(RL_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		MII_CLR(RL_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_2(sc, RL_MII) & RL_MII_DATAIN)
				frame->mii_data |= i;
			DELAY(1);
		}
		MII_SET(RL_MII_CLK);
		DELAY(1);
	}

fail:

	MII_CLR(RL_MII_CLK);
	DELAY(1);
	MII_SET(RL_MII_CLK);
	DELAY(1);

	RL_UNLOCK(sc);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int
rl_mii_writereg(sc, frame)
	struct rl_softc		*sc;
	struct rl_mii_frame	*frame;

{
	RL_LOCK(sc);

	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = RL_MII_STARTDELIM;
	frame->mii_opcode = RL_MII_WRITEOP;
	frame->mii_turnaround = RL_MII_TURNAROUND;

	/*
	 * Turn on data output.
	 */
	MII_SET(RL_MII_DIR);

	rl_mii_sync(sc);

	rl_mii_send(sc, frame->mii_stdelim, 2);
	rl_mii_send(sc, frame->mii_opcode, 2);
	rl_mii_send(sc, frame->mii_phyaddr, 5);
	rl_mii_send(sc, frame->mii_regaddr, 5);
	rl_mii_send(sc, frame->mii_turnaround, 2);
	rl_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	MII_SET(RL_MII_CLK);
	DELAY(1);
	MII_CLR(RL_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	MII_CLR(RL_MII_DIR);

	RL_UNLOCK(sc);

	return(0);
}

static int
rl_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct rl_softc		*sc;
	struct rl_mii_frame	frame;
	u_int16_t		rval = 0;
	u_int16_t		rl8139_reg = 0;

	sc = device_get_softc(dev);
	RL_LOCK(sc);

	if (sc->rl_type == RL_8139) {
		/* Pretend the internal PHY is only at address 0 */
		if (phy) {
			RL_UNLOCK(sc);
			return(0);
		}
		switch(reg) {
		case MII_BMCR:
			rl8139_reg = RL_BMCR;
			break;
		case MII_BMSR:
			rl8139_reg = RL_BMSR;
			break;
		case MII_ANAR:
			rl8139_reg = RL_ANAR;
			break;
		case MII_ANER:
			rl8139_reg = RL_ANER;
			break;
		case MII_ANLPAR:
			rl8139_reg = RL_LPAR;
			break;
		case MII_PHYIDR1:
		case MII_PHYIDR2:
			RL_UNLOCK(sc);
			return(0);
		/*
		 * Allow the rlphy driver to read the media status
		 * register. If we have a link partner which does not
		 * support NWAY, this is the register which will tell
		 * us the results of parallel detection.
		 */
		case RL_MEDIASTAT:
			rval = CSR_READ_1(sc, RL_MEDIASTAT);
			RL_UNLOCK(sc);
			return(rval);
		default:
			printf("rl%d: bad phy register\n", sc->rl_unit);
			RL_UNLOCK(sc);
			return(0);
		}
		rval = CSR_READ_2(sc, rl8139_reg);
		RL_UNLOCK(sc);
		return(rval);
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	rl_mii_readreg(sc, &frame);
	RL_UNLOCK(sc);

	return(frame.mii_data);
}

static int
rl_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct rl_softc		*sc;
	struct rl_mii_frame	frame;
	u_int16_t		rl8139_reg = 0;

	sc = device_get_softc(dev);
	RL_LOCK(sc);

	if (sc->rl_type == RL_8139) {
		/* Pretend the internal PHY is only at address 0 */
		if (phy) {
			RL_UNLOCK(sc);
			return(0);
		}
		switch(reg) {
		case MII_BMCR:
			rl8139_reg = RL_BMCR;
			break;
		case MII_BMSR:
			rl8139_reg = RL_BMSR;
			break;
		case MII_ANAR:
			rl8139_reg = RL_ANAR;
			break;
		case MII_ANER:
			rl8139_reg = RL_ANER;
			break;
		case MII_ANLPAR:
			rl8139_reg = RL_LPAR;
			break;
		case MII_PHYIDR1:
		case MII_PHYIDR2:
			RL_UNLOCK(sc);
			return(0);
			break;
		default:
			printf("rl%d: bad phy register\n", sc->rl_unit);
			RL_UNLOCK(sc);
			return(0);
		}
		CSR_WRITE_2(sc, rl8139_reg, data);
		RL_UNLOCK(sc);
		return(0);
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	rl_mii_writereg(sc, &frame);

	RL_UNLOCK(sc);
	return(0);
}

static void
rl_miibus_statchg(dev)
	device_t		dev;
{
	return;
}

/*
 * Calculate CRC of a multicast group address, return the upper 6 bits.
 */
static u_int32_t
rl_mchash(addr)
	caddr_t		addr;
{
	u_int32_t	crc, carry;
	int		idx, bit;
	u_int8_t	data;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>=1 ) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (data & 0x01);
			crc <<= 1;
			if (carry)
				crc = (crc ^ 0x04c11db6) | carry;
		}
	}

	/* return the filter bit position */
	return(crc >> 26);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void
rl_setmulti(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int32_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->arpcom.ac_if;

	rxfilt = CSR_READ_4(sc, RL_RXCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= RL_RXCFG_RX_MULTI;
		CSR_WRITE_4(sc, RL_RXCFG, rxfilt);
		CSR_WRITE_4(sc, RL_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, RL_MAR4, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, RL_MAR0, 0);
	CSR_WRITE_4(sc, RL_MAR4, 0);

	/* now program new ones */
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = rl_mchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}

	if (mcnt)
		rxfilt |= RL_RXCFG_RX_MULTI;
	else
		rxfilt &= ~RL_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, RL_RXCFG, rxfilt);
	CSR_WRITE_4(sc, RL_MAR0, hashes[0]);
	CSR_WRITE_4(sc, RL_MAR4, hashes[1]);

	return;
}

static void
rl_reset(sc)
	struct rl_softc		*sc;
{
	register int		i;

	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_RESET);

	for (i = 0; i < RL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_1(sc, RL_COMMAND) & RL_CMD_RESET))
			break;
	}
	if (i == RL_TIMEOUT)
		printf("rl%d: reset never completed!\n", sc->rl_unit);

	return;
}

/*
 * Probe for a RealTek 8129/8139 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
rl_probe(dev)
	device_t		dev;
{
	struct rl_type		*t;
        struct rl_softc		*sc;
	int			rid;
	u_int32_t		hwrev;

	t = rl_devs;
	sc = device_get_softc(dev);

	while(t->rl_name != NULL) {
		if ((pci_get_vendor(dev) == t->rl_vid) &&
		    (pci_get_device(dev) == t->rl_did)) {

			/*
			 * Temporarily map the I/O space
			 * so we can read the chip ID register.
			 */
			rid = RL_RID;
			sc->rl_res = bus_alloc_resource(dev, RL_RES, &rid,
			    0, ~0, 1, RF_ACTIVE);
			if (sc->rl_res == NULL) {
				device_printf(dev,
				    "couldn't map ports/memory\n");
				return(ENXIO);
			}
			sc->rl_btag = rman_get_bustag(sc->rl_res);
			sc->rl_bhandle = rman_get_bushandle(sc->rl_res);
			mtx_init(&sc->rl_mtx,
			    device_get_nameunit(dev),
			    MTX_NETWORK_LOCK, MTX_DEF);
                        RL_LOCK(sc);
			hwrev = CSR_READ_4(sc, RL_TXCFG) & RL_TXCFG_HWREV;
			bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);
			RL_UNLOCK(sc);
			mtx_destroy(&sc->rl_mtx);

			/* Don't attach to 8139C+ or 8169/8110 chips. */
			if (hwrev == RL_HWREV_8139CPLUS ||
			    hwrev == RL_HWREV_8169 ||
			    hwrev == RL_HWREV_8169S ||
			    hwrev == RL_HWREV_8110S) {
				t++;
				continue;
			}

			device_set_desc(dev, t->rl_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
rl_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int16_t		as[3];
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		rl_did = 0;
	struct rl_type		*t;
	int			unit, error = 0, rid, i;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	mtx_init(&sc->rl_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
#ifndef BURN_BRIDGES
	/*
	 * Handle power management nonsense.
	 */

	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		u_int32_t		iobase, membase, irq;

		/* Save important PCI config data. */
		iobase = pci_read_config(dev, RL_PCI_LOIO, 4);
		membase = pci_read_config(dev, RL_PCI_LOMEM, 4);
		irq = pci_read_config(dev, RL_PCI_INTLINE, 4);

		/* Reset the power state. */
		printf("rl%d: chip is is in D%d power mode "
		    "-- setting to D0\n", unit,
		    pci_get_powerstate(dev));

		pci_set_powerstate(dev, PCI_POWERSTATE_D0);

		/* Restore PCI config data. */
		pci_write_config(dev, RL_PCI_LOIO, iobase, 4);
		pci_write_config(dev, RL_PCI_LOMEM, membase, 4);
		pci_write_config(dev, RL_PCI_INTLINE, irq, 4);
	}
#endif
	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = RL_RID;
	sc->rl_res = bus_alloc_resource(dev, RL_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->rl_res == NULL) {
		printf ("rl%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

#ifdef notdef
	/* Detect the Realtek 8139B. For some reason, this chip is very
	 * unstable when left to autoselect the media
	 * The best workaround is to set the device to the required
	 * media type or to set it to the 10 Meg speed.
	 */

	if ((rman_get_end(sc->rl_res)-rman_get_start(sc->rl_res))==0xff) {
		printf("rl%d: Realtek 8139B detected. Warning, "
		    "this may be unstable in autoselect mode\n", unit);
	}
#endif

	sc->rl_btag = rman_get_bustag(sc->rl_res);
	sc->rl_bhandle = rman_get_bushandle(sc->rl_res);

	/* Allocate interrupt */
	rid = 0;
	sc->rl_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->rl_irq == NULL) {
		printf("rl%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	/* Reset the adapter. */
	rl_reset(sc);
	sc->rl_eecmd_read = RL_EECMD_READ_6BIT;
	rl_read_eeprom(sc, (caddr_t)&rl_did, 0, 1, 0);
	if (rl_did != 0x8129)
		sc->rl_eecmd_read = RL_EECMD_READ_8BIT;

	/*
	 * Get station address from the EEPROM.
	 */
	rl_read_eeprom(sc, (caddr_t)as, RL_EE_EADDR, 3, 0);
	for (i = 0; i < 3; i++) {
		eaddr[(i * 2) + 0] = as[i] & 0xff;
		eaddr[(i * 2) + 1] = as[i] >> 8;
	}

	/*
	 * A RealTek chip was detected. Inform the world.
	 */
	printf("rl%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->rl_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	/*
	 * Now read the exact device type from the EEPROM to find
	 * out if it's an 8129 or 8139.
	 */
	rl_read_eeprom(sc, (caddr_t)&rl_did, RL_EE_PCI_DID, 1, 0);

	t = rl_devs;
	sc->rl_type = 0;
	while(t->rl_name != NULL) {
		if (rl_did == t->rl_did) {
			sc->rl_type = t->rl_basetype;
			break;
		}
		t++;
	}

	if (sc->rl_type == 0) {
		printf("rl%d: unknown device ID: %x\n", unit, rl_did);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define RL_NSEG_NEW 32
	error = bus_dma_tag_create(NULL,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, RL_NSEG_NEW,	/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			BUS_DMA_ALLOCNOW,	/* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->rl_parent_tag);
	if (error)
		goto fail;

	/*
	 * Now allocate a tag for the DMA descriptor lists.
	 * All of our lists are allocated as a contiguous block
	 * of memory.
	 */
	error = bus_dma_tag_create(sc->rl_parent_tag,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			RL_RXBUFLEN + 1518, 1,	/* maxsize,nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			BUS_DMA_ALLOCNOW,		/* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->rl_tag);
	if (error)
		goto fail;

	/*
	 * Now allocate a chunk of DMA-able memory based on the
	 * tag we just created.
	 */
	error = bus_dmamem_alloc(sc->rl_tag,
	    (void **)&sc->rl_cdata.rl_rx_buf, BUS_DMA_NOWAIT | BUS_DMA_ZERO,
	    &sc->rl_cdata.rl_rx_dmamap);

	if (error) {
		printf("rl%d: no memory for list buffers!\n", unit);
		bus_dma_tag_destroy(sc->rl_tag);
		sc->rl_tag = NULL;
		goto fail;
	}

	/* Leave a few bytes before the start of the RX ring buffer. */
	sc->rl_cdata.rl_rx_buf_ptr = sc->rl_cdata.rl_rx_buf;
	sc->rl_cdata.rl_rx_buf += sizeof(u_int64_t);

	/* Do MII setup */
	if (mii_phy_probe(dev, &sc->rl_miibus,
	    rl_ifmedia_upd, rl_ifmedia_sts)) {
		printf("rl%d: MII without any phy!\n", sc->rl_unit);
		error = ENXIO;
		goto fail;
	}

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = rl_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = rl_start;
	ifp->if_watchdog = rl_watchdog;
	ifp->if_init = rl_init;
	ifp->if_baudrate = 10000000;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	ifp->if_capenable = ifp->if_capabilities;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);
	
	callout_handle_init(&sc->rl_stat_ch);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->rl_irq, INTR_TYPE_NET,
	    rl_intr, sc, &sc->rl_intrhand);

	if (error) {
		printf("rl%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		rl_detach(dev);

	return (error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
rl_detach(dev)
	device_t		dev;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->rl_mtx), ("rl mutex not initialized"));
	RL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		rl_stop(sc);
		ether_ifdetach(ifp);
	}
	if (sc->rl_miibus)
		device_delete_child(dev, sc->rl_miibus);
	bus_generic_detach(dev);

	if (sc->rl_intrhand)
		bus_teardown_intr(dev, sc->rl_irq, sc->rl_intrhand);
	if (sc->rl_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rl_irq);
	if (sc->rl_res)
		bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);

	if (sc->rl_tag) {
		bus_dmamap_unload(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap);
		bus_dmamem_free(sc->rl_tag, sc->rl_cdata.rl_rx_buf,
		    sc->rl_cdata.rl_rx_dmamap);
		bus_dma_tag_destroy(sc->rl_tag);
	}
	if (sc->rl_parent_tag)
		bus_dma_tag_destroy(sc->rl_parent_tag);

	RL_UNLOCK(sc);
	mtx_destroy(&sc->rl_mtx);

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int
rl_list_tx_init(sc)
	struct rl_softc		*sc;
{
	struct rl_chain_data	*cd;
	int			i;

	cd = &sc->rl_cdata;
	for (i = 0; i < RL_TX_LIST_CNT; i++) {
		cd->rl_tx_chain[i] = NULL;
		CSR_WRITE_4(sc,
		    RL_TXADDR0 + (i * sizeof(u_int32_t)), 0x0000000);
	}

	sc->rl_cdata.cur_tx = 0;
	sc->rl_cdata.last_tx = 0;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 *
 * You know there's something wrong with a PCI bus-master chip design
 * when you have to use m_devget().
 *
 * The receive operation is badly documented in the datasheet, so I'll
 * attempt to document it here. The driver provides a buffer area and
 * places its base address in the RX buffer start address register.
 * The chip then begins copying frames into the RX buffer. Each frame
 * is preceded by a 32-bit RX status word which specifies the length
 * of the frame and certain other status bits. Each frame (starting with
 * the status word) is also 32-bit aligned. The frame length is in the
 * first 16 bits of the status word; the lower 15 bits correspond with
 * the 'rx status register' mentioned in the datasheet.
 *
 * Note: to make the Alpha happy, the frame payload needs to be aligned
 * on a 32-bit boundary. To achieve this, we pass RL_ETHER_ALIGN (2 bytes)
 * as the offset argument to m_devget().
 */
static void
rl_rxeof(sc)
	struct rl_softc		*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			total_len = 0;
	u_int32_t		rxstat;
	caddr_t			rxbufpos;
	int			wrap = 0;
	u_int16_t		cur_rx;
	u_int16_t		limit;
	u_int16_t		rx_bytes = 0, max_bytes;

	RL_LOCK_ASSERT(sc);

	ifp = &sc->arpcom.ac_if;

	bus_dmamap_sync(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap,
	    BUS_DMASYNC_POSTREAD);

	cur_rx = (CSR_READ_2(sc, RL_CURRXADDR) + 16) % RL_RXBUFLEN;

	/* Do not try to read past this point. */
	limit = CSR_READ_2(sc, RL_CURRXBUF) % RL_RXBUFLEN;

	if (limit < cur_rx)
		max_bytes = (RL_RXBUFLEN - cur_rx) + limit;
	else
		max_bytes = limit - cur_rx;

	while((CSR_READ_1(sc, RL_COMMAND) & RL_CMD_EMPTY_RXBUF) == 0) {
#ifdef DEVICE_POLLING
		if (ifp->if_flags & IFF_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif /* DEVICE_POLLING */
		rxbufpos = sc->rl_cdata.rl_rx_buf + cur_rx;
		rxstat = le32toh(*(u_int32_t *)rxbufpos);

		/*
		 * Here's a totally undocumented fact for you. When the
		 * RealTek chip is in the process of copying a packet into
		 * RAM for you, the length will be 0xfff0. If you spot a
		 * packet header with this value, you need to stop. The
		 * datasheet makes absolutely no mention of this and
		 * RealTek should be shot for this.
		 */
		if ((u_int16_t)(rxstat >> 16) == RL_RXSTAT_UNFINISHED)
			break;

		if (!(rxstat & RL_RXSTAT_RXOK)) {
			ifp->if_ierrors++;
			rl_init(sc);
			return;
		}

		/* No errors; receive the packet. */
		total_len = rxstat >> 16;
		rx_bytes += total_len + 4;

		/*
		 * XXX The RealTek chip includes the CRC with every
		 * received frame, and there's no way to turn this
		 * behavior off (at least, I can't find anything in
		 * the manual that explains how to do it) so we have
		 * to trim off the CRC manually.
		 */
		total_len -= ETHER_CRC_LEN;

		/*
		 * Avoid trying to read more bytes than we know
		 * the chip has prepared for us.
		 */
		if (rx_bytes > max_bytes)
			break;

		rxbufpos = sc->rl_cdata.rl_rx_buf +
			((cur_rx + sizeof(u_int32_t)) % RL_RXBUFLEN);

		if (rxbufpos == (sc->rl_cdata.rl_rx_buf + RL_RXBUFLEN))
			rxbufpos = sc->rl_cdata.rl_rx_buf;

		wrap = (sc->rl_cdata.rl_rx_buf + RL_RXBUFLEN) - rxbufpos;

		if (total_len > wrap) {
			m = m_devget(rxbufpos, total_len, RL_ETHER_ALIGN, ifp,
			    NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
			} else {
				m_copyback(m, wrap, total_len - wrap,
					sc->rl_cdata.rl_rx_buf);
			}
			cur_rx = (total_len - wrap + ETHER_CRC_LEN);
		} else {
			m = m_devget(rxbufpos, total_len, RL_ETHER_ALIGN, ifp,
			    NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
			}
			cur_rx += total_len + 4 + ETHER_CRC_LEN;
		}

		/*
		 * Round up to 32-bit boundary.
		 */
		cur_rx = (cur_rx + 3) & ~3;
		CSR_WRITE_2(sc, RL_CURRXADDR, cur_rx - 16);

		if (m == NULL)
			continue;

		ifp->if_ipackets++;
		RL_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		RL_LOCK(sc);
	}

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void
rl_txeof(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	u_int32_t		txstat;

	ifp = &sc->arpcom.ac_if;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been uploaded.
	 */
	do {
		txstat = CSR_READ_4(sc, RL_LAST_TXSTAT(sc));
		if (!(txstat & (RL_TXSTAT_TX_OK|
		    RL_TXSTAT_TX_UNDERRUN|RL_TXSTAT_TXABRT)))
			break;

		ifp->if_collisions += (txstat & RL_TXSTAT_COLLCNT) >> 24;

		if (RL_LAST_TXMBUF(sc) != NULL) {
			bus_dmamap_unload(sc->rl_tag, RL_LAST_DMAMAP(sc));
			bus_dmamap_destroy(sc->rl_tag, RL_LAST_DMAMAP(sc));
			m_freem(RL_LAST_TXMBUF(sc));
			RL_LAST_TXMBUF(sc) = NULL;
		}
		if (txstat & RL_TXSTAT_TX_OK)
			ifp->if_opackets++;
		else {
			int			oldthresh;
			ifp->if_oerrors++;
			if ((txstat & RL_TXSTAT_TXABRT) ||
			    (txstat & RL_TXSTAT_OUTOFWIN))
				CSR_WRITE_4(sc, RL_TXCFG, RL_TXCFG_CONFIG);
			oldthresh = sc->rl_txthresh;
			/* error recovery */
			rl_reset(sc);
			rl_init(sc);
			/*
			 * If there was a transmit underrun,
			 * bump the TX threshold.
			 */
			if (txstat & RL_TXSTAT_TX_UNDERRUN)
				sc->rl_txthresh = oldthresh + 32;
			return;
		}
		RL_INC(sc->rl_cdata.last_tx);
		ifp->if_flags &= ~IFF_OACTIVE;
	} while (sc->rl_cdata.last_tx != sc->rl_cdata.cur_tx);

	ifp->if_timer =
	    (sc->rl_cdata.last_tx == sc->rl_cdata.cur_tx) ? 0 : 5;

	return;
}

static void
rl_tick(xsc)
	void			*xsc;
{
	struct rl_softc		*sc;
	struct mii_data		*mii;

	sc = xsc;
	RL_LOCK(sc);
	mii = device_get_softc(sc->rl_miibus);

	mii_tick(mii);

	sc->rl_stat_ch = timeout(rl_tick, sc, hz);
	RL_UNLOCK(sc);

	return;
}

#ifdef DEVICE_POLLING
static void
rl_poll (struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct rl_softc *sc = ifp->if_softc;

	RL_LOCK(sc);
	if (cmd == POLL_DEREGISTER) { /* final call, enable interrupts */
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS);
		goto done;
	}

	sc->rxcycles = count;
	rl_rxeof(sc);
	rl_txeof(sc);
	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		rl_start(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) { /* also check status register */
		u_int16_t       status;

		status = CSR_READ_2(sc, RL_ISR);
		if (status == 0xffff)
			goto done;
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		/*
		 * XXX check behaviour on receiver stalls.
		 */

		if (status & RL_ISR_SYSTEM_ERR) {
			rl_reset(sc);
			rl_init(sc);
		}
	}
done:
	RL_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

static void
rl_intr(arg)
	void			*arg;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;

	if (sc->suspended) {
		return;
	}

	RL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

#ifdef DEVICE_POLLING
	if  (ifp->if_flags & IFF_POLLING)
		goto done;
	if (ether_poll_register(rl_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_2(sc, RL_IMR, 0x0000);
		rl_poll(ifp, 0, 1);
		goto done;
	}
#endif /* DEVICE_POLLING */

	for (;;) {

		status = CSR_READ_2(sc, RL_ISR);
		/* If the card has gone away the read returns 0xffff. */
		if (status == 0xffff)
			break;
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		if ((status & RL_INTRS) == 0)
			break;

		if (status & RL_ISR_RX_OK)
			rl_rxeof(sc);

		if (status & RL_ISR_RX_ERR)
			rl_rxeof(sc);

		if ((status & RL_ISR_TX_OK) || (status & RL_ISR_TX_ERR))
			rl_txeof(sc);

		if (status & RL_ISR_SYSTEM_ERR) {
			rl_reset(sc);
			rl_init(sc);
		}

	}

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		rl_start(ifp);

#ifdef DEVICE_POLLING
done:
#endif
	RL_UNLOCK(sc);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
rl_encap(sc, m_head)
	struct rl_softc		*sc;
	struct mbuf		*m_head;
{
	struct mbuf		*m_new = NULL;

	/*
	 * The RealTek is brain damaged and wants longword-aligned
	 * TX buffers, plus we can only have one fragment buffer
	 * per packet. We have to copy pretty much all the time.
	 */
	m_new = m_defrag(m_head, M_DONTWAIT);

	if (m_new == NULL) {
		m_freem(m_head);
		return(1);
	}
	m_head = m_new;

	/* Pad frames to at least 60 bytes. */
	if (m_head->m_pkthdr.len < RL_MIN_FRAMELEN) {
		/*
		 * Make security concious people happy: zero out the
		 * bytes in the pad area, since we don't know what
		 * this mbuf cluster buffer's previous user might
		 * have left in it.
		 */
		bzero(mtod(m_head, char *) + m_head->m_pkthdr.len,
		     RL_MIN_FRAMELEN - m_head->m_pkthdr.len);
		m_head->m_pkthdr.len +=
		    (RL_MIN_FRAMELEN - m_head->m_pkthdr.len);
		m_head->m_len = m_head->m_pkthdr.len;
	}

	RL_CUR_TXMBUF(sc) = m_head;

	return(0);
}

/*
 * Main transmit routine.
 */

static void
rl_start(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mbuf		*m_head = NULL;
	int			pkts = 0;

	sc = ifp->if_softc;
	RL_LOCK(sc);

	while(RL_CUR_TXMBUF(sc) == NULL) {
		IFQ_LOCK(&ifp->if_snd);
		IFQ_POLL_NOLOCK(&ifp->if_snd, m_head);
		if (m_head == NULL) {
			IFQ_UNLOCK(&ifp->if_snd);
			break;
		}

		if (rl_encap(sc, m_head)) {
			IFQ_UNLOCK(&ifp->if_snd);
			m_freem(m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* now we are committed to transmit the packet */
		IFQ_DEQUEUE_NOLOCK(&ifp->if_snd, m_head);
		IFQ_UNLOCK(&ifp->if_snd);
		pkts++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, RL_CUR_TXMBUF(sc));

		/*
		 * Transmit the frame.
		 */
		bus_dmamap_create(sc->rl_tag, 0, &RL_CUR_DMAMAP(sc));
		bus_dmamap_load(sc->rl_tag, RL_CUR_DMAMAP(sc),
		    mtod(RL_CUR_TXMBUF(sc), void *),
		    RL_CUR_TXMBUF(sc)->m_pkthdr.len, rl_dma_map_txbuf, sc, 0);
		bus_dmamap_sync(sc->rl_tag, RL_CUR_DMAMAP(sc),
		    BUS_DMASYNC_PREREAD);
		CSR_WRITE_4(sc, RL_CUR_TXSTAT(sc),
		    RL_TXTHRESH(sc->rl_txthresh) |
		    RL_CUR_TXMBUF(sc)->m_pkthdr.len);

		RL_INC(sc->rl_cdata.cur_tx);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}
	if (pkts == 0) {
		RL_UNLOCK(sc);
		return;
	}

	/*
	 * We broke out of the loop because all our TX slots are
	 * full. Mark the NIC as busy until it drains some of the
	 * packets from the queue.
	 */
	if (RL_CUR_TXMBUF(sc) != NULL)
		ifp->if_flags |= IFF_OACTIVE;

	RL_UNLOCK(sc);

	return;
}

static void
rl_init(xsc)
	void			*xsc;
{
	struct rl_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;
	u_int32_t		rxcfg = 0;

	RL_LOCK(sc);
	mii = device_get_softc(sc->rl_miibus);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	rl_stop(sc);

	/*
	 * Init our MAC address.  Even though the chipset
	 * documentation doesn't mention it, we need to enter "Config
	 * register write enable" mode to modify the ID registers.
	 */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_WRITECFG);
	CSR_WRITE_STREAM_4(sc, RL_IDR0,
	    *(u_int32_t *)(&sc->arpcom.ac_enaddr[0]));
	CSR_WRITE_STREAM_4(sc, RL_IDR4,
	    *(u_int32_t *)(&sc->arpcom.ac_enaddr[4]));
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	/* Init the RX buffer pointer register. */
	bus_dmamap_load(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap,
	    sc->rl_cdata.rl_rx_buf, RL_RXBUFLEN, rl_dma_map_rxbuf, sc, 0);
	bus_dmamap_sync(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap,
	    BUS_DMASYNC_PREWRITE);

	/* Init TX descriptors. */
	rl_list_tx_init(sc);

	/*
	 * Enable transmit and receive.
	 */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	/*
	 * Set the initial TX and RX configuration.
	 */
	CSR_WRITE_4(sc, RL_TXCFG, RL_TXCFG_CONFIG);
	CSR_WRITE_4(sc, RL_RXCFG, RL_RXCFG_CONFIG);

	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RL_RXCFG);
	rxcfg |= RL_RXCFG_RX_INDIV;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		rxcfg |= RL_RXCFG_RX_ALLPHYS;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	} else {
		rxcfg &= ~RL_RXCFG_RX_ALLPHYS;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		rxcfg |= RL_RXCFG_RX_BROAD;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	} else {
		rxcfg &= ~RL_RXCFG_RX_BROAD;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	}

	/*
	 * Program the multicast filter, if necessary.
	 */
	rl_setmulti(sc);

#ifdef DEVICE_POLLING
	/*
	 * Disable interrupts if we are polling.
	 */
	if (ifp->if_flags & IFF_POLLING)
		CSR_WRITE_2(sc, RL_IMR, 0);
	else	/* otherwise ... */
#endif /* DEVICE_POLLING */
	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, RL_IMR, RL_INTRS);

	/* Set initial TX threshold */
	sc->rl_txthresh = RL_TX_THRESH_INIT;

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);

	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	mii_mediachg(mii);

	CSR_WRITE_1(sc, RL_CFG1, RL_CFG1_DRVLOAD|RL_CFG1_FULLDUPLEX);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->rl_stat_ch = timeout(rl_tick, sc, hz);
	RL_UNLOCK(sc);

	return;
}

/*
 * Set media options.
 */
static int
rl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->rl_miibus);
	mii_mediachg(mii);

	return(0);
}

/*
 * Report current media status.
 */
static void
rl_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct rl_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->rl_miibus);

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int
rl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct rl_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

	RL_LOCK(sc);

	switch(command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			rl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				rl_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		rl_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->rl_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	RL_UNLOCK(sc);

	return(error);
}

static void
rl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;

	sc = ifp->if_softc;
	RL_LOCK(sc);
	printf("rl%d: watchdog timeout\n", sc->rl_unit);
	ifp->if_oerrors++;

	rl_txeof(sc);
	rl_rxeof(sc);
	rl_init(sc);
	RL_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
rl_stop(sc)
	struct rl_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	RL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	untimeout(rl_tick, sc, sc->rl_stat_ch);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */

	CSR_WRITE_1(sc, RL_COMMAND, 0x00);
	CSR_WRITE_2(sc, RL_IMR, 0x0000);
	bus_dmamap_unload(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap);

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < RL_TX_LIST_CNT; i++) {
		if (sc->rl_cdata.rl_tx_chain[i] != NULL) {
			bus_dmamap_unload(sc->rl_tag,
			    sc->rl_cdata.rl_tx_dmamap[i]);
			bus_dmamap_destroy(sc->rl_tag,
			    sc->rl_cdata.rl_tx_dmamap[i]);
			m_freem(sc->rl_cdata.rl_tx_chain[i]);
			sc->rl_cdata.rl_tx_chain[i] = NULL;
			CSR_WRITE_4(sc, RL_TXADDR0 + i, 0x0000000);
		}
	}

	RL_UNLOCK(sc);
	return;
}

/*
 * Device suspend routine.  Stop the interface and save some PCI
 * settings in case the BIOS doesn't restore them properly on
 * resume.
 */
static int
rl_suspend(dev)
	device_t		dev;
{
	register int		i;
	struct rl_softc		*sc;

	sc = device_get_softc(dev);

	rl_stop(sc);

	for (i = 0; i < 5; i++)
		sc->saved_maps[i] = pci_read_config(dev, PCIR_MAPS + i * 4, 4);
	sc->saved_biosaddr = pci_read_config(dev, PCIR_BIOS, 4);
	sc->saved_intline = pci_read_config(dev, PCIR_INTLINE, 1);
	sc->saved_cachelnsz = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	sc->saved_lattimer = pci_read_config(dev, PCIR_LATTIMER, 1);

	sc->suspended = 1;

	return (0);
}

/*
 * Device resume routine.  Restore some PCI settings in case the BIOS
 * doesn't, re-enable busmastering, and restart the interface if
 * appropriate.
 */
static int
rl_resume(dev)
	device_t		dev;
{
	register int		i;
	struct rl_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	/* better way to do this? */
	for (i = 0; i < 5; i++)
		pci_write_config(dev, PCIR_MAPS + i * 4, sc->saved_maps[i], 4);
	pci_write_config(dev, PCIR_BIOS, sc->saved_biosaddr, 4);
	pci_write_config(dev, PCIR_INTLINE, sc->saved_intline, 1);
	pci_write_config(dev, PCIR_CACHELNSZ, sc->saved_cachelnsz, 1);
	pci_write_config(dev, PCIR_LATTIMER, sc->saved_lattimer, 1);

	/* reenable busmastering */
	pci_enable_busmaster(dev);
	pci_enable_io(dev, RL_RES);

	/* reinitialize interface if necessary */
	if (ifp->if_flags & IFF_UP)
		rl_init(sc);

	sc->suspended = 0;

	return (0);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
rl_shutdown(dev)
	device_t		dev;
{
	struct rl_softc		*sc;

	sc = device_get_softc(dev);

	rl_stop(sc);

	return;
}
