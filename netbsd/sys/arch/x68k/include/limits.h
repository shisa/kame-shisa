/*	$NetBSD: limits.h,v 1.2 1998/01/09 22:24:08 perry Exp $	*/

#ifndef	_MACHINE_LIMITS_H_
#define	_MACHINE_LIMITS_H_

/* Just use the common m68k definition */
#include <m68k/limits.h>

#ifdef KERNEL
#define CLOCKS_PER_SEC 100
#endif

#endif /* _MACHINE_LIMITS_H_ */
