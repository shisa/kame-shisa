/*	$NetBSD: netdb.h,v 1.25.2.2 2002/08/27 09:29:24 lukem Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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

/*-
 * Copyright (c) 1980, 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *      @(#)netdb.h	8.1 (Berkeley) 6/2/93
 *	Id: netdb.h,v 4.9.1.2 1993/05/17 09:59:01 vixie Exp
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#ifndef _NETDB_H_
#define _NETDB_H_

#include <machine/ansi.h>
#include <sys/ansi.h>
#include <sys/cdefs.h>
#include <inttypes.h>

/*
 * Data types
 */
#ifndef socklen_t
typedef __socklen_t	socklen_t;
#define socklen_t	__socklen_t
#endif

#ifdef  _BSD_SIZE_T_
typedef _BSD_SIZE_T_	size_t;
#undef  _BSD_SIZE_T_
#endif

#if !defined(_XOPEN_SOURCE)
#define	_PATH_HEQUIV	"/etc/hosts.equiv"
#define	_PATH_HOSTS	"/etc/hosts"
#define	_PATH_NETWORKS	"/etc/networks"
#define	_PATH_PROTOCOLS	"/etc/protocols"
#define	_PATH_SERVICES	"/etc/services"
#endif

extern int h_errno;

/*
 * Structures returned by network data base library.  All addresses are
 * supplied in host order, and returned in network order (suitable for
 * use in system calls).
 */
struct	hostent {
	char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

/*
 * Assumption here is that a network number
 * fits in an unsigned long -- probably a poor one.
 */
struct	netent {
	char		*n_name;	/* official name of net */
	char		**n_aliases;	/* alias list */
	int		n_addrtype;	/* net address type */
	unsigned long	n_net;		/* network # XXX */
};

struct	servent {
	char	*s_name;	/* official service name */
	char	**s_aliases;	/* alias list */
	int	s_port;		/* port # */
	char	*s_proto;	/* protocol to use */
};

struct	protoent {
	char	*p_name;	/* official protocol name */
	char	**p_aliases;	/* alias list */
	int	p_proto;	/* protocol # */
};

/*
 * Note: ai_addrlen used to be a size_t, per RFC 2553.
 * In XNS5.2, and subsequently in POSIX-2001 and
 * draft-ietf-ipngwg-rfc2553bis-02.txt it was changed to a socklen_t.
 * To accomodate for this while preserving binary compatibility with the
 * old interface, we prepend or append 32 bits of padding, depending on
 * the (LP64) architecture's endianness.
 *
 * This should be deleted the next time the libc major number is
 * incremented.
 */
#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE - 0) >= 520
struct addrinfo {
	int	ai_flags;	/* AI_xxx */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
#if defined(__sparc64__)
	int	__ai_pad0;
#endif
	socklen_t ai_addrlen;	/* length of ai_addr */
#if defined(__alpha__) || (defined(__i386__) && defined(_LP64))
	int	__ai_pad0;
#endif
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};
#endif

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (left in extern int h_errno).
 */

#if !defined(_XOPEN_SOURCE)
#define	NETDB_INTERNAL	-1	/* see errno */
#define	NETDB_SUCCESS	0	/* no problem */
#endif
#define	HOST_NOT_FOUND	1 /* Authoritative Answer Host not found */
#define	TRY_AGAIN	2 /* Non-Authoritative Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /* Valid name, no data record of requested type */
#if !defined(_XOPEN_SOURCE)
#define	NO_ADDRESS	NO_DATA		/* no address, look for MX record */
#endif

/*
 * Error return codes from getaddrinfo()
 */
#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE - 0) >= 520
/* #define	EAI_ADDRFAMILY	 1	(obsoleted) */
#define	EAI_AGAIN	 2	/* temporary failure in name resolution */
#define	EAI_BADFLAGS	 3	/* invalid value for ai_flags */
#define	EAI_FAIL	 4	/* non-recoverable failure in name resolution */
#define	EAI_FAMILY	 5	/* ai_family not supported */
#define	EAI_MEMORY	 6	/* memory allocation failure */
/* #define	EAI_NODATA	 7	(obsoleted) */
#define	EAI_NONAME	 8	/* hostname nor servname provided, or not known */
#define	EAI_SERVICE	 9	/* servname not supported for ai_socktype */
#define	EAI_SOCKTYPE	10	/* ai_socktype not supported */
#define	EAI_SYSTEM	11	/* system error returned in errno */
#define	EAI_BADHINTS	12	/* Invalid value for hints */
#define	EAI_PROTOCOL	13	/* Resolved protocol is unknown */
#define	EAI_OVERFLOW	14	/* Argument buffer overflow */
#define	EAI_MAX		15
#endif /* !_XOPEN_SOURCE || (_XOPEN_SOURCE - 0) >= 520 */

/*
 * Flag values for getaddrinfo()
 */
#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE - 0) >= 520
#define	AI_PASSIVE	0x00000001 /* get address to use bind() */
#define	AI_CANONNAME	0x00000002 /* fill ai_canonname */
#define	AI_NUMERICHOST	0x00000004 /* prevent name resolution */
#define	AI_NUMERICSERV	0x00000008 /* prevent service name resolution */
#define	AI_ADDRCONFIG	0x00000010 /* only if any address is assigned */
/* valid flags for addrinfo (not a standard def, apps should not use it) */
#define	AI_MASK \
    (AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV | \
	AI_ADDRCONFIG)
#endif

#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE - 0) >= 520
/*
 * Constants for getnameinfo()
 */
#if !defined(_XOPEN_SOURCE)
#define	NI_MAXHOST	1025
#define	NI_MAXSERV	32
#endif

/*
 * Flag values for getnameinfo()
 */
#define	NI_NOFQDN	0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD	0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM	0x00000010
#if 0 /* obsolete */
#define NI_WITHSCOPEID	0x00000020	/*KAME extension*/
#endif
#define NI_NUMERICSCOPE	0x00000040

/*
 * Scope delimit character
 */
#if !defined(_XOPEN_SOURCE)
#define	SCOPE_DELIMITER '%'		/*KAME extension*/
#endif
#endif /* !_XOPEN_SOURCE || (_XOPEN_SOURCE - 0) >= 520 */

/*
 * Flags for getrrsetbyname()
 */
#define RRSET_VALIDATED		1

/*
 * Return codes for getrrsetbyname()
 */
#define ERRSET_SUCCESS		0
#define ERRSET_NOMEMORY		1
#define ERRSET_FAIL		2
#define ERRSET_INVAL		3
#define ERRSET_NONAME		4
#define ERRSET_NODATA		5

/*
 * Structures used by getrrsetbyname() and freerrset()
 */
struct rdatainfo {
	unsigned int		rdi_length;	/* length of data */
	unsigned char		*rdi_data;	/* record data */
};

struct rrsetinfo {
	unsigned int		rri_flags;	/* RRSET_VALIDATED ... */
	unsigned int		rri_rdclass;	/* class number */
	unsigned int		rri_rdtype;	/* RR type number */
	unsigned int		rri_ttl;	/* time to live */
	unsigned int		rri_nrdatas;	/* size of rdatas array */
	unsigned int		rri_nsigs;	/* size of sigs array */
	char			*rri_name;	/* canonical name */
	struct rdatainfo	*rri_rdatas;	/* individual records */
	struct rdatainfo	*rri_sigs;	/* individual signatures */
};

__BEGIN_DECLS
void		endhostent __P((void));
void		endnetent __P((void));
void		endprotoent __P((void));
void		endservent __P((void));
#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE - 0) >= 500
void		freehostent __P((struct hostent *));
#endif
struct hostent	*gethostbyaddr __P((const char *, socklen_t, int));
struct hostent	*gethostbyname __P((const char *));
#if !defined(_XOPEN_SOURCE)
struct hostent	*gethostbyname2 __P((const char *, int));
#endif
struct hostent	*gethostent __P((void));
#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE - 0) >= 520
#if 0 /* we do not ship these */
struct hostent	*getipnodebyaddr __P((const void *, size_t, int, int *));
struct hostent	*getipnodebyname __P((const char *, int, int, int *));
#endif
#endif
struct netent	*getnetbyaddr __P((unsigned long, int));
struct netent	*getnetbyname __P((const char *));
struct netent	*getnetent __P((void));
struct protoent	*getprotobyname __P((const char *));
struct protoent	*getprotobynumber __P((int));
struct protoent	*getprotoent __P((void));
struct servent	*getservbyname __P((const char *, const char *));
struct servent	*getservbyport __P((int, const char *));
struct servent	*getservent __P((void));
#if !defined(_XOPEN_SOURCE)
void		herror __P((const char *));
const char	*hstrerror __P((int));
#endif
void		sethostent __P((int));
#if !defined(_XOPEN_SOURCE)
/* void		sethostfile __P((const char *)); */
#endif
void		setnetent __P((int));
void		setprotoent __P((int));
#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE - 0) >= 520
int		getaddrinfo __P((const char *, const char *,
				 const struct addrinfo *, struct addrinfo **));
int		getnameinfo __P((const struct sockaddr *, socklen_t, char *,
				 socklen_t, char *, socklen_t, int));
void		freeaddrinfo __P((struct addrinfo *));
char		*gai_strerror __P((int));
#endif
void		setservent __P((int));
int		getrrsetbyname __P((const char *, unsigned int, unsigned int,
			unsigned int, struct rrsetinfo **));
void		freerrset __P((struct rrsetinfo *));
__END_DECLS

#endif /* !_NETDB_H_ */
