/*	$OpenBSD: bcmp.S,v 1.7 1998/03/01 16:10:22 niklas Exp $	*/

#include "DEFS.h"


/*
 * bcmp(s1, s2, n)
 */
LEAF(bcmp)
	.set	noreorder
	blt	a2, 16, smallcmp	# is it worth any trouble?
	xor	v0, a0, a1		# compare low two bits of addresses
	and	v0, v0, 3
	subu	a3, zero, a1		# compute # bytes to word align address
	bne	v0, zero, unalignedcmp	# not possible to align addresses
	and	a3, a3, 3

	beq	a3, zero, 1f
	subu	a2, a2, a3		# subtract from remaining count
	move	v0, v1			# init v0,v1 so unmodified bytes match
	LWHI	v0, 0(a0)		# read 1, 2, or 3 bytes
	LWHI	v1, 0(a1)
	addu	a1, a1, a3
	bne	v0, v1, nomatch
	addu	a0, a0, a3
1:
	and	a3, a2, ~3		# compute number of whole words left
	subu	a2, a2, a3		#   which has to be >= (16-3) & ~3
	addu	a3, a3, a0		# compute ending address
2:
	lw	v0, 0(a0)		# compare words
	lw	v1, 0(a1)
	addu	a0, a0, 4
	bne	v0, v1, nomatch
	addu	a1, a1, 4
	bne	a0, a3, 2b
	nop
	b	smallcmp		# finish remainder
	nop
unalignedcmp:
	beq	a3, zero, 2f
	subu	a2, a2, a3		# subtract from remaining count
	addu	a3, a3, a0		# compute ending address
1:
	lbu	v0, 0(a0)		# compare bytes until a1 word aligned
	lbu	v1, 0(a1)
	addu	a0, a0, 1
	bne	v0, v1, nomatch
	addu	a1, a1, 1
	bne	a0, a3, 1b
	nop
2:
	and	a3, a2, ~3		# compute number of whole words left
	subu	a2, a2, a3		#   which has to be >= (16-3) & ~3
	addu	a3, a3, a0		# compute ending address
3:
	LWHI	v0, 0(a0)		# compare words a0 unaligned, a1 aligned
	LWLO	v0, 3(a0)
	lw	v1, 0(a1)
	addu	a0, a0, 4
	bne	v0, v1, nomatch
	addu	a1, a1, 4
	bne	a0, a3, 3b
	nop
smallcmp:
	ble	a2, zero, match
	addu	a3, a2, a0		# compute ending address
1:
	lbu	v0, 0(a0)
	lbu	v1, 0(a1)
	addu	a0, a0, 1
	bne	v0, v1, nomatch
	addu	a1, a1, 1
	bne	a0, a3, 1b
	nop
match:
	j	ra
	 move	v0, zero
nomatch:
	j	ra
	li	v0, 1
END(bcmp)
