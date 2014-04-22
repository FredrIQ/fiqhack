/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-04-22 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* To anyone editing this file: please write anything involving a sizeof
 * as late as possible, so savemap.pl can generate a useful mapping file.
 * In general, a long list of writes such as that found in save_flags should
 * write the tag first, then do things that can be done with the mwrite##
 * family, and only then start using mwrite. */

#include "hack.h"
#include "lev.h"
#include "quest.h"

static void save_you(struct memfile *mf, struct you *you);
static void save_utracked(struct memfile *mf, struct you *you);
static void savelevchn(struct memfile *mf);
static void savedamage(struct memfile *mf, struct level *lev);
static void freedamage(struct level *lev);
static void saveobjchn(struct memfile *mf, struct obj *);
static void free_objchn(struct obj *otmp);
static void savemonchn(struct memfile *mf, struct monst *);
static void free_monchn(struct monst *mon);
static void savetrapchn(struct memfile *mf, struct trap *, struct level *lev);
static void freetrapchn(struct trap *trap);
static void savegamestate(struct memfile *mf);
static void save_flags(struct memfile *mf);
static void save_autopickup_rules(struct memfile *mf,
                                  struct nh_autopickup_rules *ar);
static void freefruitchn(void);


int
dosave(const struct nh_cmd_arg *arg)
{
    int n;
    const int *selected;
    struct nh_menulist menu;

    (void) arg;

    init_menulist(&menu);
    add_menuitem(&menu, 1, "Quicksave and exit the game", 'y', FALSE);
    add_menuitem(&menu, 2, "Abandon this game and delete its save file", '!',
                 FALSE);
    add_menuitem(&menu, 3, "Continue playing", 'n', FALSE);
    n = display_menu(&menu, "Do you want to stop playing?",
                     PICK_ONE, PLHINT_URGENT, &selected);

    if (n)
        n = selected[0];
    else
        n = 3;

    if (n == 3) {
        action_interrupted();
    } else if (n == 1) {
        terminate(GAME_DETACHED);
    } else if (n == 2) {
        return done2();
    }
    return 0;
}


void
savegame(struct memfile *mf)
{
    int count = 0;
    xchar ltmp;

    /* no tag useful here as store_version adds one */
    store_version(mf);

    /* Place flags, player info & moves at the beginning of the save. This
       makes it possible to read them in nh_get_savegame_status without parsing
       all the dungeon and level data */
    mwrite32(mf, moves);
    save_flags(mf);
    save_you(mf, &u);
    save_mon(mf, &youmonst);

    /* store dungeon layout */
    save_dungeon(mf);
    savelevchn(mf);

    /* store levels */
    mtag(mf, 0, MTAG_LEVELS);
    for (ltmp = 1; ltmp <= maxledgerno(); ltmp++)
        if (levels[ltmp])
            count++;
    mwrite32(mf, count);
    for (ltmp = 1; ltmp <= maxledgerno(); ltmp++) {
        if (!levels[ltmp])
            continue;
        mtag(mf, ltmp, MTAG_LEVELS);
        mwrite8(mf, ltmp);      /* level number */
        savelev(mf, ltmp);      /* actual level */
    }
    savegamestate(mf);

    /* must come last, because it needs to be restored last */
    save_utracked(mf, &u);
}


static void
save_flags(struct memfile *mf)
{
    /* this is a fixed distane after version, but we tag it anyway to make
       debugging easier */
    mtag(mf, 0, MTAG_FLAGS);

    mwrite64(mf, flags.turntime);

    mwrite32(mf, flags.djinni_count);
    mwrite32(mf, flags.ghost_count);
    mwrite32(mf, flags.ident);
    mwrite32(mf, flags.last_cmd);
    mwrite32(mf, flags.moonphase);
    mwrite32(mf, flags.no_of_wizards);
    mwrite32(mf, flags.pickup_burden);
    mwrite32(mf, flags.recently_broken_otyp);

    mwrite8(mf, flags.autodig);
    mwrite8(mf, flags.autodigdown);
    mwrite8(mf, flags.autoquiver);
    mwrite8(mf, flags.beginner);
    mwrite8(mf, flags.bones_enabled);
    mwrite8(mf, flags.corridorbranch);
    mwrite8(mf, flags.debug);
    mwrite8(mf, flags.explore);
    mwrite8(mf, flags.elbereth_enabled);
    mwrite8(mf, flags.end_disclose);
    mwrite8(mf, flags.friday13);
    mwrite8(mf, flags.incomplete);
    mwrite8(mf, flags.interaction_mode);
    mwrite8(mf, flags.interrupted);
    mwrite8(mf, flags.legacy);
    mwrite8(mf, flags.made_amulet);
    mwrite8(mf, flags.menu_style);
    mwrite8(mf, flags.mon_generation);
    mwrite8(mf, flags.mon_moving);
    mwrite8(mf, flags.mon_polycontrol);
    mwrite8(mf, flags.occupation);
    mwrite8(mf, flags.permablind);
    mwrite8(mf, flags.permahallu);
    mwrite8(mf, flags.pickup);
    mwrite8(mf, flags.pickup_thrown);
    mwrite8(mf, flags.prayconfirm);
    mwrite8(mf, flags.pushweapon);
    mwrite8(mf, flags.rogue_enabled);
    mwrite8(mf, flags.seduce_enabled);
    mwrite8(mf, flags.showrace);
    mwrite8(mf, flags.show_uncursed);
    mwrite8(mf, flags.sortpack);
    mwrite8(mf, flags.sparkle);
    mwrite8(mf, flags.tombstone);
    mwrite8(mf, flags.travel_interrupt);
    mwrite8(mf, flags.verbose);

    mwrite32(mf, flags.last_arg.argtype);
    mwrite32(mf, flags.last_arg.dir);
    mwrite16(mf, flags.last_arg.pos.x);
    mwrite16(mf, flags.last_arg.pos.y);
    mwrite8(mf, flags.last_arg.invlet);
    mwrite8(mf, flags.last_arg.spelllet);
    mwrite32(mf, flags.last_arg.limit);

    mwrite(mf, flags.inv_order, sizeof (flags.inv_order));

    if (!flags.last_str_buf) {
        flags.last_str_buf = malloc(1);
        *(flags.last_str_buf) = '\0';
    }
    mwrite32(mf, strlen(flags.last_str_buf));
    mwrite(mf, flags.last_str_buf, strlen(flags.last_str_buf));

    save_autopickup_rules(mf, flags.ap_rules);
    save_coords(mf, &flags.travelcc, 1);
}


static void
save_autopickup_rules(struct memfile *mf, struct nh_autopickup_rules *ar)
{
    int len = ar->num_rules, i;
    mwrite32(mf, len);
    for (i = 0; i < len; ++i) {
        /* ar->rules[i].pattern is a char array, so this is safe */
        mwrite(mf, ar->rules[i].pattern, sizeof (ar->rules[i].pattern));
        mwrite16(mf, ar->rules[i].oclass);
        mwrite8(mf, ar->rules[i].buc);
        mwrite8(mf, ar->rules[i].action);
    }
}


static void
save_mvitals(struct memfile *mf)
{
    /* mtag useful here because migration is variable-length */
    mtag(mf, 0, MTAG_MVITALS);
    int i;

    for (i = 0; i < NUMMONS; i++) {
        mwrite8(mf, mvitals[i].born);
        mwrite8(mf, mvitals[i].died);
        mwrite8(mf, mvitals[i].mvflags);
    }
}


static void
save_quest_status(struct memfile *mf, struct q_score *q)
{
    unsigned int qflags;

    qflags =
        (q->first_start << 31) | (q->met_leader << 30) |
        (q->not_ready << 27) | (q->pissed_off << 26) |
        (q->got_quest << 25) | (q->first_locate << 24) |
        (q->met_intermed << 23) | (q->got_final << 22) |
        (q->made_goal << 19) | (q->met_nemesis << 18) |
        (q->killed_nemesis << 17) | (q->in_battle << 16) |
        (q->cheater << 15) | (q->touched_artifact << 14) |
        (q->offered_artifact << 13) |
        (q->got_thanks << 12) | (q->leader_is_dead << 11);
    mwrite32(mf, qflags);
    mwrite32(mf, q->leader_m_id);
}


static void
save_spellbook(struct memfile *mf)
{
    int i;

    for (i = 0; i < MAXSPELL + 1; i++) {
        mwrite32(mf, spl_book[i].sp_know);
        mwrite16(mf, spl_book[i].sp_id);
        mwrite8(mf, spl_book[i].sp_lev);
    }
}


static void
savegamestate(struct memfile *mf)
{
    mtag(mf, 0, MTAG_GAMESTATE);
    mfmagic_set(mf, STATE_MAGIC);

    /* must come before migrating_objs and migrating_mons are freed */
    save_timers(mf, level, RANGE_GLOBAL);
    save_light_sources(mf, level, RANGE_GLOBAL);

    saveobjchn(mf, invent);
    savemonchn(mf, migrating_mons);
    save_mvitals(mf);

    save_spellbook(mf);
    save_artifacts(mf);
    save_oracles(mf);

    mwrite(mf, pl_fruit, sizeof pl_fruit);
    mwrite32(mf, current_fruit);
    savefruitchn(mf);
    savenames(mf);
    save_waterlevel(mf);
    save_mt_state(mf);
    save_track(mf);
    save_rndmonst_state(mf);
    save_history(mf);
}


/* Note: when changing this function, you should also change the error handler
   in load_gamestate_from_binary_save, so that it can correctly calculate
   which location had the problem. It needs to know how many bytes there are
   per location (currently 64 bits = 8 bytes). */
static void
save_location(struct memfile *mf, struct rm *loc)
{
    unsigned int memflags;
    unsigned short rflags;

    unsigned char bg = loc->mem_bg;

    /* pack mem_door_l, mem_door_t into mem_bg */
    switch (bg) {
    case S_vodoor:
        if (loc->mem_door_l && loc->mem_door_t)
            bg = S_vodoor_memlt;
        else if (loc->mem_door_l)
            bg = S_vodoor_meml;
        else if (loc->mem_door_t)
            bg = S_vodoor_memt;
        break;
    case S_hodoor:
        if (loc->mem_door_l && loc->mem_door_t)
            bg = S_hodoor_memlt;
        else if (loc->mem_door_l)
            bg = S_hodoor_meml;
        else if (loc->mem_door_t)
            bg = S_hodoor_memt;
        break;
    case S_vcdoor:
        if (loc->mem_door_l && loc->mem_door_t)
            bg = S_vcdoor_memlt;
        else if (loc->mem_door_l)
            bg = S_vcdoor_meml;
        else if (loc->mem_door_t)
            bg = S_vcdoor_memt;
        break;
    case S_hcdoor:
        if (loc->mem_door_l && loc->mem_door_t)
            bg = S_hcdoor_memlt;
        else if (loc->mem_door_l)
            bg = S_hcdoor_meml;
        else if (loc->mem_door_t)
            bg = S_hcdoor_memt;
        break;
    }

    memflags = (bg << 26) | (loc->mem_trap << 21) | (loc->mem_obj << 11) |
        (loc->mem_obj_mn << 2) | (loc->mem_invis << 1) |
        (loc->mem_stepped << 0);
    rflags = (loc->flags << 11) | (loc->horizontal << 10) | (loc->lit << 9) |
        (loc->waslit << 8) | (loc->roomno << 2) | (loc->edge << 1);
    mwrite32(mf, memflags);
    mwrite8(mf, loc->typ);
    mwrite8(mf, loc->seenv);
    mwrite16(mf, rflags);
}

static void
save_you(struct memfile *mf, struct you *y)
{
    int i;
    unsigned int yflags, eflags;

    yflags =
        (y->uswallow << 31) | (y->uinwater << 30) |
        (y->uundetected << 29) | (y->mfemale << 28) |
        (y->uinvulnerable << 27) | (y->uburied << 26) |
        (y->uedibility << 25) | (y->uwelcomed << 24) |
        (y->usick_type << 22) | (y->ufemale << 21);
    eflags =
        (y->uevent.minor_oracle << 31) |
        (y->uevent.major_oracle << 30) |
        (y->uevent.qcalled << 29) |
        (y->uevent.qexpelled << 28) |
        (y->uevent.qcompleted << 27) |
        (y->uevent.uheard_tune << 25) |
        (y->uevent.uopened_dbridge << 24) |
        (y->uevent.invoked << 23) |
        (y->uevent.gehennom_entered << 22) |
        (y->uevent.uhand_of_elbereth << 20) |
        (y->uevent.udemigod << 19) |
        (y->uevent.ascended << 18);

    mtag(mf, 0, MTAG_YOU);
    mwrite64(mf, y->ubirthday);

    mwrite32(mf, yflags);
    mwrite32(mf, eflags);
    mwrite32(mf, y->uhp);
    mwrite32(mf, y->uhpmax);
    mwrite32(mf, y->uen);
    mwrite32(mf, y->uenmax);
    mwrite32(mf, y->ulevel);
    mwrite32(mf, y->umoney0);
    mwrite32(mf, y->uexp);
    mwrite32(mf, y->urexp);
    mwrite32(mf, y->ulevelmax);
    mwrite32(mf, y->umonster);
    mwrite32(mf, y->umonnum);
    mwrite32(mf, y->mh);
    mwrite32(mf, y->mhmax);
    mwrite32(mf, y->mtimedone);
    mwrite32(mf, y->ulycn);
    mwrite32(mf, y->utrap);
    mwrite32(mf, y->utraptype);
    mwrite32(mf, y->uhunger);
    mwrite32(mf, y->uhs);
    mwrite32(mf, y->oldcap);
    mwrite32(mf, y->umconf);
    mwrite32(mf, y->nv_range);
    mwrite32(mf, y->bglyph);
    mwrite32(mf, y->cglyph);
    mwrite32(mf, y->bc_order);
    mwrite32(mf, y->bc_felt);
    mwrite32(mf, y->ucreamed);
    mwrite32(mf, y->uswldtim);
    mwrite32(mf, y->udg_cnt);
    mwrite32(mf, y->next_attr_check);
    mwrite32(mf, y->ualign.record);
    mwrite32(mf, y->ugangr);
    mwrite32(mf, y->ugifts);
    mwrite32(mf, y->ublessed);
    mwrite32(mf, y->ublesscnt);
    mwrite32(mf, y->ucleansed);
    mwrite32(mf, y->uinvault);
    mwrite32(mf, y->ugallop);
    mwrite32(mf, y->urideturns);
    mwrite32(mf, y->umortality);
    mwrite32(mf, y->ugrave_arise);
    mwrite32(mf, y->weapon_slots);
    mwrite32(mf, y->skills_advanced);
    mwrite32(mf, y->initrole);
    mwrite32(mf, y->initrace);
    mwrite32(mf, y->initgend);
    mwrite32(mf, y->initalign);
    mwrite32(mf, y->upantheon);

    mwrite32(mf, y->ustuck ? y->ustuck->m_id : 0);
    mwrite32(mf, y->usteed ? y->usteed->m_id : 0);

    mwrite8(mf, y->ux);
    mwrite8(mf, y->uy);
    mwrite8(mf, y->tx);
    mwrite8(mf, y->ty);
    mwrite8(mf, y->ux0);
    mwrite8(mf, y->uy0);
    mwrite8(mf, y->uz.dnum);
    mwrite8(mf, y->uz.dlevel);
    mwrite8(mf, y->utolev.dnum);
    mwrite8(mf, y->utolev.dlevel);
    mwrite8(mf, y->utotype);
    mwrite8(mf, y->umoved);
    mwrite8(mf, y->ualign.type);
    mwrite8(mf, y->ualignbase[0]);
    mwrite8(mf, y->ualignbase[1]);
    mwrite8(mf, y->uluck);
    mwrite8(mf, y->moreluck);
    mwrite8(mf, y->uhitinc);
    mwrite8(mf, y->udaminc);
    mwrite8(mf, y->uac);
    mwrite8(mf, y->uspellprot);
    mwrite8(mf, y->usptime);
    mwrite8(mf, y->uspmtime);
    mwrite8(mf, y->twoweap);
    mwrite8(mf, y->bashmsg);

    mwrite(mf, y->uconduct, (sizeof y->uconduct));
    mwrite(mf, y->uconduct_time, (sizeof y->uconduct_time));
    mwrite(mf, y->uwhybusy, (sizeof y->uwhybusy));
    mwrite(mf, y->urooms, sizeof (y->urooms));
    mwrite(mf, y->urooms0, sizeof (y->urooms0));
    mwrite(mf, y->uentered, sizeof (y->uentered));
    mwrite(mf, y->ushops, sizeof (y->ushops));
    mwrite(mf, y->ushops0, sizeof (y->ushops0));
    mwrite(mf, y->ushops_entered, sizeof (y->ushops_entered));
    mwrite(mf, y->ushops_left, sizeof (y->ushops_left));
    mwrite(mf, y->macurr.a, sizeof (y->macurr.a));
    mwrite(mf, y->mamax.a, sizeof (y->mamax.a));
    mwrite(mf, y->acurr.a, sizeof (y->acurr.a));
    mwrite(mf, y->aexe.a, sizeof (y->aexe.a));
    mwrite(mf, y->abon.a, sizeof (y->abon.a));
    mwrite(mf, y->amax.a, sizeof (y->amax.a));
    mwrite(mf, y->atemp.a, sizeof (y->atemp.a));
    mwrite(mf, y->atime.a, sizeof (y->atime.a));
    mwrite(mf, y->skill_record, sizeof (y->skill_record));
    mwrite(mf, y->uplname, sizeof (y->uplname));

    for (i = 0; i <= LAST_PROP; i++) {
        mwrite32(mf, y->uintrinsic[i]);
    }
    for (i = 0; i < P_NUM_SKILLS; i++) {
        mwrite8(mf, y->weapon_skills[i].skill);
        mwrite8(mf, y->weapon_skills[i].max_skill);
        mwrite16(mf, y->weapon_skills[i].advance);
    }

    save_quest_status(mf, &y->quest_status);

    if (y->delayed_killers.stoning) {
        int len = strlen(y->delayed_killers.stoning);
        mwrite32(mf, len);
        mwrite(mf, y->delayed_killers.stoning, len);
    } else
        mwrite32(mf, 0);

    if (y->delayed_killers.sliming) {
        int len = strlen(y->delayed_killers.sliming);
        mwrite32(mf, len);
        mwrite(mf, y->delayed_killers.sliming, len);
    } else
        mwrite32(mf, 0);

    if (y->delayed_killers.illness) {
        int len = strlen(y->delayed_killers.illness);
        mwrite32(mf, len);
        mwrite(mf, y->delayed_killers.illness, len);
    } else
        mwrite32(mf, 0);

    if (y->delayed_killers.genocide) {
        int len = strlen(y->delayed_killers.genocide);
        mwrite32(mf, len);
        mwrite(mf, y->delayed_killers.genocide, len);
    } else
        mwrite32(mf, 0);

    mwrite32(mf, y->lastinvnr);
}

static void
save_utracked(struct memfile *mf, struct you *y)
{
    int i;
    for (i = 0; i <= tos_last_slot; i++) {
        mwrite32(mf, y->utracked[i] == &zeroobj ? -1 :
                 y->utracked[i] ? y->utracked[i]->o_id : 0);
        mwrite32(mf, y->uoccupation_progress[i]);
    }
    for (i = 0; i <= tl_last_slot; i++) {
        mwrite8(mf, y->utracked_location[i].x);
        mwrite8(mf, y->utracked_location[i].y);
    }
}


static void
save_stairway(struct memfile *mf, stairway s)
{
    mwrite8(mf, s.sx);
    mwrite8(mf, s.sy);
    save_dlevel(mf, s.tolev);
    mwrite8(mf, s.up);
}


static void
save_dest_area(struct memfile *mf, dest_area a)
{
    mwrite8(mf, a.lx);
    mwrite8(mf, a.ly);
    mwrite8(mf, a.hx);
    mwrite8(mf, a.hy);
    mwrite8(mf, a.nlx);
    mwrite8(mf, a.nly);
    mwrite8(mf, a.nhx);
    mwrite8(mf, a.nhy);
}


void
save_coords(struct memfile *mf, const coord *c, int n)
{
    const coord *p;
    for (p = c; p < c + n; ++p) {
        mwrite8(mf, p->x);
        mwrite8(mf, p->y);
    }
}


void
savelev(struct memfile *mf, xchar levnum)
{
    int x, y;
    unsigned int lflags;
    struct level *lev = levels[levnum];

    /* The purge_monsters count refers to monsters on the current level. */
    if (lev->flags.purge_monsters) {
        /* purge any dead monsters (necessary if we're starting a panic save
           rather than a normal one, or sometimes when changing levels without
           taking time -- e.g. create statue trap then immediately level
           teleport) */
        dmonsfree(lev);
    }

    mtag(mf, levnum, MTAG_LEVEL);
    mfmagic_set(mf, LEVEL_MAGIC);

    mwrite8(mf, lev->z.dnum);
    mwrite8(mf, lev->z.dlevel);
    mwrite(mf, lev->levname, sizeof (lev->levname));

    mtag(mf, levnum, MTAG_LOCATIONS);
    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++)
            save_location(mf, &lev->locations[x][y]);

    mwrite32(mf, lev->lastmoves);

    mtag(mf, levnum, MTAG_STAIRWAYS);
    save_stairway(mf, lev->upstair);
    save_stairway(mf, lev->dnstair);
    save_stairway(mf, lev->upladder);
    save_stairway(mf, lev->dnladder);
    save_stairway(mf, lev->sstairs);
    save_dest_area(mf, lev->updest);
    save_dest_area(mf, lev->dndest);

    mtag(mf, levnum, MTAG_LFLAGS);
    lflags = (lev->flags.noteleport << 22) |
        (lev->flags.hardfloor << 21) | (lev->flags.nommap << 20) |
        (lev->flags.hero_memory << 19) | (lev->flags.shortsighted << 18) |
        (lev->flags.graveyard << 17) | (lev->flags.is_maze_lev << 16) |
        (lev->flags.is_cavernous_lev << 15) | (lev->flags.arboreal << 14) |
        (lev->flags.forgotten << 13);
    mwrite32(mf, lflags);
    save_coords(mf, lev->doors, DOORMAX);

    save_rooms(mf, lev);        /* no dynamic memory to reclaim */

    /* must be saved before mons, objs, and buried objs */
    save_timers(mf, lev, RANGE_LEVEL);
    save_light_sources(mf, lev, RANGE_LEVEL);

    savemonchn(mf, lev->monlist);
    save_worm(mf, lev); /* save worm information */
    savetrapchn(mf, lev->lev_traps, lev);
    saveobjchn(mf, lev->objlist);
    saveobjchn(mf, lev->buriedobjlist);
    saveobjchn(mf, lev->billobjs);
    save_engravings(mf, lev);
    savedamage(mf, lev);
    save_regions(mf, lev);
}


void
freelev(xchar levnum)
{
    struct level *lev = levels[levnum];

    /* must be freed before mons, objs, and buried objs */
    free_timers(lev);
    free_light_sources(lev);

    free_monchn(lev->monlist);
    free_worm(lev);
    freetrapchn(lev->lev_traps);
    free_objchn(lev->objlist);
    free_objchn(lev->buriedobjlist);
    free_objchn(lev->billobjs);

    lev->monlist = NULL;
    lev->lev_traps = NULL;
    lev->objlist = NULL;
    lev->buriedobjlist = NULL;
    lev->billobjs = NULL;

    free_engravings(lev);
    freedamage(lev);
    free_regions(lev);

    free(lev);
    levels[levnum] = NULL;
}


static void
savelevchn(struct memfile *mf)
{
    s_level *tmplev;
    int cnt = 0;

    for (tmplev = sp_levchn; tmplev; tmplev = tmplev->next)
        cnt++;
    mwrite32(mf, cnt);

    for (tmplev = sp_levchn; tmplev; tmplev = tmplev->next) {
        save_d_flags(mf, tmplev->flags);
        save_dlevel(mf, tmplev->dlevel);
        mwrite(mf, tmplev->proto, sizeof (tmplev->proto));
        mwrite8(mf, tmplev->boneid);
        mwrite8(mf, tmplev->rndlevs);
    }
}


static void
savedamage(struct memfile *mf, struct level *lev)
{
    struct damage *damageptr;
    unsigned int xl = 0;

    mtag(mf, ledger_no(&lev->z), MTAG_DAMAGE);

    for (damageptr = lev->damagelist; damageptr; damageptr = damageptr->next)
        xl++;
    mwrite32(mf, xl);

    for (damageptr = lev->damagelist; damageptr; damageptr = damageptr->next) {
        mtag(mf, damageptr->when, MTAG_DAMAGEVALUE);
        mwrite32(mf, damageptr->when);
        mwrite32(mf, damageptr->cost);
        mwrite8(mf, damageptr->place.x);
        mwrite8(mf, damageptr->place.y);
        mwrite8(mf, damageptr->typ);
    }
}


static void
freedamage(struct level *lev)
{
    struct damage *damageptr, *tmp_dam;

    damageptr = lev->damagelist;
    while (damageptr) {
        tmp_dam = damageptr;
        damageptr = damageptr->next;
        free(tmp_dam);
    }
    lev->damagelist = NULL;
}


static void
free_objchn(struct obj *otmp)
{
    struct obj *otmp2;

    while (otmp) {
        otmp2 = otmp->nobj;
        if (Has_contents(otmp))
            free_objchn(otmp->cobj);

        otmp->where = OBJ_FREE;
        otmp->nobj = turnstate.floating_objects;
        turnstate.floating_objects = otmp;
        otmp->timed = 0;        /* not timed any more */
        otmp->lamplit = 0;      /* caller handled lights */
        dealloc_obj(otmp);
        otmp = otmp2;
    }
}


static void
saveobjchn(struct memfile *mf, struct obj *otmp)
{
    int count = 0;
    struct obj *otmp2;

    mfmagic_set(mf, OBJCHAIN_MAGIC);
    for (otmp2 = otmp; otmp2; otmp2 = otmp2->nobj)
        count++;
    mwrite32(mf, count);

    while (otmp) {
        save_obj(mf, otmp);
        if (Has_contents(otmp))
            saveobjchn(mf, otmp->cobj);

        otmp = otmp->nobj;
    }
}


static void
free_monchn(struct monst *mon)
{
    struct monst *mtmp2;

    while (mon) {
        mtmp2 = mon->nmon;

        if (mon->minvent)
            free_objchn(mon->minvent);
        dealloc_monst(mon);
        mon = mtmp2;
    }
}


static void
savemonchn(struct memfile *mf, struct monst *mtmp)
{
    struct monst *mtmp2;
    unsigned int count = 0;

    mfmagic_set(mf, MONCHAIN_MAGIC);
    for (mtmp2 = mtmp; mtmp2; mtmp2 = mtmp2->nmon)
        count++;
    mwrite32(mf, count);

    while (mtmp) {
        mtmp2 = mtmp->nmon;
        save_mon(mf, mtmp);

        if (mtmp->minvent)
            saveobjchn(mf, mtmp->minvent);
        mtmp = mtmp2;
    }
}


static void
savetrapchn(struct memfile *mf, struct trap *trap, struct level *lev)
{
    struct trap *trap2;
    unsigned short tflags;
    int count = 0;

    mfmagic_set(mf, TRAPCHAIN_MAGIC);
    for (trap2 = trap; trap2; trap2 = trap2->ntrap)
        count++;
    mwrite32(mf, count);

    for (; trap; trap = trap->ntrap) {
        /* To distinguish traps from each other in tags, we use x/y/z coords */
        mtag(mf,
             ledger_no(&lev->z) + ((int)trap->tx << 8) + ((int)trap->ty << 16),
             MTAG_TRAP);
        mwrite8(mf, trap->tx);
        mwrite8(mf, trap->ty);
        mwrite8(mf, trap->dst.dnum);
        mwrite8(mf, trap->dst.dlevel);
        mwrite8(mf, trap->launch.x);
        mwrite8(mf, trap->launch.y);
        tflags = (trap->ttyp << 11) | (trap->tseen << 10) |
            (trap->once << 9) | (trap->madeby_u << 8);
        mwrite16(mf, tflags);
        mwrite16(mf, trap->vl.v_launch_otyp);
    }
}


static void
freetrapchn(struct trap *trap)
{
    struct trap *trap2;

    while (trap) {
        trap2 = trap->ntrap;
        dealloc_trap(trap);
        trap = trap2;
    }
}

/* save all the fruit names and ID's; this is used only in saving whole games
 * (not levels) and in saving bones levels.  When saving a bones level,
 * we only want to save the fruits which exist on the bones level; the bones
 * level routine marks nonexistent fruits by making the fid negative.
 */
void
savefruitchn(struct memfile *mf)
{
    struct fruit *f1;
    unsigned int count = 0;

    mfmagic_set(mf, FRUITCHAIN_MAGIC);
    for (f1 = ffruit; f1; f1 = f1->nextf)
        if (f1->fid >= 0)
            count++;
    mwrite32(mf, count);

    for (f1 = ffruit; f1; f1 = f1->nextf) {
        if (f1->fid >= 0) {
            mtag(mf, f1->fid, MTAG_FRUIT);
            mwrite(mf, f1->fname, sizeof (f1->fname));
            mwrite32(mf, f1->fid);
        }
    }
}


static void
freefruitchn(void)
{
    struct fruit *f2, *f1 = ffruit;

    while (f1) {
        f2 = f1->nextf;
        dealloc_fruit(f1);
        f1 = f2;
    }
    ffruit = NULL;
}


void
freedynamicdata(void)
{
    int i;
    struct level *lev;

    if (!objects)
        return; /* no cleanup necessary */

    unload_qtlist();
    free_invbuf();      /* let_to_name (invent.c) */
    tmpsym_freeall();   /* temporary display effects */
    clear_delayed_killers();
#define free_animals()   mon_animal_list(FALSE)

    for (i = 0; i < MAXLINFO; i++) {
        lev = levels[i];
        levels[i] = NULL;
        if (!lev)
            continue;

        /* level-specific data */
        dmonsfree(lev); /* release dead monsters */
        free_timers(lev);
        free_light_sources(lev);
        free_monchn(lev->monlist);
        free_worm(lev); /* release worm segment information */
        freetrapchn(lev->lev_traps);
        free_objchn(lev->objlist);
        free_objchn(lev->buriedobjlist);
        free_objchn(lev->billobjs);
        free_engravings(lev);
        freedamage(lev);

        free(lev);
    }

    /* game-state data */
    free_objchn(invent);
    free_monchn(migrating_mons);
    /* this should normally be NULL between turns, but might not be due to
     * the game ending where pets can follow (e.g. ascension or dungeon escape)
     * or due to panicing. */
    free_monchn(turnstate.migrating_pets);
    free_animals();
    free_oracles();
    freefruitchn();
    freenames();
    free_waterlevel();
    free_dungeon();
    free_history();

    if (flags.last_str_buf) {
        free(flags.last_str_buf);
        flags.last_str_buf = NULL;
    }
    if (flags.ap_rules) {
        free(flags.ap_rules->rules);
        flags.ap_rules->rules = NULL;
        free(flags.ap_rules);
        flags.ap_rules = NULL;
    }

    free(artilist);
    free(objects);
    objects = NULL;
    artilist = NULL;

    return;
}

/*save.c*/

