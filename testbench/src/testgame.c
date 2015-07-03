/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-05-31 */
/* Copyright (c) 2015 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#ifdef AIMAKE_BUILDOS_MSWin32
# error !AIMAKE_FAIL_SILENTLY! Testing on Windows is not yet supported.
#endif

#include "nethack.h"
#include "menulist.h"
#include "common_options.h"
#include "tap.h"
#include "testgame.h"
#include "hacklib.h"
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static unsigned long long test_seed;
static char temp_directory[] = "nethack4-testsuite-XXXXXX\0";
static bool test_system_inited = false;
static bool test_verbose = false;
static int testnumber = 1;
static int cmdnumber = 0;
static const char *curcmd, *curcmd_ptr;
static char test_crga[4];
static int last_monster_d, last_monster_x, last_monster_y;

static void test_pause(enum nh_pause_reason);
static void test_display_buffer(const char *, nh_bool);
static void test_update_status(struct nh_player_info *);
static void test_print_message(int, const char *);
static void test_request_command(
    nh_bool, nh_bool, nh_bool, void *,
    void (*)(const struct nh_cmd_and_arg *, void *));
static void test_display_menu(struct nh_menulist *, const char *, int, int,
                              void *, void(*)(const int *, int, void *));
static void test_display_objects(struct nh_objlist *, const char *, int, int,
                                 void *, void(*)(const struct nh_objresult *,
                                                 int, void *));
static void test_list_items(struct nh_objlist *, nh_bool);
static void test_update_screen(struct nh_dbuf_entry[ROWNO][COLNO], int, int);
static void test_raw_print(const char *);
static struct nh_query_key_result test_query_key(
    const char *, enum nh_query_key_flags, nh_bool);
static struct nh_getpos_result test_getpos(int, int, nh_bool, const char *);
static enum nh_direction test_getdir(const char *, nh_bool);
static char test_yn_function(const char *, const char *, char);
static void test_getlin(const char *, void *, void (*)(const char *, void *));
static void test_no_op_void(void);
static void test_no_op_int(int);
static void test_outrip(struct nh_menulist *, nh_bool, const char *, int,
                        const char *, int, int);

static struct nh_window_procs test_windowprocs = {
    .win_pause = test_pause,
    .win_display_buffer = test_display_buffer,
    .win_update_status = test_update_status,
    .win_print_message = test_print_message,
    .win_request_command = test_request_command,
    .win_display_menu = test_display_menu,
    .win_display_objects = test_display_objects,
    .win_list_items = test_list_items,
    .win_update_screen = test_update_screen,
    .win_raw_print = test_raw_print,
    .win_query_key = test_query_key,
    .win_getpos = test_getpos,
    .win_getdir = test_getdir,
    .win_yn_function = test_yn_function,
    .win_getlin = test_getlin,
    .win_delay = test_no_op_void,
    .win_load_progress = test_no_op_int,
    .win_level_changed = test_no_op_int,
    .win_outrip = test_outrip,
    .win_print_message_nonblocking = test_print_message,
    .win_server_cancel = test_no_op_void
};

/* Utility. */
static bool
gridbug_ok(enum nh_direction dir, bool gridbug)
{
    return !(gridbug && (dir == DIR_NW || dir == DIR_SW ||
                         dir == DIR_NE || dir == DIR_SE));
}


/* Initialization. */

void
init_test_system(unsigned long long seed, const char crga[static 4],
                 int testcount)
{
    if (test_system_inited)
        tap_bail("Initializing test system twice");

    tap_init(testcount);
    test_seed = seed;
    memcpy(test_crga, crga, 4);

    tap_comment("Using seed: %llu", seed);

    if (!mkdtemp(temp_directory))
        tap_bail_errno("Creating a temporary directory");
    /* this is safe because we have an extra \0 at the end */
    temp_directory[strlen(temp_directory)] = '/';

    /* Read-only paths should point to the actual game data. Other paths point
       to a temporary directory we make for the purpose. */
    const char *gsd = aimake_get_option("gamesdatadir");
    size_t gsdlen = strlen(gsd);
    char gsd_with_slash[gsdlen + 2];
    strcpy(gsd_with_slash, gsd);
    if (gsdlen && gsd[gsdlen - 1] != '/' && gsd[gsdlen - 1] != '\\') {
        gsd_with_slash[gsdlen] = '/';
        gsd_with_slash[gsdlen + 1]= '\0';
    }


    const char *paths[PREFIX_COUNT] = {
        [BONESPREFIX] = "$OMIT",           /* disable bones */
        [DATAPREFIX] = gsd_with_slash,     /* read-only, use default location */
        [SCOREPREFIX] = temp_directory,    /* read-write, necessary */
        [LOCKPREFIX] = temp_directory,     /* unused */
        [TROUBLEPREFIX] = temp_directory,  /* write-only, unneeded but wanted */
        [DUMPPREFIX] = "$OMIT",            /* write-only, unneeded */
    };

    nh_lib_init(&test_windowprocs, paths);
}

void
shutdown_test_system(void)
{
    nh_lib_exit();
    char logfiles[strlen(temp_directory) + 9];

    strcpy(logfiles, temp_directory);
    strcat(logfiles, "paniclog");
    remove(logfiles);

    strcpy(logfiles, temp_directory);
    strcat(logfiles, "logfile");
    remove(logfiles);

    strcpy(logfiles, temp_directory);
    strcat(logfiles, "xlogfile");
    remove(logfiles);

    strcpy(logfiles, temp_directory);
    strcat(logfiles, "record");
    remove(logfiles);

    rmdir(temp_directory);
}


/* Test running */

/*
 * Creates a new game, then runs the commands given as its argument. Prints a
 * TAP ok if the game doesn't crash, doesn't produce an error return, and
 * doesn't lengthen the paniclog, or a not ok otherwise (listing the seed and
 * command sequence to reproduce).
 *
 * The commands are separated by commas (without spaces), in order to produce
 * sensible test names. For the time being, commands are never given arguments
 * (mostly because some cases where we want to answer prompts, such as wizmode
 * wishing, don't accept bundled arguments anyway); we use separate arguments
 * afterwards. Thus, "commands" actually contains responses to all expected
 * prompts. We use the following syntax to disambiguate:
 *
 * command     (lowercase initial letter, no quoting)    request_command
 * M1:4:16:64  (capital M, colon-separated decimal)      display_menu
 * Oab3c       (capital O, accel or count+accel)         display_objects
 * Ky          (capital K then one letter)               query_key
 * P10:10      (capital P, colon-separated decimal)      getpos
 * Pm          (literal)                                 getpos on monster
 * D4          (capital D, direction number)             getdir
 * Dm          (literal)                                 getdir on monster
 * Yy          (capital Y then one letter)               yn
 * "7 candles" (string in double-quotes)                 getlin
 *
 * In most cases, if a prompt appears that doesn't appear in "commands", the
 * testbench will improvise an appropriate response. The exception is an
 * unexpected request_command that isn't at the end of the commands string,
 * which is an error, because it implies that some of the prompt-answers
 * provided went unused; we bounce this situation off the main loop by
 * substituting a "panic" command.
 *
 * The # character may not be used in commands, and commas may not be used
 * except as separators. This currently isn't verified, and just ends up
 * producing invalid TAP or potentially sending the wrong commands to the
 * client.
 */
void
play_test_game(const char *commands, bool verbose)
{
    curcmd = commands;
    curcmd_ptr = curcmd;

    /* Our starting seed is based on the test seed as a whole, and the test
       count. (Basing it on the test count means that we're not generating the
       same dungeon each time, meaning that we get some free testing of the
       dungeon generator in at the same time as we're testing other things.) */
    unsigned long long this_test_seed = test_seed;
    this_test_seed += (testnumber * 1000000LLU);
    test_verbose = verbose;

    last_monster_d = DIR_NONE;
    last_monster_x = -1;
    last_monster_y = -1;
    cmdnumber = 0;

    char paniclog[strlen(temp_directory) + 9];
    strcpy(paniclog, temp_directory);
    strcat(paniclog, "paniclog");
    int paniclogfd = open(paniclog, O_CREAT | O_RDWR, 0644);
    if (paniclogfd < 0)
        tap_bail_errno("Opening paniclog");
    lseek(paniclogfd, 0, SEEK_END);

    struct nh_option_desc *default_options = nh_get_options();
    struct nh_option_desc *newgame_options =
        nhlib_clone_optlist(default_options);
    nhlib_free_optlist(default_options);

    static const char *const required_options[] = {
        "seed", "mode", "role", "race", "gender", "align"
    };
    int i;
    char seedbuf[80] = "<uninitialized>";
    for (i = 0; i < sizeof required_options / sizeof *required_options; i++) {
        struct nh_option_desc *opt =
            nhlib_find_option(newgame_options, required_options[i]);
        union nh_optvalue v;
        if (!opt)
            tap_bail("missing game creation option");

        /* We need a sensible initial value for this option. */
        if (i == 0) {
            v.s = seedbuf;
            /* Seeds are 16 characters of base64. We use just the digits. */
            snprintf(seedbuf, sizeof seedbuf, "%016llu", this_test_seed);
            seedbuf[17] = '\0';
        } else if (i == 1) {
            v.e = MODE_WIZARD;
        } else {
            /* Search the list of choices in the option description for one
               that starts with the given letter. Case-insensitive, but
               lowercase letters search backwards and capital letters search
               forwards (to distinguish between Rogue and Ranger). */
            char firstletter = test_crga[i - 2];
            int j, d;
            if (firstletter >= 'A' && firstletter <= 'Z') {
                j = 0;
                d = 1;
            } else {
                j = opt->e.numchoices - 1;
                d = -1;
            }
            v.e = -1;
            while (j >= 0 && d < opt->e.numchoices) {
                const struct nh_listitem *choice = &(opt->e.choices[j]);
                if (choice->caption && choice->id >= 0) {
                    if (toupper(*choice->caption) == toupper(firstletter)) {
                        v.e = choice->id;
                        break;
                    }
                }
                j += d;
            }
            if (v.e == -1)
                tap_bail("could not match crga option");
        }
        nhlib_copy_option_value(opt, v);
    }

    FILE *savefile = tmpfile();
    if (!savefile)
        tap_bail_errno("could not create save file");
    int fd = fileno(savefile);

    /* from this point on, the seed is involved, so we fail rather than bail if
       something goes wrong (and try with other seeds) */

    enum nh_create_response nhcr = nh_create_game(fd, newgame_options);
    bool start_or_restart = true;
    bool ok = false;
    bool keep_savefile = false;
    switch (nhcr) {
    case NHCREATE_FAIL:
        tap_comment("creating game: failure");
        break;
    case NHCREATE_INVALID:
        tap_comment("creating game: invalid options");
        break;
    case NHCREATE_OK:
        while (start_or_restart) {
            start_or_restart = false;
            switch (nh_play_game(fd, FM_PLAY)) {
                /* Our success status is GAME_OVER, because we exit via quitting
                   (and via the DYWYPI prompt) when everything goes right. */
            case GAME_OVER:
                ok = true;
                break;

                /* Restart statuses. CLIENT_RESTART and REPLAY_FINISHED should
                   never happen, so despite being restart statuses, we treat
                   them as errors. */
            case RESTART_PLAY:
                start_or_restart = true;
                break;

                /* Normally benign, but should be caused only via client action,
                   and we haven't caused them. */
            case GAME_DETACHED:
                tap_comment("playing game: unexpected detach");
                break;
            case GAME_ALREADY_OVER:
                tap_comment("playing game: unexpected already-over condition");
                break;
            case CLIENT_RESTART:
                tap_comment("playing game: unexpected client restart");
                break;
            case REPLAY_FINISHED:
                tap_comment("playing game: unexpected replay finish");
                break;

                /* Unrecoverable errors. */
            case ERR_BAD_ARGS:
                tap_comment("playing game: bad fd");
                break;
            case ERR_BAD_FILE:
                tap_comment("playing game: bad save file");
                keep_savefile = true;
                break;
            case ERR_IN_PROGRESS:
                tap_comment("playing game: locking issues");
                keep_savefile = true;
                break;
            case ERR_RESTORE_FAILED:
                tap_comment("playing game: corrupted save");
                keep_savefile = true;
                break;
            default:
                tap_comment("playing game: bad nh_play_game return");
                break;

                /* Recoverable errors. These are still test failures, though. */
            case ERR_RECOVER_REFUSED:
                tap_comment("playing game: recoverable error");
                keep_savefile = true;
                break;
            }
        }
        break;
    }

    char paniclog_buffer[4096];
    int plen = read(paniclogfd, paniclog_buffer, sizeof paniclog_buffer);
    if (plen) {
        char *plogline = NULL;
        if (ok) {
            tap_comment("playing game: nonempty paniclog");
            ok = 0;
        }
        while ((plogline = strtok(plogline ? NULL : paniclog_buffer, "\n")))
            tap_comment("paniclog: %s", plogline);
    }

    if (keep_savefile && !ok) {
        char savefilename[strlen(temp_directory) +
                          sizeof "testcase-4294967295.nhgame"];
        snprintf(savefilename, sizeof savefilename,
                 "%stestcase-%d.nhgame", temp_directory, testnumber);

        FILE *savefilebackup = fopen(savefilename, "wb");
        if (savefilebackup) {

            tap_comment("Saving savefile for failing test at %s", savefilename);
            lseek(fd, 0, SEEK_SET);
            char buf[1024];
            int rcount;
            while (((rcount = read(fd, buf, sizeof(buf) > 1))) > 0 ||
                   errno == EINTR)
                fwrite(buf, 1, rcount, savefilebackup);
            fclose(savefilebackup);

        } else
            tap_comment("Couldn't back savefile up at %s", savefilename);
    }

    tap_test(&testnumber, ok, "%s [seed %s]", commands, seedbuf);

    fclose(savefile);
    close(paniclogfd);
    nhlib_free_optlist(newgame_options);
}

/* Like play_test_game, but doesn't actually run the game. */
void
skip_test_game(const char *commands, bool verbose)
{
    (void) verbose;
    char seedbuf[80] = "<uninitialized>";
    unsigned long long this_test_seed = test_seed;
    this_test_seed += (testnumber * 1000000LLU);
    snprintf(seedbuf, sizeof seedbuf, "%016llu", this_test_seed);
    seedbuf[17] = '\0';
    tap_skip(&testnumber, "%s [seed %s]", commands, seedbuf);
}


/* Window procedures: error detection */

/*
 * raw_print is only called by libnethack in extreme error conditions. (Most of
 * the time, the error goes via display_menu for the desync dialog box.) The
 * list of situations which cause this, at the time of writing (not including
 * situations which immediately call another function that calls raw_print, or
 * errors reported by the client library):
 *
 * recoverquit failure              (paniclog: recoverquit, GAME_ALREADY_OVER)
 * error_reading_save               (no paniclog,           ERR_RESTORE_FAILED)
 * log_recover_core locking failure (no further paniclog,   ERR_IN_PROGRESS)
 * unrecoverable save file          (no further paniclog,   ERR_RESTORE_FAILED)
 * log_desync with a later backup   (no paniclog,           RESTART_PLAY)
 * log_desync with no later backup  (no paniclog,           via log_recover)
 * init_objects failure             (no paniclog,           doesn't exit)
 *
 * Most of these produce an error condition that we can find some other way:
 * recoverquit failures touch the paniclog, recovery errors shouldn't be
 * happening and anyway produce failure returns, errors reading save files also
 * produce failure returns, and log_desync will call into log_recover (the
 * desync dialog box, which is visible to us) unless there's a future save diff
 * or backup to jump to (which shouldn't happen except when in watch or replay
 * mode or reloading a save, cases which currenly aren't tested).
 *
 * The init_objects failure is an exception, presumably due to being in
 * nh_create_game rather than nh_play_game. The desync dialog box does actually
 * work at that point in the code, at least if you choose "Repair". (The other
 * options are unlikely to produce a useable save file.) I can see why just
 * logging the problem would be reasonable, but it should probably at least be
 * in the paniclog. TODO: Fix that.
 *
 * Anyway, the upshot of all this is that we just log the comment, without
 * automatically producing a test failure as a result.
 */
static void
test_raw_print(const char *message)
{
    size_t len = strlen(message);
    if (message[len-1] == '\n')
        tap_comment("Low-level libnethack error: %.*s", (int)(len-1), message);
    else
        tap_comment("Low-level libnethack error: %s", message);
}


/* Window procedures: server providing information to the testbench */

/* In most cases, we'll be ignoring messages from the server. There's one
   notable exception, though. A common test pattern is "create a monster, then
   do things with it". Unlike items, which can share the player's square, a
   monster is created next to or near the player (via #genesis), and it prints a
   message to let the testbench know where the monster is. Although we're
   ignoring nearly all messages, we want to parse that one. */
static void
test_print_message(int turncount, const char *message)
{
    (void) turncount;

    int dummy[3];

    if (sscanf(message, "Created a monster at (%d, %d), direction %d",
               dummy + 0, dummy + 1, dummy + 2) == 3) {
        last_monster_x = dummy[0];
        last_monster_y = dummy[1];
        last_monster_d = dummy[2];
    }

    if (test_verbose)
        tap_comment("pline: %s", message);
}


/* Window procedures: server requesting information from the testbench */

static void
test_request_command(
    nh_bool debug, nh_bool completed, nh_bool interrupted, void *callbackarg,
    void (*callback)(const struct nh_cmd_and_arg *ncaa, void *arg))
{
    /* First case: if we gave a multi-turn command, continue it. (Even if we're
       interrupted; we ignore the interruptions, using wizmode lifesaving if
       necessary). */
    if (!completed) {
        if (interrupted)
            if (test_verbose)
                tap_comment("command (interrupted): repeat");

        callback(&(struct nh_cmd_and_arg){"repeat", {.argtype = 0}},
                 callbackarg);
        return;
    }

    /* Next case: if we reached the end of our command list, quit gracefully.

       We abandon the game (meaning we end up going through the DYWYPI prompts
       much as a normal player would); this means that the game-over sequence
       gets tested, and that we finally end up sending our success return to the
       game engine. */
    if (!*curcmd_ptr) {
        nh_exit_game(EXIT_QUIT);
        /* should be unreachable; nh_exit_game returns only if the game wasn't
           running, in which case we shouldn't have been called in the first
           place */
        tap_bail("nh_exit_game returned");
    }

    /* Main case: we're expecting the client to request a command. */
    if ('a' <= *curcmd_ptr && *curcmd_ptr <= 'z') {
        const char *eos = strchr(curcmd_ptr, ',');
        if (!eos)
            eos = curcmd_ptr + strlen(curcmd_ptr);

        char cmdname[eos - curcmd_ptr + 1];
        memcpy(cmdname, curcmd_ptr, eos - curcmd_ptr);
        cmdname[eos - curcmd_ptr] = '\0';
        curcmd_ptr = *eos ? eos + 1 : eos;

        if (test_verbose)
            tap_comment("command (from command): %s", cmdname);

        callback(&(struct nh_cmd_and_arg){cmdname, {.argtype = 0}},
                 callbackarg);
        return;
    }

    /* Final fallback: we're not expecting a command right now, panic.

       The client can't normally panic on request, because it would be
       exploitable. However, as we're being asked for a command right now, we
       can just send #panic and panic that way. */
    tap_comment("byte %td of command string: engine requested command, "
                "but we have a '%c' instead", curcmd_ptr - curcmd, *curcmd_ptr);
    callback(&(struct nh_cmd_and_arg){"panic", {.argtype = 0}}, callbackarg);
}

static void
test_display_menu(struct nh_menulist *ml, const char *title,
                  int pick_type, int placement_hint, void *callbackarg,
                  void (*callback)(const int *choices, int nchoices, void *arg))
{
    int i;

    /* Special case: the desync dialog box. If this ever comes up, it's an
       automatic test failure.

       We communicate the failure back to the main part of the code via
       reverse-bouncing it off the server's main loop using ERR_RECOVER_REFUSED
       (which won't be produced in other contexts). This uses knowledge of the
       menulist we'll have been given; the index in question is 2.

       First, though, we tap_comment the log_recover_core_reasons, to make
       debugging the failure easier. */
    if (title && strcmp(title, "The save file is corrupted...") == 0) {
        for (i = 0; i < ml->icount; i++) {
            if (strncmp(ml->items[i].caption, "Error: ", 7) == 0 ||
                strncmp(ml->items[i].caption, "Location: ", 10) == 0)
                tap_comment("desync dialog: %s", ml->items[i].caption);
        }

        dealloc_menulist(ml);
        int selected = 2;
        callback(&selected, 1, callbackarg);
        return;
    }

    int results = 0;

    /* If we're not being asked for input, don't give any. */
    if (pick_type == PICK_NONE) {
        dealloc_menulist(ml);
        callback(&results, -1, callbackarg);
        return;
    }

    if (test_verbose)
        tap_comment("display_menu: %s", title ? title : "<no title>");

    /* Do we have a menu specification? */
    if (*curcmd_ptr == 'M') {
        /* TODO */
        tap_bail("'M' unimplemented");
    }

    /* Break out of loops. */
    if (popcount(++cmdnumber) >= 4) {
        dealloc_menulist(ml);
        callback(&results, -1, callbackarg);
        return;
    }

    /* We don't; pick a "random" menu item. We select exactly one item, even
       when the menu is multi-select, for simplicity. TODO: Consider changing
       this. */
    int pickchance = 1;
    unsigned long long s = test_seed + testnumber;
    char picked_accel = 0;
    for (i = 0; i < ml->icount; i++) {
        if (ml->items[i].accel || ml->items[i].id) {
            /* It's selectable. This algorithm picks menu items in a way that's
               consistent for a given seed, and which is as stable as possible
               against changes in the number of menu items. */
            if (s % pickchance == 1 || !picked_accel) {
                results = ml->items[i].id;
                picked_accel = ml->items[i].accel;
            }
            s /= pickchance;
            /* recycle entropy */
            s += test_seed + (unsigned long)testnumber * 101094863ULL;
            pickchance++;
        }
    }

    if (test_verbose)
        tap_comment("display_menu reply (random): %c", picked_accel);

    dealloc_menulist(ml);
    callback(&results, 1, callbackarg);
}

static void
test_display_objects(
    struct nh_objlist *ml, const char *title, int pick_type,
    int placement_hint, void *callbackarg, void (*callback)
    (const struct nh_objresult *choices, int nchoices, void *arg))
{
    int i;
    struct nh_objresult results = {0, -1};

    /* If we're not being asked for input, don't give any. */
    if (pick_type == PICK_NONE) {
        dealloc_objmenulist(ml);
        callback(&results, -1, callbackarg);
        return;
    }

    if (test_verbose)
        tap_comment("display_objects: %s", title ? title : "<no title>");

    /* Do we have a menu specification? */
    if (*curcmd_ptr == 'O') {
        /* TODO */
        tap_bail("'O' unimplemented");
    }

    /* Break out of loops. */
    if (popcount(++cmdnumber) >= 4) {
        dealloc_objmenulist(ml);
        callback(&results, -1, callbackarg);
        return;
    }

    /* The same code as in test_display_menu. Except not: the struct fields are
       fields of a different struct, and thus have different indexes, and so
       this compiles to different binary despite being textually almost
       identical (we're setting results.id, not results). */
    int pickchance = 1;
    char picked_accel = 0;
    unsigned long long s = test_seed + testnumber;
    for (i = 0; i < ml->icount; i++) {
        if (ml->items[i].id || ml->items[i].accel) {
            if (!(s % pickchance)) {
                results.id = ml->items[i].id;
                picked_accel = ml->items[i].accel;
            }
            s /= pickchance;
            pickchance++;
        }
    }

    if (test_verbose)
        tap_comment("display_objects reply (random): %c", picked_accel);

    dealloc_objmenulist(ml);
    callback(&results, 1, callbackarg);
}

static struct nh_query_key_result
test_query_key(const char *prompt, enum nh_query_key_flags flags,
               nh_bool allow_count)
{
    if (test_verbose)
        tap_comment("query_key: %s", prompt);

    /* Do we have a query_key specification? */
    if (*curcmd_ptr == 'K') {
        char res = curcmd_ptr[1];
        if (!res)
            tap_bail("unexpected end of string in command");
        if (curcmd_ptr[2] == '\0')
            curcmd_ptr += 2;
        else if (curcmd_ptr[2] == ',')
            curcmd_ptr += 3;
        else
            tap_bail("query_key token is more than 2 chars long");
        if (test_verbose)
            tap_comment("query_key reply (from command): %c", res);
        return (struct nh_query_key_result){.key = res, .count = -1};
    }
    /* Nope, we'll have to improvise a response. */

    /* Break out of loops. */
    if (popcount(++cmdnumber) >= 4) {
        if (test_verbose)
            tap_comment("query_key reply (cancel): <cancelled>");
        return (struct nh_query_key_result){.key = 27, .count = -1};
    }

    /* If we're given Z as an option, take it; the testsuite normally wishes
       items it wants to test into that slot. */
    if (pmatch("*[*Z*]", prompt)) {
        if (test_verbose)
            tap_comment("query_key reply (policy): Z");
        return (struct nh_query_key_result){.key = 'Z', .count = -1};
    }

    /* In most cases, a query_key prompt will specify the sensible possibilities
       in square brackets. And in most of /those/ cases, the possibilities are
       listable via '?' (sensible) or '*' (possible). We can detect that we're
       in this case by looking for the string "*]" in the prompt. In such a
       case, we just hit '*' and let the menu handling code pick an item for
       us. */
    if (strstr(prompt, "*]")) {
        if (test_verbose)
            tap_comment("query_key reply (policy): *");
        return (struct nh_query_key_result){.key = '*', .count = -1};
    }

    /* Do the flags give a hint? */
    if (flags == NQKF_LETTER_REASSIGNMENT || flags == NQKF_SYMBOL) {
        char k = flags == NQKF_SYMBOL ?
            ' ' + ((test_seed + testnumber) % 94) :
            'a' + ((test_seed + testnumber) % 26);

        if (test_verbose)
            tap_comment("query_key reply (random): <cancelled>");

        return (struct nh_query_key_result){.key = k, .count = -1};
    }

    if (test_verbose)
        tap_comment("query_key reply (random): <cancelled>");

    /* We don't really know what to press here. So cancel. */
    return (struct nh_query_key_result){.key = 27, .count = -1};
}

static char
test_yn_function(const char *query, const char *answers, char default_answer)
{
    if (test_verbose)
        tap_comment("yn: %s", query);

    /* Do we have a yn specification? This is basically the same as with
       query_key. */
    if (*curcmd_ptr == 'Y') {
        char res = curcmd_ptr[1];
        if (!res)
            tap_bail("unexpected end of string in command");
        if (curcmd_ptr[2] == '\0')
            curcmd_ptr += 2;
        else if (curcmd_ptr[2] == ',')
            curcmd_ptr += 3;
        else
            tap_bail("yn_function token is more than 2 chars long");

        if (strchr(answers, res)) {
            if (test_verbose)
                tap_comment("yn reply (from command): %c", res);
            return res;
        }
        tap_comment("expected illegal yn answer '%c' (legal: '%s')",
                    res, answers);
        nh_exit_game(EXIT_PANIC);      /* indirectly cause a test failure */
        return default_answer;         /* probably unreachable */
    }
    /* Nope, we'll have to improvise a response. */

    char ynr = answers[(test_seed + testnumber) % strlen(answers)];

    /* Break out of loops. */
    if (popcount(++cmdnumber) >= 4) {
        if (test_verbose)
            tap_comment("yn reply (cancel): %c", ynr);
        return ynr;
    }


    /* yn_function is often (usually?) used for confirmation prompts. In most
       cases, we want to say "yes", because the path for "no" is boring.
       Exception: if we die, we want to say "no" to use debug mode lifesaving
       (thus allowing us to complete whatever test we were running. */
    if (strcmp(query, "Die?") == 0) {
        if (test_verbose)
            tap_comment("yn reply (policy): n");
        return 'n';
    } else if (strchr(answers, 'y')) {
        if (test_verbose)
            tap_comment("yn reply (policy): y");
        return 'y';
    }

    /* Fall back to random. */
    if (test_verbose)
        tap_comment("yn reply (random): %c", ynr);
    return ynr;
}


static struct nh_getpos_result
test_getpos(int default_x, int default_y, nh_bool force, const char *msg)
{
    if (test_verbose)
        tap_comment("getpos: %s", msg);

    /* Do we have a getpos specification? */
    if (*curcmd_ptr == 'P') {
        if (curcmd_ptr[1] == 'm') {
            curcmd_ptr += 2;
            if (*curcmd_ptr) {
                if (*curcmd_ptr == ',')
                    curcmd_ptr++;
                else
                    tap_bail("junk after 'Pm' command");
            }
            /* fall through */
        } else {
            tap_bail("'P' unimplemented (except 'Pm')");
        }
    }

    /* Break out of loops. */
    if (popcount(++cmdnumber) >= 4) {
        if (test_verbose)
            tap_comment("getpos reply (cancel): (%d,%d)", default_x, default_y);
        return (struct nh_getpos_result){.howclosed = NHCR_CLIENT_CANCEL,
                .x = default_x,
                .y = default_y};
    }


    /* Do we have a monster to aim at? */
    if (last_monster_x > -1 && last_monster_y > -1) {
        if (test_verbose)
            tap_comment("getpos reply (at monster): (%d,%d)",
                        last_monster_x, last_monster_y);
        return (struct nh_getpos_result){.howclosed = NHCR_ACCEPTED,
                .x = last_monster_x,
                .y = last_monster_y};
    }

    if (test_verbose)
        tap_comment("getpos reply (random): (%d,%d)", default_x, default_y);

    /* We don't, so aim at the default location. */
    return (struct nh_getpos_result){.howclosed = NHCR_ACCEPTED,
            .x = default_x,
            .y = default_y};
}


static enum nh_direction
test_getdir(const char *prompt, nh_bool gridbug)
{
    static const char *const dirnames[] = {
        [DIR_W] = "west",
        [DIR_NW] = "northwest",
        [DIR_N] = "north",
        [DIR_NE] = "northeast",
        [DIR_E] = "east",
        [DIR_SE] = "southeast",
        [DIR_S] = "south",
        [DIR_SW] = "southwest",
        [DIR_UP] = "up",
        [DIR_DOWN] = "down",
        [DIR_SELF] = "self"
    };

    if (test_verbose)
        tap_comment("getdir: %s", prompt);

    /* Do we have a getdir specification? */
    if (*curcmd_ptr == 'D') {
        if (curcmd_ptr[1] == 'm') {
            curcmd_ptr += 2;
            if (*curcmd_ptr) {
                if (*curcmd_ptr == ',')
                    curcmd_ptr++;
                else
                    tap_bail("junk after 'Dm' command");
            }
            /* fall through */
        } else {
            tap_bail("'D' unimplemented (except 'Dm')");
        }
    }

    /* Break out of loops. */
    if (popcount(++cmdnumber) >= 4)
        return DIR_NONE;

    /* Do we have a monster to aim at? */
    if (last_monster_d != DIR_NONE && gridbug_ok(last_monster_d, gridbug)) {
        if (test_verbose)
            tap_comment("getdir reply (at monster): %s",
                        dirnames[last_monster_d]);
        return last_monster_d;
    }

    /* We don't, so aim in a random direction. */
    enum nh_direction dir = (test_seed + testnumber) % 11;
    if (gridbug_ok(dir, gridbug)) {
        if (test_verbose)
            tap_comment("getdir reply (random): %s", dirnames[dir]);
        return dir;
    }

    if (test_verbose)
        tap_comment("getdir reply (random): north");

    return DIR_N;
}


static void
test_getlin(const char *query, void *callbackarg,
            void (*callback)(const char *lin, void *arg))
{
    if (test_verbose)
        tap_comment("getlin: %s", query);

    /* Do we have a string in our command list? */
    if (*curcmd_ptr == '"') {
        char *eos = strchr(curcmd_ptr + 1, '"');
        if (!eos)
            tap_bail("mismatched quotes in getlin specification");

        char lin[eos - (curcmd_ptr + 1) + 1];
        memcpy(lin, curcmd_ptr + 1, eos - (curcmd_ptr + 1));
        lin[eos - (curcmd_ptr + 1)] = '\0';

        curcmd_ptr = eos + 1;
        if (*curcmd_ptr) {
            if (*curcmd_ptr == ',')
                curcmd_ptr++;
            else
                tap_bail("junk after getlin specification");
        }

        if (test_verbose)
            tap_comment("getlin reply (from command): %s", lin);
        callback(lin, callbackarg);
        return;
    }

    /* Nope. TODO: Pick a string from a list of game-relevant strings. For now,
       we just cancel. */
    if (test_verbose)
        tap_comment("getlin reply (random): <cancelled>");
    callback("\x1b", callbackarg);
}



/* Window procedures: no-ops.

   None of this is very interesting; it just ignores all given arguments, except
   that any menulists are deallocated (as per the normal behaviour of menulist
   arguments sent over the API). */
static void
test_list_items(struct nh_objlist *ml, nh_bool unused)
{
    dealloc_objmenulist(ml);
    (void) unused;
}

static void
test_no_op_void(void)
{
}

static void
test_no_op_int(int unused)
{
    (void) unused;
}

static void
test_pause(enum nh_pause_reason unused)
{
    (void) unused;
}

static void
test_display_buffer(const char *unused1, nh_bool unused2)
{
    (void) unused1;
    (void) unused2;
}

static void
test_update_status(struct nh_player_info *unused)
{
    (void) unused;
}

static void
test_update_screen(struct nh_dbuf_entry unused1[ROWNO][COLNO],
                   int unused2, int unused3)
{
    (void) unused1;
    (void) unused2;
    (void) unused3;
}

static void test_outrip(struct nh_menulist *ml, nh_bool unused1,
                        const char *unused2, int unused3,
                        const char *unused4, int unused5, int unused6)
{
    dealloc_menulist(ml);
    (void) unused1;
    (void) unused2;
    (void) unused3;
    (void) unused4;
    (void) unused5;
    (void) unused6;
}
