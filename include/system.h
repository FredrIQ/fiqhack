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
E  long random(void);
# endif
# if (!defined(bsdi) && !defined(__FreeBSD__)) || defined(RANDOM)
E void srandom(unsigned int);
# else
#  if !defined(bsdi) && !defined(__FreeBSD__)
E int srandom(unsigned int);
#  endif
# endif
#else
E long lrand48();
E void srand48();
#endif /* BSD || RANDOM */

#if !defined(BSD)
/* real BSD wants all these to return int */
E void exit(int);
E void free(void *);
E void perror(const char *);
#endif
E void qsort(void *,size_t,size_t,int(*)(const void *,const void *));

#if !defined(__GNUC__)
/* may already be defined */

# ifndef bsdi
E long lseek(int,long,int);
# endif
# ifndef bsdi
E int write(int, const void *,unsigned);
# endif

E int unlink(const char *);

#endif /* !__GNUC__ */

/* The POSIX string.h is required to define all the mem* and str* functions */
#include <string.h>

#if defined(SYSV)
E unsigned sleep();
#endif

E char *getenv(const char *);
E char *getlogin();
E pid_t getpid(void);
E uid_t getuid(void);
E gid_t getgid(void);

/*# string(s).h #*/

# if !defined(SVR4)
E int vsprintf(char *, const char *, va_list);
E int vfprintf(FILE *, const char *, va_list);
E int vprintf(const char *, va_list);
# endif


E int tgetent(char *,const char *);
E void tputs(const char *,int,int (*)());
E int tgetnum(const char *);
E int tgetflag(const char *);
E char *tgetstr(const char *,char **);
E char *tgoto(const char *,int,int);

/* time functions */

E struct tm *localtime(const time_t *);

E time_t time(time_t *);

#undef E

#endif /*  !__cplusplus */

#endif /* SYSTEM_H */
