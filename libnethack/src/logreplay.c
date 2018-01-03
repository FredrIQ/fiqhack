/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-01-03 */
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
    struct sinfo program_state;
    int action;
    int move;
    int from_action;
    int from_move;
    struct memfile binary_save;
    boolean by_desync;
};

struct replayinfo {
    int action; /* current action */
    int msg; /* msg within action -- to allow proper message history */
    int max; /* last action */
    int target; /* target action or move */
    boolean target_is_move; /* whether target is by action or by move */
    boolean replaying; /* if we are using the replay "windowport" */
    int move; /* current turncount (stored to make move_action accurate) */
    struct checkpoint *prev_checkpoint;
    struct checkpoint *next_checkpoint;
    char *game_id; /* unique for each game */

    /* Replay loading may not be near-instantaneous. Avoid queueing up
       several commands just beecaus replay reload took a bit of time by
       swallowing commands briefly. */
    microseconds last_load;
#define LOAD_DELAY 10000

    /* Desync management. If at any point we encounter a desync, we invoke the
       normal save differ to load the closest following action and then store it
       in a list with a corresponding checkpoint (which is added as part of the
       usual checkpoint list). */
    int desyncs;
    boolean in_load; /* Used to check for a desync */

    /* Mark that we have already jumped backwards once. This is needed to avoid
       a loop by a move we are seeking to being skipped. */
    boolean jumped;
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
static struct checkpoint *replay_create_checkpoint(void);
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

static struct checkpoint *checkpoints = NULL;

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
    if (!replay.desyncs)
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
    replay_reset_windowport(FALSE);

    /* are we even in replaymode? */
    if (program_state.followmode != FM_REPLAY)
        return;

    /* get game id */
    const char *game_id = msgprintf("%s_%" PRIdLEAST64 "", u.uplname,
                                    (int_least64_t)u.ubirthday / 1000000L);

    /* if it's the same, reuse the replay state but force target to be
       right before our current point unless it's at the beginning */
    if (replay.game_id && !strcmp(game_id, replay.game_id)) {
        if (replay.in_load) {
            replay.action--;
            replay.msg = 0;
            replay_add_desync(FALSE);
        }

        /* reload from a checkpoint */
        if (replay.prev_checkpoint)
            replay_restore_checkpoint(replay.prev_checkpoint);
        return;
    }

    /* if game id is non-empty, we have existing stuff, free it */
    if (replay.game_id) {
        free(replay.game_id);

        struct checkpoint *chk, *chknext;
        for (chk = replay.next_checkpoint; chk; chk = chknext) {
            chknext = chk->next;
            mfree(&chk->binary_save);
            free(chk);
        }
        for (chk = replay.prev_checkpoint; chk; chk = chknext) {
            chknext = chk->next;
            mfree(&chk->binary_save);
            free(chk);
        }
    }

    /* clear the replay struct, we don't have anything else to do with it */
    memset(&replay, 0, sizeof (struct replayinfo));

    replay.max = replay_count_actions();

    /* set game id */
    replay.game_id = malloc(strlen(game_id) + 1);
    strcpy(replay.game_id, game_id);

    if (!replay.max) {
        /* user requested instant replay, go back to the beginning and playback
           here */
        log_sync(1, TLU_TURNS, FALSE);
        replay.max = replay_count_actions();
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
    struct checkpoint *chk = NULL;
    if (replay.target <
        (replay.target_is_move ? replay.move : replay.action)) {

        /* Iterate until we have the checkpoint we want to restore. We may
           modify target in case a checkpoint covers our target. */
        chk = replay.prev_checkpoint;
        while (replay.target <
               (replay.target_is_move ? chk->from_move : chk->from_action)) {
            if (replay.target >
                (replay.target_is_move ? chk->move : chk->action)) {
                /* This checkpoint covers our target. Set target to just at
                   this checkpoint's "from". */
                replay.target =
                    (replay.target_is_move ? chk->from_move : chk->from_action);
            }

            chk = chk->prev;
            if (!chk) {
                raw_print("Replay checkpoint load error: went too far.");
                replay_goal_reached();
                return TRUE;
            }
        }

        replay_restore_checkpoint(chk);

        /* Don't jump more than once. */
        if (replay.jumped) {
            replay_goal_reached();
            return TRUE;
        }
        replay.jumped = TRUE;
    }

    /* check if we reached the end */
    if (replay.action == replay.max) {
        replay_goal_reached();
        return TRUE;
    }

    if (replay.target ==
        (replay.target_is_move ? replay.move : replay.action)) {
        replay_goal_reached();
        return TRUE;
    }

    /* Possibly load from our next checkpoint */
    chk = replay.next_checkpoint;
    if (chk &&
        (chk->from_action <= replay.action ||
         replay.target >
         (replay.target_is_move ? chk->from_move : chk->from_action))) {
        while (chk->next && replay.target >
               (replay.target_is_move ? chk->from_move : chk->from_action))
            chk = chk->next;

        replay_restore_checkpoint(chk);

        /* we might have overshot our target as a result */
        if (replay.target_is_move ?
            (replay.target <= replay.move) :
            (replay.target <= replay.action)) {
            replay_goal_reached();
            return TRUE;
        }
    }

    /* if this action is past a certain point, or if none exist, create a
       checkpoint */
    if (!replay.next_checkpoint &&
        (!flags.incomplete || flags.interrupted) &&
        !u_helpless(hm_all) &&
        (!replay.prev_checkpoint ||
         replay.prev_checkpoint->action + CHECKPOINT_FREQ <= replay.action))
        replay_create_checkpoint();

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
        cur_location = replay.move;
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

/* Updates replay.move/replay.action for a new action. */
void
replay_set_action(void)
{
    if (program_state.followmode != FM_REPLAY)
        return;

    replay.move = moves;
    replay.action++;
    replay.msg = 0;
}

/* Forces the next action to read a diff/binary checkpoint. Done by forcing
   this action to be treated as a desync. */
void
replay_force_diff(void)
{
    if (program_state.followmode == FM_REPLAY)
        replay_add_desync(TRUE);
}

/* Creates a checkpoint. */
static struct checkpoint *
replay_create_checkpoint(void)
{
    /* Create a new checkpoint at this location */
    struct checkpoint *chk = malloc(sizeof (struct checkpoint));
    memset(chk, 0, sizeof (struct checkpoint));
    chk->prev = replay.prev_checkpoint;
    replay.prev_checkpoint = chk;
    chk->next = replay.next_checkpoint;
    replay_save_checkpoint(chk);
    return chk;
}

static void
replay_save_checkpoint(struct checkpoint *chk)
{
    chk->action = replay.action;
    chk->move = replay.move;
    chk->from_action = replay.action;
    chk->from_move = replay.move;
    chk->program_state = program_state;
    savegame(&chk->binary_save);
}

/* Restores the given checkpoint */
static void
replay_restore_checkpoint(struct checkpoint *chk)
{
    struct memfile old_ps_binary = program_state.binary_save;
    boolean old_ps_allocation = program_state.binary_save_allocated;
    program_state = chk->program_state;
    program_state.binary_save = old_ps_binary;
    program_state.binary_save_allocated = old_ps_allocation;
    freedynamicdata();
    init_data(FALSE);
    startup_common(FALSE);
    dorecover(&chk->binary_save);
    replay.action = chk->action;
    replay.msg = 0;
    replay.move = chk->move;

    replay.prev_checkpoint = chk;
    replay.next_checkpoint = chk->next;
    vision_reset();
}

/* Add a note about this action causing a desync and create a checkpoint to
   handle it. */
static void
replay_add_desync(boolean by_interrupt)
{
    /* Check if we have an existing checkpoint caused by a desync, we might
       simply need to go further on that one. */
    int from_action = replay.action;
    int from_move = replay.move;
    int to_action = replay.action;
    struct checkpoint *chk = replay.prev_checkpoint;
    if (chk->action == replay.action && chk->by_desync)
        to_action++;
    else
        chk = NULL;

    if (!by_interrupt) {
        if (!replay.desyncs)
            raw_printf("Some commands appears to be made on an obsolete engine.  "
                       "These will use diffs instead.  "
                       "Some actions may be skipped.");

        replay.desyncs++;
    }

    /* Reset the binary save location. This will greatly speed up seeking of
       later parts of the game since at this point, we are at the very beginning
       program state-wise. */
    program_state.binary_save_location = 0;

    /* Make the differ do its thing. */
    log_sync(replay.move, TLU_TURNS, FALSE);
    int cur_action = replay.max - replay_count_actions();
    while (cur_action <= to_action) {
        if (by_interrupt && cur_action == to_action)
            break;

        while (cur_action++ <= to_action)
            log_sync(0, TLU_NEXT, FALSE);

        cur_action = replay.max - replay_count_actions();
    }

    replay.action = cur_action;
    replay.msg = 0;
    if (!chk) {
        chk = replay_create_checkpoint();
        chk->from_action = from_action;
        chk->from_move = from_move;
        chk->by_desync = TRUE;
    } else {
        chk->action = replay.action;
        chk->move = replay.move;
        mfree(&chk->binary_save);
        mnew(&chk->binary_save, NULL);
        savegame(&chk->binary_save);
    }
}

/* Go N turns forward/backwards in move/actions */
void
replay_seek(int target, boolean target_is_move)
{
    /* ensure that target holds the correct thing we can offset */
    replay.target = (target_is_move ? replay.move : replay.action);
    replay.target += target;
    if (replay.target < 1)
        replay.target = 1;
    replay.target_is_move = target_is_move;
    replay.jumped = FALSE;
}

/* Go to move/action N. Target being 0 means "go to the end" */
void
replay_goto(int target, boolean target_is_move)
{
    if (!target) {
        replay.target_is_move = FALSE;
        replay.target = replay.max;
        return;
    }

    replay.target = target;
    if (replay.target < 1)
        replay.target = 1;
    replay.target_is_move = target_is_move;
    replay.jumped = FALSE;
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
