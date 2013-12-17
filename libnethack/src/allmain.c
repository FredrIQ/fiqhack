/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-12-17 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* various code that was replicated in *main.c */

#include "hack.h"

#include <sys/stat.h>

#include "dlb.h"
#include "hack.h"
#include "patchlevel.h"

extern const struct cmd_desc cmdlist[];

static const char *const copyright_banner[] =
    { COPYRIGHT_BANNER_A, COPYRIGHT_BANNER_B, COPYRIGHT_BANNER_C, NULL };

static void pre_move_tasks(boolean);

static void newgame(void);

static void handle_lava_trap(boolean didmove);

static void command_input(int cmdidx, int rep, struct nh_cmd_arg *arg);

const char *const *
nh_get_copyright_banner(void)
{
    return copyright_banner;
}

void
nh_lib_init(const struct nh_window_procs *procs, char **paths)
{
    int i;

    API_ENTRY_CHECKPOINT_RETURN_VOID_ON_ERROR();
    windowprocs = *procs;

    for (i = 0; i < PREFIX_COUNT; i++)
        fqn_prefix[i] = strdup(paths[i]);

    u.uhp = 1;  /* prevent RIP on early quits */
    init_opt_struct();
    turntime = 0;

    API_EXIT();
}


void
nh_lib_exit(void)
{
    int i;

    xmalloc_cleanup();

    for (i = 0; i < PREFIX_COUNT; i++) {
        free(fqn_prefix[i]);
        fqn_prefix[i] = NULL;
    }

    cleanup_opt_struct();
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

    xmalloc_cleanup();

    if (program_state.game_running) {

        switch (exit_type) {
        case EXIT_REQUEST_SAVE:
            dosave();   /* will ask "really save?" and, if 'y', eventually call 
                           terminate. */
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
           checkpoint with terminate(). */
        terminate(GAME_ALREADY_OVER); /* doesn't return */
    }
}


void
startup_common(boolean including_program_state)
{
    /* (re)init all global data */
    init_data(including_program_state);
    reset_food();       /* zero out victual and tin */
    reset_steal();
    reset_dig_status();
    reset_encumber_msg();

    /* create mutable copies of object and artifact liss */
    init_objlist();
    init_artilist();
    reset_rndmonst(NON_PM);
    free_dungeon();     /* clean up stray dungeon data */

    program_state.game_running = 0;
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
        }
    }

    if (fri13 && flags.friday13)
        pline("Watch out!  Bad things can happen on Friday the 13th.");
}


static void
realtime_tasks(void)
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

    realtime_messages(msg_moonphase, msg_friday13);
}


static void
post_init_tasks(void)
{
    /* dorecover() used to run timers. The side effects from dorecover() have
       been moved here (so that dorecover() can be used to test save file
       integrity); but the timer run has been removed altogether (the /correct/
       place for it is goto_level(), and there was already a call there). */

    /* Prepare for the first move. */
    flags.move = 0;                  /* TODO: Does this help? Does this hurt? */
    pre_move_tasks(0);
}


int
nh_create_game(int fd, const char *name, int irole, int irace, int igend,
               int ialign, enum nh_game_modes playmode)
{
    unsigned int seed = 0;

    API_ENTRY_CHECKPOINT_RETURN_ON_ERROR(FALSE);

    program_state.suppress_screen_updates = TRUE;

    if (fd == -1 || !name || !*name)
        goto err_out;

    turntime = (unsigned long long)time(NULL);
    seed = turntime ^ get_seedval();
    /* initialize the random number generator */
    mt_srand(seed);

    startup_common(TRUE);

    if (playmode == MODE_EXPLORE)
        discover = TRUE;
    else if (playmode == MODE_WIZARD)
        wizard = TRUE;

    if (name && name[0]) {
        strncpy(u.uplname, name, PL_NSIZ);
        u.uplname[PL_NSIZ - 1] = '\0';
    }

    if (wizard)
        strcpy(u.uplname, "wizard");

    if (!validrole(irole) || !validrace(irole, irace) ||
        !validgend(irole, irace, igend) || !validalign(irole, irace, ialign))
        goto err_out;
    u.initrole = irole;
    u.initrace = irace;
    u.initgend = igend;
    u.initalign = ialign;

    /* We create a new save file that saves the state immediately after
       newgame() is called. */
    log_init(fd);
    log_newgame(turntime, seed, playmode);

    newgame();

    /* We need a full backup save after creating the new game, because we
       don't have anything to diff against. */
    log_backup_save();
    log_uninit();

    program_state.suppress_screen_updates = FALSE;
    API_EXIT();
    return TRUE;

err_out:
    program_state.suppress_screen_updates = FALSE;
    API_EXIT();
    return FALSE;
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
    IF_API_EXCEPTION(ERR_REPLAY_FAILED):
        ret = ERR_REPLAY_FAILED;
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

    realtime_messages(TRUE, TRUE);
    update_inventory();

    /* The main loop. */
    boolean completed = TRUE;
    boolean interrupted = FALSE;
    while (1) {
        char cmd[BUFSZ];
        struct nh_cmd_arg arg;
        int limit;

        int cmdidx;

        (*windowprocs.win_request_command)
            (wizard, completed, interrupted, cmd, &arg, &limit);
        cmdidx = get_command_idx(cmd);
        if (!strcmp(cmd, "repeat")) {
            cmdidx = -1;
            
        } else if (cmdidx < 0) {
            pline("Unrecognised command '%s'", cmd);
            completed = TRUE;
            interrupted = FALSE;
            continue;
        } else if (multi > 0) {
            /* allow interruption of multi-turn commands */
            nomul(0, NULL);
        }

        if (program_state.viewing &&
            (cmdidx < 0 || !(cmdlist[cmdidx].flags & CMD_NOTIME))) {
            pline("Command '%s' unavailable while watching/replaying a game.",
                  cmd);
            completed = TRUE;
            interrupted = FALSE;
            continue;
        }

        /* TODO: Better resolution for turntime */
        turntime = time(NULL);

        unsigned int pre_rngstate = mt_nextstate();
        int pre_moves = moves;

        command_input(cmdidx, limit, &arg);

        /* make sure we actually want this command to be logged */
        if (cmdidx >= 0 && (cmdlist[cmdidx].flags & CMD_NOTIME) &&
            pre_rngstate == mt_nextstate() && pre_moves == moves)
            log_revert_command();   /* nope, cut it out of the log */
        else if (!multi && !occupation)
            neutral_turnstate_tasks();

        /* 
         * performing a command can put the game into several different states:
         *  - the command completes immediately: a simple move or an attack etc.
         *    multi == 0, occupation == NULL
         *  - if a count is given, the command will (usually) take count turns
         *    multi == count (> 0), occupation == NULL
         *  - the command may cause a delay: for ex. putting on or removing
         *    armor multi == -delay (< 0), occupation == NULL
         *    multi is incremented in you_moved
         *  - the command may take multiple moves, and require a callback to be
         *    run for each move. example: forcing a lock
         *    multi >= 0, occupation == callback
         */
        completed = multi == 0 && !occupation;
        interrupted = FALSE; /* TODO */
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

            if (iflags.mon_generation &&
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
            iflags.botl = 1;

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
                    iflags.botl = 1;
                } else if (u.mh < 1)
                    rehumanize();
            } else if (Upolyd && u.mh < u.mhmax) {
                if (u.mh < 1)
                    rehumanize();
                else if (Regeneration ||
                         (wtcap < MOD_ENCUMBER && !(moves % 20))) {
                    iflags.botl = 1;
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
                    iflags.botl = 1;
                    u.uhp += heal;
                    if (u.uhp > u.uhpmax)
                        u.uhp = u.uhpmax;
                } else if (Regeneration ||
                           (u.ulevel <= 9 &&
                            !(moves % ((MAXULEV + 12) / (u.ulevel + 2) + 1)))) {
                    iflags.botl = 1;
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
                        fall_asleep(-10, FALSE);
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
                iflags.botl = 1;
            }

            if (!u.uinvulnerable) {
                if (Teleportation && !rn2(85)) {
                    xchar old_ux = u.ux, old_uy = u.uy;

                    tele();
                    if (u.ux != old_ux || u.uy != old_uy) {
                        if (!next_to_u()) {
                            check_leash(old_ux, old_uy);
                        }
                        iflags.botl = 1;
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
                    if (multi >= 0) {
                        if (occupation)
                            stop_occupation();
                        else
                            nomul(0, NULL);
                        if (change == 1)
                            polyself(FALSE);
                        else
                            you_were();
                        change = 0;
                    }
                }
            }

            if (Searching && multi >= 0)
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

            if (!u.umoved && (Is_waterlevel(&u.uz) || !(Flying || Levitation))) {
                if (Underwater)
                    drown();
                else if (is_lava(level, u.ux, u.uy))
                    lava_effects();
            }

            /* when immobile, count is in turns */
            if (multi < 0) {
                if (++multi == 0) {     /* finished yet? */
                    unmul(NULL);
                    /* if unmul caused a level change, take it now */
                    if (u.utotype)
                        deferred_goto();
                }
            }
        }
    } while (youmonst.movement < NORMAL_SPEED); /* hero can't move loop */

    /******************************************/
    /* once-per-hero-took-time things go here */
    /******************************************/

    if (u.utrap && u.utraptype == TT_LAVA)
        handle_lava_trap(TRUE);
}


static void
handle_occupation(void)
{
    if ((*occupation) () == 0) {
        occupation = NULL;
        *turnstate.occupation_txt = 0;
        return;
    }
    if (monster_nearby()) {
        stop_occupation();
        reset_eat();
    }
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
            Norep("You sink deeper into the lava.");
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

    if (vision_full_recalc)
        vision_recalc(0);       /* vision! */
}


static void
pre_move_tasks(boolean didmove)
{
    /* recalc attribute bonuses from items */
    calc_attr_bonus();
    find_ac();
    if (!flags.mv || Blind)
        special_vision_handling();

    if (iflags.botl)
        bot();

    if (didmove && Clairvoyant && !In_endgame(&u.uz) && !(moves % 15) &&
        !rn2(2))
        do_vicinity_map();

    u.umoved = FALSE;

    if (multi > 0) {
        lookaround();
        if (!multi) {
            /* lookaround may clear multi */
            flags.move = 0;
            iflags.botl = 1;
        }
    }

    if (didmove && moves % 100 == 0)
        realtime_tasks();

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


/* perform the command given by cmdidx (an index into cmdlist in cmd.c) */
static void
command_input(int cmdidx, int rep, struct nh_cmd_arg *arg)
{
    boolean didmove = FALSE;

    if (multi >= 0 && occupation)
        handle_occupation();
    else if (multi == 0 || (multi > 0 && cmdidx != -1)) {
        if (multi) {
            turnstate.saved_cmd = cmdidx;
            turnstate.saved_arg = *arg;
        }
        switch (do_command(cmdidx, rep, TRUE, arg)) {
        case COMMAND_UNKNOWN:
            pline("Unrecognised command.");
            nomul(0, NULL);
            return;
        case COMMAND_DEBUG_ONLY:
            pline("That command is only available in debug mode.");
            nomul(0, NULL);
            return;
        case COMMAND_BAD_ARG:
            pline("I don't understand what you want that command to apply to.");
            nomul(0, NULL);
            return;
        }
    } else if (multi > 0) {
        if (flags.mv) {
            if (multi < COLNO && !--multi)
                flags.travel = iflags.travel1 = flags.mv = flags.run = 0;
            if (!multi)
                nomul(0, NULL); /* reset multi state */
            if (!domove(u.dx, u.dy, 0)) {
                /* Don't use a move when travelling into an obstacle. */
                flags.move = FALSE;
                nomul(0, NULL);
            }
        } else
            if (do_command(turnstate.saved_cmd, multi, FALSE,
                           &turnstate.saved_arg) !=
                COMMAND_OK) {
                pline("Unrecognised command."); 
                nomul(0, NULL);
                return;
            }
    }
    /* no need to do anything here for multi < 0 */

    if (u.utotype)      /* change dungeon level */
        deferred_goto();        /* after rhack() */
    /* !flags.move here: multiple movement command stopped */
    else if (!flags.move || !flags.mv)
        iflags.botl = 1;

    if (vision_full_recalc)
        vision_recalc(0);       /* vision! */
    /* when running in non-tport mode, this gets done through domove() */
    if ((!flags.run || iflags.runmode == RUN_TPORT) &&
        (multi && (!flags.travel ? !(multi % 7) : !(moves % 7L)))) {
        if (flags.run)
            iflags.botl = 1;
        flush_screen();
    }

    didmove = flags.move;
    if (didmove) {
        you_moved();
    }

    /* actual time passed */
    /****************************************/
    /* once-per-player-input things go here */
     /****************************************/
    xmalloc_cleanup();
    iflags.next_msg_nonblocking = 0;

    /* prepare for the next move */
    flags.move = 1;
    pre_move_tasks(didmove);

    if (multi == 0 && !occupation)
        flush_screen(); /* Flush screen buffer */
}


void
stop_occupation(void)
{
    if (occupation) {
        if (!maybe_finished_meal(TRUE))
            pline("You stop %s.", turnstate.occupation_txt);
        occupation = 0;
        *(turnstate.occupation_txt) = 0;
        iflags.botl = 1;        /* in case u.uhs changed */
        /* fainting stops your occupation, there's no reason to sync.
           sync_hunger(); */
    }
    nomul(0, NULL); /* running, travel and autotravel don't count as occupations */
}


static void
newgame(void)
{
    int i;

    flags.ident = 1;

    for (i = 0; i < NUMMONS; i++)
        mvitals[i].mvflags = mons[i].geno & G_NOCORPSE;

    init_objects();     /* must be before u_init() */

    flags.pantheon = -1;        /* role_init() will reset this */
    role_init();        /* must be before init_dungeons(), u_init(), and
                           init_artifacts() */

    init_dungeons();    /* must be before u_init() to avoid rndmonst() creating 
                           odd monsters for any tins and eggs in hero's initial 
                           inventory */
    init_artifacts();
    u_init();   /* struct you must have some basic data for mklev to work right 
                 */

    load_qtlist();      /* load up the quest text info */

    level = mklev(&u.uz);

    u_init_inv_skills();        /* level must be valid to create items */
    u_on_upstairs();
    vision_reset();     /* set up internals for level (after mklev) */
    check_special_room(FALSE);

    iflags.botl = 1;

    /* Move the monster from under you or else makedog() will fail when it
       calls makemon().  - ucsfcgl!kneller */
    if (MON_AT(level, u.ux, u.uy))
        mnexto(m_at(level, u.ux, u.uy));
    makedog();

    /* Stop autoexplore revisiting the entrance stairs (or position). */
    level->locations[u.ux][u.uy].mem_stepped = 1;

    historic_event(FALSE,
                   "entered the Dungeons of Doom to retrieve the Amulet of Yendor!");

    /* prepare for the first move */
    flags.move = 0;
    set_wear();

    program_state.game_running = TRUE;
    youmonst.movement = NORMAL_SPEED;   /* give the hero some movement points */
    post_init_tasks();

    return;
}

/*allmain.c*/
