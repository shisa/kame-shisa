/*	$NetBSD: setjmp.h,v 1.7 2002/03/05 14:17:16 simonb Exp $	*/

/*
 * mips/setjmp.h: machine dependent setjmp-related information.
 *
 * For the size of this, refer to <machine/signal.h> as this uses the
 * struct sigcontext to restore it.
 */

#include <machine/cdefs.h>	/* for API selection */

#if !defined(_MIPS_BSD_API) || _MIPS_BSD_API == _MIPS_BSD_API_LP32
#define _JBLEN 87		/* XXX Naively 84; 87 for compatibility */
#else
#define _JBLEN	120
#endif
