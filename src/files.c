/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "dlb.h"

#include <ctype.h>
#include <fcntl.h>

#include <errno.h>
#ifdef _MSC_VER	/* MSC 6.0 defines errno quite differently */
# if (_MSC_VER >= 600)
#  define SKIP_ERRNO
# endif
#else
# define SKIP_ERRNO
#endif
#ifndef SKIP_ERRNO
extern int errno;
#endif

#if defined(WIN32)
#include <sys/stat.h>
#endif
#ifndef O_BINARY
# define O_BINARY 0
#endif

#define FQN_NUMBUF 4
static char fqn_filename_buffer[FQN_NUMBUF][FQN_MAX_FILENAME];

#if !defined(WIN32)
char bones[] = "bonesnn.xxx";
char lock[PL_NSIZ+14];		/* long enough for uid+name+.99 */
#else
# if defined(WIN32)
char bones[] = "bonesnn.xxx";
char lock[PL_NSIZ+25];		/* long enough for username+-+name+.99 */
# endif
#endif

#ifdef HOLD_LOCKFILE_OPEN
struct level_ftrack {
int init;
int fd;					/* file descriptor for level file     */
int oflag;				/* open flags                         */
boolean nethack_thinks_it_is_open;	/* Does NetHack think it's open?       */
} lftrack;
# if defined(WIN32)
#include <share.h>
# endif
#endif /*HOLD_LOCKFILE_OPEN*/

#if defined(WIN32)
static int lockptr;
#define Close close
#define DeleteFile unlink
#endif

extern int n_dgns;		/* from dungeon.c */

static char *set_bonesfile_name(char *,d_level*);
static char *set_bonestemp_name(void);
static char *make_lockname(const char *,char *);
#ifdef HOLD_LOCKFILE_OPEN
static int open_levelfile_exclusively(const char *, int, int);
#endif



void display_file(const char *fname, boolean complain)
{
	dlb *fp;
	char *buf;
	int fsize;

	fp = dlb_fopen(fname, "r");
	if (!fp) {
	    if (complain) {
		pline("Cannot open \"%s\".", fname);
	    } else if (program_state.something_worth_saving) docrt();
	} else {
	    dlb_fseek(fp, 0, SEEK_END);
	    fsize = dlb_ftell(fp);
	    dlb_fseek(fp, 0, SEEK_SET);
	    
	    buf = malloc(fsize);
	    dlb_fread(buf, fsize, 1, fp);
	    
	    dlb_fclose(fp);
	    
	    display_buffer(buf, complain);
	    
	    free(buf);
	}
}


void regularize(char *s)
{
	char *lp;
#ifdef UNIX
	while ((lp=strchr(s, '.')) || (lp=strchr(s, '/')) || (lp=strchr(s,' ')))
		*lp = '_';
#else
# ifdef WIN32
	for (lp = s; *lp; lp++)
	    if ( *lp == '?' || *lp == '"' || *lp == '\\' ||
		 *lp == '/' || *lp == '>' || *lp == '<'  ||
		 *lp == '*' || *lp == '|' || *lp == ':'  || (*lp > 127))
			*lp = '_';
# endif
#endif
}

/*
 * fname_encode()
 *
 *   Args:
 *	legal		zero-terminated list of acceptable file name characters
 *	quotechar	lead-in character used to quote illegal characters as hex digits
 *	s		string to encode
 *	callerbuf	buffer to house result
 *	bufsz		size of callerbuf
 *
 *   Notes:
 *	The hex digits 0-9 and A-F are always part of the legal set due to
 *	their use in the encoding scheme, even if not explicitly included in 'legal'.
 *
 *   Sample:
 *	The following call:
 *	    fname_encode("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
 *				'%', "This is a % test!", buf, 512);
 *	results in this encoding:
 *	    "This%20is%20a%20%25%20test%21"
 */
char *fname_encode(const char *legal, char quotechar, char *s,
		   char *callerbuf, int bufsz)
{
	char *sp, *op;
	int cnt = 0;
	static const char hexdigits[] = "0123456789ABCDEF";

	sp = s;
	op = callerbuf;
	*op = '\0';
	
	while (*sp) {
		/* Do we have room for one more character or encoding? */
		if ((bufsz - cnt) <= 4) return callerbuf;

		if (*sp == quotechar) {
			sprintf(op, "%c%02X", quotechar, *sp);
			 op += 3;
			 cnt += 3;
		} else if ((strchr(legal, *sp) != 0) || (strchr(hexdigits, *sp) != 0)) {
			*op++ = *sp;
			*op = '\0';
			cnt++;
		} else {
			sprintf(op,"%c%02X", quotechar, *sp);
			op += 3;
			cnt += 3;
		}
		sp++;
	}
	return callerbuf;
}

/*
 * fname_decode()
 *
 *   Args:
 *	quotechar	lead-in character used to quote illegal characters as hex digits
 *	s		string to decode
 *	callerbuf	buffer to house result
 *	bufsz		size of callerbuf
 */
char *fname_decode(char quotechar, char *s, char *callerbuf, int bufsz)
{
	char *sp, *op;
	int k,calc,cnt = 0;
	static const char hexdigits[] = "0123456789ABCDEF";

	sp = s;
	op = callerbuf;
	*op = '\0';

	while (*sp) {
		/* Do we have room for one more character? */
		if ((bufsz - cnt) <= 2) return callerbuf;
		if (*sp == quotechar) {
			sp++;
			for (k=0; k < 16; ++k) if (*sp == hexdigits[k]) break;
			if (k >= 16) return callerbuf;	/* impossible, so bail */
			calc = k << 4; 
			sp++;
			for (k=0; k < 16; ++k) if (*sp == hexdigits[k]) break;
			if (k >= 16) return callerbuf;	/* impossible, so bail */
			calc += k; 
			sp++;
			*op++ = calc;
			*op = '\0';
		} else {
			*op++ = *sp++;
			*op = '\0';
		}
		cnt++;
	}
	return callerbuf;
}

const char *fqname(const char *filename, int whichprefix, int buffnum)
{
	if (!filename || whichprefix < 0 || whichprefix >= PREFIX_COUNT)
		return filename;
	if (!fqn_prefix[whichprefix])
		return filename;
	if (buffnum < 0 || buffnum >= FQN_NUMBUF) {
		impossible("Invalid fqn_filename_buffer specified: %d",
								buffnum);
		buffnum = 0;
	}
	if (strlen(fqn_prefix[whichprefix]) + strlen(filename) >=
						    FQN_MAX_FILENAME) {
		impossible("fqname too long: %s + %s", fqn_prefix[whichprefix],
						filename);
		return filename;	/* XXX */
	}
	strcpy(fqn_filename_buffer[buffnum], fqn_prefix[whichprefix]);
	return strcat(fqn_filename_buffer[buffnum], filename);
}

/* reasonbuf must be at least BUFSZ, supplied by caller */
/*ARGSUSED*/
int validate_prefix_locations(char *reasonbuf)
{
	FILE *fp;
	const char *filename;
	int prefcnt, failcount = 0;
	char panicbuf1[BUFSZ], panicbuf2[BUFSZ];
	const char *details;

	if (reasonbuf) reasonbuf[0] = '\0';
	for (prefcnt = 1; prefcnt < PREFIX_COUNT; prefcnt++) {
		/* don't test writing to configdir or datadir; they're readonly */
		if (prefcnt == DATAPREFIX)
		    continue;
		filename = fqname("validate", prefcnt, 3);
		if ((fp = fopen(filename, "w"))) {
			fclose(fp);
			unlink(filename);
		} else {
			if (reasonbuf) {
				if (failcount) strcat(reasonbuf,", ");
				strcat(reasonbuf, fqn_prefix_names[prefcnt]);
			}
			/* the paniclog entry gets the value of errno as well */
			sprintf(panicbuf1,"Invalid %s", fqn_prefix_names[prefcnt]);
			
			if (!(details = strerror(errno)))
				details = "";
			sprintf(panicbuf2,"\"%s\", (%d) %s",
				fqn_prefix[prefcnt], errno, details);
			paniclog(panicbuf1, panicbuf2);
			failcount++;
		}	
	}
	if (failcount)
		return 0;
	
	return 1;
}

/* fopen a file, with OS-dependent bells and whistles */
/* NOTE: a simpler version of this routine also exists in util/dlb_main.c */
FILE *fopen_datafile(const char *filename, const char *mode, int prefix)
{
	FILE *fp;

	filename = fqname(filename, prefix, prefix == TROUBLEPREFIX ? 3 : 0);
	fp = fopen(filename, mode);
	return fp;
}

/* ----------  BEGIN LEVEL FILE HANDLING ----------- */

/* Construct a file name for a level-type file, which is of the form
 * something.level (with any old level stripped off).
 * This assumes there is space on the end of 'file' to append
 * a two digit number.  This is true for 'level'
 * but be careful if you use it for other things -dgk
 */
void set_levelfile_name(char *file, int lev)
{
	char *tf;

	tf = strrchr(file, '.');
	if (!tf) tf = eos(file);
	sprintf(tf, ".%d", lev);
	return;
}

int create_levelfile(int lev, char errbuf[])
{
	int fd;
	const char *fq_lock;

	if (errbuf) *errbuf = '\0';
	set_levelfile_name(lock, lev);
	fq_lock = fqname(lock, LEVELPREFIX, 0);

#if defined(WIN32)
	/* Use O_TRUNC to force the file to be shortened if it already
	 * exists and is currently longer.
	 */
# ifdef HOLD_LOCKFILE_OPEN
	if (lev == 0)
		fd = open_levelfile_exclusively(fq_lock, lev,
				O_WRONLY |O_CREAT | O_TRUNC | O_BINARY);
	else
# endif
	fd = open(fq_lock, O_WRONLY |O_CREAT | O_TRUNC | O_BINARY, FCMASK);
#else
	fd = creat(fq_lock, FCMASK);
#endif /* WIN32 */

	if (fd >= 0)
	    level_info[lev].flags |= LFILE_EXISTS;
	else if (errbuf)	/* failure explanation */
	    sprintf(errbuf,
		    "Cannot create file \"%s\" for level %d (errno %d).",
		    lock, lev, errno);

	return fd;
}


int open_levelfile(int lev, char errbuf[])
{
	int fd;
	const char *fq_lock;

	if (errbuf) *errbuf = '\0';
	set_levelfile_name(lock, lev);
	fq_lock = fqname(lock, LEVELPREFIX, 0);
# ifdef HOLD_LOCKFILE_OPEN
	if (lev == 0)
		fd = open_levelfile_exclusively(fq_lock, lev, O_RDONLY | O_BINARY );
	else
# endif
	fd = open(fq_lock, O_RDONLY | O_BINARY, 0);

	/* for failure, return an explanation that our caller can use;
	   settle for `lock' instead of `fq_lock' because the latter
	   might end up being too big for nethack's BUFSZ */
	if (fd < 0 && errbuf)
	    sprintf(errbuf,
		    "Cannot open file \"%s\" for level %d (errno %d).",
		    lock, lev, errno);

	return fd;
}


void delete_levelfile(int lev)
{
	/*
	 * Level 0 might be created by port specific code that doesn't
	 * call create_levfile(), so always assume that it exists.
	 */
	if (lev == 0 || (level_info[lev].flags & LFILE_EXISTS)) {
		set_levelfile_name(lock, lev);
#ifdef HOLD_LOCKFILE_OPEN
		if (lev == 0) really_close();
#endif
		unlink(fqname(lock, LEVELPREFIX, 0));
		level_info[lev].flags &= ~LFILE_EXISTS;
	}
}


void clearlocks(void)
{
	int x;

	/* can't access maxledgerno() before dungeons are created -dlc */
	for (x = (n_dgns ? maxledgerno() : 0); x >= 0; x--)
		delete_levelfile(x);	/* not all levels need be present */
}

#ifdef HOLD_LOCKFILE_OPEN
static int open_levelfile_exclusively(const char *name, int lev, int oflag)
{
	int reslt, fd;
	if (!lftrack.init) {
		lftrack.init = 1;
		lftrack.fd = -1;
	}
	if (lftrack.fd >= 0) {
		/* check for compatible access */
		if (lftrack.oflag == oflag) {
			fd = lftrack.fd;
			reslt = lseek(fd, 0L, SEEK_SET);
			if (reslt == -1L)
			    panic("open_levelfile_exclusively: lseek failed %d", errno);
			lftrack.nethack_thinks_it_is_open = TRUE;
		} else {
			really_close();
			fd = sopen(name, oflag,SH_DENYRW, FCMASK);
			lftrack.fd = fd;
			lftrack.oflag = oflag;
			lftrack.nethack_thinks_it_is_open = TRUE;
		}
	} else {
			fd = sopen(name, oflag,SH_DENYRW, FCMASK);
			lftrack.fd = fd;
			lftrack.oflag = oflag;
			if (fd >= 0)
			    lftrack.nethack_thinks_it_is_open = TRUE;
	}
	return fd;
}

void really_close(void)
{
	int fd = lftrack.fd;
	lftrack.nethack_thinks_it_is_open = FALSE;
	lftrack.fd = -1;
	lftrack.oflag = 0;
	_close(fd);
	return;
}

int close(int fd)
{
 	if (lftrack.fd == fd) {
		really_close();	/* close it, but reopen it to hold it */
		fd = open_levelfile(0, NULL);
		lftrack.nethack_thinks_it_is_open = FALSE;
		return 0;
	}
	return _close(fd);
}
#endif
	
/* ----------  END LEVEL FILE HANDLING ----------- */


/* ----------  BEGIN BONES FILE HANDLING ----------- */

/* set up "file" to be file name for retrieving bones, and return a
 * bonesid to be read/written in the bones file.
 */
static char *set_bonesfile_name(char *file, d_level *lev)
{
	s_level *sptr;
	char *dptr;

	sprintf(file, "bon%c%s", dungeons[lev->dnum].boneid,
			In_quest(lev) ? urole.filecode : "0");
	dptr = eos(file);
	if ((sptr = Is_special(lev)) != 0)
	    sprintf(dptr, ".%c", sptr->boneid);
	else
	    sprintf(dptr, ".%d", lev->dlevel);
	
	return dptr-2;
}

/* set up temporary file name for writing bones, to avoid another game's
 * trying to read from an uncompleted bones file.  we want an uncontentious
 * name, so use one in the namespace reserved for this game's level files.
 * (we are not reading or writing level files while writing bones files, so
 * the same array may be used instead of copying.)
 */
static char *set_bonestemp_name(void)
{
	char *tf;

	tf = strrchr(lock, '.');
	if (!tf) tf = eos(lock);
	sprintf(tf, ".bn");
	return lock;
}

int create_bonesfile(d_level *lev, char **bonesid, char errbuf[])
{
	const char *file;
	int fd;

	if (errbuf) *errbuf = '\0';
	*bonesid = set_bonesfile_name(bones, lev);
	file = set_bonestemp_name();
	file = fqname(file, BONESPREFIX, 0);

#if defined(WIN32)
	/* Use O_TRUNC to force the file to be shortened if it already
	 * exists and is currently longer.
	 */
	fd = open(file, O_WRONLY |O_CREAT | O_TRUNC | O_BINARY, FCMASK);
#else
	fd = creat(file, FCMASK);
#endif
	if (fd < 0 && errbuf) /* failure explanation */
	    sprintf(errbuf,
		    "Cannot create bones \"%s\", id %s (errno %d).",
		    lock, *bonesid, errno);

	return fd;
}


/* move completed bones file to proper name */
void commit_bonesfile(d_level *lev)
{
	const char *fq_bones, *tempname;
	int ret;

	set_bonesfile_name(bones, lev);
	fq_bones = fqname(bones, BONESPREFIX, 0);
	tempname = set_bonestemp_name();
	tempname = fqname(tempname, BONESPREFIX, 1);

	ret = rename(tempname, fq_bones);
	if (wizard && ret != 0)
		pline("couldn't rename %s to %s.", tempname, fq_bones);
}


int open_bonesfile(d_level *lev, char **bonesid)
{
	const char *fq_bones;
	int fd;

	*bonesid = set_bonesfile_name(bones, lev);
	fq_bones = fqname(bones, BONESPREFIX, 0);
	fd = open(fq_bones, O_RDONLY | O_BINARY, 0);
	return fd;
}


int delete_bonesfile(d_level *lev)
{
	set_bonesfile_name(bones, lev);
	return !(unlink(fqname(bones, BONESPREFIX, 0)) < 0);
}

/* ----------  END BONES FILE HANDLING ----------- */


/* ----------  BEGIN FILE LOCKING HANDLING ----------- */

static int nesting = 0;

static char *make_lockname(const char *filename, char *lockname)
{
#if defined(UNIX) || defined(WIN32)
	strcpy(lockname, filename);
	strcat(lockname, "_lock");
	return lockname;
# else
	lockname[0] = '\0';
	return NULL;
#endif  /* UNIX || WIN32 */
}


/* lock a file */
boolean lock_file(const char *filename, int whichprefix, int retryct)
{
	char locknambuf[BUFSZ];
	const char *lockname;

	nesting++;
	if (nesting > 1) {
	    impossible("TRIED TO NEST LOCKS");
	    return TRUE;
	}

	lockname = make_lockname(filename, locknambuf);
	filename = fqname(filename, whichprefix, 0);
	lockname = fqname(lockname, LOCKPREFIX, 2);

#if defined(UNIX)
	while (link(filename, lockname) == -1) {
	    int errnosv = errno;

	    switch (errnosv) {	/* George Barbanis */
	    case EEXIST:
		if (retryct--) {
		    raw_printf("Waiting for access to %s.  (%d retries left).\n",
			    filename, retryct);
			sleep(1);
		} else {
		    raw_print("I give up.  Sorry.\n");
		    raw_printf("Perhaps there is an old %s around?\n",
					lockname);
		    nesting--;
		    return FALSE;
		}

		break;
	    case ENOENT:
		raw_printf("Can't find file %s to lock!\n", filename);
		nesting--;
		return FALSE;
	    case EACCES:
		raw_printf("No write permission to lock %s!\n", filename);
		nesting--;
		return FALSE;
	    default:
		perror(lockname);
		raw_printf("Cannot lock %s for unknown reason (%d).\n",
			       filename, errnosv);
		nesting--;
		return FALSE;
	    }

	}
#endif  /* UNIX */

#if defined(WIN32)
#define OPENFAILURE(fd) (fd < 0)
    lockptr = -1;
    while (--retryct && OPENFAILURE(lockptr)) {
	lockptr = sopen(lockname, O_RDWR|O_CREAT, SH_DENYRW, S_IWRITE);
	if (OPENFAILURE(lockptr)) {
	    raw_printf("Waiting for access to %s.  (%d retries left).\n",
			filename, retryct);
	    Delay(50);
	}
    }
    if (!retryct) {
	raw_printf("I give up.  Sorry.\n");
	nesting--;
	return FALSE;
    }
#endif /* WIN32 */
	return TRUE;
}

/* unlock file, which must be currently locked by lock_file */
void unlock_file(const char *filename)
{
	char locknambuf[BUFSZ];
	const char *lockname;

	if (nesting == 1) {
		lockname = make_lockname(filename, locknambuf);
		lockname = fqname(lockname, LOCKPREFIX, 2);

#if defined(UNIX)
		if (unlink(lockname) < 0)
			raw_printf("Can't unlink %s.\n", lockname);
#endif  /* UNIX */

#if defined(WIN32)
		if (lockptr) Close(lockptr);
		DeleteFile(lockname);
		lockptr = 0;
#endif /* WIN32 */
	}

	nesting--;
}

/* ----------  END FILE LOCKING HANDLING ----------- */

/* ----------  BEGIN SCOREBOARD CREATION ----------- */

/* verify that we can write to the scoreboard file; if not, try to create one */
void check_recordfile(const char *dir)
{
	const char *fq_record;
	int fd;

#if defined(UNIX)
	fq_record = fqname(RECORD, SCOREPREFIX, 0);
	fd = open(fq_record, O_RDWR, 0);
	if (fd >= 0) {
	    close(fd);	/* RECORD is accessible */
	} else if ((fd = open(fq_record, O_CREAT|O_RDWR, FCMASK)) >= 0) {
	    close(fd);	/* RECORD newly created */
	} else {
	    raw_printf("Warning: cannot write scoreboard file %s\n", fq_record);
	}
#endif  /* UNIX */
#if defined(WIN32)
	char tmp[PATHLEN];
	strcpy(tmp, RECORD);
	fq_record = fqname(RECORD, SCOREPREFIX, 0);

	if ((fd = open(fq_record, O_RDWR)) < 0) {
	    /* try to create empty record */
	    if ((fd = open(fq_record, O_CREAT|O_RDWR, S_IREAD|S_IWRITE)) < 0) {
	raw_printf("Warning: cannot write record %s\n", tmp);
	    } else
		close(fd);
	} else		/* open succeeded */
	    close(fd);
#endif /* WIN32*/
}

/* ----------  END SCOREBOARD CREATION ----------- */

/* ----------  BEGIN PANIC/IMPOSSIBLE LOG ----------- */

/*ARGSUSED*/
void paniclog(const char *type,   /* panic, impossible, trickery */
	      const char *reason) /* explanation */
{
#ifdef PANICLOG
	FILE *lfile;
	char buf[BUFSZ];

	if (!program_state.in_paniclog) {
		program_state.in_paniclog = 1;
		lfile = fopen_datafile(PANICLOG, "a", TROUBLEPREFIX);
		if (lfile) {
		    fprintf(lfile, "%s %08ld: %s %s\n",
				   version_string(buf), yyyymmdd((time_t)0L),
				   type, reason);
		    fclose(lfile);
		}
		program_state.in_paniclog = 0;
	}
#endif /* PANICLOG */
	return;
}

/* ----------  END PANIC/IMPOSSIBLE LOG ----------- */

/*files.c*/
