/*	$NetBSD: null.h,v 1.4 2000/05/19 18:57:48 thorpej Exp $	*/

#ifndef	NULL
#if !defined(__GNUG__) || __GNUG__ < 2 || (__GNUG__ == 2 && __GNUC_MINOR__ < 90)
#if !defined(__cplusplus)
#define	NULL	(void *)0
#else
#define	NULL	0
#endif /* __AUDIT__ && !__cplusplus */
#else
#define	NULL	__null
#endif
#endif
