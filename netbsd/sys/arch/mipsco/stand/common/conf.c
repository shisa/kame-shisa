/*	$NetBSD: conf.c,v 1.1 2000/09/18 11:40:48 wdk Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	@(#)conf.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/dev_net.h>
#include <lib/libsa/ufs.h>
#include <lib/libsa/lfs.h>
#include <lib/libsa/nfs.h>
#include <lib/libsa/cd9660.h>
#include <lib/libsa/ustarfs.h>
#include <machine/prom.h>
#include "../common/saio.h"

#ifndef LIBSA_SINGLE_DEVICE

#ifdef LIBSA_NO_DEV_CLOSE
#define saioclose /*(()(struct open_file*))*/0
#endif

#ifdef LIBSA_NO_DEV_IOCTL
#define saioioctl /*(()(struct open_file*, u_long, void*))*/0
#else
#define	saioioctl	noioctl
#endif

struct devsw devsw[] = {
	{ "dksd", saiostrategy, saioopen, saioclose, saioioctl },	/* 1 */
	{ "tqsd", saiostrategy, saioopen, saioclose, saioioctl },	/* 2 */
#ifdef BOOTNET
	{ "bootp", net_strategy, net_open, net_close, net_ioctl },	/* 3 */
#endif
};

int	ndevs = (sizeof(devsw)/sizeof(devsw[0]));
#endif


#ifndef LIBSA_SINGLE_FILESYSTEM
#ifdef LIBSA_NO_FS_CLOSE
#define ufs_close	0
#define lfs_close	0
#define cd9660_close	0
#define ustarfs_close	0
#define nfs_close	0
#endif
#ifdef LIBSA_NO_FS_WRITE
#define ufs_write	0
#define lfs_write	0
#define cd9660_write	0
#define ustarfs_write	0
#define nfs_write	0
#endif

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
	{ lfs_open, lfs_close, lfs_read, lfs_write, lfs_seek, lfs_stat },
	{ cd9660_open, cd9660_close, cd9660_read, cd9660_write, cd9660_seek,
	    cd9660_stat },
	{ ustarfs_open, ustarfs_close, ustarfs_read, ustarfs_write,
	    ustarfs_seek, ustarfs_stat },
#ifdef BOOTNET
	{ nfs_open, nfs_close, nfs_read, nfs_write, nfs_seek, nfs_stat },
#endif
};

int nfsys = sizeof(file_system)/sizeof(struct fs_ops);
#endif

#ifdef BOOTNET
extern struct netif_driver prom_netif_driver;

struct netif_driver *netif_drivers[] = {
	&prom_netif_driver,
};
int	n_netif_drivers = (sizeof(netif_drivers) / sizeof(netif_drivers[0]));
#endif


