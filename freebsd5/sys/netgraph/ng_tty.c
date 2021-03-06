
/*
 * ng_tty.c
 *
 * Copyright (c) 1996-1999 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 *
 * $FreeBSD: src/sys/netgraph/ng_tty.c,v 1.25 2003/02/19 05:47:32 imp Exp $
 * $Whistle: ng_tty.c,v 1.21 1999/11/01 09:24:52 julian Exp $
 */

/*
 * This file implements a terminal line discipline that is also a
 * netgraph node. Installing this line discipline on a terminal device
 * instantiates a new netgraph node of this type, which allows access
 * to the device via the "hook" hook of the node.
 *
 * Once the line discipline is installed, you can find out the name
 * of the corresponding netgraph node via a NGIOCGINFO ioctl().
 *
 * Incoming characters are delievered to the hook one at a time, each
 * in its own mbuf. You may optionally define a ``hotchar,'' which causes
 * incoming characters to be buffered up until either the hotchar is
 * seen or the mbuf is full (MHLEN bytes). Then all buffered characters
 * are immediately delivered.
 *
 * NOTE: This node operates at spltty().
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/ttycom.h>
#include <sys/syslog.h>
#include <sys/errno.h>
#include <sys/ioccom.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_tty.h>

/* Misc defs */
#define MAX_MBUFQ		3	/* Max number of queued mbufs */
#define NGT_HIWATER		400	/* High water mark on output */

/* Per-node private info */
struct ngt_sc {
	struct	tty *tp;		/* Terminal device */
	node_p	node;			/* Netgraph node */
	hook_p	hook;			/* Netgraph hook */
	struct	mbuf *m;		/* Incoming data buffer */
	struct	mbuf *qhead, **qtail;	/* Queue of outgoing mbuf's */
	short	qlen;			/* Length of queue */
	short	hotchar;		/* Hotchar, or -1 if none */
	u_int	flags;			/* Flags */
	struct	callout_handle chand;	/* See man timeout(9) */
};
typedef struct ngt_sc *sc_p;

/* Flags */
#define FLG_TIMEOUT		0x0001	/* A timeout is pending */
#define FLG_DEBUG		0x0002

/* Debugging */
#ifdef INVARIANTS
#define QUEUECHECK(sc)							\
    do {								\
      struct mbuf	**mp;						\
      int		k;						\
									\
      for (k = 0, mp = &sc->qhead;					\
	k <= MAX_MBUFQ && *mp;						\
	k++, mp = &(*mp)->m_nextpkt);					\
      if (k != sc->qlen || k > MAX_MBUFQ || *mp || mp != sc->qtail)	\
	panic("%s: queue", __func__);					\
    } while (0)
#else
#define QUEUECHECK(sc)	do {} while (0)
#endif

/* Line discipline methods */
static int	ngt_open(dev_t dev, struct tty *tp);
static int	ngt_close(struct tty *tp, int flag);
static int	ngt_read(struct tty *tp, struct uio *uio, int flag);
static int	ngt_write(struct tty *tp, struct uio *uio, int flag);
static int	ngt_tioctl(struct tty *tp,
		    u_long cmd, caddr_t data, int flag, struct thread *);
static int	ngt_input(int c, struct tty *tp);
static int	ngt_start(struct tty *tp);

/* Netgraph methods */
static ng_constructor_t	ngt_constructor;
static ng_rcvmsg_t	ngt_rcvmsg;
static ng_shutdown_t	ngt_shutdown;
static ng_newhook_t	ngt_newhook;
static ng_connect_t	ngt_connect;
static ng_rcvdata_t	ngt_rcvdata;
static ng_disconnect_t	ngt_disconnect;
static int	ngt_mod_event(module_t mod, int event, void *data);

/* Other stuff */
static void	ngt_timeout(void *arg);

#define ERROUT(x)		do { error = (x); goto done; } while (0)

/* Line discipline descriptor */
static struct linesw ngt_disc = {
	ngt_open,
	ngt_close,
	ngt_read,
	ngt_write,
	ngt_tioctl,
	ngt_input,
	ngt_start,
	ttymodem,
	NG_TTY_DFL_HOTCHAR	/* XXX can't change this in serial driver */
};

/* Netgraph node type descriptor */
static struct ng_type typestruct = {
	NG_ABI_VERSION,
	NG_TTY_NODE_TYPE,
	ngt_mod_event,
	ngt_constructor,
	ngt_rcvmsg,
	ngt_shutdown,
	ngt_newhook,
	NULL,
	ngt_connect,
	ngt_rcvdata,
	ngt_disconnect,
	NULL
};
NETGRAPH_INIT(tty, &typestruct);

static int ngt_unit;
static int ngt_nodeop_ok;	/* OK to create/remove node */
static int ngt_ldisc;

/******************************************************************
		    LINE DISCIPLINE METHODS
******************************************************************/

/*
 * Set our line discipline on the tty.
 * Called from device open routine or ttioctl() at >= splsofttty()
 */
static int
ngt_open(dev_t dev, struct tty *tp)
{
	struct thread *const td = curthread;	/* XXX */
	char name[sizeof(NG_TTY_NODE_TYPE) + 8];
	sc_p sc;
	int s, error;

	/* Super-user only */
	if ((error = suser(td)))
		return (error);
	s = splnet();
	(void) spltty();	/* XXX is this necessary? */

	/* Already installed? */
	if (tp->t_line == NETGRAPHDISC) {
		sc = (sc_p) tp->t_sc;
		if (sc != NULL && sc->tp == tp)
			goto done;
	}

	/* Initialize private struct */
	MALLOC(sc, sc_p, sizeof(*sc), M_NETGRAPH, M_WAITOK | M_ZERO);
	if (sc == NULL) {
		error = ENOMEM;
		goto done;
	}
	sc->tp = tp;
	sc->hotchar = NG_TTY_DFL_HOTCHAR;
	sc->qtail = &sc->qhead;
	QUEUECHECK(sc);
	callout_handle_init(&sc->chand);

	/* Setup netgraph node */
	ngt_nodeop_ok = 1;
	error = ng_make_node_common(&typestruct, &sc->node);
	ngt_nodeop_ok = 0;
	if (error) {
		FREE(sc, M_NETGRAPH);
		goto done;
	}
	snprintf(name, sizeof(name), "%s%d", typestruct.name, ngt_unit++);

	/* Assign node its name */
	if ((error = ng_name_node(sc->node, name))) {
		log(LOG_ERR, "%s: node name exists?\n", name);
		ngt_nodeop_ok = 1;
		NG_NODE_UNREF(sc->node);
		ngt_nodeop_ok = 0;
		goto done;
	}

	/* Set back pointers */
	NG_NODE_SET_PRIVATE(sc->node, sc);
	tp->t_sc = (caddr_t) sc;

	/*
	 * Pre-allocate cblocks to the an appropriate amount.
	 * I'm not sure what is appropriate.
	 */
	ttyflush(tp, FREAD | FWRITE);
	clist_alloc_cblocks(&tp->t_canq, 0, 0);
	clist_alloc_cblocks(&tp->t_rawq, 0, 0);
	clist_alloc_cblocks(&tp->t_outq,
	    MLEN + NGT_HIWATER, MLEN + NGT_HIWATER);

done:
	/* Done */
	splx(s);
	return (error);
}

/*
 * Line specific close routine, called from device close routine
 * and from ttioctl at >= splsofttty(). This causes the node to
 * be destroyed as well.
 */
static int
ngt_close(struct tty *tp, int flag)
{
	const sc_p sc = (sc_p) tp->t_sc;
	int s;

	s = spltty();
	ttyflush(tp, FREAD | FWRITE);
	clist_free_cblocks(&tp->t_outq);
	tp->t_line = 0;
	if (sc != NULL) {
		if (sc->flags & FLG_TIMEOUT) {
			untimeout(ngt_timeout, sc, sc->chand);
			sc->flags &= ~FLG_TIMEOUT;
		}
		ngt_nodeop_ok = 1;
		ng_rmnode_self(sc->node);
		ngt_nodeop_ok = 0;
		tp->t_sc = NULL;
	}
	splx(s);
	return (0);
}

/*
 * Once the device has been turned into a node, we don't allow reading.
 */
static int
ngt_read(struct tty *tp, struct uio *uio, int flag)
{
	return (EIO);
}

/*
 * Once the device has been turned into a node, we don't allow writing.
 */
static int
ngt_write(struct tty *tp, struct uio *uio, int flag)
{
	return (EIO);
}

/*
 * We implement the NGIOCGINFO ioctl() defined in ng_message.h.
 */
static int
ngt_tioctl(struct tty *tp, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	const sc_p sc = (sc_p) tp->t_sc;
	int s, error = 0;

	s = spltty();
	switch (cmd) {
	case NGIOCGINFO:
	    {
		struct nodeinfo *const ni = (struct nodeinfo *) data;
		const node_p node = sc->node;

		bzero(ni, sizeof(*ni));
		if (NG_NODE_HAS_NAME(node))
			strncpy(ni->name, NG_NODE_NAME(node), sizeof(ni->name) - 1);
		strncpy(ni->type, node->nd_type->name, sizeof(ni->type) - 1);
		ni->id = (u_int32_t) ng_node2ID(node);
		ni->hooks = NG_NODE_NUMHOOKS(node);
		break;
	    }
	default:
		ERROUT(ENOIOCTL);
	}
done:
	splx(s);
	return (error);
}

/*
 * Receive data coming from the device. We get one character at
 * a time, which is kindof silly.
 * Only guaranteed to be at splsofttty() or spltty().
 */
static int
ngt_input(int c, struct tty *tp)
{
	const sc_p sc = (sc_p) tp->t_sc;
	const node_p node = sc->node;
	struct mbuf *m;
	int s, error = 0;

	if (!sc || tp != sc->tp)
		return (0);
	s = spltty();
	if (!sc->hook)
		ERROUT(0);

	/* Check for error conditions */
	if ((tp->t_state & TS_CONNECTED) == 0) {
		if (sc->flags & FLG_DEBUG)
			log(LOG_DEBUG, "%s: no carrier\n", NG_NODE_NAME(node));
		ERROUT(0);
	}
	if (c & TTY_ERRORMASK) {
		/* framing error or overrun on this char */
		if (sc->flags & FLG_DEBUG)
			log(LOG_DEBUG, "%s: line error %x\n",
			    NG_NODE_NAME(node), c & TTY_ERRORMASK);
		ERROUT(0);
	}
	c &= TTY_CHARMASK;

	/* Get a new header mbuf if we need one */
	if (!(m = sc->m)) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (!m) {
			if (sc->flags & FLG_DEBUG)
				log(LOG_ERR,
				    "%s: can't get mbuf\n", NG_NODE_NAME(node));
			ERROUT(ENOBUFS);
		}
		m->m_len = m->m_pkthdr.len = 0;
		m->m_pkthdr.rcvif = NULL;
		sc->m = m;
	}

	/* Add char to mbuf */
	*mtod(m, u_char *) = c;
	m->m_data++;
	m->m_len++;
	m->m_pkthdr.len++;

	/* Ship off mbuf if it's time */
	if (sc->hotchar == -1 || c == sc->hotchar || m->m_len >= MHLEN) {
		m->m_data = m->m_pktdat;
		NG_SEND_DATA_ONLY(error, sc->hook, m);
		sc->m = NULL;
	}
done:
	splx(s);
	return (error);
}

/*
 * This is called when the device driver is ready for more output.
 * Called from tty system at splsofttty() or spltty().
 * Also call from ngt_rcv_data() when a new mbuf is available for output.
 */
static int
ngt_start(struct tty *tp)
{
	const sc_p sc = (sc_p) tp->t_sc;
	int s;

	s = spltty();
	while (tp->t_outq.c_cc < NGT_HIWATER) {	/* XXX 2.2 specific ? */
		struct mbuf *m = sc->qhead;

		/* Remove first mbuf from queue */
		if (!m)
			break;
		if ((sc->qhead = m->m_nextpkt) == NULL)
			sc->qtail = &sc->qhead;
		sc->qlen--;
		QUEUECHECK(sc);

		/* Send as much of it as possible */
		while (m) {
			int     sent;

			sent = m->m_len
			    - b_to_q(mtod(m, u_char *), m->m_len, &tp->t_outq);
			m->m_data += sent;
			m->m_len -= sent;
			if (m->m_len > 0)
				break;	/* device can't take no more */
			m = m_free(m);
		}

		/* Put remainder of mbuf chain (if any) back on queue */
		if (m) {
			m->m_nextpkt = sc->qhead;
			sc->qhead = m;
			if (sc->qtail == &sc->qhead)
				sc->qtail = &m->m_nextpkt;
			sc->qlen++;
			QUEUECHECK(sc);
			break;
		}
	}

	/* Call output process whether or not there is any output. We are
	 * being called in lieu of ttstart and must do what it would. */
	if (tp->t_oproc != NULL)
		(*tp->t_oproc) (tp);

	/* This timeout is needed for operation on a pseudo-tty, because the
	 * pty code doesn't call pppstart after it has drained the t_outq. */
	if (sc->qhead && (sc->flags & FLG_TIMEOUT) == 0) {
		sc->chand = timeout(ngt_timeout, sc, 1);
		sc->flags |= FLG_TIMEOUT;
	}
	splx(s);
	return (0);
}

/*
 * We still have data to output to the device, so try sending more.
 */
static void
ngt_timeout(void *arg)
{
	const sc_p sc = (sc_p) arg;
	int s;

	s = spltty();
	sc->flags &= ~FLG_TIMEOUT;
	ngt_start(sc->tp);
	splx(s);
}

/******************************************************************
		    NETGRAPH NODE METHODS
******************************************************************/

/*
 * Initialize a new node of this type.
 *
 * We only allow nodes to be created as a result of setting
 * the line discipline on a tty, so always return an error if not.
 */
static int
ngt_constructor(node_p node)
{
	return (EOPNOTSUPP);
}

/*
 * Add a new hook. There can only be one.
 */
static int
ngt_newhook(node_p node, hook_p hook, const char *name)
{
	const sc_p sc = NG_NODE_PRIVATE(node);
	int s, error = 0;

	if (strcmp(name, NG_TTY_HOOK))
		return (EINVAL);
	s = spltty();
	if (sc->hook)
		ERROUT(EISCONN);
	sc->hook = hook;
done:
	splx(s);
	return (error);
}

/*
 * Set the hooks into queueing mode (for outgoing packets)
 * Force single client at a time.
 */
static int
ngt_connect(hook_p hook)
{
	/*NG_HOOK_FORCE_WRITER(hook);
	NG_HOOK_FORCE_QUEUE(NG_HOOK_PEER(hook));*/
	return (0);
}

/*
 * Disconnect the hook
 */
static int
ngt_disconnect(hook_p hook)
{
	const sc_p sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	int s;

	s = spltty();
	if (hook != sc->hook)
		panic(__func__);
	sc->hook = NULL;
	m_freem(sc->m);
	sc->m = NULL;
	splx(s);
	return (0);
}

/*
 * Remove this node. The does the netgraph portion of the shutdown.
 * This should only be called indirectly from ngt_close().
 */
static int
ngt_shutdown(node_p node)
{
	const sc_p sc = NG_NODE_PRIVATE(node);

	if (!ngt_nodeop_ok)
		return (EOPNOTSUPP);
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(sc->node);
	m_freem(sc->qhead);
	m_freem(sc->m);
	bzero(sc, sizeof(*sc));
	FREE(sc, M_NETGRAPH);
	return (0);
}

/*
 * Receive incoming data from netgraph system. Put it on our
 * output queue and start output if necessary.
 */
static int
ngt_rcvdata(hook_p hook, item_p item)
{
	const sc_p sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	int s, error = 0;
	struct mbuf *m;

	if (hook != sc->hook)
		panic(__func__);

	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);
	s = spltty();
	if (sc->qlen >= MAX_MBUFQ)
		ERROUT(ENOBUFS);
	m->m_nextpkt = NULL;
	*sc->qtail = m;
	sc->qtail = &m->m_nextpkt;
	sc->qlen++;
	QUEUECHECK(sc);
	m = NULL;
	if (sc->qlen == 1)
		ngt_start(sc->tp);
done:
	splx(s);
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Receive control message
 */
static int
ngt_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const sc_p sc = NG_NODE_PRIVATE(node);
	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	NGI_GET_MSG(item, msg);
	switch (msg->header.typecookie) {
	case NGM_TTY_COOKIE:
		switch (msg->header.cmd) {
		case NGM_TTY_SET_HOTCHAR:
		    {
			int     hotchar;

			if (msg->header.arglen != sizeof(int))
				ERROUT(EINVAL);
			hotchar = *((int *) msg->data);
			if (hotchar != (u_char) hotchar && hotchar != -1)
				ERROUT(EINVAL);
			sc->hotchar = hotchar;	/* race condition is OK */
			break;
		    }
		case NGM_TTY_GET_HOTCHAR:
			NG_MKRESPONSE(resp, msg, sizeof(int), M_NOWAIT);
			if (!resp)
				ERROUT(ENOMEM);
			/* Race condition here is OK */
			*((int *) resp->data) = sc->hotchar;
			break;
		default:
			ERROUT(EINVAL);
		}
		break;
	default:
		ERROUT(EINVAL);
	}
done:
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

/******************************************************************
		    	INITIALIZATION
******************************************************************/

/*
 * Handle loading and unloading for this node type
 */
static int
ngt_mod_event(module_t mod, int event, void *data)
{
	/* struct ng_type *const type = data;*/
	int s, error = 0;

	switch (event) {
	case MOD_LOAD:

		/* Register line discipline */
		s = spltty();
		if ((ngt_ldisc = ldisc_register(NETGRAPHDISC, &ngt_disc)) < 0) {
			splx(s);
			log(LOG_ERR, "%s: can't register line discipline",
			    __func__);
			return (EIO);
		}
		splx(s);
		break;

	case MOD_UNLOAD:

		/* Unregister line discipline */
		s = spltty();
		ldisc_deregister(ngt_ldisc);
		splx(s);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

