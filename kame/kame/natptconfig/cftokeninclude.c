/*	$KAME: cftokeninclude.c,v 1.1 2001/09/02 19:28:34 fujisawa Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000 and 2001 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

void
switchToBuffer(char *Wow)
{
	YY_BUFFER_STATE	yyb;

	yyb = yy_scan_string(Wow);
	yy_switch_to_buffer(yyb);
}


void
reassembleCommandLine(int argc, char *argv[])
{
	const char *fn = __FUNCTION__;

	YY_BUFFER_STATE	 yyb;
	char		*spc, *s, *d;
	char		*kk;
	char		 Wow[BUFSIZ+GETA];

	kk = Wow + BUFSIZ;
	bzero(Wow, sizeof(Wow));
	for (d = Wow; argc; argc--, argv++) {
		s = *(argv);
		if ((spc = strchr(s, ' ')) == NULL)
			spc = strchr(s, '\t');

		if (spc)	*d++ = '"';
		while (*s)	*d++ = *s++;
		if (spc)	*d++ = '"';

		*d++ = ' ';
		if (d >= kk)
			errx(1, "%s: command lien too long", fn);
	}

	yyb = yy_scan_string(Wow);
	yy_switch_to_buffer(yyb);
}


int
getDecimal(char *yytext)
{
	yylval.UInt = strtol(yytext, NULL, 0);
	return (SDECIMAL);
}


int
getHexadecimal(char *yytext)
{
	yylval.UInt = strtoul(yytext, NULL, 0);
	return (SHEXADECIMAL);
}


int
getDQuoteString(char *yytext)
{
	int	 ch;
	char	*p = yytext;

	for (;;) {
		switch (ch = input()) {
		case 0:		return (SDQUOTE);
		case '\"':	*p = 0; return (SSTRING);
		default:	*p++ = ch;
		}
	}

	return (SSTRING);
}
