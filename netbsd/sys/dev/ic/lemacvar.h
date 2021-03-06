/*      $NetBSD: lemacvar.h,v 1.6 2001/06/13 10:46:03 wiz Exp $ */

/*
 * Copyright (c) 1997 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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

#ifndef _LEMAC_VAR_H
#define	_LEMAC_VAR_H

#include "rnd.h"
#if NRND > 0
#include <sys/rnd.h>
#endif

/*
 * Ethernet status, per interface.
 */
typedef struct {
    struct device sc_dv;
    void *sc_ih;
    void *sc_ats;
    struct ethercom sc_ec;
    struct ifmedia sc_ifmedia;
    bus_space_tag_t sc_iot;
    bus_space_tag_t sc_memt;
    bus_space_handle_t sc_ioh;
    bus_space_handle_t sc_memh;
    unsigned sc_flags;			/* */
#define	LEMAC_PIO_MODE		0x0000U
#define	LEMAC_2K_MODE		0x0001U
#define	LEMAC_WAS_32K_MODE	0x0002U
#define	LEMAC_WAS_64K_MODE	0x0003U
#define	LEMAC_MODE_MASK		0x0003U
#define	LEMAC_ALLMULTI		0x0010U
#define	LEMAC_ALIVE		0x0020U
#define	LEMAC_LINKUP		0x0040U
    unsigned sc_lastpage;		/* last 2K page */
    unsigned sc_txctl;			/* Transmit Control Byte */
    unsigned sc_ctlmode;		/* media ctl bits */
    struct {
	u_int8_t csr_cs;
	u_int8_t csr_tqc;
	u_int8_t csr_fmq;
    } sc_csr;
    unsigned sc_laststatus;		/* last read of LEMAC_REG_CS */
    u_int16_t sc_mctbl[LEMAC_MCTBL_SIZE/sizeof(u_int16_t)];
					/* local copy of multicast table */
    struct {
	unsigned cntr_txnospc;		/* total # of no trnasmit memory */
	unsigned cntr_txfull;		/* total # of tranmitter full */
	unsigned cntr_tne_intrs;	/* total # of tranmit done intrs */
	unsigned cntr_rne_intrs;	/* total # of receive done intrs */
	unsigned cntr_txd_intrs;	/* total # of tranmit error intrs */
	unsigned cntr_rxd_intrs;	/* total # of receive error intrs */
    } sc_cntrs;
    /*
     * We rely on sc_enaddr being aligned on (at least) a 16 bit boundary
     */
    unsigned char sc_enaddr[ETHER_ADDR_LEN];	/* current Ethernet address */
    char sc_prodname[LEMAC_EEP_PRDNMSZ+1]; /* product name DE20x-xx */
    u_int8_t sc_eeprom[LEMAC_EEP_SIZE];	/* local copy eeprom */
#if NRND > 0
    rndsource_element_t rnd_source;
#endif
} lemac_softc_t;

#define	sc_if	sc_ec.ec_if

#define	LEMAC_IFP_TO_SOFTC(ifp)	((lemac_softc_t *)((ifp)->if_softc))
#define	LEMAC_USE_PIO_MODE(sc)	(((sc->sc_flags & LEMAC_MODE_MASK) == LEMAC_PIO_MODE) || (sc->sc_if.if_flags & IFF_LINK0))

#define	LEMAC_OUTB(sc, o, v)	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (o), (v))
#define	LEMAC_OUTSB(sc, o, l, p)	bus_space_write_multi_1((sc)->sc_iot, (sc)->sc_ioh, (o), (p), (l))
#define	LEMAC_INB(sc, o)	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (o))
#define	LEMAC_INSB(sc, o, l, p)	bus_space_read_multi_1((sc)->sc_iot, (sc)->sc_ioh, (o), (p), (l))

#define	LEMAC_PUTBUF8(sc, o, l, p)	bus_space_write_region_1((sc)->sc_memt, (sc)->sc_memh, (o), (p), (l))
#define	LEMAC_PUTBUF16(sc, o, l, p)	bus_space_write_region_2((sc)->sc_memt, (sc)->sc_memh, (o), (p), (l))
#define	LEMAC_PUTBUF32(sc, o, l, p)	bus_space_write_region_4((sc)->sc_memt, (sc)->sc_memh, (o), (p), (l))

#define	LEMAC_PUT8(sc, o, v)	bus_space_write_1((sc)->sc_memt, (sc)->sc_memh, (o), (v))
#define	LEMAC_PUT16(sc, o, v)	bus_space_write_2((sc)->sc_memt, (sc)->sc_memh, (o), (v))
#define	LEMAC_PUT32(sc, o, v)	bus_space_write_4((sc)->sc_memt, (sc)->sc_memh, (o), (v))

#define	LEMAC_GETBUF8(sc, o, l, p)	bus_space_read_region_1((sc)->sc_memt, (sc)->sc_memh, (o), (p), (l))
#define	LEMAC_GETBUF16(sc, o, l, p)	bus_space_read_region_2((sc)->sc_memt, (sc)->sc_memh, (o), (p), (l))
#define	LEMAC_GETBUF32(sc, o, l, p)	bus_space_read_region_4((sc)->sc_memt, (sc)->sc_memh, (o), (p), (l))

#define	LEMAC_GET8(sc, o)	bus_space_read_1((sc)->sc_memt, (sc)->sc_memh, (o))
#define	LEMAC_GET16(sc, o)	bus_space_read_2((sc)->sc_memt, (sc)->sc_memh, (o))
#define	LEMAC_GET32(sc, o)	bus_space_read_4((sc)->sc_memt, (sc)->sc_memh, (o))


#define	LEMAC_INTR_ENABLE(sc) \
	LEMAC_OUTB(sc, LEMAC_REG_IC, LEMAC_INB(sc, LEMAC_REG_IC) | LEMAC_IC_ALL)

#define	LEMAC_INTR_DISABLE(sc) \
	LEMAC_OUTB(sc, LEMAC_REG_IC, LEMAC_INB(sc, LEMAC_REG_IC) & ~LEMAC_IC_ALL)

#define LEMAC_IS_64K_MODE(mbase) (((mbase) >= 0x0A) && ((mbase) <= 0x0F))
#define LEMAC_IS_32K_MODE(mbase) (((mbase) >= 0x14) && ((mbase) <= 0x1F))
#define LEMAC_IS_2K_MODE(mbase)	( (mbase) >= 0x40)

#define	LEMAC_DECODEIRQ(i)	((0xFBA5 >> ((i) >> 3)) & 0x0F)

#define	LEMAC_ADDREQUAL(a1, a2) \
	(((u_int16_t *)a1)[0] == ((u_int16_t *)a2)[0] \
	 && ((u_int16_t *)a1)[1] == ((u_int16_t *)a2)[1] \
	 && ((u_int16_t *)a1)[2] == ((u_int16_t *)a2)[2])

#define	LEMAC_ADDRBRDCST(a1) \
	(((u_int16_t *)a1)[0] == 0xFFFFU \
	 && ((u_int16_t *)a1)[1] == 0xFFFFU \
	 && ((u_int16_t *)a1)[2] == 0xFFFFU)

extern void lemac_ifattach(lemac_softc_t *sc);
extern void lemac_info_get(const bus_space_tag_t iot,
			   const bus_space_handle_t ioh,
			   bus_addr_t *maddr_p,
			   bus_size_t *msize_p,
			   int *irq_p);
extern int lemac_port_check(const bus_space_tag_t iot,
			    const bus_space_handle_t ioh);
extern int lemac_intr(void *arg);
extern void lemac_shutdown(void *arg);

#endif /* _LEMACVAR_H */
