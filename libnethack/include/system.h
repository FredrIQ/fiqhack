/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-30 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef SYSTEM_H
# define SYSTEM_H

/* Note: this header defines features of _UNIX_ systems. */

# if !defined(__cplusplus)

#  include <sys/types.h>

# endif/* !__cplusplus */

/* You may want to change this to fit your system, as this is almost
 * impossible to get right automatically.
 * This is the type of signal handling functions.
 */
# if (defined(_MSC_VER) || defined(WIN32))
#  define SIG_RET_TYPE void (__cdecl *)(int)
# endif

# ifndef SIG_RET_TYPE
#  define SIG_RET_TYPE void (*)(int)
# endif

# if !defined(__cplusplus)

extern void exit(int);
extern void perror(const char *);
extern void qsort(void *, size_t, size_t, int (*)(const void *, const void *));

#  if !defined(__GNUC__)
/* may already be defined */

extern long lseek(int, long, int);
extern int write(int, const void *, unsigned);

extern int unlink(const char *);

#  endif
       /* !__GNUC__ */

/* The POSIX string.h is required to define all the mem* and str* functions */
#  include <string.h>

extern unsigned sleep(unsigned);

extern char *getenv(const char *);
extern char *getlogin(void);

#  ifndef AIMAKE_BUILDOS_MSWin32
extern pid_t getpid(void);
extern uid_t getuid(void);
extern gid_t getgid(void);
#  endif


/* time functions */
extern time_t time(time_t *);

# endif/* !__cplusplus */

#endif /* SYSTEM_H */

