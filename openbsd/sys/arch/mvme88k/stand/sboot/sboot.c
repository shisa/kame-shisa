/*	$OpenBSD: sboot.c,v 1.3 2003/06/04 16:36:15 deraadt Exp $	*/

/*
 * Copyright (c) 1995 Theo de Raadt
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1995 Charles D. Cranor and Seth Widoff
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
 *      This product includes software developed by Charles D. Cranor
 *	and Seth Widoff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include "sboot.h"

void
main()
{
	char    buf[128];

	buf[0] = '0';
	printf("\nsboot: MVME147 bootstrap program\n");
	while (1) {
		printf(">>> ");
		gets(buf);
		do_cmd(buf);
	}
	/* not reached */
}

/*
 * exit to rom
 */
void 
callrom()
{
	asm("trap #15; .word 0x0063");
}

/*
 * do_cmd: do a command
 */
void 
do_cmd(buf)
	char   *buf;
{
	switch (*buf) {
	case '\0':
		break;
	case 'a':
		if (rev_arp()) {
			printf("My ip address is: %d.%d.%d.%d\n", myip[0],
			    myip[1], myip[2], myip[3]);
			printf("Server ip address is: %d.%d.%d.%d\n", servip[0],
			    servip[1], servip[2], servip[3]);
		} else {
			printf("Failed.\n");
		}
		break;
	case 'q':
		printf("exiting to ROM\n");
		callrom();
		break;
	case 'f':
		if (do_get_file() == 1) {
			printf("Download Failed\n");
		} else {
			printf("Download was a success!\n");
		}
		break;
	case 'b':
		le_init();
		if (rev_arp()) {
			printf("client IP address %d.%d.%d.%d\n", myip[0],
			    myip[1], myip[2], myip[3]);
			printf("server IP address %d.%d.%d.%d\n", servip[0],
			    servip[1], servip[2], servip[3]);
		} else {
			printf("REVARP: Failed.\n");
			return;
		}
		if (do_get_file() == 1) {
			printf("Download Failed\n");
			return;
		} else {
			printf("received secondary boot program.\n");
		}
		if (*++buf == '\0')
			buf = "bsd";
		go(buf);
		break;
	case 'h':
	case '?':
		printf("valid commands\n");
		printf("a - send a RARP\n");
		printf("b - boot the system\n");
		printf("q - exit to ROM\n");
		printf("f - ftp the boot file\n");
		printf("g - execute the boot file\n");
		printf("h - help\n");
		printf("i - init LANCE enet chip\n");
		break;
	case 'i':
		le_init();
		break;
	case 'g':
		go(buf);
		break;
	default:
		printf("sboot: %s: Unknown command\n", buf);
	}
}

go(buf)
	char *buf;
{
	void (*entry)() = (void (*))LOAD_ADDR;

	printf("jumping to boot program at 0x%x.\n", entry);

	asm("clrl d0; clrl d1");	/* XXX network device */
	asm("movl %0, a3" : : "a" (buf) : "a3");
	asm("movl %0, a4" : : "a" (buf + strlen(buf)) : "a4");
	asm("jmp %0@" : : "a" (entry));
}
