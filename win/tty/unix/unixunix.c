/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* This file collects some Unix dependencies */

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "nethack.h"
#include "wintty.h"

#ifdef __linux__
extern void linux_mapon(void);
extern void linux_mapoff(void);
#endif


#ifdef GETRES_SUPPORT

extern int nh_getresuid(uid_t *, uid_t *, uid_t *);
extern uid_t nh_getuid(void);
extern uid_t nh_geteuid(void);
extern int nh_getresgid(gid_t *, gid_t *, gid_t *);
extern gid_t nh_getgid(void);
extern gid_t nh_getegid(void);

int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    return nh_getresuid(ruid, euid, suid);
}

uid_t getuid(void)
{
    return nh_getuid();
}

uid_t geteuid(void)
{
    return nh_geteuid();
}

int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid)
{
    return nh_getresgid(rgid, egid, sgid);
}

gid_t getgid(void)
{
    return nh_getgid();
}

gid_t getegid(void)
{
    return nh_getegid();
}

#endif	/* GETRES_SUPPORT */
