/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-21 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* various code that was replicated in *main.c */

#include "hack.h"
#include "hungerstatus.h"
#include "common_options.h"

#include <sys/stat.h>
#include <limits.h>

#include "dlb.h"
#include "patchlevel.h"

static const char *const copyright_banner[] =
    { COPYRIGHT_BANNER_A, COPYRIGHT_BANNER_B, COPYRIGHT_BANNER_C, NULL };

static void pre_move_tasks(boolean, boolean);

static void newgame(microseconds birthday);

static void handle_lava_trap(boolean didmove);

static void command_input(int cmdidx, struct nh_cmd_arg *arg);

static void decrement_helplessness(void);

const char *const *
nh_get_copyright_banner(void)
{
    return copyright_banner;
}

void
nh_lib_init(const struct nh_window_procs *procs, const char *const *paths)
{
    int i;

    DEBUG_LOG("Initializing NetHack engine...\n");

    API_ENTRY_CHECKPOINT_RETURN_VOID_ON_ERROR();
    windowprocs = *procs;

    for (i = 0; i < PREFIX_COUNT; i++)
        fqn_prefix[i] = strdup(paths[i]);

    u.uhp = 1;  /* prevent RIP on early quits */

#ifdef AIMAKE_BUILDOS_linux
    /* SIGRTMIN+{1,2} are used by the lock monitoring code. This means that we
       could end up with spurious signals due to race conditions after a game
       exits or when reading save files in the main menu of the client. In such
       cases, we just ignore the signals; we're not doing any lock monitoring
       anyway, so this is safe unless the process is stuck (in which case,
       things are broken anyway). */
    signal(SIGRTMIN+1, SIG_IGN);
    signal(SIGRTMIN+2, SIG_IGN);
#endif

    API_EXIT();
}


void
nh_lib_exit(void)
{
    int i;

    DEBUG_LOG("Exiting NetHack engine...\n");

    xmalloc_cleanup(&api_blocklist);

    for (i = 0; i < PREFIX_COUNT; i++) {
        free(fqn_prefix[i]);
        fqn_prefix[i] = NULL;
    }
}


boolean
nh_exit_game(int exit_type)
{
    volatile int etype = exit_type;

    /* This routine always throws an exception, and normally doesn't return (it
       lets nh_play_game do the returning). It will, however, have to return
       itself if the game isn't running. In such a case, all options give a
       success return; saving/exiting the game is easy (but a no-op) if it isn't
       running.
    */
    API_ENTRY_CHECKPOINT_RETURN_ON_ERROR(TRUE);

    xmalloc_cleanup(&api_blocklist);

    if (program_state.game_running) {

        if (program_state.followmode != FM_PLAY && etype != EXIT_RESTART)
            etype = EXIT_SAVE; /* no quitting other players' games */

        /* Note: All these "break"s are unreachable. */
        switch (etype) {
        case EXIT_SAVE:
            if (program_state.followmode == FM_PLAY)
                log_game_state();

            terminate(GAME_DETACHED);
            break;

        case EXIT_RESTART:
            /* This is no more abusable than EXIT_SAVE (hopefully neither is
               abusable at all). */
            terminate(CLIENT_RESTART);
            break;

        case EXIT_QUIT:
            done_noreturn(QUIT, NULL);
            break;

        case EXIT_PANIC:
            /* We can't/shouldn't abort the turn just because the client claimed
               to malfunction; that's exploitable. We can safely log the
               failure, though; and we can safely detach, because clients can do
               that anyway. Perhaps we should add some method of specifying a
               panic message. */
            paniclog("ui_problem", "Unspecified UI problem.");
            terminate(GAME_DETACHED);
            break;
        }

        API_EXIT();

        return FALSE;

    } else {
        /* Calling terminate() will get us out of nested contexts safely. I'm
           not sure if this can happen with no game running, but it doesn't hurt
           to code for the possibility it might (via jumping back to the
           checkpoint with terminate()). */
        terminate(GAME_ALREADY_OVER); /* doesn't return */
    }
}

/* note: must not contain any RNG calls */
void
startup_common(boolean including_program_state)
{
    reset_encumber_msg();

    /* create mutable copies of object and artifact liss */
    init_objlist();
    init_artilist();
    reset_rndmonst(NON_PM);
    free_dungeon();     /* clean up stray dungeon data */

    initoptions();

    dlb_init(); /* must be before newgame() */

    /*
     *  Initialize the vision system.  This must be before mklev() on a
     *  new game or before a level restore on a saved game.
     */

    if (including_program_state) {
        vision_init();
        cls();
    }

    initrack();
}


static void
realtime_messages(boolean moon, boolean fri13)
{
    if (moon) {
        switch (flags.moonphase) {
        case FULL_MOON:
            pline("You are lucky!  Full moon tonight.");
            break;
        case NEW_MOON:
            pline("Be careful!  New moon tonight.");
            break;
        default:
            /* special moonphase time period ended */
            pline("The moon seems less notable tonight...");
            break;
        }
    }

    if (fri13 && flags.friday13)
        pline("Watch out!  Bad things can happen on Friday the 13th.");
}

/* Called at the end of each command to synchronize the game to realtime.  (This
   is based on flags.turntime, which was updated during the running of the
   command.)  The messages are always printed if we just ran a 'welcome'
   command; otherwise, they are printed only if they changed. (This makes the
   'welcome' command unexploitable; /omitting/ a welcome is slightly exploitable
   in that it can be used to delay the effect of a change in realtime for one
   command, but fixing that makes the time behaviour so much more complex that
   it probably isn't worth it.) */
static void
realtime_tasks(boolean always_print, boolean never_print)
{
    int prev_moonphase = flags.moonphase;
    int prev_friday13 = flags.friday13;
    boolean msg_moonphase = TRUE;
    boolean msg_friday13 = TRUE;

    flags.moonphase = phase_of_the_moon();
    if (flags.moonphase == FULL_MOON && prev_moonphase != FULL_MOON) {
        change_luck(1);
    } else if (flags.moonphase != FULL_MOON && prev_moonphase == FULL_MOON) {
        change_luck(-1);
    } else if ((flags.moonphase == NEW_MOON && prev_moonphase != NEW_MOON) ||
               (flags.moonphase != NEW_MOON && prev_moonphase == NEW_MOON)) {
        /* Do nothing, but show message. */
    } else {
        msg_moonphase = FALSE;
    }

    flags.friday13 = friday_13th();
    if (flags.friday13 && !prev_friday13) {
        change_luck(-1);
    } else if (!flags.friday13 && prev_friday13) {
        change_luck(1);
    } else {
        msg_friday13 = FALSE;
    }

    if (!never_print)
        realtime_messages(msg_moonphase || (always_print &&
                                            (flags.moonphase == NEW_MOON ||
                                             flags.moonphase == FULL_MOON)),
                          msg_friday13 || always_print);
}


static void
post_init_tasks(void)
{
    /* dorecover() used to run timers. The side effects from dorecover() have
       been moved here (so that dorecover() can be used to test save file
       integrity); but the timer run has been removed altogether (the /correct/
       place for it is goto_level(), and there was already a call there).

       After some effort eliminating them, there are no dorecover() side effects
       here at all right now, and that's the way it should be; saving then
       reloading should have no effect. */

    /* Prepare for the first move. */
    pre_move_tasks(FALSE, TRUE);
}


enum nh_create_response
nh_create_game(int fd, struct nh_option_desc *opts_orig)
{
    microseconds birthday;
    int i;
    volatile int log_inited = 0;
    struct nh_option_desc *volatile opts = opts_orig;

    API_ENTRY_CHECKPOINT() {
        IF_API_EXCEPTION(GAME_CREATED):
            if (log_inited)
                log_uninit();
            return NHCREATE_OK;

        IF_ANY_API_EXCEPTION():
            if (log_inited)
                log_uninit();
            return NHCREATE_FAIL;
    }

    if (fd == -1) {
        API_EXIT();
        return NHCREATE_INVALID;
    }

    program_state.suppress_screen_updates = TRUE;
    program_state.followmode = FM_PLAY;

    birthday = utc_time();

    /* Initalize all global data. (This also wipes the RNG seed, so needs to
       be called before seed_rng_from_entropy.) */
    init_data(TRUE);
    startup_common(TRUE);

    /* Set defaults in case list of options from client was incomplete. */
    struct nh_option_desc *defaults = default_options();
    for (i = 0; defaults[i].name; i++)
        nh_set_option(defaults[i].name, defaults[i].value);
    nhlib_free_optlist(defaults);
    for (i = 0; opts[i].name; i++)
        nh_set_option(opts[i].name, opts[i].value);

    /* Initialize the random number generator. This can use any algorithm we
       like, and is not constrained by timing rules, so we use the entropy
       collectors in newrng.c, unless the user set a seed. This must be called
       after options setup because we won't otherwise know if we're playing with
       a set seed. */
    if (*flags.setseed) {
        if (!seed_rng_from_base64(flags.setseed)) {
            API_EXIT();
            return NHCREATE_INVALID;
        }
    } else
        seed_rng_from_entropy();

    if (wizard)
        strcpy(u.uplname, "wizard");
    if (!*u.uplname && discover)
        strcpy(u.uplname, "explorer");

    if (!validrole(u.initrole) || !validrace(u.initrole, u.initrace) ||
        !validgend(u.initrole, u.initrace, u.initgend) ||
        !validalign(u.initrole, u.initrace, u.initalign) ||
        (!*u.uplname && !wizard)) {
        API_EXIT();
        return NHCREATE_INVALID;
    }

    /* We create a new save file that saves the state immediately after
       newgame() is called. */
    log_init(fd);
    log_inited = 1;
    log_newgame(birthday);

    newgame(birthday);

    /* Handle the polyinit option. */
    if (flags.polyinit_mnum != -1) {
        int light_range;

        polymon(flags.polyinit_mnum, FALSE);

        /* polymon() doesn't handle light sources. Do that here. */

        light_range = emits_light(youmonst.data);

        if (light_range == 1)
            ++light_range; /* matches polyself() handling */

        if (light_range)
            new_light_source(level, u.ux, u.uy, light_range, LS_MONSTER,
                             &youmonst);
    }

    /* We need a full backup save after creating the new game, because we
       don't have anything to diff against. */
    log_backup_save();

    program_state.suppress_screen_updates = FALSE;

    terminate(GAME_CREATED);
}

enum nh_play_status
nh_play_game(int fd, enum nh_followmode followmode)
{
    volatile int ret;
    volatile int replay_forced = FALSE;
    volatile int file_done = FALSE;

    if (fd < 0)
        return ERR_BAD_ARGS;

    switch (nh_get_savegame_status(fd, NULL)) {
    case LS_INVALID:
        return ERR_BAD_FILE;
    case LS_DONE:
        if (followmode != FM_REPLAY && followmode != FM_RECOVERQUIT) {
            replay_forced = TRUE;
            followmode = FM_REPLAY; /* force into replay mode */
            file_done = TRUE;
        }
        break;
    case LS_CRASHED:
        return ERR_RESTORE_FAILED;
    case LS_IN_PROGRESS:
        return ERR_IN_PROGRESS;
    case LS_SAVED:
        break;  /* default, everything is A-OK */
    }

    program_state.followmode = followmode;

    /* setjmp cannot portably do anything with its return value but a series of
       comparisons; even assigning it to a variable directly doesn't
       work. Instead, we enumerate all the possible values, which works even on
       C implementations that can't generate a temporary to do an assignment
       inside a setjmp, and will probably be optimized by compilers that can. */
    API_ENTRY_CHECKPOINT() {
        /* Normal termination statuses. */
    IF_API_EXCEPTION(GAME_DETACHED):
        ret = GAME_DETACHED;
        goto normal_exit;
    IF_API_EXCEPTION(GAME_OVER):
        ret = GAME_OVER;
        goto normal_exit;
    IF_API_EXCEPTION(GAME_ALREADY_OVER):
        ret = GAME_ALREADY_OVER;
        goto normal_exit;
    IF_API_EXCEPTION(REPLAY_FINISHED):
        ret = REPLAY_FINISHED;
        goto normal_exit;

        /* This happens if the game needs to escape from a deeply nested
           context. The longjmp() is not enough by itself in case the network
           API is involved (returning from nh_play_game out of sequence causes
           the longjmp() to propagate across the network). */
    IF_API_EXCEPTION(RESTART_PLAY):
        ret = RESTART_PLAY;
        goto normal_exit;
    IF_API_EXCEPTION(CLIENT_RESTART):
        ret = CLIENT_RESTART;
        goto normal_exit;

        /* Errors while loading. These still use the normal_exit codepath. */
    IF_API_EXCEPTION(ERR_BAD_ARGS):
        ret = ERR_BAD_ARGS;
        goto normal_exit;
    IF_API_EXCEPTION(ERR_BAD_FILE):
        ret = ERR_BAD_FILE;
        goto normal_exit;
    IF_API_EXCEPTION(ERR_IN_PROGRESS):
        ret = ERR_IN_PROGRESS;
        goto normal_exit;
    IF_API_EXCEPTION(ERR_RESTORE_FAILED):
        ret = ERR_RESTORE_FAILED;
        goto normal_exit;
    IF_API_EXCEPTION(ERR_RECOVER_REFUSED):
        ret = ERR_RECOVER_REFUSED;
        goto normal_exit;

        /* Catchall. This should never happen. We can't call panic() because the
           exit_jmp_buf is no longer valid, also if the state is this badly
           screwed up we probably can't rely on it working. So we just log a
           failure and exit the program. */
    IF_ANY_API_EXCEPTION():
        paniclog("panic", "impossible program termination");
        pline("Internal error: program terminated in an impossible way.");
        ret = GAME_DETACHED;
        goto normal_exit;
    }

    init_data(TRUE);
    startup_common(TRUE);

    /* Load the save file. log_sync() needs to be called at least once because
       we no longer try to rerun the new game sequence, and thus must start by
       loading a binary save. (In addition, using log_sync() is /much/ faster
       than attempting to replay the entire game.) */
    log_init(fd);

    /* If we're in recoverquit mode and the 'Q' to show game over is still in
       the file, undo that. */
    if (program_state.followmode == FM_RECOVERQUIT)
        log_maybe_undo_quit(); /* returns via RESTART_PLAY if there is a 'Q' */

    if (file_done)
        log_sync(1, TLU_TURNS, FALSE);
    else
        log_sync(0, TLU_EOF, FALSE);

    program_state.game_running = TRUE;
    post_init_tasks();

just_reloaded_save:
    /* While loading a save file, we don't do rendering, and we don't run
       the vision system. Do all that stuff now. */
    vision_reset();
    doredraw();
    notify_levelchange(NULL);
    bot();
    flush_screen();

    update_inventory();

    if (replay_forced) {
        pline("This game ended while you were loading it.");
        pline("Loading in replay mode instead.");
        replay_forced = FALSE;
    }

    /* The main loop. */
    while (1) {
        struct nh_cmd_and_arg cmd;
        int cmdidx;
        nh_bool command_from_user = FALSE;

        if (u_helpless(hm_all)) {
            cmd.cmd = "wait";
            cmdidx = get_command_idx("wait");
            cmd.arg.argtype = 0;
        } else {

            if (((program_state.followmode == FM_REPLAY &&
                  (!flags.incomplete || flags.interrupted)) ||
                 !log_replay_command(&cmd))) {
                if (program_state.followmode == FM_RECOVERQUIT) {
                    /* We shouldn't be here. */
                    paniclog("recoverquit",
                             "FM_RECOVERQUIT needed a command");
                    raw_print("Could not recover this game; the game over "
                              "sequence did not replay correctly.");
                    terminate(GAME_ALREADY_OVER);
                }
                (*windowprocs.win_request_command)
                    (wizard, program_state.followmode == FM_PLAY ?
                     !flags.incomplete : 1, flags.interrupted,
                     &cmd, msg_request_command_callback);
                command_from_user = TRUE;
            }

            cmdidx = get_command_idx(cmd.cmd);
        }

        if (cmdidx < 0) {
            pline("Unrecognised command '%s'", cmd.cmd);
            continue;
        }

        if (program_state.followmode != FM_PLAY && command_from_user &&
            !(cmdlist[cmdidx].flags & CMD_NOTIME)) {

            /* TODO: Add a "seek to specific turn" command */

            /* If we got a direction as part of the command, and we're
               replaying, move forwards or backwards respectively. */
            if (program_state.followmode == FM_REPLAY &&
                cmd.arg.argtype & CMD_ARG_DIR) {
                int moveswas = moves;
                switch (cmd.arg.dir) {
                case DIR_E:
                case DIR_S:
                forward_one_turn:
                    /* Move forwards one command (and thus to the next neutral
                       turnstate, because we don't ask for a command outside
                       neutral turnstate on a replay). */
                    log_replay_command(&cmd);
                    command_from_user = FALSE;
                    cmdidx = get_command_idx(cmd.cmd);
                    if (cmdidx < 0)
                        panic("Invalid command '%s' replayed from save file",
                              cmd.cmd);
                    break;
                case DIR_W:
                case DIR_N:
                    /* Move backwards one command. */
                    log_sync(program_state.binary_save_location-1,
                             TLU_BYTES, FALSE);
                    goto just_reloaded_save;
                case DIR_NW:
                    /* Move to turn 1. */
                    log_sync(1, TLU_TURNS, FALSE);
                    goto just_reloaded_save;
                case DIR_SW:
                    /* Move to the end of the replay. */
                    log_sync(0, TLU_EOF, FALSE);
                    goto just_reloaded_save;
                case DIR_NE:
                    /* Move backwards 50 turns. */
                    log_sync(moves - 50, TLU_TURNS, FALSE);
                    goto just_reloaded_save;
                case DIR_SE:
                    /* Move forwards 50 turns. */
                    log_sync(moves + 50, TLU_TURNS, FALSE);
                    if (moves == moveswas) {
                        /* If we moved forwards no more than a turn (because
                           the following turn had a >50-move action), go to
                           the move after that. */
                        goto forward_one_turn;
                    }
                    goto just_reloaded_save;
                default:
                    pline("That direction has no meaning while replaying.");
                    continue;
                }
            } else {
                /* Internal commands weren't sent by the player, so don't
                   complain about them, just ignore them. Ditto for repeat. */
                if (!(cmdlist[cmdidx].flags & CMD_INTERNAL) &&
                    cmdlist[cmdidx].func)
                    pline("Command '%s' is unavailable while %s.", cmd.cmd,
                          program_state.followmode == FM_WATCH ?
                          "watching" : "replaying");
                continue;
            }
        }

        /* Make sure the client hasn't given extra arguments on top of the ones
           that we'd normally accept. To simplify things, we just silently drop
           any additional arguments. We do this before logging so that the
           extra arguments aren't recorded in the save file. */
        cmd.arg.argtype &= cmdlist[cmdidx].flags;

        /* Proper incomplete/interrupted handling depends on knowing who sent
           the command (known here, but not elsewhere). So we save and restore
           flags.incomplete/flags.interrupted here in viewing processes that
           sent the command themselves. */

        boolean save_incomplete = flags.incomplete;
        boolean save_interrupted = flags.interrupted;

        if (cmdlist[cmdidx].flags & CMD_NOTIME &&
            !(cmdlist[cmdidx].flags & CMD_INTERNAL) &&
            program_state.followmode == FM_PLAY &&
            (flags.incomplete || !flags.interrupted || flags.occupation)) {

            /* CMD_NOTIME actions don't set last_cmd/last_arg, so we need to
               ensure we interrupt them in order to avoid screwing up command
               repeat. We accomplish this via logging an "interrupt" command.

               Exception: server cancels, which are a literal no-op; and
               commands made when playing the game, because the command repeat
               will still repeat the original command. */

            flags.incomplete = FALSE;
            flags.interrupted = FALSE; /* set to true by "interrupt" */
            flags.occupation = occ_none;

            log_record_command("interrupt",
                               &(struct nh_cmd_arg){.argtype = 0});
            log_time_line();
            command_input(get_command_idx("interrupt"),
                          &(struct nh_cmd_arg){.argtype = 0});
            neutral_turnstate_tasks();

        } else if (!(cmdlist[cmdidx].flags & CMD_NOTIME) ||
                   !(cmdlist[cmdidx].flags & CMD_INTERNAL)) {

            flags.incomplete = FALSE;
            flags.interrupted = FALSE;
            flags.occupation = occ_none;

        }

        program_state.in_zero_time_command =
            !!(cmdlist[cmdidx].flags & CMD_NOTIME);

        /* We can't record a command if we didn't prompt for one; it creates
           desyncs when replaying. (If we /did/ prompt for one, it's OK to
           record a different one, which is why the record of "interrupt" above
           is OK; it's not OK to record two, but log_record_command won't record
           zero-time commands.) */
        if (!u_helpless(hm_all)) {
            log_record_command(cmd.cmd, &(cmd.arg));
            log_time_line();
        }

        command_input(cmdidx, &(cmd.arg));

        if (command_from_user && program_state.followmode != FM_PLAY) {
            flags.incomplete = save_incomplete;
            flags.interrupted = save_interrupted;
        }

        program_state.in_zero_time_command = FALSE;

        /* Record or revert the gamestate change, depending on what happened.
           A revert should be a no-op; it'll impossible() if it isn't.  The
           flags.incomplete check is needed in case we interrupt an incomplete
           command with a server cancel; the incomplete command hasn't written a
           save file, so comparing against that save file will show a
           difference. */
        if (cmdlist[cmdidx].flags & CMD_NOTIME) {
            if (!flags.incomplete)
                log_revert_command(cmd.cmd);
        } else if ((!flags.incomplete || flags.interrupted) &&
                 !u_helpless(hm_all))
            neutral_turnstate_tasks();
        /* Note: neutral_turnstate_tasks() frees cmd (because it frees all
           messages, and we made cmd a message in our callback above), so don't
           use it past this point */
    }

normal_exit:
    program_state.game_running = FALSE;
    log_uninit();

    return ret;
}

static void
you_moved(void)
{
    int wtcap = 0, change = 0;
    boolean monscanmove = FALSE;
    enum youprop p;

    /* Note: we track which heroes/monsters can move using flags.actions and
       can_act_this_turn. When a hero or monster acts, we conceptually increase
       the value of flags.actions for that hero or monster. However, there's
       only one flags.actions value. Therefore, we need to track each
       hero/monster's offset of action count from the global flags.actions.

       At this point in the program, the hero has taken 1 more action than the
       global, and monsters have taken 0 more actions than the global. */

    do {        /* hero can't move this turn loop */
        wtcap = encumber_msg();
        calc_attr_bonus();

        flags.mon_moving = TRUE;
        do {
            /* Players have taken 1 more action than the global, monsters have
               taken 0 more actions than the global. */

            monscanmove = movemon();

            /* Now both players and monsters have taken 1 more action than the
               global... */

            flags.actions++;

            /* ...and now the global is correct. */

            if (can_act_this_turn(&youmonst))
                break;          /* it's now your turn */

            /* If we reach this point, then either monscanmove (in which case,
               the player will effectively spend an action doing nothing when we
               go back to the start of the loop, which is OK because they're out
               of actions for this turn and running them into the negatives
               won't have a visible effect); or !monscanmove, in which case the
               turn is over and we don't have to worry about flags.actions being
               incorrect because were about to overwrite it anyway. */

        } while (monscanmove);
        flags.mon_moving = FALSE;

        if (!monscanmove && !can_act_this_turn(&youmonst)) {
            /* both you and the monsters are out of steam this round */

            /**************************************/
            /* turn boundary handling starts here */
            /**************************************/

            mcalcdistress();    /* adjust monsters' trap, blind, etc */

            /* No actions have happened yet this turn. (Combined with the change
               in 'moves', this effectively gives monsters and player a new
               movement point ration by changing the inputs to
               can_act_this_turn.) */
            flags.actions = 0;

            if (flags.mon_generation &&
                !rn2(u.uevent.udemigod ? 25 :
                     (depth(&u.uz) > depth(&stronghold_level)) ? 50 : 70))
                makemon(NULL, level, COLNO, ROWNO, MM_SPECIESLEVRNG);

            int oldmoveamt = u.moveamt;

            /* Calculate how much movement you get this turn. (We cache this in
               struct you because unlike for monsters, there's a random factor
               in player movement.) */
            if (u.usteed && u.umoved) {
                /* your speed doesn't augment steed's speed */
                u.moveamt = mcalcmove(u.usteed);
            } else {
                u.moveamt = youmonst.data->mmove;

                if (Very_fast) {        /* speed boots or potion */
                    /* average movement is 1.67 times normal */
                    u.moveamt += NORMAL_SPEED / 2;
                    if (rn2(3) == 0)
                        u.moveamt += NORMAL_SPEED / 2;
                } else if (Fast) {
                    /* average movement is 1.33 times normal */
                    if (rn2(3) != 0)
                        u.moveamt += NORMAL_SPEED / 2;
                }
            }

            switch (wtcap) {
            case UNENCUMBERED:
                break;
            case SLT_ENCUMBER:
                u.moveamt -= (u.moveamt / 4);
                break;
            case MOD_ENCUMBER:
                u.moveamt -= (u.moveamt / 2);
                break;
            case HVY_ENCUMBER:
                u.moveamt -= ((u.moveamt * 3) / 4);
                break;
            case EXT_ENCUMBER:
                u.moveamt -= ((u.moveamt * 7) / 8);
                break;
            default:
                break;
            }

            /* Adjust the move offset for the change in speed. */
            adjust_move_offset(&youmonst, oldmoveamt, u.moveamt);

            /* SAVEBREAK (4.3-beta1 -> 4.3-beta2)

               If we're loading a 4.3-beta1 save, then youmonst.moveamt may have
               been set incorrectly. The game detects this by seeing whether
               youmonst.moveoffset >= 12 (which it never can be, under normal
               circumstances, but which it will be in a 4.3-beta1 save because
               you need 12 movement points for the code to reach the point where
               it runs neutral_turnstate_tasks). We're about to set
               youmonst.moveamt correctly, so we need to set youmonst.moveoffset
               correctly too to let can_act_this_turn know to use it. */
            if (youmonst.moveoffset >= 12) {
                youmonst.moveoffset = 0;
            }

            settrack();

            moves++;
            level->lastmoves = moves;

            /***************************************************/
            /* most turn boundary effects should be below here */
            /***************************************************/

            if (flags.bypasses)
                clear_bypasses();
            if (Glib)
                glibr();
            nh_timeout();
            run_regions(level);

            if (u.ublesscnt)
                u.ublesscnt--;

            /* One possible result of prayer is healing. Whether or not you get
               healed depends on your current hit points. If you are allowed to
               regenerate during the prayer, the end-of-prayer calculation
               messes up on this. Another possible result is rehumanization,
               which requires that encumbrance and movement rate be
               recalculated. */
            if (u.uinvulnerable) {
                /* for the moment at least, you're in tiptop shape */
                wtcap = UNENCUMBERED;
            } else if (Upolyd && youmonst.data->mlet == S_EEL &&
                       !is_pool(level, u.ux, u.uy) && !Is_waterlevel(&u.uz)) {
                if (u.mh > 1) {
                    u.mh--;
                } else if (u.mh < 1)
                    rehumanize(DIED, NULL);
            } else if (Upolyd && u.mh < u.mhmax) {
                if (u.mh < 1)
                    rehumanize(DIED, NULL);
                else if (Regeneration ||
                         (wtcap < MOD_ENCUMBER && !(moves % 20))) {
                    u.mh++;
                }
            } else if (u.uhp < u.uhpmax &&
                       (wtcap < MOD_ENCUMBER || !u.umoved || Regeneration)) {
                if (u.ulevel > 9 && !(moves % 3)) {
                    int heal, Con = (int)ACURR(A_CON);

                    if (Con <= 12) {
                        heal = 1;
                    } else {
                        heal = rnd(Con);
                        if (heal > u.ulevel - 9)
                            heal = u.ulevel - 9;
                    }
                    u.uhp += heal;
                    if (u.uhp > u.uhpmax)
                        u.uhp = u.uhpmax;
                } else if (Regeneration ||
                           (u.ulevel <= 9 &&
                            !(moves % ((MAXULEV + 12) / (u.ulevel + 2) + 1)))) {
                    u.uhp++;
                }
            }

            /* moving around while encumbered is hard work */
            if (wtcap > MOD_ENCUMBER && u.umoved) {
                if (!(wtcap < EXT_ENCUMBER ? moves % 30 : moves % 10)) {
                    if (Upolyd && u.mh > 1) {
                        u.mh--;
                    } else if (!Upolyd && u.uhp > 1) {
                        u.uhp--;
                    } else {
                        pline("You pass out from exertion!");
                        exercise(A_CON, FALSE);
                        helpless(10, hr_fainted, "passed out from exertion",
                                 NULL);
                    }
                }
            }

            if ((u.uen < u.uenmax) &&
                ((wtcap < MOD_ENCUMBER &&
                  (!(moves %
                     ((MAXULEV + 8 -
                       u.ulevel) * (Role_if(PM_WIZARD) ? 3 : 4) / 6))))
                 || Energy_regeneration)) {
                u.uen += rn1((int)(ACURR(A_WIS) + ACURR(A_INT)) / 15 + 1, 1);
                if (u.uen > u.uenmax)
                    u.uen = u.uenmax;
            }

            if (!u.uinvulnerable) {
                if (Teleportation && !rn2(85)) {
                    xchar old_ux = u.ux, old_uy = u.uy;

                    tele();
                    if (u.ux != old_ux || u.uy != old_uy) {
                        if (!next_to_u()) {
                            check_leash(old_ux, old_uy);
                        }
                    }
                }
                /* delayed change may not be valid anymore */
                if ((change == 1 && !Polymorph) ||
                    (change == 2 && u.ulycn == NON_PM))
                    change = 0;
                if (Polymorph && !rn2(100))
                    change = 1;
                else if (u.ulycn >= LOW_PM && !Upolyd &&
                         !rn2(80 - (20 * night())))
                    change = 2;
                if (change && !Unchanging) {
                    if (!u_helpless(hm_all)) {
                        action_interrupted();
                        if (change == 1)
                            polyself(FALSE);
                        else
                            you_were();
                        change = 0;
                    }
                }
            }

            if (Searching && !u_helpless(hm_all))
                dosearch0(1);
            dosounds();
            do_storms();
            gethungry();
            age_spells();
            exerchk();
            invault();
            if (Uhave_amulet)
                amulet();
            if (!rn2(40 + (int)(ACURR(A_DEX) * 3)))
                u_wipe_engr(rnd(3));
            if (u.uevent.udemigod && !u.uinvulnerable) {
                if (u.udg_cnt)
                    u.udg_cnt--;
                if (!u.udg_cnt) {
                    intervene();
                    u.udg_cnt = rn1(200, 50);
                }
            }
            restore_attrib();
            /* underwater and waterlevel vision are done here */
            if (Is_waterlevel(&u.uz))
                movebubbles();
            else if (Underwater)
                under_water(0);
            /* vision while buried done here */
            else if (u.uburied)
                under_ground(0);

            if (!u.umoved && (Is_waterlevel(&u.uz) ||
                              !(Flying || Levitation))) {
                if (Underwater)
                    drown();
                else if (is_lava(level, u.ux, u.uy))
                    lava_effects();
            }

            /* when immobile, count is in turns */
            decrement_helplessness();
        }

        /* flags.actions is currently correct for hero and monsters. If the hero
           gets no actions at all this turn, we go back to the start of the
           loop; flags.actions will then be too high for the hero (i.e. the hero
           will have negative actions), but because the hero can't act at all
           this turn, that's OK and will have the desired effect. */

    } while (!can_act_this_turn(&youmonst)); /* hero can't move loop */

    /******************************************/
    /* once-per-hero-took-time things go here */
    /******************************************/

    /* Lava */
    if (u.utrap && u.utraptype == TT_LAVA)
        handle_lava_trap(TRUE);

    /* Record which properties the character has ever used.

       We have three properties fields: permanently gained intrinsics; worn
       equipment; and temporary properties (os_timeout, os_circumstance,
       os_polyform). Steeds and carried artifacts count with equipment in this
       categorization; birth options count with permanently gained
       intrinsics. */
    for (p = 1; p <= LAST_PROP; p++) {
        unsigned reasons = u_have_property(p, ANY_PROPERTY, TRUE);
        if (reasons & (W_MASKABLE | W_ARTIFACT))
            u.ever_extrinsic[p / 8] |= 1 << (p % 8);
        if (reasons & (INTRINSIC | W_MASK(os_birthopt)))
            u.ever_intrinsic[p / 8] |= 1 << (p % 8);
        if (reasons & (W_MASK(os_timeout) | W_MASK(os_circumstance) |
                       W_MASK(os_polyform)))
            u.ever_temporary[p / 8] |= 1 << (p % 8);
    }

    /* We can only port to a new version of the save code when the player takes
       time (otherwise, the desync detector rightly gets confused). */
    flags.save_encoding = saveenc_moverel;
}


static void
handle_lava_trap(boolean didmove)
{
    if (!is_lava(level, u.ux, u.uy))
        u.utrap = 0;
    else if (!u.uinvulnerable) {
        u.utrap -= 1 << 8;
        if (u.utrap < 1 << 8) {
            pline("You sink below the surface and die.");
            done(DISSOLVED, killer_msg(DISSOLVED, "molten lava"));
        } else if (didmove && !u.umoved) {
            pline_once("You sink deeper into the lava.");
            u.utrap += rnd(4);
        }
    }
}


static void
special_vision_handling(void)
{
    /* redo monsters if hallu or wearing a helm of telepathy */
    if (Hallucination) {        /* update screen randomly */
        see_monsters(TRUE);
        see_objects(TRUE);
        see_traps(TRUE);
        if (Engulfed)
            swallowed(0);
    } else if (Unblind_telepat) {
        see_monsters(FALSE);
    } else if (Warning || Warn_of_mon)
        see_monsters(FALSE);

    if (turnstate.vision_full_recalc)
        vision_recalc(0);       /* vision! */
}


static void
pre_move_tasks(boolean didmove, boolean loading_game)
{
    /* recalc attribute bonuses from items */
    calc_attr_bonus();

    /* we need to do this before vision handling; clairvoyance can set
       vision_full_recalc */
    if (didmove && Clairvoyant && !In_endgame(&u.uz) && !(moves % 15) &&
        !rn2(2))
        do_vicinity_map();

    /* hallucination, etc. */
    if (didmove)
        special_vision_handling();
    else if (turnstate.vision_full_recalc)
        vision_recalc(0);

    bot();

    u.umoved = FALSE;

    /* Cancel occupations if they no longer apply. */
    reset_occupations(FALSE);

    if (didmove) {
        if (last_command_was("move"))
            lookaround(flags.interaction_mode);
        else if (last_command_was("moveonly") || last_command_was("go"))
            lookaround(uim_nointeraction);
        else if (last_command_was("run"))
            lookaround(uim_displace);
        else if (flags.occupation == occ_autoexplore)
            lookaround(uim_displace);
        else if (flags.occupation == occ_travel)
            lookaround(uim_nointeraction);
        else if (monster_nearby())
            action_interrupted();
    }

    /* Running is the only thing that needs or wants persistence in
       travel direction. */
    if (flags.interrupted || !last_command_was("run"))
        clear_travel_direction();

    /* Handle realtime change now. If we just loaded a save, always print the
       messages. Otherwise, print them only on change. */
    if (!program_state.in_zero_time_command)
        realtime_tasks(last_command_was("welcome"), loading_game);

    update_inventory();
    update_location(FALSE);

    if (didmove) {
        /* Mark the current square as stepped on unless blind, since that would
           imply that we had properly explored it. */
        struct rm *loc = &level->locations[u.ux][u.uy];

        if (!Blind && !loc->mem_stepped)
            loc->mem_stepped = 1;
    }
}


void
helpless(int turns, enum helpless_reason reason, const char *cause,
         const char *endmsg)
{
    if (turns < 0) {
        impossible("Helpless for negative time; using absolute value instead");
        if (turns <= -INT_MAX)
            turns = INT_MAX;
        else
            turns = -turns;
    }

    if (turns <= turnstate.helpless_timers[reason])
        return;

    action_interrupted();

    turnstate.helpless_timers[reason] = turns;

    if (reason == hr_asleep || reason == hr_fainted) {
        turnstate.vision_full_recalc = 1;
        see_monsters(FALSE);
        see_objects(FALSE);
    }

    strcpy(turnstate.helpless_causes[reason], cause);
    if (endmsg)
        strcpy(turnstate.helpless_endmsgs[reason], endmsg);
    else
        switch (reason) {
        case hr_paralyzed:
            strcpy(turnstate.helpless_endmsgs[reason], "You can move again.");
            break;
        case hr_asleep:
            strcpy(turnstate.helpless_endmsgs[reason], "You wake up.");
            break;
        case hr_fainted:
            strcpy(turnstate.helpless_endmsgs[reason], "You come to.");
            break;
        default:
            strcpy(turnstate.helpless_endmsgs[reason], "");
        }

    /* This should come last in case we end up cancelling this helplessness as a
       result of cancelling the old one (e.g. if, hypothetically, a prayer
       completes and your god intervenes to unparalyze you) */
    enum helpless_mask mask = 0;
    if (reason != hr_praying)
        mask |= hm_praying;
    if (reason == hr_asleep || reason == hr_fainted || reason == hr_paralyzed)
        mask |= hm_busy | hm_engraving | hm_praying | hm_mimicking | hm_afraid;
    cancel_helplessness(mask, "");
}

/*
 * Cancel all helplessness that matches the given mask.
 *
 * If msg is null, then the saved message given to helpless() is printed.
 * This is used to indicate that the helplessness is expiring naturally.
 *
 * If msg is non-null and non-empty, then it will be printed if any form of
 * helplessness is canceled.
 *
 * In any case, this will perform necessary cleanups upon the respective form of
 * helplessness being canceled, such as activating a prayer's effect or ending
 * mimcry.
 */
void
cancel_helplessness(enum helpless_mask mask, const char *msg)
{
    boolean previously_unconscious = u_helpless(hm_unconscious);
    int i;

    action_interrupted();

    for (i = hr_first; i <= hr_last; ++i)
        if (mask & (1 << i)) {
            if (!u_helpless(1 << i)) {
                if (turnstate.helpless_timers[i])
                    impossible("Helpless with time but no cause at index %d",
                               i);
                /* We update mask to remove this reason, so that later
                   processing won't treat this reason as having been
                   cancelled. */
                mask &= ~(1 << i);
                continue;
            }

            turnstate.helpless_timers[i] = 0;
            *turnstate.helpless_causes[i] = '\0';
            if (!msg && *turnstate.helpless_endmsgs[i])
                pline("%s", turnstate.helpless_endmsgs[i]);
            *turnstate.helpless_endmsgs[i] = '\0';
        }

    if (mask && msg && *msg)
        pline("%s", msg);

    if (previously_unconscious && !u_helpless(hm_unconscious)) {
        turnstate.vision_full_recalc = 1;
        see_monsters(FALSE);
        see_objects(FALSE);
    }

    /* Were we mimicking something? */
    if ((mask & hm_mimicking) && youmonst.m_ap_type) {
        youmonst.m_ap_type = M_AP_NOTHING;
        youmonst.mappearance = 0;
        newsym(u.ux, u.uy);
    }

    if ((mask & hm_fainted) && u.uhs == FAINTED)
        u.uhs = FAINTING;

    if (mask & hm_praying) {
        prayer_done();
        if (u.uinvulnerable) {
            impossible("prayer_done did not clear invulnerability");
            u.uinvulnerable = FALSE;
        }
    }
}


static void
decrement_helplessness(void)
{
    int i;
    enum helpless_mask mask = hm_none;

    for (i = hr_first; i <= hr_last; ++i)
        if (turnstate.helpless_timers[i])
            if (!--turnstate.helpless_timers[i])
                mask |= 1 << i;

    if (mask)
        cancel_helplessness(mask, NULL);
}


/* Cancel mimicking.  You can't just call cancel_helplessness with hm_helpless
   to do this in all cases because it'll fail in cases where mimicking doesn't
   actually make you helpless (for instance, #monster while polymorphed while
   polymorphed into a mimic). */
void
cancel_mimicking(const char* msg)
{
    /* Make sure we're actually mimicking. */
    if (youmonst.m_ap_type) {
        turnstate.helpless_timers[hr_mimicking] = 0;
        *turnstate.helpless_causes[hr_mimicking] = '\0';
        if (!msg && *turnstate.helpless_endmsgs[hr_mimicking])
            pline("%s", turnstate.helpless_endmsgs[hr_mimicking]);
        *turnstate.helpless_endmsgs[hr_mimicking] = '\0';
        youmonst.m_ap_type = M_AP_NOTHING;
        youmonst.mappearance = 0;
        newsym(u.ux, u.uy);
    }
    else
        return;

    if (msg && *msg)
        pline("%s", msg);
}

boolean
canhear(void)
{
    return !u_helpless(hm_unconscious);
}


/* Called from command implementations to indicate that they're multi-turn
   commands; all but the last turn should call this */
void
action_incomplete(const char *reason, enum occupation ocode)
{
    strcpy(u.uwhybusy, reason);
    flags.incomplete = TRUE;
    flags.occupation = ocode;
}

/* Called when an action is logically complete: it completes it and interrupts
   it with no message. For single-action commands, this effectively means "I
   know the action only takes one turn, but even if you gave a repeat count it
   shouldn't be repeated." For multi-action commands, this means "I thought this
   action might take more than one turn, but it didn't." */
void
action_completed(void)
{
    flags.incomplete = FALSE;
    flags.interrupted = TRUE;
    flags.occupation = occ_none;
}

/* Called from just about anywhere to abort an interruptible multi-turn
 * command. This happens even if the server doesn't consider a multi-turn
 * command to be in progress; the current API allows the client to substitute
 * its own definition if it wants to.
 *
 * The general rule about which action_ function to use is:
 * - action_interrupted: if something surprises the character that is unrelated
 *   to the command they input
 * - action_completed: if the interruption is due to the action that the
 *   character was attempting, either because of invalid input, or something
 *   that interrupts that command in particular (such as walking onto an item
 *   while exploring).
 *
 * The user interface is aware of the difference between these functions.
 * However, no present interface treats them differently (apart from the message
 * that action_interrupted prints).
 */
void
action_interrupted(void)
{
    if (flags.incomplete && !flags.interrupted)
        pline("You stop %s.", u.uwhybusy);
    flags.interrupted = TRUE;
}

/* Helper function for occupations. */
void
one_occupation_turn(int (*callback)(void), const char *gerund,
                    enum occupation ocode)
{
    action_incomplete(gerund, ocode);
    if (!callback())
        action_completed();
}

/* Record if/when a given conduct was broken. */
void
break_conduct(enum player_conduct conduct)
{
    u.uconduct[conduct]++;
    if(!u.uconduct_time[conduct])
        u.uconduct_time[conduct] = moves;

    /* Monks avoid breaking vegetarian conduct. */
    if(conduct == conduct_vegetarian && Role_if(PM_MONK)) {
        pline("You feel guilty.");
        adjalign(-1);
    }
}

/* perform the command given by cmdidx (an index into cmdlist in cmd.c) */
static void
command_input(int cmdidx, struct nh_cmd_arg *arg)
{
    boolean didmove = TRUE;

    if (!u_helpless(hm_all)) {
        switch (do_command(cmdidx, arg)) {
        case COMMAND_UNKNOWN:
            pline("Unrecognised command.");
            action_interrupted();
            return;
        case COMMAND_DEBUG_ONLY:
            pline("That command is only available in debug mode.");
            action_interrupted();
            return;
        case COMMAND_BAD_ARG:
            pline("I don't understand what you want that command to apply to.");
            action_interrupted();
            return;
        case COMMAND_ZERO_TIME:
            didmove = FALSE;
            break;
        case COMMAND_OK:
            /* nothing to do */
            break;
        }
    }

    deferred_goto();        /* after rhack() */

    if (turnstate.vision_full_recalc)
        vision_recalc(0);       /* vision! */

    if (didmove)
        you_moved();

    /* actual time passed */

    /* prepare for the next move */
    pre_move_tasks(didmove, FALSE);

    flush_screen();
}


static void
newgame(microseconds birthday)
{
    int i;

    flags.ident = FIRST_PERMANENT_IDENT; /* lower values are temporaries */

    for (i = 0; i < NUMMONS; i++)
        mvitals[i].mvflags = mons[i].geno & G_NOCORPSE;

    flags.turntime = birthday;       /* get realtime right for level gen */

    init_objects();     /* must be before u_init() */

    role_init();        /* must be before init_dungeons(), u_init(), and
                           init_artifacts() */

    init_dungeons();    /* must be before u_init() to avoid rndmonst() creating
                           odd monsters for any tins and eggs in hero's initial
                           inventory */
    init_artifacts();
    u_init(birthday);   /* struct you must have some basic data for mklev to
                           work right */
    pantheon_init(TRUE);

    load_qtlist();      /* load up the quest text info */

    level = mklev(&u.uz);

    u_on_upstairs();    /* place the player on the upstairs before initializing
                           inventory, or else the x-ray vision check when
                           wearing armor will cause 0,0 to be seen */
    u_init_inv_skills();        /* level must be valid to create items */
    vision_reset();     /* set up internals for level (after mklev) */
    check_special_room(FALSE);

    /* Move the monster from under you or else makedog() will fail when it
       calls makemon().  - ucsfcgl!kneller */
    if (MON_AT(level, u.ux, u.uy))
        mnexto(m_at(level, u.ux, u.uy));

    /* This can clobber the charstars RNGs, so call it late */
    makedog();

    /* Stop autoexplore revisiting the entrance stairs (or position). */
    level->locations[u.ux][u.uy].mem_stepped = 1;

    historic_event(FALSE, FALSE,
                   "entered the Dungeons of Doom to retrieve the Amulet of "
                   "Yendor!");

    /* prepare for the first move */
    set_wear();

    u.moveamt = NORMAL_SPEED;   /* hero is normal speed on turn 1 */
    post_init_tasks();
}

/*allmain.c*/
