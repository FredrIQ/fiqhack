/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-09 */
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
create_bonesfile(const char *bonesid, const char **errbuf)
{
    const char *file;
    char tempname[PL_NSIZ + 32];
    int fd;

    if (errbuf)
        *errbuf = "";
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
        *errbuf = msgprintf("Cannot create bones id %s (errno %d).",
                            bonesid, errno);

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

/*
 * change_fd_lock is the general locking/unlocking/upgrading/downgrading
 * routine. We conceptually have four kinds of lock status (each of which is
 * strictly stricter than the one before):
 *
 * - No lock. Anything can happen to the file, and we don't know about it.
 *
 * - Monitor lock. The file can change, but we're notified when that happens
 *   (and do a server cancel). This sort of lock is only supported on
 *   program_state.logfile.
 *
 * - Read lock. The file cannot change, but we don't mind other processes
 *   reading it at the same time.
 *
 * - Write lock. We're taking control of changing the file; other processes
 *   cannot read it while we're changing it.
 *
 * To avoid deadlocks, upgrading directly from a read lock to a write lock is
 * forbidden; you must drop down to a monitor lock in between.
 *
 * The implementation varies by OS. On UNIX:
 *
 * - No lock: we hold no locks on the file
 *
 * - Monitor lock: we hold a read lock on the file using fcntl, and relinquish
 *   it if we receive a SIGRTMIN+1
 *
 * - Read lock: we hold a read lock on the file using fcntl, and block
 *   SIGRTMIN+1 until we finish reading (thus preventing other processes from
 *   writing, as we won't respond to their SIGRTMIN+1s until we're done)
 *
 * - Write lock: we hold a write lock on the file using fcntl; if we can't
 *   establish it, we SIGRTMIN+1 processes holding it until we can
 *
 * The behaviour on Windows is currently much more primitive (any Windows
 * experts out there to help?): we lock the file at LT_READ or higher, and
 * leave it unlocked at LT_MONITOR or lower. This means that watching games
 * locally doesn't work (watching games played on a server still works, though,
 * because the server itself is running UNIX).
 */


#ifdef UNIX
/*
 * We want to be notified about changes to the logfile; and we want to ensure
 * that the logfile is always locked when we read from it.
 *
 * There are basically two ways we could do this:
 *
 * - When receiving a SIGRTMIN+1, send a server cancel. Then unlock the logfile
 *   when the server cancel is reflected from the client.
 *
 * - When receiving a SIGRTMIN+1, unlock the file immediately, then relock it
 *   later in the same signal handler. Send a server cancel some time during
 *   this process, but don't care about when or if it's reflected.
 *
 * The first method is perfectly safe with respect to signal and lock timing,
 * and would be the "usual" way to do things. However, the problem is that it
 * requires the client to reflect server cancels in a timely manner, meaning
 * that we need a lot of discipline from the clients (and a lot of complexity in
 * the network server code to prevent malicious clients disrupting the locking
 * behaviour).
 *
 * The second method is much easier on the communications security front; if it
 * fails to reflect a server cancel, it will only affect that client (thus it's
 * its own fault that it's malfunctioning), and if it sends spurious server
 * cancels, that won't affect the server.  However, it makes the signal safety
 * properties harder to reason about; blocking on signals inside a signal
 * handler is almost unheard of.
 *
 * This code uses the second method; receiving a SIGRTMIN+1 will block for up to
 * three seconds, hopefully much less (SIGRTMIN+2 is used to end the blocking
 * early).  In order to prevent the logfile being written to while we're reading
 * it, SIGRTMIN+1 is masked while performing a read operation.
 */
static volatile sig_atomic_t unmatched_sigrtmin1s = 0;

/* WARNING: This function can be called async-signal on UNIX. Thus, all system
   calls it makes must be async-signal-safe, and it can't touch globals. */
static int
sigrtmin1_some_locker(int fd)
{
    struct flock sflock;
    int ok;
    sflock.l_type = F_WRLCK;
    sflock.l_whence = SEEK_SET;
    sflock.l_start = 0;
    sflock.l_len = 0;
    sflock.l_pid = -2; /* not necessary, but valgrind doesn't know that
                          fcntl initializes this, so we initialize it ourselves
                          to let valgrind know what's happening */

    ok = fcntl(fd, F_GETLK, &sflock) >= 0;                   /* fcntl is safe */

    if (!ok)
        return -1;    /* can't really do much else here */

    if (sflock.l_type == F_UNLCK)
        return -1;    /* everything's OK, no lockers to signal */

    if (sflock.l_type == F_WRLCK)
        return -1;    /* everything's OK,
                         the process that needs the lock has it */

    /* Try to signal the process that holds the lock, to ask it to release it */
    kill(sflock.l_pid, SIGRTMIN+1);                          /* kill is safe */
    return sflock.l_pid;
}

/* Unrelinquish the logfile lock. This just sets a flag. */
static void
handle_sigrtmin2(int signum)
{
    (void) signum;
    /* If someone's sending us spurious SIGRTMIN+2 signals, ignore them. (This
       could also happen if a SIGRTMIN+2 gets delayed for over 3 seconds; very
       unlikely, but possible, e.g. the computer goes into suspend mode with a
       very precise timing).

       The reason we use SIGRTMIN+2, not SIGUSR2, is that if a SIGRTMIN+1 and a
       SIGRTMIN+2 are sent in that order, the SIGRTMIN+1 is 100% guaranteed to
       arrive first, something that is not true of the more normal SIGUSR*
       signals. (If they are sent in the other order, higher then lower, then
       they could arrive either way round.) */
    if (unmatched_sigrtmin1s)
        unmatched_sigrtmin1s--;
}

/* The actual work of relinquishing the logfile lock.

   THIS FUNCTION RUNS ASYNC-SIGNAL. All globals accessed must be volatile (and
   protected by locks of at least LT_READ on the log file, or else only changed
   while SIGRTMIN+1 is being ignored, e.g. outside the game). All calls must be
   async-signal-safe.

   All system calls have an "is safe" comment added when they are verified to be
   safe. Make sure you check "man 7 signal" before adding such a comment
   yourself. (If a system call doesn't have such a comment, someone forgot to
   check it for safety.)

   The handler is installed in such a way that SIGRTMIN+2 is blocked inside this
   function. Thus, we use sigtimedwait and call the signal handler manually
   (sigtimedwait does not call the handler). */
static void
handle_sigrtmin1(int signum)
{
    int pid;
    sigset_t sigset;
    struct timespec timeout = {.tv_sec = 3, .tv_nsec = 0};

    (void) signum;

    unmatched_sigrtmin1s++;

    /* Give up the lock. (change_fd_lock is async-signal-safe specifically
       because it is called from this function; program_state.logfile is
       volatile for the same reason.) */
    change_fd_lock(program_state.logfile, TRUE, LT_NONE, 0);

    /* Tell any other processes that have the lock (other than the one that
       wants it) to give up the lock. (sigrtmin1_some_locker is also
       async-signal-safe for this reason.) */
    pid = sigrtmin1_some_locker(program_state.logfile);

    /* Wait until all SIGRTMIN+1s are unmatched, or 3 seconds elapse with no
       signal (in which case we assume that either we missed it, perhaps due to
       two SIGRTMIN+2s coming at almost the same instant, or there's a
       stuck/crashed process somewhere). */
    sigemptyset(&sigset);                           /* sigemptyset is safe */
    sigaddset(&sigset, SIGRTMIN+2);                 /* sigaddset is safe */

    while (unmatched_sigrtmin1s &&
           sigtimedwait(&sigset, NULL, &timeout) == SIGRTMIN+2)
        unmatched_sigrtmin1s--;

    unmatched_sigrtmin1s = 0; /* in case we got here by timeout */

    /* If we told another process to give up the lock, tell it it can take
       the lock again. */
    if (pid != -1)
        kill(pid, SIGRTMIN+2);                      /* kill is safe */
    
    /* Block until the other process has finished writing, then relock the
       logfile and return. */
    if (!change_fd_lock(program_state.logfile, TRUE, LT_MONITOR, 3)) {
        /* We can't safely call panic() here (it calls into printf). We also
           can't safely call longjmp() here. And we can't call raw_print here,
           because we might be linked to the client directly and it might do
           anything.
           
           Out of the various error handling options, our best legal option
           is abort(). Sadly, we can't produce meaningful feedback to the user,
           but it will at least be visible in a debugger.
           
           (Also, the save format is designed such that crashing at any point
           produces a working save file. Thus, this doesn't harm a player's
           game.) */
        abort();                                    /* abort is safe */
    }

    (windowprocs.win_server_cancel)();
}

static volatile sig_atomic_t alarmed = 0;
static void
handle_sigalrm(int signum)
{
    /* We use alarm() to implement the timeout on locking. The handler does
       nothing, but because it exists and isn't SIG_IGN, it'll interrupt our
       lock attempt with EINTR. Then change_fd_lock will notice that alarmed was
       set. */
    alarmed = 1;
    (void) signum;
}

/* WARNING: This function can be called async-signal on UNIX. Thus, all system
   calls it makes must be async-signal-safe (except if !on_logfile, we can be a
   little laxer then), and it can't touch globals.

   All system calls have an "is safe" comment added when they are verified to be
   safe. Make sure you check "man 7 signal" before adding such a comment
   yourself. (If a system call doesn't have such a comment, someone forgot to
   check it for safety.) */
boolean
change_fd_lock(int fd, boolean on_logfile, enum locktype type, int timeout)
{
    struct flock sflock;
    struct sigaction saction, oldsaction;
    sigset_t sigset;
    int ret;
    int pid = -1;

    if (fd == -1)
        return FALSE;

    if (type == LT_MONITOR && !on_logfile)
        panic("Attempt to monitor lock something other than the logfile");

    if (on_logfile) {
        /* On the logfile, upgrading from monitor to write is quite difficult,
           because of a potential race condition (if it happens from two
           processes at once, they both SIGRTMIN+1 the other, then they both
           mask SIGRTMIN+1 before the other SIGRTMIN+1 arrives), they'll each
           wait indefinitely for the other to unlock the file. However, we only
           do the upgrade in three circumstances: recovering the lockfile,
           starting to update the lockfile, and the new game sequence. When
           recovering, we don't care about monitoring anyway. When starting to
           update, we do care, but the logfile handling code checks for changes
           manually immediately after doing the lock. In the new game sequence,
           only one process should be writing.

           Thus, in each case, we unlock the file entirely before establishing
           the write lock. This prevents a deadlock (one of the write locks is
           guaranteed to succeed before the other can be established), and the
           loss of monitor information is not fatal.

           This is important for another reason: if we hold a read lock when
           sending SIGRTMIN+1, the other process will tell us to relinquish our
           lock so that we can take the lock, which just causes confusion
           (because each process is blocking on the other's SIGRTMIN+2). */
        if (type == LT_WRITE) {
            sflock.l_type = F_UNLCK;
            sflock.l_whence = SEEK_SET;
            sflock.l_start = 0;
            sflock.l_len = 0;
            fcntl(fd, F_SETLK, &sflock);
        }

        /* Set our SIGRTMIN+1 and SIGRTMIN+2 handlers based on the lock type. */
        if (type == LT_NONE) {
            signal(SIGRTMIN+1, SIG_IGN);              /* signal is safe */
            signal(SIGRTMIN+2, SIG_IGN);              /* signal is safe */
        } else {
            saction.sa_flags = 0;
            sigemptyset(&saction.sa_mask);            /* sigemptyset is safe */
            sigaddset(&saction.sa_mask, SIGRTMIN+1);  /* sigaddset is safe */
            sigaddset(&saction.sa_mask, SIGRTMIN+2);  /* sigaddset is safe */

            saction.sa_handler = handle_sigrtmin1;
            sigaction(SIGRTMIN+1, &saction, NULL);    /* sigaction is safe */
            saction.sa_handler = handle_sigrtmin2;
            sigaction(SIGRTMIN+2, &saction, NULL);    /* sigaction is safe */
        }

        /* Mask SIGRTMIN+1 at LT_READ or higher. */
        sigemptyset(&sigset);                         /* sigemptyset is safe */
        sigaddset(&sigset, SIGRTMIN+1);               /* sigaddset is safe */
        if (type == LT_READ || type == LT_WRITE)
            sigprocmask(SIG_BLOCK, &sigset, NULL);    /* sigprocmask is safe */
        else
            sigprocmask(SIG_UNBLOCK, &sigset, NULL);  /* sigprocmask is safe */
    }

    saction.sa_handler = handle_sigalrm;
    saction.sa_flags = SA_RESETHAND;
    sigemptyset(&saction.sa_mask);                    /* sigemptyset is safe */
    sigaction(SIGALRM, &saction, &oldsaction);        /* sigaction is safe */

    alarmed = 0;
    if (on_logfile && type == LT_WRITE)
        alarm(1);
    else if (timeout) {
        alarm(timeout);                               /* alarm is safe */
        timeout = 0;
    }

    sflock.l_type =
        type == LT_WRITE ? F_WRLCK :
        type == LT_READ ? F_RDLCK :
        type == LT_MONITOR ? F_RDLCK :
        type == LT_NONE ? F_UNLCK :
        (impossible("invalid lock type in change_fd_lock"), F_UNLCK);
    sflock.l_whence = SEEK_SET;
    sflock.l_start = 0;
    sflock.l_len = 0;

    do {
        /* When writing, tell monitoring processes to release their locks. */
        if (on_logfile && type == LT_WRITE) {
            int pid2 = sigrtmin1_some_locker(fd);
            if (pid2 != -1)
                pid = pid2;
            alarmed = 0;
            alarm(1);
        }
    
        do {
            errno = 0;
            ret = fcntl(fd, F_SETLKW, &sflock) >= 0;   /* fcntl is safe */
        } while (!ret && errno == EINTR && !alarmed);
    } while (alarmed && timeout--);

    /* If a process relinquished its lock, tell it it can grab the lock again.
       (This will actually make it block until we release our write lock, but
       that's OK). This doesn't work reliably if we ended up having to
       SIGRTMIN+1 more than one process, but that only happens in exceptional
       conditions, and the only symptom is a 3-second freeze in some watching
       process, so that's acceptable; SIGRTMIN+1ing more than one process only
       happens in case of race condition, or if a process crashes before it can
       relay the SIGRTMIN+1. */
    if (pid != -1)
        kill(pid, SIGRTMIN+2);                        /* kill is safe */

    alarm(0);                                         /* alarm is safe */
    sigaction(SIGALRM, &oldsaction, NULL);            /* sigaction is safe */
    
    return ret;
}

#elif defined (WIN32)   /* windows versionf of lock_fd(), unlock_fd() */

/* lock any open file using LockFile */
boolean
change_fd_lock(int fd, boolean on_logfile, enum locktype type, int timeout)
{
    HANDLE hFile;
    BOOL ret;

    (void) on_logfile;

    if (fd == -1)
        return FALSE;

    hFile = (HANDLE) _get_osfhandle(fd);

    if (type == LT_NONE || type == LT_MONITOR) {
        UnlockFile(hFile, 0, 0, 64, 0);
        return TRUE;
    }

    /* TODO: Check that the return value of this has the right semantics
       (it didn't in the UNIX code); check to see if separate read locks
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

    if (!program_state.in_paniclog) {
        program_state.in_paniclog = 1;
        lfile = fopen_datafile(PANICLOG, "a", TROUBLEPREFIX);
        if (lfile) {
            fprintf(lfile, "%s %08ld: %s %s\n", version_string(),
                    yyyymmdd(utc_time()), type, reason);
            fclose(lfile);
        }
        program_state.in_paniclog = 0;
    }
#endif /* PANICLOG */
    return;
}

/* ----------  END PANIC/IMPOSSIBLE LOG ----------- */

/*files.c*/

