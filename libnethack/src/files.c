/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#if defined(AIMAKE_BUILDOS_MSWin32)
# define WIN32_LEAN_AND_MEAN
# include <Windows.h> /* must be before compilers.h */

# if !defined(S_IRUSR)
#  define S_IRUSR _S_IREAD
#  define S_IWUSR _S_IWRITE
# endif
#endif

#include "hack.h"
#include "dlb.h"

#include <ctype.h>
#include <fcntl.h>

#include <errno.h>

/* #define DEBUG */

#if defined(WIN32)
# include <sys/stat.h>
#else
# include <signal.h>
# include <sys/select.h>
# ifdef AIMAKE_BUILDOS_linux
#  include <ucontext.h>
# endif
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

#define FQN_NUMBUF 4
static char fqn_filename_buffer[FQN_NUMBUF][FQN_MAX_FILENAME];

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

        buf = malloc(fsize + 1);
        dlb_fread(buf, fsize, 1, fp);
        buf[fsize] = '\0';

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


static const char *
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

    if (strcmp(fqn_prefix[prefix], "$OMIT") == 0)
        return NULL;

    filename = fqname(filename, prefix, prefix == TROUBLEPREFIX ? 3 : 0);
    fp = fopen(filename, mode);

    /* prefix check is to prevent infinite recursion if we can't open the
       panic log */
    if (!fp && prefix != TROUBLEPREFIX)
        paniclog("nofile", filename);

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

    if (strcmp(fqn_prefix[prefix], "$OMIT") == 0)
        return -1;

    filename = fqname(filename, prefix, prefix == TROUBLEPREFIX ? 3 : 0);

    /* If the file doesn't exist, we want to create it with the permissions
       that it would have had if the installer created it.

       For the files that we open via open_datafile and fopen_datafile:

       u+r permissions:
           nhdat       (DATAPREFIX)

       u+rw permissions:
           dump files  (DUMPPREFIX)

       ug+rw permissions:
           paniclog    (TROUBLEPREFIX)
           logfile     (SCOREPREFIX)
           xlogfile    (SCOREPREFIX)
           record      (SCOREPREFIX)

       nhdat can't meaningfully be created, so we only have to worry about
       the others.
    */

#ifdef S_IRGRP   /* the OS has per-group permissions */
    if (prefix == SCOREPREFIX || prefix == TROUBLEPREFIX)
        fd = open(filename, oflags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    else
        fd = open(filename, oflags, S_IRUSR | S_IWUSR);
#else            /* the OS doesn't have per-group permissions */
    fd = open(filename, oflags, S_IRUSR | S_IWUSR);
#endif
    return fd;
}


/* ----------  BEGIN BONES FILE HANDLING ----------- */

/* caller is responsible for deallocating */
char *
bones_filename(const char *bonesid)
{
    static const char base[] = "bonesnn.xxx";
    char *fn = malloc(SIZE(base));
    strncpy(fn, base, SIZE(base));
    snprintf(fn, SIZE(base), "bon%s", bonesid);
    return fn;
}

int
create_bonesfile(const char *bonesid, const char **errbuf)
{
    const char *file;
    char tempname[PL_NSIZ + 32];
    int fd;

    if (strcmp(fqn_prefix[BONESPREFIX], "$OMIT") == 0)
        return -1;

    if (errbuf)
        *errbuf = "";
    snprintf(tempname, SIZE(tempname), "%d%s.bn", (int)getuid(), u.uplname);
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

    if (strcmp(fqn_prefix[BONESPREFIX], "$OMIT") == 0)
        return;

    char *bonesfn = bones_filename(bonesid);
    fq_bones = fqname(bonesfn, BONESPREFIX, 0);
    snprintf(tempbuf, SIZE(tempbuf), "%d%s.bn", (int)getuid(), u.uplname);
    tempname = fqname(tempbuf, BONESPREFIX, 1);

    ret = rename(tempname, fq_bones);
    if (wizard && ret != 0)
        pline("couldn't rename %s to %s.", tempname, fq_bones);

    free(bonesfn);
}


int
open_bonesfile(char *bonesid)
{
    const char *fq_bones;
    int fd;

    char *bonesfn = bones_filename(bonesid);
    fq_bones = fqname(bonesfn, BONESPREFIX, 0);

    fd = open(fq_bones, O_RDONLY | O_BINARY, 0);

    free(bonesfn);
    return fd;
}


int
delete_bonesfile(char *bonesid)
{
    if (strcmp(fqn_prefix[BONESPREFIX], "$OMIT") == 0)
        return 0;

    char *bonesfn = bones_filename(bonesid);
    nh_bool ret = unlink(fqname(bonesfn, BONESPREFIX, 0)) >= 0;

    free(bonesfn);
    return ret;
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
 * The implementation varies by OS. On Linux (and other POSIX with real-time
 * signals):
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
 *   establish it, we SIGRTMIN+1 processes holding it until we can; SIGRTMIN+1
 *   is blocked, but only after the write lock has been established (so that if
 *   two processes want to write at once, they don't deadlock on each other)
 *
 * The key invariant here is *if a file is write-locked by any process, all
 * processes have been informed of the write*. This is the case regardless of
 * any signals sent anywhere; if a process has a write lock, then there are no
 * processes with a read lock (by definition), and all processes with monitor
 * locks have reliquished their locks in order to allow the write (and thus are
 * aware of the write). The only purpose of the signals is to allow a write lock
 * to be established in the first place (SIGRTMIN+1), to inform other processes
 * that the write lock has been established so that they don't re-lock the file
 * before the write can happen (SIGRTMIN+2), and to inform other processes that
 * the file is now unlocked (SIGRTMIN+4). None of these signals have any effect
 * on write notification; they are both just to make the writes actually capabe
 * of handling.
 *
 * The code, and documentation, is written in terms of SIGRTMIN+1 for the
 * "unlock" signal, but there is also a second unlock signal, SIGRTMIN+0, which
 * works identically in most ways. The difference is that processes use
 * SIGRTMIN+1 when communicating with processes with higher PIDs, and SIGRTMIN+0
 * when communicating with processes with lower PIDs. This introduces asymmetry
 * that is used to resolve otherwise symmetrical situations, like two processes
 * trying to write simultaneously. There is likewise a SIGRTMIN+3 which
 * corresponds to SIGRTMIN+2, for communicating with processes with higher PIDs.
 * (This is so that 0 and 2, and 1 and 3, can always be masked simultaneously to
 * get the signals in the right order).
 *
 * The behaviour on Windows is currently much more primitive (any Windows
 * experts out there to help?): we lock the file at LT_READ or higher, and leave
 * it unlocked at LT_MONITOR or lower. This means that watching games locally
 * doesn't work properly (watching games played on a server still works, though,
 * because the server itself is running UNIX, and the mechanism via which server
 * cancels are relayed to the client has nothing to do with the locking
 * mechanism). Watching local games is still possible on Windows, but the screen
 * will not update until a key is pressed.  (In this sense, watching a game is
 * quite like resizing a window; both only react on keypress.)
 */


#ifdef AIMAKE_BUILDOS_linux
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
 * it (thus implementing a read lock), SIGRTMIN+1 is masked while performing a
 * read operation; a process can ask us to unlock the file, but until we're done
 * reading, we're not going to (and we're not even going to look at the request,
 * we'll get to it once the reading's over).
 *
 * TODO: Should these be namespaced? program_state is the obvious namespace for
 * them. If namespacing this global (and others like it), remember to keep the
 * "volatile".
 */
static volatile sig_atomic_t unmatched_sigrtmin1s_incoming = 0;
static volatile sig_atomic_t unmatched_sigrtmin1s_outgoing = 0;
static volatile sig_atomic_t alarmed = 0;

/* Tells watchers they can unrelinquish the lock. This should not be called
   from a signal handler (but can, and is, be called from terminate). */
void
flush_logfile_watchers(void)
{
    while (program_state.logfile_watcher_count--) {
        pid_t pid = ((pid_t *)program_state.logfile_watchers)
            [program_state.logfile_watcher_count];
#ifdef DEBUG
        fprintf(stderr, "%6ld: sending SIGRTMIN+2 to %ld\n",
                (long)getpid(), (long)pid);
        fflush(stderr);
#endif

        sigqueue(pid, pid < getpid() ? SIGRTMIN+2 : SIGRTMIN+3,
                 (union sigval){.sival_int = 0});
    }

    free(program_state.logfile_watchers);
    program_state.logfile_watchers = NULL;
    program_state.logfile_watcher_count = 0;
}

/* Unrelinquish the logfile lock. This just sets a flag that's read by
   handle_sigrtmin1.

   Fun note: Quite a few API functions have an extra void * argument reserved
   for future expansion, that's meant to be set to NULL until a use for it is
   thought up. The callback to sigaction() is the first time I've actually seen
   it be given a purpose. (Not that most people care about the context.) */
static void
handle_sigrtmin2(int signum, siginfo_t *siginfo, void *context)
{
    (void) signum;
    (void) siginfo;
    (void) context;
    /* If someone's sending us spurious SIGRTMIN+2 signals, ignore them. (This
       could also happen if a SIGRTMIN+2 gets delayed for over 3 seconds; very
       unlikely, but possible, e.g. the computer goes into suspend mode with a
       very precise timing).

       The reason we use SIGRTMIN+2, not SIGUSR2, is that if a SIGRTMIN+1 and a
       SIGRTMIN+2 are sent in that order, the SIGRTMIN+1 is 100% guaranteed to
       arrive first, something that is not true of the more normal SIGUSR*
       signals. (If they are sent in the other order, higher then lower, then
       they could arrive either way round.) */
    if (unmatched_sigrtmin1s_incoming)
        unmatched_sigrtmin1s_incoming--;
#ifdef DEBUG
    fprintf(stderr, "%6ld: received SIGRTMIN+2 from %ld,"
            " unmatched incoming %d\n", (long)getpid(), (long)siginfo->si_pid,
            (int)unmatched_sigrtmin1s_incoming);
    fflush(stderr);
#endif
}

/* Very similar to the previous case, but for a different sort of
   acknowledgement; "I received your signal", rather than "My signal is no
   longer relevant". */
static void
handle_sigrtmin4(int signum, siginfo_t *siginfo, void *context)
{
    (void) signum;
    (void) siginfo;
    (void) context;

    if (unmatched_sigrtmin1s_outgoing)
        unmatched_sigrtmin1s_outgoing--;
#ifdef DEBUG
    fprintf(stderr, "%6ld: received SIGRTMIN+4 from %ld,"
            " unmatched outgoing %d\n", (long)getpid(), (long)siginfo->si_pid,
            (int)unmatched_sigrtmin1s_outgoing);
    fflush(stderr);
#endif
}


/* Called when we get a second request to relinquish the lock while the lock is
   already relinquished. This is baically handle_sigrtmin1 without the part that
   actually does the lock processing. */
static void
handle_sigrtmin1_recursive(int signum, siginfo_t *siginfo, void *context)
{
    (void) signum;
    (void) context;

    unmatched_sigrtmin1s_incoming++;
#ifdef DEBUG
    fprintf(stderr, "%6ld: received SIGRTMIN+1 from %ld,"
            " unmatched incoming %d\n", (long)getpid(), (long)siginfo->si_pid,
            (int)unmatched_sigrtmin1s_incoming);
    fflush(stderr);
#endif

    sigqueue(siginfo->si_pid, SIGRTMIN+4, (union sigval){.sival_int = 0});
#ifdef DEBUG
    fprintf(stderr, "%6ld: sending SIGRTMIN+4 to process %ld\n",
            (long)getpid(), (long)siginfo->si_pid);
    fflush(stderr);
#endif
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

   SIGRTMIN+1..3 are blocked by the kernel upon entrance to this function, and
   unblocked on exit (if they were unblocked before), due to the way it's
   installed. */
static void
handle_sigrtmin1(int signum, siginfo_t *siginfo, void *context)
{
    struct timespec timeout = {.tv_sec = 3, .tv_nsec = 0};
    struct sigaction saction, oldsaction0, oldsaction1;
    int save_errno = errno;
    sig_atomic_t save_alarmed = alarmed;
    sigset_t sigset;

    (void) signum;

    if (siginfo->si_code != SI_QUEUE)
        return; /* not from a NetHack 4 process */

    unmatched_sigrtmin1s_incoming++;

#ifdef DEBUG
    fprintf(stderr, "%6ld: received SIGRTMIN+1 from %ld, "
            "unmatched incoming %d\n", (long)getpid(), (long)siginfo->si_pid,
            (int)unmatched_sigrtmin1s_incoming);
    fflush(stderr);
#endif

    /* If we receive SIGRTMIN+1 recursively, use a simpler handler, because the
       file will be unlocked already; we don't look for signals in this handler
       until the file is already unlocked. (We could postpone the SIGRTMIN+1
       until we return, instead; this works, but has performance issues, in that
       it gives quadratic performance in the signal overhead.) */
    saction.sa_sigaction = handle_sigrtmin1_recursive;
    sigemptyset(&saction.sa_mask);
    sigaddset(&saction.sa_mask, SIGRTMIN+0);
    sigaddset(&saction.sa_mask, SIGRTMIN+1);
    sigaddset(&saction.sa_mask, SIGRTMIN+2);
    sigaddset(&saction.sa_mask, SIGRTMIN+3);
    sigaddset(&saction.sa_mask, SIGRTMIN+4);
    saction.sa_flags = SA_SIGINFO;

    sigaction(SIGRTMIN+0, &saction, &oldsaction0);  /* sigaction is safe */
    sigaction(SIGRTMIN+1, &saction, &oldsaction1);  /* sigaction is safe */

    /* Give up the lock. This has on_logfile == FALSE because we don't want to
       do all the signal handling bookkeeping, just the actual file lock. */
    change_fd_lock(program_state.logfile, FALSE, LT_NONE, 0);

    /* Cause the process that sent us the signal to EINTR out of its lock
       attempt, if there are other processes that also have the lock (maybe even
       if there aren't, but that's OK; it'll just try again). This lets it
       signal those processes. (An older version of the code tried to signal
       those processes itself, but that's fraught with complexity.) */
    sigqueue(siginfo->si_pid, SIGRTMIN+4, (union sigval){.sival_int = 0});
#ifdef DEBUG
    fprintf(stderr, "%6ld: sending SIGRTMIN+4 to process %ld\n",
            (long)getpid(), (long)siginfo->si_pid);
    fflush(stderr);
#endif

    /* Wait until all SIGRTMIN+1s are unmatched, or 3 seconds elapse with no
       signal (in which case we assume that either we missed it, perhaps because
       there's a stuck/crashed process somewhere). We do this by unblocking
       SIGRTMIN+1 and SIGRTMIN+2 temporarily; this lets us receive recursive
       SIGRTMIN+1 signals and SIGRTMIN+2 signals.

       Actually doing the temporary unblock is hard; sigtimedwait lets us
       simulate one, but is not thread-safe (and the UI may be threaded);
       pselect is safe, but needs an absolute rather than relative signal mask,
       and we can't get the current mask to compare from sigprocmask (not
       thread-safe) or pthread_sigmask (not async-signal-safe). Using an empty
       mask doesn't work either; the UI might want to block SIGPIPE, for
       instance.

       The solution is to use the third argument to the signal handler; it holds
       what is effectively a siglongjmp() buffer, but with some fields split out
       in a portable way. One of those fields is the signal mask that was in
       place before the signal happened, which is just what we need; we don't
       even need to do operations on it to specify the signal we need. Thus, the
       solution to there being no appropriate API calls is that there's a way to
       get the information with no API calls.

       We do, however, need to delete the signals we want to receive from the
       signal mask; this is SIGRTMIN+1,3,4. (We take 0 and 2 from the signal
       mask.) The problem is that the mask in ucontext can have signals masked
       even if they were temporarily unmasked by pselect, sigsuspend, or the
       like. Somehow, strace manages to report this argument incorrectly, even
       if its value is copied to another memory location. (A small test program
       shows the argument containing no signals, yet behaving differently if
       signals are removed from it.)

       Interestingly, setcontext(), the POSIX API call that is intended to be
       used with the third argument, was removed from POSIX in 2008. However,
       the third argument to a sigaction() callback is still there, even though
       this is close to the only use that can be made of it (especially because
       SUSv2 documents that setcontext() doesn't actually work for exiting
       signal handlers).

       Huh, I guess that void * was useful after all. Now I wonder if it's a
       void * for future expansion, or just a void * because most people don't
       need ucontext.h, given that it defines a mechanism that doesn't actually
       work. */

    sigset = ((ucontext_t *)context)->uc_sigmask;
    sigdelset(&sigset, SIGRTMIN+1);
    sigdelset(&sigset, SIGRTMIN+3);
    sigdelset(&sigset, SIGRTMIN+4);

    errno = 0;
    while (unmatched_sigrtmin1s_incoming &&         /* pselect is safe */
           pselect(0, 0, 0, 0, &timeout, &sigset) < 0 && errno == EINTR) {
        errno = 0;
    }

    unmatched_sigrtmin1s_incoming = 0; /* in case we got here by timeout */

    /* Restore the signal handlers before we do anything that might exit. */
    sigaction(SIGRTMIN+0, &oldsaction0, NULL);      /* sigaction is safe */
    sigaction(SIGRTMIN+1, &oldsaction1, NULL);      /* sigaction is safe */

    /* Block until the other process has finished writing, then relock the
       logfile. We only partially undid the monitor lock earlier (we removed the
       lock on the file itself, but not the rest of the monitoring state); thus,
       to re-establish the monitor lock, we place a read lock on the underlying
       file. */
    if (!change_fd_lock(program_state.logfile, FALSE, LT_READ, 3)) {
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

    /* While running a zero-time command, instead of following the other
       process "live", we freeze the gamestate until the command ends. If not
       in a zero-time command, we follow other processes that are playing the
       same game. Exception: in replay mode, we're always frozen.

       win_server_cancel is defined as async-signal by the API documentation. */
    if (program_state.game_running && program_state.followmode != FM_REPLAY &&
        !program_state.in_zero_time_command)
        (windowprocs.win_server_cancel)();

    errno = save_errno;
    alarmed = save_alarmed;
}

static void
handle_sigalrm(int signum, siginfo_t *siginfo, void *context)
{
    (void)signum;
    (void)siginfo;
    (void)context;
    alarmed = 1;
}


/* This function has two modes of operation. If on_logfile is FALSE, then this
   is async-signal-safe. Otherwise, this is not async-signal-safe.

   All system calls have an "is safe" comment added when they are verified to be
   safe. Make sure you check "man 7 signal" before adding such a comment
   yourself. (If a system call doesn't have such a comment, someone forgot to
   check it for safety.) */
boolean
change_fd_lock(int fd, boolean on_logfile, enum locktype type, int timeout)
{
    struct flock sflock, sflock_copy;
    struct sigaction saction, oldsaction;
    sigset_t sigset, oldsigset;
    int ret, oldtimeout;

    if (fd == -1)
        return FALSE;

    if (type == LT_MONITOR && !on_logfile)
        panic("Attempt to monitor lock something other than the logfile");

    if (on_logfile) {
        /* Set our SIGRTMIN+0..4 handlers based on the lock type. */
        if (type == LT_NONE) {
            signal(SIGRTMIN+0, SIG_IGN);              /* signal is safe */
            signal(SIGRTMIN+1, SIG_IGN);              /* signal is safe */
            signal(SIGRTMIN+2, SIG_IGN);              /* signal is safe */
            signal(SIGRTMIN+3, SIG_IGN);              /* signal is safe */
            signal(SIGRTMIN+4, SIG_IGN);              /* signal is safe */
        } else {
            saction.sa_flags = SA_SIGINFO;
            sigemptyset(&saction.sa_mask);            /* sigemptyset is safe */
            sigaddset(&saction.sa_mask, SIGRTMIN+0);  /* sigaddset is safe */
            sigaddset(&saction.sa_mask, SIGRTMIN+1);  /* sigaddset is safe */
            sigaddset(&saction.sa_mask, SIGRTMIN+2);  /* sigaddset is safe */
            sigaddset(&saction.sa_mask, SIGRTMIN+3);  /* sigaddset is safe */
            sigaddset(&saction.sa_mask, SIGRTMIN+4);  /* sigaddset is safe */

            saction.sa_sigaction = handle_sigrtmin1;
            sigaction(SIGRTMIN+0, &saction, NULL);    /* sigaction is safe */
            sigaction(SIGRTMIN+1, &saction, NULL);    /* sigaction is safe */
            saction.sa_sigaction = handle_sigrtmin2;
            sigaction(SIGRTMIN+2, &saction, NULL);    /* sigaction is safe */
            sigaction(SIGRTMIN+3, &saction, NULL);    /* sigaction is safe */
            saction.sa_sigaction = handle_sigrtmin4;
            sigaction(SIGRTMIN+4, &saction, NULL);    /* sigaction is safe */
        }
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

    /* Most of what we're doing is signal-based, so convert the timeout into
       a signal. */
    saction.sa_flags = SA_SIGINFO;
    sigemptyset(&saction.sa_mask);            /* sigemptyset is safe */
    saction.sa_sigaction = handle_sigalrm;
    sigaction(SIGALRM, &saction, &oldsaction);
    if (timeout == 0)
        timeout = 1;

    if (on_logfile) {
        /* Semantics we want: wait until either we can set a lock, or we receive
           a signal. Apparently easy, fcntl does that (if the signal isn't set
           to SA_RESTART). The problem is that the signal can arrive between the
           sigrtmin1_some_locker call and the fcntl call, in which case, the
           wait happens too long; and there's no pfcntl() call to help us out.
       
           One thing we can do, though, is attempt to set the lock in a
           non-blocking manner. If it works, fine. If it doesn't, we can wait
           until we receive a signal. Thus, we only have to consider reasons why
           the lock might become establishable without a signal. For a read
           lock, this involves process A finishing writing when it was process B
           that told us to relinquish the lock (for an unrelated write). For a
           write lock, this involves a process relinquishing on another
           process's request, because it was trying to do an unrelated
           write. (When there isn't a third unrelated process involved, we get
           SIGRTMIN+2 and SIGRTMIN+4 respectively in these cases.)
       
           Thus, we use the following algorithm: if we know a reason why the
           file would be locked, we block on a signal, because we're going to
           get one; if we don't know a reason why the file would be locked, we
           alternate between attempting to set the lock, and checking to see who
           holds a conflicting lock and telling them to let go of it.

           All this time, to avoid race conditions, we keep the relevant signals
           masked. The functions here do not have to be async-signal-safe (a
           good thing too, because some of them aren't). */

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGRTMIN+0);
        sigaddset(&sigset, SIGRTMIN+1);
        sigaddset(&sigset, SIGRTMIN+2);
        sigaddset(&sigset, SIGRTMIN+3);
        sigaddset(&sigset, SIGRTMIN+4);
        sigaddset(&sigset, SIGALRM);
        pthread_sigmask(SIG_BLOCK, &sigset, &oldsigset);

        /* We want to see SIGRTMIN+1,3,4 and SIGALRM, even if they were masked
           before. We do /not/ see SIGRTMIN+0,2. That way, processes with higher
           PIDs cannot take locks from us, and we can always take locks from
           processes with lower PIDs, introducing asymmetry that prevents
           deadlock. */
        sigaddset(&oldsigset, SIGRTMIN+0);
        sigaddset(&oldsigset, SIGRTMIN+2);

        sigdelset(&oldsigset, SIGRTMIN+1);
        sigdelset(&oldsigset, SIGRTMIN+3);
        sigdelset(&oldsigset, SIGRTMIN+4);
        sigdelset(&oldsigset, SIGALRM);

        alarmed = 0;
        oldtimeout = alarm(timeout);
        
        while (1) {
            /* Do we know a reason why the lock won't hold? If so, wait.

               This also does things like handle requests to unlock the file
               that came in while we held a write lock, as we downgrade to a
               monitor lock. */
            while ((unmatched_sigrtmin1s_incoming ||
                    unmatched_sigrtmin1s_outgoing) && !alarmed)
                sigsuspend(&oldsigset);

            if (alarmed) {
                /* Assume the signals got lost. We're probably going to go into
                   panic mode anyway, and we don't want them blocking the
                   panic. */
                unmatched_sigrtmin1s_incoming = 0;
                unmatched_sigrtmin1s_outgoing = 0;
                ret = 0;
                break;
            }

            /* Can we place the lock? */
            errno = 0;
            ret = fcntl(fd, F_SETLK, &sflock) >= 0;
            if (ret)
                break; /* Yes, we can! */

            /* Not even sure this is possible for a nonblocking lock set, and it
               would have to be a signal we don't otherwise handle ourselves
               (perhaps the client is handling SIGWINCH?), but... */
            if (errno == EINTR)
                continue;

            /* Has something gone unexpectedly wrong? If the failure to place
               the lock is due to a conflicting lock, that's EAGAIN or EACCES
               (depending on what mood the kernel's in that day, or
               something). If it's for some other reason (e.g. too many locks),
               we should give up. */
            if (errno != EAGAIN && errno != EACCES)
                break;

            /* fcntl thought there was a conflicting lock. Try to get details on
               it. (It might be unlocked behind our backs, in which case we'll
               be told there's no lock, but that's fine; just try again.) */

            sflock_copy = sflock;
            sflock_copy.l_pid = -2; /* not necessary, but valgrind doesn't know
                                       that fcntl initializes this, so we
                                       initialize it ourselves to let valgrind
                                       know what's happening */

            if (fcntl(fd, F_GETLK, &sflock_copy) < 0)
                break;         /* this shouldn't be happening */

            if (sflock_copy.l_type == F_UNLCK)
                continue;      /* apparently it got unlocked by itself */

            /* Signal the process that holds the lock, and ask it to unlock it.
               This serves three purposes: getting the lock unlocked; telling
               that process to check the file again to see if it's changed; and
               getting it to give us a response signal (SIGRTMIN+4) when it's
               done (thus interrupting our sigsuspend()). */
            unmatched_sigrtmin1s_outgoing++;
#ifdef DEBUG
            fprintf(stderr, "%6ld: sending SIGRTMIN+1 to %ld, "
                    "unmatched outgoing %d\n",
                    (long)getpid(), (long)sflock_copy.l_pid,
                    (int)unmatched_sigrtmin1s_outgoing);
            fflush(stderr);
#endif
            sigqueue(sflock_copy.l_pid, sflock_copy.l_pid < getpid() ?
                     SIGRTMIN+0 : SIGRTMIN+1, (union sigval){.sival_int = 0});

            /* logfile_watchers is a list of processes that we told to unlock
               the file; we need to track this so we can tell them that it's OK
               to lock the file again. */
            program_state.logfile_watchers = realloc(
            program_state.logfile_watchers,
                (program_state.logfile_watcher_count+1) * sizeof (pid_t));
            ((pid_t *)program_state.logfile_watchers)
                [program_state.logfile_watcher_count++] = sflock_copy.l_pid;
        }
            
        /* Note: We leave our signal mask in place for a bit. If we're setting a
           read or write lock, we want to keep the signals masked for the
           duration of the lock, and unmasking them in between means that we
           might get a SIGRTMIN+1, harmless for a read lock, but dangerous for a
           write lock because it would drop the lock down to a read lock and
           we'd have to go and re-establish it again. */
    } else {
        /* We want the same semantics, except without the signal handling.  This
           is much easier. We reset the alarm each time around the loop to avoid
           a race condition due to getting some unexpected signal just before
           the alarm runs out. */
        do {
            alarmed = 0;
            oldtimeout = alarm(timeout);               /* alarm is safe */
            ret = fcntl(fd, F_SETLKW, &sflock) >= 0;   /* fcntl is safe */
        } while (!ret && errno == EINTR && !alarmed);
    }
    
#ifdef DEBUG
    if (alarmed)
        fprintf(stderr, "%6ld: alarm() timeout!\n",
                (long)getpid());
    else if (ret)
        fprintf(stderr, "%6ld: established %slock on fd %d\n",
                (long)getpid(), type == LT_NONE ? "un" :
                type == LT_MONITOR ? "monitor "
                : type == LT_READ ? "read " : "write ", fd);
    else
        fprintf(stderr, "%6ld: fcntl failed to %slock fd %d: %s\n",
                (long)getpid(), type == LT_NONE ? "un" :
                type == LT_MONITOR ? "monitor "
                : type == LT_READ ? "read " : "write ", fd,
                strerror(errno));
                
    fflush(stderr);
#endif

    if (on_logfile) {
        /* We successfully got our lock. Tell the watchers that they can start
           monitoring again. We need to do this before unblocking signals;
           otherwise, the process whose unlock request we respond to might not
           be able to act on it because it's waiting for an ok-to-relock
           message from us, leading to a deadlock. */
        if (on_logfile)
             flush_logfile_watchers();

        /* We mask SIGRTMIN+1 at LT_READ or higher, and unmask it at LT_MONITOR
           or lower (ditto SIGRTMIN+0). We also have to mask SIGRTMIN+2 at the
           same time, to prevent the signals arriving in the wrong
           order. There's no need to mask SIGRTMIN+4, given that it shouldn't be
           arriving anyway, and if it does, we want to discard it.

           We actually have all the relevant signals masked already (to prevent
           us losing a write lock due to receiving a SIGRTMIN+1). Thus, we want
           to unmask SIGRTMIN+4 and SIGALRM unconditionally, and also SIGRTMIN+0
           to SIGRTMIN+3 if our lock is monitor or lower. */
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGRTMIN+4);
        sigaddset(&sigset, SIGALRM);
        if (type != LT_READ && type != LT_WRITE) {
            sigaddset(&sigset, SIGRTMIN+0);
            sigaddset(&sigset, SIGRTMIN+1);
            sigaddset(&sigset, SIGRTMIN+2);
            sigaddset(&sigset, SIGRTMIN+3);
        }

        pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
    }
    
    alarm(oldtimeout);                                /* alarm is safe */
    sigaction(SIGALRM, &oldsaction, NULL);            /* sigaction is safe */

    return ret;
}

#elif defined (UNIX)
/* lock any open file using fcntl; this is used for Unices like Darwin that
   don't support real-time signals and so don't support multiprocess play */

static void
handle_sigalrm(int signum)
{
    /* We use alarm() to implement the timeout on locking. The handler does
       nothing, but because it exists and isn't SIG_DFL, it'll interrupt our
       lock attempt with EINTR. */
    (void) signum;
}

boolean
change_fd_lock(int fd, boolean on_logfile, enum locktype type, int timeout)
{
    struct flock sflock;
    struct sigaction saction, oldsaction;
    int ret;

    if (fd == -1)
        return FALSE;

    if (type == LT_MONITOR && !on_logfile)
        panic("Attempt to monitor lock something other than the logfile");

    saction.sa_handler = handle_sigalrm;
    saction.sa_flags = SA_RESETHAND;
    sigemptyset(&saction.sa_mask);
    sigaction(SIGALRM, &saction, &oldsaction);
    if (timeout)
        alarm(timeout);

    sflock.l_type =
        type == LT_WRITE ? F_WRLCK :
        type == LT_READ ? F_RDLCK :
        type == LT_NONE || type == LT_MONITOR ? F_UNLCK :
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
change_fd_lock(int fd, boolean on_logfile, enum locktype type, int timeout)
{
    HANDLE hFile;
    BOOL ret;

    (void) on_logfile;

    if (fd == -1)
        return FALSE;

    hFile = (HANDLE) _get_osfhandle(fd);

    UnlockFile(hFile, 0, 0, 64, 0); /* prevent issues with recursive locks */

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
