/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2013-12-22 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "patchlevel.h"
#include <zlib.h>
/* stdint.h, inttypes.h let us printf long longs portably */
#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>

/* #define DEBUG */

extern const struct cmd_desc cmdlist[];

int logfile = -1;
unsigned int last_cmd_pos;
unsigned int action_count;
static struct memfile recent_cmd_states[2];
static struct memfile *last_cmd_state = recent_cmd_states;
static const char *const statuscodes[] = { "save", "done", "inpr" };

static int last_curline;

static const unsigned char b64e[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void log_binary(const char *buf, int buflen, char prefix[3]);

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
lprintf(const char *fmt, ...)
{
    va_list vargs;
    char outbuf[4096];
    int size;

    va_start(vargs, fmt);
    size = vsnprintf(outbuf, sizeof (outbuf), fmt, vargs);
    va_end(vargs);

    if (write(logfile, outbuf, size) != size)
        panic("writing %d bytes to the log failed.");

    return size;
}


void
log_option(struct nh_option_desc *opt)
{
    char encbuf[ENCBUFSZ];
    char *str, *encbuf2;

    if (iflags.disable_log || logfile == -1)
        return;

    base64_encode(opt->name, encbuf);
    lprintf("\n!%s:", encbuf);

    switch (opt->type) {
    case OPTTYPE_STRING:
        str = opt->value.s ? opt->value.s : "";
        base64_encode(str, encbuf);
        lprintf("s:%s", encbuf);
        break;

    case OPTTYPE_ENUM:
        lprintf("e:%x", opt->value.e);
        break;

    case OPTTYPE_INT:
        lprintf("i:%d", opt->value.i);
        break;

    case OPTTYPE_BOOL:
        lprintf("b:%x", ! !opt->value.b);
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        str = autopickup_to_string(opt->value.ar);
        lprintf("a:");
        encbuf2 = malloc(base64size(strlen(str)));
        base64_encode(str, encbuf2);
        /* write directly, large numbers of rules might overflow outbuf in
           lprintf */
        write(logfile, encbuf2, strlen(encbuf2));
        free(encbuf2);
        free(str);
        break;
    }

    last_cmd_pos = lseek(logfile, 0, SEEK_CUR);
}


static void
log_game_opts(void)
{
    int i;

    for (i = 0; options[i].name; i++) {}
    lprintf("opts:%x", i);

    for (i = 0; options[i].name; i++)
        log_option(&options[i]);
}


static void
log_command_list(void)
{
    int i;
    char encbuf[ENCBUFSZ];

    for (i = 0; cmdlist[i].name; i++) ;
    lprintf("%x", i);
    for (i = 0; cmdlist[i].name; i++) {
        base64_encode(cmdlist[i].name, encbuf);
        lprintf(" %s", encbuf);
    }
    lprintf("\n");
}


long
get_tz_offset(void)
{
#if !defined(__FreeBSD__)
    tzset();    /* sets the extern "timezone" which has the offset from UTC in
                   seconds */
    return timezone;
#else
    time_t t = time(NULL);

    return -localtime(&t)->tm_gmtoff;
#endif
}


void
log_newgame(int logfd, unsigned long long start_time, unsigned int seed)
{
    char encbuf[ENCBUFSZ];
    const char *role;
    uint_least64_t start_time_l64 = (uint_least64_t) start_time;

    if (program_state.restoring || iflags.disable_log)
        return;

    if (!lock_fd(logfd, 1))
        panic("The game log is locked. Aborting.");

    logfile = logfd;
    /* FIXME: needs file locking */

    if (u.initgend == 1 && roles[u.initrole].name.f)
        role = roles[u.initrole].name.f;
    else
        role = roles[u.initrole].name.m;

    lprintf("NHGAME inpr %08x 00000000 %d.%d.%d\n", 0, VERSION_MAJOR,
            VERSION_MINOR, PATCHLEVEL);

    base64_encode(plname, encbuf);
    lprintf("%" PRIxLEAST64 " %x %d %s %s %s %s %s\n", start_time_l64, seed,
            wizard ? MODE_WIZARD : discover ? MODE_EXPLORE : MODE_NORMAL,
            encbuf, role, races[u.initrace].noun, genders[u.initgend].adj,
            aligns[u.initalign].adj);
    log_command_list();
    log_game_opts();
    /* all the timestamps are UTC, so timezone info is required to interpret
       them */
    log_timezone(get_tz_offset());
}



void
log_timezone(int tz_offset)
{
    if (iflags.disable_log || logfile == -1)
        return;

    lprintf("\nTZ%d", tz_offset);
}


void
log_command(int cmd, int rep, struct nh_cmd_arg *arg)
{
    uint_least64_t turntime_l64 = (uint_least64_t) turntime;

    if (iflags.disable_log || logfile == -1)
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
}


void
log_command_result(void)
{
    if (iflags.disable_log || !program_state.something_worth_saving ||
        logfile == -1)
        return;

    if (!multi && !occupation) {
        /* We want to log all the messages produced since the last command,
           especially nonblocking ones, so we can let the user know what
           happened if we're replaying from diffs. */
        while (last_curline != curline) {
            log_binary(toplines[last_curline], strlen(toplines[last_curline]),
                       "\n--");
            last_curline++;
            last_curline %= MSGCOUNT;
        }
    }

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
        log_binary(this_cmd_state->diffbuf, this_cmd_state->diffpos, " f:");
#ifdef DEBUG
        /* some debug code for checking diff efficiency */
        int edits = 0, editbytes = 0, copies = 0, copybytes = 0, seeks = 0, i;

        for (i = 0; i < this_cmd_state->diffpos; i++) {
            uint16_t j = *(uint16_t *) (this_cmd_state->diffbuf + i);

            i++;
            switch (j >> 14) {
            case MDIFF_SEEK:
                seeks++;
                break;
            case MDIFF_COPY:
                copies++;
                copybytes += (j & 0x3fff);
                break;
            case MDIFF_EDIT:
                edits++;
                editbytes += (j & 0x3fff);
                i += (j & 0x3fff);
                break;
            }
        }
        lprintf(" (%d edits (%d bytes), %d copies (%d bytes), %d seeks)", edits,
                editbytes, copies, copybytes, seeks);
#endif
        mfree(last_cmd_state);
        last_cmd_state = this_cmd_state;
        action_count++;
    }

    last_cmd_pos = lseek(logfile, 0, SEEK_CUR);
    lseek(logfile, 0, SEEK_SET);
    lprintf("NHGAME %4s %08x %08x", statuscodes[LS_IN_PROGRESS], last_cmd_pos,
            action_count);
    lseek(logfile, last_cmd_pos, SEEK_SET);
}


/* remove the ongoing command fom the logfile. This is used to suppress the
 * logging of commands marked as CMD_NOTIME */
void
log_revert_command(void)
{
    if (logfile == -1 || iflags.disable_log)
        return;

    lseek(logfile, last_cmd_pos, SEEK_SET);
    ftruncate(logfile, last_cmd_pos);
}


void
log_getpos(int ret, int x, int y)
{
    if (logfile == -1 || iflags.disable_log)
        return;
    lprintf(" p:%d:%x:%x", ret, x, y);
}


void
log_getdir(enum nh_direction dir)
{
    if (logfile == -1 || iflags.disable_log)
        return;
    lprintf(" d:%d", dir);
}


void
log_query_key(char key, int *count)
{
    if (logfile == -1 || iflags.disable_log)
        return;

    lprintf(" k:%hhx", key);
    if (count && *count != -1)
        lprintf(":%x", *count);
}


void
log_getlin(char *buf)
{
    char encodebuf[ENCBUFSZ];

    if (logfile == -1 || iflags.disable_log)
        return;
    base64_encode(buf, encodebuf);
    lprintf(" l:%s", encodebuf);
}


void
log_yn_function(char key)
{
    if (logfile == -1 || iflags.disable_log)
        return;
    lprintf(" y:%hhx", key);
}


void
log_menu(int n, int *results)
{
    int i;

    if (logfile == -1 || iflags.disable_log)
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

    if (logfile == -1 || iflags.disable_log)
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

static void
log_binary(const char *buf, int buflen, char prefix[3])
{
    char *b64buf;

    if (logfile == -1 || iflags.disable_log)
        return;

    b64buf = malloc(base64size(buflen));
    base64_encode_binary((const unsigned char *)buf, b64buf, buflen);

    /* don't use lprintf, b64buf might be too big for the buffer used by
       lprintf */
    write(logfile, prefix, 3);
    write(logfile, b64buf, strlen(b64buf));

    free(b64buf);
}

/* bones files must also be logged, since they are an input into the game state */
void
log_bones(const char *bonesbuf, int buflen)
{
    log_binary(bonesbuf, buflen, " b:");
}


void
log_finish(enum nh_log_status status)
{
    if (!program_state.something_worth_saving || logfile == -1 ||
        iflags.disable_log)
        return;

    lseek(logfile, last_cmd_pos++, SEEK_SET);
    lprintf("\n");
    lseek(logfile, 0, SEEK_SET);
    lprintf("NHGAME %4s %08x", statuscodes[status], last_cmd_pos);
    lseek(logfile, last_cmd_pos, SEEK_SET);

    if (status != LS_IN_PROGRESS)
        unlock_fd(logfile);
    logfile = -1;
}

void
log_truncate(void)
{
    if (ftruncate(logfile, last_cmd_pos) < 0)
        panic("Cannot truncate logfile");
    /* the replay code might leave the file pointer anywhere (it uses stdio),
       so move it to the right place manually */
    lseek(logfile, last_cmd_pos, SEEK_SET);
}

void
log_init(void)
{
    mfree(recent_cmd_states);
    mfree(recent_cmd_states + 1);
    mnew(recent_cmd_states, NULL);
    mnew(recent_cmd_states + 1, NULL);
    last_curline = curline;
}
