/*	$KAME: common.h,v 1.40 2004/11/28 11:29:36 jinmei Exp $	*/
/*
 * Copyright (C) 1998 and 1999 WIDE Project.
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

#define IN6_IFF_INVALID (IN6_IFF_ANYCAST|IN6_IFF_TENTATIVE|\
		IN6_IFF_DUPLICATED|IN6_IFF_DETACHED)

#ifdef HAVE_ANSI_FUNC
#define FNAME __func__
#elif defined (HAVE_GCC_FUNCTION)
#define FNAME __FUNCTION__
#else
#define FNAME ""
#endif

/* XXX: bsdi4 does not have TAILQ_EMPTY */
#ifndef TAILQ_EMPTY
#define	TAILQ_EMPTY(head) ((head)->tqh_first == NULL)
#endif

extern int foreground;
extern int debug_thresh;
extern char *device;

/* search option for dhcp6_find_listval() */
#define MATCHLIST_PREFIXLEN 0x1

/* common.c */
extern int dhcp6_copy_list __P((struct dhcp6_list *, struct dhcp6_list *));
extern void dhcp6_move_list __P((struct dhcp6_list *, struct dhcp6_list *));
extern void dhcp6_clear_list __P((struct dhcp6_list *));
extern void dhcp6_clear_listval __P((struct dhcp6_listval *));
extern struct dhcp6_listval *dhcp6_find_listval __P((struct dhcp6_list *,
    dhcp6_listval_type_t, void *, int));
extern struct dhcp6_listval *dhcp6_add_listval __P((struct dhcp6_list *,
    dhcp6_listval_type_t, void *, struct dhcp6_list *));
extern int dhcp6_vbuf_copy __P((struct dhcp6_vbuf *, struct dhcp6_vbuf *));
extern void dhcp6_vbuf_free __P((struct dhcp6_vbuf *));
extern int dhcp6_vbuf_cmp __P((struct dhcp6_vbuf *, struct dhcp6_vbuf *));
extern struct dhcp6_event *dhcp6_create_event __P((struct dhcp6_if *, int));
extern void dhcp6_remove_event __P((struct dhcp6_event *));
extern void dhcp6_remove_evdata __P((struct dhcp6_event *));
extern struct authparam *new_authparam __P((int, int, int));
extern struct authparam *copy_authparam __P((struct authparam *));
extern int dhcp6_auth_replaycheck __P((int, u_int64_t, u_int64_t));
extern int getifaddr __P((struct in6_addr *, char *, struct in6_addr *,
			  int, int, int));
extern int getifidfromaddr __P((struct in6_addr *, unsigned int *));
extern int transmit_sa __P((int, struct sockaddr *, char *, size_t));
extern long random_between __P((long, long));
extern int prefix6_mask __P((struct in6_addr *, int));
extern int sa6_plen2mask __P((struct sockaddr_in6 *, int));
extern char *addr2str __P((struct sockaddr *));
extern char *in6addr2str __P((struct in6_addr *, int));
extern int in6_addrscopebyif __P((struct in6_addr *, char *));
extern int in6_scope __P((struct in6_addr *));
extern void setloglevel __P((int));
extern void dprintf __P((int, const char *, const char *, ...));
extern int get_duid __P((char *, struct duid *));
extern void dhcp6_init_options __P((struct dhcp6_optinfo *));
extern void dhcp6_clear_options __P((struct dhcp6_optinfo *));
extern int dhcp6_copy_options __P((struct dhcp6_optinfo *,
				   struct dhcp6_optinfo *));
extern int dhcp6_get_options __P((struct dhcp6opt *, struct dhcp6opt *,
				  struct dhcp6_optinfo *));
extern int dhcp6_set_options __P((int, struct dhcp6opt *, struct dhcp6opt *,
				  struct dhcp6_optinfo *));
extern void dhcp6_set_timeoparam __P((struct dhcp6_event *));
extern void dhcp6_reset_timer __P((struct dhcp6_event *));
extern char *dhcp6optstr __P((int));
extern char *dhcp6msgstr __P((int));
extern char *dhcp6_stcodestr __P((u_int16_t));
extern char *duidstr __P((struct duid *));
extern char *dhcp6_event_statestr __P((struct dhcp6_event *));
extern int get_rdvalue __P((int, void *, size_t));
extern int duidcpy __P((struct duid *, struct duid *));
extern int duidcmp __P((struct duid *, struct duid *));
extern void duidfree __P((struct duid *));

/* missing */
#ifndef HAVE_STRLCAT
extern size_t strlcat __P((char *, const char *, size_t));
#endif
#ifndef HAVE_STRLCPY
extern size_t strlcpy __P((char *, const char *, size_t));
#endif
