/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-02-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "dlb.h"

#include <ctype.h>
#include <fcntl.h>

#include <errno.h>

#if defined(WIN32)
# include <sys/stat.h>
#else
# include <signal.h>
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

#define FQN_NUMBUF 4
static char fqn_filename_buffer[FQN_NUMBUF][FQN_MAX_FILENAME];
char bones[] = "bonesnn.xxx";

#if defined(WIN32)
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>

# if !defined(S_IRUSR)
#  define S_IRUSR _S_IREAD
#  define S_IWUSR _S_IWRITE
# endif
#endif

static const char *fqname(const char *, int, int);


void
display_file(const char *fname, boolean complain)
{
    dlb *fp;
    char *buf;
    int fsize;

    fp = dlb_fopen(fname, "r");
    if (!fp) {
        if (complain) {
            pline("Cannot open \"%s\".", fname);
        } else if (program_state.game_running)
            doredraw();
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


/* load a file into malloc'd memory, starting at the current file position
 * A single '\0' byte is appended to the file data, so that it can be treated as
 * a string safely. */
char *
loadfile(int fd, int *datasize)
{
    int start, end, len, bytes_left, ret;
    char *data;

    if (fd == -1)
        return NULL;

    start = lseek(fd, 0, SEEK_CUR);
    end = lseek(fd, 0, SEEK_END);
    lseek(fd, start, SEEK_SET);

    len = end - start;
    if (len == 0)
        return NULL;

    data = malloc(len + 1);
    bytes_left = len;
    do {
        /* read may return fewer bytes than requested for reasons that are not
           errors */
        ret = read(fd, &data[len - bytes_left], bytes_left);
        if (ret == -1) {
            free(data);
            return NULL;
        }

        bytes_left -= ret;
    } while (bytes_left);
    data[len] = '\0';

    *datasize = len;
    return data;
}


const char *
fqname(const char *filename, int whichprefix, int buffnum)
{
    if (!filename || whichprefix < 0 || whichprefix >= PREFIX_COUNT)
        return filename;
    if (!fqn_prefix[whichprefix])
        return filename;
    if (buffnum < 0 || buffnum >= FQN_NUMBUF) {
        impossible("Invalid fqn_filename_buffer specified: %d", buffnum);
        buffnum = 0;
    }
    if (strlen(fqn_prefix[whichprefix]) + strlen(filename) >=
            FQN_MAX_FILENAME) {
        impossible("fqname too long: %s + %s", fqn_prefix[whichprefix],
                   filename);
        return filename;        /* XXX */
    }
    strcpy(fqn_filename_buffer[buffnum], fqn_prefix[whichprefix]);
    return strcat(fqn_filename_buffer[buffnum], filename);
}


/* fopen a file */
/* NOTE: a simpler version of this routine also exists in util/dlb_main.c */
FILE *
fopen_datafile(const char *filename, const char *mode, int prefix)
{
    FILE *fp;

    filename = fqname(filename, prefix, prefix == TROUBLEPREFIX ? 3 : 0);
    fp = fopen(filename, mode);
    return fp;
}

/* open a file */
int
open_datafile(const char *filename, int oflags, int prefix)
{
    int fd;

#ifdef WIN32
    oflags |= O_BINARY;
#endif

    filename = fqname(filename, prefix, prefix == TROUBLEPREFIX ? 3 : 0);
    fd = open(filename, oflags, S_IRUSR | S_IWUSR);
    return fd;
}


/* ----------  BEGIN BONES FILE HANDLING ----------- */

int
create_bonesfile(char *bonesid, char errbuf[])
{
    const char *file;
    char tempname[PL_NSIZ + 32];
    int fd;

    if (errbuf)
        *errbuf = '\0';
    sprintf(bones, "bon%s", bonesid);
    sprintf(tempname, "%d%s.bn", (int)getuid(), u.uplname);
    file = fqname(tempname, BONESPREFIX, 0);

#if defined(WIN32)
    /* Use O_TRUNC to force the file to be shortened if it already exists and
       is currently longer. */
    fd = open(file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, FCMASK);
#else
    fd = creat(file, FCMASK);
#endif
    if (fd < 0 && errbuf)       /* failure explanation */
        sprintf(errbuf, "Cannot create bones id %s (errno %d).", bonesid,
                errno);

    return fd;
}


/* move completed bones file to proper name */
void
commit_bonesfile(char *bonesid)
{
    const char *fq_bones, *tempname;
    char tempbuf[PL_NSIZ + 32];
    int ret;

    sprintf(bones, "bon%s", bonesid);
    fq_bones = fqname(bones, BONESPREFIX, 0);
    sprintf(tempbuf, "%d%s.bn", (int)getuid(), u.uplname);
    tempname = fqname(tempbuf, BONESPREFIX, 1);

    ret = rename(tempname, fq_bones);
    if (wizard && ret != 0)
        pline("couldn't rename %s to %s.", tempname, fq_bones);
}


int
open_bonesfile(char *bonesid)
{
    const char *fq_bones;
    int fd;

    sprintf(bones, "bon%s", bonesid);
    fq_bones = fqname(bones, BONESPREFIX, 0);
    fd = open(fq_bones, O_RDONLY | O_BINARY, 0);
    return fd;
}


int
delete_bonesfile(char *bonesid)
{
    sprintf(bones, "bon%s", bonesid);
    return !(unlink(fqname(bones, BONESPREFIX, 0)) < 0);
}

/* ----------  END BONES FILE HANDLING ----------- */


/* ----------  BEGIN FILE LOCKING HANDLING ----------- */

/* change_fd_lock is the general locking/unlocking/upgrading/downgrading
   routine. The idea is that a write lock is used to protect writes to a file,
   and a read lock is used both to protect reads to the file, and (on
   Linux/UNIX) to express interest in being notified about its contents,
   something that's important for server play and for watching games. */

#if defined(UNIX)
/* lock any open file using fcntl */

static void
handle_sigalrm(int signum)
{
    /* We use alarm() to implement the timeout on locking. The handler does
       nothing, but because it exists and isn't SIG_DFL, it'll interrupt our
       lock attempt with EINTR. */
    (void) signum;
}

boolean
change_fd_lock(int fd, enum locktype type, int timeout)
{
    struct flock sflock;
    struct sigaction saction, oldsaction;
    int ret;

    if (fd == -1)
        return FALSE;

    saction.sa_handler = handle_sigalrm;
    saction.sa_flags = SA_RESETHAND;
    sigemptyset(&saction.sa_mask);
    sigaction(SIGALRM, &saction, &oldsaction);
    if (timeout)
        alarm(timeout);

    sflock.l_type =
        type == LT_WRITE ? F_WRLCK :
        type == LT_READ ? F_RDLCK :
        type == LT_NONE ? F_UNLCK :
        (impossible("invalid lock type in change_fd_lock"), F_UNLCK);
    sflock.l_whence = SEEK_SET;
    sflock.l_start = 0;
    sflock.l_len = 0;

    /* TODO: Check to see if any processes have read locks, tell them to unlock
       the file, tell them to relock the file once the lock's been
       established. */

    ret = fcntl(fd, F_SETLKW, &sflock) >= 0;

    alarm(0);
    sigaction(SIGALRM, &oldsaction, NULL);
    
    return ret;
}

#elif defined (WIN32)   /* windows versionf of lock_fd(), unlock_fd() */

/* lock any open file using LockFile */
boolean
change_fd_lock(int fd, enum locktype type, int timeout)
{
    HANDLE hFile;
    BOOL ret;

    if (fd == -1)
        return FALSE;

    hFile = (HANDLE) _get_osfhandle(fd);

    if (type == LT_NONE) {
        UnlockFile(hFile, 0, 0, 64, 0);
        return TRUE;
    }

    /* TODO: Check that the return value of this has the right semantics
       (it didn't in the UNIX code; check to see if separate read locks
       and write locks exist */

    /* lock only the first 64 bytes of the file to avoid problems with
       mismatching lock/unlock ranges */
    while (!(ret = LockFile(hFile, 0, 0, 64, 0)) && timeout--)
        Sleep(1);
    return ret;
}
#endif

/* ----------  END FILE LOCKING HANDLING ----------- */

/* ----------  BEGIN PANIC/IMPOSSIBLE LOG ----------- */

 /*ARGSUSED*/ void
paniclog(const char *type,      /* panic, impossible, trickery */
         const char *reason)
{       /* explanation */
#ifdef PANICLOG
    FILE *lfile;
    char buf[BUFSZ];

    if (!program_state.in_paniclog) {
        program_state.in_paniclog = 1;
        lfile = fopen_datafile(PANICLOG, "a", TROUBLEPREFIX);
        if (lfile) {
            fprintf(lfile, "%s %08ld: %s %s\n", version_string(buf),
                    yyyymmdd((time_t) 0L), type, reason);
            fclose(lfile);
        }
        program_state.in_paniclog = 0;
    }
#endif /* PANICLOG */
    return;
}

/* ----------  END PANIC/IMPOSSIBLE LOG ----------- */

/*files.c*/
