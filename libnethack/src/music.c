/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-14 */
/* Copyright (c) 1989 by Jean-Christophe Collet */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * This file contains the different functions designed to manipulate the
 * musical instruments and their various effects.
 *
 * Actually the list of instruments / effects is :
 *
 * (wooden) flute       may calm snakes if player has enough dexterity
 * magic flute          may put monsters to sleep:  area of effect depends
 *                      on player level.
 * (tooled) horn        Will awaken monsters:  area of effect depends on player
 *                      level.  May also scare monsters.
 * fire horn            Acts like a wand of fire.
 * frost horn           Acts like a wand of cold.
 * bugle                Will awaken soldiers (if any):  area of effect depends
 *                      on player level.
 * (wooden) harp        May calm nymph if player has enough dexterity.
 * magic harp           Charm monsters:  area of effect depends on player
 *                      level.
 * (leather) drum       Will awaken monsters like the horn.
 * drum of earthquake   Will initiate an earthquake whose intensity depends
 *                      on player level.  That is, it creates random pits
 *                      called here chasms.
 */

#include "hack.h"

static void put_monsters_to_sleep(int);
static void charm_snakes(int);
static void calm_nymphs(int);
static void charm_monsters(int);
static void do_earthquake(int);
static int do_improvisation(struct obj *, const struct nh_cmd_arg *);

/* Wake every monster in range... */
void
awaken_monsters(struct monst *mon, int distance)
{
    struct monst *mtmp;
    int distm;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp) || mon == mtmp)
            continue;

        distm = dist2(m_mx(mon), m_my(mon), mtmp->mx, mtmp->my);
        if (distm >= distance || mon == mtmp)
            continue;

        mtmp->msleeping = 0;
        mtmp->mcanmove = 1;
        mtmp->mfrozen = 0;

        /* May scare some monsters */
        if (distm >= distance / 3 ||
            resist(mon, mtmp, TOOL_CLASS, NOTELL, 0))
            continue;

        monflee(mtmp, 0, FALSE, TRUE);
    }

    /* affect the player */
    distm = dist2(m_mx(mon), m_my(mon), u.ux, u.uy);
    if (distm >= distance || mon == &youmonst)
        return;

    cancel_helplessness(hm_unconscious,
                        "Surprised, you are jolted into full consciousness!");

    if (distm >= distance / 3)
        return;

    /* there is no real player scare effect, so make it work similar to potion
       ghosts, except only 2 turns. Until an actual player scared effect can
       be made, monsters utilizing the paralyzation "intelligently" isn't
       fair game -- it currently only happens if a monster is about to discover
       that a magical instrument horn is out of charges */
    if (mon != &youmonst) {
        pline(msgc_statusbad,
              "The loud noise frightens you momentarily and you are unable to move!");
        helpless(2, hr_afraid, "being frightened",
                 "You regain your composure.");
    }
}

/* Make monsters fall asleep.  Note that they may resist the spell. */
static void
put_monsters_to_sleep(int distance)
{
    struct monst *mtmp = level->monlist;

    while (mtmp) {
        if (!DEADMONSTER(mtmp) && distu(mtmp->mx, mtmp->my) < distance &&
            sleep_monst(&youmonst, mtmp, dice(10, 10), TOOL_CLASS)) {
            mtmp->msleeping = 1;        /* 10d10 turns + wake_nearby to rouse */
            slept_monst(mtmp);
        }
        mtmp = mtmp->nmon;
    }
}

/* Charm snakes in range.  Note that the snakes are NOT tamed. */
static void
charm_snakes(int distance)
{
    struct monst *mtmp = level->monlist;
    int could_see_mon, was_peaceful;

    while (mtmp) {
        if (!DEADMONSTER(mtmp) && mtmp->data->mlet == S_SNAKE && mtmp->mcanmove
            && distu(mtmp->mx, mtmp->my) < distance) {
            was_peaceful = mtmp->mpeaceful;
            mtmp->mavenge = 0;
            could_see_mon = canspotmon(mtmp);
            mtmp->mundetected = 0;
            msethostility(mtmp, FALSE, FALSE); /* does a newsym() */
            if (canseemon(mtmp)) {
                if (!could_see_mon)
                    pline(msgc_youdiscover,
                          "You notice %s, swaying with the music.",
                          a_monnam(mtmp));
                else
                    pline(msgc_actionok,
                          "%s freezes, then sways with the music%s.",
                          Monnam(mtmp),
                          was_peaceful ? "" : ", and now seems quieter");
            }
        }
        mtmp = mtmp->nmon;
    }
}

/* Calm nymphs in range. */
static void
calm_nymphs(int distance)
{
    struct monst *mtmp = level->monlist;

    while (mtmp) {
        if (!DEADMONSTER(mtmp) && mtmp->data->mlet == S_NYMPH && mtmp->mcanmove
            && distu(mtmp->mx, mtmp->my) < distance) {
            mtmp->msleeping = 0;
            msethostility(mtmp, FALSE, FALSE);
            mtmp->mavenge = 0;
            if (canseemon(mtmp))
                pline(msgc_actionok,
                      "%s listens cheerfully to the music, then seems quieter.",
                      Monnam(mtmp));
        }
        mtmp = mtmp->nmon;
    }
}

/* Awake only soldiers of the level. */
void
awaken_soldiers(struct monst *culprit)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (!DEADMONSTER(mtmp) && is_mercenary(mtmp->data) &&
            !mx_egd(mtmp)) {
            mtmp->mfrozen = 0;
            msethostility(mtmp, TRUE, FALSE);
            mtmp->mcanmove = 1;
            mtmp->msleeping = 0;
            if (canseemon(mtmp))
                pline(culprit == &youmonst ? msgc_actionok :
                      msgc_moncombatbad, "%s is now ready for battle!",
                      Monnam(mtmp));
            else
                pline_once(msgc_levelsound,
                           "You hear the rattle of battle gear being readied.");
        }
    }
}

/* Charm monsters in range. Note that they may resist the spell.  If swallowed,
   range is reduced to 0. */
static void
charm_monsters(int distance)
{
    struct monst *mtmp, *mtmp2;

    if (Engulfed) {
        if (!resist(&youmonst, u.ustuck, TOOL_CLASS, NOTELL, 0))
            tamedog(u.ustuck, NULL);
    } else {
        for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
            mtmp2 = mtmp->nmon;
            if (DEADMONSTER(mtmp))
                continue;

            if (distu(mtmp->mx, mtmp->my) <= distance) {
                if (!resist(&youmonst, mtmp, TOOL_CLASS, NOTELL, 0))
                    tamedog(mtmp, NULL);
            }
        }
    }

}

/* Generate earthquake :-) of desired force. That is: create random chasms
   (pits). Currently assumes that the player created it (you'll need to change
   at least messages, angering, and kill credit if you generalize it). */
static void
do_earthquake(int force)
{
    int x, y;
    struct monst *mtmp;
    struct obj *otmp;
    struct trap *chasm, *oldtrap;
    int start_x, start_y, end_x, end_y;

    start_x = u.ux - (force * 2);
    start_y = u.uy - (force * 2);
    end_x = u.ux + (force * 2);
    end_y = u.uy + (force * 2);
    if (start_x < 0)
        start_x = 0;
    if (start_y < 0)
        start_y = 0;
    if (end_x >= COLNO)
        end_x = COLNO - 1;
    if (end_y >= ROWNO)
        end_y = ROWNO - 1;
    for (x = start_x; x <= end_x; x++)
        for (y = start_y; y <= end_y; y++) {
            if ((mtmp = m_at(level, x, y)) != 0) {
                wakeup(mtmp, FALSE);  /* peaceful monster will become hostile */
                if (mtmp->mundetected && is_hider(mtmp->data)) {
                    mtmp->mundetected = 0;
                    if (cansee(x, y))
                        pline(msgc_youdiscover, "%s is shaken loose from %s!",
                              Amonnam(mtmp), mtmp->data == &mons[PM_TRAPPER] ?
                              "its hiding place" : the(ceiling(u.ux, u.uy)));
                    else
                        You_hear(msgc_levelsound, "a thumping sound.");
                    if (x == u.ux && y == u.uy &&
                        mtmp->data != &mons[PM_TRAPPER])
                        pline(msgc_moncombatgood,
                              "You easily dodge the falling %s.",
                              mon_nam(mtmp));
                    newsym(x, y);
                }
            }
            if (!rn2(14 - force))
                switch (level->locations[x][y].typ) {
                case FOUNTAIN: /* Make the fountain disappear */
                    if (cansee(x, y))
                        pline(msgc_consequence,
                              "The fountain falls into a chasm.");
                    goto do_pit;
                case SINK:
                    if (cansee(x, y))
                        pline(msgc_consequence,
                              "The kitchen sink falls into a chasm.");
                    goto do_pit;
                case ALTAR:
                    if (level->locations[x][y].flags & AM_SANCTUM)
                        break;

                    if (cansee(x, y))
                        pline(msgc_consequence,
                              "The altar falls into a chasm.");
                    goto do_pit;
                case GRAVE:
                    if (cansee(x, y))
                        pline(msgc_consequence,
                              "The headstone topples into a chasm.");
                    goto do_pit;
                case THRONE:
                    if (cansee(x, y))
                        pline(msgc_consequence,
                              "The throne falls into a chasm.");
                    /* Falls into next case */
                case ROOM:
                case CORR:     /* Try to make a pit */
                    /* Pits, spiked pits, holes, trapdoors, vibrating squares,
                       magic portals are immune.  A bear trap will leave the
                       trap in the pit.  It would be kind of cool to make
                       landmines detonate, but that's more trouble than it's
                       worth. */
                    if ((oldtrap = t_at(level, x, y))) {
                        if (oldtrap->ttyp == PIT || oldtrap->ttyp == SPIKED_PIT
                            || oldtrap->ttyp == HOLE ||
                            oldtrap->ttyp == TRAPDOOR ||
                            oldtrap->ttyp == VIBRATING_SQUARE ||
                            oldtrap->ttyp == MAGIC_PORTAL)
                            break;

                        if (oldtrap->ttyp == BEAR_TRAP) {
                            if (mtmp)
                                mtmp->mtrapped = 0;
                            cnv_trap_obj(level, BEARTRAP, 1, oldtrap);
                        }
                    }

                do_pit:
                    chasm = maketrap(level, x, y, PIT, rng_main);
                    if (!chasm)
                        break;  /* no pit if portal at that location */
                    chasm->tseen = 1;

                    level->locations[x][y].flags = 0;

                    mtmp = m_at(level, x, y);

                    if ((otmp = sobj_at(BOULDER, level, x, y)) != 0) {
                        if (cansee(x, y))
                            pline(msgc_consequence,
                                  "KADOOM! The boulder falls into a chasm%s!",
                                  ((x == u.ux) &&
                                   (y == u.uy)) ? " below you" : "");
                        if (mtmp)
                            mtmp->mtrapped = 0;
                        obj_extract_self(otmp);
                        flooreffects(otmp, x, y, "");
                        break;
                    }

                    /* We have to check whether monsters or player falls in a
                       chasm... */

                    if (mtmp) {
                        if (!flying(mtmp) && !levitates(mtmp) &&
                            !is_clinger(mtmp->data)) {
                            mtmp->mtrapped = 1;
                            if (cansee(x, y))
                                pline(combat_msgc(&youmonst, mtmp, cr_hit),
                                      "%s falls into a chasm!", Monnam(mtmp));
                            else if (humanoid(mtmp->data))
                                You_hear(msgc_levelsound, "a scream!");
                            mselftouch(mtmp, "Falling, ", &youmonst);
                            if (!DEADMONSTER(mtmp))
                                if ((mtmp->mhp -= rnd(6)) <= 0) {
                                    if (!cansee(x, y))
                                        pline(msgc_kill, "It is destroyed!");
                                    else {
                                        pline(msgc_petfatal, "You destroy %s!",
                                              mtmp->mtame ?
                                              x_monnam(mtmp, ARTICLE_THE,
                                                       "poor", mx_name(mtmp) ?
                                                       SUPPRESS_SADDLE : 0,
                                                       FALSE) : mon_nam(mtmp));
                                    }
                                    xkilled(mtmp, 0);
                                }
                        }
                    } else if (!u.utrap && x == u.ux && y == u.uy) {
                        if (Levitation || Flying || is_clinger(youmonst.data)) {
                            pline(msgc_noconsequence,
                                  "A chasm opens up under you!");
                            pline(msgc_noconsequence, "You don't fall in!");
                        } else {
                            pline(msgc_badidea, "You fall into a chasm!");
                            u.utrap = rn1(6, 2);
                            u.utraptype = TT_PIT;
                            turnstate.vision_full_recalc = TRUE;
                            losehp(rnd(6), "fell into a chasm");
                            selftouch("Falling, you",
                                      "falling into a chasm while wielding");
                        }
                    } else
                        newsym(x, y);
                    break;
                case DOOR:     /* Make the door collapse */
                    if (level->locations[x][y].flags == D_NODOOR)
                        goto do_pit;
                    if (cansee(x, y))
                        pline(msgc_consequence, "The door collapses.");
                    if (*in_rooms(level, x, y, SHOPBASE))
                        add_damage(x, y, 0L);
                    level->locations[x][y].flags = D_NODOOR;
                    unblock_point(x, y);
                    newsym(x, y);
                    break;
                }
        }
}

/* The player is trying to extract something from his/her instrument. */
static int
do_improvisation(struct obj *instr, const struct nh_cmd_arg *arg)
{
    int do_spec = !Confusion;

    if (!do_spec)
        pline(msgc_yafm, "What you produce is quite far from music...");
    else
        pline(msgc_occstart, "You start playing %s.", the(xname(instr)));

    switch (instr->otyp) {
    case MAGIC_FLUTE:  /* Make monster fall asleep */
        if (do_spec && instr->spe > 0) {
            consume_obj_charge(instr, TRUE);

            pline(msgc_actionok, "You produce soft music.");
            put_monsters_to_sleep(u.ulevel * 5);
            exercise(A_DEX, TRUE);
            break;
        }       /* else FALLTHRU */
    case WOODEN_FLUTE: /* May charm snakes */
        do_spec &= (rn2(ACURR(A_DEX)) + u.ulevel > 25);
        pline(do_spec ? msgc_actionok : msgc_failrandom, "%s.",
              Tobjnam(instr, do_spec ? "trill" : "toot"));
        if (do_spec)
            charm_snakes(u.ulevel * 3);
        exercise(A_DEX, TRUE);
        break;
    case FROST_HORN:   /* Idem wand of cold */
    case FIRE_HORN:    /* Idem wand of fire */
        if (do_spec && instr->spe > 0) {
            schar dx, dy, dz;

            consume_obj_charge(instr, TRUE);

            if (!getargdir(arg, NULL, &dx, &dy, &dz)) {
                pline(msgc_yafm, "%s.", Tobjnam(instr, "vibrate"));
                break;
            } else {
                buzz((instr->otyp == FROST_HORN) ? AD_COLD - 1 : AD_FIRE - 1,
                     rn1(6, 6), u.ux, u.uy, dx, dy, 0);
            }
            makeknown(instr->otyp);
            break;
        }       /* else FALLTHRU */
    case TOOLED_HORN:  /* Awaken or scare monsters */
        pline(msgc_actionok, "You produce a frightful, grave sound.");
        awaken_monsters(&youmonst, u.ulevel * 30);
        exercise(A_WIS, FALSE);
        break;
    case BUGLE:        /* Awaken & attract soldiers */
        pline(msgc_actionok, "You extract a loud noise from %s.",
              the(xname(instr)));
        awaken_soldiers(&youmonst);
        exercise(A_WIS, FALSE);
        break;
    case MAGIC_HARP:   /* Charm monsters */
        if (do_spec && instr->spe > 0) {
            consume_obj_charge(instr, TRUE);

            pline(msgc_actionok, "%s very attractive music.",
                  Tobjnam(instr, "produce"));
            charm_monsters((u.ulevel - 1) / 3 + 1);
            exercise(A_DEX, TRUE);
            break;
        }       /* else FALLTHRU */
    case WOODEN_HARP:  /* May calm Nymph */
        do_spec &= (rn2(ACURR(A_DEX)) + u.ulevel > 25);
        pline(do_spec ? msgc_actionok : msgc_failrandom,
              "%s %s.", The(xname(instr)),
              do_spec ? "produces a lilting melody" : "twangs");
        if (do_spec)
            calm_nymphs(u.ulevel * 3);
        exercise(A_DEX, TRUE);
        break;
    case DRUM_OF_EARTHQUAKE:   /* create several pits */
        if (do_spec && instr->spe > 0) {
            consume_obj_charge(instr, TRUE);

            pline(msgc_occstart, "You produce a heavy, thunderous rolling!");
            pline_implied(msgc_occstart,
                          "The entire dungeon is shaking around you!");
            do_earthquake((u.ulevel - 1) / 3 + 1);
            /* shake up monsters in a much larger radius... */
            awaken_monsters(&youmonst, ROWNO * COLNO);
            makeknown(DRUM_OF_EARTHQUAKE);
            break;
        }       /* else FALLTHRU */
    case LEATHER_DRUM: /* Awaken monsters */
        pline(msgc_actionok, "You beat a deafening row!");
        awaken_monsters(&youmonst, u.ulevel * 40);
        exercise(A_WIS, FALSE);
        break;
    default:
        impossible("What a weird instrument (%d)!", instr->otyp);
        break;
    }
    return 2;   /* That takes time */
}

/* Some countries name the notes of the scale A to G. In others, "B" refers
   specifically to B flat, and a B natural is called H. This function places
   both onto the same seven-note scale. */
static char
highc_htob(char c)
{
    c = highc(c);
    return c == 'H' ? 'B' : c;
}

/* So you want music... */
int
do_play_instrument(struct obj *instr, const struct nh_cmd_arg *arg)
{
    char c = 'y';
    const char *buf;
    int x, y;
    boolean ok;

    if (Underwater) {
        pline(msgc_cancelled, "You can't play music underwater!");
        return 0;
    }
    if (Upolyd && !can_blow_instrument(youmonst.data) &&
        (instr->otyp == BUGLE || instr->otyp == WOODEN_FLUTE ||
         instr->otyp == MAGIC_FLUTE || instr->otyp == TOOLED_HORN ||
         instr->otyp == FIRE_HORN || instr->otyp == FROST_HORN)) {
        pline(msgc_cancelled,
              "You are incapable of playing %s in your current form!",
              the(xname(instr)));
        return 0;
    }
    if (instr->otyp != LEATHER_DRUM && instr->otyp != DRUM_OF_EARTHQUAKE) {
        c = yn("Improvise?");
    }
    if (c == 'n') {
        if (u.uevent.uheard_tune == 2 && yn("Play the passtune?") == 'y') {
            buf = msg_from_string(gamestate.castle_tune);
        } else {
            /* Note: This is explicitly not getarglin(); we don't want
               command repeat to repeat the tune. */
            buf = getlin("What tune are you playing? [5 notes, A-G]", FALSE);
            if (*buf == '\033')
                buf = "";
            buf = msgmungspaces(buf);
            /* convert to uppercase and change any "H" to the expected "B" */
            buf = msgcaseconv(buf, highc_htob, highc_htob, highc_htob);
        }
        pline(msgc_occstart, "You extract a strange sound from %s!",
              the(xname(instr)));

        /* Check if there was the Stronghold drawbridge near and if the tune
           conforms to what we're waiting for. */
        if (Is_stronghold(&u.uz)) {
            exercise(A_WIS, TRUE);      /* just for trying */
            if (!strcmp(buf, gamestate.castle_tune)) {
                /* Search for the drawbridge */
                for (y = u.uy - 1; y <= u.uy + 1; y++)
                    for (x = u.ux - 1; x <= u.ux + 1; x++)
                        if (isok(x, y))
                            if (find_drawbridge(&x, &y)) {
                                /* tune now fully known */
                                u.uevent.uheard_tune = 2;
                                if (level->locations[x][y].typ ==
                                    DRAWBRIDGE_DOWN)
                                    close_drawbridge(x, y);
                                else
                                    open_drawbridge(x, y);
                                return 1;
                            }
            } else if (canhear()) {
                if (u.uevent.uheard_tune < 1)
                    u.uevent.uheard_tune = 1;
                /* Okay, it wasn't the right tune, but perhaps we can give the
                   player some hints like in the Mastermind game */
                ok = FALSE;
                for (y = u.uy - 1; y <= u.uy + 1 && !ok; y++)
                    for (x = u.ux - 1; x <= u.ux + 1 && !ok; x++)
                        if (isok(x, y))
                            if (IS_DRAWBRIDGE(level->locations[x][y].typ) ||
                                is_drawbridge_wall(x, y) >= 0)
                                ok = TRUE;
                if (ok) {       /* There is a drawbridge near */
                    int tumblers, gears;
                    boolean matched[5];

                    tumblers = gears = 0;
                    for (x = 0; x < 5; x++)
                        matched[x] = FALSE;

                    for (x = 0; x < (int)strlen(buf); x++)
                        if (x < 5) {
                            if (buf[x] == gamestate.castle_tune[x]) {
                                gears++;
                                matched[x] = TRUE;
                            } else
                                for (y = 0; y < 5; y++)
                                    if (!matched[y] &&
                                        buf[x] == gamestate.castle_tune[y] &&
                                        buf[y] != gamestate.castle_tune[y]) {
                                        tumblers++;
                                        matched[y] = TRUE;
                                        break;
                                    }
                        }
                    if (tumblers)
                        if (gears)
                            You_hear(msgc_hint,
                                     "%d tumbler%s click and %d gear%s turn.",
                                     tumblers, plur(tumblers), gears,
                                     plur(gears));
                        else
                            You_hear(msgc_hint, "%d tumbler%s click.", tumblers,
                                     plur(tumblers));
                    else if (gears) {
                        You_hear(msgc_hint, "%d gear%s turn.", gears,
                                 plur(gears));
                        /* could only get `gears == 5' by playing five correct
                           notes followed by excess; otherwise, tune would have 
                           matched above */
                        if (gears == 5)
                            u.uevent.uheard_tune = 2;
                    }
                }
            }
        }
        return 1;
    } else
        return do_improvisation(instr, arg);
}

/*music.c*/

