/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
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

extern const struct cmd_desc cmdlist[];

static const char *const copyright_banner[] =
    { COPYRIGHT_BANNER_A, COPYRIGHT_BANNER_B, COPYRIGHT_BANNER_C, NULL };

static void pre_move_tasks(boolean);

static void newgame(void);

static void handle_lava_trap(boolean didmove);

static void command_input(int cmdidx, struct nh_cmd_arg *arg);

static void decrement_helplessness(void);

const char *const *
nh_get_copyright_banner(void)
{
    return copyright_banner;
}

void
nh_lib_init(const struct nh_window_procs *procs, char **paths)
{
    int i;

    DEBUG_LOG("Initializing NetHack engine...\n");

    API_ENTRY_CHECKPOINT_RETURN_VOID_ON_ERROR();
    windowprocs = *procs;

    for (i = 0; i < PREFIX_COUNT; i++)
        fqn_prefix[i] = strdup(paths[i]);

    u.uhp = 1;  /* prevent RIP on early quits */
    turntime = 0;

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
    /* This routine always throws an exception, and normally doesn't return (it
       lets nh_play_game do the returning). It will, however, have to return
       itself if the game isn't running. In such a case, all options give a
       success return; saving/exiting the game is easy (but a no-op) if it isn't
       running.
    */
    API_ENTRY_CHECKPOINT_RETURN_ON_ERROR(TRUE);

    xmalloc_cleanup(&api_blocklist);

    if (program_state.game_running) {

        switch (exit_type) {
        case EXIT_REQUEST_SAVE:
            dosave(&(struct nh_cmd_arg){.argtype = 0});
            break;

        case EXIT_FORCE_SAVE:
            terminate(GAME_DETACHED);
            break;

        case EXIT_REQUEST_QUIT:
            done2();
            break;

        case EXIT_FORCE_QUIT:
            done(QUIT);
            break;      /* not reached; quitting can't be lifesaved */

        case EXIT_PANIC:
            /* We can't/shouldn't abort the turn just because the client claimed
               to malfunction; that's exploitable. We can safely log the
               failure, though. Perhaps we should add some method of specifying
               a panic message. */
            paniclog("ui_problem", "Unspecified UI problem.");
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


void
startup_common(boolean including_program_state)
{
    /* (re)init all global data */
    init_data(including_program_state);
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


void
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
        }
    }

    if (fri13 && flags.friday13)
        pline("Watch out!  Bad things can happen on Friday the 13th.");
}


static void
realtime_tasks(boolean print)
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
    } else if (flags.moonphase == NEW_MOON && prev_moonphase != NEW_MOON) {
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

    if (print)
        realtime_messages(msg_moonphase, msg_friday13);
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
    pre_move_tasks(0);
}


enum nh_create_response
nh_create_game(int fd, struct nh_option_desc *opts)
{
    unsigned int seed = 0;
    int i;

    API_ENTRY_CHECKPOINT() {
        IF_API_EXCEPTION(GAME_CREATED):
            return NHCREATE_OK;

        IF_ANY_API_EXCEPTION():
            return NHCREATE_FAIL;
    }

    if (fd == -1) {
        API_EXIT();
        return NHCREATE_INVALID;
    }

    program_state.suppress_screen_updates = TRUE;

    turntime = (unsigned long long)time(NULL);
    seed = turntime ^ get_seedval();
    /* initialize the random number generator */
    mt_srand(seed);

    startup_common(TRUE);
    /* Set defaults in case list of options from client was incomplete. */
    struct nh_option_desc *defaults = default_options();
    for (i = 0; defaults[i].name; i++)
        nh_set_option(defaults[i].name, defaults[i].value);
    nhlib_free_optlist(defaults);
    for (i = 0; opts[i].name; i++)
        nh_set_option(opts[i].name, opts[i].value);

    if (wizard)
        strcpy(u.uplname, "wizard");

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
    log_newgame(turntime, seed);

    newgame();

    /* We need a full backup save after creating the new game, because we
       don't have anything to diff against. */
    log_backup_save();
    log_uninit();

    program_state.suppress_screen_updates = FALSE;

    terminate(GAME_CREATED);
}

enum nh_play_status
nh_play_game(int fd)
{
    volatile int ret;

    if (fd < 0)
        return ERR_BAD_ARGS;

    switch (nh_get_savegame_status(fd, NULL)) {
    case LS_INVALID:
        return ERR_BAD_FILE;
    case LS_DONE:
        /* TODO: Load in replay mode. */
        return GAME_ALREADY_OVER;
    case LS_CRASHED:
        return ERR_RESTORE_FAILED;
    case LS_IN_PROGRESS:
        return ERR_IN_PROGRESS;
    case LS_SAVED:
        break;  /* default, everything is A-OK */
    }

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

        /* This happens if the game needs to escape from a deeply nested
           context. The longjmp() is not enough by itself in case the network
           API is involved (returning from nh_play_game out of sequence causes
           the longjmp() to propagate across the newtork). */
    IF_API_EXCEPTION(RESTART_PLAY):
        ret = RESTART_PLAY;
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
    
    startup_common(TRUE);

    /* Load the save file. log_sync() needs to be called at least once because
       we no longer try to rerun the new game sequence, and thus must start by
       loading a binary save. (In addition, using log_sync() is /much/ faster
       than attempting to replay the entire game.) */
    log_init(fd);
    program_state.target_location_units = TLU_EOF;
    log_sync();

    program_state.game_running = TRUE;
    post_init_tasks();

    /* While loading a save file, we don't do rendering, and we don't run
       the vision system. Do all that stuff now. */
    vision_reset();
    doredraw();
    notify_levelchange(NULL);
    bot();
    flush_screen();

    realtime_tasks(FALSE);
    update_inventory();

    /* The main loop. */
    while (1) {
        char cmd[BUFSZ];
        /* We must initialize arg because it can end up being copied into the
           save file. */
        struct nh_cmd_arg arg = {0};

        int cmdidx;

        if (u_helpless(hm_all) && !program_state.viewing) {
            cmdidx = get_command_idx("wait");
            arg.argtype = 0;
        } else {

            if (!log_replay_command(cmd, &arg))
                (*windowprocs.win_request_command)
                    (wizard, !flags.incomplete, flags.interrupted, cmd, &arg);

            cmdidx = get_command_idx(cmd);
        }

        if (cmdidx < 0) {
            pline("Unrecognised command '%s'", cmd);
            continue;
        }

        if (program_state.viewing &&
            (cmdidx < 0 || !(cmdlist[cmdidx].flags & CMD_NOTIME))) {
            pline("Command '%s' unavailable while watching/replaying a game.",
                  cmd);
            continue;
        }

        /* TODO: Better resolution for turntime */
        turntime = time(NULL);

        /* Make sure the client hasn't given extra arguments on top of the ones
           that we'd normally accept. To simplify things, we just silently drop
           any additional arguments. We do this before logging so that the
           extra arguments aren't recorded in the save file. */
        arg.argtype &= cmdlist[cmdidx].flags;

        if (cmdlist[cmdidx].flags & CMD_NOTIME &&
            (flags.incomplete || !flags.interrupted || flags.occupation)) {

            /* CMD_NOTIME actions don't set last_cmd/last_arg, so we need to
               ensure we interrupt them in order to avoid screwing up command
               repeat. We accomplish this via logging an "interrupt" command. */

            flags.incomplete = FALSE;
            flags.interrupted = FALSE; /* set to true by "interrupt" */

            log_record_command("interrupt",
                               &(struct nh_cmd_arg){.argtype = 0});
            command_input(get_command_idx("interrupt"),
                          &(struct nh_cmd_arg){.argtype = 0});
            neutral_turnstate_tasks();

        } else {

            flags.incomplete = FALSE;
            flags.interrupted = FALSE;

        }

        program_state.in_zero_time_command =
            !!(cmdlist[cmdidx].flags & CMD_NOTIME);

        /* We can't record a command if we didn't prompt for one; it creates
           desyncs when replaying. (If we /did/ prompt for one, it's OK to
           record a different one, which is why the record of "interrupt" above
           is OK; it's not OK to record two, but log_record_command won't record
           zero-time commands.) */
        if (!u_helpless(hm_all))
            log_record_command(cmd, &arg);

        command_input(cmdidx, &arg);

        program_state.in_zero_time_command = FALSE;

        if (cmdlist[cmdidx].flags & CMD_NOTIME)
            log_revert_command(); /* make sure it didn't change the gamestate */
        else if ((!flags.incomplete || flags.interrupted) &&
                 !u_helpless(hm_all))
            neutral_turnstate_tasks();
    }

normal_exit:
    program_state.game_running = FALSE;
    log_uninit();

    return ret;
}


static void
you_moved(void)
{
    int moveamt = 0, wtcap = 0, change = 0;
    boolean monscanmove = FALSE;

    /* actual time passed */
    youmonst.movement -= NORMAL_SPEED;

    do {        /* hero can't move this turn loop */
        wtcap = encumber_msg();
        calc_attr_bonus();

        flags.mon_moving = TRUE;
        do {
            monscanmove = movemon();
            if (youmonst.movement > NORMAL_SPEED)
                break;  /* it's now your turn */
        } while (monscanmove);
        flags.mon_moving = FALSE;

        if (!monscanmove && youmonst.movement < NORMAL_SPEED) {
            /* both you and the monsters are out of steam this round */
            /* set up for a new turn */
            struct monst *mtmp;

            mcalcdistress();    /* adjust monsters' trap, blind, etc */

            /* reallocate movement rations to monsters */
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
                mtmp->movement += mcalcmove(mtmp);

            if (flags.mon_generation &&
                !rn2(u.uevent.udemigod ? 25 :
                     (depth(&u.uz) > depth(&stronghold_level)) ? 50 : 70))
                makemon(NULL, level, 0, 0, NO_MM_FLAGS);

            /* calculate how much time passed. */
            if (u.usteed && u.umoved) {
                /* your speed doesn't augment steed's speed */
                moveamt = mcalcmove(u.usteed);
            } else {
                moveamt = youmonst.data->mmove;

                if (Very_fast) {        /* speed boots or potion */
                    /* average movement is 1.67 times normal */
                    moveamt += NORMAL_SPEED / 2;
                    if (rn2(3) == 0)
                        moveamt += NORMAL_SPEED / 2;
                } else if (Fast) {
                    /* average movement is 1.33 times normal */
                    if (rn2(3) != 0)
                        moveamt += NORMAL_SPEED / 2;
                }
            }

            switch (wtcap) {
            case UNENCUMBERED:
                break;
            case SLT_ENCUMBER:
                moveamt -= (moveamt / 4);
                break;
            case MOD_ENCUMBER:
                moveamt -= (moveamt / 2);
                break;
            case HVY_ENCUMBER:
                moveamt -= ((moveamt * 3) / 4);
                break;
            case EXT_ENCUMBER:
                moveamt -= ((moveamt * 7) / 8);
                break;
            default:
                break;
            }

            youmonst.movement += moveamt;
            if (youmonst.movement < 0)
                youmonst.movement = 0;
            settrack();

            moves++;
            level->lastmoves = moves;

            /********************************/
            /* once-per-turn things go here */
            /********************************/

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
                    rehumanize();
            } else if (Upolyd && u.mh < u.mhmax) {
                if (u.mh < 1)
                    rehumanize();
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
    } while (youmonst.movement < NORMAL_SPEED); /* hero can't move loop */

    /******************************************/
    /* once-per-hero-took-time things go here */
    /******************************************/

    if (u.utrap && u.utraptype == TT_LAVA)
        handle_lava_trap(TRUE);
}


static void
handle_lava_trap(boolean didmove)
{
    if (!is_lava(level, u.ux, u.uy))
        u.utrap = 0;
    else if (!u.uinvulnerable) {
        u.utrap -= 1 << 8;
        if (u.utrap < 1 << 8) {
            killer_format = KILLED_BY;
            killer = "molten lava";
            pline("You sink below the surface and die.");
            done(DISSOLVED);
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
        see_monsters();
        see_objects();
        see_traps();
        if (Engulfed)
            swallowed(0);
    } else if (Unblind_telepat) {
        see_monsters();
    } else if (Warning || Warn_of_mon)
        see_monsters();

    if (turnstate.vision_full_recalc)
        vision_recalc(0);       /* vision! */
}


static void
pre_move_tasks(boolean didmove)
{
    /* recalc attribute bonuses from items */
    calc_attr_bonus();
    
    /* hallucination, etc. */
    special_vision_handling();

    bot();

    if (didmove && Clairvoyant && !In_endgame(&u.uz) && !(moves % 15) &&
        !rn2(2))
        do_vicinity_map();

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
     * travel direction. */
    if (flags.interrupted || !last_command_was("run"))
        clear_travel_direction();    

    if (didmove && moves % 100 == 0)
        realtime_tasks(TRUE);

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
        see_monsters();
        see_objects();
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
     * result of cancelling the old one (e.g. if, hypothetically, a prayer
     * completes and your god intervenes to unparalyze you)
     */
    enum helpless_mask mask = 0;
    if (reason != hr_praying)
        mask |= hm_praying;
    if (reason == hr_asleep || reason == hr_fainted || reason == hr_paralyzed)
        mask |= hm_busy | hm_engraving | hm_praying | hm_mimicking | hm_afraid;
    cancel_helplessness(mask, "");
}

/* Cancel all helplessness that matches the given mask.
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
                 * processing won't treat this reason as having been canceled.
                 */
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
        see_monsters();
        see_objects();
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


boolean
u_helpless(enum helpless_mask mask)
{
    int i;

    /* A lack of a cause canonically indicates that we weren't actually helpless
     * for this reason. We may not have an endmsg, and the timer may already
     * have expired but the helplessness not yet been canceled, so we can't use
     * these as indications. */
    for (i = hr_first; i <= hr_last; ++i)
        if ((mask & (1 << i)) && *turnstate.helpless_causes[i])
            return TRUE;

    return FALSE;
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
        int i = 1 << hr_mimicking;
        turnstate.helpless_timers[i] = 0;
        *turnstate.helpless_causes[i] = '\0';
        if (!msg && *turnstate.helpless_endmsgs[i])
            pline("%s", turnstate.helpless_endmsgs[i]);
        *turnstate.helpless_endmsgs[i] = '\0';
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

    flags.occupation = occ_none;
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

    if (u.utotype)      /* change dungeon level */
        deferred_goto();        /* after rhack() */

    if (turnstate.vision_full_recalc)
        vision_recalc(0);       /* vision! */

    if (didmove)
        you_moved();

    /* actual time passed */

    /* prepare for the next move */
    pre_move_tasks(didmove);

    flush_screen(); 
}


static void
newgame(void)
{
    int i;

    flags.ident = FIRST_PERMANENT_IDENT; /* lower values are temporaries */

    for (i = 0; i < NUMMONS; i++)
        mvitals[i].mvflags = mons[i].geno & G_NOCORPSE;

    init_objects();     /* must be before u_init() */

    role_init();        /* must be before init_dungeons(), u_init(), and
                           init_artifacts() */

    init_dungeons();    /* must be before u_init() to avoid rndmonst() creating 
                           odd monsters for any tins and eggs in hero's initial 
                           inventory */
    init_artifacts();
    u_init();   /* struct you must have some basic data for mklev to work right 
                 */
    pantheon_init(TRUE);

    load_qtlist();      /* load up the quest text info */

    level = mklev(&u.uz);

    u_init_inv_skills();        /* level must be valid to create items */
    u_on_upstairs();
    vision_reset();     /* set up internals for level (after mklev) */
    check_special_room(FALSE);

    /* Move the monster from under you or else makedog() will fail when it
       calls makemon().  - ucsfcgl!kneller */
    if (MON_AT(level, u.ux, u.uy))
        mnexto(m_at(level, u.ux, u.uy));
    makedog();

    /* Stop autoexplore revisiting the entrance stairs (or position). */
    level->locations[u.ux][u.uy].mem_stepped = 1;

    historic_event(FALSE,
                   "entered the Dungeons of Doom to retrieve the Amulet of "
                   "Yendor!");

    /* prepare for the first move */
    set_wear();

    youmonst.movement = NORMAL_SPEED;   /* give the hero some movement points */
    post_init_tasks();
}

/*allmain.c*/

