/*
 * System call switch table.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $FreeBSD: src/sys/svr4/svr4_sysent.c,v 1.5.2.3 2001/10/05 07:38:44 peter Exp $
 * created from FreeBSD: src/sys/svr4/syscalls.master,v 1.6.2.2 2001/10/05 07:34:37 peter Exp 
 */

#include <sys/types.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <netinet/in.h>
#include <svr4/svr4.h>
#include <svr4/svr4_types.h>
#include <svr4/svr4_signal.h>
#include <svr4/svr4_proto.h>

#define AS(name) (sizeof(struct name) / sizeof(register_t))

/* The casts are bogus but will do for now. */
struct sysent svr4_sysent[] = {
	{ 0, (sy_call_t *)nosys },			/* 0 = unused */
	{ AS(sys_exit_args), (sy_call_t *)sys_exit },	/* 1 = exit */
	{ 0, (sy_call_t *)fork },			/* 2 = fork */
	{ AS(read_args), (sy_call_t *)read },		/* 3 = read */
	{ AS(write_args), (sy_call_t *)write },		/* 4 = write */
	{ AS(svr4_sys_open_args), (sy_call_t *)svr4_sys_open },	/* 5 = svr4_sys_open */
	{ AS(close_args), (sy_call_t *)close },		/* 6 = close */
	{ AS(svr4_sys_wait_args), (sy_call_t *)svr4_sys_wait },	/* 7 = svr4_sys_wait */
	{ AS(svr4_sys_creat_args), (sy_call_t *)svr4_sys_creat },	/* 8 = svr4_sys_creat */
	{ AS(link_args), (sy_call_t *)link },		/* 9 = link */
	{ AS(unlink_args), (sy_call_t *)unlink },	/* 10 = unlink */
	{ AS(svr4_sys_execv_args), (sy_call_t *)svr4_sys_execv },	/* 11 = svr4_sys_execv */
	{ AS(chdir_args), (sy_call_t *)chdir },		/* 12 = chdir */
	{ AS(svr4_sys_time_args), (sy_call_t *)svr4_sys_time },	/* 13 = svr4_sys_time */
	{ AS(svr4_sys_mknod_args), (sy_call_t *)svr4_sys_mknod },	/* 14 = svr4_sys_mknod */
	{ AS(chmod_args), (sy_call_t *)chmod },		/* 15 = chmod */
	{ AS(chown_args), (sy_call_t *)chown },		/* 16 = chown */
	{ AS(svr4_sys_break_args), (sy_call_t *)svr4_sys_break },	/* 17 = svr4_sys_break */
	{ AS(svr4_sys_stat_args), (sy_call_t *)svr4_sys_stat },	/* 18 = svr4_sys_stat */
	{ AS(lseek_args), (sy_call_t *)lseek },		/* 19 = lseek */
	{ 0, (sy_call_t *)getpid },			/* 20 = getpid */
	{ 0, (sy_call_t *)nosys },			/* 21 = old_mount */
	{ 0, (sy_call_t *)nosys },			/* 22 = sysv_umount */
	{ AS(setuid_args), (sy_call_t *)setuid },	/* 23 = setuid */
	{ 0, (sy_call_t *)getuid },			/* 24 = getuid */
	{ 0, (sy_call_t *)nosys },			/* 25 = stime */
	{ 0, (sy_call_t *)nosys },			/* 26 = ptrace */
	{ AS(svr4_sys_alarm_args), (sy_call_t *)svr4_sys_alarm },	/* 27 = svr4_sys_alarm */
	{ AS(svr4_sys_fstat_args), (sy_call_t *)svr4_sys_fstat },	/* 28 = svr4_sys_fstat */
	{ 0, (sy_call_t *)svr4_sys_pause },		/* 29 = svr4_sys_pause */
	{ AS(svr4_sys_utime_args), (sy_call_t *)svr4_sys_utime },	/* 30 = svr4_sys_utime */
	{ 0, (sy_call_t *)nosys },			/* 31 = stty */
	{ 0, (sy_call_t *)nosys },			/* 32 = gtty */
	{ AS(svr4_sys_access_args), (sy_call_t *)svr4_sys_access },	/* 33 = svr4_sys_access */
	{ AS(svr4_sys_nice_args), (sy_call_t *)svr4_sys_nice },	/* 34 = svr4_sys_nice */
	{ 0, (sy_call_t *)nosys },			/* 35 = statfs */
	{ 0, (sy_call_t *)sync },			/* 36 = sync */
	{ AS(svr4_sys_kill_args), (sy_call_t *)svr4_sys_kill },	/* 37 = svr4_sys_kill */
	{ 0, (sy_call_t *)nosys },			/* 38 = fstatfs */
	{ AS(svr4_sys_pgrpsys_args), (sy_call_t *)svr4_sys_pgrpsys },	/* 39 = svr4_sys_pgrpsys */
	{ 0, (sy_call_t *)nosys },			/* 40 = xenix */
	{ AS(dup_args), (sy_call_t *)dup },		/* 41 = dup */
	{ 0, (sy_call_t *)pipe },			/* 42 = pipe */
	{ AS(svr4_sys_times_args), (sy_call_t *)svr4_sys_times },	/* 43 = svr4_sys_times */
	{ 0, (sy_call_t *)nosys },			/* 44 = profil */
	{ 0, (sy_call_t *)nosys },			/* 45 = plock */
	{ AS(setgid_args), (sy_call_t *)setgid },	/* 46 = setgid */
	{ 0, (sy_call_t *)getgid },			/* 47 = getgid */
	{ AS(svr4_sys_signal_args), (sy_call_t *)svr4_sys_signal },	/* 48 = svr4_sys_signal */
#if defined(NOTYET)
	{ AS(svr4_sys_msgsys_args), (sy_call_t *)svr4_sys_msgsys },	/* 49 = svr4_sys_msgsys */
#else
	{ 0, (sy_call_t *)nosys },			/* 49 = msgsys */
#endif
	{ AS(svr4_sys_sysarch_args), (sy_call_t *)svr4_sys_sysarch },	/* 50 = svr4_sys_sysarch */
	{ 0, (sy_call_t *)nosys },			/* 51 = acct */
	{ 0, (sy_call_t *)nosys },			/* 52 = shmsys */
	{ 0, (sy_call_t *)nosys },			/* 53 = semsys */
	{ AS(svr4_sys_ioctl_args), (sy_call_t *)svr4_sys_ioctl },	/* 54 = svr4_sys_ioctl */
	{ 0, (sy_call_t *)nosys },			/* 55 = uadmin */
	{ 0, (sy_call_t *)nosys },			/* 56 = exch */
	{ AS(svr4_sys_utssys_args), (sy_call_t *)svr4_sys_utssys },	/* 57 = svr4_sys_utssys */
	{ AS(fsync_args), (sy_call_t *)fsync },		/* 58 = fsync */
	{ AS(svr4_sys_execve_args), (sy_call_t *)svr4_sys_execve },	/* 59 = svr4_sys_execve */
	{ AS(umask_args), (sy_call_t *)umask },		/* 60 = umask */
	{ AS(chroot_args), (sy_call_t *)chroot },	/* 61 = chroot */
	{ AS(svr4_sys_fcntl_args), (sy_call_t *)svr4_sys_fcntl },	/* 62 = svr4_sys_fcntl */
	{ AS(svr4_sys_ulimit_args), (sy_call_t *)svr4_sys_ulimit },	/* 63 = svr4_sys_ulimit */
	{ 0, (sy_call_t *)nosys },			/* 64 = reserved */
	{ 0, (sy_call_t *)nosys },			/* 65 = reserved */
	{ 0, (sy_call_t *)nosys },			/* 66 = reserved */
	{ 0, (sy_call_t *)nosys },			/* 67 = reserved */
	{ 0, (sy_call_t *)nosys },			/* 68 = reserved */
	{ 0, (sy_call_t *)nosys },			/* 69 = reserved */
	{ 0, (sy_call_t *)nosys },			/* 70 = advfs */
	{ 0, (sy_call_t *)nosys },			/* 71 = unadvfs */
	{ 0, (sy_call_t *)nosys },			/* 72 = rmount */
	{ 0, (sy_call_t *)nosys },			/* 73 = rumount */
	{ 0, (sy_call_t *)nosys },			/* 74 = rfstart */
	{ 0, (sy_call_t *)nosys },			/* 75 = sigret */
	{ 0, (sy_call_t *)nosys },			/* 76 = rdebug */
	{ 0, (sy_call_t *)nosys },			/* 77 = rfstop */
	{ 0, (sy_call_t *)nosys },			/* 78 = rfsys */
	{ AS(rmdir_args), (sy_call_t *)rmdir },		/* 79 = rmdir */
	{ AS(mkdir_args), (sy_call_t *)mkdir },		/* 80 = mkdir */
	{ AS(svr4_sys_getdents_args), (sy_call_t *)svr4_sys_getdents },	/* 81 = svr4_sys_getdents */
	{ 0, (sy_call_t *)nosys },			/* 82 = libattach */
	{ 0, (sy_call_t *)nosys },			/* 83 = libdetach */
	{ 0, (sy_call_t *)nosys },			/* 84 = sysfs */
	{ AS(svr4_sys_getmsg_args), (sy_call_t *)svr4_sys_getmsg },	/* 85 = svr4_sys_getmsg */
	{ AS(svr4_sys_putmsg_args), (sy_call_t *)svr4_sys_putmsg },	/* 86 = svr4_sys_putmsg */
	{ AS(svr4_sys_poll_args), (sy_call_t *)svr4_sys_poll },	/* 87 = svr4_sys_poll */
	{ AS(svr4_sys_lstat_args), (sy_call_t *)svr4_sys_lstat },	/* 88 = svr4_sys_lstat */
	{ AS(symlink_args), (sy_call_t *)symlink },	/* 89 = symlink */
	{ AS(readlink_args), (sy_call_t *)readlink },	/* 90 = readlink */
	{ AS(getgroups_args), (sy_call_t *)getgroups },	/* 91 = getgroups */
	{ AS(setgroups_args), (sy_call_t *)setgroups },	/* 92 = setgroups */
	{ AS(fchmod_args), (sy_call_t *)fchmod },	/* 93 = fchmod */
	{ AS(fchown_args), (sy_call_t *)fchown },	/* 94 = fchown */
	{ AS(svr4_sys_sigprocmask_args), (sy_call_t *)svr4_sys_sigprocmask },	/* 95 = svr4_sys_sigprocmask */
	{ AS(svr4_sys_sigsuspend_args), (sy_call_t *)svr4_sys_sigsuspend },	/* 96 = svr4_sys_sigsuspend */
	{ AS(svr4_sys_sigaltstack_args), (sy_call_t *)svr4_sys_sigaltstack },	/* 97 = svr4_sys_sigaltstack */
	{ AS(svr4_sys_sigaction_args), (sy_call_t *)svr4_sys_sigaction },	/* 98 = svr4_sys_sigaction */
	{ AS(svr4_sys_sigpending_args), (sy_call_t *)svr4_sys_sigpending },	/* 99 = svr4_sys_sigpending */
	{ AS(svr4_sys_context_args), (sy_call_t *)svr4_sys_context },	/* 100 = svr4_sys_context */
	{ 0, (sy_call_t *)nosys },			/* 101 = evsys */
	{ 0, (sy_call_t *)nosys },			/* 102 = evtrapret */
	{ AS(svr4_sys_statvfs_args), (sy_call_t *)svr4_sys_statvfs },	/* 103 = svr4_sys_statvfs */
	{ AS(svr4_sys_fstatvfs_args), (sy_call_t *)svr4_sys_fstatvfs },	/* 104 = svr4_sys_fstatvfs */
	{ 0, (sy_call_t *)nosys },			/* 105 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 106 = nfssvc */
	{ AS(svr4_sys_waitsys_args), (sy_call_t *)svr4_sys_waitsys },	/* 107 = svr4_sys_waitsys */
	{ 0, (sy_call_t *)nosys },			/* 108 = sigsendsys */
	{ AS(svr4_sys_hrtsys_args), (sy_call_t *)svr4_sys_hrtsys },	/* 109 = svr4_sys_hrtsys */
	{ 0, (sy_call_t *)nosys },			/* 110 = acancel */
	{ 0, (sy_call_t *)nosys },			/* 111 = async */
	{ 0, (sy_call_t *)nosys },			/* 112 = priocntlsys */
	{ AS(svr4_sys_pathconf_args), (sy_call_t *)svr4_sys_pathconf },	/* 113 = svr4_sys_pathconf */
	{ 0, (sy_call_t *)nosys },			/* 114 = mincore */
	{ AS(svr4_sys_mmap_args), (sy_call_t *)svr4_sys_mmap },	/* 115 = svr4_sys_mmap */
	{ AS(mprotect_args), (sy_call_t *)mprotect },	/* 116 = mprotect */
	{ AS(munmap_args), (sy_call_t *)munmap },	/* 117 = munmap */
	{ AS(svr4_sys_fpathconf_args), (sy_call_t *)svr4_sys_fpathconf },	/* 118 = svr4_sys_fpathconf */
	{ 0, (sy_call_t *)vfork },			/* 119 = vfork */
	{ AS(fchdir_args), (sy_call_t *)fchdir },	/* 120 = fchdir */
	{ AS(readv_args), (sy_call_t *)readv },		/* 121 = readv */
	{ AS(writev_args), (sy_call_t *)writev },	/* 122 = writev */
	{ AS(svr4_sys_xstat_args), (sy_call_t *)svr4_sys_xstat },	/* 123 = svr4_sys_xstat */
	{ AS(svr4_sys_lxstat_args), (sy_call_t *)svr4_sys_lxstat },	/* 124 = svr4_sys_lxstat */
	{ AS(svr4_sys_fxstat_args), (sy_call_t *)svr4_sys_fxstat },	/* 125 = svr4_sys_fxstat */
	{ AS(svr4_sys_xmknod_args), (sy_call_t *)svr4_sys_xmknod },	/* 126 = svr4_sys_xmknod */
	{ 0, (sy_call_t *)nosys },			/* 127 = clocal */
	{ AS(svr4_sys_setrlimit_args), (sy_call_t *)svr4_sys_setrlimit },	/* 128 = svr4_sys_setrlimit */
	{ AS(svr4_sys_getrlimit_args), (sy_call_t *)svr4_sys_getrlimit },	/* 129 = svr4_sys_getrlimit */
	{ AS(lchown_args), (sy_call_t *)lchown },	/* 130 = lchown */
	{ AS(svr4_sys_memcntl_args), (sy_call_t *)svr4_sys_memcntl },	/* 131 = svr4_sys_memcntl */
	{ 0, (sy_call_t *)nosys },			/* 132 = getpmsg */
	{ 0, (sy_call_t *)nosys },			/* 133 = putpmsg */
	{ AS(rename_args), (sy_call_t *)rename },	/* 134 = rename */
	{ AS(svr4_sys_uname_args), (sy_call_t *)svr4_sys_uname },	/* 135 = svr4_sys_uname */
	{ AS(setegid_args), (sy_call_t *)setegid },	/* 136 = setegid */
	{ AS(svr4_sys_sysconfig_args), (sy_call_t *)svr4_sys_sysconfig },	/* 137 = svr4_sys_sysconfig */
	{ AS(adjtime_args), (sy_call_t *)adjtime },	/* 138 = adjtime */
	{ AS(svr4_sys_systeminfo_args), (sy_call_t *)svr4_sys_systeminfo },	/* 139 = svr4_sys_systeminfo */
	{ 0, (sy_call_t *)nosys },			/* 140 = notused */
	{ AS(seteuid_args), (sy_call_t *)seteuid },	/* 141 = seteuid */
	{ 0, (sy_call_t *)nosys },			/* 142 = vtrace */
	{ 0, (sy_call_t *)nosys },			/* 143 = { */
	{ 0, (sy_call_t *)nosys },			/* 144 = sigtimedwait */
	{ 0, (sy_call_t *)nosys },			/* 145 = lwp_info */
	{ 0, (sy_call_t *)nosys },			/* 146 = yield */
	{ 0, (sy_call_t *)nosys },			/* 147 = lwp_sema_wait */
	{ 0, (sy_call_t *)nosys },			/* 148 = lwp_sema_post */
	{ 0, (sy_call_t *)nosys },			/* 149 = lwp_sema_trywait */
	{ 0, (sy_call_t *)nosys },			/* 150 = notused */
	{ 0, (sy_call_t *)nosys },			/* 151 = notused */
	{ 0, (sy_call_t *)nosys },			/* 152 = modctl */
	{ AS(svr4_sys_fchroot_args), (sy_call_t *)svr4_sys_fchroot },	/* 153 = svr4_sys_fchroot */
	{ AS(svr4_sys_utimes_args), (sy_call_t *)svr4_sys_utimes },	/* 154 = svr4_sys_utimes */
	{ 0, (sy_call_t *)svr4_sys_vhangup },		/* 155 = svr4_sys_vhangup */
	{ AS(svr4_sys_gettimeofday_args), (sy_call_t *)svr4_sys_gettimeofday },	/* 156 = svr4_sys_gettimeofday */
	{ AS(getitimer_args), (sy_call_t *)getitimer },	/* 157 = getitimer */
	{ AS(setitimer_args), (sy_call_t *)setitimer },	/* 158 = setitimer */
	{ 0, (sy_call_t *)nosys },			/* 159 = lwp_create */
	{ 0, (sy_call_t *)nosys },			/* 160 = lwp_exit */
	{ 0, (sy_call_t *)nosys },			/* 161 = lwp_suspend */
	{ 0, (sy_call_t *)nosys },			/* 162 = lwp_continue */
	{ 0, (sy_call_t *)nosys },			/* 163 = lwp_kill */
	{ 0, (sy_call_t *)nosys },			/* 164 = lwp_self */
	{ 0, (sy_call_t *)nosys },			/* 165 = lwp_getprivate */
	{ 0, (sy_call_t *)nosys },			/* 166 = lwp_setprivate */
	{ 0, (sy_call_t *)nosys },			/* 167 = lwp_wait */
	{ 0, (sy_call_t *)nosys },			/* 168 = lwp_mutex_unlock */
	{ 0, (sy_call_t *)nosys },			/* 169 = lwp_mutex_lock */
	{ 0, (sy_call_t *)nosys },			/* 170 = lwp_cond_wait */
	{ 0, (sy_call_t *)nosys },			/* 171 = lwp_cond_signal */
	{ 0, (sy_call_t *)nosys },			/* 172 = lwp_cond_broadcast */
	{ 0, (sy_call_t *)nosys },			/* 173 = { */
	{ 0, (sy_call_t *)nosys },			/* 174 = { */
	{ AS(svr4_sys_llseek_args), (sy_call_t *)svr4_sys_llseek },	/* 175 = svr4_sys_llseek */
	{ 0, (sy_call_t *)nosys },			/* 176 = inst_sync */
	{ 0, (sy_call_t *)nosys },			/* 177 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 178 = kaio */
	{ 0, (sy_call_t *)nosys },			/* 179 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 180 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 181 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 182 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 183 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 184 = tsolsys */
	{ AS(svr4_sys_acl_args), (sy_call_t *)svr4_sys_acl },	/* 185 = svr4_sys_acl */
	{ AS(svr4_sys_auditsys_args), (sy_call_t *)svr4_sys_auditsys },	/* 186 = svr4_sys_auditsys */
	{ 0, (sy_call_t *)nosys },			/* 187 = processor_bind */
	{ 0, (sy_call_t *)nosys },			/* 188 = processor_info */
	{ 0, (sy_call_t *)nosys },			/* 189 = p_online */
	{ 0, (sy_call_t *)nosys },			/* 190 = sigqueue */
	{ 0, (sy_call_t *)nosys },			/* 191 = clock_gettime */
	{ 0, (sy_call_t *)nosys },			/* 192 = clock_settime */
	{ 0, (sy_call_t *)nosys },			/* 193 = clock_getres */
	{ 0, (sy_call_t *)nosys },			/* 194 = timer_create */
	{ 0, (sy_call_t *)nosys },			/* 195 = timer_delete */
	{ 0, (sy_call_t *)nosys },			/* 196 = timer_settime */
	{ 0, (sy_call_t *)nosys },			/* 197 = timer_gettime */
	{ 0, (sy_call_t *)nosys },			/* 198 = timer_overrun */
	{ AS(nanosleep_args), (sy_call_t *)nanosleep },	/* 199 = nanosleep */
	{ AS(svr4_sys_facl_args), (sy_call_t *)svr4_sys_facl },	/* 200 = svr4_sys_facl */
	{ 0, (sy_call_t *)nosys },			/* 201 = door */
	{ AS(setreuid_args), (sy_call_t *)setreuid },	/* 202 = setreuid */
	{ AS(setregid_args), (sy_call_t *)setregid },	/* 203 = setregid */
	{ 0, (sy_call_t *)nosys },			/* 204 = install_utrap */
	{ 0, (sy_call_t *)nosys },			/* 205 = signotify */
	{ 0, (sy_call_t *)nosys },			/* 206 = schedctl */
	{ 0, (sy_call_t *)nosys },			/* 207 = pset */
	{ 0, (sy_call_t *)nosys },			/* 208 = whoknows */
	{ AS(svr4_sys_resolvepath_args), (sy_call_t *)svr4_sys_resolvepath },	/* 209 = svr4_sys_resolvepath */
	{ 0, (sy_call_t *)nosys },			/* 210 = signotifywait */
	{ 0, (sy_call_t *)nosys },			/* 211 = lwp_sigredirect */
	{ 0, (sy_call_t *)nosys },			/* 212 = lwp_alarm */
	{ AS(svr4_sys_getdents64_args), (sy_call_t *)svr4_sys_getdents64 },	/* 213 = svr4_sys_getdents64 */
	{ AS(svr4_sys_mmap64_args), (sy_call_t *)svr4_sys_mmap64 },	/* 214 = svr4_sys_mmap64 */
	{ AS(svr4_sys_stat64_args), (sy_call_t *)svr4_sys_stat64 },	/* 215 = svr4_sys_stat64 */
	{ AS(svr4_sys_lstat64_args), (sy_call_t *)svr4_sys_lstat64 },	/* 216 = svr4_sys_lstat64 */
	{ AS(svr4_sys_fstat64_args), (sy_call_t *)svr4_sys_fstat64 },	/* 217 = svr4_sys_fstat64 */
	{ AS(svr4_sys_statvfs64_args), (sy_call_t *)svr4_sys_statvfs64 },	/* 218 = svr4_sys_statvfs64 */
	{ AS(svr4_sys_fstatvfs64_args), (sy_call_t *)svr4_sys_fstatvfs64 },	/* 219 = svr4_sys_fstatvfs64 */
	{ AS(svr4_sys_setrlimit64_args), (sy_call_t *)svr4_sys_setrlimit64 },	/* 220 = svr4_sys_setrlimit64 */
	{ AS(svr4_sys_getrlimit64_args), (sy_call_t *)svr4_sys_getrlimit64 },	/* 221 = svr4_sys_getrlimit64 */
	{ 0, (sy_call_t *)nosys },			/* 222 = pread64 */
	{ 0, (sy_call_t *)nosys },			/* 223 = pwrite64 */
	{ AS(svr4_sys_creat64_args), (sy_call_t *)svr4_sys_creat64 },	/* 224 = svr4_sys_creat64 */
	{ AS(svr4_sys_open64_args), (sy_call_t *)svr4_sys_open64 },	/* 225 = svr4_sys_open64 */
	{ 0, (sy_call_t *)nosys },			/* 226 = rpcsys */
	{ 0, (sy_call_t *)nosys },			/* 227 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 228 = whoknows */
	{ 0, (sy_call_t *)nosys },			/* 229 = whoknows */
	{ AS(svr4_sys_socket_args), (sy_call_t *)svr4_sys_socket },	/* 230 = svr4_sys_socket */
	{ AS(socketpair_args), (sy_call_t *)socketpair },	/* 231 = socketpair */
	{ AS(bind_args), (sy_call_t *)bind },		/* 232 = bind */
	{ AS(listen_args), (sy_call_t *)listen },	/* 233 = listen */
	{ AS(accept_args), (sy_call_t *)accept },	/* 234 = accept */
	{ AS(connect_args), (sy_call_t *)connect },	/* 235 = connect */
	{ AS(shutdown_args), (sy_call_t *)shutdown },	/* 236 = shutdown */
	{ AS(svr4_sys_recv_args), (sy_call_t *)svr4_sys_recv },	/* 237 = svr4_sys_recv */
	{ AS(recvfrom_args), (sy_call_t *)recvfrom },	/* 238 = recvfrom */
	{ AS(recvmsg_args), (sy_call_t *)recvmsg },	/* 239 = recvmsg */
	{ AS(svr4_sys_send_args), (sy_call_t *)svr4_sys_send },	/* 240 = svr4_sys_send */
	{ AS(sendmsg_args), (sy_call_t *)sendmsg },	/* 241 = sendmsg */
	{ AS(svr4_sys_sendto_args), (sy_call_t *)svr4_sys_sendto },	/* 242 = svr4_sys_sendto */
	{ AS(getpeername_args), (sy_call_t *)getpeername },	/* 243 = getpeername */
	{ AS(getsockname_args), (sy_call_t *)getsockname },	/* 244 = getsockname */
	{ AS(getsockopt_args), (sy_call_t *)getsockopt },	/* 245 = getsockopt */
	{ AS(setsockopt_args), (sy_call_t *)setsockopt },	/* 246 = setsockopt */
	{ 0, (sy_call_t *)nosys },			/* 247 = sockconfig */
	{ 0, (sy_call_t *)nosys },			/* 248 = { */
	{ 0, (sy_call_t *)nosys },			/* 249 = { */
};