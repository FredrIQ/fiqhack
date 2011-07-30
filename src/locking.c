/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "hack.h"

/* MAXLOCKNO must be greater than MAXDUNGEON * MAXLEVEL */
#define MAXLOCKNO 1024

static struct stat buf;

/* see whether we should throw away this xlock file */
static int veryold(int fd)
{
	time_t date;

	if (fstat(fd, &buf)) return 0;			/* cannot get status */
#ifndef INSURANCE
	if (buf.st_size != sizeof(int)) return 0;	/* not an xlock file */
#endif
	time(&date);
	if (date - buf.st_mtime < 3L*24L*60L*60L) {	/* recent */
		int lockedpid;	/* should be the same size as hackpid */

		if (read(fd, &lockedpid, sizeof(lockedpid)) !=
			sizeof(lockedpid))
			/* strange ... */
			return 0;

		return 0;
	}
	close(fd);
	return 1;
}

static int eraseoldlocks(void)
{
	int i;

	/* cannot use maxledgerno() here, because we need to find a lock name
	 * before starting everything (including the dungeon initialization
	 * that sets astral_level, needed for maxledgerno()) up
	 */
	for (i = 1; i <= MAXLOCKNO; i++) {
		/* try to remove all */
		set_levelfile_name(lock, i);
		unlink(fqname(lock, LEVELPREFIX, 0));
	}
	set_levelfile_name(lock, 0);
	if (unlink(fqname(lock, LEVELPREFIX, 0)))
		return 0;				/* cannot remove it */
	return 1;					/* success! */
}

void getlock(int locknum)
{
	int i = 0, fd, c;
	const char *fq_lock;

	/* we ignore QUIT and INT at this point */
	if (!lock_file(HLOCK, LOCKPREFIX, 10)) {
		panic("%s", "");
	}

	regularize(lock);
	set_levelfile_name(lock, 0);

	if (locknum) {
		if (locknum > 25) locknum = 25;

		do {
			lock[0] = 'a' + i++;
			fq_lock = fqname(lock, LEVELPREFIX, 0);

			if ((fd = open(fq_lock, 0)) == -1) {
			    if (errno == ENOENT) goto gotlock; /* no such file */
			    perror(fq_lock);
			    unlock_file(HLOCK);
			    panic("Cannot open %s", fq_lock);
			}

			if (veryold(fd) /* closes fd if true */
					&& eraseoldlocks())
				goto gotlock;
			close(fd);
		} while (i < locknum);

		unlock_file(HLOCK);
		panic("Too many hacks running now.");
	} else {
		fq_lock = fqname(lock, LEVELPREFIX, 0);
		if ((fd = open(fq_lock, 0)) == -1) {
			if (errno == ENOENT) goto gotlock;    /* no such file */
			perror(fq_lock);
			unlock_file(HLOCK);
			panic("Cannot open %s", fq_lock);
		}

		if (veryold(fd) /* closes fd if true */ && eraseoldlocks())
			goto gotlock;
		close(fd);

		
		raw_print("\nThere is already a game in progress under your name.");
		raw_print("  Destroy old game? [yn] ");
		fflush(stdout);
		c = getchar();
		putchar(c);
		fflush(stdout);
		while (getchar() != '\n') ; /* eat rest of line and newline */
		
		if (c == 'y' || c == 'Y')
			if (eraseoldlocks())
				goto gotlock;
			else {
				unlock_file(HLOCK);
				panic("Couldn't destroy old game.");
			}
		else {
			unlock_file(HLOCK);
			panic("%s", "");
		}
	}

gotlock:
	fd = creat(fq_lock, FCMASK);
	unlock_file(HLOCK);
	if (fd == -1) {
		panic("cannot creat lock file (%s).", fq_lock);
	} else {
		if (write(fd, &hackpid, sizeof(hackpid))
		    != sizeof(hackpid)){
			panic("cannot write lock (%s)", fq_lock);
		}
		if (close(fd) == -1) {
			panic("cannot close lock (%s)", fq_lock);
		}
	}
}
