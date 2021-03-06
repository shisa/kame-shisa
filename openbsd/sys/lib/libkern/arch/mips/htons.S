/*	$OpenBSD: htons.S,v 1.5 1998/03/01 16:10:32 niklas Exp $	*/

#include "DEFS.h"


/*
 * netorder = htons(hostorder)
 * hostorder = ntohs(netorder)
 */
LEAF(htons)
ALEAF(ntohs)
	.set	noreorder
	srl	v0, a0, 8
	and	v0, v0, 0xff
	sll	v1, a0, 8
	and	v1, v1, 0xff00
	j	ra
	 or	v0, v0, v1
END(htons)
