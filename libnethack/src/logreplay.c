/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "dlb.h"
#include "patchlevel.h"
#include <ctype.h>
#include <zlib.h>

#define DEBUG

extern int logfile;
extern unsigned int last_cmd_pos;
extern unsigned int action_count;

static void
replay_pause(enum nh_pause_reason r)
{
}

static void
replay_display_buffer(const char *buf, boolean trymove)
{
}

static void
replay_update_status(struct nh_player_info *pi)
{
}

static void
replay_print_message(int turn, const char *msg)
{
}

static void
replay_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO], int ux, int uy)
{
}

static void
replay_delay_output(void)
{
}

static void
replay_level_changed(int displaymode)
{
}

static void
replay_outrip(struct nh_menuitem *items, int icount, boolean tombstone,
              const char *name, int gold, const char *killbuf, int end_how,
              int year)
{
}

static int replay_display_menu(struct nh_menuitem *items, int icount,
                               const char *title, int how, int placement_hint,
                               int *results);
static int replay_display_objects(struct nh_objitem *items, int icount,
                                  const char *title, int how,
                                  int placement_hint,
                                  struct nh_objresult *pick_list);
static char replay_query_key(const char *query, int *count);
static int replay_getpos(int *x, int *y, boolean force, const char *goal);
static enum nh_direction replay_getdir(const char *query, boolean restricted);
static char replay_yn_function(const char *query, const char *rset,
                               char defchoice);
static void replay_getlin(const char *query, char *buf);


static struct loginfo {
    FILE *flog;
    unsigned long endpos;
    long nonjumped_filepointer;
    long last_token_start;
    unsigned int actioncount;
    boolean diffs_are_invalid;
    boolean cmds_are_invalid;
    boolean out_of_sync;
} loginfo;

static struct memfile diff_base;

struct replay_checkpoint {
    int actions, moves, nexttoken;
    struct nh_option_desc *opt; /* option state at the time of the checkpoint */
    struct memfile cpdata;      /* binary save data */
};

static struct replay_checkpoint *checkpoints;
static char **commands;
static int cmdcount, cpcount;
static struct nh_option_desc *saved_options;
static struct nh_window_procs replay_windowprocs, orig_windowprocs;

/* base 64 decoding table */
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


static const struct nh_window_procs def_replay_windowprocs = {
    replay_pause,
    replay_display_buffer,
    replay_update_status,
    replay_print_message,
    replay_display_menu,
    replay_display_objects,
    NULL,       /* no function required for list_items */
    replay_update_screen,
    NULL,       /* always use a given raw_print */
    replay_query_key,
    replay_getpos,
    replay_getdir,
    replay_yn_function,
    replay_getlin,
    replay_delay_output,
    replay_level_changed,
    replay_outrip,
    replay_print_message,
};

static int
base64_strlen(const char *in)
{
    /* If the input is uncompressed, just return its size. If it's compressed,
       read the size from the header. */
    if (*in != '$')
        return strlen(in);
    return atoi(in + 1);
}

static void
base64_decode(const char *in, char *out)
{
    int i, len = strlen(in), pos = 0;
    char *o = out;

    if (*in == '$')
        o = malloc(len);

    for (i = 0; i < len; i += 4) {
        /* skip data between $ signs, it's used for the header for compressed
           binary data */
        if (in[i] == '$')
            for (i += 2; in[i - 1] != '$' && in[i]; i++) {
            }
        /* decode blocks; padding '=' are converted to 0 in the decoding table */
        o[pos] = b64d[(int)in[i]] << 2 | b64d[(int)in[i + 1]] >> 4;
        o[pos + 1] = b64d[(int)in[i + 1]] << 4 | b64d[(int)in[i + 2]] >> 2;
        o[pos + 2] =
            ((b64d[(int)in[i + 2]] << 6) & 0xc0) | b64d[(int)in[i + 3]];
        pos += 3;
    }
    i -= 4;
    if ((in[i + 2] == '=' || !in[i + 2]) && (in[i + 3] == '=' || !in[i + 3]))
        pos--;
    if ((in[i + 1] == '=' || !in[i + 2]) && (in[i + 2] == '=' || !in[i + 3]))
        pos--;

    o[pos] = 0;

    if (*in == '$') {
        unsigned long blen = base64_strlen(in);
        int errcode = uncompress((unsigned char *)out, &blen,
                                 (unsigned char *)o, pos);

        free(o);
        if (errcode != Z_OK) {
            raw_printf("Decompressing save file failed at %ld: %s",
                       (long)ftell(loginfo.flog),
                       errcode == Z_MEM_ERROR ? "Out of memory" : errcode ==
                       Z_BUF_ERROR ? "Invalid size" : errcode ==
                       Z_DATA_ERROR ? "Corrupted file" : "(unknown error)");
            terminate();
        }
    }
}


void
replay_set_logfile(int logfd)
{
    if (!program_state.restoring && logfile)
        log_finish(LS_IN_PROGRESS);

    if (!lock_fd(logfd, 1)) {
        raw_printf
            ("The game log is locked by another NetHack process. Aborting.");
        terminate();
    }
    logfile = logfd;
}


void
replay_begin(void)
{
    long filesize;
    int i, dupped_fd;
    boolean recovery = FALSE;

    if (loginfo.flog)
        fclose(loginfo.flog);

    loginfo.diffs_are_invalid = FALSE;
    loginfo.cmds_are_invalid = FALSE;
    loginfo.out_of_sync = FALSE;
    lseek(logfile, 0, SEEK_SET);
    dupped_fd = dup(logfile);
    if (dupped_fd < 0)
        panic("Could not duplicate file descriptor");

    loginfo.flog = fdopen(dupped_fd, "rb");
    if (!loginfo.flog)
        panic("Could not open save file with stdio");

    fseek(loginfo.flog, 0, SEEK_END);
    filesize = ftell(loginfo.flog);
    fseek(loginfo.flog, 0, SEEK_SET);

    if (filesize < 24 ||
        !fscanf(loginfo.flog, "NHGAME %*s %lx %x", &loginfo.endpos,
                &loginfo.actioncount) || loginfo.endpos > filesize) {
        fclose(loginfo.flog);
        loginfo.flog = NULL;
        terminate();
    }

    action_count = loginfo.actioncount;

    if (!loginfo.endpos) {
        loginfo.endpos = filesize;
        recovery = TRUE;
    }

    if (recovery) {
        /* The last token should always be a command diff. So we look backwards 
           through the file for a line starting with ~. Because standard file
           reading functions only look /forwards/, we start 1024 bytes before
           the end of the file, and move 1024 bytes backwards if we don't find
           it, etc. */
        long fstart = loginfo.endpos;
        long found = -1;

        while (fstart > 0) {
            char endbuf[1024];
            int tread;

            fstart -= 1024;
            if (fstart < 0)
                fstart = 0;
            fseek(loginfo.flog, fstart, SEEK_SET);
            tread =
                fread(endbuf, 1,
                      (loginfo.endpos - fstart >
                       1026 ? 1026 : loginfo.endpos - fstart), loginfo.flog);
            for (i = 0; i < tread - 1; i++) {
                if ((endbuf[i] == '\r' || endbuf[i] == '\n') &&
                    endbuf[i + 1] == '~')
                    found = i + 1;
            }
            if (found > 0)
                found += fstart;
        }
        loginfo.endpos = found;
    }

    last_cmd_pos = loginfo.endpos;
    fseek(loginfo.flog, 0, SEEK_SET);

    mfree(&diff_base);
    mnew(&diff_base, NULL);

    /* the log contains options that need to be set while the log is being
       replayed. However, the current option state is the most recent state set 
       by the user and should be preserved */
    saved_options = clone_optlist(options);
}

void
replay_jump_to_endpos(void)
{
    loginfo.nonjumped_filepointer = lseek(logfile, 0, SEEK_CUR);
    lseek(logfile, loginfo.endpos, SEEK_SET);
}

void
replay_undo_jump_to_endpos(void)
{
    lseek(logfile, loginfo.nonjumped_filepointer, SEEK_SET);
}

void
replay_end(void)
{
    int i;
    long tz_off;

    if (!loginfo.flog)
        return;

    fclose(loginfo.flog);
    loginfo.flog = 0;

    tz_off = get_tz_offset();
    if (tz_off != replay_timezone)
        log_timezone(tz_off);

    /* restore saved options */
    for (i = 0; saved_options[i].name; i++)
        nh_set_option(saved_options[i].name, saved_options[i].value, FALSE);
    free_optlist(saved_options);

    mfree(&diff_base);

    if (!commands)
        return;

    for (i = 1; i < cmdcount; i++)
        free(commands[i]);
    free(commands);
    commands = NULL;
}


static void NORETURN
parse_error(const char *str)
{
#ifdef DEBUG
    raw_printf("Error at file position: %ld\n", ftell(loginfo.flog), str);
#else
    raw_printf("The command log seems to be in an outdated format. "
               "The game will be replayed from diffs instead.");
#endif
    loginfo.cmds_are_invalid = TRUE;
    terminate();
}

/* note: returns a buffer that is overwritten on every call */
static char *
next_log_token(void)
{
    static char *rbuf = NULL;
    static int rbuflen = 0;
    int rbpos = 0;
    long filepos = ftell(loginfo.flog);

    loginfo.last_token_start = filepos;
    while (1) {
        char c;

        if (rbpos >= rbuflen) {
            rbuflen *= 2;
            if (rbuflen < 256)
                rbuflen = 256;
            rbuf = realloc(rbuf, rbuflen);
        }
        if (rbpos + filepos >= loginfo.endpos) {
            rbuf[rbpos] = 0;
            return rbpos ? rbuf : NULL;
        }
        if (fread(&c, 1, 1, loginfo.flog) != 1) {
            raw_printf("Unexpected EOF or error in save file");
            terminate();
        }
        rbuf[rbpos] = c;
        if (rbuf[rbpos] == '\r' || rbuf[rbpos] == '\n' || rbuf[rbpos] == ' ') {
            if (rbpos != 0)
                break;
            /* otherwise overwrite the redundant whitespace the next time round 
               the loop */
        } else
            rbpos++;
    }
    rbuf[rbpos] = 0;
    return rbuf;
}


static int
replay_display_menu(struct nh_menuitem *items, int icount, const char *title,
                    int how, int placement_hint, int *results)
{
    int i, j, val;
    char *token;
    char *resultbuf;
    boolean id_ok;

    if (how == PICK_NONE)
        return 0;

    token = next_log_token();
    if (!token || token[0] != 'm')
        parse_error("Bad menu data");
    resultbuf = token + 2;

    if (*resultbuf++ == 'x')
        return -1;

    i = 0;
    while (sscanf(resultbuf, "%x:", &val)) {
        /* make sure all ids are valid - program changes could have broken the
           save */
        id_ok = FALSE;
        for (j = 0; j < icount && !id_ok; j++)
            if (items[j].id == val)
                id_ok = TRUE;
        if (!id_ok)
            parse_error("Invalid menu id in menu data");

        results[i++] = val;
        resultbuf = strchr(resultbuf, ':') + 1;
    }

    return i;
}


static int
replay_display_objects(struct nh_objitem *items, int icount, const char *title,
                       int how, int placement_hint,
                       struct nh_objresult *pick_list)
{
    int i, j, id, count;
    char *token;
    char *resultbuf;
    boolean id_ok;

    if (how == PICK_NONE)
        return 0;

    token = next_log_token();
    if (!token || token[0] != 'o')
        parse_error("Bad object menu data");
    resultbuf = token + 2;

    if (*resultbuf++ == 'x')
        return -1;

    i = 0;
    count = -1;
    while (sscanf(resultbuf, "%x,%x:", &id, &count)) {
        /* make sure all ids are valid - program changes could have broken the
           save */
        id_ok = FALSE;
        for (j = 0; j < icount && !id_ok; j++)
            if (items[j].id == id)
                id_ok = TRUE;
        if (!id_ok)
            parse_error("Invalid menu id in object menu data");

        pick_list[i].id = id;
        pick_list[i].count = count;
        i++;
        resultbuf = strchr(resultbuf, ':') + 1;
        count = -1;
    }

    return i;
}


static char
replay_query_key(const char *query, int *count)
{
    int key;
    char *token = next_log_token();
    int cnt = -1, n;

    n = sscanf(token, "k:%x:%x", &key, &cnt);
    if (n < 1)
        parse_error("Bad query_key data");

    if (count)
        *count = cnt;

    return key;
}


static int
replay_getpos(int *x, int *y, boolean force, const char *goal)
{
    int ret, n;
    char *token = next_log_token();

    n = sscanf(token, "p:%d:%x:%x", &ret, x, y);
    if (n != 3)
        parse_error("Bad getpos data");

    return ret;
}


static enum nh_direction
replay_getdir(const char *query, boolean restricted)
{
    int dir;
    char *token = next_log_token();

    int n = sscanf(token, "d:%d", &dir);

    if (n != 1)
        parse_error("Bad getdir data");

    return (enum nh_direction)dir;
}


static char
replay_yn_function(const char *query, const char *rset, char defchoice)
{
    int key;
    char *token = next_log_token();

    int n = sscanf(token, "y:%x", &key);

    if (n != 1)
        parse_error("Bad yn_function data");

    return key;
}


static void
replay_getlin(const char *query, char *buf)
{
    char *encdata, *token = next_log_token();

    encdata = strchr(token, ':');
    if (token[0] != 'l' || !encdata)
        parse_error("Bad getlin data");

    if (base64_strlen(encdata) > EQBUFSZ)
        parse_error
            ("Encoded getlin string is too long to decode into the target buffer.");

    base64_decode(encdata + 1, buf);
}


char *
replay_bones(int *buflen)
{
    char *b64data, *token = next_log_token();
    char *buf = NULL;

    if (!token) /* end of replay data reached */
        return NULL;

    if (strncmp(token, "b:", 2) != 0) {
        /* no bones to load */
        fseek(loginfo.flog, loginfo.last_token_start, SEEK_SET);
        return NULL;
    }

    b64data = token + 2;
    *buflen = base64_strlen(b64data);
    buf = malloc(*buflen);
    memset(buf, 0, *buflen);

    base64_decode(b64data, buf);

    return buf;
}


void
replay_setup_windowprocs(const struct nh_window_procs *procs)
{
    void (*win_raw_print) (const char *str);

    orig_windowprocs = windowprocs;

    if (!procs) {
        win_raw_print = windowprocs.win_raw_print;
        windowprocs = def_replay_windowprocs;
        windowprocs.win_raw_print = win_raw_print;
        return;
    }

    windowprocs = *procs;
    windowprocs.win_query_key = replay_query_key;
    windowprocs.win_yn_function = replay_yn_function;
    windowprocs.win_getlin = replay_getlin;
    windowprocs.win_getpos = replay_getpos;
    windowprocs.win_getdir = replay_getdir;
    windowprocs.win_display_menu = replay_display_menu;
    windowprocs.win_display_objects = replay_display_objects;
    windowprocs.win_pause = replay_pause;
    windowprocs.win_delay = replay_delay_output;

    replay_windowprocs = windowprocs;
}


void
replay_restore_windowprocs(void)
{
    if (orig_windowprocs.win_raw_print) /* test if orig_windowprocs is inited */
        windowprocs = orig_windowprocs;
}


static void
replay_read_commandlist(void)
{
    int i;
    char decbuf[BUFSZ], *token;

    cmdcount = strtol(next_log_token(), NULL, 16);
    if (!cmdcount)
        parse_error("expected number of commands");
    cmdcount++; /* the NULL command is not in the list */
    commands = malloc(cmdcount * sizeof (char *));
    commands[0] = NULL;

    for (i = 1; i < cmdcount; i++) {
        token = next_log_token();
        if (base64_strlen(token) > ENCBUFSZ)
            parse_error
                ("Encoded command name is too long for the decode buffer");
        base64_decode(token, decbuf);
        commands[i] = strdup(decbuf);
    }
}


void
replay_read_newgame(unsigned long long *init, int *playmode, char *namebuf,
                    int *initrole, int *initrace, int *initgend, int *initalign)
{
    char *header, *verstr;
    int ver1, ver2, ver3, n;
    unsigned int seed;

    header = next_log_token();
    if (!header || strcmp(header, "NHGAME"))
        parse_error("This file does not look like a NetHack logfile.");
    /* now header is not read any more */

    next_log_token();   /* marker */
    next_log_token();   /* end pos, see replay_begin() */
    next_log_token();   /* action count, see replay_begin(); */
    verstr = next_log_token();

    n = sscanf(verstr, "%d.%d.%d", &ver1, &ver2, &ver3);
    if (n != 3)
        parse_error("No version found where it was expected");
    /* now verstr is not read any more */

    if (ver1 != VERSION_MAJOR && ver2 != VERSION_MINOR)
        raw_printf("Warning: Version mismatch; expected %d.%d, got %d.%d\n",
                   VERSION_MAJOR, VERSION_MINOR, ver1, ver2);

    sscanf(next_log_token(), "%llx", init);
    sscanf(next_log_token(), "%x", &seed);
    *playmode = atoi(next_log_token());
    base64_decode(next_log_token(), namebuf);
    *initrole = str2role(next_log_token());
    *initrace = str2race(next_log_token());
    *initgend = str2gend(next_log_token());
    *initalign = str2align(next_log_token());

    if (*initrole == ROLE_NONE || *initrace == ROLE_NONE ||
        *initgend == ROLE_NONE || *initalign == ROLE_NONE)
        terminate();

    mt_srand(seed);

    replay_read_commandlist();
}


static void
replay_read_timezone(char *token)
{
    int n;

    n = sscanf(token, "TZ%d", &replay_timezone);
    if (n != 1)
        parse_error("Bad timezone offset data.");
}


static void
replay_read_option(char *token)
{
    char *name, *otype, *valstr, *arbuf, optname[BUFSZ], valbuf[BUFSZ];
    union nh_optvalue value;

    name = token + 1;
    otype = strchr(name, ':');
    if (!otype)
        terminate();
    *otype++ = '\0';
    valstr = strchr(otype, ':');
    if (!valstr)
        terminate();
    *valstr++ = '\0';

    base64_decode(name, optname);

    switch (otype[0]) {
    case 's':
        base64_decode(valstr, valbuf);
        value.s = (valbuf[0] != '\0') ? valbuf : NULL;
        break;
    case 'e':
        sscanf(valstr, "%x", &value.e);
        break;
    case 'i':
        sscanf(valstr, "%d", &value.i);
        break;
    case 'b':
        value.b = atoi(valstr);
        break;
    case 'a':
        arbuf = calloc(base64_strlen(valstr) + 1, 1);
        base64_decode(valstr, arbuf);
        value.ar = parse_autopickup_rules(arbuf);
        free(arbuf);
        break;

    default:
        parse_error("Unrecognized option type");
    }

    nh_set_option(optname, value, FALSE);
}


static boolean
replay_parse_arg(char *argstr, struct nh_cmd_arg *arg)
{
    int n, dir;

    if (!argstr || !arg)
        return FALSE;

    switch (argstr[0]) {
    case 'n':
        arg->argtype = CMD_ARG_NONE;
        return TRUE;

    case 'd':
        arg->argtype = CMD_ARG_DIR;
        n = sscanf(argstr, "d:%d", &dir);
        arg->d = (enum nh_direction)dir;
        return n == 1;

    case 'p':
        arg->argtype = CMD_ARG_POS;
        n = sscanf(argstr, "p:%hx:%hx", &arg->pos.x, &arg->pos.y);
        return n == 2;

    case 'o':
        arg->argtype = CMD_ARG_OBJ;
        n = sscanf(argstr, "o:%c", &arg->invlet);
        return n == 1;

    default:
        parse_error("unrecognized arg type");
        return FALSE;
    }
}


static void
replay_read_command(char *cmdtok, char **cmd, int *count,
                    struct nh_cmd_arg *arg)
{
    int cmdidx, n;

    if (!cmdtok)
        return;

    n = sscanf(cmdtok, ">%llx:%x:%d", &turntime, &cmdidx, count);
    if (n != 3 || cmdidx > cmdcount)
        parse_error("Error: Incorrect command spec\n");

    *cmd = commands[cmdidx];

    if (!replay_parse_arg(next_log_token(), arg))
        parse_error("Bad command argument");
}


static void
replay_check_cmdresult(char *token)
{
    int n;
    unsigned int rngstate;

    if (!token)
        return;

    if (loginfo.cmds_are_invalid)
        return;

    n = sscanf(token, "<%x", &rngstate);
    if (n != 1)
        parse_error("Error: incorrect command result specification\n");

    if (rngstate != (mt_nextstate() & 0xffff)) {
        loginfo.cmds_are_invalid = TRUE;
        raw_printf("The recorded commands in the recording seem to be invalid. "
                   "Replay will use diffs instead.");
    }
}

static void
replay_check_msg(char *token)
{
    char *b64data, *buf;
    int buflen;

    if (!token)
        return;

    if (*token != '-' || token[1] != '-')
        parse_error("Error: incorrect message format");

    b64data = token + 2;
    buflen = base64_strlen(b64data);

    buf = malloc(buflen + 2);
    memset(buf, 0, buflen + 2);
    base64_decode(b64data, buf);

    pline("%s", buf);
    free(buf);
}

void
replay_sync_save(void)
{
    if (!loginfo.out_of_sync)
        return;

    volatile struct sinfo ps;
    jmp_buf old_exit_jmp_buf;

    /* Use the version in the diff, not the one reached by playing through the
       game */

    ps = program_state;

    /* we want to catch exceptions during the load, which means storing the old 
       jmp_buf somewhere */
    memcpy(&old_exit_jmp_buf, &exit_jmp_buf, sizeof (jmp_buf));
    api_exit();

    if (!api_entry_checkpoint()) {
        raw_printf("The diffs in recording seem to be invalid. "
                   "Replay will use recorded commands instead.");
        loginfo.diffs_are_invalid = TRUE;
        freedynamicdata();
        program_state.restoring = TRUE;
        startup_common(0,
                       wizard ? MODE_WIZARD : discover ? MODE_EXPLORE :
                       MODE_NORMAL);
        dorecover(&diff_base);
    } else {
        freedynamicdata();
        program_state.restoring = TRUE;
        startup_common(0,
                       wizard ? MODE_WIZARD : discover ? MODE_EXPLORE :
                       MODE_NORMAL);
        dorecover(&diff_base);
    }
    iflags.disable_log = TRUE;
    program_state = ps;

    memcpy(&exit_jmp_buf, &old_exit_jmp_buf, sizeof (jmp_buf));
    exit_jmp_buf_valid = 1;
}

static long
true_moves(void)
{
    long pos = diff_base.pos;
    struct you sg_you;
    struct flag sg_flags;
    long rv;

    if (!loginfo.out_of_sync)
        return moves;
    diff_base.pos = 0;
    uptodate(&diff_base, NULL);
    restore_flags(&diff_base, &sg_flags);
    restore_you(&diff_base, &sg_you);
    rv = mread32(&diff_base);
    diff_base.pos = pos;
    return rv;
}

static void
replay_check_diff(char *token, boolean optonly, boolean fast)
{
    char *b64data, *buf, *bufp;
    int buflen, dbpos = 0;
    boolean do_realloc;
    struct memfile mf;

    if (!token)
        return;

    if (loginfo.diffs_are_invalid)
        return; /* this won't work, so no point in doing it */

    if (strncmp(token, "f:", 2))
        parse_error("Error: incorrect binary diff format.\n");

    b64data = token + 2;
    buflen = base64_strlen(b64data);

    buf = malloc(buflen + 2);
    memset(buf, 0, buflen + 2);
    base64_decode(b64data, buf);
    /* We create the save game as it should look, from the diff, in a new
       memfile mf. Then we save the game as it actually is in diff_base (we
       need to do this anyway to interpret future diffs), and compare. If
       they're different, we have a desync; either save or replay compatibility 
       broke, and we can choose which to follow, depending on whether we're
       trying to reconstruct the saves from the replay or vice versa. */
    mnew(&mf, NULL);
    bufp = buf;
    while (*bufp || bufp[1]) {
        /* 0x0000 means "seek 0" which would never be generated */
        signed short n = (unsigned char)(bufp[1]) & 0x3F;

        n *= 256;
        n += (unsigned char)(bufp[0]);
        switch ((unsigned char)(bufp[1]) >> 6) {
        case MDIFF_SEEK:
            if (n >= 0x2000)
                n -= 0x4000;
            if (dbpos < n) {
                free(buf);
                mfree(&mf);
                parse_error("diff seeks past start of file");
            }
            dbpos -= n;
            bufp += 2;
            break;
        case MDIFF_COPY:
        case MDIFF_EDIT:
            do_realloc = FALSE;
            while (mf.len < mf.pos + n) {
                mf.len += 4096;
                do_realloc = TRUE;
            }
            if (do_realloc)
                mf.buf = realloc(mf.buf, mf.len);
            if ((unsigned char)(bufp[1]) >> 6 == MDIFF_COPY) {
                if (dbpos + n > diff_base.pos) {
                    free(buf);
                    mfree(&mf);
                    parse_error("binary diff reads past EOF");
                }
                memcpy(mf.buf + mf.pos, diff_base.buf + dbpos, n);
                dbpos += n;
                mf.pos += n;
                bufp += 2;
            } else {
                bufp += 2;
                if (bufp - buf + n > buflen) {
                    free(buf);
                    mfree(&mf);
                    parse_error("binary diff ends unexpectedly");
                }
                memcpy(mf.buf + mf.pos, bufp, n);
                dbpos += n;     /* can legally go past the end of diff_base! */
                mf.pos += n;
                bufp += n;
            }
            break;
        default:
            free(buf);
            mfree(&mf);
            parse_error("unknown command in binary diff");
        }
    }
    if (optonly) {
        /* We aren't checking anything, but still need to record the diff so
           that we don't lose track of things. */
        mfree(&diff_base);
        diff_base = mf;
        /* then we let mf go out of scope */
    } else {
        if (!fast) {
            mfree(&diff_base);
            mnew(&diff_base, NULL);
        }
        if (!loginfo.cmds_are_invalid && !fast)
            savegame(&diff_base);
        if (fast || diff_base.pos != mf.pos ||
            memcmp(diff_base.buf, mf.buf, mf.pos)) {
#ifdef DEBUG    /* desync location debugging */
            if (mf.pos == diff_base.pos && !loginfo.cmds_are_invalid && !fast) {
                int i;
                struct memfile_tag origtag;

                origtag.next = 0;
                origtag.tagdata = 99;
                origtag.tagtype = MTAG_START;
                origtag.pos = 0;
                struct memfile_tag *best_tag = &origtag;

                for (dbpos = 0; dbpos < diff_base.pos; dbpos++) {
                    if (mf.buf[dbpos] != diff_base.buf[dbpos]) {
                        for (i = 0; i < MEMFILE_HASHTABLE_SIZE; i++) {
                            struct memfile_tag *tp;

                            for (tp = diff_base.tags[i]; tp; tp = tp->next) {
                                if (tp->pos <= dbpos &&
                                    tp->pos >= best_tag->pos)
                                    best_tag = tp;
                            }
                        }
                        raw_printf("desync between recording and save at tag "
                                   "(%d, %ld) + %d bytes",
                                   (int)best_tag->tagtype, best_tag->tagdata,
                                   dbpos - best_tag->pos);
                        break;  /* comment this out to see all desyncs */
                    }
                }
            } else if (!loginfo.cmds_are_invalid && !fast) {
                raw_printf("desync between recording (length %d) and "
                           "recorded save (length %d)", diff_base.pos, mf.pos);
            }
#endif
            loginfo.out_of_sync = TRUE;
            mfree(&diff_base);
            diff_base = mf;
            if (!fast) {
                replay_sync_save();
                if (!loginfo.diffs_are_invalid) {
                    /* it loaded fine, but is different from the recorded
                       version. Most likely cause: the recorded version is out
                       of date. */
                    if (!loginfo.cmds_are_invalid) {
                        loginfo.cmds_are_invalid = TRUE;
                        raw_printf
                            ("The recorded commands in the recording seem "
                             "to be invalid. Replay will use diffs instead.");
                    }
                }
            }
        } else
            mfree(&mf);
    }
    free(buf);
    /* otherwise everything is fine, and we've saved in diff_base already */
}

static char *
strdupnull(char *s)
{
    if (s)
        return strdup(s);
    return NULL;
}

/* optonly: look only for options
   singlestep: run one line at a time
   fast: it's OK to not update save data
         (replay_sync_save() must be called afterwards if this is true)
   Note: in the special case of TRUE, FALSE, the log may not be used
   for anything other than options (i.e. replay_end must be called as
   the next replay-related command, other than replay_restore_windowprocs). */
boolean
replay_run_cmdloop(boolean optonly, boolean singlestep, boolean fast)
{
    char *cmd, *token;
    int count, cmdidx;
    struct nh_cmd_arg cmdarg;
    struct nh_option_desc *tmp;
    boolean did_action = FALSE;

    /* the log contains the birth options that are required for this game, so
       nh_set_option calls during the replay must change active_birth_options */
    tmp = birth_options;
    birth_options = active_birth_options;
    active_birth_options = tmp;

    token = strdupnull(next_log_token());

    while (token) {
        switch (token[0]) {
        case '!':      /* Option */
            replay_read_option(token);
            break;

        case 'T':      /* timezone offset */
            replay_read_timezone(token);
            break;

        case '>':      /* command */
            if (!optonly && !loginfo.cmds_are_invalid) {
                replay_read_command(token, &cmd, &count, &cmdarg);
                cmdidx = get_command_idx(cmd);
                command_input(cmdidx, count, &cmdarg);
            }
            if (!optonly)
                did_action = TRUE;
            break;

        case '<':      /* a command result */
            if (!optonly)
                replay_check_cmdresult(token);
            break;

        case '~':      /* a diff */
            if (!optonly || singlestep)
                replay_check_diff(next_log_token(), optonly, fast);

            if (singlestep) {
                goto out;
            }
            break;

        case '-':
            /* We want to display the welcome messages in the new-game sequence 
               even if recovering from diffs. */
            if (program_state.viewing && loginfo.cmds_are_invalid && !optonly) {
                replay_check_msg(token);
            }
            break;
        }
        free(token);

        token = strdupnull(next_log_token());
    }

out:
    /* return the birth options to normal */
    tmp = birth_options;
    birth_options = active_birth_options;
    active_birth_options = tmp;

    return did_action;
}


static void
make_checkpoint(int actions)
{
    /* only make a checkpoint if enough actions have happened since the last
       one and creating a checkpoint is safe */
    if ((cpcount > 0 && (actions <= checkpoints[cpcount - 1].actions + 1000 ||
                         true_moves() <= checkpoints[cpcount - 1].moves)) ||
        multi || occupation) /* checkpointing while something is in
                                progress doesn't work */
        return;

    replay_sync_save();

    cpcount++;
    checkpoints =
        realloc(checkpoints, sizeof (struct replay_checkpoint) * cpcount);
    checkpoints[cpcount - 1].actions = actions;
    checkpoints[cpcount - 1].moves = moves;
    checkpoints[cpcount - 1].nexttoken = ftell(loginfo.flog);
    /* the active option list must be saved: it is not part of the normal
       binary save */
    checkpoints[cpcount - 1].opt = clone_optlist(options);
    mnew(&checkpoints[cpcount - 1].cpdata, NULL);
    savegame(&checkpoints[cpcount - 1].cpdata);
    checkpoints[cpcount - 1].cpdata.len = checkpoints[cpcount - 1].cpdata.pos;
    checkpoints[cpcount - 1].cpdata.pos = 0;
}


static int
load_checkpoint(int idx)
{
    int playmode, i, irole, irace, igend, ialign;
    boolean cmd_invalid, diff_invalid;
    char namebuf[BUFSZ];

    if (idx < 0 || idx >= cpcount)
        return -1;

    cmd_invalid = loginfo.cmds_are_invalid;
    diff_invalid = loginfo.diffs_are_invalid;
    loginfo.out_of_sync = FALSE;        /* we're destroying saved state anyway */

    replay_end();
    freedynamicdata();

    replay_begin();
    replay_read_newgame(&turntime, &playmode, namebuf, &irole, &irace, &igend,
                        &ialign);
    fseek(loginfo.flog, checkpoints[idx].nexttoken, SEEK_SET);

    loginfo.cmds_are_invalid = cmd_invalid;
    loginfo.diffs_are_invalid = diff_invalid;

    program_state.restoring = TRUE;
    startup_common(namebuf, playmode);
    dorecover(&checkpoints[idx].cpdata);
    checkpoints[idx].cpdata.pos = 0;

    mfree(&diff_base);
    mnew(&diff_base, NULL);

    iflags.disable_log = TRUE;
    program_state.viewing = TRUE;
    program_state.game_running = TRUE;

    /* restore the full option state of the time of the checkpoint */
    for (i = 0; checkpoints[idx].opt[i].name; i++)
        nh_set_option(checkpoints[idx].opt[i].name,
                      checkpoints[idx].opt[i].value, FALSE);

    savegame(&diff_base);

    return checkpoints[idx].actions;
}


static void
free_checkpoints(void)
{
    int i;

    for (i = 0; i < cpcount; i++) {
        free_optlist(checkpoints[i].opt);
        mfree(&(checkpoints[i].cpdata));
    }
    free(checkpoints);
    checkpoints = NULL;
    cpcount = 0;
}


static boolean
find_next_command(char *buf, int buflen)
{
    buf[0] = '\0';
    return FALSE;
#if 0
    for (i = loginfo.next; i < loginfo.tokencount; i++)
        if (*loginfo.tokens[i] == '>')
            break;

    if (i == loginfo.tokencount)
        return FALSE;

    n = sscanf(loginfo.tokens[i], ">%lx:%x:%d", &dummy, &cmdidx, &count);
    if (n != 3)
        return FALSE;

    cmdname = commands[cmdidx] ? commands[cmdidx] : "<continue>";
    strncpy(buf, cmdname, buflen);
    return TRUE;
#endif
}


boolean
nh_view_replay_start(int fd, struct nh_window_procs * rwinprocs,
                     struct nh_replay_info * info)
{
    int playmode;
    char namebuf[PL_NSIZ];
    struct nh_game_info gi;

    if (!api_entry_checkpoint())
        return FALSE;

    memset(info, 0, sizeof (struct nh_replay_info));
    if (logfile != -1 || nh_get_savegame_status(fd, &gi) == LS_INVALID) {
        api_exit();
        return FALSE;
    }

    program_state.restoring = TRUE;
    iflags.disable_log = TRUE;
    logfile = fd;
    replay_begin();
    replay_read_newgame(&turntime, &playmode, namebuf, &u.initrole, &u.initrace,
                        &u.initgend, &u.initalign);
    replay_setup_windowprocs(rwinprocs);

    initoptions();
    replay_run_cmdloop(TRUE, TRUE, FALSE);      /* (re)set options */
    nh_start_game(fd, namebuf, u.initrole, u.initrace, u.initgend, u.initalign,
                  playmode);
    program_state.restoring = FALSE;
    program_state.viewing = TRUE;
    replay_restore_windowprocs();

    /* the win_update_screen proc in the replay_windowprocs does nothing, so
       flush (again) after switching back to regular window procs */
    flush_screen();

    info->max_moves = gi.moves;
    info->max_actions = loginfo.actioncount - 1; /* - 1 for the new-game ~ */
    find_next_command(info->nextcmd, sizeof (info->nextcmd));
    update_inventory();
    make_checkpoint(0);

    api_exit();

    return TRUE;
}


boolean
nh_view_replay_step(struct nh_replay_info * info, enum replay_control action,
                    volatile int count)
{
    boolean did_action = FALSE;
    int i, prev_actions, target;
    volatile int moves_this_step = true_moves();

    if (!program_state.viewing) {
        info->actions++;
        did_action = TRUE;
        goto out2;
    }

    if (!api_entry_checkpoint()) {
        /* Something went wrong replaying the turn; however, we have two
           different method of replaying, so the other one might work. Replay
           back up to the current turn. */
        if (moves_this_step == -1) {
            raw_printf("Could not restore state after replay failed!");
            did_action = TRUE;
            goto out2;
        }
        count = moves_this_step;
        action = REPLAY_GOTO;
        moves = 0;
        moves_this_step = -1;   /* avoid recursion */
        replay_restore_windowprocs();
    }

    program_state.restoring = TRUE;
    replay_setup_windowprocs(&replay_windowprocs);
    info->moves = true_moves();
    switch (action) {
    case REPLAY_BACKWARD:
        prev_actions = info->actions;
        target = prev_actions - count;
        for (i = 0; i < cpcount - 1; i++)
            if (checkpoints[i + 1].actions >= target)
                break;

        /* rewind the entire game state to the checkpoint */
        info->actions = load_checkpoint(i);
        count = target - info->actions;
        if (count == 0) {
            did_action = TRUE;
            goto out;
        }
        /* else fall through */

    case REPLAY_FORWARD:
        did_action = TRUE;
        i = 0;
        while (i < count && did_action) {
            i++;
            did_action = replay_run_cmdloop(FALSE, TRUE, i != count);
            if (did_action) {
                info->actions++;
                make_checkpoint(info->actions);
            }
        }
        break;

    case REPLAY_GOTO:
        target = count;
        if (target < true_moves()) {
            for (i = 0; i < cpcount - 1; i++)
                if (checkpoints[i + 1].moves >= target)
                    break;
            /* rewind the entire game state to the checkpoint */
            info->actions = load_checkpoint(i);
        }

        did_action = info->actions < info->max_actions;
        while (true_moves() < count && did_action) {
            did_action = replay_run_cmdloop(FALSE, TRUE, TRUE);
            if (did_action) {
                info->actions++;
                make_checkpoint(info->actions);
            }
        }
        replay_sync_save();
        did_action = (moves_this_step == -1 ? TRUE : true_moves() == count);
        break;
    }

out:
    api_exit();
out2:
    program_state.restoring = FALSE;
    info->moves = true_moves();
    find_next_command(info->nextcmd, sizeof (info->nextcmd));
    replay_restore_windowprocs();
    if (loginfo.cmds_are_invalid)
        doredraw();
    flush_screen();     /* must happen after replay_restore_windowprocs to
                           ensure output */
    update_inventory();

    /* if we're going backwards, the timestamp on this message will let the ui
       know it should erase messages in the future */
    print_message(true_moves(), "");

    return did_action;
}


void
nh_view_replay_finish(void)
{
    replay_restore_windowprocs();
    replay_end();
    program_state.viewing = FALSE;
    program_state.game_running = FALSE;
    freedynamicdata();
    free_checkpoints();
    logfile = -1;
    iflags.disable_log = FALSE;
}


enum nh_log_status
nh_get_savegame_status(int fd, struct nh_game_info *gi)
{
    char header[128], status[8], encplname[PL_NSIZ * 2];
    char role[PLRBUFSZ], race[PLRBUFSZ], gend[PLRBUFSZ], algn[PLRBUFSZ];
    int n, v1, v2, v3;
    unsigned int savepos, endpos, seed, playmode;
    struct memfile mf;
    volatile enum nh_log_status ret;
    boolean game_inited = (wiz1_level.dlevel != 0);
    struct you sg_you;
    struct flag sg_flags;
    long sg_moves;
    long long starttime;

    lseek(fd, 0, SEEK_SET);
    if (read(fd, header, 127) <= 0)
        return LS_INVALID;
    header[127] = '\0';
    n = sscanf(header, "NHGAME %4s %x %*8s %d.%d.%d\n%llx %x %x %s %s %s %s %s",
               status, &savepos, &v1, &v2, &v3, &starttime, &seed, &playmode,
               encplname, role, race, gend, algn);
    if (n != 13)
        return LS_INVALID;

    endpos = lseek(fd, 0, SEEK_END);
    if (!strcmp(status, "done"))
        ret = LS_DONE;
    else if (!strcmp(status, "inpr"))
        ret = LS_CRASHED;
    else if (!strcmp(status, "save"))
        ret = LS_SAVED;
    else
        return LS_INVALID;

    if (ret == LS_SAVED && endpos == savepos)
        ret = LS_CRASHED;

    /* if we can't lock the file, it's in use */
    if (!lock_fd(fd, 0))
        ret = LS_IN_PROGRESS;
    unlock_fd(fd);      /* don't need the lock, we're not going to write */

    if (!gi)
        return ret;

    memset(gi, 0, sizeof (struct nh_game_info));
    gi->playmode = playmode;
    base64_decode(encplname, gi->name);
    role[0] = lowc(role[0]);
    strcpy(gi->plrole, role);
    strcpy(gi->plrace, race);
    strcpy(gi->plgend, gend);
    strcpy(gi->plalign, algn);

    if (ret == LS_CRASHED || ret == LS_IN_PROGRESS)
        return ret;

    if (!api_entry_checkpoint())
        /* something went wrong, hopefully it isn't so bad that replay won't
           work */
        return LS_CRASHED;

    lseek(fd, savepos, SEEK_SET);
    if (ret == LS_SAVED) {
        mf.pos = 0;
        mf.buf = loadfile(fd, &mf.len);
        if (!mf.buf)
            return 0;

        if (!uptodate(&mf, NULL)) {
            free(mf.buf);
            api_exit();
            return LS_CRASHED;  /* probably still a valid game */
        }

        restore_flags(&mf, &sg_flags);
        restore_you(&mf, &sg_you);
        sg_moves = mread32(&mf);
        free(mf.buf);

        /* make sure topten_level_name can work correctly */
        if (!game_inited) {
            dlb_init();
            init_dungeons();
        }

        gi->depth = depth(&sg_you.uz);
        gi->moves = sg_moves;
        gi->level_desc[0] = '\0';
        gi->has_amulet = sg_you.uhave.amulet;
        topten_level_name(sg_you.uz.dnum, depth(&sg_you.uz), gi->level_desc);

        if (!game_inited) {
            free_dungeon();
            dlb_cleanup();
        }
    } else if (ret == LS_DONE) {
        struct nh_topten_entry tt;

        read_log_toptenentry(fd, &tt);
        gi->moves = tt.moves;
        gi->depth = tt.maxlvl;
        strncpy(gi->death, tt.death, BUFSZ);
    }

    api_exit();

    return ret;
}
