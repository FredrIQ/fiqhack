/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985,1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"

static boolean no_bones_level(d_level *);
static void goodfruit(int);
static void resetobjs(struct obj *, boolean);
static void drop_upon_death(struct monst *, struct obj *);


static char *
make_bones_id(char *buf, d_level * dlev)
{
    s_level *sptr;

    sprintf(buf, "%c%s", find_dungeon(dlev).boneid,
            In_quest(dlev) ? urole.filecode : "0");
    if ((sptr = Is_special(dlev)) != 0)
        sprintf(buf + 2, ".%c", sptr->boneid);
    else
        sprintf(buf + 2, ".%d", dlev->dlevel);

    return buf;
}


static boolean
no_bones_level(d_level * lev)
{
    s_level *sptr;

    return (boolean) (((sptr = Is_special(lev)) != 0 && !sptr->boneid)
                      || !find_dungeon(lev).boneid
                      /* no bones on the last or multiway branch levels */
                      /* in any dungeon (level 1 isn't multiway).  */
                      || Is_botlevel(lev)
                      || (Is_branchlev(lev) && lev->dlevel > 1)
                      /* no bones in the invocation level */
                      || (In_hell(lev) &&
                          lev->dlevel == dunlevs_in_dungeon(lev) - 1)
                      /* no bones on the first level */
                      /* TODO: remove hardcoding in dungeon rewrite */
                      || (lev->dnum == 0 && lev->dlevel == 1)
        );
}


/* Call this function for each fruit object saved in the bones level: it marks
   that particular type of fruit as existing (the marker is that that type's ID
   is positive instead of negative). This way, when we later save the chain of
   fruit types, we know to only save the types that exist. */
static void
goodfruit(int id)
{
    struct fruit *f;

    for (f = gamestate.fruits.chain; f; f = f->nextf) {
        if (f->fid == -id) {
            f->fid = id;
            return;
        }
    }
}

static void
resetobjs(struct obj *ochain, boolean restore)
{
    struct obj *otmp;

    for (otmp = ochain; otmp; otmp = otmp->nobj) {
        if (otmp->cobj)
            resetobjs(otmp->cobj, restore);

        if (((otmp->otyp != CORPSE || otmp->corpsenm < SPECIAL_PM)
             && otmp->otyp != STATUE)
            && ((!restore && !otmp->oartifact) ||
                (restore && otmp->oartifact &&
                 (exist_artifact(otmp->otyp, ONAME(otmp))
                  || is_quest_artifact(otmp))))) {
            otmp->oartifact = 0;
            otmp->onamelth = 0;
        } else if (otmp->oartifact && restore)
            artifact_exists(otmp, ONAME(otmp), TRUE);
        if (!restore) {
            /* do not zero out o_ids for ghost levels anymore */

            if (objects[otmp->otyp].oc_uses_known)
                otmp->known = 0;
            otmp->dknown = otmp->bknown = 0;
            otmp->rknown = 0;
            otmp->invlet = 0;
            otmp->no_charge = 0;
            otmp->was_thrown = 0;
            otmp->was_dropped = 0;

            if (otmp->otyp == SLIME_MOLD)
                goodfruit(otmp->spe);
            else if (otmp->otyp == EGG)
                otmp->spe = 0;
            else if (otmp->otyp == TIN) {
                /* make tins of unique monster's meat be empty */
                if (otmp->corpsenm >= LOW_PM &&
                    (mons[otmp->corpsenm].geno & G_UNIQ))
                    otmp->corpsenm = NON_PM;
            } else if (otmp->otyp == AMULET_OF_YENDOR) {
                /* no longer the real Amulet */
                otmp->otyp = FAKE_AMULET_OF_YENDOR;
                curse(otmp);
            } else if (otmp->otyp == CANDELABRUM_OF_INVOCATION) {
                if (otmp->lamplit)
                    end_burn(otmp, TRUE);
                otmp->otyp = WAX_CANDLE;
                otmp->age = 50L;        /* assume used */
                if (otmp->spe > 0)
                    otmp->quan = (long)otmp->spe;
                otmp->spe = 0;
                otmp->owt = weight(otmp);
                curse(otmp);
            } else if (otmp->otyp == BELL_OF_OPENING) {
                otmp->otyp = BELL;
                curse(otmp);
            } else if (otmp->otyp == SPE_BOOK_OF_THE_DEAD) {
                otmp->otyp = SPE_BLANK_PAPER;
                curse(otmp);
            }
        }
    }
}


static void
drop_upon_death(struct monst *mtmp, struct obj *cont)
{
    struct obj *otmp;

    /* This needs to come before we begin freeing objects from the inventory,
       or we'll panic when updating the cached list of items. */
    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (otmp->otyp == SLIME_MOLD)
            goodfruit(otmp->spe);

    while ((otmp = invent) != 0) {
        /* The desync detector dislikes floating wielded objects, even for a few
           lines of code. Also, we need to do mark it as unworn anyway to avoid
           worn items lying around on the ground for other players to find.

           This thus must be before curse() (can cause weapons to drop during
           twoweaponing, IIRC) and obj_extract_self() (drives the desync
           detector crazy).

           Because this unwear is due to death rather than due to more usual
           reasons, we do the unwear by hand, omitting the parts of the unwear
           that would affect the character (we're just focusing on what
           happens to the item, the character is dead). */
        otmp->owornmask = 0;

        obj_extract_self(otmp);
        obj_no_longer_held(otmp);

        /* lamps don't go out when dropped */
        if ((cont || artifact_light(otmp)) && obj_is_burning(otmp))
            end_burn(otmp, TRUE);       /* smother in statue */

        if (rn2(5))
            curse(otmp);
        if (mtmp)
            add_to_minv(mtmp, otmp);
        else if (cont)
            add_to_container(cont, otmp);
        else
            place_object(otmp, level, u.ux, u.uy);
    }
    if (cont)
        cont->owt = weight(cont);
}

/* check whether bones are feasible */
boolean
can_make_bones(d_level *lev)
{
    struct trap *ttmp;

    if (ledger_no(lev) <= 0 || ledger_no(lev) > maxledgerno())
        return FALSE;
    if (no_bones_level(lev))
        return FALSE;   /* no bones for specific levels */
    if (Engulfed) {
        return FALSE;   /* no bones when swallowed */
    }
    if (!Is_branchlev(lev)) {
        /* no bones on non-branches with portals */
        for (ttmp = level->lev_traps; ttmp; ttmp = ttmp->ntrap)
            if (ttmp->ttyp == MAGIC_PORTAL)
                return FALSE;
    }

    turnstate.generating_bones = TRUE;
    int rn2chance = rn2(1 + (depth(lev) >> 2));
    turnstate.generating_bones = FALSE;

    if (depth(lev) <= 0 ||      /* bulletproofing for endgame */
        (!rn2chance             /* fewer ghosts on low levels */
         && !wizard))
        return FALSE;
    /* don't let multiple restarts generate multiple copies of objects in bones 
       files */
    if (discover)
        return FALSE;
    /* don't drop multiple bones files from the same dungeon */
    if (*flags.setseed)
        return FALSE;
    return TRUE;
}


/* save bones and possessions of a deceased adventurer */
void
savebones(struct obj *corpse, boolean take_items)
{
    int fd, x, y;
    struct trap *ttmp;
    struct monst *mtmp;
    const struct permonst *mptr;
    struct fruit *f;
    char c, bonesid[10];
    struct memfile mf;
    struct obj *statue = 0;
    uchar cnamelth = 0, snamelth = 0;
    const char *whynot;

    /* Bones creation does require some calls to the RNG. Ensure that they are
    * reproduced correclty so as to get the same bones. */

    program_state.in_zero_time_command = FALSE;
    turnstate.generating_bones = TRUE; /* and we never set it back to FALSE */

    if (take_items)
        finish_paybill();

    mnew(&mf, NULL);

    /* caller has already checked `can_make_bones()' */

    clear_bypasses();
    make_bones_id(bonesid, &u.uz);
    fd = open_bonesfile(bonesid);
    if (fd >= 0) {
        close(fd);
        if (wizard) {
            if (yn("Bones file already exists.  Replace it?") == 'y') {
                if (delete_bonesfile(bonesid))
                    goto make_bones;
                else
                    pline("Cannot unlink old bones.");
            }
        }
        return;
    }

make_bones:
    unleash_all();
    /* in case these characters are not in their home bases */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        mptr = mtmp->data;
        if (mtmp->iswiz || mptr == &mons[PM_MEDUSA] ||
            mptr->msound == MS_NEMESIS || mptr->msound == MS_LEADER ||
            mptr == &mons[PM_VLAD_THE_IMPALER])
            mongone(mtmp);
    }
    if (u.usteed)
        dismount_steed(DISMOUNT_BONES);
    dmonsfree(level);   /* discard dead or gone monsters */

    /* clear level annotation */
    level->levname[0] = '\0';

    /* mark all fruits as nonexistent; when we come to them we'll mark them as
       existing (using goodfruit()) */
    for (f = gamestate.fruits.chain; f; f = f->nextf)
        f->fid = -f->fid;

    /* dispose of your possessions, usually cursed */
    if (u.ugrave_arise == (NON_PM - 1)) {
        /* embed your possessions in your statue */
        statue = mk_named_object(STATUE, &mons[u.umonnum], u.ux, u.uy,
                                 u.uplname);

        drop_upon_death(NULL, statue);
        if (!statue)
            return;     /* couldn't make statue */
        mtmp = NULL;
    } else if (u.ugrave_arise < LOW_PM) {
        /* drop everything */
        drop_upon_death(NULL, NULL);
        /* trick makemon() into allowing monster creation on your location */
        in_mklev = TRUE;
        mtmp = makemon(&mons[PM_GHOST], level, u.ux, u.uy, MM_NONAME);
        in_mklev = FALSE;
        if (!mtmp)
            return;
        mtmp = christen_monst(mtmp, u.uplname);
        if (corpse)
            corpse = obj_attach_mid(corpse, mtmp->m_id);
    } else {
        /* give your possessions to the monster you become */
        in_mklev = TRUE;
        mtmp = makemon(&mons[u.ugrave_arise], level, u.ux, u.uy, NO_MM_FLAGS);
        in_mklev = FALSE;
        if (!mtmp) {
            drop_upon_death(NULL, NULL);
            return;
        }
        mtmp = christen_monst(mtmp, u.uplname);
        newsym(u.ux, u.uy);
        pline("Your body rises from the dead as %s...",
              an(mons[u.ugrave_arise].mname));
        win_pause_output(P_MESSAGE);
        drop_upon_death(mtmp, NULL);
        m_dowear(mtmp, TRUE);
    }
    if (mtmp) {
        mtmp->m_lev = (u.ulevel ? u.ulevel : 1);
        mtmp->mhp = mtmp->mhpmax = u.uhpmax;
        mtmp->female = u.ufemale;
        mtmp->msleeping = 1;
    }
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        resetobjs(mtmp->minvent, FALSE);
        /* do not zero out m_ids for bones levels any more */
        mtmp->mlstmv = 0L;
        if (mtmp->mtame)
            mtmp->mtame = mtmp->mpeaceful = 0;
    }
    for (ttmp = level->lev_traps; ttmp; ttmp = ttmp->ntrap) {
        ttmp->madeby_u = 0;
        ttmp->tseen = (ttmp->ttyp == HOLE);
    }
    /* This will reset names; put them back for the corpse and/or statue. */
    if (corpse)
        cnamelth = corpse->onamelth;
    if (statue)
        snamelth = statue->onamelth;
    resetobjs(level->objlist, FALSE);
    resetobjs(level->buriedobjlist, FALSE);
    if (corpse)
        corpse->onamelth = cnamelth;
    if (statue)
        statue->onamelth = snamelth;

    /* Hero is no longer on the map. */
    u.ux = u.uy = 0;

    /* Clear all memory from the level. */
    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++) {
            level->locations[x][y].seenv = 0;
            level->locations[x][y].waslit = 0;
            clear_memory_glyph(x, y, S_unexplored);
        }

    fd = create_bonesfile(bonesid, &whynot);
    if (fd < 0) {
        if (wizard)
            pline("%s", whynot);

        /* bones file creation problems are silent to the player. Keep it that
           way, but place a clue into the paniclog. */
        paniclog("savebones", whynot);
        return;
    }
    c = (char)(strlen(bonesid) + 1);

    store_version(&mf);
    /* no tagging is useful here, as the tags in bones memfiles aren't used for 
       anything anyway */
    mwrite8(&mf, c);
    mwrite(&mf, bonesid, (unsigned)c);  /* DD.nnn */
    savefruitchn(&mf);
    update_mlstmv();    /* update monsters for eventual restoration */
    savelev(&mf, ledger_no(&u.uz));

    store_mf(fd, &mf);  /* also frees mf */

    close(fd);
    commit_bonesfile(bonesid);
}


int
getbones(d_level *levnum)
{
    int ok;
    char c, bonesid[10], oldbonesid[10];
    struct memfile mf;
    boolean from_file = FALSE;
    char *bonesfn = NULL;
    enum rng rng = rng_for_level(levnum);

    mnew(&mf, NULL);

    /* save bones files for real games; also turn them off in set-seed play
       because they wouldn't be the same for different players */
    if (discover || !flags.bones_enabled || *flags.setseed)
        goto fail;

    /* note: this rn2 call has to be before we check to see if a bones file
       actually exists, to avoid level gen desyncs due to bones file presence */

    /* wizard check added by GAN 02/05/87 */
    if (rn2_on_rng(3, rng)  /* only once in three times do we find bones */
        && !wizard)
        goto fail;
    if (no_bones_level(levnum))
        goto fail;

    make_bones_id(bonesid, levnum);

    if (log_want_replay('B')) {
        if (log_replay_input(0, "B!")) {
            goto record_fail;
        } else if (!log_replay_bones(&mf)) {
            log_replay_no_more_options();
        }
    } else if (program_state.followmode == FM_WATCH) {
        /* Bleh, we beat the main process to the bones file, and we can't
           lock things up in a menu on the client like usual. */
        terminate(RESTART_PLAY); /* one of the better of many bad options */
    } else {
        int fd = open_bonesfile(bonesid);

        if (fd == -1)
            goto record_fail;
        mf.buf = loadfile(fd, &mf.len);
        close(fd);
        if (!mf.buf)
            goto record_fail;

        from_file = TRUE;
    }

    log_record_bones(&mf);

    bonesfn = bones_filename(bonesid);
    if ((ok = uptodate(&mf, bonesfn)) == 0) {
        if (!wizard)
            pline("Discarding unuseable bones; no need to panic...");
    } else {

        if (wizard && yn("Get bones?") == 'n')
            goto fail;

        c = mread8(&mf);       /* length incl. '\0' */
        mread(&mf, oldbonesid, (unsigned)c);    /* DD.nnn */
        if (strcmp(bonesid, oldbonesid) != 0) {
            const char *errbuf;

            errbuf = msgprintf("This is bones level '%s', not '%s'!",
                               oldbonesid, bonesid);

            if (wizard) {
                pline("%s", errbuf);
                ok = FALSE;     /* won't die of trickery */
            }

            trickery(errbuf);
        } else {
            struct monst *mtmp;
            struct level *lev = getlev(&mf, ledger_no(levnum), TRUE);

            /* Note that getlev() now keeps tabs on unique monsters such as
               demon lords, and tracks the birth counts of all species just as
               makemon() does.  If a bones monster is extinct or has been
               subject to genocide, their mhpmax will be set to the magic
               DEFUNCT_MONSTER cookie value. */
            for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon) {
                if (mtmp->mhpmax == DEFUNCT_MONSTER)
                    mongone(mtmp);
                else
                    /* to correctly reset named artifacts on the level */
                    resetobjs(mtmp->minvent, TRUE);
            }
            resetobjs(lev->objlist, TRUE);
            resetobjs(lev->buriedobjlist, TRUE);
        }
    }
    mfree(&mf);
    free(bonesfn);

    if (wizard) {
        if (yn("Unlink bones?") == 'n') {
            return ok;
        }
    }
    if (from_file && !delete_bonesfile(bonesid)) {
        /* When N games try to simultaneously restore the same bones file, N-1
           of them will fail to delete it (the first N-1 under AmigaDOS, the
           last N-1 under UNIX). So no point in a mysterious message for a
           normal event -- just generate a new level for those N-1 games. */
        /* pline("Cannot unlink bones."); */
        freelev(ledger_no(levnum));
        return 0;
    }
    return ok;

record_fail:
    log_record_input("B!");
fail:
    if (bonesfn)
        free(bonesfn);
    mfree(&mf);
    return 0;
}

/*bones.c*/

