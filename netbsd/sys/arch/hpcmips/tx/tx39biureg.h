/*	$NetBSD: tx39biureg.h,v 1.3 2001/06/14 11:09:55 uch Exp $ */

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
/*
 * Toshiba TX3912/3922 BIU module (Bus Interface Unit)
 */

/*
 * System Address Map
 */
#define TX39_SYSADDR_DRAMBANK0CS1	0x00000000
#define TX39_SYSADDR_DRAMBANK1CS1	0x02000000
#define TX39_SYSADDR_DRAMBANK0		0x04000000
#define TX39_SYSADDR_DRAMBANK1		0x06000000
#define TX39_SYSADDR_DRAMBANK_LEN	0x02000000

#define TX39_SYSADDR_CARD1		0x08000000
#define TX39_SYSADDR_CARD2		0x0C000000
/* 64MByte */
#define TX39_SYSADDR_CARD_SIZE		0x04000000 

#define TX39_SYSADDR_CS1		0x10000000
#define TX39_SYSADDR_CS2		0x10400000
#define TX39_SYSADDR_CS3		0x10800000
/* 4MByte */
#define TX39_SYSADDR_CS_SIZE		0x00400000

#define TX39_SYSADDR_CONFIG_REG		0x10c00000
#define TX39_SYSADDR_CONFIG_REG_LEN	0x00200000

#define TX39_SYSADDR_SDRAMBANK0MODE_REG	0x10e00000
#define TX39_SYSADDR_SDRAMBANK1MODE_REG	0x10f00000
#define TX39_SYSADDR_CS0		0x11000000
#define TX39_SYSADDR_KUSEG_DRAMBANK0CS1	0x40000000
#define TX39_SYSADDR_KUSEG_DRAMBANK1CS1	0x42000000
#define TX39_SYSADDR_KUSEG_DRAMBANK0	0x44000000
#define TX39_SYSADDR_KUSEG_DRAMBANK1	0x46000000
#define TX39_SYSADDR_KUSEG_CS0		0x50000000
#define TX39_SYSADDR_KUSEG_CS1		0x58000000
#define TX39_SYSADDR_KUSEG_CS2		0x5c000000
#define TX39_SYSADDR_KUSEG_CS3		0x60000000
#define TX39_SYSADDR_CARD1MEM		0x64000000
#define TX39_SYSADDR_CARD2MEM		0x68000000
#define TX39_SYSADDR_MCS0		0x6c000000
#define TX39_SYSADDR_MCS1		0x70000000
#ifdef TX391X
#define TX39_SYSADDR_MCS2		0x74000000
#define TX39_SYSADDR_MCS3		0x78000000
#endif /* TX391X */
/* 64MByte */
#define TX39_SYSADDR_MCS_SIZE		0x04000000

/*
 *	BIU module registers.
 */
#define TX39_MEMCONFIG0_REG		0x00
#define TX39_MEMCONFIG1_REG		0x04
#define TX39_MEMCONFIG2_REG		0x08
#define TX39_MEMCONFIG3_REG		0x0C
#define TX39_MEMCONFIG4_REG		0x10
#define TX39_MEMCONFIG5_REG		0x14
#define TX39_MEMCONFIG6_REG		0x18
#define TX39_MEMCONFIG7_REG		0x1C
#define TX39_MEMCONFIG8_REG		0x20

/*
 *	Memory Configuration 0 Register
 */
/* R/W */
#define TX39_MEMCONFIG0_ENDCLKOUTTRI	0x40000000
#define TX39_MEMCONFIG0_DISDQMINIT	0x20000000
#define TX39_MEMCONFIG0_ENSDRAMPD	0x10000000
#define TX39_MEMCONFIG0_SHOWDINO	0x08000000
#define TX39_MEMCONFIG0_ENRMAP2		0x04000000
#define TX39_MEMCONFIG0_ENRMAP1		0x02000000
#define TX39_MEMCONFIG0_ENWRINPAGE	0x01000000
#define TX39_MEMCONFIG0_ENCS3USER	0x00800000
#define TX39_MEMCONFIG0_ENCS2USER	0x00400000
#define TX39_MEMCONFIG0_ENCS1USER	0x00200000
#define TX39_MEMCONFIG0_ENCS1DRAM	0x00100000

#define TX39_MEMCONFIG0_BANK1CONF_SHIFT 18
#define TX39_MEMCONFIG0_BANK1CONF_MASK	0x3
#define TX39_MEMCONFIG0_BANK1CONF(cr)					\
	(((cr) >> TX39_MEMCONFIG0_BANK1CONF_SHIFT) &			\
	TX39_MEMCONFIG0_BANK1CONF_MASK)
#define TX39_MEMCONFIG0_BANK1CONF_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG0_BANK1CONF_SHIFT) &		\
	(TX39_MEMCONFIG0_BANK1CONF_MASK << TX39_MEMCONFIG0_BANK1CONF_SHIFT)))
#define TX39_MEMCONFIG0_BANK0CONF_SHIFT 16
#define TX39_MEMCONFIG0_BANK0CONF_MASK	0x3
#define TX39_MEMCONFIG0_BANK0CONF(cr)					\
	(((cr) >> TX39_MEMCONFIG0_BANK0CONF_SHIFT) &			\
	TX39_MEMCONFIG0_BANK0CONF_MASK)
#define TX39_MEMCONFIG0_BANK0CONF_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG0_BANK0CONF_SHIFT) &		\
	(TX39_MEMCONFIG0_BANK0CONF_MASK << TX39_MEMCONFIG0_BANK0CONF_SHIFT)))
#define TX39_MEMCONFIG0_BANKCONF_16BITSDRAM	0x3
#define TX39_MEMCONFIG0_BANKCONF_8BITSDRAM	0x2
#define TX39_MEMCONFIG0_BANKCONF_32BITSDHDRAM	0x1
#define TX39_MEMCONFIG0_BANKCONF_16BITSDHDRAM	0x0

#define TX39_MEMCONFIG0_ROWSEL1_SHIFT 14
#define TX39_MEMCONFIG0_ROWSEL1_MASK	0x3
#define TX39_MEMCONFIG0_ROWSEL1(cr)					\
	(((cr) >> TX39_MEMCONFIG0_ROWSEL1_SHIFT) &			\
	TX39_MEMCONFIG0_ROWSEL1_MASK)
#define TX39_MEMCONFIG0_ROWSEL1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG0_ROWSEL1_SHIFT) &		\
	(TX39_MEMCONFIG0_ROWSEL1_MASK << TX39_MEMCONFIG0_ROWSEL1_SHIFT)))
#define TX39_MEMCONFIG0_ROWSEL0_SHIFT 12
#define TX39_MEMCONFIG0_ROWSEL0_MASK	0x3
#define TX39_MEMCONFIG0_ROWSEL0(cr)					\
	(((cr) >> TX39_MEMCONFIG0_ROWSEL0_SHIFT) &			\
	TX39_MEMCONFIG0_ROWSEL0_MASK)
#define TX39_MEMCONFIG0_ROWSEL0_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG0_ROWSEL0_SHIFT) &		\
	(TX39_MEMCONFIG0_ROWSEL0_MASK << TX39_MEMCONFIG0_ROWSEL0_SHIFT)))

#define TX39_MEMCONFIG0_COLSEL1_SHIFT 8
#define TX39_MEMCONFIG0_COLSEL1_MASK	0xf
#define TX39_MEMCONFIG0_COLSEL1(cr)					\
	(((cr) >> TX39_MEMCONFIG0_COLSEL1_SHIFT) &			\
	TX39_MEMCONFIG0_COLSEL1_MASK)
#define TX39_MEMCONFIG0_COLSEL1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG0_COLSEL1_SHIFT) &		\
	(TX39_MEMCONFIG0_COLSEL1_MASK << TX39_MEMCONFIG0_COLSEL1_SHIFT)))
#define TX39_MEMCONFIG0_COLSEL0_SHIFT 4
#define TX39_MEMCONFIG0_COLSEL0_MASK	0xf
#define TX39_MEMCONFIG0_COLSEL0(cr)					\
	(((cr) >> TX39_MEMCONFIG0_COLSEL0_SHIFT) &			\
	TX39_MEMCONFIG0_COLSEL0_MASK)
#define TX39_MEMCONFIG0_COLSEL0_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG0_COLSEL0_SHIFT) &		\
	(TX39_MEMCONFIG0_COLSEL0_MASK << TX39_MEMCONFIG0_COLSEL0_SHIFT)))

#define TX39_MEMCONFIG0_CS3SIZE		0x00000008
#define TX39_MEMCONFIG0_CS2SIZE		0x00000004
#define TX39_MEMCONFIG0_CS1SIZE		0x00000002
#define TX39_MEMCONFIG0_CS0SIZE		0x00000001

/*
 *	Memory Configuration 1 Register
 */
#ifdef TX391X
#define TX39_MEMCONFIG1_MCS3ACCVAL1_SHIFT	28
#define TX39_MEMCONFIG1_MCS3ACCVAL1_MASK	0xf
#define TX39_MEMCONFIG1_MCS3ACCVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG1_MCS3ACCVAL1_SHIFT) &			\
	TX39_MEMCONFIG1_MCS3ACCVAL1_MASK)
#define TX39_MEMCONFIG1_MCS3ACCVAL1_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG1_MCS3ACCVAL1_SHIFT) &		\
	(TX39_MEMCONFIG1_MCS3ACCVAL1_MASK <<				\
	TX39_MEMCONFIG1_MCS3ACCVAL1_SHIFT)))

#define TX39_MEMCONFIG1_MCS3ACCVAL2_SHIFT	24
#define TX39_MEMCONFIG1_MCS3ACCVAL2_MASK	0xf
#define TX39_MEMCONFIG1_MCS3ACCVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG1_MCS3ACCVAL2_SHIFT) &			\
	TX39_MEMCONFIG1_MCS3ACCVAL2_MASK)
#define TX39_MEMCONFIG1_MCS3ACCVAL2_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG1_MCS3ACCVAL2_SHIFT) &		\
	(TX39_MEMCONFIG1_MCS3ACCVAL2_MASK <<				\
	TX39_MEMCONFIG1_MCS3ACCVAL2_SHIFT)))

#define TX39_MEMCONFIG1_MCS2ACCVAL1_SHIFT	20
#define TX39_MEMCONFIG1_MCS2ACCVAL1_MASK	0xf
#define TX39_MEMCONFIG1_MCS2ACCVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG1_MCS2ACCVAL1_SHIFT) &			\
	TX39_MEMCONFIG1_MCS2ACCVAL1_MASK)
#define TX39_MEMCONFIG1_MCS2ACCVAL1_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG1_MCS2ACCVAL1_SHIFT) &		\
	(TX39_MEMCONFIG1_MCS2ACCVAL1_MASK <<				\
	TX39_MEMCONFIG1_MCS2ACCVAL1_SHIFT)))

#define TX39_MEMCONFIG1_MCS2ACCVAL2_SHIFT	16
#define TX39_MEMCONFIG1_MCS2ACCVAL2_MASK	0xf
#define TX39_MEMCONFIG1_MCS2ACCVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG1_MCS2ACCVAL2_SHIFT) &			\
	TX39_MEMCONFIG1_MCS2ACCVAL2_MASK)
#define TX39_MEMCONFIG1_MCS2ACCVAL2_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG1_MCS2ACCVAL2_SHIFT) &		\
	(TX39_MEMCONFIG1_MCS2ACCVAL2_MASK <<				\
	TX39_MEMCONFIG1_MCS2ACCVAL2_SHIFT)))
#endif /* TX391X */
#ifdef TX392X
#define	TX39_MEMCONFIG1_C48MPLLON	0x40000000
#define	TX39_MEMCONFIG1_ENMCS1BE	0x20000000
#define	TX39_MEMCONFIG1_ENMCS0BE	0x10000000
#define	TX39_MEMCONFIG1_ENMCS1ACC	0x08000000
#define	TX39_MEMCONFIG1_ENMCS0ACC	0x04000000
#define TX39_MEMCONFIG1_BCLKDIV_SHIFT	23
#define TX39_MEMCONFIG1_BCLKDIV_MASK	0x7
#define TX39_MEMCONFIG1_BCLKDIV(cr)					\
	(((cr) >> TX39_MEMCONFIG1_BCLKDIV_SHIFT) &			\
	TX39_MEMCONFIG1_BCLKDIV_MASK)
#define TX39_MEMCONFIG1_BCLKDIV_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG1_BCLKDIV_SHIFT) &		\
	(TX39_MEMCONFIG1_BCLKDIV_MASK << TX39_MEMCONFIG1_BCLKDIV_SHIFT)))
#define	TX39_MEMCONFIG1_ENBCLK		0x00400000
#define	TX39_MEMCONFIG1_ENMCS1PAGE	0x00200000
#define	TX39_MEMCONFIG1_ENMCS0PAGE	0x00100000
#define	TX39_MEMCONFIG1_ENMCS1WAIT	0x00080000
#define	TX39_MEMCONFIG1_ENMCS0WAIT	0x00040000
#define	TX39_MEMCONFIG1_MCS1_32		0x00020000
#define	TX39_MEMCONFIG1_MCS0_32		0x00010000
#endif /* TX392X */

#define TX39_MEMCONFIG1_MCS1ACCVAL1_SHIFT	12
#define TX39_MEMCONFIG1_MCS1ACCVAL1_MASK	0xf
#define TX39_MEMCONFIG1_MCS1ACCVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG1_MCS1ACCVAL1_SHIFT) &			\
	TX39_MEMCONFIG1_MCS1ACCVAL1_MASK)
#define TX39_MEMCONFIG1_MCS1ACCVAL1_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG1_MCS1ACCVAL1_SHIFT) &		\
	(TX39_MEMCONFIG1_MCS1ACCVAL1_MASK <<				\
	TX39_MEMCONFIG1_MCS1ACCVAL1_SHIFT)))

#define TX39_MEMCONFIG1_MCS1ACCVAL2_SHIFT	8
#define TX39_MEMCONFIG1_MCS1ACCVAL2_MASK	0xf
#define TX39_MEMCONFIG1_MCS1ACCVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG1_MCS1ACCVAL2_SHIFT) &			\
	TX39_MEMCONFIG1_MCS1ACCVAL2_MASK)
#define TX39_MEMCONFIG1_MCS1ACCVAL2_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG1_MCS1ACCVAL2_SHIFT) &		\
	(TX39_MEMCONFIG1_MCS1ACCVAL2_MASK <<				\
	TX39_MEMCONFIG1_MCS1ACCVAL2_SHIFT)))

#define TX39_MEMCONFIG1_MCS0ACCVAL1_SHIFT	4
#define TX39_MEMCONFIG1_MCS0ACCVAL1_MASK	0xf
#define TX39_MEMCONFIG1_MCS0ACCVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG1_MCS0ACCVAL1_SHIFT) &			\
	TX39_MEMCONFIG1_MCS0ACCVAL1_MASK)
#define TX39_MEMCONFIG1_MCS0ACCVAL1_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG1_MCS0ACCVAL1_SHIFT) &		\
	(TX39_MEMCONFIG1_MCS0ACCVAL1_MASK <<				\
	TX39_MEMCONFIG1_MCS0ACCVAL1_SHIFT)))

#define TX39_MEMCONFIG1_MCS0ACCVAL2_SHIFT	0
#define TX39_MEMCONFIG1_MCS0ACCVAL2_MASK	0xf
#define TX39_MEMCONFIG1_MCS0ACCVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG1_MCS0ACCVAL2_SHIFT) &			\
	TX39_MEMCONFIG1_MCS0ACCVAL2_MASK)
#define TX39_MEMCONFIG1_MCS0ACCVAL2_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG1_MCS0ACCVAL2_SHIFT) &		\
	(TX39_MEMCONFIG1_MCS0ACCVAL2_MASK <<				\
	TX39_MEMCONFIG1_MCS0ACCVAL2_SHIFT)))

/*
 *	Memory Configuration 2 Register
 */
/* Define access timing. not required yet */
#define TX39_MEMCONFIG2_CS3ACCVAL1_SHIFT	28
#define TX39_MEMCONFIG2_CS3ACCVAL1_MASK		0xf
#define TX39_MEMCONFIG2_CS3ACCVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG2_CS3ACCVAL1_SHIFT) &			\
	TX39_MEMCONFIG2_CS3ACCVAL1_MASK)
#define TX39_MEMCONFIG2_CS3ACCVAL1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG2_CS3ACCVAL1_SHIFT) &		\
	(TX39_MEMCONFIG2_CS3ACCVAL1_MASK << TX39_MEMCONFIG2_CS3ACCVAL1_SHIFT)))

#define TX39_MEMCONFIG2_CS3ACCVAL2_SHIFT	24
#define TX39_MEMCONFIG2_CS3ACCVAL2_MASK		0xf
#define TX39_MEMCONFIG2_CS3ACCVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG2_CS3ACCVAL2_SHIFT) &			\
	TX39_MEMCONFIG2_CS3ACCVAL2_MASK)
#define TX39_MEMCONFIG2_CS3ACCVAL2_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG2_CS3ACCVAL2_SHIFT) &		\
	(TX39_MEMCONFIG2_CS3ACCVAL2_MASK << TX39_MEMCONFIG2_CS3ACCVAL2_SHIFT)))

#define TX39_MEMCONFIG2_CS2ACCVAL1_SHIFT	20
#define TX39_MEMCONFIG2_CS2ACCVAL1_MASK		0xf
#define TX39_MEMCONFIG2_CS2ACCVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG2_CS2ACCVAL1_SHIFT) &			\
	TX39_MEMCONFIG2_CS2ACCVAL1_MASK)
#define TX39_MEMCONFIG2_CS2ACCVAL1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG2_CS2ACCVAL1_SHIFT) &		\
	(TX39_MEMCONFIG2_CS2ACCVAL1_MASK << TX39_MEMCONFIG2_CS2ACCVAL1_SHIFT)))

#define TX39_MEMCONFIG2_CS2ACCVAL2_SHIFT	16
#define TX39_MEMCONFIG2_CS2ACCVAL2_MASK		0xf
#define TX39_MEMCONFIG2_CS2ACCVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG2_CS2ACCVAL2_SHIFT) &			\
	TX39_MEMCONFIG2_CS2ACCVAL2_MASK)
#define TX39_MEMCONFIG2_CS2ACCVAL2_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG2_CS2ACCVAL2_SHIFT) &		\
	(TX39_MEMCONFIG2_CS2ACCVAL2_MASK << TX39_MEMCONFIG2_CS2ACCVAL2_SHIFT)))

#define TX39_MEMCONFIG2_CS1ACCVAL1_SHIFT	12
#define TX39_MEMCONFIG2_CS1ACCVAL1_MASK		0xf
#define TX39_MEMCONFIG2_CS1ACCVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG2_CS1ACCVAL1_SHIFT) &			\
	TX39_MEMCONFIG2_CS1ACCVAL1_MASK)
#define TX39_MEMCONFIG2_CS1ACCVAL1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG2_CS1ACCVAL1_SHIFT) &		\
	(TX39_MEMCONFIG2_CS1ACCVAL1_MASK << TX39_MEMCONFIG2_CS1ACCVAL1_SHIFT)))

#define TX39_MEMCONFIG2_CS1ACCVAL2_SHIFT	8
#define TX39_MEMCONFIG2_CS1ACCVAL2_MASK		0xf
#define TX39_MEMCONFIG2_CS1ACCVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG2_CS1ACCVAL2_SHIFT) &			\
	TX39_MEMCONFIG2_CS1ACCVAL2_MASK)
#define TX39_MEMCONFIG2_CS1ACCVAL2_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG2_CS1ACCVAL2_SHIFT) &		\
	(TX39_MEMCONFIG2_CS1ACCVAL2_MASK << TX39_MEMCONFIG2_CS1ACCVAL2_SHIFT)))

#define TX39_MEMCONFIG2_CS0ACCVAL1_SHIFT	4
#define TX39_MEMCONFIG2_CS0ACCVAL1_MASK		0xf
#define TX39_MEMCONFIG2_CS0ACCVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG2_CS0ACCVAL1_SHIFT) &			\
	TX39_MEMCONFIG2_CS0ACCVAL1_MASK)
#define TX39_MEMCONFIG2_CS0ACCVAL1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG2_CS0ACCVAL1_SHIFT) &		\
	(TX39_MEMCONFIG2_CS0ACCVAL1_MASK << TX39_MEMCONFIG2_CS0ACCVAL1_SHIFT)))

#define TX39_MEMCONFIG2_CS0ACCVAL2_SHIFT	0
#define TX39_MEMCONFIG2_CS0ACCVAL2_MASK		0xf
#define TX39_MEMCONFIG2_CS0ACCVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG2_CS0ACCVAL2_SHIFT) &			\
	TX39_MEMCONFIG2_CS0ACCVAL2_MASK)
#define TX39_MEMCONFIG2_CS0ACCVAL2_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG2_CS0ACCVAL2_SHIFT) &		\
	(TX39_MEMCONFIG2_CS0ACCVAL2_MASK << TX39_MEMCONFIG2_CS0ACCVAL2_SHIFT)))

/*
 *	Memory Configuration 3 Register
 */
/* Define access timing, enable read page mode, PC-Card. */
#define TX39_MEMCONFIG3_CARD2ACCVAL_SHIFT	28
#define TX39_MEMCONFIG3_CARD2ACCVAL_MASK	0xf
#define TX39_MEMCONFIG3_CARD2ACCVAL(cr)					\
	(((cr) >> TX39_MEMCONFIG3_CARD2ACCVAL_SHIFT) &			\
	TX39_MEMCONFIG3_CARD2ACCVAL_MASK)
#define TX39_MEMCONFIG3_CARD2ACCVAL_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG3_CARD2ACCVAL_SHIFT) &		\
	(TX39_MEMCONFIG3_CARD2ACCVAL_MASK <<				\
	TX39_MEMCONFIG3_CARD2ACCVAL_SHIFT)))

#define TX39_MEMCONFIG3_CARD1ACCVAL_SHIFT	24
#define TX39_MEMCONFIG3_CARD1ACCVAL_MASK	0xf
#define TX39_MEMCONFIG3_CARD1ACCVAL(cr)					\
	(((cr) >> TX39_MEMCONFIG3_CARD1ACCVAL_SHIFT) &			\
	TX39_MEMCONFIG3_CARD1ACCVAL_MASK)
#define TX39_MEMCONFIG3_CARD1ACCVAL_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG3_CARD1ACCVAL_SHIFT) &		\
	(TX39_MEMCONFIG3_CARD1ACCVAL_MASK <<				\
	TX39_MEMCONFIG3_CARD1ACCVAL_SHIFT)))

#define TX39_MEMCONFIG3_CARD2IOACCVAL_SHIFT	20
#define TX39_MEMCONFIG3_CARD2IOACCVAL_MASK	0xf
#define TX39_MEMCONFIG3_CARD2IOACCVAL(cr)				\
	(((cr) >> TX39_MEMCONFIG3_CARD2IOACCVAL_SHIFT) &		\
	TX39_MEMCONFIG3_CARD2IOACCVAL_MASK)
#define TX39_MEMCONFIG3_CARD2IOACCVAL_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG3_CARD2IOACCVAL_SHIFT) &	\
	(TX39_MEMCONFIG3_CARD2IOACCVAL_MASK <<				\
	TX39_MEMCONFIG3_CARD2IOACCVAL_SHIFT)))

#define TX39_MEMCONFIG3_CARD1IOACCVAL_SHIFT	16
#define TX39_MEMCONFIG3_CARD1IOACCVAL_MASK	0xf
#define TX39_MEMCONFIG3_CARD1IOACCVAL(cr)				\
	(((cr) >> TX39_MEMCONFIG3_CARD1IOACCVAL_SHIFT) &		\
	TX39_MEMCONFIG3_CARD1IOACCVAL_MASK)
#define TX39_MEMCONFIG3_CARD1IOACCVAL_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG3_CARD1IOACCVAL_SHIFT) &	\
	(TX39_MEMCONFIG3_CARD1IOACCVAL_MASK <<				\
	TX39_MEMCONFIG3_CARD1IOACCVAL_SHIFT)))
#ifdef TX391X
#define TX39_MEMCONFIG3_ENMCS3PAGE		0x00008000
#define TX39_MEMCONFIG3_ENMCS2PAGE		0x00004000
#define TX39_MEMCONFIG3_ENMCS1PAGE		0x00002000
#define TX39_MEMCONFIG3_ENMCS0PAGE		0x00001000
#endif /* TX391X */
#define TX39_MEMCONFIG3_ENCS3PAGE		0x00000800
#define TX39_MEMCONFIG3_ENCS2PAGE		0x00000400
#define TX39_MEMCONFIG3_ENCS1PAGE		0x00000200
#define TX39_MEMCONFIG3_ENCS0PAGE		0x00000100
#define TX39_MEMCONFIG3_CARD2WAITEN		0x00000080
#define TX39_MEMCONFIG3_CARD1WAITEN		0x00000040
#define TX39_MEMCONFIG3_CARD2IOEN		0x00000020
#define TX39_MEMCONFIG3_CARD1IOEN		0x00000010
#ifdef TX391X
#define TX39_MEMCONFIG3_PORT8SEL		0x00000008
#endif /* TX391X */
#ifdef TX392X
#define TX39_MEMCONFIG3_CARD2_8SEL		0x00000008
#define TX39_MEMCONFIG3_CARD1_8SEL		0x00000004
#endif /* TX392X */
/*
 *	Memory Configuration 4 Register
 */
/* DMA */
#define TX39_MEMCONFIG4_ENBANK1HDRAM		0x80000000
#define TX39_MEMCONFIG4_ENBANK0HDRAM		0x40000000
#define TX39_MEMCONFIG4_ENARB			0x20000000
#define TX39_MEMCONFIG4_DISSNOOP		0x10000000
#define TX39_MEMCONFIG4_CLRWRBUSERRINT		0x08000000
#define TX39_MEMCONFIG4_ENBANK1OPT		0x04000000
#define TX39_MEMCONFIG4_ENBANK0OPT		0x02000000
#define TX39_MEMCONFIG4_ENWATCH			0x01000000

/*
 * WatchDogTimerRate = (WATCHTIME[3:0] + 1) * 64 / 36.864MHz
 */
#define TX39_MEMCONFIG4_WATCHTIMEVAL_SHIFT	20
#define TX39_MEMCONFIG4_WATCHTIMEVAL_MASK	0xf
#define TX39_MEMCONFIG4_WATCHTIMEVAL(cr)				\
	(((cr) >> TX39_MEMCONFIG4_WATCHTIMEVAL_SHIFT) &			\
	TX39_MEMCONFIG4_WATCHTIMEVAL_MASK)
#define TX39_MEMCONFIG4_WATCHTIMEVAL_SET(cr, val)			\
	((cr) | (((val) << TX39_MEMCONFIG4_WATCHTIMEVAL_SHIFT) &	\
	(TX39_MEMCONFIG4_WATCHTIMEVAL_MASK <<				\
	TX39_MEMCONFIG4_WATCHTIMEVAL_SHIFT)))


#define TX39_MEMCONFIG4_MEMPOWERDOWN		0x00010000
#define TX39_MEMCONFIG4_ENRFSH1			0x00008000
#define TX39_MEMCONFIG4_ENRFSH0			0x00004000

#define TX39_MEMCONFIG4_RFSHVAL1_SHIFT	8
#define TX39_MEMCONFIG4_RFSHVAL1_MASK	0x3f
#define TX39_MEMCONFIG4_RFSHVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG4_RFSHVAL1_SHIFT) &			\
	TX39_MEMCONFIG4_RFSHVAL1_MASK)
#define TX39_MEMCONFIG4_RFSHVAL1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG4_RFSHVAL1_SHIFT) &		\
	(TX39_MEMCONFIG4_RFSHVAL1_MASK << TX39_MEMCONFIG4_RFSHVAL1_SHIFT)))

#define TX39_MEMCONFIG4_RFSHVAL0_SHIFT	0
#define TX39_MEMCONFIG4_RFSHVAL0_MASK	0x3f
#define TX39_MEMCONFIG4_RFSHVAL0(cr)					\
	(((cr) >> TX39_MEMCONFIG4_RFSHVAL0_SHIFT) &			\
	TX39_MEMCONFIG4_RFSHVAL0_MASK)
#define TX39_MEMCONFIG4_RFSHVAL0_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG4_RFSHVAL0_SHIFT) &		\
	(TX39_MEMCONFIG4_RFSHVAL0_MASK << TX39_MEMCONFIG4_RFSHVAL0_SHIFT)))

/*
 *	Memory Configuration 5 Register
 */
/* Address remap region 2 */
#define TX39_MEMCONFIG5_STARTVAL2_SHIFT	9
#define TX39_MEMCONFIG5_STARTVAL2_MASK	0x007fffff
#define TX39_MEMCONFIG5_STARTVAL2(cr)					\
	(((cr) >> TX39_MEMCONFIG5_STARTVAL2_SHIFT) &			\
	TX39_MEMCONFIG5_STARTVAL2_MASK)
#define TX39_MEMCONFIG5_STARTVAL2_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG5_STARTVAL2_SHIFT) &		\
	(TX39_MEMCONFIG5_STARTVAL2_MASK << TX39_MEMCONFIG5_STARTVAL2_SHIFT)))

#define TX39_MEMCONFIG5_MASK2_SHIFT	0
#define TX39_MEMCONFIG5_MASK2_MASK	0xf
#define TX39_MEMCONFIG5_MASK2(cr)					\
	(((cr) >> TX39_MEMCONFIG5_MASK2_SHIFT) &			\
	TX39_MEMCONFIG5_MASK2_MASK)
#define TX39_MEMCONFIG5_MASK2_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG5_MASK2_SHIFT) &		\
	(TX39_MEMCONFIG5_MASK2_MASK << TX39_MEMCONFIG5_MASK2_SHIFT)))

/*
 *	Memory Configuration 6 Register
 */
/* Address remap region 1 */
#define TX39_MEMCONFIG6_STARTVAL1_SHIFT	9
#define TX39_MEMCONFIG6_STARTVAL1_MASK	0x007fffff
#define TX39_MEMCONFIG6_STARTVAL1(cr)					\
	(((cr) >> TX39_MEMCONFIG6_STARTVAL1_SHIFT) &			\
	TX39_MEMCONFIG6_STARTVAL1_MASK)
#define TX39_MEMCONFIG6_STARTVAL1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG6_STARTVAL1_SHIFT) &		\
	(TX39_MEMCONFIG6_STARTVAL1_MASK << TX39_MEMCONFIG6_STARTVAL1_SHIFT)))

#define TX39_MEMCONFIG6_MASK1_SHIFT	0
#define TX39_MEMCONFIG6_MASK1_MASK	0xf
#define TX39_MEMCONFIG6_MASK1(cr)					\
	(((cr) >> TX39_MEMCONFIG6_MASK1_SHIFT) &			\
	TX39_MEMCONFIG6_MASK1_MASK)
#define TX39_MEMCONFIG6_MASK1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG6_MASK1_SHIFT) &		\
	(TX39_MEMCONFIG6_MASK1_MASK << TX39_MEMCONFIG6_MASK1_SHIFT)))

/*
 *	Memory Configuration 7 Register
 */
/* Address remap region 2 */
#define TX39_MEMCONFIG7_RMAPADD2_SHIFT	9
#define TX39_MEMCONFIG7_RMAPADD2_MASK	0x007fffff
#define TX39_MEMCONFIG7_RMAPADD2(cr)					\
	(((cr) >> TX39_MEMCONFIG7_RMAPADD2_SHIFT) &			\
	TX39_MEMCONFIG7_RMAPADD2_MASK)
#define TX39_MEMCONFIG7_RMAPADD2_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG7_RMAPADD2_SHIFT) &		\
	(TX39_MEMCONFIG7_RMAPADD2_MASK << TX39_MEMCONFIG7_RMAPADD2_SHIFT)))

/*
 *	Memory Configuration 8 Register
 */
/* Address remap region 1 */
#define TX39_MEMCONFIG8_RMAPADD1_SHIFT	9
#define TX39_MEMCONFIG8_RMAPADD1_MASK	0x007fffff
#define TX39_MEMCONFIG8_RMAPADD1(cr)					\
	(((cr) >> TX39_MEMCONFIG8_RMAPADD1_SHIFT) &			\
	TX39_MEMCONFIG8_RMAPADD1_MASK)
#define TX39_MEMCONFIG8_RMAPADD1_SET(cr, val)				\
	((cr) | (((val) << TX39_MEMCONFIG8_RMAPADD1_SHIFT) &		\
	(TX39_MEMCONFIG8_RMAPADD1_MASK << TX39_MEMCONFIG8_RMAPADD1_SHIFT)))
