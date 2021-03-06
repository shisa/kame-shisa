/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/hea/eni_globals.c,v 1.10 2003/08/24 17:46:08 obrien Exp $");

/*
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Global variable definitions
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>

#include <vm/uma.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/dev/hea/eni_globals.c,v 1.10 2003/08/24 17:46:08 obrien Exp $");
#endif

/*
 * Device unit table
 */
Eni_unit	*eni_units[ENI_MAX_UNITS] = {NULL};

/*
 * ATM Interface services
 */
/*
 * AAL5 service stack
 */
static struct stack_defn	eni_svaal5 = {
	NULL,
	SAP_CPCS_AAL5,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};
/*
 * Efficient hardware doesn't support AAL3/4. Don't define
 * an AAL3/4 stack.
 */
/*
 * AAL0 service stack
 */
static struct stack_defn	eni_svaal0 = {
	&eni_svaal5,
	SAP_ATM,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};
struct stack_defn	*eni_services = &eni_svaal0;

/*
 * Storage pools
 */
uma_zone_t	eni_nif_zone;
uma_zone_t	eni_vcc_zone;
