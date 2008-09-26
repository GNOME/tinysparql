#ifndef MINGW_COMPAT_H
#define MINGW_COMPAT_H

#include <Windows.h>

#define _fullpath_internal(res,path,size) \
  (GetFullPathName ((path), (size), (res), NULL) ? (res) : NULL)

#define realpath(path,resolved_path) _fullpath_internal(resolved_path, path, MAX_PATH)

#define getc_unlocked(s) getc(s)

#define RLIMIT_CPU	0		/* CPU time in seconds */
#define RLIMIT_AS	6		/* address space (virt. memory) limit */
typedef unsigned long rlim_t;

struct rlimit {
	rlim_t	rlim_cur;
	rlim_t	rlim_max;
};

#define localtime_r( _clock, _result ) \
	( *(_result) = *localtime( (_clock) ), \
	  (_result) )

#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>

struct timezone {
       int tz_minuteswest; /* minutes west of Greenwich */
       int tz_dsttime;	   /* type of dst correction */
     };

static int gettimeofday (struct timeval *tv, struct timezone *tz)
{
   struct _timeb tb;

   if (!tv)
      return (-1);

  _ftime (&tb);
  tv->tv_sec  = tb.time;
  tv->tv_usec = tb.millitm * 1000 + 500;
  if (tz)
  {
    tz->tz_minuteswest = -60 * _timezone;
    tz->tz_dsttime = _daylight;
  }
  return (0);
}

// Does not exist in a windows filesystem
#define S_IFLNK 0
#define S_IFSOCK 0
#define S_ISVTX 0
#define S_ISLNK(X) 0
#define S_ISUID 0
#define S_ISGID 0
#define S_ISGRP 0
#define S_IXOTH 0
#define S_IXGRP 0

#define link(from, to) 0

#define	_LK_UNLCK	0	/* Unlock */
#define	_LK_LOCK	1	/* Lock */
#define	_LK_NBLCK	2	/* Non-blocking lock */
#define	_LK_RLCK	3	/* Lock for read only */
#define	_LK_NBRLCK	4	/* Non-blocking lock for read only */

#define F_TLOCK _LK_NBLCK /* Test and lock a section for exclusive use */

#ifndef SIGHUP
#define SIGHUP 1 /* hangup */
#endif
#ifndef SIGBUS
#define SIGBUS 7 /* bus error */
#endif
#ifndef SIGKILL
#define SIGKILL 9 /* kill (cannot be caught or ignored) */
#endif
#ifndef SIGSEGV
#define SIGSEGV 11 /* segment violation */
#endif
#ifndef SIGPIPE
#define SIGPIPE 13 /* write on a pipe with no one to read it */
#endif
#ifndef SIGCHLD
#define SIGCHLD 20 /* to parent on child stop or exit */
#endif
#ifndef SIGUSR1
#define SIGUSR1 30 /* user defined signal 1 */
#endif
#ifndef SIGUSR2
#define SIGUSR2 31 /* user defined signal 2 */
#endif

#define sigemptyset(pset)    (*(pset) = 0)
#define sigfillset(pset)     (*(pset) = (unsigned int)-1)
#define sigaddset(pset, num) (*(pset) |= (1L<<(num)))
#define sigdelset(pset, num) (*(pset) &= ~(1L<<(num)))
#define sigismember(pset, num) (*(pset) & (1L<<(num)))

#define lockf _locking

#define nice(nice_level) 0

#endif
