/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $FreeBSD: src/sys/ia64/ia32/ia32_proto.h,v 1.11 2003/11/07 20:28:09 jhb Exp $
 * created from FreeBSD: src/sys/ia64/ia32/syscalls.master,v 1.23 2003/11/07 20:27:16 jhb Exp 
 */

#ifndef _IA32_SYSPROTO_H_
#define	_IA32_SYSPROTO_H_

#include <sys/signal.h>
#include <sys/acl.h>
#include <sys/thr.h>
#include <sys/umtx.h>
#include <posix4/_semaphore.h>

#include <sys/ucontext.h>

struct proc;

struct thread;

#define	PAD_(t)	(sizeof(register_t) <= sizeof(t) ? \
		0 : sizeof(register_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define	PADL_(t)	0
#define	PADR_(t)	PAD_(t)
#else
#define	PADL_(t)	PAD_(t)
#define	PADR_(t)	0
#endif

struct ia32_open_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char mode_l_[PADL_(int)]; int mode; char mode_r_[PADR_(int)];
};
struct ia32_wait4_args {
	char pid_l_[PADL_(int)]; int pid; char pid_r_[PADR_(int)];
	char status_l_[PADL_(int *)]; int * status; char status_r_[PADR_(int *)];
	char options_l_[PADL_(int)]; int options; char options_r_[PADR_(int)];
	char rusage_l_[PADL_(struct rusage32 *)]; struct rusage32 * rusage; char rusage_r_[PADR_(struct rusage32 *)];
};
struct ia32_getfsstat_args {
	char buf_l_[PADL_(struct statfs32 *)]; struct statfs32 * buf; char buf_r_[PADR_(struct statfs32 *)];
	char bufsize_l_[PADL_(long)]; long bufsize; char bufsize_r_[PADR_(long)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct ia32_access_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct ia32_chflags_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct ia32_sigaltstack_args {
	char ss_l_[PADL_(struct sigaltstack32 *)]; struct sigaltstack32 * ss; char ss_r_[PADR_(struct sigaltstack32 *)];
	char oss_l_[PADL_(struct sigaltstack32 *)]; struct sigaltstack32 * oss; char oss_r_[PADR_(struct sigaltstack32 *)];
};
struct ia32_execve_args {
	char fname_l_[PADL_(char *)]; char * fname; char fname_r_[PADR_(char *)];
	char argv_l_[PADL_(u_int32_t *)]; u_int32_t * argv; char argv_r_[PADR_(u_int32_t *)];
	char envv_l_[PADL_(u_int32_t *)]; u_int32_t * envv; char envv_r_[PADR_(u_int32_t *)];
};
struct ia32_setitimer_args {
	char which_l_[PADL_(u_int)]; u_int which; char which_r_[PADR_(u_int)];
	char itv_l_[PADL_(struct itimerval32 *)]; struct itimerval32 * itv; char itv_r_[PADR_(struct itimerval32 *)];
	char oitv_l_[PADL_(struct itimerval32 *)]; struct itimerval32 * oitv; char oitv_r_[PADR_(struct itimerval32 *)];
};
struct ia32_select_args {
	char nd_l_[PADL_(int)]; int nd; char nd_r_[PADR_(int)];
	char in_l_[PADL_(fd_set *)]; fd_set * in; char in_r_[PADR_(fd_set *)];
	char ou_l_[PADL_(fd_set *)]; fd_set * ou; char ou_r_[PADR_(fd_set *)];
	char ex_l_[PADL_(fd_set *)]; fd_set * ex; char ex_r_[PADR_(fd_set *)];
	char tv_l_[PADL_(struct timeval32 *)]; struct timeval32 * tv; char tv_r_[PADR_(struct timeval32 *)];
};
struct ia32_gettimeofday_args {
	char tp_l_[PADL_(struct timeval32 *)]; struct timeval32 * tp; char tp_r_[PADR_(struct timeval32 *)];
	char tzp_l_[PADL_(struct timezone *)]; struct timezone * tzp; char tzp_r_[PADR_(struct timezone *)];
};
struct ia32_getrusage_args {
	char who_l_[PADL_(int)]; int who; char who_r_[PADR_(int)];
	char rusage_l_[PADL_(struct rusage32 *)]; struct rusage32 * rusage; char rusage_r_[PADR_(struct rusage32 *)];
};
struct ia32_readv_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
};
struct ia32_writev_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
};
struct ia32_settimeofday_args {
	char tv_l_[PADL_(struct timeval32 *)]; struct timeval32 * tv; char tv_r_[PADR_(struct timeval32 *)];
	char tzp_l_[PADL_(struct timezone *)]; struct timezone * tzp; char tzp_r_[PADR_(struct timezone *)];
};
struct ia32_utimes_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char tptr_l_[PADL_(struct timeval32 *)]; struct timeval32 * tptr; char tptr_r_[PADR_(struct timeval32 *)];
};
struct ia32_adjtime_args {
	char delta_l_[PADL_(struct timeval32 *)]; struct timeval32 * delta; char delta_r_[PADR_(struct timeval32 *)];
	char olddelta_l_[PADL_(struct timeval32 *)]; struct timeval32 * olddelta; char olddelta_r_[PADR_(struct timeval32 *)];
};
struct ia32_statfs_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char buf_l_[PADL_(struct statfs32 *)]; struct statfs32 * buf; char buf_r_[PADR_(struct statfs32 *)];
};
struct ia32_fstatfs_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(struct statfs32 *)]; struct statfs32 * buf; char buf_r_[PADR_(struct statfs32 *)];
};
struct ia32_semsys_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char a2_l_[PADL_(int)]; int a2; char a2_r_[PADR_(int)];
	char a3_l_[PADL_(int)]; int a3; char a3_r_[PADR_(int)];
	char a4_l_[PADL_(int)]; int a4; char a4_r_[PADR_(int)];
	char a5_l_[PADL_(int)]; int a5; char a5_r_[PADR_(int)];
};
struct ia32_msgsys_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char a2_l_[PADL_(int)]; int a2; char a2_r_[PADR_(int)];
	char a3_l_[PADL_(int)]; int a3; char a3_r_[PADR_(int)];
	char a4_l_[PADL_(int)]; int a4; char a4_r_[PADR_(int)];
	char a5_l_[PADL_(int)]; int a5; char a5_r_[PADR_(int)];
	char a6_l_[PADL_(int)]; int a6; char a6_r_[PADR_(int)];
};
struct ia32_shmsys_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char a2_l_[PADL_(int)]; int a2; char a2_r_[PADR_(int)];
	char a3_l_[PADL_(int)]; int a3; char a3_r_[PADR_(int)];
	char a4_l_[PADL_(int)]; int a4; char a4_r_[PADR_(int)];
};
struct ia32_pread_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(void *)]; void * buf; char buf_r_[PADR_(void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
};
struct ia32_pwrite_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(const void *)]; const void * buf; char buf_r_[PADR_(const void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
};
struct ia32_stat_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char ub_l_[PADL_(struct stat32 *)]; struct stat32 * ub; char ub_r_[PADR_(struct stat32 *)];
};
struct ia32_fstat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char ub_l_[PADL_(struct stat32 *)]; struct stat32 * ub; char ub_r_[PADR_(struct stat32 *)];
};
struct ia32_lstat_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char ub_l_[PADL_(struct stat32 *)]; struct stat32 * ub; char ub_r_[PADR_(struct stat32 *)];
};
struct ia32_mmap_args {
	char addr_l_[PADL_(caddr_t)]; caddr_t addr; char addr_r_[PADR_(caddr_t)];
	char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
	char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char poslo_l_[PADL_(u_int32_t)]; u_int32_t poslo; char poslo_r_[PADR_(u_int32_t)];
	char poshi_l_[PADL_(u_int32_t)]; u_int32_t poshi; char poshi_r_[PADR_(u_int32_t)];
};
struct ia32_lseek_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
	char whence_l_[PADL_(int)]; int whence; char whence_r_[PADR_(int)];
};
struct ia32_truncate_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char lengthlo_l_[PADL_(u_int32_t)]; u_int32_t lengthlo; char lengthlo_r_[PADR_(u_int32_t)];
	char lengthhi_l_[PADL_(u_int32_t)]; u_int32_t lengthhi; char lengthhi_r_[PADR_(u_int32_t)];
};
struct ia32_ftruncate_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char lengthlo_l_[PADL_(u_int32_t)]; u_int32_t lengthlo; char lengthlo_r_[PADR_(u_int32_t)];
	char lengthhi_l_[PADL_(u_int32_t)]; u_int32_t lengthhi; char lengthhi_r_[PADR_(u_int32_t)];
};
struct ia32_sysctl_args {
	char name_l_[PADL_(int *)]; int * name; char name_r_[PADR_(int *)];
	char namelen_l_[PADL_(u_int)]; u_int namelen; char namelen_r_[PADR_(u_int)];
	char old_l_[PADL_(void *)]; void * old; char old_r_[PADR_(void *)];
	char oldlenp_l_[PADL_(u_int32_t *)]; u_int32_t * oldlenp; char oldlenp_r_[PADR_(u_int32_t *)];
	char new_l_[PADL_(void *)]; void * new; char new_r_[PADR_(void *)];
	char newlen_l_[PADL_(u_int32_t)]; u_int32_t newlen; char newlen_r_[PADR_(u_int32_t)];
};
struct ia32_sigaction_args {
	char sig_l_[PADL_(int)]; int sig; char sig_r_[PADR_(int)];
	char act_l_[PADL_(struct sigaction32 *)]; struct sigaction32 * act; char act_r_[PADR_(struct sigaction32 *)];
	char oact_l_[PADL_(struct sigaction32 *)]; struct sigaction32 * oact; char oact_r_[PADR_(struct sigaction32 *)];
};
struct ia32_sendfile_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
	char nbytes_l_[PADL_(size_t)]; size_t nbytes; char nbytes_r_[PADR_(size_t)];
	char hdtr_l_[PADL_(struct sf_hdtr *)]; struct sf_hdtr * hdtr; char hdtr_r_[PADR_(struct sf_hdtr *)];
	char sbytes_l_[PADL_(off_t *)]; off_t * sbytes; char sbytes_r_[PADR_(off_t *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
int	ia32_open(struct thread *, struct ia32_open_args *);
int	ia32_wait4(struct thread *, struct ia32_wait4_args *);
int	ia32_getfsstat(struct thread *, struct ia32_getfsstat_args *);
int	ia32_access(struct thread *, struct ia32_access_args *);
int	ia32_chflags(struct thread *, struct ia32_chflags_args *);
int	ia32_sigaltstack(struct thread *, struct ia32_sigaltstack_args *);
int	ia32_execve(struct thread *, struct ia32_execve_args *);
int	ia32_setitimer(struct thread *, struct ia32_setitimer_args *);
int	ia32_select(struct thread *, struct ia32_select_args *);
int	ia32_gettimeofday(struct thread *, struct ia32_gettimeofday_args *);
int	ia32_getrusage(struct thread *, struct ia32_getrusage_args *);
int	ia32_readv(struct thread *, struct ia32_readv_args *);
int	ia32_writev(struct thread *, struct ia32_writev_args *);
int	ia32_settimeofday(struct thread *, struct ia32_settimeofday_args *);
int	ia32_utimes(struct thread *, struct ia32_utimes_args *);
int	ia32_adjtime(struct thread *, struct ia32_adjtime_args *);
int	ia32_statfs(struct thread *, struct ia32_statfs_args *);
int	ia32_fstatfs(struct thread *, struct ia32_fstatfs_args *);
int	ia32_semsys(struct thread *, struct ia32_semsys_args *);
int	ia32_msgsys(struct thread *, struct ia32_msgsys_args *);
int	ia32_shmsys(struct thread *, struct ia32_shmsys_args *);
int	ia32_pread(struct thread *, struct ia32_pread_args *);
int	ia32_pwrite(struct thread *, struct ia32_pwrite_args *);
int	ia32_stat(struct thread *, struct ia32_stat_args *);
int	ia32_fstat(struct thread *, struct ia32_fstat_args *);
int	ia32_lstat(struct thread *, struct ia32_lstat_args *);
int	ia32_mmap(struct thread *, struct ia32_mmap_args *);
int	ia32_lseek(struct thread *, struct ia32_lseek_args *);
int	ia32_truncate(struct thread *, struct ia32_truncate_args *);
int	ia32_ftruncate(struct thread *, struct ia32_ftruncate_args *);
int	ia32_sysctl(struct thread *, struct ia32_sysctl_args *);
int	ia32_sigaction(struct thread *, struct ia32_sigaction_args *);
int	ia32_sendfile(struct thread *, struct ia32_sendfile_args *);

#ifdef COMPAT_43


#endif /* COMPAT_43 */


#ifdef COMPAT_FREEBSD4

struct freebsd4_ia32_sendfile_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
	char nbytes_l_[PADL_(size_t)]; size_t nbytes; char nbytes_r_[PADR_(size_t)];
	char hdtr_l_[PADL_(struct sf_hdtr *)]; struct sf_hdtr * hdtr; char hdtr_r_[PADR_(struct sf_hdtr *)];
	char sbytes_l_[PADL_(off_t *)]; off_t * sbytes; char sbytes_r_[PADR_(off_t *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
int	freebsd4_ia32_sendfile(struct thread *, struct freebsd4_ia32_sendfile_args *);

#endif /* COMPAT_FREEBSD4 */

#undef PAD_
#undef PADL_
#undef PADR_

#endif /* !_IA32_SYSPROTO_H_ */
