/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-21 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* This file contains the command routines dowhatis() and dohelp() and a few
   other help related facilities. It also handles farlooking for the API, and
   thus cannot use the msg* functions for string handing; it works using
   fixed-size buffers instead, which never escape this file into the rest of
   libnethack (although they do end up in the client). */

#include "hack.h"
#include "dlb.h"

static boolean append_str(const char **buf, const char *new_str,
                          int is_plur, int is_in);
static void mon_vision_summary(const struct monst *mtmp, char *outbuf);
static void describe_bg(int x, int y, int bg, char *buf);
static int describe_object(int x, int y, int votyp, char *buf, int known_embed,
                           boolean *feature_described);
static void describe_mon(int x, int y, int monnum, char *buf);
static void add_mon_info(struct nh_menulist *, const struct permonst *);
static void checkfile(const char *inp, const struct permonst *,
                      boolean, boolean);
static int do_look(boolean, const struct nh_cmd_arg *);

/* The explanations below are also used when the user gives a string
 * for blessed genocide, so no text should wholly contain any later
 * text.  They should also always contain obvious names (eg. cat/feline).
 */
const char *const monexplain[MAXMCLASSES] = {
    0,
    "ant or other insect", "blob", "cockatrice",
    "dog or other canine", "eye or sphere", "cat or other feline",
    "gremlin", "humanoid", "imp or minor demon",
    "jelly", "kobold", "leprechaun",
    "mimic", "nymph", "orc",
    "piercer", "quadruped", "rodent",
    "arachnid or centipede", "trapper or lurker above", "unicorn or horse",
    "vortex", "worm", "xan or other mythical/fantastic insect",
    "light", "zruty",

    "angelic being", "bat or bird", "centaur",
    "dragon", "elemental", "fungus or mold",
    "gnome", "giant humanoid", 0,
    "jabberwock", "Keystone Kop", "lich",
    "mummy", "naga", "ogre",
    "pudding or ooze", "quantum mechanic", "rust monster or disenchanter",
    "snake", "troll", "umber hulk",
    "vampire", "wraith or ghost", "xorn",
    "apelike creature", "zombie",

    "human or elf", "golem", "major demon", "sea monster", "lizard",
    "long worm tail", "mimic"
};

static const char invisexplain[] = "remembered, unseen, creature";

/* Concatenates new_str to *buf, returning the result back in *buf, with some
   grammatical fixes. (The previous documentation said "if new_str doesn't
   already exist as a substring of buf", but this appears to be inaccurate.)
   Returns TRUE if the string was appended, FALSE otherwise. */
static boolean
append_str(const char **buf, const char *new_str, int is_plur, int is_in)
{
    if (!new_str || !new_str[0])
        return FALSE;

    if (**buf)
        *buf = msgcat(*buf, is_in ? " in " : " on ");

    if (is_plur)
        *buf = msgcat(*buf, new_str);
    else
        *buf = msgcat(*buf, an(new_str));

    return TRUE;
}

/* Like the above, except with commas, and into a BUFSZ-sized buffer for API
   boundary uses. */
static boolean
append_str_comma(char *buf, char **end_of_buf, const char *new_str)
{
    int remaining_space = BUFSZ - (*end_of_buf - buf);
    int newlen = strlen(new_str);

    if (remaining_space < 2 + newlen)
        return FALSE; /* just leave this portion out */

    if (remaining_space < BUFSZ) {
        memcpy(*end_of_buf, ", ", 2);
        *end_of_buf += 2;
    }

    memcpy(*end_of_buf, new_str, newlen + 1);
    *end_of_buf += newlen;

    return TRUE;
}

static void
mon_vision_summary(const struct monst *mtmp, char *outbuf)
{
    unsigned msense_status = msensem(&youmonst, mtmp);
    char *outbufp = outbuf;
    char wbuf[BUFSZ];

    outbuf[0] = '\0';

    if (msense_status & MSENSE_VISION)
        append_str_comma(outbuf, &outbufp, "normal vision");
    if (msense_status & MSENSE_SEEINVIS)
        append_str_comma(outbuf, &outbufp, "see invisible");
    if (msense_status & MSENSE_INFRAVISION)
        append_str_comma(outbuf, &outbufp, "infravision");
    if (msense_status & MSENSE_DISPLACED)
        append_str_comma(outbuf, &outbufp, "displacement");
    if (msense_status & MSENSE_TELEPATHY)
        append_str_comma(outbuf, &outbufp, "telepathy");
    if (msense_status & MSENSE_XRAY)
        append_str_comma(outbuf, &outbufp, "astral vision");
    if (msense_status & MSENSE_MONDETECT)
        append_str_comma(outbuf, &outbufp, "monster detection");
    if (msense_status & MSENSE_WARNOFMON) {
        snprintf(wbuf, SIZE(wbuf), "warned of %s",
                 makeplural(pm_name(mtmp)));
        append_str_comma(outbuf, &outbufp,
                         Hallucination ? "paranoid delusion" : wbuf);
    }
    if (msense_status & MSENSE_COVETOUS)
        append_str_comma(outbuf, &outbufp, "artifact sense");
    if (msense_status & MSENSE_GOLDSMELL)
        append_str_comma(outbuf, &outbufp, "smell of gold");
    if (msense_status & MSENSE_SCENT)
        append_str_comma(outbuf, &outbufp, "scent");
    if (msense_status & MSENSE_TEAMTELEPATHY)
        append_str_comma(outbuf, &outbufp, "cooperative telepathy");
    if (msense_status & MSENSE_AGGRAVATE)
        append_str_comma(outbuf, &outbufp, "aggravate monster");
    if (strcmp(outbuf, "normal vision") == 0)
        outbuf[0] = '\0';
}


static void
describe_bg(int x, int y, int bg, char *buf)
{
    if (!bg) {
        sprintf (buf, "unexplored area");
        return;
    }

    switch (bg) {
    case S_ndoor:
        if (is_drawbridge_wall(x, y) >= 0)
            strcpy(buf, "open drawbridge portcullis");
        else if ((level->locations[x][y].flags & ~D_TRAPPED) == D_BROKEN)
            strcpy(buf, "broken door");
        else
            strcpy(buf, "doorway");
        break;

    case S_cloud:
        strcpy(buf, Is_airlevel(&u.uz) ? "cloudy area" : "fog/vapor cloud");
        break;

    default:
        strcpy(buf, defexplain[bg]);
        break;
    }
}


static int
describe_object(int x, int y, int votyp, char *buf, int known_embed,
                boolean *feature_described)
{
    int num_objs = 0;
    struct obj *otmp;
    int typ;

    *feature_described = FALSE;

    if (votyp == -1)
        return -1;

    otmp = vobj_at(x, y);

    if (!otmp || otmp->otyp != votyp) {
        /* We have a mimic. */
        if (votyp == STRANGE_OBJECT) {
            strcpy(buf, "strange object");
        } else {
            otmp = mktemp_sobj(level, votyp);
            otmp->corpsenm = PM_TENGU;
            /* (basic object only, no random features) */
            if (otmp->oclass == COIN_CLASS)
                otmp->quan = 1L;        /* to force pluralization off */
            else if (otmp->otyp == SLIME_MOLD)
                otmp->spe = gamestate.fruits.current;/* give the fruit a type */
            strcpy(buf, distant_name(otmp, xname));
            dealloc_obj(otmp);
            otmp = vobj_at(x, y);       /* make sure we don't point to the temp 
                                           obj any more */
        }
    } else
        strcpy(buf, distant_name(otmp, xname));

    boolean pile = level->locations[x][y].pile;
    if (pile)
        strcat(buf, " on other objects");

    typ = level->locations[x][y].typ;
    if (known_embed && IS_TREE(typ))
        strcat(buf, " stuck");
    else if (known_embed && (IS_ROCK(typ) || closed_door(level, x, y)))
        strcat(buf, " embedded");
    else if (IS_TREE(typ)) {
        strcat(buf, " stuck in a tree");
        *feature_described = TRUE;
    } else if (typ == STONE || typ == SCORR) {
        strcat(buf, " embedded in stone");
        *feature_described = TRUE;
    } else if (IS_WALL(typ) || typ == SDOOR) {
        strcat(buf, " embedded in a wall");
        *feature_described = TRUE;
    } else if (closed_door(level, x, y)) {
        strcat(buf, " embedded in a door");
        *feature_described = TRUE;
    } else if (is_pool(level, x, y)) {
        strcat(buf, " in water");
        *feature_described = TRUE;
    } else if (is_lava(level, x, y)) {
        strcat(buf, " in molten lava"); /* [can this ever happen?] */
        *feature_described = TRUE;
    }

    if (!cansee(x, y))
        return -1;      /* don't disclose the number of objects for location
                           out of LOS */

    if (!otmp)
        /* There is no object here. Since the player sees one it must be a
           mimic */
        return 1;

    if (otmp->otyp != votyp)
        /* Hero sees something other than the actual top object. Probably a
           mimic */
        num_objs++;

    for (; otmp; otmp = otmp->nexthere)
        num_objs++;

    return num_objs;
}


static void
describe_mon(int x, int y, int monnum, char *buf)
{
    char race[QBUFSZ];
    const char *name;
    boolean accurate = !Hallucination;
    char steedbuf[BUFSZ];
    struct monst *mtmp = NULL;
    char visionbuf[BUFSZ], temp_buf[BUFSZ];

    static const int maximum_output_len = 78;

    if (monnum == -1)
        return;

    if (u.ux == x && u.uy == y && senseself()) {
        /* if not polymorphed, show both the role and the race */
        race[0] = 0;
        if (!Upolyd)
            snprintf(race, SIZE(race), "%s ", urace.adj);

        sprintf(buf, "%s%s%s called %s", Invis ? "invisible " : "", race,
                u.ufemale ? mons[u.umonnum].fname : mons[u.umonnum].mname, u.uplname);

        if (u.usteed) {
            snprintf(steedbuf, SIZE(steedbuf), ", mounted on %s", y_monnam(u.usteed));
            /* assert((sizeof buf >= strlen(buf)+strlen(steedbuf)+1); */
            strcat(buf, steedbuf);
        }
        /* When you see yourself normally, no explanation is appended (even if
           you could also see yourself via other means). Sensing self while
           blind or swallowed is treated as if it were by normal vision (cf
           canseeself()). */
        if ((Invisible || u.uundetected) && !Blind && !Engulfed) {
            unsigned how = 0;

            if (Infravision)
                how |= 1;
            if (Unblind_telepat)
                how |= 2;
            if (Detect_monsters)
                how |= 4;

            if (how)
                sprintf(buf + strlen(buf), " [seen: %s%s%s%s%s]",
                        (how & 1) ? "infravision" : "",
                        /* add comma if telep and infrav */
                        ((how & 3) > 2) ? ", " : "",
                        (how & 2) ? "telepathy" : "",
                        /* add comma if detect and (infrav or telep or both) */
                        ((how & 7) > 4) ? ", " : "",
                        (how & 4) ? "monster detection" : "");
        }

    } else if (monnum >= NUMMONS) {
        monnum -= NUMMONS;
        if (monnum < WARNCOUNT)
            strcat(buf, warnexplain[monnum]);

        return;
    }

    mtmp = vismon_at(level, x, y);
    if (!mtmp)
        return;

    bhitpos.x = x;
    bhitpos.y = y;

    if (mtmp->data == &mons[PM_COYOTE] && accurate && !mtmp->mpeaceful)
        name = an(coyotename(mtmp));
    else
        name = distant_monnam(
            mtmp, (mtmp->mtame && accurate) ? "tame" :
            (mtmp->mpeaceful && accurate) ? "peaceful" : NULL,
            ARTICLE_A);

    boolean spotted = canspotmon(mtmp);

    if (!spotted && (mtmp->mx != x || mtmp->my != y) &&
        (mtmp->dx != x || mtmp->dy != y))
        name = "an unseen long worm";
    else if (!spotted)
        /* we can't safely impossible/panic from this point in the code;
           well, we /could/, but as it's called on every mouse movement,
           it'd quickly get very confusing and possibly lead to recursive
           panics; just put up an obvious message instead */
        name = "a <BUG: monster both seen and unseen>";

    snprintf(buf, BUFSZ-1, "%s%s",
             ((mtmp->mx != x || mtmp->my != y) &&
              (mtmp->dx != x || mtmp->dy != y)) ? "tail of " : "", name);
    buf[BUFSZ-1] = '\0';
    if (u.ustuck == mtmp)
        strcat(buf,
               (Upolyd &&
                sticks(youmonst.data)) ? ", being held" : ", holding you");
    if (mtmp->mleashed)
        strcat(buf, ", leashed to you");

    if (mtmp->mtrapped && cansee(mtmp->mx, mtmp->my)) {
        struct trap *t = t_at(level, mtmp->mx, mtmp->my);
        int tt = t ? t->ttyp : NO_TRAP;

        /* newsym lets you know of the trap, so mention it here */
        if (tt == BEAR_TRAP || tt == PIT || tt == SPIKED_PIT || tt == WEB)
            sprintf(buf + strlen(buf),
                    ", trapped in %s", an(trapexplain[tt - 1]));
    }

#ifdef DEBUG_AI
    if (wizard) {
        snprintf(temp_buf, SIZE(temp_buf),
                 " (%s), target %02x,%02x, muxy %02x%02x",
                 mtmp->mstrategy == st_none ? "none" :
                 mtmp->mstrategy == st_close ? "waits until close" :
                 mtmp->mstrategy == st_waiting ? "waiting" :
                 mtmp->mstrategy == st_mon ? "chasing mon" :
                 mtmp->mstrategy == st_obj ? "chasing obj" :
                 mtmp->mstrategy == st_trap ? "chasing trap" :
                 mtmp->mstrategy == st_square ? "chasing square" :
                 mtmp->mstrategy == st_ascend ? "seeking high altar" :
                 mtmp->mstrategy == st_wander ? "wandering" :
                 mtmp->mstrategy == st_escape ? "escaping" :
                 mtmp->mstrategy == st_heal ? "healing" : "unknown",
                 (int)mtmp->sx, (int)mtmp->sy,
                 (int)mtmp->mux, (int)mtmp->muy);
        strncat(buf, temp_buf, maximum_output_len - strlen(buf) - 1);
    }
#endif

    /* Don't mention how a long worm tail is seen; msensem() only works on
       monster heads. (Probably, the only unusual way to see a long worm
       tail is see invisible, anyway.) */
    if ((mtmp->mx == x && mtmp->my == y) ||
        (mtmp->dx == x && mtmp->dy == y)) {
        mon_vision_summary(mtmp, visionbuf);
        if (visionbuf[0]) {
            snprintf(temp_buf, SIZE(temp_buf), " [seen: %s]", visionbuf);
            strncat(buf, temp_buf, maximum_output_len - strlen(buf) - 1);
        }
    }
}


void
nh_describe_pos(int x, int y, struct nh_desc_buf *bufs, int *is_in)
{
    bufs->bgdesc[0] = '\0';
    bufs->trapdesc[0] = '\0';
    bufs->objdesc[0] = '\0';
    bufs->mondesc[0] = '\0';
    bufs->invisdesc[0] = '\0';
    bufs->effectdesc[0] = '\0';
    bufs->feature_described = FALSE;
    bufs->objcount = -1;

    if (is_in)
        *is_in = 0;

    if (!program_state.game_running || !isok(x, y))
        return;

    API_ENTRY_CHECKPOINT_RETURN_VOID_ON_ERROR();

    if (is_in) {
        if (IS_ROCK(level->locations[x][y].typ) || closed_door(level, x, y))
            *is_in = 1;
        else
            *is_in = 0;
    }

    int monid = dbuf_get_mon(x, y);
    int mem_bg = level->locations[x][y].mem_bg;

    describe_bg(x, y, mem_bg, bufs->bgdesc);

    int tt = level->locations[x][y].mem_trap;

    if (tt) {
        strcpy(bufs->trapdesc,
               trapexplain[level->locations[x][y].mem_trap - 1]);
        if (tt != BEAR_TRAP && tt != WEB && tt != STATUE_TRAP && mem_bg &&
            is_in)
            *is_in = 1;
    }

    bufs->objcount =
        describe_object(x, y, level->locations[x][y].mem_obj - 1, bufs->objdesc,
                        mem_bg && is_in, &bufs->feature_described);

    describe_mon(x, y, monid - 1, bufs->mondesc);

    if (level->locations[x][y].mem_invis)
        strcpy(bufs->invisdesc, invisexplain);

    if (Engulfed && (x != u.ux || y != u.uy)) {
        /* all locations when swallowed other than the hero are the monster */
        snprintf(bufs->effectdesc, SIZE(bufs->effectdesc), "interior of %s",
                Blind ? "a monster" : a_monnam(u.ustuck));
    }

    API_EXIT();
}

#define ADDPROP(prop, str)                              \
    if (pm_has_property(pm, (prop)))                    \
        buf = msgprintf("%s%s", buf && *buf ?           \
                        msgcat(buf, ", ") : "", str);
#define ADDMR(field, res, str)                          \
    if (field & (res))                                  \
        buf = msgprintf("%s%s", buf && *buf ?           \
                        msgcat(buf, ", ") : "", str);
#define APPENDC(cond, str)                              \
    if (cond)                                           \
        buf = msgprintf("%s%s", buf && *buf ?           \
                        msgcat(buf, ", ") : "", str);
static void
add_mon_info(struct nh_menulist *menu, const struct permonst *pm)
{
    const char *buf;
    int diff = monstr[monsndx(pm)];
    int gen = pm->geno;
    int freq = (gen & G_FREQ);
    boolean uniq = !!(gen & G_UNIQ);
    boolean hell = !!(gen & G_HELL);
    boolean nohell = !!(gen & G_NOHELL);
    uchar mcon = pm->mconveys;
    mcon &= ~(MR_ACID | MR_STONE); /* these don't do anything */
    unsigned int mflag1 = pm->mflags1;
    unsigned int mflag2 = pm->mflags2;

    /* Misc */
    buf = msgprintf("Difficulty %d, %s, willpower %d.", diff,
                    show_ac("%s %d", pm->ac), pm->mr);
    add_menutext(menu, buf);

    /* Speed */
    int uspeed = mcalcmove(&youmonst);
    buf = msgprintf("Speed: %d (%s).", pm->mmove,
                    !pm->mmove ? "immobile" :
                    uspeed > pm->mmove ? "slower than you" :
                    uspeed < pm->mmove ? "faster than you" :
                    "equally fast as you");
    add_menutext(menu, buf);

    /* Generation */
    if (uniq)
        buf = "Unique.";
    else if (!freq)
        buf = "Specially generated.";
    else
        buf = msgprintf("Normally %s%s, %s.",
                        hell ? "only appears in Gehennom" :
                        nohell ? "only appears outside Gehennom" :
                        "appears everywhere",
                        (gen & G_SGROUP) ? " in groups" :
                        (gen & G_LGROUP) ? " in large groups" : "",
                        freq >= 5 ? "very common" :
                        freq == 4 ? "common" :
                        freq == 3 ? "slightly rare" :
                        freq == 2 ? "rare" :
                        freq == 1 ? "very rare" :
                        "not randomly generated");
    add_menutext(menu, buf);

    /* Resistances */
    buf = NULL;
    ADDPROP(FIRE_RES, "fire");
    ADDPROP(COLD_RES, "cold");
    ADDPROP(SLEEP_RES, "sleep");
    ADDPROP(DISINT_RES, "disintegration");
    ADDPROP(SHOCK_RES, "shock");
    ADDPROP(POISON_RES, "poison");
    ADDPROP(ACID_RES, "acid");
    ADDPROP(STONE_RES, "petrification");
    ADDPROP(DRAIN_RES, "life-drain");
    ADDPROP(SICK_RES, "sickness");
    ADDPROP(ANTIMAGIC, "magic");
    if (buf)
        buf = msgprintf("Resists %s.", buf);
    else
        buf = "Has no resistances.";
    add_menutext(menu, buf);

    /* Corpse conveyances */
    buf = NULL;
    ADDMR(mcon, MR_FIRE, "fire");
    ADDMR(mcon, MR_COLD, "cold");
    ADDMR(mcon, MR_SLEEP, "sleep");
    ADDMR(mcon, MR_DISINT, "disintegration");
    ADDMR(mcon, MR_ELEC, "shock");
    ADDMR(mcon, MR_POISON, "poison");
    if (buf)
        buf = msgprintf("%s resistance", buf);
    ADDMR(mflag1, M1_TPORT, "teleportitis");
    ADDMR(mflag1, M1_TPORT_CNTRL, "teleport control");
    ADDMR(mflag2, M2_TELEPATHIC, "telepathy");
    if (!(gen & G_NOCORPSE)) {
        if (buf)
            buf = msgprintf("Corpse conveys %s.", buf);
        else
            buf = "Corpse conveys nothing.";
        add_menutext(menu, buf);
    }
    else
        add_menutext(menu, "Leaves no corpse.");

    /* Flag descriptions */
    buf = NULL;
    APPENDC(pm->msize == MZ_TINY, "tiny");
    APPENDC(pm->msize == MZ_SMALL, "small");
    APPENDC(pm->msize == MZ_LARGE, "large");
    APPENDC(pm->msize == MZ_HUGE, "huge");
    APPENDC(pm->msize == MZ_GIGANTIC, "gigantic");
    if (!buf) {
        /* for nonstandard sizes */
        APPENDC(verysmall(pm), "small");
        if (!buf)
            APPENDC(bigmonst(pm), "big");
    }

    APPENDC(!(gen & G_GENO), "ungenocideable");
    APPENDC(breathless(pm), "breathless");
    if (!breathless(pm))
        APPENDC(amphibious(pm), "amphibious");
    APPENDC(amorphous(pm), "amorphous");
    APPENDC(noncorporeal(pm), "unsolid");
    APPENDC(acidic(pm), "acidic");
    APPENDC(poisonous(pm), "poisonous");
    APPENDC(pm->mflags1 & M1_REGEN, "regenerating");
    APPENDC(is_reviver(pm), "reviving");
    APPENDC(is_floater(pm), "floating");
    APPENDC(pm_invisible(pm), "invisible");
    APPENDC(is_undead(pm), "undead");
    if (!is_undead(pm))
        APPENDC(nonliving(pm), "nonliving");
    if (buf) {
        buf = msgprintf("Is %s.", buf);
        add_menutext(menu, buf);
        buf = NULL;
    }

    APPENDC(is_hider(pm), "hide");
    APPENDC(pm_swims(pm), "swim");
    if (!is_floater(pm)) /* overrides */
        APPENDC(pm->mflags1 & M1_FLY, "fly");
    APPENDC(pm->mflags1 & M1_WALLWALK, "phase through walls");
    APPENDC(pm->mflags1 & M1_TPORT, "teleport");
    APPENDC(is_clinger(pm), "cling");
    APPENDC(needspick(pm), "mine");
    if (!needspick(pm))
        APPENDC(tunnels(pm), "dig");
    if (buf) {
        buf = msgprintf("Can %s.", buf);
        add_menutext(menu, buf);
        buf = NULL;
    }

    /* Full-line remarks. */
    if (touch_petrifies(pm))
        add_menutext(menu, "Petrifies by touch.");
    if (pm->mflags1 & M1_SEE_INVIS)
        add_menutext(menu, "Can see invisible.");
    if (pm->mflags1 & M1_TPORT_CNTRL)
        add_menutext(menu, "Has teleport control.");
    if (your_race(pm))
        add_menutext(menu, "Is the same race as you.");
    if (!(gen & G_NOCORPSE)) {
        if (vegan(pm))
            add_menutext(menu, "May be eaten by vegans.");
        else if (vegetarian(pm))
            add_menutext(menu, "May be eaten by vegetarians.");
    }
    buf = msgprintf("Is %sa valid polymorph form.",
                    polyok(pm) ? "" : "not ");
    add_menutext(menu, buf);

    /* Attacks */
    buf = NULL;
    const char *atkbuf;
    int i;
    for (i = 0; i < 6; i++) {
        atkbuf = oneattack(&pm->mattk[i]);
        if (!atkbuf)
            break;
        if (!(i % 2)) {
            if (buf) /* more attacks to follow */
                add_menutext(menu, msgcat(buf, ","));
            buf = msgprintf("%s%s", !i ?
                            "Attacks: " :
                            "         ", atkbuf);
        } else
            buf = msgprintf("%s, %s", buf, atkbuf);
    }
    if (buf)
        add_menutext(menu, buf);
}
#undef APPENDC
#undef ADDMR
#undef ADDPROP

/* Look in the "data" file for more info.  Called if the user typed in the
   whole name (user_typed_name == TRUE), or we've found a possible match
   with a character/glyph. */
static void
checkfile(const char *inp, const struct permonst *pm,
          boolean user_typed_name, boolean without_asking)
{
    dlb *fp;
    char buf[BUFSZ], newstr[BUFSZ];
    char *ep, *dbase_str;
    long txt_offset = 0;
    int chk_skip;
    boolean found_in_file = FALSE, skipping_entry = FALSE;

    fp = dlb_fopen(DATAFILE, "r");
    if (!fp) {
        pline(msgc_saveload, "Cannot open data file!");
        return;
    }

    /* To prevent the need for entries in data.base like *ngel to account for
       Angel and angel, make the lookup string the same for both
       user_typed_name and picked name. */
    if (pm != NULL && !user_typed_name)
        dbase_str = strcpy(newstr, pm->mname);
    else
        dbase_str = strcpy(newstr, inp);

    for (ep = dbase_str; *ep; ep++)
        *ep = lowc(*ep);

    if (!strncmp(dbase_str, "interior of ", 12))
        dbase_str += 12;
    if (!strncmp(dbase_str, "a ", 2))
        dbase_str += 2;
    else if (!strncmp(dbase_str, "an ", 3))
        dbase_str += 3;
    else if (!strncmp(dbase_str, "the ", 4))
        dbase_str += 4;
    if (!strncmp(dbase_str, "tame ", 5))
        dbase_str += 5;
    else if (!strncmp(dbase_str, "peaceful ", 9))
        dbase_str += 9;
    if (!strncmp(dbase_str, "invisible ", 10))
        dbase_str += 10;
    if (!strncmp(dbase_str, "statue of ", 10))
        dbase_str[6] = '\0';
    else if (!strncmp(dbase_str, "figurine of ", 12))
        dbase_str[8] = '\0';

    /* Make sure the name is non-empty. */
    if (*dbase_str) {
        /* adjust the input to remove " [seen" and "named " and convert to
           lower case */
        const char *alt = 0;  /* alternate description */

        if ((ep = strstri_mutable(dbase_str, " [seen")) != 0)
            *ep = '\0';

        if ((ep = strstri_mutable(dbase_str, " named ")) != 0)
            alt = ep + 7;
        else
            ep = strstri_mutable(dbase_str, " called ");
        if (!ep)
            ep = strstri_mutable(dbase_str, ", ");
        if (ep && ep > dbase_str)
            *ep = '\0';

        /* 
         * If the object is named, then the name is the alternate description;
         * otherwise, the result of makesingular() applied to the name is. This
         * isn't strictly optimal, but named objects of interest to the user
         * will usually be found under their name, rather than under their
         * object type, so looking for a singular form is pointless.
         */

        if (!alt)
            alt = makesingular(dbase_str);
        else if (user_typed_name)
            alt = msglowercase(alt);

        /* skip first record; read second */
        txt_offset = 0L;
        if (!dlb_fgets(buf, BUFSZ, fp) || !dlb_fgets(buf, BUFSZ, fp)) {
            impossible("can't read 'data' file");
            dlb_fclose(fp);
            return;
        } else if (sscanf(buf, "%8lx\n", &txt_offset) < 1 || txt_offset <= 0)
            goto bad_data_file;

        /* look for the appropriate entry */
        while (dlb_fgets(buf, BUFSZ, fp)) {
            if (*buf == '.')
                break;  /* we passed last entry without success */

            if (digit(*buf)) {
                /* a number indicates the end of current entry */
                skipping_entry = FALSE;
            } else if (!skipping_entry) {
                if (!(ep = strchr(buf, '\n')))
                    goto bad_data_file;
                *ep = 0;
                /* if we match a key that begins with "~", skip this entry */
                chk_skip = (*buf == '~') ? 1 : 0;
                if (pmatch(&buf[chk_skip], dbase_str) ||
                    (alt && pmatch(&buf[chk_skip], alt))) {
                    if (chk_skip) {
                        skipping_entry = TRUE;
                        continue;
                    } else {
                        found_in_file = TRUE;
                        break;
                    }
                }
            }
        }
    }

    if (!pm) {
        /* Try to parse as a monster name for monster info */
        int mndx = name_to_mon(dbase_str);
        if (mndx >= 0)
            pm = &mons[mndx];
    }

    if (found_in_file) {
        long entry_offset;
        int entry_count;
        int i;

        /* skip over other possible matches for the info */
        do {
            if (!dlb_fgets(buf, BUFSZ, fp))
                goto bad_data_file;
        } while (!digit(*buf));

        if (sscanf(buf, "%ld,%d\n", &entry_offset, &entry_count) < 2) {
        bad_data_file:impossible("'data' file in wrong format");
            dlb_fclose(fp);
            return;
        }

        if (user_typed_name || without_asking || yn("More info?") == 'y') {
            struct nh_menulist menu;

            if (dlb_fseek(fp, txt_offset + entry_offset, SEEK_SET) < 0) {
                pline(msgc_saveload, "? Seek error on 'data' file!");
                dlb_fclose(fp);
                return;
            }

            init_menulist(&menu);

            if (pm) {
                add_mon_info(&menu, pm);
                add_menutext(&menu, "");
            }

            for (i = 0; i < entry_count; i++) {
                if (!dlb_fgets(buf, BUFSZ, fp))
                    goto bad_data_file;
                if ((ep = strchr(buf, '\n')) != 0)
                    *ep = 0;
                if (strchr(buf + 1, '\t') != 0)
                    tabexpand(buf + 1);
                add_menutext(&menu, buf + 1);
            }

            display_menu(&menu,
                         dbase_str && *dbase_str ?
                         msgupcasefirst(dbase_str) : NULL,
                         FALSE, PLHINT_ANYWHERE, NULL);
        }
    } else if (pm) {
        struct nh_menulist menu;

        init_menulist(&menu);
        add_mon_info(&menu, pm);
        display_menu(&menu, msgupcasefirst(pm->mname), FALSE,
                     PLHINT_ANYWHERE, NULL);
    } else if (user_typed_name)
        pline(msgc_info, "I don't have any information on those things.");

    dlb_fclose(fp);
}


/* also used by getpos hack in do_name.c */
static const char what_is_an_unknown_object[] = "an unknown object";

/* quick: use cursor && don't search for "more info" */
static int
do_look(boolean quick, const struct nh_cmd_arg *arg)
{
    const char *out_str;
    const char *firstmatch;
    int i, ans = 0, objplur = 0, is_in;
    coord cc;   /* screen pos of unknown glyph */
    boolean from_screen;        /* question from the screen */
    boolean is_warning = FALSE; /* is a warning symbol */
    struct nh_desc_buf descbuf;
    struct obj *otmp;

    if (arg->argtype & CMD_ARG_OBJ) {
        from_screen = FALSE;
    } else if (quick || (arg->argtype & CMD_ARG_POS)) {
        from_screen = TRUE;     /* yes, we want to use the cursor */
    } else {
        i = ynq("Specify unknown object by cursor?");
        if (i == 'q')
            return 0;
        from_screen = (i == 'y');
    }

    if (from_screen) {
        cc.x = u.ux;
        cc.y = u.uy;
    } else {
        if (arg->argtype & CMD_ARG_OBJ) {
            static const char allowall[] = { ALL_CLASSES, 0 };
            out_str = simple_typename(getargobj(arg, allowall, "explain")->otyp);
        } else {
            out_str = getarglin(arg, "Specify what? (type the word)");
            if (out_str[0] == '\0' || out_str[0] == '\033')
                return 0;
        }

        /* the ability to specify symbols is gone: it is simply impossible to
           know how the window port is displaying things (tiles?) and even if
           charaters are used it may not be possible to type them (utf8) */

        checkfile(out_str, NULL, !(arg->argtype & CMD_ARG_OBJ), TRUE);
        return 0;
    }

    /* we're identifying from the screen. */
    do {
        /* Reset some variables. */
        firstmatch = NULL;
        objplur = 0;

        pline(msgc_uiprompt, "Please move the cursor to %s.",
              what_is_an_unknown_object);

        ans = getargpos(arg, &cc, FALSE, what_is_an_unknown_object);
        if (ans == NHCR_CLIENT_CANCEL || cc.x < 0) {
            pline(msgc_cancelled, quick ? "Never mind." : "Done.");
            return 0;   /* done */
        }

        nh_describe_pos(cc.x, cc.y, &descbuf, &is_in);
        if (dbuf_get_mon(cc.x, cc.y) > NUMMONS)
            is_warning = TRUE;

        otmp = vobj_at(cc.x, cc.y);
        if (otmp && is_plural(otmp))
            objplur = 1;

        out_str = "";
        if (append_str(&out_str, descbuf.effectdesc, 0, 0))
            if (!firstmatch)
                firstmatch = descbuf.effectdesc;

        if (append_str(&out_str, descbuf.invisdesc, 0, 0))
            if (!firstmatch)
                firstmatch = descbuf.invisdesc;

        /* We already have a/an added by describe_mon; don't add it again,
           because that'll fail in cases like "Dudley's ghost" */
        struct monst *mon = NULL;
        if (append_str(&out_str, descbuf.mondesc, 1, 0)) {
            mon = vismon_at(level, cc.x, cc.y);
            if (!mon && cc.x == u.ux && cc.y == u.uy)
                mon = &youmonst;
            if (!canclassifymon(mon) || Hallucination)
                mon = NULL;

            if (!firstmatch)
                firstmatch = descbuf.mondesc;
        }

        if (append_str(&out_str, descbuf.objdesc, objplur, 0)) {
            if (!firstmatch)
                firstmatch = descbuf.objdesc;
            if (level->locations[cc.x][cc.y].pile)
                show_obj_memories_at(level, cc.x, cc.y);
        }

        if (append_str(&out_str, descbuf.trapdesc, 0, 0))
            if (!firstmatch)
                firstmatch = descbuf.trapdesc;

        if (!descbuf.feature_described &&
            append_str(&out_str, descbuf.bgdesc, 0, is_in))
            if (!firstmatch)
                firstmatch = descbuf.bgdesc;

        /* Finally, print out our explanation. */
        if (firstmatch) {
            pline(msgc_info, "%s.", msgupcasefirst(out_str));
            /* check the data file for information about this thing */
            if (firstmatch && ans != NHCR_CONTINUE && !is_warning &&
                (ans == NHCR_MOREINFO ||
                 ans == NHCR_MOREINFO_CONTINUE || !quick)) {
                checkfile(firstmatch, mon ? mon->data : NULL, FALSE,
                          ans == NHCR_MOREINFO ||
                          ans == NHCR_MOREINFO_CONTINUE);
            }
        } else {
            pline(msgc_info, "I've never heard of such things.");
        }
    } while (ans == NHCR_CONTINUE || ans == NHCR_MOREINFO_CONTINUE);

    if (!quick)
        pline(msgc_cancelled, "Done.");

    return 0;
}


int
dowhatis(const struct nh_cmd_arg *arg)
{
    return do_look(FALSE, arg);
}

/* TODO: CMD_ARG_POS is meaningful here, we should implement it. */
int
doquickwhatis(const struct nh_cmd_arg *arg)
{
    return do_look(TRUE, arg);
}

int
doidtrap(const struct nh_cmd_arg *arg)
{
    struct trap *trap;
    int x, y, tt;
    schar dx, dy, dz;

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    x = u.ux + dx;
    y = u.uy + dy;
    for (trap = level->lev_traps; trap; trap = trap->ntrap)
        if (trap->tx == x && trap->ty == y) {
            if (!trap->tseen)
                break;
            tt = trap->ttyp;
            if (dz) {
                if (dz < 0 ? (tt == TRAPDOOR || tt == HOLE) : tt == ROCKTRAP)
                    break;
            }
            /* This command is CMD_NOTIME, pick the RNG accordingly */
            tt = what_trap(tt, x, y, rn2_on_display_rng);
            pline(msgc_info, "That is %s%s%s.", an(trapexplain[tt - 1]),
                  !trap->madeby_u ? "" : (tt == WEB) ? " woven" :
                  /* trap doors & spiked pits can't be made by player, and
                     should be considered at least as much "set" as "dug"
                     anyway */
                  (tt == HOLE ||
                   tt == PIT) ? " dug" : " set",
                  !trap->madeby_u ? "" : " by you");
            return 0;
        }
    pline(msgc_info, "I can't see a trap there.");
    return 0;
}


int
dolicense(const struct nh_cmd_arg *arg)
{
    (void) arg;
    display_file(LICENSE, TRUE);
    return 0;
}


int
doverhistory(const struct nh_cmd_arg *arg)
{
    (void) arg;
    display_file(HISTORY, TRUE);
    return 0;
}

/*pager.c*/

