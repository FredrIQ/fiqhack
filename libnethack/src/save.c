/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-01-01 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

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
static void save_options(struct memfile *mf);
static void save_option(struct memfile *mf, struct nh_option_desc *opt);
static void save_autopickup_rules(struct memfile *mf,
                                  struct nh_autopickup_rules *ar);
static void freefruitchn(void);


int
dosave(const struct nh_cmd_arg *arg)
{
    int n, selected[1];
    struct nh_menulist menu;

    (void) arg;

    init_menulist(&menu);
    add_menuitem(&menu, 1, "Quicksave and exit the game", 'y', FALSE);
    add_menuitem(&menu, 2, "Abandon this game and delete its save file", '!',
                 FALSE);
    add_menuitem(&menu, 3, "Continue playing", 'n', FALSE);
    n = display_menu(&menu, "Do you want to stop playing?",
                     PICK_ONE, PLHINT_URGENT, selected);

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
    save_options(mf);
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
    /* this is a fixed difference after version, but we tag it anyway to
       make debugging easier */
    mtag(mf, 0, MTAG_FLAGS);

    mwrite32(mf, flags.djinni_count);         /*  0 */
    mwrite32(mf, flags.ghost_count);          /*  4 */
    mwrite32(mf, flags.ident);                /*  8 */
    mwrite32(mf, flags.last_cmd);             /* 12 */
    mwrite32(mf, flags.moonphase);            /* 16 */
    mwrite32(mf, flags.no_of_wizards);        /* 20 */
    mwrite32(mf, flags.pickup_burden);        /* 24 */
    mwrite32(mf, flags.recently_broken_otyp); /* 28 */

    mwrite8(mf, flags.autodig);               /* 32 */
    mwrite8(mf, flags.autodigdown);           /* 33 */
    mwrite8(mf, flags.autoquiver);            /* 34 */
    mwrite8(mf, flags.beginner);              /* 35 */
    mwrite8(mf, flags.bones_enabled);         /* 36 */
    mwrite8(mf, flags.corridorbranch);        /* 37 */
    mwrite8(mf, flags.debug);                 /* 38 */
    mwrite8(mf, flags.explore);               /* 39 */
    mwrite8(mf, flags.elbereth_enabled);      /* 40 */
    mwrite8(mf, flags.end_disclose);          /* 41 */
    mwrite8(mf, flags.friday13);              /* 42 */
    mwrite8(mf, flags.incomplete);            /* 43 */
    mwrite8(mf, flags.interaction_mode);      /* 44 */
    mwrite8(mf, flags.interrupted);           /* 45 */
    mwrite8(mf, flags.legacy);                /* 46 */
    mwrite8(mf, flags.lit_corridor);          /* 47 */
    mwrite8(mf, flags.made_amulet);           /* 48 */
    mwrite8(mf, flags.menu_style);            /* 49 */
    mwrite8(mf, flags.mon_generation);        /* 50 */
    mwrite8(mf, flags.mon_moving);            /* 51 */
    mwrite8(mf, flags.mon_polycontrol);       /* 52 */
    mwrite8(mf, flags.occupation);            /* 53 */
    mwrite8(mf, flags.permablind);            /* 54 */
    mwrite8(mf, flags.permahallu);            /* 55 */
    mwrite8(mf, flags.pickup);                /* 56 */
    mwrite8(mf, flags.pickup_thrown);         /* 57 */
    mwrite8(mf, flags.prayconfirm);           /* 58 */
    mwrite8(mf, flags.pushweapon);            /* 59 */
    mwrite8(mf, flags.rogue_enabled);         /* 60 */
    mwrite8(mf, flags.seduce_enabled);        /* 61 */
    mwrite8(mf, flags.showrace);              /* 62 */
    mwrite8(mf, flags.show_uncursed);         /* 63 */
    mwrite8(mf, flags.sortpack);              /* 64 */
    mwrite8(mf, flags.soundok);               /* 65 */
    mwrite8(mf, flags.sparkle);               /* 66 */
    mwrite8(mf, flags.tombstone);             /* 67 */
    mwrite8(mf, flags.travel_interrupt);      /* 68 */
    mwrite8(mf, flags.verbose);               /* 69 */
    mwrite(mf, flags.inv_order, sizeof (flags.inv_order)); /* 87 */

    mwrite32(mf, flags.last_arg.argtype);     /* 91 */
    mwrite32(mf, flags.last_arg.dir);         /* 95 */
    mwrite16(mf, flags.last_arg.pos.x);       /* 97 */
    mwrite16(mf, flags.last_arg.pos.y);       /* 99 */
    mwrite8(mf, flags.last_arg.invlet);       /* 100 */
    mwrite8(mf, flags.last_arg.spelllet);     /* 101 */
    mwrite32(mf, flags.last_arg.limit);       /* 105 */

    mwrite(mf, flags.last_arg.str, sizeof flags.last_arg.str);

    save_autopickup_rules(mf, flags.ap_rules);
    save_coords(mf, &flags.travelcc, 1);
}


static void
save_options(struct memfile *mf)
{
    mtag(mf, 0, MTAG_OPTIONS);

    int i;
    for (i = 0; options[i].name; ++i) {}
    mwrite32(mf, i);

    /* When saving options, we sanity-check the spacing, but we assume that
     * the order of options in memory is preserved, since we can't well handle
     * changes to the option list anyway. */
    for (i = 0; options[i].name; ++i) {
        mtag(mf, i, MTAG_OPTION);
        save_option(mf, &options[i]);
    }
}


static void
save_option(struct memfile *mf, struct nh_option_desc *opt)
{
    int len;

    switch (opt->type) {
    case OPTTYPE_BOOL:
        mwrite8(mf, opt->value.b);
        break;
    case OPTTYPE_INT:
    case OPTTYPE_ENUM:
        mwrite32(mf, opt->value.i); /* equivalent opt->value.e */
        break;
    case OPTTYPE_STRING:
        len = opt->value.s ? strlen(opt->value.s) : 0 ;
        mwrite32(mf, len);
        if (len)
            mwrite(mf, opt->value.s, len);
        break;
    case OPTTYPE_AUTOPICKUP_RULES:
        save_autopickup_rules(mf, opt->value.ar);
        break;
    }
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

    mwrite(mf, pl_character, sizeof pl_character);
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
    mwrite64(mf, y->ubirthday);                                  /*   0 */

    mwrite32(mf, yflags);                                        /*   8 */
    mwrite32(mf, eflags);                                        /*  12 */
    mwrite32(mf, y->uhp);                                        /*  16 */
    mwrite32(mf, y->uhpmax);                                     /*  20 */
    mwrite32(mf, y->uen);                                        /*  24 */
    mwrite32(mf, y->uenmax);                                     /*  28 */
    mwrite32(mf, y->ulevel);                                     /*  32 */
    mwrite32(mf, y->umoney0);                                    /*  36 */
    mwrite32(mf, y->uexp);                                       /*  40 */
    mwrite32(mf, y->urexp);                                      /*  44 */
    mwrite32(mf, y->ulevelmax);                                  /*  48 */
    mwrite32(mf, y->umonster);                                   /*  52 */
    mwrite32(mf, y->umonnum);                                    /*  56 */
    mwrite32(mf, y->mh);                                         /*  60 */
    mwrite32(mf, y->mhmax);                                      /*  64 */
    mwrite32(mf, y->mtimedone);                                  /*  68 */
    mwrite32(mf, y->ulycn);                                      /*  72 */
    mwrite32(mf, y->last_str_turn);                              /*  76 */
    mwrite32(mf, y->utrap);                                      /*  80 */
    mwrite32(mf, y->utraptype);                                  /*  84 */
    mwrite32(mf, y->uhunger);                                    /*  88 */
    mwrite32(mf, y->uhs);                                        /*  92 */
    mwrite32(mf, y->oldcap);                                     /*  94 */
    mwrite32(mf, y->umconf);                                     /*  96 */
    mwrite32(mf, y->nv_range);                                   /* 100 */
    mwrite32(mf, y->bglyph);                                     /* 104 */
    mwrite32(mf, y->cglyph);                                     /* 108 */
    mwrite32(mf, y->bc_order);                                   /* 112 */
    mwrite32(mf, y->bc_felt);                                    /* 116 */
    mwrite32(mf, y->ucreamed);                                   /* 120 */
    mwrite32(mf, y->uswldtim);                                   /* 124 */
    mwrite32(mf, y->uhelpless);                                  /* 128 */
    mwrite32(mf, y->udg_cnt);                                    /* 132 */
    mwrite32(mf, y->next_attr_check);                            /* 136 */
    mwrite32(mf, y->ualign.record);                              /* 140 */
    mwrite32(mf, y->ugangr);                                     /* 144 */
    mwrite32(mf, y->ugifts);                                     /* 148 */
    mwrite32(mf, y->ublessed);                                   /* 152 */
    mwrite32(mf, y->ublesscnt);                                  /* 156 */
    mwrite32(mf, y->ucleansed);                                  /* 160 */
    mwrite32(mf, y->usleep);                                     /* 164 */
    mwrite32(mf, y->uinvault);                                   /* 168 */
    mwrite32(mf, y->ugallop);                                    /* 172 */
    mwrite32(mf, y->urideturns);                                 /* 176 */
    mwrite32(mf, y->umortality);                                 /* 180 */
    mwrite32(mf, y->ugrave_arise);                               /* 184 */
    mwrite32(mf, y->weapon_slots);                               /* 188 */
    mwrite32(mf, y->skills_advanced);                            /* 192 */
    mwrite32(mf, y->initrole);                                   /* 196 */
    mwrite32(mf, y->initrace);                                   /* 200 */
    mwrite32(mf, y->initgend);                                   /* 204 */
    mwrite32(mf, y->initalign);                                  /* 208 */
    mwrite32(mf, y->upantheon);                                  /* 216 */
    mwrite32(mf, y->uconduct.unvegetarian);                      /* 220 */
    mwrite32(mf, y->uconduct.unvegan);                           /* 224 */
    mwrite32(mf, y->uconduct.food);                              /* 228 */
    mwrite32(mf, y->uconduct.gnostic);                           /* 232 */
    mwrite32(mf, y->uconduct.weaphit);                           /* 236 */
    mwrite32(mf, y->uconduct.killer);                            /* 240 */
    mwrite32(mf, y->uconduct.literate);                          /* 244 */
    mwrite32(mf, y->uconduct.polypiles);                         /* 248 */
    mwrite32(mf, y->uconduct.polyselfs);                         /* 252 */
    mwrite32(mf, y->uconduct.wishes);                            /* 256 */
    mwrite32(mf, y->uconduct.wisharti);                          /* 260 */
    mwrite32(mf, y->uconduct.elbereths);                         /* 264 */
    mwrite32(mf, y->uconduct.puddings);                          /* 268 */

    mwrite32(mf, y->ustuck ? y->ustuck->m_id : 0);               /* 272 */
    mwrite32(mf, y->usteed ? y->usteed->m_id : 0);               /* 276 */

    mwrite8(mf, y->ux);                                          /* 280 */
    mwrite8(mf, y->uy);                                          /* 281 */
    mwrite8(mf, y->dx);                                          /* 282 */
    mwrite8(mf, y->dy);                                          /* 283 */
    mwrite8(mf, y->tx);                                          /* 284 */
    mwrite8(mf, y->ty);                                          /* 285 */
    mwrite8(mf, y->ux0);                                         /* 286 */
    mwrite8(mf, y->uy0);                                         /* 287 */
    mwrite8(mf, y->uz.dnum);                                     /* 288 */
    mwrite8(mf, y->uz.dlevel);                                   /* 289 */
    mwrite8(mf, y->utolev.dnum);                                 /* 290 */
    mwrite8(mf, y->utolev.dlevel);                               /* 291 */
    mwrite8(mf, y->utotype);                                     /* 292 */
    mwrite8(mf, y->umoved);                                      /* 293 */
    mwrite8(mf, y->ualign.type);                                 /* 294 */
    mwrite8(mf, y->ualignbase[0]);                               /* 295 */
    mwrite8(mf, y->ualignbase[1]);                               /* 296 */
    mwrite8(mf, y->uluck);                                       /* 297 */
    mwrite8(mf, y->moreluck);                                    /* 298 */
    mwrite8(mf, y->uhitinc);                                     /* 299 */
    mwrite8(mf, y->udaminc);                                     /* 300 */
    mwrite8(mf, y->uac);                                         /* 301 */
    mwrite8(mf, y->uspellprot);                                  /* 302 */
    mwrite8(mf, y->usptime);                                     /* 303 */
    mwrite8(mf, y->uspmtime);                                    /* 304 */
    mwrite8(mf, y->twoweap);                                     /* 305 */

    mwrite(mf, y->uwhybusy, (sizeof y->uwhybusy));
    mwrite(mf, y->umoveagain, (sizeof y->umoveagain));
    mwrite(mf, y->usick_cause, sizeof (y->usick_cause));
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

        otmp->where = OBJ_FREE; /* set to free so dealloc will work */
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
    free_youbuf();      /* You_buf,&c (pline.c) */
    tmpsym_freeall();   /* temporary display effects */
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
    free_monchn(mydogs);        /* ascension or dungeon escape */
    free_animals();
    free_oracles();
    freefruitchn();
    freenames();
    free_waterlevel();
    free_dungeon();
    free_history();

    if (flags.ap_rules) {
        free(flags.ap_rules->rules);
        flags.ap_rules->rules = NULL;
        free(flags.ap_rules);
    }
    flags.ap_rules = NULL;
    free(artilist);
    free(objects);
    objects = NULL;
    artilist = NULL;

    return;
}

/*save.c*/
