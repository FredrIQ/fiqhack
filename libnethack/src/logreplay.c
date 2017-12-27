/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-27 */
/* Copyright (c) Fredrik Ljungdahl, 2017. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static struct nh_window_procs orig_winprocs = {0};

/* Replaymode handling + replaymode windowport stuff */

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
static void replay_panic(const char *);

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

struct checkpoint {
    struct checkpoint *next;
    struct sinfo program_state;
    int action;
    struct memfile binary_save;
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
replay_list_items(struct nh_objlist *ml, boolean unused)
{
    dealloc_objmenulist(ml);
    (void) unused;
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
    /* Show the print first */
    orig_winprocs.win_raw_print(message);
    replay_panic("replay_raw_print");
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
void
replay_set_windowport(void)
{
    if (program_state.replaying)
        return;

    program_state.replaying = TRUE;
    orig_winprocs = windowprocs;
    windowprocs = replay_windowprocs;
}

/* Resets back to the original windowport and refreshes the screen */
void
replay_reset_windowport(void)
{
    if (!program_state.replaying)
        return;

    program_state.replaying = FALSE;
    if (orig_winprocs.win_request_command)
        windowprocs = orig_winprocs;

    vision_reset();
    doredraw();
    notify_levelchange(NULL);
    bot();
    flush_screen();
    update_inventory();
}

/* Resets the windowport and panics. Used if the engine requested a command that
   should never be requested in replaymode. */
static void
replay_panic(const char *str)
{
    replay_reset_windowport();
    panic("Replaymode requested invalid command: %s", str);
}

/* Creates a checkpoint. action tells what action this checkpoint is for */
void
replay_create_checkpoint(int action)
{
    /* Check if one exists already at or beyond this point */
    struct checkpoint *chk, *lchk;
    lchk = NULL;
    for (chk = checkpoints; chk; chk = chk->next) {
        if (chk->action >= action)
            return;

        lchk = chk;
    }

    /* Create a new checkpoint at this location */
    chk = malloc(sizeof (struct checkpoint));
    memset(chk, 0, sizeof (struct checkpoint));
    if (lchk)
        lchk->next = chk;
    else
        checkpoints = chk;

    chk->action = action;
    chk->program_state = program_state;
    savegame(&chk->binary_save);
}

/* Loads the checkpoint closest before given action.
   Returns the action of the checpoint. */
int
replay_load_checkpoint(int action)
{
    struct checkpoint *chk, *lchk;
    lchk = NULL;
    for (chk = checkpoints; chk; chk = chk->next) {
        if (chk->action > action)
            break;

        lchk = chk;
    }

    if (!lchk)
        panic("Failed to find a checkpoint to restore");

    struct memfile old_ps_binary = program_state.binary_save;
    program_state = lchk->program_state;
    program_state.binary_save = old_ps_binary;
    freedynamicdata();
    init_data(FALSE);
    startup_common(FALSE);
    dorecover(&lchk->binary_save);
    return lchk->action;
}
