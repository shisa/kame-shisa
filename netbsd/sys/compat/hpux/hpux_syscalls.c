/* $NetBSD: hpux_syscalls.c,v 1.36 2001/11/13 02:08:19 lukem Exp $ */

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.26 2001/06/06 16:17:40 thorpej Exp 
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpux_syscalls.c,v 1.36 2001/11/13 02:08:19 lukem Exp $");

#if defined(_KERNEL_OPT)
#if defined(_KERNEL_OPT)
#include "opt_sysv.h"
#include "opt_compat_43.h"
#endif
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/poll.h>
#include <sys/syscallargs.h>
#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_syscallargs.h>
#include <machine/hpux_machdep.h>
#endif /* _KERNEL_OPT */

const char *const hpux_syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"wait",			/* 7 = wait */
	"creat",			/* 8 = creat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"execv",			/* 11 = execv */
	"chdir",			/* 12 = chdir */
	"time_6x",			/* 13 = time_6x */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"chown",			/* 16 = chown */
	"obreak",			/* 17 = obreak */
	"stat_6x",			/* 18 = stat_6x */
	"lseek",			/* 19 = lseek */
	"getpid",			/* 20 = getpid */
	"#21 (unimplemented mount)",		/* 21 = unimplemented mount */
	"#22 (unimplemented umount)",		/* 22 = unimplemented umount */
	"setuid",			/* 23 = setuid */
	"getuid",			/* 24 = getuid */
	"stime_6x",			/* 25 = stime_6x */
#if 0
	"ptrace",			/* 26 = ptrace */
#else
	"#26 (unimplemented ptrace)",		/* 26 = unimplemented ptrace */
#endif
	"alarm_6x",			/* 27 = alarm_6x */
	"fstat_6x",			/* 28 = fstat_6x */
	"pause_6x",			/* 29 = pause_6x */
	"utime_6x",			/* 30 = utime_6x */
	"stty_6x",			/* 31 = stty_6x */
	"gtty_6x",			/* 32 = gtty_6x */
	"access",			/* 33 = access */
	"nice_6x",			/* 34 = nice_6x */
	"ftime_6x",			/* 35 = ftime_6x */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"stat",			/* 38 = stat */
	"setpgrp_6x",			/* 39 = setpgrp_6x */
	"lstat",			/* 40 = lstat */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"times_6x",			/* 43 = times_6x */
	"profil",			/* 44 = profil */
	"#45 (unimplemented ki_syscall)",		/* 45 = unimplemented ki_syscall */
	"setgid",			/* 46 = setgid */
	"getgid",			/* 47 = getgid */
	"ssig_6x",			/* 48 = ssig_6x */
	"#49 (unimplemented reserved for USG)",		/* 49 = unimplemented reserved for USG */
	"#50 (unimplemented reserved for USG)",		/* 50 = unimplemented reserved for USG */
	"#51 (unimplemented acct)",		/* 51 = unimplemented acct */
	"#52 (unimplemented)",		/* 52 = unimplemented */
	"#53 (unimplemented)",		/* 53 = unimplemented */
	"ioctl",			/* 54 = ioctl */
	"#55 (unimplemented reboot)",		/* 55 = unimplemented reboot */
	"symlink",			/* 56 = symlink */
	"utssys",			/* 57 = utssys */
	"readlink",			/* 58 = readlink */
	"execve",			/* 59 = execve */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"fcntl",			/* 62 = fcntl */
	"ulimit",			/* 63 = ulimit */
	"#64 (unimplemented)",		/* 64 = unimplemented */
	"#65 (unimplemented)",		/* 65 = unimplemented */
	"vfork",			/* 66 = vfork */
	"vread",			/* 67 = vread */
	"vwrite",			/* 68 = vwrite */
	"#69 (unimplemented)",		/* 69 = unimplemented */
	"#70 (unimplemented)",		/* 70 = unimplemented */
	"mmap",			/* 71 = mmap */
	"#72 (unimplemented)",		/* 72 = unimplemented */
	"munmap",			/* 73 = munmap */
	"mprotect",			/* 74 = mprotect */
	"#75 (unimplemented)",		/* 75 = unimplemented */
	"#76 (unimplemented)",		/* 76 = unimplemented */
	"#77 (unimplemented)",		/* 77 = unimplemented */
	"#78 (unimplemented)",		/* 78 = unimplemented */
	"getgroups",			/* 79 = getgroups */
	"setgroups",			/* 80 = setgroups */
	"getpgrp2",			/* 81 = getpgrp2 */
	"setpgrp2",			/* 82 = setpgrp2 */
	"setitimer",			/* 83 = setitimer */
	"wait3",			/* 84 = wait3 */
	"#85 (unimplemented swapon)",		/* 85 = unimplemented swapon */
	"getitimer",			/* 86 = getitimer */
	"#87 (unimplemented)",		/* 87 = unimplemented */
	"#88 (unimplemented)",		/* 88 = unimplemented */
	"#89 (unimplemented)",		/* 89 = unimplemented */
	"dup2",			/* 90 = dup2 */
	"#91 (unimplemented)",		/* 91 = unimplemented */
	"fstat",			/* 92 = fstat */
	"select",			/* 93 = select */
	"#94 (unimplemented)",		/* 94 = unimplemented */
	"fsync",			/* 95 = fsync */
	"#96 (unimplemented)",		/* 96 = unimplemented */
	"#97 (unimplemented)",		/* 97 = unimplemented */
	"#98 (unimplemented)",		/* 98 = unimplemented */
	"#99 (unimplemented)",		/* 99 = unimplemented */
	"#100 (unimplemented)",		/* 100 = unimplemented */
	"#101 (unimplemented)",		/* 101 = unimplemented */
	"#102 (unimplemented)",		/* 102 = unimplemented */
	"sigreturn",			/* 103 = sigreturn */
	"#104 (unimplemented)",		/* 104 = unimplemented */
	"#105 (unimplemented)",		/* 105 = unimplemented */
	"#106 (unimplemented)",		/* 106 = unimplemented */
	"#107 (unimplemented)",		/* 107 = unimplemented */
	"sigvec",			/* 108 = sigvec */
	"sigblock",			/* 109 = sigblock */
	"sigsetmask",			/* 110 = sigsetmask */
	"sigpause",			/* 111 = sigpause */
	"sigstack",			/* 112 = sigstack */
	"#113 (unimplemented)",		/* 113 = unimplemented */
	"#114 (unimplemented)",		/* 114 = unimplemented */
	"#115 (unimplemented)",		/* 115 = unimplemented */
	"gettimeofday",			/* 116 = gettimeofday */
	"#117 (unimplemented)",		/* 117 = unimplemented */
	"#118 (unimplemented)",		/* 118 = unimplemented */
	"#119 (unimplemented io_stub)",		/* 119 = unimplemented io_stub */
	"readv",			/* 120 = readv */
	"writev",			/* 121 = writev */
	"settimeofday",			/* 122 = settimeofday */
	"__posix_fchown",			/* 123 = __posix_fchown */
	"fchmod",			/* 124 = fchmod */
	"#125 (unimplemented)",		/* 125 = unimplemented */
	"setresuid",			/* 126 = setresuid */
	"setresgid",			/* 127 = setresgid */
	"rename",			/* 128 = rename */
	"truncate",			/* 129 = truncate */
	"ftruncate",			/* 130 = ftruncate */
	"#131 (unimplemented)",		/* 131 = unimplemented */
	"sysconf",			/* 132 = sysconf */
	"#133 (unimplemented)",		/* 133 = unimplemented */
	"#134 (unimplemented)",		/* 134 = unimplemented */
	"#135 (unimplemented)",		/* 135 = unimplemented */
	"mkdir",			/* 136 = mkdir */
	"rmdir",			/* 137 = rmdir */
	"#138 (unimplemented)",		/* 138 = unimplemented */
	"#139 (unimplemented)",		/* 139 = unimplemented */
	"#140 (unimplemented)",		/* 140 = unimplemented */
	"#141 (unimplemented)",		/* 141 = unimplemented */
	"#142 (unimplemented)",		/* 142 = unimplemented */
	"#143 (unimplemented)",		/* 143 = unimplemented */
	"getrlimit",			/* 144 = getrlimit */
	"setrlimit",			/* 145 = setrlimit */
	"#146 (unimplemented)",		/* 146 = unimplemented */
	"#147 (unimplemented)",		/* 147 = unimplemented */
	"#148 (unimplemented)",		/* 148 = unimplemented */
	"#149 (unimplemented)",		/* 149 = unimplemented */
	"#150 (unimplemented)",		/* 150 = unimplemented */
	"#151 (unimplemented privgrp)",		/* 151 = unimplemented privgrp */
	"rtprio",			/* 152 = rtprio */
	"#153 (unimplemented plock)",		/* 153 = unimplemented plock */
	"netioctl",			/* 154 = netioctl */
	"lockf",			/* 155 = lockf */
#ifdef SYSVSEM
	"semget",			/* 156 = semget */
	"osemctl",			/* 157 = osemctl */
	"semop",			/* 158 = semop */
#else
	"#156 (unimplemented semget)",		/* 156 = unimplemented semget */
	"#157 (unimplemented osemctl)",		/* 157 = unimplemented osemctl */
	"#158 (unimplemented semop)",		/* 158 = unimplemented semop */
#endif
#ifdef SYSVMSG
	"msgget",			/* 159 = msgget */
	"omsgctl",			/* 160 = omsgctl */
	"msgsnd",			/* 161 = msgsnd */
	"msgrcv",			/* 162 = msgrcv */
#else
	"#159 (unimplemented msgget)",		/* 159 = unimplemented msgget */
	"#160 (unimplemented omsgctl)",		/* 160 = unimplemented omsgctl */
	"#161 (unimplemented msgsnd)",		/* 161 = unimplemented msgsnd */
	"#162 (unimplemented msgrcv)",		/* 162 = unimplemented msgrcv */
#endif
#ifdef SYSVSHM
	"shmget",			/* 163 = shmget */
	"oshmctl",			/* 164 = oshmctl */
	"shmat",			/* 165 = shmat */
	"shmdt",			/* 166 = shmdt */
#else
	"#163 (unimplemented shmget)",		/* 163 = unimplemented shmget */
	"#164 (unimplemented shmctl)",		/* 164 = unimplemented shmctl */
	"#165 (unimplemented shmat)",		/* 165 = unimplemented shmat */
	"#166 (unimplemented shmdt)",		/* 166 = unimplemented shmdt */
#endif
	"advise",			/* 167 = advise */
	"#168 (unimplemented nsp_init)",		/* 168 = unimplemented nsp_init */
	"#169 (unimplemented cluster)",		/* 169 = unimplemented cluster */
	"#170 (unimplemented mkrnod)",		/* 170 = unimplemented mkrnod */
	"#171 (unimplemented test)",		/* 171 = unimplemented test */
	"#172 (unimplemented unsp_open)",		/* 172 = unimplemented unsp_open */
	"#173 (unimplemented)",		/* 173 = unimplemented */
	"getcontext",			/* 174 = getcontext */
	"#175 (unimplemented setcontext)",		/* 175 = unimplemented setcontext */
	"#176 (unimplemented bigio)",		/* 176 = unimplemented bigio */
	"#177 (unimplemented pipenode)",		/* 177 = unimplemented pipenode */
	"#178 (unimplemented lsync)",		/* 178 = unimplemented lsync */
	"#179 (unimplemented getmachineid)",		/* 179 = unimplemented getmachineid */
	"#180 (unimplemented mysite)",		/* 180 = unimplemented mysite */
	"#181 (unimplemented sitels)",		/* 181 = unimplemented sitels */
	"#182 (unimplemented swapclients)",		/* 182 = unimplemented swapclients */
	"#183 (unimplemented rmtprocess)",		/* 183 = unimplemented rmtprocess */
	"#184 (unimplemented dskless_stats)",		/* 184 = unimplemented dskless_stats */
	"#185 (unimplemented Northrop special)",		/* 185 = unimplemented Northrop special */
	"#186 (unimplemented setacl)",		/* 186 = unimplemented setacl */
	"#187 (unimplemented fsetacl)",		/* 187 = unimplemented fsetacl */
	"#188 (unimplemented getacl)",		/* 188 = unimplemented getacl */
	"#189 (unimplemented fgetacl)",		/* 189 = unimplemented fgetacl */
	"getaccess",			/* 190 = getaccess */
	"#191 (unimplemented getaudid)",		/* 191 = unimplemented getaudid */
	"#192 (unimplemented setaudid)",		/* 192 = unimplemented setaudid */
	"#193 (unimplemented getaudproc)",		/* 193 = unimplemented getaudproc */
	"#194 (unimplemented setaudproc)",		/* 194 = unimplemented setaudproc */
	"#195 (unimplemented getevent)",		/* 195 = unimplemented getevent */
	"#196 (unimplemented setevent)",		/* 196 = unimplemented setevent */
	"#197 (unimplemented audwrite)",		/* 197 = unimplemented audwrite */
	"#198 (unimplemented audswitch)",		/* 198 = unimplemented audswitch */
	"#199 (unimplemented audctl)",		/* 199 = unimplemented audctl */
	"waitpid",			/* 200 = waitpid */
	"#201 (unimplemented)",		/* 201 = unimplemented */
	"#202 (unimplemented)",		/* 202 = unimplemented */
	"#203 (unimplemented)",		/* 203 = unimplemented */
	"#204 (unimplemented)",		/* 204 = unimplemented */
	"#205 (unimplemented)",		/* 205 = unimplemented */
	"#206 (unimplemented)",		/* 206 = unimplemented */
	"#207 (unimplemented)",		/* 207 = unimplemented */
	"#208 (unimplemented)",		/* 208 = unimplemented */
	"#209 (unimplemented)",		/* 209 = unimplemented */
	"#210 (unimplemented)",		/* 210 = unimplemented */
	"#211 (unimplemented)",		/* 211 = unimplemented */
	"#212 (unimplemented)",		/* 212 = unimplemented */
	"#213 (unimplemented)",		/* 213 = unimplemented */
	"#214 (unimplemented)",		/* 214 = unimplemented */
	"#215 (unimplemented)",		/* 215 = unimplemented */
	"#216 (unimplemented)",		/* 216 = unimplemented */
	"#217 (unimplemented)",		/* 217 = unimplemented */
	"#218 (unimplemented)",		/* 218 = unimplemented */
	"#219 (unimplemented)",		/* 219 = unimplemented */
	"#220 (unimplemented)",		/* 220 = unimplemented */
	"#221 (unimplemented)",		/* 221 = unimplemented */
	"#222 (unimplemented)",		/* 222 = unimplemented */
	"#223 (unimplemented)",		/* 223 = unimplemented */
	"#224 (unimplemented set_no_trunc)",		/* 224 = unimplemented set_no_trunc */
	"pathconf",			/* 225 = pathconf */
	"fpathconf",			/* 226 = fpathconf */
	"#227 (unimplemented)",		/* 227 = unimplemented */
	"#228 (unimplemented)",		/* 228 = unimplemented */
	"#229 (unimplemented async_daemon)",		/* 229 = unimplemented async_daemon */
	"#230 (unimplemented nfs_fcntl)",		/* 230 = unimplemented nfs_fcntl */
	"getdirentries",			/* 231 = getdirentries */
	"getdomainname",			/* 232 = getdomainname */
	"#233 (unimplemented getfh)",		/* 233 = unimplemented getfh */
	"#234 (unimplemented vfsmount)",		/* 234 = unimplemented vfsmount */
	"#235 (unimplemented nfssvc)",		/* 235 = unimplemented nfssvc */
	"setdomainname",			/* 236 = setdomainname */
	"#237 (unimplemented statfs)",		/* 237 = unimplemented statfs */
	"#238 (unimplemented fstatfs)",		/* 238 = unimplemented fstatfs */
	"sigaction",			/* 239 = sigaction */
	"sigprocmask",			/* 240 = sigprocmask */
	"sigpending",			/* 241 = sigpending */
	"sigsuspend",			/* 242 = sigsuspend */
	"#243 (unimplemented fsctl)",		/* 243 = unimplemented fsctl */
	"#244 (unimplemented)",		/* 244 = unimplemented */
	"#245 (unimplemented pstat)",		/* 245 = unimplemented pstat */
	"#246 (unimplemented)",		/* 246 = unimplemented */
	"#247 (unimplemented)",		/* 247 = unimplemented */
	"#248 (unimplemented)",		/* 248 = unimplemented */
	"#249 (unimplemented)",		/* 249 = unimplemented */
	"#250 (unimplemented)",		/* 250 = unimplemented */
	"#251 (unimplemented ulconnect)",		/* 251 = unimplemented ulconnect */
	"#252 (unimplemented ulcontrol)",		/* 252 = unimplemented ulcontrol */
	"#253 (unimplemented ulcreate)",		/* 253 = unimplemented ulcreate */
	"#254 (unimplemented uldest)",		/* 254 = unimplemented uldest */
	"#255 (unimplemented ulrecv)",		/* 255 = unimplemented ulrecv */
	"#256 (unimplemented ulrecvcn)",		/* 256 = unimplemented ulrecvcn */
	"#257 (unimplemented ulsend)",		/* 257 = unimplemented ulsend */
	"#258 (unimplemented ulshutdown)",		/* 258 = unimplemented ulshutdown */
	"#259 (unimplemented swapfs)",		/* 259 = unimplemented swapfs */
	"#260 (unimplemented)",		/* 260 = unimplemented */
	"#261 (unimplemented)",		/* 261 = unimplemented */
	"#262 (unimplemented)",		/* 262 = unimplemented */
	"#263 (unimplemented)",		/* 263 = unimplemented */
	"#264 (unimplemented)",		/* 264 = unimplemented */
	"#265 (unimplemented)",		/* 265 = unimplemented */
	"#266 (unimplemented)",		/* 266 = unimplemented */
	"#267 (unimplemented tsync)",		/* 267 = unimplemented tsync */
	"getdtablesize",			/* 268 = getdtablesize */
	"poll",			/* 269 = poll */
	"#270 (unimplemented getmsg)",		/* 270 = unimplemented getmsg */
	"#271 (unimplemented putmsg)",		/* 271 = unimplemented putmsg */
	"fchdir",			/* 272 = fchdir */
	"#273 (unimplemented getmount_cnt)",		/* 273 = unimplemented getmount_cnt */
	"#274 (unimplemented getmount_entry)",		/* 274 = unimplemented getmount_entry */
	"accept",			/* 275 = accept */
	"bind",			/* 276 = bind */
	"connect",			/* 277 = connect */
	"getpeername",			/* 278 = getpeername */
	"getsockname",			/* 279 = getsockname */
	"getsockopt",			/* 280 = getsockopt */
	"listen",			/* 281 = listen */
	"recv",			/* 282 = recv */
	"recvfrom",			/* 283 = recvfrom */
	"recvmsg",			/* 284 = recvmsg */
	"send",			/* 285 = send */
	"sendmsg",			/* 286 = sendmsg */
	"sendto",			/* 287 = sendto */
	"setsockopt2",			/* 288 = setsockopt2 */
	"shutdown",			/* 289 = shutdown */
	"socket",			/* 290 = socket */
	"socketpair",			/* 291 = socketpair */
	"#292 (unimplemented)",		/* 292 = unimplemented */
	"#293 (unimplemented)",		/* 293 = unimplemented */
	"#294 (unimplemented)",		/* 294 = unimplemented */
	"#295 (unimplemented)",		/* 295 = unimplemented */
	"#296 (unimplemented)",		/* 296 = unimplemented */
	"#297 (unimplemented)",		/* 297 = unimplemented */
	"#298 (unimplemented ipccreate)",		/* 298 = unimplemented ipccreate */
	"#299 (unimplemented ipcname)",		/* 299 = unimplemented ipcname */
	"#300 (unimplemented ipcnamerase)",		/* 300 = unimplemented ipcnamerase */
	"#301 (unimplemented ipclookup)",		/* 301 = unimplemented ipclookup */
	"#302 (unimplemented ipcselect)",		/* 302 = unimplemented ipcselect */
	"#303 (unimplemented ipcconnect)",		/* 303 = unimplemented ipcconnect */
	"#304 (unimplemented ipcrecvcn)",		/* 304 = unimplemented ipcrecvcn */
	"#305 (unimplemented ipcsend)",		/* 305 = unimplemented ipcsend */
	"#306 (unimplemented ipcrecv)",		/* 306 = unimplemented ipcrecv */
	"#307 (unimplemented ipcgetnodename)",		/* 307 = unimplemented ipcgetnodename */
	"#308 (unimplemented ipcsetnodename)",		/* 308 = unimplemented ipcsetnodename */
	"#309 (unimplemented ipcontrol)",		/* 309 = unimplemented ipcontrol */
	"#310 (unimplemented ipcshutdown)",		/* 310 = unimplemented ipcshutdown */
	"#311 (unimplemented ipcdest)",		/* 311 = unimplemented ipcdest */
#ifdef SYSVSEM
	"semctl",			/* 312 = semctl */
#else
	"#312 (unimplemented semctl)",		/* 312 = unimplemented semctl */
#endif
#ifdef SYSVMSG
	"msgctl",			/* 313 = msgctl */
#else
	"#313 (unimplemented msgctl)",		/* 313 = unimplemented msgctl */
#endif
#ifdef SYSVSHM
	"shmctl",			/* 314 = shmctl */
#else
	"#314 (unimplemented shmctl)",		/* 314 = unimplemented shmctl */
#endif
	"#315 (unimplemented mpctl)",		/* 315 = unimplemented mpctl */
	"#316 (unimplemented exportfs)",		/* 316 = unimplemented exportfs */
	"#317 (unimplemented getpmsg)",		/* 317 = unimplemented getpmsg */
	"#318 (unimplemented putpmsg)",		/* 318 = unimplemented putpmsg */
	"#319 (unimplemented strioctl)",		/* 319 = unimplemented strioctl */
	"#320 (unimplemented msync)",		/* 320 = unimplemented msync */
	"#321 (unimplemented msleep)",		/* 321 = unimplemented msleep */
	"#322 (unimplemented mwakeup)",		/* 322 = unimplemented mwakeup */
	"#323 (unimplemented msem_init)",		/* 323 = unimplemented msem_init */
	"#324 (unimplemented msem_remove)",		/* 324 = unimplemented msem_remove */
};
