/*	SCCS Id: @(#)unixunix.c	3.4	1994/11/07	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* This file collects some Unix dependencies */

#include "hack.h"	/* mainly for index() which depends on BSD */

#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#ifdef __linux__
extern void linux_mapon(void);
extern void linux_mapoff(void);
#endif

static struct stat buf;

/* see whether we should throw away this xlock file */
static int veryold(int fd)
{
	time_t date;

	if(fstat(fd, &buf)) return 0;			/* cannot get status */
#ifndef INSURANCE
	if(buf.st_size != sizeof(int)) return 0;	/* not an xlock file */
#endif
	(void) time(&date);
	if(date - buf.st_mtime < 3L*24L*60L*60L) {	/* recent */
		int lockedpid;	/* should be the same size as hackpid */

		if(read(fd, (void *)&lockedpid, sizeof(lockedpid)) !=
			sizeof(lockedpid))
			/* strange ... */
			return 0;

		/* From: Rick Adams <seismo!rick> */
		/* This will work on 4.1cbsd, 4.2bsd and system 3? & 5. */
		/* It will do nothing on V7 or 4.1bsd. */
#ifndef NETWORK
		/* It will do a VERY BAD THING if the playground is shared
		   by more than one machine! -pem */
		if(!(kill(lockedpid, 0) == -1 && errno == ESRCH))
#endif
			return 0;
	}
	(void) close(fd);
	return 1;
}

static int eraseoldlocks(void)
{
	int i;

	/* cannot use maxledgerno() here, because we need to find a lock name
	 * before starting everything (including the dungeon initialization
	 * that sets astral_level, needed for maxledgerno()) up
	 */
	for(i = 1; i <= MAXDUNGEON*MAXLEVEL + 1; i++) {
		/* try to remove all */
		set_levelfile_name(lock, i);
		(void) unlink(fqname(lock, LEVELPREFIX, 0));
	}
	set_levelfile_name(lock, 0);
	if (unlink(fqname(lock, LEVELPREFIX, 0)))
		return 0;				/* cannot remove it */
	return 1;					/* success! */
}

void getlock(void)
{
	int i = 0, fd, c;
	const char *fq_lock;

#ifdef TTY_GRAPHICS
	/* idea from rpick%ucqais@uccba.uc.edu
	 * prevent automated rerolling of characters
	 * test input (fd0) so that tee'ing output to get a screen dump still
	 * works
	 * also incidentally prevents development of any hack-o-matic programs
	 */
	/* added check for window-system type -dlc */
	if (!strcmp(windowprocs.name, "tty"))
	    if (!isatty(0))
		error("You must play from a terminal.");
#endif

	/* we ignore QUIT and INT at this point */
	if (!lock_file(HLOCK, LOCKPREFIX, 10)) {
		wait_synch();
		error("%s", "");
	}

	regularize(lock);
	set_levelfile_name(lock, 0);

	if(locknum) {
		if(locknum > 25) locknum = 25;

		do {
			lock[0] = 'a' + i++;
			fq_lock = fqname(lock, LEVELPREFIX, 0);

			if((fd = open(fq_lock, 0)) == -1) {
			    if(errno == ENOENT) goto gotlock; /* no such file */
			    perror(fq_lock);
			    unlock_file(HLOCK);
			    error("Cannot open %s", fq_lock);
			}

			if(veryold(fd) /* closes fd if true */
							&& eraseoldlocks())
				goto gotlock;
			(void) close(fd);
		} while(i < locknum);

		unlock_file(HLOCK);
		error("Too many hacks running now.");
	} else {
		fq_lock = fqname(lock, LEVELPREFIX, 0);
		if((fd = open(fq_lock, 0)) == -1) {
			if(errno == ENOENT) goto gotlock;    /* no such file */
			perror(fq_lock);
			unlock_file(HLOCK);
			error("Cannot open %s", fq_lock);
		}

		if(veryold(fd) /* closes fd if true */ && eraseoldlocks())
			goto gotlock;
		(void) close(fd);

		if(iflags.window_inited) {
		    c = yn("There is already a game in progress under your name.  Destroy old game?");
		} else {
		    (void) printf("\nThere is already a game in progress under your name.");
		    (void) printf("  Destroy old game? [yn] ");
		    (void) fflush(stdout);
		    c = getchar();
		    (void) putchar(c);
		    (void) fflush(stdout);
		    while (getchar() != '\n') ; /* eat rest of line and newline */
		}
		if(c == 'y' || c == 'Y')
			if(eraseoldlocks())
				goto gotlock;
			else {
				unlock_file(HLOCK);
				error("Couldn't destroy old game.");
			}
		else {
			unlock_file(HLOCK);
			error("%s", "");
		}
	}

gotlock:
	fd = creat(fq_lock, FCMASK);
	unlock_file(HLOCK);
	if(fd == -1) {
		error("cannot creat lock file (%s).", fq_lock);
	} else {
		if(write(fd, (void *) &hackpid, sizeof(hackpid))
		    != sizeof(hackpid)){
			error("cannot write lock (%s)", fq_lock);
		}
		if(close(fd) == -1) {
			error("cannot close lock (%s)", fq_lock);
		}
	}
}

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
