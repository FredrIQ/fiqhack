/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static boolean tele_jump_ok(int, int, int, int);
static boolean teleok(int, int, boolean, boolean wizard_tele);
static void vault_tele(void);
static boolean rloc_pos_ok(int, int, struct monst *);
static void mvault_tele(struct monst *);

/*
 * Is (x,y) a good position of mtmp?  If mtmp is NULL, then is (x,y) good
 * for an object?
 *
 * This function will only look at mtmp->mdat, so makemon, mplayer, etc can
 * call it to generate new monster positions with fake monster structures.
 *
 * This function might be called during level generation, so should return
 * deterministic results.
 */
boolean
goodpos(struct level *lev, int x, int y, struct monst *mtmp, unsigned gpflags)
{
    const struct permonst *mdat = NULL;
    boolean ignorewater = ((gpflags & MM_IGNOREWATER) != 0);

    if (!isok(x, y))
        return FALSE;

    /* in many cases, we're trying to create a new monster, which can't go on
       top of the player or any existing monster. however, occasionally we are
       relocating engravings or objects, which could be co-located and thus get
       restricted a bit too much. oh well. */
    if (mtmp != &youmonst && x == u.ux && y == u.uy &&
        (!u.usteed || mtmp != u.usteed) && !(gpflags & MM_IGNOREMONST))
        return FALSE;

    if (mtmp) {
        struct monst *mtmp2 = (gpflags & MM_IGNOREMONST) ?
            NULL : m_at(lev, x, y);

        /* Be careful with long worms.  A monster may be placed back in its own
           location.  Normally, if m_at() returns the same monster that we're
           trying to place, the monster is being placed in its own location.
           However, that is not correct for worm segments, because all the
           segments of the worm return the same m_at(). Actually we overdo the
           check a little bit--a worm can't be placed in its own location,
           period.  If we just checked for mtmp->mx != x || mtmp->my != y, we'd
           miss the case where we're called to place the worm segment and the
           worm's head is at x,y. */
        if (mtmp2 && (mtmp2 != mtmp || mtmp->wormno))
            return FALSE;

        mdat = mtmp->data;
        if (is_pool(lev, x, y) && !ignorewater) {
            if (mtmp == &youmonst)
                return !!(HLevitation || Flying || Wwalking || Swimming ||
                          Amphibious);
            else
                return (is_flyer(mdat) || is_swimmer(mdat) || is_clinger(mdat));
        } else if (mdat->mlet == S_EEL && !ignorewater) {
            return FALSE;
        } else if (is_lava(lev, x, y)) {
            if (mtmp == &youmonst)
                return ! !HLevitation;
            else
                return is_flyer(mdat) || likes_lava(mdat);
        }
        if (IS_STWALL(lev->locations[x][y].typ)) {
            if (passes_walls(mdat) && may_passwall(lev, x, y))
                return TRUE;
            if (gpflags & MM_CHEWROCK && may_dig(lev, x, y))
                return TRUE;
        }
    }
    if (!ACCESSIBLE(lev->locations[x][y].typ)) {
        if (!(is_pool(lev, x, y) && ignorewater))
            return FALSE;
    }

    if (!(gpflags & MM_IGNOREDOORS) && closed_door(lev, x, y) &&
        (!mdat || !amorphous(mdat)))
        return FALSE;
    if (!(gpflags & MM_IGNOREDOORS) && sobj_at(BOULDER, lev, x, y) &&
        (!mdat || !throws_rocks(mdat)))
        return FALSE;
    return TRUE;
}

/*
 * "entity next to"
 *
 * Attempt to find a good place for the given monster type in the closest
 * position to (xx,yy).  Do so in successive square rings around (xx,yy).
 * If there is more than one valid positon in the ring, choose one randomly.
 * Return TRUE and the position chosen when successful, FALSE otherwise.
 *
 * Always uses the main RNG.
 */
boolean
enexto(coord * cc, struct level * lev, xchar xx, xchar yy,
       const struct permonst * mdat)
{
    return enexto_core(cc, lev, xx, yy, mdat, 0);
}

boolean
enexto_core(coord * cc, struct level *lev, xchar xx, xchar yy,
            const struct permonst *mdat, unsigned entflags)
{
#define MAX_GOOD 15
    coord good[MAX_GOOD], *good_ptr;
    int x, y, range, i;
    int xmin, xmax, ymin, ymax;
    struct monst fakemon = zeromonst;   /* dummy monster */

    if (!mdat) {
        /* default to player's original monster type */
        mdat = &mons[u.umonster];
    }

    fakemon.data = mdat;        /* set up for goodpos */
    good_ptr = good;
    range = 1;
    /*
     * Walk around the border of the square with center (xx,yy) and
     * radius range.  Stop when we find at least one valid position.
     */
    do {
        xmin = max(0, xx - range);
        xmax = min(COLNO - 1, xx + range);
        ymin = max(0, yy - range);
        ymax = min(ROWNO - 1, yy + range);

        for (x = xmin; x <= xmax; x++)
            if (goodpos(lev, x, ymin, &fakemon, entflags)) {
                good_ptr->x = x;
                good_ptr->y = ymin;
                /* beware of accessing beyond segment boundaries.. */
                if (good_ptr++ == &good[MAX_GOOD - 1])
                    goto full;
            }
        for (x = xmin; x <= xmax; x++)
            if (goodpos(lev, x, ymax, &fakemon, entflags)) {
                good_ptr->x = x;
                good_ptr->y = ymax;
                /* beware of accessing beyond segment boundaries.. */
                if (good_ptr++ == &good[MAX_GOOD - 1])
                    goto full;
            }
        for (y = ymin + 1; y < ymax; y++)
            if (goodpos(lev, xmin, y, &fakemon, entflags)) {
                good_ptr->x = xmin;
                good_ptr->y = y;
                /* beware of accessing beyond segment boundaries.. */
                if (good_ptr++ == &good[MAX_GOOD - 1])
                    goto full;
            }
        for (y = ymin + 1; y < ymax; y++)
            if (goodpos(lev, xmax, y, &fakemon, entflags)) {
                good_ptr->x = xmax;
                good_ptr->y = y;
                /* beware of accessing beyond segment boundaries.. */
                if (good_ptr++ == &good[MAX_GOOD - 1])
                    goto full;
            }
        range++;

        /* return if we've grown too big (nothing is valid) */
        if (range > ROWNO && range > COLNO)
            return FALSE;
    } while (good_ptr == good);

full:
    i = rn2((int)(good_ptr - good));
    cc->x = good[i].x;
    cc->y = good[i].y;
    return TRUE;
}

/*
 * Check for restricted areas present in some special levels.  (This might
 * need to be augmented to allow deliberate passage in wizard mode, but
 * only for explicitly chosen destinations.)
 */
static boolean
tele_jump_ok(int x1, int y1, int x2, int y2)
{
    if (level->dndest.nlx != COLNO) {
        /* if inside a restricted region, can't teleport outside */
        if (within_bounded_area
            (x1, y1, level->dndest.nlx, level->dndest.nly, level->dndest.nhx,
             level->dndest.nhy) &&
            !within_bounded_area(x2, y2, level->dndest.nlx, level->dndest.nly,
                                 level->dndest.nhx, level->dndest.nhy))
            return FALSE;
        /* and if outside, can't teleport inside */
        if (!within_bounded_area
            (x1, y1, level->dndest.nlx, level->dndest.nly, level->dndest.nhx,
             level->dndest.nhy) &&
            within_bounded_area(x2, y2, level->dndest.nlx, level->dndest.nly,
                                level->dndest.nhx, level->dndest.nhy))
            return FALSE;
    }
    if (level->updest.nlx != COLNO) {        /* ditto */
        if (within_bounded_area
            (x1, y1, level->updest.nlx, level->updest.nly, level->updest.nhx,
             level->updest.nhy) &&
            !within_bounded_area(x2, y2, level->updest.nlx, level->updest.nly,
                                 level->updest.nhx, level->updest.nhy))
            return FALSE;
        if (!within_bounded_area
            (x1, y1, level->updest.nlx, level->updest.nly, level->updest.nhx,
             level->updest.nhy) &&
            within_bounded_area(x2, y2, level->updest.nlx, level->updest.nly,
                                level->updest.nhx, level->updest.nhy))
            return FALSE;
    }
    return TRUE;
}

static boolean
teleok(int x, int y, boolean trapok, boolean wizard_tele)
{
    if (!trapok && t_at(level, x, y))
        return FALSE;
    if (!goodpos(level, x, y, &youmonst, 0))
        return FALSE;
    if (!wizard_tele && !tele_jump_ok(u.ux, u.uy, x, y))
        return FALSE;
    if (!in_out_region(level, x, y))
        return FALSE;
    return TRUE;
}

void
teleds(int nux, int nuy, boolean allow_drag)
{
    boolean ball_active = (Punished &&
                           uball->where != OBJ_FREE), ball_still_in_range =
        FALSE;

    /* If they have to move the ball, then drag if allow_drag is true;
       otherwise they are teleporting, so unplacebc(). If they don't have to
       move the ball, then always "drag" whether or not allow_drag is true,
       because we are calling that function, not to drag, but to move the
       chain.  *However* there are some dumb special cases: 0 0 _X move east
       -----> X_ @ @ These are permissible if teleporting, but not if dragging.
       As a result, drag_ball() needs to know about allow_drag and might end up
       dragging the ball anyway.  Also, drag_ball() might find that dragging
       the ball is completely impossible (ball in range but there's rock in the
       way), in which case it teleports the ball on its own. */
    if (ball_active) {
        if (!carried(uball) && distmin(nux, nuy, uball->ox, uball->oy) <= 2)
            ball_still_in_range = TRUE; /* don't have to move the ball */
        else {
            /* have to move the ball */
            if (!allow_drag || distmin(u.ux, u.uy, nux, nuy) > 1) {
                /* we should not have dist > 1 and allow_drag at the same time,
                   but just in case, we must then revert to teleport. */
                allow_drag = FALSE;
                unplacebc();
            }
        }
    }
    u.utrap = 0;
    u.ustuck = 0;
    u.ux0 = u.ux;
    u.uy0 = u.uy;

    if (hides_under(youmonst.data))
        u.uundetected = OBJ_AT(nux, nuy);
    else if (youmonst.data->mlet == S_EEL)
        u.uundetected = is_pool(level, nux, nuy);
    else {
        u.uundetected = 0;
        /* mimics stop being unnoticed */
        if (youmonst.data->mlet == S_MIMIC)
            youmonst.m_ap_type = M_AP_NOTHING;
    }

    if (Engulfed) {
        u.uswldtim = Engulfed = 0;
        if (Punished && !ball_active) {
            /* ensure ball placement, like unstuck */
            ball_active = TRUE;
            allow_drag = FALSE;
        }
        doredraw();
    }
    if (ball_active) {
        if (ball_still_in_range || allow_drag) {
            int bc_control;
            xchar ballx, bally, chainx, chainy;
            boolean cause_delay;

            if (drag_ball
                (nux, nuy, &bc_control, &ballx, &bally, &chainx, &chainy,
                 &cause_delay, allow_drag))
                move_bc(0, bc_control, ballx, bally, chainx, chainy);
        }
    }
    /* must set u.ux, u.uy after drag_ball(), which may need to know the old
       position if allow_drag is true... */
    u.ux = nux;
    u.uy = nuy;
    fill_pit(level, u.ux0, u.uy0);
    if (ball_active) {
        if (!ball_still_in_range && !allow_drag)
            placebc();
    }
    initrack(); /* teleports mess up tracking monsters without this */
    update_player_regions(level);
    /* Move your steed, too */
    if (u.usteed) {
        u.usteed->mx = nux;
        u.usteed->my = nuy;
    }

    /*
     *  Make sure the hero disappears from the old location.  This will
     *  not happen if she is teleported within sight of her previous
     *  location.  Force a full vision recalculation because the hero
     *  is now in a new location.
     */
    newsym(u.ux0, u.uy0);
    see_monsters(FALSE);
    turnstate.vision_full_recalc = TRUE;
    action_interrupted();
    vision_recalc(0);   /* vision before effects */
    spoteffects(TRUE);
    invocation_message();
}

boolean
safe_teleds(boolean allow_drag)
{
    int nux, nuy, tcnt = 0;

    do {
        nux = rn2(COLNO);
        nuy = rn2(ROWNO);
    } while (!teleok(nux, nuy, (boolean) (tcnt > 200), FALSE) && ++tcnt <= 400);

    if (tcnt <= 400) {
        teleds(nux, nuy, allow_drag);
        return TRUE;
    } else
        return FALSE;
}

static void
vault_tele(void)
{
    struct mkroom *croom = search_special(level, VAULT);
    coord c;

    if (croom && somexy(level, croom, &c, rng_main) &&
        teleok(c.x, c.y, FALSE, FALSE)) {
        teleds(c.x, c.y, FALSE);
        return;
    }
    tele();
}

boolean
teleport_pet(struct monst * mtmp, boolean force_it)
{
    struct obj *otmp;

    if (mtmp == u.usteed)
        return FALSE;

    if (mtmp->mleashed) {
        otmp = get_mleash(mtmp);
        if (!otmp) {
            impossible("%s is leashed, without a leash.", Monnam(mtmp));
            goto release_it;
        }
        if (otmp->cursed && !force_it) {
            yelp(mtmp);
            return FALSE;
        } else {
            pline("Your leash goes slack.");
        release_it:
            m_unleash(mtmp, FALSE);
            return TRUE;
        }
    }
    return TRUE;
}

int
tele(void)
{
    return tele_impl(FALSE, FALSE);
}

int
tele_impl(boolean wizard_tele, boolean run_next_to_u)
{
    coord cc;

    /* Disable teleportation in stronghold && Vlad's Tower */
    if (level->flags.noteleport) {
        if (!wizard_tele) {
            pline("A mysterious force prevents you from teleporting!");
            return 1;
        }
    }

    /* don't show trap if "Sorry..." */
    if (!Blinded)
        make_blinded(0L, FALSE);

    /* when it happens at all, happens too often to be worth a custom RNG */
    if ((Uhave_amulet || On_W_tower_level(&u.uz)) && !rn2(3)) {
        pline("You feel disoriented for a moment.");
        return 1;
    }
    if ((Teleport_control && !Stunned) || wizard_tele) {
        if (u_helpless(hm_unconscious)) {
            pline("Being unconscious, you cannot control your teleport.");
        } else {
            pline("To what position do you%s want to be teleported?",
                  u.usteed ? msgcat(" and ", mon_nam(u.usteed)) : "");
            cc.x = u.ux;
            cc.y = u.uy;
            if (getpos(&cc, FALSE, "the desired position", FALSE)
                == NHCR_CLIENT_CANCEL)
                return 0; /* abort */

            if (run_next_to_u) {
                if (!next_to_u()) {
                    pline("You shudder for a moment.");
                    return 1;
                }
            }

            /* possible extensions: introduce a small error if magic power is
               low; allow transfer to solid rock */
            if (teleok(cc.x, cc.y, FALSE, wizard_tele)) {
                teleds(cc.x, cc.y, FALSE);
                return 1;
            }
            pline("Sorry...");
        }
    }

    safe_teleds(FALSE);
    return 1;
}

/* TODO: Perhaps work out some way to let controlled teleport in on a
   CMD_ARG_POS, but there are too many codeflow possibilities involved to make
   that easy. For now, if dotele turns into the spell, we copy the argument on
   to the spell-handling function (which currently ignores it), but the other
   possible codepaths just lose it. */
int
dotele(const struct nh_cmd_arg *arg)
{
    struct trap *trap;

    trap = t_at(level, u.ux, u.uy);
    if (trap && (!trap->tseen || trap->ttyp != TELEP_TRAP))
        trap = 0;

    if (trap) {
        if (trap->once) {
            pline("This is a vault teleport, usable once only.");
            if (yn("Jump in?") == 'n')
                trap = 0;
            else {
                deltrap(level, trap);
                newsym(u.ux, u.uy);
            }
        }
        if (trap)
            pline("You %s onto the teleportation trap.",
                  locomotion(youmonst.data, "jump"));
    }
    if (!trap) {
        boolean castit = FALSE;
        int sp_no = 0, energy = 0;

        if (!supernatural_ability_available(SPID_RLOC)) {
            /* Try to use teleport away spell. */
            if (objects[SPE_TELEPORT_AWAY].oc_name_known && !Confusion)
                for (sp_no = 0; sp_no < MAXSPELL; sp_no++)
                    if (spl_book[sp_no].sp_id == SPE_TELEPORT_AWAY) {
                        castit = TRUE;
                        break;
                    }
            if (!castit) {
                if (!Teleportation)
                    pline("You don't know that spell.");
                else
                    pline("You are not able to teleport at will.");
                return 0;
            }
        }

        if (u.uhunger <= 100 || ACURR(A_STR) < 6) {
            pline("You lack the strength %s.",
                  castit ? "for a teleport spell" : "to teleport");
            return 1;
        }

        energy = objects[SPE_TELEPORT_AWAY].oc_level * 7 / 2 - 2;
        if (u.uen <= energy) {
            pline("You lack the energy %s.",
                  castit ? "for a teleport spell" : "to teleport");
            return 1;
        }

        if (check_capacity("Your concentration falters from carrying so much."))
            return 1;

        if (castit) {
            exercise(A_WIS, TRUE);
            if (spelleffects(sp_no, TRUE, arg))
                return 1;
            else
                return 0;
        } else
            u.uen -= energy;
    }

    if (trap && trap->once) {
        if (next_to_u())
            vault_tele();
        else
            pline("You shudder for a moment.");
    }

    if (!tele_impl(FALSE, TRUE))
        return 0;
    next_to_u();

    if (!trap)
        morehungry(100);
    return 1;
}


void
level_tele(void)
{
    level_tele_impl(FALSE);
}

void
level_tele_impl(boolean wizard_tele)
{
    int newlev;
    d_level newlevel;
    const char *escape_by_flying = 0;   /* when surviving dest of -N */
    boolean force_dest = FALSE;
    const char *buf, *killer = NULL;

    if ((Uhave_amulet || In_endgame(&u.uz) || In_sokoban(&u.uz))
        && !wizard_tele) {
        pline("You feel very disoriented for a moment.");
        return;
    }
    if ((Teleport_control && !Stunned) || wizard_tele) {
        int trycnt = 0;
        const char *qbuf = "To what level do you want to teleport?";
        do {
            if (++trycnt == 2) {
                if (wizard_tele)
                    qbuf = msgcat(qbuf, " [type a number or ? for a menu]");
                else
                    qbuf = msgcat(qbuf, " [type a number]");
            }
            buf = getlin(qbuf, FALSE);
            if (!strcmp(buf, "\033")) { /* cancelled */
                if (Confusion && rnl(5)) {
                    pline("Oops...");
                    goto random_levtport;
                }
                return;
            } else if (!strcmp(buf, "*")) {
                goto random_levtport;
            } else if (Confusion && rnl(5)) {
                pline("Oops...");
                goto random_levtport;
            }

            if (wizard_tele && !strcmp(buf, "?")) {
                schar destlev = 0;
                xchar destdnum = 0;

                if ((newlev = (int)print_dungeon(TRUE, &destlev, &destdnum))) {
                    newlevel.dnum = destdnum;
                    newlevel.dlevel = destlev;
                    if (In_endgame(&newlevel) && !In_endgame(&u.uz)) {
                        const char *dest = "Destination is earth level";
                        if (!Uhave_amulet) {
                            struct obj *obj;

                            obj = mksobj(level, AMULET_OF_YENDOR, TRUE, FALSE,
                                         rng_main);
                            if (obj) {
                                addinv(obj);
                                dest = msgcat(dest, " with the amulet");
                            }
                        }
                        assign_level(&newlevel, &earth_level);
                        pline("%s.", dest);
                    }
                    force_dest = TRUE;
                } else
                    return;
            } else if ((newlev = lev_by_name(buf)) == 0)
                newlev = atoi(buf);
        } while (!newlev && !digit(buf[0]) && (buf[0] != '-' || !digit(buf[1]))
                 && trycnt < 10);

        /* no dungeon escape via this route */
        if (newlev == 0) {
            if (trycnt >= 10)
                goto random_levtport;
            if (ynq("Go to Nowhere.  Are you sure?") != 'y')
                return;
            pline("You %s in agony as your body begins to warp...",
                  is_silent(youmonst.data) ? "writhe" : "scream");
            win_pause_output(P_MESSAGE);
            pline("You cease to exist.");
            if (invent)
                pline("Your possessions land on the %s with a thud.",
                      surface(u.ux, u.uy));
            done(DIED, "committed suicide");
            pline("An energized cloud of dust begins to coalesce.");
            pline("Your body rematerializes%s.",
                  invent ? ", and you gather up all your possessions" : "");
            return;
        }

        /* if in Knox and the requested level > 0, stay put. we let negative
           values requests fall into the "heaven" loop. */
        if (Is_knox(&u.uz) && newlev > 0) {
            pline("You shudder for a moment.");
            return;
        }
        /* if in Quest, the player sees "Home 1", etc., on the status line,
           instead of the logical depth of the level.  controlled level
           teleport request is likely to be relativized to the status line, and
           consequently it should be incremented to the value of the logical
           depth of the target level. we let negative values requests fall into
           the "heaven" loop. */
        if (In_quest(&u.uz) && newlev > 0)
            newlev = newlev + find_dungeon(&u.uz).depth_start - 1;
    } else {    /* involuntary level tele */
    random_levtport:
        newlev = random_teleport_level();
        if (newlev == depth(&u.uz)) {
            pline("You shudder for a moment.");
            return;
        }
    }

    if (!next_to_u()) {
        pline("You shudder for a moment.");
        return;
    }

    if (In_endgame(&u.uz)) {    /* must already be wizard */
        int llimit = dunlevs_in_dungeon(&u.uz);

        if (newlev >= 0 || newlev <= -llimit) {
            pline("You can't get there from here.");
            return;
        }
        newlevel.dnum = u.uz.dnum;
        newlevel.dlevel = llimit + newlev;
        schedule_goto(&newlevel, FALSE, FALSE, 0, NULL, NULL);
        return;
    }

    if (newlev < 0 && !force_dest) {
        if (*u.ushops0) {
            /* take unpaid inventory items off of shop bills */
            in_mklev = TRUE;    /* suppress map update */
            u_left_shop(u.ushops0, TRUE);
            /* you're now effectively out of the shop */
            *u.ushops0 = *u.ushops = '\0';
            in_mklev = FALSE;
        }
        if (newlev <= -10) {
            pline("You arrive in heaven.");
            verbalize("Thou art early, but we'll admit thee.");
            killer = "went to heaven prematurely";
        } else if (newlev == -9) {
            pline("You feel deliriously happy. ");
            pline("(In fact, you're on Cloud 9!) ");
            win_pause_output(P_MESSAGE);
        } else
            pline("You are now high above the clouds...");

        if (killer) {
            ;   /* arrival in heaven is pending */
        } else if (Levitation) {
            escape_by_flying = "float gently down to earth";
        } else if (Flying) {
            escape_by_flying = "fly down to the ground";
        } else {
            pline("Unfortunately, you don't know how to fly.");
            pline("You plummet a few thousand feet to your death.");
            killer = msgcat_many("teleported out of the dungeon and fell to ",
                                 uhis(), " death", NULL);
        }
    }

    if (killer) {       /* the chosen destination was not survivable */
        d_level lsav;

        /* set specific death location; this also suppresses bones */
        lsav = u.uz;    /* save current level, see below */
        u.uz.dnum = 0;  /* main dungeon */
        u.uz.dlevel = (newlev <= -10) ? -10 : 0;        /* heaven or surface */
        done(DIED, killer);
        /* can only get here via life-saving (or declining to die in
           explore|debug mode); the hero has now left the dungeon... */
        escape_by_flying = "find yourself back on the surface";
        u.uz = lsav;    /* restore u.uz so escape code works */
    }

    /* calls done(ESCAPED) if newlevel==0 */
    if (escape_by_flying) {
        pline("You %s.", escape_by_flying);
        done(ESCAPED, "teleported to safety");
    } else if (u.uz.dnum == medusa_level.dnum &&
               newlev >= (find_dungeon(&u.uz).depth_start +
                          dunlevs_in_dungeon(&u.uz))) {
        if (!(wizard_tele && force_dest))
            find_hell(&newlevel);
    } else {
        /* if invocation did not yet occur, teleporting into the last level of
           Gehennom is forbidden. */
        if (!wizard_tele)
            if (Inhell && !u.uevent.invoked &&
                newlev >= (find_dungeon(&u.uz).depth_start +
                           dunlevs_in_dungeon(&u.uz) - 1)) {
                newlev = (find_dungeon(&u.uz).depth_start +
                          dunlevs_in_dungeon(&u.uz) - 2);
                pline("Sorry...");
            }
        /* no teleporting out of quest dungeon */
        if (In_quest(&u.uz) && newlev < depth(&qstart_level))
            newlev = depth(&qstart_level);
        /* the player thinks of levels purely in logical terms, so we must
           translate newlev to a number relative to the current dungeon. */
        if (!(wizard_tele && force_dest))
            get_level(&newlevel, newlev);
    }
    schedule_goto(&newlevel, FALSE, FALSE, 0, NULL, NULL);
    /* in case player just read a scroll and is about to be asked to call it
       something, we can't defer until the end of the turn */
    if (!flags.mon_moving)
        deferred_goto();
}

void
domagicportal(struct trap *ttmp)
{
    struct d_level target_level;

    if (!next_to_u()) {
        pline("You shudder for a moment.");
        return;
    }

    pline("You activated a magic portal!");

    /* prevent the poor shnook, whose amulet was stolen while in the endgame,
       from accidently triggering the portal to the next level, and thus losing
       the game */
    if (In_endgame(&u.uz) && !Uhave_amulet) {
        pline("You feel dizzy for a moment, but nothing happens...");
        return;
    }

    target_level = ttmp->dst;
    schedule_goto(&target_level, FALSE, FALSE, 1,
                  "You feel dizzy for a moment, but the sensation passes.",
                  NULL);
}

void
tele_trap(struct trap *trap)
{
    if (In_endgame(&u.uz) || Antimagic) {
        if (Antimagic)
            shieldeff(u.ux, u.uy);
        pline("You feel a wrenching sensation.");
    } else if (!next_to_u()) {
        pline("You shudder for a moment.");
    } else if (trap->once) {
        deltrap(level, trap);
        newsym(u.ux, u.uy);     /* get rid of trap symbol */
        vault_tele();
    } else
        tele();
}

void
level_tele_trap(struct trap *trap)
{
    pline("You %s onto a level teleport trap!",
          Levitation ? (const char *)"float" :
          locomotion(youmonst.data, "step"));
    if (Antimagic) {
        shieldeff(u.ux, u.uy);
    }
    if (Antimagic || In_endgame(&u.uz)) {
        pline("You feel a wrenching sensation.");
        return;
    }
    if (!Blind)
        pline("You are momentarily blinded by a flash of light.");
    else
        pline("You are momentarily disoriented.");
    deltrap(level, trap);
    newsym(u.ux, u.uy); /* get rid of trap symbol */
    level_tele();
}

/* check whether monster can arrive at location <x,y> via Tport (or fall) */
static boolean
rloc_pos_ok(int x, int y,       /* coordinates of candidate location */
            struct monst *mtmp)
{
    int xx, yy;

    if (!goodpos(level, x, y, mtmp, 0))
        return FALSE;
    /*
     * Check for restricted areas present in some special levels.
     *
     * `xx' is current column; if 0, then `yy' will contain flag bits
     * rather than row:  bit #0 set => moving upwards; bit #1 set =>
     * inside the Wizard's tower.
     */
    xx = mtmp->mx;
    yy = mtmp->my;
    if (!xx) {
        /* no current location (migrating monster arrival) */
        if (level->dndest.nlx && On_W_tower_level(&u.uz))
            return ((yy & 2) != 0) ^    /* inside xor not within */
                !within_bounded_area(x, y, level->dndest.nlx, level->dndest.nly,
                                     level->dndest.nhx, level->dndest.nhy);
        if (level->updest.lx && (yy & 1) != COLNO)  /* moving up */
            return (within_bounded_area
                    (x, y, level->updest.lx, level->updest.ly,
                     level->updest.hx, level->updest.hy) &&
                    (!level->updest.nlx ||
                     !within_bounded_area(
                       x, y, level->updest.nlx, level->updest.nly,
                       level->updest.nhx, level->updest.nhy)));
        if (level->dndest.lx && (yy & 1) == COLNO)  /* moving down */
            return (within_bounded_area
                    (x, y, level->dndest.lx, level->dndest.ly,
                     level->dndest.hx, level->dndest.hy) &&
                    (!level->dndest.nlx ||
                     !within_bounded_area(
                         x, y, level->dndest.nlx, level->dndest.nly,
                         level->dndest.nhx, level->dndest.nhy)));
    } else {
        /* current location is <xx,yy> */
        if (!tele_jump_ok(xx, yy, x, y))
            return FALSE;
    }
    /* <x,y> is ok */
    return TRUE;
}

/*
 * rloc_to()
 *
 * Pulls a monster from its current position and places a monster at a new x and
 * y.  If oldx is COLNO, then the monster was not in the levels.monsters array.
 * However, if oldx is COLNO, oldy may still have a value because mtmp is a
 * migrating_mon.  Worm tails are always placed randomly around the head of the
 * worm.  This only works for monsters on the current level.
 */
void
rloc_to(struct monst *mtmp, int x, int y)
{
    int oldx = mtmp->mx, oldy = mtmp->my;
    boolean resident_shk = mtmp->isshk && inhishop(mtmp);

    if (x == mtmp->mx && y == mtmp->my) /* that was easy */
        return;

    if (oldx != COLNO) { /* "pick up" monster */
        if (mtmp->wormno)
            remove_worm(mtmp);
        else {
            remove_monster(level, oldx, oldy);
            newsym(oldx, oldy); /* update old location */
        }
    }

    place_monster(mtmp, x, y);  /* put monster down */
    update_monster_region(mtmp);

    if (mtmp->wormno)   /* now put down tail */
        place_worm_tail_randomly(mtmp, x, y, rng_main);

    if (u.ustuck == mtmp) {
        if (Engulfed) {
            u.ux = x;
            u.uy = y;
            doredraw();
        } else
            u.ustuck = 0;
    }

    newsym(x, y);       /* update new location */
    set_apparxy(mtmp);  /* orient monster */

    /* In some cases involving migration, the player and monster are currently
       on the same square. One of them will move, but we don't want the monster
       to have itself in its muxy. */
    if (mtmp->mux == mtmp->mx && mtmp->muy == mtmp->my) {
        mtmp->mux = COLNO;
        mtmp->muy = ROWNO;
    }

    /* shopkeepers will only teleport if you zap them with a wand of
       teleportation or if they've been transformed into a jumpy monster; the
       latter only happens if you've attacked them with polymorph */
    if (resident_shk && !inhishop(mtmp))
        make_angry_shk(mtmp, oldx, oldy);
}

/* place a monster at a random location, typically due to teleport */
/* return TRUE if successful, FALSE if not */
boolean
rloc(struct monst *mtmp,        /* mx==COLNO implies migrating monster arrival */
     boolean suppress_impossible)
{
    int x, y, trycount;
    int relaxed_goodpos;

    if (mtmp == u.usteed) {
        tele();
        return TRUE;
    }

    if (mtmp->iswiz && mtmp->mx != COLNO) {      /* Wizard, not just arriving */
        if (!In_W_tower(u.ux, u.uy, &u.uz))
            x = level->upstair.sx, y = level->upstair.sy;
        else if (!isok(level->dnladder.sx, level->dnladder.sy))
            x = level->upladder.sx, y = level->upladder.sy;/* bottom of tower */
        else
            x = level->dnladder.sx, y = level->dnladder.sy;
        /* if the wiz teleports away to heal, try the up staircase, to block
           the player's escaping before he's healed (deliberately use `goodpos'
           rather than `rloc_pos_ok' here) */
        if (goodpos(level, x, y, mtmp, 0))
            goto found_xy;
    }

    for (relaxed_goodpos = 0; relaxed_goodpos < 2; relaxed_goodpos++) {

        /* first try sensible terrain; if none exists, ignore water,
           doors and boulders */
        int gpflags = relaxed_goodpos ? MM_IGNOREWATER | MM_IGNOREDOORS : 0;

        /* try several pairs of positions; try the more restrictive rloc_pos_ok
           before we use the less restrictive goodpos */
        trycount = 0;
        do {
            x = rn2(COLNO);
            y = rn2(ROWNO);
            if ((trycount < 500) ? rloc_pos_ok(x, y, mtmp)
                : goodpos(level, x, y, mtmp, gpflags))
                goto found_xy;
        } while (++trycount < 1000);

        /* try every square on the level as a fallback */
        for (x = 0; x < COLNO; x++)
            for (y = 0; y < ROWNO; y++)
                if (goodpos(level, x, y, mtmp, gpflags))
                    goto found_xy;
    }

    /* level either full of monsters or somehow faulty */
    if (!suppress_impossible)
        impossible("rloc(): couldn't relocate monster");
    return FALSE;

found_xy:
    rloc_to(mtmp, x, y);
    return TRUE;
}

static void
mvault_tele(struct monst *mtmp)
{
    struct mkroom *croom = search_special(level, VAULT);
    coord c;

    if (croom && somexy(level, croom, &c, rng_main) &&
        goodpos(level, c.x, c.y, mtmp, 0)) {
        rloc_to(mtmp, c.x, c.y);
        return;
    }
    rloc(mtmp, TRUE);
}

boolean
tele_restrict(struct monst * mon)
{
    if (level->flags.noteleport) {
        if (canseemon(mon))
            pline("A mysterious force prevents %s from teleporting!",
                  mon_nam(mon));
        return TRUE;
    }
    return FALSE;
}

void
mtele_trap(struct monst *mtmp, struct trap *trap, int in_sight)
{
    const char *monname;

    if (tele_restrict(mtmp))
        return;
    if (teleport_pet(mtmp, FALSE)) {
        /* save name with pre-movement visibility */
        monname = Monnam(mtmp);

        /* Note: don't remove the trap if a vault.  Otherwise the monster will
           be stuck there, since the guard isn't going to come for it... */
        if (trap->once)
            mvault_tele(mtmp);
        else
            rloc(mtmp, TRUE);

        if (in_sight) {
            if (canseemon(mtmp))
                pline("%s seems disoriented.", monname);
            else
                pline("%s suddenly disappears!", monname);
            seetrap(trap);
        }
    }
}

/* return 0 if still on level, 3 if not */
int
mlevel_tele_trap(struct monst *mtmp, struct trap *trap, boolean force_it,
                 int in_sight)
{
    int tt = trap->ttyp;
    const struct permonst *mptr = mtmp->data;

    if (mtmp == u.ustuck)       /* probably a vortex */
        return 0;       /* temporary? kludge */
    if (teleport_pet(mtmp, force_it)) {
        d_level tolevel;
        int migrate_typ = MIGR_RANDOM;

        if ((tt == HOLE || tt == TRAPDOOR)) {
            if (Is_stronghold(&u.uz)) {
                assign_level(&tolevel, &valley_level);
            } else if (Is_botlevel(&u.uz)) {
                if (in_sight && trap->tseen)
                    pline("%s avoids the %s.", Monnam(mtmp),
                          (tt == HOLE) ? "hole" : "trap");
                return 0;
            } else {
                get_level(&tolevel, depth(&u.uz) + 1);
            }
        } else if (tt == MAGIC_PORTAL) {
            if (In_endgame(&u.uz) &&
                (mon_has_amulet(mtmp) ||
                 is_home_elemental(&mtmp->dlevel->z, mptr))) {
                if (in_sight && mptr->mlet != S_ELEMENTAL) {
                    pline("%s seems to shimmer for a moment.", Monnam(mtmp));
                    seetrap(trap);
                }
                return 0;
            } else {
                assign_level(&tolevel, &trap->dst);
                migrate_typ = MIGR_PORTAL;
            }
        } else {        /* (tt == LEVEL_TELEP) */
            int nlev;

            if (mon_has_amulet(mtmp) || In_endgame(&u.uz)) {
                if (in_sight)
                    pline("%s seems very disoriented for a moment.",
                          Monnam(mtmp));
                return 0;
            }
            nlev = random_teleport_level();
            if (nlev == depth(&u.uz)) {
                if (in_sight)
                    pline("%s shudders for a moment.", Monnam(mtmp));
                return 0;
            }
            get_level(&tolevel, nlev);
        }

        if (in_sight) {
            pline("Suddenly, %s disappears out of sight.", mon_nam(mtmp));
            seetrap(trap);
        }
        migrate_to_level(mtmp, ledger_no(&tolevel), migrate_typ, NULL);
        return 3;       /* no longer on this level */
    }
    return 0;
}

void
rloco_pos(struct level *lev, struct obj *obj, int *nx, int *ny)
{
    xchar tx, ty, otx;
    boolean restricted_fall;
    int try_limit = 4000;

    otx = obj->ox;
    restricted_fall = (otx == -1 && lev->dndest.lx);
    do {
        tx = rn2(COLNO);
        ty = rn2(ROWNO);
        if (!--try_limit)
            break;
    } while (!goodpos(lev, tx, ty, NULL, 0) ||
             /* bug: this lacks provision for handling the Wizard's tower */
             (restricted_fall &&
              (!within_bounded_area
               (tx, ty, lev->dndest.lx, lev->dndest.ly, lev->dndest.hx,
                lev->dndest.hy) ||
               (level->dndest.nlx &&
                within_bounded_area(tx, ty,
                                    lev->dndest.nlx, lev->dndest.nly,
                                    lev->dndest.nhx, lev->dndest.nhy)))));

    *nx = tx;
    *ny = ty;
}

void
rloco(struct obj *obj)
{
    int tx, ty, otx, oty;

    otx = obj->ox;
    oty = obj->oy;

    if (obj->otyp == CORPSE && is_rider(&mons[obj->corpsenm])) {
        if (revive_corpse(obj))
            return;
    }

    obj_extract_self(obj);
    rloco_pos(level, obj, &tx, &ty);

    if (flooreffects(obj, tx, ty, "fall")) {
        return;
    } else if (otx == 0 && oty == 0) {
        ;       /* fell through a trap door; no update of old loc needed */
    } else {
        if (costly_spot(otx, oty)
            && (!costly_spot(tx, ty) ||
                !strchr(in_rooms(level, tx, ty, 0),
                        *in_rooms(level, otx, oty, 0)))) {
            if (costly_spot(u.ux, u.uy) &&
                strchr(u.urooms, *in_rooms(level, otx, oty, 0)))
                addtobill(obj, FALSE, FALSE, FALSE);
            else
                stolen_value(obj, otx, oty, FALSE, FALSE);
        }
        newsym(otx, oty);       /* update old location */
    }
    place_object(obj, level, tx, ty);
    newsym(tx, ty);
}

/* Returns an absolute depth */
int
random_teleport_level(void)
{
    int nlev, max_depth, min_depth, cur_depth = (int)depth(&u.uz);

    if (Is_knox(&u.uz) || !rn2_on_rng(5, rng_levport_results))
        return cur_depth;

    if (In_endgame(&u.uz))      /* only happens in wizmode */
        return cur_depth;

    /*
     * What I really want to do is as follows:
     * -- If in a dungeon that goes down, the new level is to be restricted to
     *    [top of parent, bottom of current dungeon]
     * -- If in a dungeon that goes up, the new level is to be restricted to
     *    [top of current dungeon, bottom of parent]
     * -- If in a quest dungeon or similar dungeon entered by portals, the new
     *    level is to be restricted to [top of current dungeon, bottom of current
     *    dungeon]
     *
     * The current behavior is not as sophisticated as that ideal, but is still
     * better what we used to do, which was like this for players but different
     * for monsters for no obvious reason.  Currently, we must explicitly check
     * for special dungeons.  We check for Knox above; endgame is handled in the
     * caller due to its different message ("disoriented").
     *
     * -- KAA 3.4.2: explicitly handle quest here too, to fix the problem of
     * monsters sometimes level teleporting out of it into main dungeon. Also
     * prevent monsters reaching the Sanctum prior to invocation.
     */
    min_depth = In_quest(&u.uz) ? find_dungeon(&u.uz).depth_start : 1;
    max_depth =
        dunlevs_in_dungeon(&u.uz) + (find_dungeon(&u.uz).depth_start - 1);
    /* can't reach the Sanctum if the invocation hasn't been performed */
    if (Inhell && !u.uevent.invoked)
        max_depth -= 1;

    /* Get a random value relative to the current dungeon */
    /* Range is 1 to current+3, current not counting */
    nlev = rn2_on_rng(cur_depth + 3 - min_depth, rng_levport_results) +
        min_depth;
    if (nlev >= cur_depth)
        nlev++;

    if (nlev > max_depth) {
        nlev = max_depth;
        /* teleport up if already on bottom */
        if (Is_botlevel(&u.uz))
            nlev -= rnd(3);
    }
    if (nlev < min_depth) {
        nlev = min_depth;
        if (nlev == cur_depth) {
            nlev += rnd(3);
            if (nlev > max_depth)
                nlev = max_depth;
        }
    }
    return nlev;
}

/* you teleport a monster (via wand, spell, or poly'd q.mechanic attack);
   return false iff the attempt fails */
boolean
u_teleport_mon(struct monst * mtmp, boolean give_feedback)
{
    coord cc;

    if (mtmp->ispriest && *in_rooms(level, mtmp->mx, mtmp->my, TEMPLE)) {
        if (give_feedback)
            pline("%s resists your magic!", Monnam(mtmp));
        return FALSE;
    } else if (level->flags.noteleport && Engulfed && mtmp == u.ustuck) {
        if (give_feedback)
            pline("You are no longer inside %s!", mon_nam(mtmp));
        unstuck(mtmp);
        rloc(mtmp, FALSE);
    } else if (is_rider(mtmp->data) && rn2(13) &&
               enexto(&cc, level, u.ux, u.uy, mtmp->data)) {
        rloc_to(mtmp, cc.x, cc.y);
    } else
        return rloc(mtmp, TRUE);
    return TRUE;
}

/*teleport.c*/
