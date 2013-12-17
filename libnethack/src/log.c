/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-12-17 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "patchlevel.h"
#include "iomodes.h"
#include <zlib.h>
/* stdint.h, inttypes.h let us printf long longs portably */
#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>

/* #define DEBUG */

static void log_reset(void);
static void log_binary(const char *buf, int buflen);
static long get_log_offset(void);

/***** Error handling *****/

static void NORETURN
error_reading_save(char *reason)
{
    if (strchr(reason, '%'))
        raw_printf(reason, get_log_offset());
    else
        raw_printf("%s", reason);

    log_reset();
    terminate(ERR_RESTORE_FAILED);
}

/***** Base 64 handling *****/

static const unsigned char b64e[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char b64d[256] = {
    /* 32 control chars */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* ' ' - '/' */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 63,
    /* '0' - '9' */ 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    /* ':' - '@' */ 0, 0, 0, 0, 0, 0, 0,
    /* 'A' - 'Z' */ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25,
    /* '[' - '\'' */ 0, 0, 0, 0, 0, 0,
    /* 'a' - 'z' */ 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

static int
base64size(int n)
{
    return compressBound(n) * 4 / 3 + 4 + 12;   /* 12 for $4294967296$ */
}

static void
base64_encode_binary(const unsigned char *in, char *out, int len)
{
    int i, pos, rem;
    unsigned long olen = compressBound(len);
    unsigned char *o = malloc(olen);

    if (compress2(o, &olen, in, len, Z_BEST_COMPRESSION) != Z_OK) {
        panic("Could not compress input data!");
    }

    pos = sprintf(out, "$%d$", len);

    if (pos + olen >= len) {
        pos = 0;
        olen = len;
    } else
        in = o;

    for (i = 0; i < (olen / 3) * 3; i += 3) {
        out[pos] = b64e[in[i] >> 2];
        out[pos + 1] = b64e[(in[i] & 0x03) << 4 | (in[i + 1] & 0xf0) >> 4];
        out[pos + 2] = b64e[(in[i + 1] & 0x0f) << 2 | (in[i + 2] & 0xc0) >> 6];
        out[pos + 3] = b64e[in[i + 2] & 0x3f];
        pos += 4;
    }

    rem = olen - i;
    if (rem > 0) {
        out[pos] = b64e[in[i] >> 2];
        out[pos + 1] =
            b64e[(in[i] & 0x03) << 4 |
                 (rem == 1 ? 0 : (in[i + 1] & 0xf0) >> 4)];
        out[pos + 2] = (rem == 1) ? '=' : b64e[(in[i + 1] & 0x0f) << 2];
        out[pos + 3] = '=';
        pos += 4;
    }

    free(o);

    out[pos] = '\0';
}


static void
base64_encode(const char *in, char *out)
{
    base64_encode_binary((const unsigned char *)in, out, strlen(in));
}

static int
base64_strlen(const char *in)
{
    /* If the input is uncompressed, just return its size. If it's compressed,
       read the size from the header. */
    if (*in != '$')
        return strlen(in);
    return atoi(in + 1);
}

/* TODO: This should be communicating the end position of the base 64 data. */
static void
base64_decode(const char *in, char *out, int outlen)
{
    int i, len = strlen(in), pos = 0, olen;
    char *o = out;

    olen = outlen;
    if (*in == '$') {
        o = malloc(len);
        olen = len;
    }

    for (i = 0; i < len; i += 4) {

        /* skip data between $ signs, it's used for the header for compressed
           binary data */
        if (in[i] == '$')
            for (i += 2; in[i - 1] != '$' && in[i]; i++) {}

        /* decode blocks; padding '=' are converted to 0 in the decoding table */
        if (pos < olen)
            o[pos] = b64d[(int)in[i]] << 2 | b64d[(int)in[i + 1]] >> 4;
        if (pos + 1 < olen)
            o[pos + 1] = b64d[(int)in[i + 1]] << 4 | b64d[(int)in[i + 2]] >> 2;
        if (pos + 2 < olen)
            o[pos + 2] =
                ((b64d[(int)in[i + 2]] << 6) & 0xc0) | b64d[(int)in[i + 3]];
        pos += 3;

    }

    i -= 4;
    if ((in[i + 2] == '=' || !in[i + 2]) && (in[i + 3] == '=' || !in[i + 3]))
        pos--;
    if ((in[i + 1] == '=' || !in[i + 2]) && (in[i + 2] == '=' || !in[i + 3]))
        pos--;

    if (pos < olen)
        o[pos] = 0;
    else
        error_reading_save("Uncompressed base64 data was too long at %ld\n");

    if (*in == '$') {

        unsigned long blen = base64_strlen(in);
        if (blen > outlen) {
            free(o);
            error_reading_save("Compressed base64 data was too long at %ld\n");
        }
        int errcode = uncompress((unsigned char *)out, &blen,
                                 (unsigned char *)o, pos);

        free(o);
        if (errcode != Z_OK) {
            raw_printf("Decompressing save file failed at %ld: %s\n",
                       get_log_offset(),
                       errcode == Z_MEM_ERROR ? "Out of memory" : errcode ==
                       Z_BUF_ERROR ? "Invalid size" : errcode ==
                       Z_DATA_ERROR ? "Corrupted file" : "(unknown error)");
            error_reading_save("");
        }
    }
}

/***** Log I/O *****/
static long
get_log_offset(void)
{
    return lseek(program_state.logfile, 0, SEEK_CUR);
}

/* full_read and full_write are versions of the POSIX read/write that never do
   short reads or short writes. They return TRUE if all the bytes requested were
   read/written; FALSE if something went wrong (either because there weren't
   enough bytes in the file, or because of an I/O error). */
static boolean
full_read(int fd, void *buffer, int len)
{
    int rv;
    long o = lseek(fd, 0, SEEK_CUR);
    errno = 0;
    rv = read(fd, buffer, len);
    if (rv < 0 && errno == EINTR) {
        lseek(fd, o, SEEK_SET);
        return full_read(fd, buffer, len);
    }
    if (rv <= 0 || rv > len)
        return FALSE;
    if (rv == len)
        return TRUE;
    return full_read(fd, ((char *)buffer) + rv, len - rv);
}
static boolean
full_write(int fd, const void *buffer, int len)
{
    int rv;
    long o = lseek(fd, 0, SEEK_CUR);
    errno = 0;
    rv = write(fd, buffer, len);
    if (rv < 0 && errno == EINTR) {
        lseek(fd, o, SEEK_SET);
        return full_write(fd, buffer, len);
    }
    if (rv <= 0 || rv > len)
        return FALSE;
    if (rv == len)
        return TRUE;
    return full_write(fd, ((const char *)buffer) + rv, len - rv);
}

static int
lprintf(const char *fmt, ...)
{
    va_list vargs;
    char outbuf[4096];
    int size;

    va_start(vargs, fmt);
    size = vsnprintf(outbuf, sizeof (outbuf), fmt, vargs);
    va_end(vargs);

    if (!full_write(program_state.logfile, outbuf, size))
        panic("Could not write text content to the log.");

    return size;
}

static void
log_binary(const char *buf, int buflen)
{
    char *b64buf;

    if (program_state.logfile == -1)
        return;

    b64buf = malloc(base64size(buflen));
    base64_encode_binary((const unsigned char *)buf, b64buf, buflen);

    /* don't use lprintf, b64buf might be too big for the buffer used by
       lprintf */
    if (!full_write(program_state.logfile, b64buf, strlen(b64buf)))
        panic("Could not write binary content to the log.");

    free(b64buf);
}

/* Reads a line starting from the current file pointer. Returns NULL if the line
   is incomplete or spos is past EOF, otherwise mallocs enough space for the
   line and returns it. The file pointer is left at the newline, or in an
   unpredictable location in case of error. */
static char *
lgetline_malloc(int fd)
{
    char *inbuf = NULL;
    long inbuflen = 0;  /* number of bytes read */
    long fpos = 0;      /* file pointer, relative to its original location */
    void *nlloc = NULL;
    do {
        inbuflen += 4;
        inbuflen *= 3;
        inbuflen /= 2;
        inbuf = realloc(inbuf, inbuflen);
        if (!inbuf)
            panic("Out of memory in lgetline_malloc");
        /* Return values from read:
           negative return = error
           zero return = EOF
           positive return = success, even if it didn't return as many
           bytes as expected (in which case we must rerun read) */
        inbuflen = read(fd, inbuf + fpos, inbuflen - fpos) + fpos;
        /* TODO: EINTR */
        if (inbuflen < fpos) { /* error */
            free(inbuf);
            return NULL;
        }

        /* We want to break out of the loop if we're at EOF (inbuflen ==
           fpos) or if a newline was found. */
        nlloc = memchr(inbuf, '\n', fpos);
        if (inbuflen == fpos)
            break;

        fpos = inbuflen;        
    } while (!nlloc);

    if (!nlloc) {
        free(inbuf);
        return NULL;
    }

    /* We could trim inbuf down to size, but there's not a lot of point,
       especially as in most cases it's going to be free'd almost
       immediately. We must, however, undo the excess read via seeking the
       cursor backwards to just after the final newline. */
    *(char *)nlloc = '\0';
    lseek(fd, ((char *)nlloc - inbuf) + 1 - fpos, SEEK_CUR);

    return inbuf;
}

/***** Locking *****/

/*
 * To be able to update the logfile, we need to ensure the following:
 *
 * * We have a write lock on the logfile;
 * * The logfile hasn't grown past where we expect it to be (i.e. there are
 *   no lines past the one that starts at the gamestate location)
 * * The recovery count is what we expect it to be.
 *
 * change_fd_lock is responsible for doing the necessary complexities to
 * grab the lock, but if something goes wrong with the other two rules, that
 * isn't an error: it just means that we're working from the wrong gamestate.
 * In some cases, the caller could recover via rereading the current line, but
 * a generic mechanism that works in all cases is simply to use terminate() to
 * replay the turn.
 *
 * Before calling this function, ensure that the current connection can play the
 * current game and the game has been synced to a target location of EOF
 * (i.e. !viewing).
 */
static void
start_updating_logfile(void)
{
    char *s;
    long o;

    if (program_state.viewing)
        panic("Logfile update while watching a game");

    if (!change_fd_lock(program_state.logfile, LT_WRITE, 2))
        panic("Could not upgrade to write lock on logfile");

    /* TODO: check recovery count */

    lseek(program_state.logfile, program_state.gamestate_location, SEEK_SET);
    s = lgetline_malloc(program_state.logfile);
    if (!s)
        panic("Log file has shrunk without increasing recovery count");
    free(s);
    /* now the file pointer should be at EOF */
    o = get_log_offset();

    if (o != lseek(program_state.logfile, 0, SEEK_END)) {
        if (!change_fd_lock(program_state.logfile, LT_READ, 1))
            panic("Could not downgrade to read lock on logfile");
        terminate(RESTART_PLAY);
    }
}

static void
stop_updating_logfile(void)
{
    if (!change_fd_lock(program_state.logfile, LT_READ, 1))
        panic("Could not downgrade to read lock on logfile");
}

/***** Creating specific log entries *****/

#define SECOND_LOGLINE_LEN      78
#define SECOND_LOGLINE_LEN_STR "78"
static_assert(SECOND_LOGLINE_LEN < COLNO, "SECOND_LOGLINE_LEN too long");

void
log_newgame(unsigned long long start_time, unsigned int seed, int playmode)
{
    char encbuf[ENCBUFSZ];
    const char *role;
    long start_of_third_line;

    /* There's no portable way to print an unsigned long long. The standards
       say %llu / %llx, but Windows doesn't follow them. It does, however,
       correctly obey the standards for unit_least64_t. */
    uint_least64_t start_time_l64 = (uint_least64_t) start_time;

    if (u.initgend == 1 && roles[u.initrole].name.f)
        role = roles[u.initrole].name.f;
    else
        role = roles[u.initrole].name.m;

    lprintf("NHGAME 00000001 %d.%03d.%03d\n",
            VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);
    lprintf("%" SECOND_LOGLINE_LEN_STR "s\n", "(new game)");
    start_of_third_line = get_log_offset();

    base64_encode(u.uplname, encbuf);
    lprintf("%0" PRIxLEAST64 " %x %d %s %.3s %.3s %.3s %.3s\n",
            start_time_l64, seed, playmode, encbuf, role,
            races[u.initrace].noun, genders[u.initgend].adj,
            aligns[u.initalign].adj);

    /* The gamestate location is meant to be set to the start of the last line
       of the log, when the log's in a state ready to be updated. Ensure that
       invariant now. */
    program_state.gamestate_location = start_of_third_line;
}

void
log_game_over(char *death)
{
    start_updating_logfile();
    lseek(program_state.logfile,
          strlen("NHGAME 00000001 4.000.000\n"), SEEK_SET);
    lprintf("%" SECOND_LOGLINE_LEN_STR "." SECOND_LOGLINE_LEN_STR "s", death);
    stop_updating_logfile();
}

void
log_command(int cmd, int rep, struct nh_cmd_arg *arg)
{
#ifdef TODO
    uint_least64_t turntime_l64 = (uint_least64_t) turntime;

    if (program_state.logfile == -1)
        return;

    /* command numbers are shifted by 1, so that they can be array indices
       during replay */
    lprintf("\n>%" PRIxLEAST64 ":%x:%d ", turntime_l64, cmd + 1, rep);
    switch (arg->argtype) {
    case CMD_ARG_NONE:
        lprintf("n");
        break;
    case CMD_ARG_DIR:
        lprintf("d:%d", arg->d);
        break;
    case CMD_ARG_POS:
        lprintf("p:%x:%x", arg->pos.x, arg->pos.y);
        break;
    case CMD_ARG_OBJ:
        lprintf("o:%c", arg->invlet);
        break;
    }
#endif
}

void
log_backup_save(void)
{
    if (program_state.logfile == -1)
        panic("log_backup_save called with no logfile");

    if (multi || occupation)
        panic("log_neutral_turnstate called but turnstate isn't neutral");

    start_updating_logfile();

    program_state.binary_save_location = 0;
    if (program_state.binary_save_allocated)
        mfree(&program_state.binary_save);
    mnew(&program_state.binary_save, NULL);
    program_state.binary_save_allocated = TRUE;
    savegame(&program_state.binary_save);

    long o = get_log_offset();
    boolean is_newgame = program_state.save_backup_location == 0;
    lprintf("*%08x ", program_state.save_backup_location);
    program_state.save_backup_location = o;
    program_state.binary_save_location = o;
    log_binary(program_state.binary_save.buf, program_state.binary_save.pos);
    lprintf("\n");

    /* Record the location of this save backup in the appropriate place. */
    lseek(program_state.logfile, is_newgame ? o + 1 :
          program_state.last_save_backup_location_location, SEEK_SET);
    lprintf("%08x", o);
    lseek(program_state.logfile, 0, SEEK_END);

    stop_updating_logfile();
}

void
log_neutral_turnstate(void)
{
    if (program_state.logfile == -1)
        panic("log_neutral_turnstate called with no logfile");

    if (multi || occupation)
        panic("log_neutral_turnstate called but turnstate isn't neutral");

#ifdef TODO
    lprintf("\n<%x", mt_nextstate() & 0xffff);

    if (!multi && !occupation) {
        struct memfile *this_cmd_state =
            (last_cmd_state ==
             recent_cmd_states ? recent_cmd_states + 1 : recent_cmd_states);

        mnew(this_cmd_state, last_cmd_state);
        savegame(this_cmd_state);       /* both records the state, and calcs a
                                           diff */
        lprintf("\n~");
        mdiffflush(this_cmd_state);
        log_binary(this_cmd_state->diffbuf, this_cmd_state->diffpos);
        mfree(last_cmd_state);
        last_cmd_state = this_cmd_state;
        action_count++;
    }

    last_cmd_pos = lseek(program_state.logfile, 0, SEEK_CUR);
    lseek(program_state.logfile, 0, SEEK_SET);
    lprintf("NHGAME %4s %08x %08x", statuscodes[LS_IN_PROGRESS], last_cmd_pos,
            action_count);
    lseek(program_state.logfile, last_cmd_pos, SEEK_SET);
#endif
}


/* remove the ongoing command fom the logfile. This is used to suppress the
   logging of commands marked as CMD_NOTIME */
void
log_revert_command(void)
{
#ifdef TODO
    if (program_state.logfile == -1)
        return;

    lseek(program_state.logfile, last_cmd_pos, SEEK_SET);
    ftruncate(program_state.logfile, last_cmd_pos);
#endif
}

#ifdef TODO
void
log_getpos(int ret, int x, int y)
{
    if (program_state.logfile == -1)
        return;
    lprintf(" p:%d:%x:%x", ret, x, y);
}


void
log_getdir(enum nh_direction dir)
{
    if (program_state.logfile == -1)
        return;
    lprintf(" d:%d", dir);
}


void
log_query_key(char key, int *count)
{
    if (program_state.logfile == -1)
        return;

    lprintf(" k:%hhx", key);
    if (count && *count != -1)
        lprintf(":%x", *count);
}


void
log_getlin(char *buf)
{
    char encodebuf[ENCBUFSZ];

    if (program_state.logfile == -1)
        return;
    base64_encode(buf, encodebuf);
    lprintf(" l:%s", encodebuf);
}


void
log_yn_function(char key)
{
    if (program_state.logfile == -1)
        return;
    lprintf(" y:%hhx", key);
}


void
log_menu(int n, int *results)
{
    int i;

    if (program_state.logfile == -1)
        return;

    if (n == -1) {
        lprintf(" m:x");
        return;
    }

    lprintf(" m:[");
    for (i = 0; i < n; i++)
        lprintf("%x:", results[i]);

    lprintf("]");
}


void
log_objmenu(int n, struct nh_objresult *pick_list)
{
    int i;

    if (program_state.logfile == -1)
        return;

    if (n == -1) {
        lprintf(" o:x");
        return;
    }

    lprintf(" o:[");
    for (i = 0; i < n; i++) {
        if (pick_list[i].count > -1)
            lprintf("%x,%x:", pick_list[i].id, pick_list[i].count);
        else
            lprintf("%x:", pick_list[i].id);
    }

    lprintf("]");
}
#endif

/* bones files must also be logged, since they are an input into the game state */
void
log_bones(const char *bonesbuf, int buflen)
{
#ifdef TODO
    log_binary(bonesbuf, buflen);
#endif
}

/***** Reading the log *****/

/* Code common to nh_get_savegame_status and log loading */
static enum nh_log_status
read_log_header(int fd, struct nh_game_info *si, boolean do_locking)
{
    char *logline, *p;
    char namebuf[BUFSZ];
    int playmode, version_major, version_minor, version_patchlevel;

    if (do_locking && !change_fd_lock(fd, LT_READ, 1))
        return LS_IN_PROGRESS;

    lseek(fd, 0, SEEK_SET);
    logline = lgetline_malloc(fd);
    if (!logline)
        goto invalid_log;

    if (sscanf(logline, "NHGAME %*8x %d.%3d.%3d",
               &version_major, &version_minor, &version_patchlevel) != 3)
        goto invalid_logline;

    free(logline);

    logline = lgetline_malloc(fd);
    if (!logline)
        goto invalid_log;

    if (strlen(logline) != SECOND_LOGLINE_LEN)
        goto invalid_logline;

    p = logline;
    while (*p == ' ')
        p++;
    strcpy(si->game_state, p);

    free(logline);

    logline = lgetline_malloc(fd);
    if (!logline)
        goto invalid_log;

    /* The numbers here are chosen to avoid overflow and underflow; the intended
       max lengths are (32 * 4 / 3) and 3, and the buffers that are eventually
       stored into are 32 and 16. Thus the temporary buffer in the case of
       namebuf. */
    if (sscanf(logline, "%*x %*x %d %64s %6s %6s %6s %6s",
               &playmode, namebuf, si->plrole, si->plrace,
               si->plgend, si->plalign) != 6)
        goto invalid_logline;

    free(logline);

    si->playmode = playmode;
    base64_decode(namebuf, si->name, sizeof (si->name));

    if (do_locking)
        change_fd_lock(fd, LT_NONE, 0);
    return LS_SAVED;

invalid_logline:
    free(logline);
invalid_log:
    if (do_locking)
        change_fd_lock(fd, LT_NONE, 0);
    return LS_INVALID;
}

enum nh_log_status
nh_get_savegame_status(int fd, struct nh_game_info *si)
{
    struct nh_game_info dummy;
    if (!si)
        si = &dummy;
    return read_log_header(fd, si, TRUE);
}

/* Sets the gamestate pointer and the actual gamestate from the binary save
   pointer and binary save. This function restores the gamestate-related
   program_state invariants. It also ensures that the binary save is correctly
   tagged and internally consistent.

   If maybe_old_version is false, then we insist that the save file in question
   loads correctly (i.e. loading and saving again produces an identical file);
   this helps catch errors where the gamestate is not saved correctly. If it's
   true, then we allow backwards-compatible changes to the save format; if
   loading and immediately re-saving does not produce an identical file, we
   force the next save-related line to be a diff not a backup. */
static void
load_gamestate_from_binary_save(boolean maybe_old_version)
{
    struct memfile mf;
    void *oldp, *newp;
    long len;

    /* Load the saved game. */
    program_state.gamestate_location = program_state.binary_save_location;
    dorecover(&program_state.binary_save);

    /* Save the loaded game. */
    mnew(&mf, NULL);
    savegame(&mf);

    /* Compare the old and new save files.

       TODO: This is a fatal error for the time being. Perhaps we should make it
       a warning instead, so that backwards-compatible changes to the save
       format */
    len = mf.pos;
    if (len != program_state.binary_save.pos) {
        if (maybe_old_version) {
            mfree(&mf);
            program_state.ok_to_diff = FALSE;
            return;
        }
        error_reading_save(
            "loading then immediately saving changes save file length\n");
    }
    oldp = mmmap(&program_state.binary_save, len, 0);
    newp = mmmap(&mf, len, 0);
    if (memcmp(oldp, newp, len) != 0) {
        if (maybe_old_version) {
            mfree(&mf);
            program_state.ok_to_diff = FALSE;
            return;
        }
        error_reading_save(
            "loading then immediately saving changes save file contents\n");
    }

    /* Replace the old save file with the new save file. */
    mfree(&program_state.binary_save);
    program_state.binary_save = mf;
    program_state.ok_to_diff = TRUE;
}

/* Decodes the given save diff into program_state.binary_save. The caller should
   check that the string actually is a representation of a save diff, is
   responsibe for fixing the invariants on program_state, and must move the
   binary save out of the way for safekeeping first.

   TODO: Perhaps this function would make more sense in memfile.c (with a
   slightly different calling convention), in case we need to apply diffs to
   anything else. It's here because this is the only file that needs to be able
   to apply diffs, for the time being. */
static void
apply_save_diff(char *s, struct memfile *diff_base)
{
    char *buf, *bufp, *mfp;
    long buflen, dbpos = 0;

    if (program_state.binary_save_allocated)
        panic("The caller of apply_save_diff must back up and deallocate "
              "the binary save");

    mnew(&program_state.binary_save, NULL);
    program_state.binary_save_allocated = TRUE;

    /* The header of a save diff is one byte, '~'. */
    s++;

    buflen = base64_strlen(s);
    buf = malloc(buflen + 2);
    memset(buf, 0, buflen + 2);
    base64_decode(s, buf, buflen);

    bufp = buf;
    while (bufp[0] || bufp[1]) {
        /* 0x0000 means "seek 0", which is never generated, and thus works as an
           EOF marker */

        signed short n = (unsigned char)(bufp[1]) & 0x3F;
        n *= 256;
        n += (unsigned char)(bufp[0]);

        bufp += 2;

        switch ((unsigned char)(bufp[1]) >> 6) {
        case MDIFF_SEEK:

            if (n >= 0x2000)
                n -= 0x4000;

            if (dbpos < n) {
                free(buf);
                mfree(&program_state.binary_save);
                error_reading_save("binary diff seeks past start of file");
            }

            dbpos -= n;

            break;

        case MDIFF_COPY:

            mfp = mmmap(&program_state.binary_save, n,
                        program_state.binary_save.pos);

            if (dbpos + n > diff_base->pos) {
                free(buf);
                mfree(&program_state.binary_save);
                error_reading_save("binary diff reads past EOF");
            }

            memcpy(mfp, diff_base->buf + dbpos, n);
            dbpos += n;

            break;

        case MDIFF_EDIT:
            
            mfp = mmmap(&program_state.binary_save, n,
                        program_state.binary_save.pos);

            if (bufp - buf + n > buflen) {
                free(buf);
                mfree(&program_state.binary_save);
                error_reading_save("binary diff ends unexpectedly");
            }

            memcpy(mfp, bufp, n);
            dbpos += n;     /* can legally go past the end of diff_base! */
            bufp += n;

            break;

        default:
            free(buf);
            mfree(&program_state.binary_save);
            error_reading_save("unknown command in binary diff");
        }
    }

    free(buf);
}

/* Decodes the given string into program_state.binary_save. The caller should
   check that the string actually is a representation of a save backup, and is
   responsible for fixing the invariants on program_state. */
static void
load_save_backup_from_string(char *s)
{
    void *mp;
    long len;

    if (program_state.binary_save_allocated)
        mfree(&program_state.binary_save);
    mnew(&program_state.binary_save, NULL);
    program_state.binary_save_allocated = TRUE;

    /* The header is '*', an 8 digit hex number, and ' ', = 10 bytes. */
    s += 10;
    len = base64_strlen(s);

    mp = mmmap(&program_state.binary_save, len, 0);
    base64_decode(s, mp, len);
}

/* Sets the binary save and save backup locations from the argument (which
   should be the byte offset of a save backup; the caller must check this), and
   sets the binary save to match. This does /not/ enforce the invariant that
   binary_save_location <= gamestate_location; the caller may need to fix
   that. The log file pointer is left immediately after the save backup. */
static void
load_save_backup_from_offset(long offset)
{
    char *logline;

    program_state.binary_save_location = offset;
    program_state.save_backup_location = offset;
    lseek(program_state.logfile, offset, SEEK_SET);

    logline = lgetline_malloc(program_state.logfile);
    if (!logline)
        error_reading_save("EOF when reading save backup\n");

    load_save_backup_from_string(logline);
    free(logline);
}

/* Checks to see if a save backup exists at a given file location. Returns -1 if
   no save backup was found there, 0 if a save backup was found but with a
   garbage next-offset field, or the next-offset field otherwise. */
static long
get_save_backup_offset(long offset)
{
    long oldoffset = get_log_offset();
    long rv = -1;
    char sbbuf[11];
    long sbloc;
    char *sbptr;

    if (lseek(program_state.logfile, offset, SEEK_SET) < 0)
        goto cleanup;

    /* Read the save backup header. */
    if (!full_read(program_state.logfile, sbbuf, 10))
        goto cleanup;
    sbbuf[10] = '\0';

    /* A save backup starts with an asterisk, then an 8-digit hexadecimal
       number, then a space. */
    if (sbbuf[0] != '*' || sbbuf[9] != ' ')
        goto cleanup;
    sbloc = strtol(sbbuf+1, &sbptr, 16);
    if (sbptr != sbbuf+9) /* sbbuf+1 didn't contain 8 hex digits */
        goto cleanup;

    /* It looks like a save backup. Is the offset valid? strtol returns LONG_MAX
       or LONG_MIN if out of range. */
    rv = 0;
    if (sbloc > 0 && sbloc < LONG_MAX)
        rv = sbloc;

cleanup:
    lseek(program_state.logfile, oldoffset, SEEK_SET);
    return rv;
}

/* Returns positive if the binary save is ahead of the target location, negative
   if the binary save is behind the target location, zero if they're the
   same. The argument is the binary save location; while the invariants hold,
   this is just program_state.binary_save_location, but because this function is
   mostly useful to log_sync(), which breaks the invariants at times, it allows
   a different value to be given.

   Leaves the binary save pointer in its original location, but the log file
   pointer in an unpredictable location. */
static long
relative_to_target(long bsl)
{
    long targetpos = program_state.target_location;
    long temp_pos = program_state.binary_save.pos;
    int turncount;
    switch (program_state.target_location_units) {

    case TLU_EOF:
        targetpos = lseek(program_state.logfile, 0, SEEK_END);
        /* fall through */
    case TLU_BYTES: 

        return bsl - targetpos;

    case TLU_TURNS:

        program_state.binary_save.pos = 0;
        if (!uptodate(&program_state.binary_save, NULL))
            error_reading_save(
                "binary save is from the wrong version of NetHack\n");

        turncount = mread32(&program_state.binary_save);
        program_state.binary_save.pos = temp_pos;

        return turncount - targetpos;

    default:
        panic("Invalid target_location_units");
    }
}

/*
 * Fastforwards/rewinds the gamestate to the target location.
 *
 * This must be called in the following situations:
 * * To load a save file for the first time (i.e. you call log_init then
 *   log_sync in order to load a save file);
 * * Whenever the target location moves to before the gamestate location
 *   (rewinding a replay, etc.).
 * * Whenever a section of the save file needs to be skipped due to desyncing.
 *
 * This also does something useful in the following situation:
 * * When the target location gets ahead of the gamestate location by more than
 *   a turn, calling log_sync() has the same effect as just running round the
 *   main loop in nh_play_game() would (assuming it syncs correctly), but is
 *   faster and less sensitive to engine changes.
 *
 * The gamestate location will not be moved to the target location itself if it
 * happens to be halfway through a turn; in this case, it will be moved to the
 * last save diff or save backup before the target location. (This happens even
 * if the previous gamestate location was between the target location and the
 * last save diff. In such a case, you may want to avoid using this function
 * altogether.)
 *
 * This should only be called from the nh_play_game main loop, or similar
 * locations that can handle arbitrary gamestate changes (including the
 * deallocation and reallocation of all game pointers). This also implies that
 * when this function is called, the gamestate location is equal to the binary
 * save location, or else the binary save location is zero (i.e. the game
 * restore sequence). (If you want to use this function in a situation where
 * that is not true, use terminate(RESTART_PLAY), which causes the main loop to
 * restart, and modify nh_play_game to set the target location you want rather
 * than its default of TLU_EOF.) It also requires us to hold a read lock on the
 * log (i.e. log_init() needs to have been called, and it can't be called from
 * the section of nh_input_aborted() that temporarily relinquishes the lock, not
 * that you'd want to do that anyway).
 *
 * The invariants on the gamestate are suspended during this function, and fixed
 * at the end. In particular, while the function is running, neither
 * program_state.gamestate_location, or any elements of the actual gamestate,
 * have any meaning at all.
 */
void
log_sync(void)
{
    struct nh_game_info si;
    struct memfile bsave;
    long sloc, loglineloc;
    char *logline;

    /* If the file is newly loaded, fill the locations with correct values
       rather than zeroes. TODO: Perhaps we should also do this when seeking to
       EOF, if the current save backup location is sufficiently far from the end
       of the file. */
    if (program_state.binary_save_location == 0) {
        /* Check it's a valid save file; simultaneously, move the file
           pointer to the start of line 4 (the first save backup). */
        if (read_log_header(program_state.logfile, &si, FALSE) != LS_SAVED)
            error_reading_save(
                "logfile has a bad header (is it from an old version?)\n");

        /* Now we know the location that the location of the last save backup
           should be stored in. This location is only advisory, though, and
           may contain an incorrect value. We also initialize the save backup
           location, while we're here. */
        program_state.save_backup_location = get_log_offset();
        program_state.last_save_backup_location_location =
            program_state.save_backup_location + 1;

        sloc = get_save_backup_offset(program_state.save_backup_location);
        if (sloc < 0) /* fourth line wasn't a save backup */
            error_reading_save(
                "fourth line of logfile is in the wrong format\n");

        /* Find a save backup. This should ideally be the last one in the file,
           but any save backup will do, and later is better (more efficient to
           load). We check to see if there actually is a save backup at the
           suggested location. If there is, we use it; it may be the one we
           want, and even if it isn't, it's going to be no earlier than the
           first save backup in the file, so it can't be a worse choice than the
           alternative. Otherwise, we use the first save backup in the file,
           because we already know where that is (and save_backup_location has
           that value already). */
        if (get_save_backup_offset(sloc) >= 0)
            program_state.save_backup_location = sloc;

        /* Set the binary save and gamestate locations, also the actual
           gamestate itself. Now all the log-related but gamestate-unrelated
           fields of program_state have sensible values (target_location is set
           by the caller, we set the save backup locations earlier, and this
           handles the binary save). This does not set the gamestate. */
        load_save_backup_from_offset(program_state.save_backup_location);

    } else if (program_state.gamestate_location !=
               program_state.binary_save_location)
        panic("log_sync called mid-turn");

    /* If we're ahead of the target, move back to the last save backup (because
       we can't run save diffs backwards, our only choice is to move forwards
       from the save backup location). */
    if (program_state.binary_save_location != program_state.save_backup_location
        && relative_to_target(program_state.binary_save_location) > 0) {

        load_save_backup_from_offset(program_state.save_backup_location);
    }

    /* While we're still ahead of the target, try progressively earlier
       backups. */
    while (relative_to_target(program_state.binary_save_location) > 0 &&
           program_state.save_backup_location >
           program_state.last_save_backup_location_location) {

        sloc = get_save_backup_offset(program_state.save_backup_location);
        load_save_backup_from_offset(sloc);
    }

    /* If we're behind the target, move forwards until we're at or ahead of the
       target, via adding together diffs.

       TODO: We could increase the loading speed via caching the end location of
       the save diff / backup, rather than skipping it each time round the
       loop. That isn't implemented yet in order to start off by getting the
       simplest possilbe version of the code working. */
    sloc = program_state.binary_save_location;
    while (relative_to_target(program_state.binary_save_location) < 0) {
        lseek(program_state.logfile, sloc, SEEK_SET);
        /* Skip the save diff or backup itself. */
        free(lgetline_malloc(program_state.logfile));

        /* Look for the next save diff or backup line. */
        for ((loglineloc = get_log_offset()),
                 (logline = lgetline_malloc(program_state.logfile));
             logline;
             free(logline), (loglineloc = get_log_offset()),
                 (logline = lgetline_malloc(program_state.logfile))) {
            if (*logline == '*' || *logline == '~')
                break;
        }

        if (!logline) {
            /* We're at EOF. The binary save and its location are already
               correct, so we just need to get the gamestate and its location
               correct. */
            load_gamestate_from_binary_save(TRUE);
            return;
        }

        /* Back up the binary save, so that we can go back if we overshoot.
           (This trick to avoid having to use mclone() is inspired by C++
           move constructors.) */
        bsave = program_state.binary_save;
        program_state.binary_save_allocated = FALSE;

        if (*logline == '*') {
            /* This is a save backup. */
            load_save_backup_from_string(logline);
        } else if (*logline == '~') {
            /* This is a save diff. */
            apply_save_diff(logline, &bsave);
        }

        if (relative_to_target(loglineloc) > 0) {

            /* We overshot. */
            sloc = program_state.binary_save_location;

            if (!program_state.binary_save_allocated) /* should never happen */
                panic("overshoot in log_sync but no binary save present");

            mfree(&program_state.binary_save);
            program_state.binary_save = bsave;

        } else {

            /* We didn't overshoot: set the locations to match this new save. */
            program_state.binary_save_location = sloc;
            if (*logline == '*')
                program_state.save_backup_location = sloc;

        }

        free(logline);
    }

    /* Fix the invariant on the gamestate. */
    load_gamestate_from_binary_save(TRUE);
}


static void
log_reset(void)
{
    if (program_state.binary_save_allocated)
        mfree(&program_state.binary_save);
    program_state.binary_save_allocated = FALSE;
    program_state.ok_to_diff = FALSE;

    program_state.save_backup_location = 0;
    program_state.binary_save_location = 0;
    program_state.gamestate_location = 0;
    program_state.target_location = 0;
    program_state.target_location_units = TLU_EOF;
    program_state.last_save_backup_location_location = 0;
}

void
log_init(int logfd)
{

    if (!change_fd_lock(logfd, LT_READ, 2))
        terminate(ERR_IN_PROGRESS);

    log_reset();

    program_state.logfile = logfd;
}

void
log_uninit(void)
{
    if (program_state.logfile > -1)
        change_fd_lock(program_state.logfile, LT_NONE, 0);

    program_state.logfile = -1;
}
