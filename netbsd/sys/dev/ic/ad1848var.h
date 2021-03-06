/*	$NetBSD: ad1848var.h,v 1.8 2001/11/04 08:08:26 itohy Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ken Hornstein and John Kohl.
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
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 *
 */


struct ad1848_volume {
	u_char	left;
	u_char	right;
};

struct ad1848_softc {
	struct	device sc_dev;		/* base device */
	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */

	int	(*sc_readreg)__P((struct ad1848_softc *, int));
	void	(*sc_writereg)__P((struct ad1848_softc *, int, int));
#define ADREAD(sc, index)		(*(sc)->sc_readreg)(sc, index)
#define ADWRITE(sc, index, data)	(*(sc)->sc_writereg)(sc, index, data)

	void	*parent;

	/* We keep track of these */
	struct ad1848_volume gains[6];
	struct ad1848_volume rec_gain;

	int	rec_port;		/* recording port */

	/* ad1848 */
	u_char	MCE_bit;
	char	mic_gain_on;		/* CS4231 only */
        char    mute[6];

	char	*chip_name;
	int	mode;
	int	is_ad1845;

	u_int	precision;		/* 8/16 bits */
	int	channels;

	u_char	speed_bits;
	u_char	format_bits;
	u_char	need_commit;

	u_char	wave_mute_status;
	int	open_mode;
};

#define MUTE_LEFT       1
#define MUTE_RIGHT      2
#define MUTE_ALL        (MUTE_LEFT | MUTE_RIGHT)
#define MUTE_MONO       MUTE_ALL

#define WAVE_MUTE0	1		/* force mute (overrides UNMUTE1) */
#define WAVE_UNMUTE1	2		/* unmute (overrides MUTE2) */
#define WAVE_MUTE2	4		/* weak mute */
#define WAVE_MUTE2_INIT	0		/* init and MUTE2 */

/*
 * Don't change this ordering without seriously looking around.
 * These are indexes into mute[] array and into a register
 * information array.
 */
#define AD1848_AUX2_CHANNEL	0
#define AD1848_AUX1_CHANNEL	1
#define AD1848_DAC_CHANNEL	2
#define AD1848_LINE_CHANNEL	3
#define AD1848_MONO_CHANNEL	4
#define AD1848_MONITOR_CHANNEL	5 /* Doesn't seem to be on all later chips */

/*
 * Ad1848 registers.
 */
#define MIC_IN_PORT	0
#define LINE_IN_PORT	1
#define AUX1_IN_PORT	2
#define DAC_IN_PORT	3

#define AD1848_KIND_LVL   0
#define AD1848_KIND_MUTE  1
#define AD1848_KIND_RECORDGAIN 2
#define AD1848_KIND_MICGAIN 3
#define AD1848_KIND_RECORDSOURCE 4

typedef struct ad1848_devmap {
	int  id;
	int  kind;
	int  dev;
} ad1848_devmap_t;

#ifdef _KERNEL

int	ad_read __P((struct ad1848_softc *, int));
int	ad_xread __P((struct ad1848_softc *, int));
void	ad_write __P((struct ad1848_softc *, int, int));
void	ad_xwrite __P((struct ad1848_softc *, int, int));

void	ad1848_attach __P((struct ad1848_softc *));
void	ad1848_reset __P((struct ad1848_softc *));
int	ad1848_open __P((void *, int));
void	ad1848_close __P((void *));

int	ad1848_mixer_get_port __P((struct ad1848_softc *, ad1848_devmap_t *,
				   int, mixer_ctrl_t *));
int	ad1848_mixer_set_port __P((struct ad1848_softc *, ad1848_devmap_t *,
				   int, mixer_ctrl_t *));
int	ad1848_set_speed __P((struct ad1848_softc *, u_long *));
void	ad1848_mute_wave_output __P((struct ad1848_softc *, int, int));
int	ad1848_query_encoding __P((void *, struct audio_encoding *));
int	ad1848_set_params __P((void *, int, int, struct audio_params *,
			       struct audio_params *));
int	ad1848_round_blocksize __P((void *, int));
int	ad1848_commit_settings __P((void *));
int	ad1848_set_rec_port __P((struct ad1848_softc *, int));
int	ad1848_get_rec_port __P((struct ad1848_softc *));
int	ad1848_set_channel_gain __P((struct ad1848_softc *, int,
				     struct ad1848_volume *));
int	ad1848_get_device_gain __P((struct ad1848_softc *, int,
				    struct ad1848_volume *));
int	ad1848_set_rec_gain __P((struct ad1848_softc *,
				 struct ad1848_volume *));
int	ad1848_get_rec_gain __P((struct ad1848_softc *,
				 struct ad1848_volume *));
/* Note: The mic pre-MUX gain is not a variable gain, it's 20dB or 0dB */
int	ad1848_set_mic_gain __P((struct ad1848_softc *,
				 struct ad1848_volume *));
int	ad1848_get_mic_gain __P((struct ad1848_softc *,
				 struct ad1848_volume *));
void	ad1848_mute_channel __P((struct ad1848_softc *, int device, int mute));
int	ad1848_to_vol __P((mixer_ctrl_t *, struct ad1848_volume *));
int	ad1848_from_vol __P((mixer_ctrl_t *, struct ad1848_volume *));

int	ad1848_halt_output __P((void *));
int	ad1848_halt_input __P((void *));
paddr_t	ad1848_mappage __P((void *, void *, off_t, int));

#ifdef AUDIO_DEBUG
void	ad1848_dump_regs __P((struct ad1848_softc *));
#endif

#endif
