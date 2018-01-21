/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-01-21 */
/* Copyright (c) Fredrik Ljungdahl, 2017. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>

/* How often we create checkpoints */
#define CHECKPOINT_FREQ 50

static struct nh_window_procs orig_winprocs = {0};

/* Replaymode handling + replaymode windowport stuff */

struct checkpoint {
    struct checkpoint *prev;
    struct checkpoint *next;
    int action;
    int move;
    int from_action;
    int from_move;
    int by_desync;
    struct sinfo program_state;
    struct memfile binary_save;
    long file_location;
};

struct replayinfo {
    int action; /* current action */
    int move; /* current move */
    int msg; /* msg within action -- to allow proper message history */
    int max; /* last action */
    int target; /* target action or move */
    boolean reverse; /* if seeking in reverse */
    boolean target_is_move; /* whether target is by action or by move */
    boolean replaying; /* if we are using the replay "windowport" */
    char cmd[BUFSZ]; /* command ID of next command */
    struct checkpoint *prev_checkpoint;
    struct checkpoint *next_checkpoint;
    char game_id[BUFSZ]; /* unique for each game */

    /* Replay loading may not be near-instantaneous. Avoid queueing up
       several commands just beecaus replay reload took a bit of time by
       swallowing commands briefly. */
    microseconds last_load;
#define LOAD_DELAY 10000

    boolean desync;
    boolean in_load; /* Used to check for a desync */

    /* Mark that we have already jumped backwards once. This is needed to avoid
       a loop by a move we are seeking to being skipped. */
    boolean jumped;
    char *diff_ok;
    boolean diff_allocated;
    int max_old;
};

static struct replayinfo replay = {0};

static void replay_pause(enum nh_pause_reason);
static void replay_display_buffer(const char *, boolean);
static void replay_update_status(struct nh_player_info *);
static void replay_print_message(int, int, int, enum msg_channel, const char *);
static void replay_request_command(
    boolean, boolean, boolean, void *,
    void (*)(const struct nh_cmd_and_arg *, void *));
static void replay_display_menu(struct nh_menulist *, const char *, int, int,
                              void *, void(*)(const int *, int, void *));
static void replay_display_objects(struct nh_objlist *, const char *, int, int,
                                 void *, void(*)(const struct nh_objresult *,
                                                 int, void *));
static void replay_list_items(struct nh_objlist *, boolean);
static void replay_update_screen(struct nh_dbuf_entry[ROWNO][COLNO], int, int);
static void replay_raw_print(const char *);
static struct nh_query_key_result replay_query_key(
    const char *, enum nh_query_key_flags, boolean);
static struct nh_getpos_result replay_getpos(int, int, boolean, const char *);
static enum nh_direction replay_getdir(const char *, boolean);
static char replay_yn_function(const char *, const char *, char);
static void replay_getlin(const char *, void *, void (*)(const char *, void *));
static void replay_format(const char *, int, int, void *, void (*)(const char *,
                                                                   void *));
static void replay_no_op_void(void);
static void replay_no_op_int(int);
static void replay_outrip(struct nh_menulist *, boolean, const char *, int,
                          const char *, int, int);
static void replay_set_windowport(void);
static void replay_panic(const char *);
static void replay_goal_reached(void);
static void replay_save_checkpoint(struct checkpoint *);
static void replay_restore_checkpoint(struct checkpoint *);
static void replay_add_desync(boolean);

static struct nh_window_procs replay_windowprocs = {
    .win_pause = replay_pause,
    .win_display_buffer = replay_display_buffer,
    .win_update_status = replay_update_status,
    .win_print_message = replay_print_message,
    .win_request_command = replay_request_command,
    .win_display_menu = replay_display_menu,
    .win_display_objects = replay_display_objects,
    .win_list_items = replay_list_items,
    .win_update_screen = replay_update_screen,
    .win_raw_print = replay_raw_print,
    .win_query_key = replay_query_key,
    .win_getpos = replay_getpos,
    .win_getdir = replay_getdir,
    .win_yn_function = replay_yn_function,
    .win_getlin = replay_getlin,
    .win_format = replay_format,
    .win_delay = replay_no_op_void,
    .win_load_progress = replay_no_op_int,
    .win_level_changed = replay_no_op_int,
    .win_outrip = replay_outrip,
    .win_server_cancel = replay_no_op_void
};

static void
replay_pause(enum nh_pause_reason unused)
{
    (void) unused;
}

static void
replay_display_buffer(const char *unused1, boolean unused2)
{
    (void) unused1;
    (void) unused2;
}

static void
replay_update_status(struct nh_player_info *unused)
{
    (void) unused;
}

static void
replay_print_message(int action, int id, int turn, enum msg_channel msgc,
                     const char *message)
{
    orig_winprocs.win_print_message(replay.action, replay.msg, turn, msgc,
                                    message);
    if (msgc != msgc_setaction)
        replay.msg++;
}

static void
replay_request_command(boolean debug, boolean completed, boolean interrupted,
                       void *callbackarg,
                       void (*callback)(const struct nh_cmd_and_arg *ncaa,
                                        void *arg))
{
    replay_panic("replay_request_command");
}

static void
replay_display_menu(struct nh_menulist *ml, const char *title,
                    int pick_type, int placement_hint, void *callbackarg,
                    void (*callback)(const int *choices, int nchoices,
                                     void *arg))
{
    dealloc_menulist(ml);
    replay_panic("replay_display_menu");
}

static void
replay_display_objects(
    struct nh_objlist *ml, const char *title, int pick_type,
    int placement_hint, void *callbackarg, void (*callback)
    (const struct nh_objresult *choices, int nchoices, void *arg))
{
    dealloc_objmenulist(ml);
    replay_panic("replay_display_objmenu");
}

static void
replay_list_items(struct nh_objlist *ml, boolean invent)
{
    dealloc_objmenulist(ml);
}

static void
replay_update_screen(struct nh_dbuf_entry unused1[ROWNO][COLNO],
                     int unused2, int unused3)
{
    (void) unused1;
    (void) unused2;
    (void) unused3;
}

static void
replay_raw_print(const char *message)
{
    orig_winprocs.win_raw_print(message);
}

static struct nh_query_key_result
replay_query_key(const char *prompt, enum nh_query_key_flags flags,
                 boolean allow_count)
{
    replay_panic("replay_query_key");
    return orig_winprocs.win_query_key(prompt, flags, allow_count);
}

static struct nh_getpos_result
replay_getpos(int default_x, int default_y, boolean force, const char *key)
{
    replay_panic("replay_getpos");
    return orig_winprocs.win_getpos(default_x, default_y, force, key);
}

static enum nh_direction
replay_getdir(const char *prompt, boolean gridbug)
{
    replay_panic("replay_getdir");
    return orig_winprocs.win_getdir(prompt, gridbug);
}

static char
replay_yn_function(const char *query, const char *answers, char default_answer)
{
    replay_panic("replay_yn_function");
    return orig_winprocs.win_yn_function(query, answers, default_answer);
}

static void
replay_getlin(const char *query, void *callbackarg,
              void (*callback)(const char *lin, void *arg))
{
    replay_panic("replay_getlin");
}

static void
replay_format(const char *formatstring, int fmt_type, int param, void *res,
              void (*callback)(const char *, void *))
{
    orig_winprocs.win_format(formatstring, fmt_type, param, res, callback);
}

static void
replay_outrip(struct nh_menulist *ml, boolean unused1,
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

static void
replay_no_op_void(void)
{
}

static void
replay_no_op_int(int unused)
{
    (void) unused;
}

/* End of windowprocs commands */

/* Sets the windowport to the replay windowport */
static void
replay_set_windowport(void)
{
    if (replay.replaying)
        return;

    replay.replaying = TRUE;
    orig_winprocs = windowprocs;
    windowprocs = replay_windowprocs;
}

/* Resets back to the original windowport and refreshes the screen. Use silent
   to avoid refreshing the screen -- used if you need to i.e. panic, which needs
   to avoid the refresh itself chaining panics. */
void
replay_reset_windowport(boolean silent)
{
    if (!replay.replaying)
        return;

    replay.replaying = FALSE;
    replay_next_cmd(replay.cmd);
    if (!strcmp(replay.cmd, "<end>") && replay.action != replay.max &&
        !silent) {
        /* probably caused by a rewind, change replay.max accordingly */
        replay.max = replay.action;
    }
    if (orig_winprocs.win_request_command)
        windowprocs = orig_winprocs;

    if (silent)
        return;

    vision_reset();
    doredraw();
    notify_levelchange(NULL);
    bot();
    update_inventory();
    update_location(FALSE);
    flush_screen();
    replay.last_load = utc_time();
}

/* Returns TRUE if we should swallow the command */
boolean
replay_delay(void)
{
    if (program_state.followmode != FM_REPLAY)
        return FALSE;

    /* Handle time travel properly */
    microseconds cur_time = utc_time();
    if (replay.last_load > cur_time)
        replay.last_load = cur_time;

    if ((replay.last_load + LOAD_DELAY) > cur_time)
        return TRUE;

    return FALSE;
}

/* Used if the engine requested a command that should never be requested in
   replaymode. */
static void
replay_panic(const char *str)
{
    panic("Replaymode requested invalid command: %s", str);
}

/* (Re)initializes replay variables */
void
replay_init(void)
{
    /* reset the windowport in case we left it in the replay one */
    replay_reset_windowport(TRUE);

    /* are we even in replaymode? */
    if (program_state.followmode != FM_REPLAY)
        return;

    /* get game id */
    char game_id[BUFSZ];
    snprintf(game_id, BUFSZ, "%s_%" PRIdLEAST64 "", u.uplname,
             (int_least64_t)u.ubirthday / 1000000L);

    strncpy(replay.cmd, "???", BUFSZ);

    /* if it's the same, reuse the replay state but force target to be
       right before our current point unless it's at the beginning */
    if (!strcmp(game_id, replay.game_id)) {
        boolean at_end = FALSE;
        if (replay.in_load) {
            replay.action--;
            replay.msg = 0;
            replay_add_desync(FALSE);
        } else {
            if (!replay_count_actions(FALSE))
                at_end = TRUE;

            log_sync(1, TLU_TURNS, FALSE);
            replay.max = replay_count_actions(FALSE);
        }

        /* reload from a checkpoint */
        if (replay.prev_checkpoint)
            replay_restore_checkpoint(replay.prev_checkpoint);

        if (at_end) {
            replay.target = replay.max;
            replay.target_is_move = FALSE;
        }
        return;
    }

    struct checkpoint *chk, *chknext;

    /* if game id is non-empty, we have existing stuff, free it */
    if (*replay.game_id) {
        if (replay.diff_allocated)
            free(replay.diff_ok);

        for (chk = replay.next_checkpoint; chk; chk = chknext) {
            chknext = chk->next;
            mfree(&chk->binary_save);
            free(chk);
        }
        for (chk = replay.prev_checkpoint; chk; chk = chknext) {
            chknext = chk->prev;
            mfree(&chk->binary_save);
            free(chk);
        }
    }

    /* clear the replay struct, we don't have anything else to do with it */
    memset(&replay, 0, sizeof (struct replayinfo));

    replay.max = replay_count_actions(TRUE);
    chk = replay.prev_checkpoint;
    while (chk && chk->prev)
        chk = chk->prev;
    if (chk) {
        replay.prev_checkpoint = NULL;
        replay.next_checkpoint = chk;
    }

    /* set game id */
    strncpy(replay.game_id, game_id, BUFSZ);
    replay.game_id[BUFSZ-1] = '\0';

    if (!replay.max) {
        /* user requested instant replay, go back to the beginning and playback
           here */
        log_sync(1, TLU_TURNS, FALSE);
        replay.max = replay_count_actions(TRUE);
        chk = replay.prev_checkpoint;
        while (chk && chk->prev)
            chk = chk->prev;
        if (chk) {
            replay.prev_checkpoint = NULL;
            replay.next_checkpoint = chk;
        }
        log_sync(1, TLU_TURNS, FALSE);
        replay_create_checkpoint(0, 0, 0);
        replay_goto(0, FALSE);
        return;
    }

    /* we don't care about the initial welcome message */
    replay.target = 1;
}

/* Sets target as if we reached our goal. Used if we actually did reach it, or
   if we arrived as close as we could. */
static void
replay_goal_reached(void)
{
    /* Cleanup temporary checkpoints */
    struct checkpoint *chk;
    struct checkpoint *chkprev;
    struct checkpoint *firstchk = NULL;
    int allowed = 100;

    for (chk = replay.prev_checkpoint; chk; chk = chkprev) {
        chkprev = chk->prev;
        if (chk->by_desync != 1)
            continue;

        if (allowed) {
            if (!--allowed)
                firstchk = chk;
            continue;
        }

        if (firstchk) {
            firstchk->prev = chkprev;
            if (chkprev)
                chkprev->next = firstchk;
        }

        mfree(&chk->binary_save);
        free(chk);
    }

    replay_reset_windowport(FALSE);
    replay.target = replay.action;
    replay.target_is_move = FALSE;
    replay.in_load = FALSE;
    replay.msg = 0;
}

/* Returns TRUE if we are in replaymode and want user input */
boolean
replay_want_userinput(void)
{
    if (program_state.followmode != FM_REPLAY)
        return FALSE;

    replay_set_windowport();
    replay.in_load = TRUE;

    /* maybe we want to load a checkpoint */
#define replaction() (replay.target_is_move ? moves : replay.action)
#define chkaction(c) (replay.target_is_move ? (c)->move : (c)->action)
#define chkfromaction(c)                                        \
    (replay.target_is_move ? (c)->from_move : (c)->from_action)

    struct checkpoint *chk = NULL;
    if (!replay.jumped &&
        replay.target != replaction()) {
        replay.jumped = TRUE;
        replay.reverse = FALSE;
        if (replay.target < replaction()) {
            replay.reverse = TRUE;
            chk = replay.prev_checkpoint;
            while (chk && chk->prev && replay.target < chkaction(chk)) {
                if (replay.target > chkfromaction(chk))
                    replay.target = chkfromaction(chk);

                chk = chk->prev;
            }

            if (replay.target < chkaction(chk))
                replay.target = chkaction(chk);
        } else if (replay.target > (replaction() + 200)) {
            chk = replay.next_checkpoint;
            if (chk && replay.target < chkfromaction(chk))
                chk = NULL;

            while (chk && chk->next &&
                   replay.target > chkfromaction(chk->next)) {
                if (replay.target < chkaction(chk->next)) {
                    replay.target = chkfromaction(chk->next);
                    break;
                }

                chk = chk->next;
            }
        }

        if (chk)
            replay_restore_checkpoint(chk);
    } else if (replay.target < replaction()) {
        if (!replay.prev_checkpoint) {
            /* shouldn't happen */
            replay_goal_reached();
            return TRUE;
        } else {
            /* overshoot */
            if (replay.reverse)
                replay.target--;
            else
                replay.target++;
            replay_restore_checkpoint(replay.prev_checkpoint);
        }
    }

    /* check if we reached the end */
    if (replay.action == replay.max) {
        replay_goal_reached();
        return TRUE;
    }

    if (replay.target == replaction() && replay.action) {
        replay_goal_reached();
        return TRUE;
    }

    /* Possibly load from our next checkpoint */
    chk = replay.next_checkpoint;
    if (chk && (chk->from_action <= replay.action))
        replay_restore_checkpoint(chk);

    /* if this action is past a certain point, or if none exist, create a
       checkpoint */
    if ((!flags.incomplete || flags.interrupted) &&
        !u_helpless(hm_all) &&
        (!replay.prev_checkpoint ||
         replay.prev_checkpoint->action + CHECKPOINT_FREQ <= replay.action) &&
        (!replay.next_checkpoint ||
         replay.next_checkpoint->action - CHECKPOINT_FREQ >= replay.action ||
         (replay.next_checkpoint->action && !replay.action)))
        replay_create_checkpoint(replay.action, 0, 0);

    return FALSE;
}

/* Parses a non-notime command in replaymode (used for replay navigation).
   Returns TRUE if we are about to change target as a result. */
boolean
replay_parse_command(const struct nh_cmd_and_arg cmd)
{
    int cmdidx = get_command_idx(cmd.cmd);

    if (cmd.arg.argtype & CMD_ARG_DIR) {
        /* If we got a direction as part of the command, and we're
           replaying, move forwards or backwards respectively. */
        switch (cmd.arg.dir) {
        case DIR_SE:
            /* Move forwards 50 turns. */
            replay_seek(50, TRUE);
            break;
        case DIR_E:
        case DIR_S:
            replay_seek(1, FALSE);
            break;
        case DIR_W:
        case DIR_N:
            replay_seek(-1, FALSE);
            break;
        case DIR_NW:
            replay_goto(1, FALSE);
            break;
        case DIR_SW:
            /* Move to the end of the replay. */
            replay_goto(0, FALSE);
            break;
        case DIR_NE:
            /* Move backwards 50 turns. */
            replay_seek(-50, TRUE);
            break;
        default:
            pline(msgc_mispaste,
                  "That direction has no meaning while replaying.");
            break;
        }
    } else if (!strcmp(cmd.cmd, "grope")) {
        /* go to a specific turn */
        int trycnt = 0;
        int turn = 0;
        const char *buf;
        const char *qbuf = "To what turn would you like to go to?";
        do {
            if (++trycnt == 2)
                qbuf = msgcat(qbuf, " [type a number above 0]");

            (*windowprocs.win_getlin) (qbuf, &buf, msg_getlin_callback);
        } while (!turn && strcmp(buf, "\033") && !digit(buf[0]) && trycnt < 10);

        if (trycnt == 10 || !strcmp(buf, "\033"))
            return FALSE; /* aborted or refused to input a number 10 times */

        turn = atoi(buf);
        replay_goto(turn, TRUE);
    } else {
        /* Internal commands weren't sent by the player, so don't
           complain about them, just ignore them. Ditto for repeat. */
        if (!(cmdlist[cmdidx].flags & CMD_INTERNAL) &&
            cmdlist[cmdidx].func)
            pline(msgc_cancelled, "Command '%s' is unavailable while replaying.",
                  cmd.cmd);
    }

    int cur_location = replay.action;
    if (replay.target_is_move)
        cur_location = moves;
    if (cur_location != replay.target)
        return TRUE;
    return FALSE;
}

/* Handles game completion in replaymode. We may not necessarily want to exit
   the game. */
noreturn void
replay_done_noreturn(void)
{
    /* If we aren't at our last action yet, this means that we ended up dying
       (or otherwise) earlier than we were supposed to. Treat this as if we
       desynced. */
    if (replay.action != replay.max) {
        replay.in_load = TRUE;
        terminate(RESTART_PLAY);
    }

    /* Otherwise, we simply reached the end. We can't safely return into the
       main loop from this point, so reset the windowport and parse commands
       until we switch target, then restart and let replay_init do what is
       needed. */
    replay_reset_windowport(FALSE);
    replay.in_load = FALSE;

    struct nh_cmd_and_arg cmd;
    int cmdidx;

    while (1) {
        do {
            (*windowprocs.win_request_command)
                (wizard, 1, flags.interrupted, &cmd,
                 msg_request_command_callback);
        } while (replay_delay());
        cmdidx = get_command_idx(cmd.cmd);

        if (cmdlist[cmdidx].flags & CMD_NOTIME) {
            program_state.in_zero_time_command = TRUE;
            command_input(cmdidx, &(cmd.arg));
        } else if (replay_parse_command(cmd))
            terminate(RESTART_PLAY);
    }
}

/* Updates replay.action for a new action. */
void
replay_set_action(void)
{
    if (program_state.followmode != FM_REPLAY)
        return;

    replay.action++;
    replay.move = moves;
    replay.msg = 0;
}

/* Forces the next action to read a diff/binary checkpoint. Done by forcing
   this action to be treated as a desync. */
void
replay_force_diff(void)
{
    replay.action--;
    replay_set_diffstate(2);
}

/* Creates a checkpoint. */
struct checkpoint *
replay_create_checkpoint(int action, long file_location, int by_desync)
{
    /* Don't create duplicate checkpoints */
    struct checkpoint *chk;
    if (replay.prev_checkpoint && replay.prev_checkpoint->action == action) {
        chk = replay.prev_checkpoint;
        mfree(&chk->binary_save);
    } else if (replay.next_checkpoint &&
               replay.next_checkpoint->action == action) {
        chk = replay.next_checkpoint;
        mfree(&chk->binary_save);
    } else {
        /* Create a new checkpoint at this location */
        chk = malloc(sizeof (struct checkpoint));
        memset(chk, 0, sizeof (struct checkpoint));
        chk->prev = replay.prev_checkpoint;
        replay.prev_checkpoint = chk;
        if (chk->prev)
            chk->prev->next = chk;
        chk->next = replay.next_checkpoint;
        if (chk->next)
            chk->next->prev = chk;
    }
    chk->action = action;
    chk->move = moves;
    chk->from_action = action;
    chk->from_move = moves;
    chk->program_state = program_state;
    chk->file_location = file_location;
    chk->by_desync = by_desync;
    if (!file_location)
        replay_save_checkpoint(chk);
    return chk;
}

static void
replay_save_checkpoint(struct checkpoint *chk)
{
    program_state.binary_save_allocated = FALSE;
    mclone(&chk->binary_save, &program_state.binary_save);
}

/* Restores the given checkpoint */
static void
replay_restore_checkpoint(struct checkpoint *chk)
{
    if (program_state.binary_save_allocated)
        mfree(&program_state.binary_save);
    program_state = chk->program_state;
    mnew(&program_state.binary_save, NULL);
    mclone(&program_state.binary_save, &chk->binary_save);
    program_state.binary_save_allocated = TRUE;
    if (!chk->file_location) {
        freedynamicdata();
        init_data(FALSE);
        startup_common(FALSE);
        dorecover(&chk->binary_save);
    } else {
        /* Clear the binary save location, to ensure we load immediately from
           the binary save location, rather than trying to replay to it. */
        program_state.binary_save_location = 0;
        log_sync(chk->file_location, TLU_BYTES, FALSE);
    }
    replay.action = chk->action;
    replay.move = chk->move;
    replay.msg = 0;

    replay.prev_checkpoint = chk;
    replay.next_checkpoint = chk->next;

    /* These are needed to completely refresh our gamestate as it should be set
       up. */
    vision_reset();
    doredraw();
    notify_levelchange(NULL);
    bot();
    flush_screen();
    update_inventory();
    if (replay.reverse)
        pline(msgc_setaction, "set");
}

void
replay_announce_desync(const char *why)
{
    /* Check for equal game ID to ensure we've initialized. */
    /* get game id */
    char game_id[BUFSZ];
    snprintf(game_id, BUFSZ, "%s_%" PRIdLEAST64 "", u.uplname,
             (int_least64_t)u.ubirthday / 1000000L);

    if (strcmp(game_id, replay.game_id) || replay.desync)
        return;

    if (why)
        raw_printf("%s", why);

    raw_printf("Some commands appears to be made on an old version."
               "  Replay will use diffs instead.");

    replay.desync = TRUE;
}

/* Add a note about this action causing a desync and create a checkpoint to
   handle it. */
static void
replay_add_desync(boolean by_interrupt)
{
    /* Check if we have an existing checkpoint caused by a desync, we might
       simply need to go further on that one. */
    int from_action = replay.action;
    int from_move = moves;
    int to_action = replay.action;
    struct checkpoint *chk = replay.prev_checkpoint;
    if (chk->action == replay.action && chk->by_desync == 2)
        to_action++;
    else
        chk = NULL;

    if (!by_interrupt)
        replay_announce_desync(NULL);

    /* Reset the binary save location. This will greatly speed up seeking of
       later parts of the game since at this point, we are at the very beginning
       program state-wise. */
    program_state.binary_save_location = 0;

    /* Make the differ do its thing. */
    log_sync(replay.move, TLU_TURNS, FALSE);
    int cur_action = replay.max - replay_count_actions(FALSE);
    while (cur_action <= to_action) {
        if (by_interrupt && cur_action == to_action)
            break;

        while (cur_action++ <= to_action)
            log_sync(0, TLU_NEXT, FALSE);

        cur_action = replay.max - replay_count_actions(FALSE);
    }

    replay.action = cur_action;
    replay.msg = 0;
    if (!chk) {
        chk = replay_create_checkpoint(replay.action,
                                       program_state.end_of_gamestate_location,
                                       2);
        chk->from_action = from_action;
        chk->from_move = from_move;
    } else {
        chk->action = replay.action;
        chk->move = moves;
        mfree(&chk->binary_save);
        program_state.binary_save_allocated = FALSE;
        mclone(&chk->binary_save, &program_state.binary_save);
    }
}

static void
replay_maybe_create_diffstate(void)
{
    char *new_diff;
    if (!replay.diff_allocated || replay.max != replay.max_old) {
        new_diff = malloc(replay.max + 1);
        memset(new_diff, 0, replay.max + 1);
        if (replay.diff_allocated) {
            /* already allocated, copy over the existing data */
            int i;
            for (i = 0; i <= replay.max && i <= replay.max_old; i++)
                new_diff[i] = replay.diff_ok[i];

            free(replay.diff_ok);
        }

        replay.diff_ok = new_diff;
        replay.max_old = replay.max;
        replay.diff_allocated = TRUE;
    }
}

/* Returns TRUE if we want to ignore diffs. We want to do that unless we have
   desynced. */
boolean
replay_ignore_diff(void)
{
    if (program_state.followmode != FM_REPLAY)
        return FALSE;

    replay_maybe_create_diffstate();

    if (!replay.desync)
        return TRUE;

    if (replay.diff_ok[replay.action] == 1)
        return TRUE;

    return FALSE;
}

void
replay_set_diffstate(int res)
{
    if (program_state.followmode != FM_REPLAY)
        return;

    /* If we're marking a desync (res == 1), make older actions inherit this
       until we reach our previous checkpoint. */
    if (res == 2) {
        int i;
        int prev_checkpoint = 0;
        if (replay.prev_checkpoint)
            prev_checkpoint = replay.prev_checkpoint->action;
        for (i = replay.action; i > prev_checkpoint; i--)
            replay.diff_ok[i] = res;
    }

    replay_maybe_create_diffstate();
    replay.diff_ok[replay.action] = res;
}

int
replay_get_diffstate(void)
{
    if (program_state.followmode != FM_REPLAY)
        return 0;

    replay_maybe_create_diffstate();
    return replay.diff_ok[replay.action];
}

/* Go N turns forward/backwards in move/actions */
void
replay_seek(int target, boolean target_is_move)
{
    /* ensure that target holds the correct thing we can offset */
    replay.jumped = FALSE;
    replay.target = (target_is_move ? moves : replay.action);
    replay.target += target;
    if (replay.target < 1)
        replay.target = 1;
    replay.target_is_move = target_is_move;
}

/* Go to move/action N. Target being 0 means "go to the end" */
void
replay_goto(int target, boolean target_is_move)
{
    replay.jumped = FALSE;
    if (!target) {
        replay.target_is_move = FALSE;
        replay.target = replay.max;
        return;
    }

    replay.target = target;
    if (replay.target < 1)
        replay.target = 1;
    replay.target_is_move = target_is_move;
}

/* Returns current action */
int
replay_action(void)
{
    return replay.action;
}

/* Returns total actions */
int
replay_max(void)
{
    return replay.max;
}

/* Returns the upcoming command. */
char *
replay_cmd(void)
{
    return replay.cmd;
}

boolean
replay_desynced(void)
{
    return replay.desync;
}
