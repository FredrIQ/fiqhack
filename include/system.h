/*	SCCS Id: @(#)system.h	3.4	2001/12/07	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef SYSTEM_H
#define SYSTEM_H

#if !defined(__cplusplus)

#define E extern

#include <sys/types.h>

#endif /* !__cplusplus */

/* You may want to change this to fit your system, as this is almost
 * impossible to get right automatically.
 * This is the type of signal handling functions.
 */
#if (defined(_MSC_VER) || defined(WIN32))
# define SIG_RET_TYPE void (__cdecl *)(int)
#endif

#ifndef SIG_RET_TYPE
# define SIG_RET_TYPE void (*)()
#endif

#if !defined(__cplusplus)

#if defined(BSD) || defined(RANDOM)
# ifdef random
# undef random
# endif
# if !defined(LINUX)
E  long NDECL(random);
# endif
# if (!defined(bsdi) && !defined(__FreeBSD__)) || defined(RANDOM)
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
E void FDECL(perror, (const char *));
#endif
E void FDECL(qsort, (genericptr_t,size_t,size_t,
		     int(*)(const genericptr,const genericptr)));

#if !defined(__GNUC__)
/* may already be defined */

# ifndef bsdi
E long FDECL(lseek, (int,long,int));
# endif
# ifndef bsdi
E int FDECL(write, (int, const void *,unsigned));
# endif

E int FDECL(unlink, (const char *));

#endif /* !__GNUC__ */

/* The POSIX string.h is required to define all the mem* and str* functions */
#include <string.h>

#if defined(SYSV)
E unsigned sleep();
#endif

E char *FDECL(getenv, (const char *));
E char *getlogin();
E pid_t NDECL(getpid);
E uid_t NDECL(getuid);
E gid_t NDECL(getgid);

/*# string(s).h #*/

#ifdef NEED_VARARGS
# if defined(USE_STDARG) || defined(USE_VARARGS)
#  if !defined(SVR4)
E int FDECL(vsprintf, (char *, const char *, va_list));
E int FDECL(vfprintf, (FILE *, const char *, va_list));
E int FDECL(vprintf, (const char *, va_list));
#  endif
# else
#  define vprintf	printf
#  define vfprintf	fprintf
#  define vsprintf	sprintf
# endif
#endif /* NEED_VARARGS */


E int FDECL(tgetent, (char *,const char *));
E void FDECL(tputs, (const char *,int,int (*)()));
E int FDECL(tgetnum, (const char *));
E int FDECL(tgetflag, (const char *));
E char *FDECL(tgetstr, (const char *,char **));
E char *FDECL(tgoto, (const char *,int,int));

#ifdef ALLOC_C
E genericptr_t FDECL(malloc, (size_t));
#endif

/* time functions */

E struct tm *FDECL(localtime, (const time_t *));

E time_t FDECL(time, (time_t *));

#undef E

#endif /*  !__cplusplus */

#endif /* SYSTEM_H */
