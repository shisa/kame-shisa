/*	$NetBSD: if_ieee80211.h,v 1.12 2001/09/19 04:09:56 onoe Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
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

#ifndef _NET_IF_IEEE80211_H_
#define _NET_IF_IEEE80211_H_

#include <net/if_ether.h>

#define	IEEE80211_ADDR_LEN			ETHER_ADDR_LEN

/*
 * generic definitions for IEEE 802.11 frames
 */
struct ieee80211_frame {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr3[IEEE80211_ADDR_LEN];
	u_int8_t	i_seq[2];
	/* possibly followed by addr4[IEEE80211_ADDR_LEN]; */
	/* see below */
} __attribute__((__packed__));

struct ieee80211_frame_addr4 {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr3[IEEE80211_ADDR_LEN];
	u_int8_t	i_seq[2];
	u_int8_t	i_addr4[IEEE80211_ADDR_LEN];
} __attribute__((__packed__));

#define	IEEE80211_FC0_VERSION_MASK		0x03
#define	IEEE80211_FC0_VERSION_0			0x00
#define	IEEE80211_FC0_TYPE_MASK			0x0c
#define	IEEE80211_FC0_TYPE_MGT			0x00
#define	IEEE80211_FC0_TYPE_CTL			0x04
#define	IEEE80211_FC0_TYPE_DATA			0x08

#define	IEEE80211_FC0_SUBTYPE_MASK		0xf0
/* for TYPE_MGT */
#define	IEEE80211_FC0_SUBTYPE_ASSOC_REQ		0x00
#define	IEEE80211_FC0_SUBTYPE_ASSOC_RESP	0x10
#define	IEEE80211_FC0_SUBTYPE_REASSOC_REQ	0x20
#define	IEEE80211_FC0_SUBTYPE_REASSOC_RESP	0x30
#define	IEEE80211_FC0_SUBTYPE_PROBE_REQ		0x40
#define	IEEE80211_FC0_SUBTYPE_PROBE_RESP	0x50
#define	IEEE80211_FC0_SUBTYPE_BEACON		0x80
#define	IEEE80211_FC0_SUBTYPE_ATIM		0x90
#define	IEEE80211_FC0_SUBTYPE_DISASSOC		0xa0
#define	IEEE80211_FC0_SUBTYPE_AUTH		0xb0
#define	IEEE80211_FC0_SUBTYPE_DEAUTH		0xc0
/* for TYPE_CTL */
#define	IEEE80211_FC0_SUBTYPE_PS_POLL		0xa0
#define	IEEE80211_FC0_SUBTYPE_RTS		0xb0
#define	IEEE80211_FC0_SUBTYPE_CTS		0xc0
#define	IEEE80211_FC0_SUBTYPE_ACK		0xd0
#define	IEEE80211_FC0_SUBTYPE_CF_END		0xe0
#define	IEEE80211_FC0_SUBTYPE_CF_END_ACK	0xf0
/* for TYPE_DATA (bit combination) */
#define	IEEE80211_FC0_SUBTYPE_CF_ACK		0x10
#define	IEEE80211_FC0_SUBTYPE_CF_POLL		0x20
#define	IEEE80211_FC0_SUBTYPE_NODATA		0x40

#define	IEEE80211_FC1_DIR_MASK			0x03
#define	IEEE80211_FC1_DIR_NODS			0x00	/* STA->STA */
#define	IEEE80211_FC1_DIR_TODS			0x01	/* STA->AP  */
#define	IEEE80211_FC1_DIR_FROMDS		0x02	/* AP ->STA */
#define	IEEE80211_FC1_DIR_DSTODS		0x03	/* AP ->AP  */

#define	IEEE80211_FC1_MORE_FRAG			0x04
#define	IEEE80211_FC1_RETRY			0x08
#define	IEEE80211_FC1_PWR_MGT			0x10
#define	IEEE80211_FC1_MORE_DATA			0x20
#define	IEEE80211_FC1_WEP			0x40
#define	IEEE80211_FC1_ORDER			0x80

#define	IEEE80211_SEQ_FRAG_MASK			0x000f
#define	IEEE80211_SEQ_FRAG_SHIFT		0
#define	IEEE80211_SEQ_SEQ_MASK			0xfff0
#define	IEEE80211_SEQ_SEQ_SHIFT			4

/*
 * Management Frames
 */

#define	IEEE80211_ELEMID_SSID			0
#define	IEEE80211_ELEMID_RATES			1
#define	IEEE80211_ELEMID_FHPARMS		2
#define	IEEE80211_ELEMID_DSPARMS		3
#define	IEEE80211_ELEMID_CFPARMS		4
#define	IEEE80211_ELEMID_TIM			5
#define	IEEE80211_ELEMID_IBSSPARMS		6
#define	IEEE80211_ELEMID_CHALLENGE		16

#define	IEEE80211_RATE_BASIC			0x80
#define	IEEE80211_RATE_VAL			0x7f

#define	IEEE80211_AUTH_ALG_OPEN			0x0000
#define	IEEE80211_AUTH_ALG_SHARED		0x0001

#define	IEEE80211_CAPINFO_ESS			0x0001
#define	IEEE80211_CAPINFO_IBSS			0x0002
#define	IEEE80211_CAPINFO_CF_POLLABLE		0x0004
#define	IEEE80211_CAPINFO_CF_POLLREQ		0x0008
#define	IEEE80211_CAPINFO_PRIVACY		0x0010
#define	IEEE80211_CAPINFO_SHORT_PREAMBLE	0x0020
#define	IEEE80211_CAPINFO_PBCC			0x0040
#define	IEEE80211_CAPINFO_CHNL_AGILITY		0x0080

#define	IEEE80211_REASON_UNSPECIFIED		1
#define	IEEE80211_REASON_AUTH_EXPIRE		2
#define	IEEE80211_REASON_AUTH_LEAVE		3
#define	IEEE80211_REASON_ASSOC_EXPIRE		4
#define	IEEE80211_REASON_ASSOC_TOOMANY		5
#define	IEEE80211_REASON_NOT_AUTHED		6  
#define	IEEE80211_REASON_NOT_ASSOCED		7
#define	IEEE80211_REASON_ASSOC_LEAVE		8
#define	IEEE80211_REASON_ASSOC_NOT_AUTHED	9

#define	IEEE80211_STATUS_SUCCESS		0
#define	IEEE80211_STATUS_UNSPECIFIED		1
#define	IEEE80211_STATUS_CAPINFO		10
#define	IEEE80211_STATUS_NOT_ASSOCED		11
#define	IEEE80211_STATUS_OTHER			12
#define	IEEE80211_STATUS_ALG			13
#define	IEEE80211_STATUS_SEQUENCE		14
#define	IEEE80211_STATUS_CHALLENGE		15
#define	IEEE80211_STATUS_TIMEOUT		16
#define	IEEE80211_STATUS_TOOMANY		17
#define	IEEE80211_STATUS_BASIC_RATE		18
#define	IEEE80211_STATUS_SP_REQUIRED		19
#define	IEEE80211_STATUS_PBCC_REQUIRED		20
#define	IEEE80211_STATUS_CA_REQUIRED		21

#define	IEEE80211_WEP_KEYLEN			5	/* 40bit */
#define	IEEE80211_WEP_IVLEN			3	/* 24bit */
#define	IEEE80211_WEP_KIDLEN			1	/* 1 octet */
#define	IEEE80211_WEP_CRCLEN			4	/* CRC-32 */
#define	IEEE80211_WEP_NKID			4	/* number of key ids */

#define	IEEE80211_NWID_LEN			32

#define	IEEE80211_CRC_LEN			4

#define	IEEE80211_MTU				1500
#define	IEEE80211_MAX_LEN			(2300 + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))

/*
 * ioctls
 */

/* nwid is pointed at by ifr.ifr_data */
struct ieee80211_nwid {
	u_int8_t	i_len;
	u_int8_t	i_nwid[IEEE80211_NWID_LEN];
};

#define	SIOCS80211NWID		_IOWR('i', 230, struct ifreq)
#define	SIOCG80211NWID		_IOWR('i', 231, struct ifreq)

/* the first member must be matched with struct ifreq */
struct ieee80211_nwkey {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	int		i_wepon;		/* wep enabled flag */
	int		i_defkid;		/* default encrypt key id */
	struct {
		int		i_keylen;
		u_int8_t	*i_keydat;
	}		i_key[IEEE80211_WEP_NKID];
};
#define	SIOCS80211NWKEY		 _IOW('i', 232, struct ieee80211_nwkey)
#define	SIOCG80211NWKEY		_IOWR('i', 233, struct ieee80211_nwkey)
/* i_wepon */
#define	IEEE80211_NWKEY_OPEN	0		/* No privacy */
#define	IEEE80211_NWKEY_WEP	1		/* WEP enabled */
#define	IEEE80211_NWKEY_EAP	2		/* EAP enabled */
#define	IEEE80211_NWKEY_PERSIST	0x100		/* designate persist keyset */

/* power management parameters */
struct ieee80211_power {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	int		i_enabled;		/* 1 == on, 0 == off */
	int		i_maxsleep;		/* max sleep in ms */
};
#define	SIOCS80211POWER		 _IOW('i', 234, struct ieee80211_power)
#define	SIOCG80211POWER		_IOWR('i', 235, struct ieee80211_power)


#ifdef _KERNEL

#define	IEEE80211_ASCAN_WAIT	2		/* active scan wait */
#define	IEEE80211_PSCAN_WAIT 	5		/* passive scan wait */
#define	IEEE80211_TRANS_WAIT 	5		/* transition wait */

/*
 * Structure for IEEE 802.11 drivers.
 */

#define	IEEE80211_CHAN_MAX	255
#define	IEEE80211_RATE_SIZE	12
#define	IEEE80211_KEYBUF_SIZE	16

enum ieee80211_state {
	IEEE80211_S_INIT,		/* default state */
	IEEE80211_S_SCAN,		/* scanning */
	IEEE80211_S_AUTH,		/* try to authenticate */
	IEEE80211_S_ASSOC,		/* try to assoc */
	IEEE80211_S_RUN			/* associated */
};

struct ieee80211_bss {
	TAILQ_ENTRY(ieee80211_bss)	bs_list;
	u_int8_t		bs_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t		bs_bssid[IEEE80211_ADDR_LEN];
	u_int8_t		bs_esslen;
	u_int8_t		bs_essid[IEEE80211_NWID_LEN];
	u_int8_t		bs_tstamp[8];
	u_int16_t		bs_intval;
	u_int16_t		bs_capinfo;
	u_int16_t		bs_fhdwell;
	u_int8_t		bs_fhindex;
	u_int16_t		bs_associd;
	u_int32_t		bs_timoff;
	u_int8_t		bs_chan;
	u_int8_t		bs_rssi;
	u_int8_t		bs_rates[IEEE80211_RATE_SIZE];
	u_int16_t		bs_txseq;
	u_int16_t		bs_rxseq;
	int			bs_nrate;
	int			bs_fails;
	int			bs_txrate;
};

/* bs_chan encoding for FH phy */
#define	IEEE80211_FH_CHANMOD	80
#define	IEEE80211_FH_CHAN(set,pat)	(((set)-1)*IEEE80211_FH_CHANMOD+(pat))
#define	IEEE80211_FH_CHANSET(chan)	((chan)/IEEE80211_FH_CHANMOD+1)
#define	IEEE80211_FH_CHANPAT(chan)	((chan)%IEEE80211_FH_CHANMOD)

struct ieee80211_wepkey {
	int			wk_len;
	u_int8_t		wk_key[IEEE80211_KEYBUF_SIZE];
};

struct ieee80211com {
	struct ethercom		ic_ec;
	int			(*ic_newstate)(void *, enum ieee80211_state);
	int			(*ic_chancheck)(void *, u_char *);
	u_int8_t		ic_myaddr[IEEE80211_ADDR_LEN];
	u_int8_t		ic_sup_rates[IEEE80211_RATE_SIZE];
	u_char			ic_chan_avail[(IEEE80211_CHAN_MAX+1)/NBBY];
	u_char			ic_chan_active[(IEEE80211_CHAN_MAX+1)/NBBY];
	struct ifqueue		ic_mgtq;
	int			ic_flags;
	enum ieee80211_state	ic_state;
	struct ieee80211_bss	ic_bss;
	u_int8_t		ic_ibss_chan;
	int			ic_fixed_rate;
	TAILQ_HEAD(, ieee80211_bss)	ic_scan;
	u_int16_t		ic_lintval;	/* listen interval */
	int			ic_mgt_timer;	/* mgmt timeout */
	int			ic_scan_timer;	/* scant wait */
	int			ic_des_esslen;
	u_int8_t		ic_des_essid[IEEE80211_NWID_LEN];
	struct ieee80211_wepkey	ic_nw_keys[IEEE80211_WEP_NKID];
	int			ic_wep_txkey;
	void			*ic_wep_ctx;
};
#define	ic_if		ic_ec.ec_if
#define	ic_softc	ic_ec.ec_if.if_softc

/* ic_flags */
#define	IEEE80211_F_ASCAN	0x00000001	/* STATUS: active scan */
#define	IEEE80211_F_SIBSS	0x00000002	/* STATUS: start IBSS */
#define	IEEE80211_F_WEPON	0x00000100	/* CONF: WEP enabled */
#define	IEEE80211_F_IBSSON	0x00000200	/* CONF: IBSS creation enable */
#define	IEEE80211_F_PMGTON	0x00000400	/* CONF: Power mgmt enable */
#define	IEEE80211_F_ADHOC	0x00000800	/* CONF: adhoc mode */
#define	IEEE80211_F_HASWEP	0x00010000	/* CAPABILITY: WEP available */
#define	IEEE80211_F_HASIBSS	0x00020000	/* CAPABILITY: IBSS available */
#define	IEEE80211_F_HASPMGT	0x00040000	/* CAPABILITY: Power mgmt */

/* flags for ieee80211_fix_rate */
#define	IEEE80211_F_DOSORT	0x00000001	/* sort rate list */
#define	IEEE80211_F_DOFRATE	0x00000002	/* use fixed rate */
#define	IEEE80211_F_DONEGO	0x00000004	/* calc negotiated rate */
#define	IEEE80211_F_DODEL	0x00000008	/* delete ignore rate */

void	ieee80211_ifattach(struct ifnet *);
void	ieee80211_ifdetach(struct ifnet *);
void	ieee80211_input(struct ifnet *, struct mbuf *, int, u_int);
int	ieee80211_mgmt_output(struct ifnet *, struct mbuf *, int);
struct mbuf *ieee80211_encap(struct ifnet *, struct mbuf *);
struct mbuf *ieee80211_decap(struct ifnet *, struct mbuf *);
int	ieee80211_ioctl(struct ifnet *, u_long, caddr_t);
void	ieee80211_print_essid(u_int8_t *, int);
void	ieee80211_dump_pkt(u_int8_t *, int, int, int);
void	ieee80211_watchdog(struct ifnet *);
void	ieee80211_next_scan(struct ifnet *);
void	ieee80211_end_scan(struct ifnet *);
void	ieee80211_free_scan(struct ifnet *);
int	ieee80211_fix_rate(struct ieee80211com *, struct ieee80211_bss *, int);
int	ieee80211_new_state(struct ifnet *, enum ieee80211_state, int);
struct mbuf *ieee80211_wep_crypt(struct ifnet *, struct mbuf *, int);

int	ieee80211_cfgget(struct ifnet *, u_long, caddr_t);
int	ieee80211_cfgset(struct ifnet *, u_long, caddr_t);

#endif /* _KERNEL */

#endif /* _NET_IF_IEEE80211_H_ */
