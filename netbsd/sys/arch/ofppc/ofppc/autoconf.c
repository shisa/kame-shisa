/*	$NetBSD: autoconf.c,v 1.11 2001/08/26 02:47:40 matt Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/powerpc.h>

static void findroot __P((void));

struct device *booted_device;	/* boot device */
int booted_partition;		/* ...and partition on that device */

/*
 * Determine device configuration for a machine.
 */
void
cpu_configure()
{
	if (config_rootfound("mainbus", NULL) == NULL)
		panic("configure: mainbus not configured");

	(void)spl0();
}

/*
 * Setup root device.
 * Configure swap area.
 */
void
cpu_rootconf()
{

	findroot();

	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition);
}

/*
 * Try to find the device we were booted from to set rootdev.
 */
void
findroot()
{
	char *cp;

	/*
	 * Try to find the device where we were booted from.
	 */
	if (bootpath != NULL) {
		for (cp = bootpath + strlen(bootpath); --cp >= bootpath;) {
			if (*cp == '/') {
				*cp = '\0';
				if (!dk_match(bootpath)) {
					*cp = '/';
					break;
				}
				*cp = '/';
			}
		}
	}
	dk_cleanup();
}
