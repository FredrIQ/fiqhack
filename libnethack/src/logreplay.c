/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-29 */
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
    struct checkpoint *next;
    struct checkpoint *prev;
    struct sinfo program_state;
    int action;
    int move;
    int submove;
    struct memfile binary_save;
};

struct replayinfo {
    int action; /* current action */
    int max; /* last action */
    int target; /* target action or move */
    boolean target_is_move; /* whether target is by action or by move */
    boolean replaying; /* if we are using the replay "windowport" */
    int move; /* current turncount (stored to make move_action accurate) */
    int submove; /* action during this move (starting from 0) */
    struct checkpoint *checkpoints; /* checkpoint list */
    struct checkpoint *revcheckpoints; /* last checkpoint */
    char *game_id;

    /* Replay loading may not be near-instantaneous. Avoid queueing up
       several commands just beecaus replay reload took a bit of time by
       swallowing commands briefly. */
    microseconds last_load;
#define LOAD_DELAY 10000
};

static struct replayinfo replay = {0};

static void replay_pause(enum nh_pause_reason);
static void replay_display_buffer(const char *, boolean);
static void replay_update_status(struct nh_player_info *);
static void replay_print_message(int, enum msg_channel, const char *);
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
static void replay_format(const char *, int, int, void *, void (*)(const char *, void *));
static void replay_no_op_void(void);
static void replay_no_op_int(int);
static void replay_outrip(struct nh_menulist *, boolean, const char *, int,
                          const char *, int, int);
static void replay_set_windowport(void);
static void replay_panic(const char *);
static void replay_create_checkpoint(void);
static int replay_load_checkpoint(int, boolean);
static void replay_save_checkpoint(struct checkpoint *);
static void replay_restore_checkpoint(struct checkpoint *);

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
replay_print_message(int turn, enum msg_channel msgc, const char *message)
{
    /* TODO */
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
    orig_winprocs.win_list_items(ml, invent);
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

    /* This means something went wrong. Force-revert the windowport. */
    replay_reset_windowport();
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

/* Resets back to the original windowport and refreshes the screen */
void
replay_reset_windowport(void)
{
    if (!replay.replaying)
        return;

    replay.replaying = FALSE;
    if (orig_winprocs.win_request_command)
        windowprocs = orig_winprocs;

    vision_reset();
    doredraw();
    notify_levelchange(NULL);
    bot();
    flush_screen();
    update_inventory();
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
    replay_reset_windowport();

    /* are we even in replaymode? */
    if (program_state.followmode != FM_REPLAY)
        return;

    /* get game id */
    const char *game_id = msgprintf("%s_%" PRIdLEAST64 "", u.uplname,
                                    (int_least64_t)u.ubirthday / 1000000L);

    /* if it's the same, reuse the replay state but force target to be
       right before our current point unless it's at the beginning */
    if (replay.game_id && !strcmp(game_id, replay.game_id)) {
        replay.target = replay.action - 1;
        if (replay.target) {
            replay.target_is_move = FALSE;
            return;
        }
    }

    if (replay.game_id)
        free(replay.game_id);

    /* deallocate checkpoints */
    struct checkpoint *chk, *chknext;
    for (chk = replay.checkpoints; chk; chk = chknext) {
        chknext = chk->next;
        mfree(&chk->binary_save);
        free(chk);
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

/* Returns TRUE if we are in replaymode and want user input */
boolean
replay_want_userinput(void)
{
    if (program_state.followmode != FM_REPLAY)
        return FALSE;

    /* maybe we want to load a checkpoint */
    replay_set_windowport();

    boolean just_reloaded = FALSE;
    if (replay.target_is_move ?
        ((replay.target < replay.move) ||
         (replay.target == replay.move && replay.submove)) :
        (replay.target < replay.action)) {
        replay_load_checkpoint(replay.target, replay.target_is_move);
        just_reloaded = TRUE;
    }

    /* check if we reached the end */
    if (replay.action == replay.max) {
        replay.target = replay.action;
        replay.target_is_move = FALSE;
    }

    if (replay.target_is_move ?
        (replay.target == replay.move && !replay.submove) :
        (replay.target == replay.action)) {
        replay_reset_windowport();
        return TRUE;
    }

    if (just_reloaded)
        return FALSE;

    if (moves != replay.move) {
        replay.move = moves;
        replay.submove = 0;
    } else
        replay.submove++;
    replay.action++;

    /* if this action is past a certain point, or if none exist, create a
       checkpoint */
    if (!replay.revcheckpoints ||
        (replay.revcheckpoints->action + CHECKPOINT_FREQ <= replay.action &&
         !check_turnstate_move(FALSE)))
        replay_create_checkpoint();

    return FALSE;
}

/* Creates a checkpoint. action tells what action this checkpoint is for */
static void
replay_create_checkpoint(void)
{
    /* Check if one exists already at or beyond this point */
    struct checkpoint *chk = replay.revcheckpoints;
    if (chk && chk->action >= replay.action)
        return;

    /* Create a new checkpoint at this location */
    struct checkpoint *nchk = malloc(sizeof (struct checkpoint));
    memset(nchk, 0, sizeof (struct checkpoint));
    if (chk) {
        chk->next = nchk;
        nchk->prev = chk;
    } else
        replay.checkpoints = nchk;

    replay.revcheckpoints = nchk;

    replay_save_checkpoint(nchk);
}

static void
replay_save_checkpoint(struct checkpoint *chk)
{
    chk->action = replay.action;
    chk->move = replay.move;
    chk->submove = replay.submove;
    chk->program_state = program_state;
    savegame(&chk->binary_save);
}

/* Loads the checkpoint closest before given action.
   Returns the action of the checpoint. */
static int
replay_load_checkpoint(int target, boolean target_is_move)
{
    struct checkpoint *chk;
    for (chk = replay.revcheckpoints; chk; chk = chk->prev) {
        if ((target_is_move) ?
            (chk->move < target ||
             (chk->move == target && !chk->submove)) :
            (chk->action <= target))
            break;
    }

    if (!chk)
        panic("Failed to find a checkpoint to restore");

    replay_restore_checkpoint(chk);
}

static void
replay_restore_checkpoint(struct checkpoint *chk)
{
    struct memfile old_ps_binary = program_state.binary_save;
    program_state = chk->program_state;
    program_state.binary_save = old_ps_binary;
    freedynamicdata();
    init_data(FALSE);
    startup_common(FALSE);
    dorecover(&chk->binary_save);
    replay.action = chk->action;
    replay.move = chk->move;
    replay.submove = chk->submove;
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
