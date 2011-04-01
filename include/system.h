/*	SCCS Id: @(#)system.h	3.4	2001/12/07	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef SYSTEM_H
#define SYSTEM_H

#if !defined(__cplusplus)

#define E extern

#include <sys/types.h>

#if defined(__TURBOC__)
#include <time.h>	/* time_t is not in <sys/types.h> */
#endif

#if defined(THINKC4) || defined(__TURBOC__)
typedef long	off_t;
#endif

#endif /* !__cplusplus */

/* You may want to change this to fit your system, as this is almost
 * impossible to get right automatically.
 * This is the type of signal handling functions.
 */
#if (defined(_MSC_VER) || defined(__TURBOC__) || defined(__SC__) || defined(WIN32))
# define SIG_RET_TYPE void (__cdecl *)(int)
#endif
#ifndef SIG_RET_TYPE
# if defined(NHSTDC) || defined(POSIX_TYPES) || defined(__DECC)
#  define SIG_RET_TYPE void (*)()
# endif
#endif
#ifndef SIG_RET_TYPE
# if defined(SUNOS4) || defined(SVR3) || defined(SVR4)
	/* SVR3 is defined automatically by some systems */
#  define SIG_RET_TYPE void (*)()
# endif
#endif
#ifndef SIG_RET_TYPE	/* BSD, SIII, SVR2 and earlier, Sun3.5 and earlier */
# define SIG_RET_TYPE int (*)()
#endif

#if !defined(__cplusplus)

#if defined(BSD) || defined(RANDOM)
# ifdef random
# undef random
# endif
# if !defined(__SC__) && !defined(LINUX)
E  long NDECL(random);
# endif
# if (!defined(SUNOS4) && !defined(bsdi) && !defined(__FreeBSD__)) || defined(RANDOM)
E void FDECL(srandom, (unsigned int));
# else
#  if !defined(bsdi) && !defined(__FreeBSD__)
E int FDECL(srandom, (unsigned int));
#  endif
# endif
#else
E long lrand48();
E void srand48();
#endif /* BSD || RANDOM */

#if !defined(BSD)
			/* real BSD wants all these to return int */
E void FDECL(exit, (int));

/* compensate for some CSet/2 bogosities */

/* If flex thinks that we're not __STDC__ it declares free() to return
   int and we die.  We must use __STDC__ instead of NHSTDC because
   the former is naturally what flex tests for. */
# if defined(__STDC__) || !defined(FLEX_SCANNER)
#   ifndef MONITOR_HEAP
E void FDECL(free, (genericptr_t));
#   endif
# endif
# if !defined(__SC__)
E void FDECL(perror, (const char *));
# endif
#endif
#ifndef NeXT
#ifdef POSIX_TYPES
E void FDECL(qsort, (genericptr_t,size_t,size_t,
		     int(*)(const genericptr,const genericptr)));
#else
# if defined(BSD)
E  int qsort();
# else
E   void FDECL(qsort, (genericptr_t,size_t,size_t,
		       int(*)(const genericptr,const genericptr)));
# endif
#endif
#endif /* NeXT */

#if !defined(__GNUC__)
/* may already be defined */

# ifndef bsdi
E long FDECL(lseek, (int,long,int));
# endif
#  if defined(POSIX_TYPES) || defined(__TURBOC__)
#   ifndef bsdi
E int FDECL(write, (int, const void *,unsigned));
#   endif
#  else
#   ifndef __MWERKS__	/* metrowerks defines write via universal headers */
E int FDECL(write, (int,genericptr_t,unsigned));
#   endif
#  endif

#  ifndef __SC__
E int FDECL(unlink, (const char *));
#  endif

#endif /* !__GNUC__ */

#if defined(HPUX) && !defined(_POSIX_SOURCE)
E long NDECL(fork);
#endif

#ifdef POSIX_TYPES
/* The POSIX string.h is required to define all the mem* and str* functions */
#include <string.h>
#else
#if defined(SYSV) || defined(SUNOS4)
# if defined(NHSTDC)
#  if !(defined(SUNOS4) && defined(__STDC__))
				/* Solaris unbundled cc (acc) */
E int FDECL(memcmp, (const void *,const void *,size_t));
E void *FDECL(memcpy, (void *, const void *, size_t));
E void *FDECL(memset, (void *, int, size_t));
#  endif
# else
#  ifndef memcmp	/* some systems seem to macro these back to b*() */
E int memcmp();
#  endif
#  ifndef memcpy
E char *memcpy();
#  endif
#  ifndef memset
E char *memset();
#  endif
# endif
#else
# ifdef HPUX
E int FDECL(memcmp, (char *,char *,int));
E void *FDECL(memcpy, (char *,char *,int));
E void *FDECL(memset, (char*,int,int));
# endif
#endif
#endif /* POSIX_TYPES */

#if defined(SYSV)
E unsigned sleep();
#endif
#if defined(HPUX)
E unsigned int FDECL(sleep, (unsigned int));
#endif

E char *FDECL(getenv, (const char *));
E char *getlogin();
#if defined(HPUX) && !defined(_POSIX_SOURCE)
E long NDECL(getuid);
E long NDECL(getgid);
E long NDECL(getpid);
#else
# ifdef POSIX_TYPES
E pid_t NDECL(getpid);
E uid_t NDECL(getuid);
E gid_t NDECL(getgid);
# else	/* !POSIX_TYPES */
#  ifndef getpid		/* Borland C defines getpid() as a macro */
E int NDECL(getpid);
#  endif
# endif	/*?POSIX_TYPES*/
#endif	/*?(HPUX && !_POSIX_SOURCE)*/

/* add more architectures as needed */
#if defined(HPUX)
#define seteuid(x) setreuid(-1, (x));
#endif

/*# string(s).h #*/
#if !defined(_XtIntrinsic_h) && !defined(POSIX_TYPES)
/* <X11/Intrinsic.h> #includes <string[s].h>; so does defining POSIX_TYPES */

#if defined(NeXT) && defined(__GNUC__)
#include <strings.h>
#else
E char	*FDECL(strcpy, (char *,const char *));
E char	*FDECL(strncpy, (char *,const char *,size_t));
E char	*FDECL(strcat, (char *,const char *));
E char	*FDECL(strncat, (char *,const char *,size_t));
E char	*FDECL(strpbrk, (const char *,const char *));

# if defined(SYSV) || defined(HPUX)
E char	*FDECL(strchr, (const char *,int));
E char	*FDECL(strrchr, (const char *,int));
# else /* BSD */
E char	*FDECL(index, (const char *,int));
E char	*FDECL(rindex, (const char *,int));
# endif

E int	FDECL(strcmp, (const char *,const char *));
E int	FDECL(strncmp, (const char *,const char *,size_t));
# ifdef HPUX
E unsigned int	FDECL(strlen, (char *));
# else
E int	FDECL(strlen, (const char *));
# endif /* HPUX */
#endif /* NeXT */

#endif	/* !_XtIntrinsic_h_ && !POSIX_TYPES */

#ifdef NEED_VARARGS
# if defined(USE_STDARG) || defined(USE_VARARGS)
#  if !defined(SVR4)
#    if !(defined(SUNOS4) && defined(__STDC__)) /* Solaris unbundled cc (acc) */
E int FDECL(vsprintf, (char *, const char *, va_list));
E int FDECL(vfprintf, (FILE *, const char *, va_list));
E int FDECL(vprintf, (const char *, va_list));
#    endif
#  endif
# else
#  define vprintf	printf
#  define vfprintf	fprintf
#  define vsprintf	sprintf
# endif
#endif /* NEED_VARARGS */


#if ! (defined(HPUX) && defined(_POSIX_SOURCE))
E int FDECL(tgetent, (char *,const char *));
E void FDECL(tputs, (const char *,int,int (*)()));
#endif
E int FDECL(tgetnum, (const char *));
E int FDECL(tgetflag, (const char *));
E char *FDECL(tgetstr, (const char *,char **));
E char *FDECL(tgoto, (const char *,int,int));

#ifdef ALLOC_C
E genericptr_t FDECL(malloc, (size_t));
#endif

/* time functions */

E struct tm *FDECL(localtime, (const time_t *));

# if (defined(BSD) && defined(POSIX_TYPES)) || defined(SYSV) || (defined(HPUX) && defined(_POSIX_SOURCE))
E time_t FDECL(time, (time_t *));
# else
E long FDECL(time, (time_t *));
# endif


#undef E

#endif /*  !__cplusplus */

#endif /* SYSTEM_H */
