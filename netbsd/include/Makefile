#	$NetBSD: Makefile,v 1.104 2002/03/31 14:12:37 bjh21 Exp $
#	@(#)Makefile	8.2 (Berkeley) 1/4/94

# Doing a make includes builds /usr/include

# Missing: mp.h

INCS=	a.out.h ar.h assert.h bitstring.h bm.h cpio.h ctype.h db.h dirent.h \
	disktab.h dlfcn.h err.h errno.h fmtmsg.h fnmatch.h fstab.h fts.h \
	getopt.h glob.h grp.h ifaddrs.h \
	inttypes.h iso646.h kvm.h langinfo.h libgen.h \
	limits.h link.h link_aout.h link_elf.h locale.h \
	login_cap.h malloc.h math.h md2.h md4.h md5.h \
	memory.h mpool.h ndbm.h netconfig.h netdb.h netgroup.h nlist.h \
	nl_types.h \
	nsswitch.h paths.h pwd.h ranlib.h re_comp.h regex.h regexp.h \
	resolv.h rmd160.h rmt.h sched.h search.h setjmp.h sgtty.h sha1.h \
	signal.h stab.h stddef.h stdio.h stdlib.h string.h strings.h \
	stringlist.h struct.h sysexits.h tar.h time.h ttyent.h tzfile.h \
	ulimit.h unistd.h util.h utime.h utmp.h utmpx.h vis.h wchar.h wctype.h
INCS+=	arpa/ftp.h arpa/inet.h arpa/nameser.h arpa/telnet.h arpa/tftp.h
INCS+=	protocols/dumprestore.h protocols/routed.h protocols/rwhod.h \
	protocols/talkd.h protocols/timed.h

INCS+=	hesiod.h

INCS+=	rpcsvc/yp_prot.h rpcsvc/ypclnt.h

.if (${MACHINE_ARCH} != "vax")
INCS+=	ieeefp.h
.endif

INCSDIR=	/usr/include

NOOBJ=		# defined

SUBDIR=		rpc

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
