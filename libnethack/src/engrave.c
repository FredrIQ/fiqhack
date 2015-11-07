/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"
#include <ctype.h>

/* random engravings */
static const char *const random_mesg[] = {
    "Elbereth",
    /* trap engravings */
    "Vlad was here", "ad aerarium",
    /* take-offs and other famous engravings */
    "Owlbreath", "Galadriel",
    "Kilroy was here",
    "A.S. ->", "<- A.S.",       /* Journey to the Center of the Earth */
    "You won't get it up the steps",    /* Adventure */
    "Lasciate ogni speranza o voi ch'entrate.", /* Inferno */
    "Well Come",        /* Prisoner */
    "We apologize for the inconvenience.",      /* So Long... */
    "See you next Wednesday",   /* Thriller */
    "notary sojak",     /* Smokey Stover */
    "For a good time call 8?7-5309",
    "Please don't feed the animals.",   /* Various zoos around the world */
    "Madam, in Eden, I'm Adam.",        /* A palindrome */
    "Two thumbs up!",   /* Siskel & Ebert */
    "Hello, World!",    /* The First C Program */
    "As if!",   /* Clueless */
};

const char *
random_engraving(enum rng rng)
{
    const char *rumor;

    /* a random engraving may come from the "rumors" file, or from the list
       above */
    if (!rn2_on_rng(4, rng) ||
        !((rumor = getrumor(0, TRUE, NULL, rng))) || !*rumor)
        rumor = random_mesg[rn2_on_rng(SIZE(random_mesg), rng)];

    return eroded_text(rumor, (int)(strlen(rumor) / 4),
                       rn2_on_rng(255, rng) + 1);
}

/* Partial rubouts for engraving characters. -3. */
static const struct {
    char wipefrom;
    const char *wipeto;
} rubouts[] = {
    {
    'A', "^"}, {
    'B', "Pb["}, {
    'C', "("}, {
    'D', "|)["}, {
    'E', "|FL[_"}, {
    'F', "|-"}, {
    'G', "C("}, {
    'H', "|-"}, {
    'I', "|"}, {
    'K', "|<"}, {
    'L', "|_"}, {
    'M', "|"}, {
    'N', "|\\"}, {
    'O', "C("}, {
    'P', "F"}, {
    'Q', "C("}, {
    'R', "PF"}, {
    'T', "|"}, {
    'U', "J"}, {
    'V', "/\\"}, {
    'W', "V/\\"}, {
    'Z', "/"}, {
    'b', "|"}, {
    'd', "c|"}, {
    'e', "c"}, {
    'g', "c"}, {
    'h', "n"}, {
    'j', "i"}, {
    'k', "|"}, {
    'l', "|"}, {
    'm', "nr"}, {
    'n', "r"}, {
    'o', "c"}, {
    'q', "c"}, {
    'w', "v"}, {
    'y', "v"}, {
    ':', "."}, {
    ';', ","}, {
    '0', "C("}, {
    '1', "|"}, {
    '6', "o"}, {
    '7', "/"}, {
    '8', "3o"}
};

void
wipeout_text(char *engr, int cnt, unsigned seed)
{       /* for semi-controlled randomization */
    char *s;
    int i, j, nxt, use_rubout, lth = (int)strlen(engr);

    if (lth && cnt > 0) {
        while (cnt--) {
            /* pick next character */
            if (!seed) {
                /* random */
                nxt = rn2(lth);
                use_rubout = rn2(4);
            } else {
                /* predictable; caller can reproduce the same sequence by
                   supplying the same arguments later, or a pseudo-random
                   sequence by varying any of them */
                nxt = seed % lth;
                seed *= 31;
                seed %= 255; /* previously BUFSZ-1 */
                if (!seed)
                    seed++;
                use_rubout = seed & 3;
            }
            s = &engr[nxt];
            if (*s == ' ')
                continue;

            /* rub out unreadable & small punctuation marks */
            if (strchr("?.,'`-|_", *s)) {
                *s = ' ';
                continue;
            }

            if (!use_rubout)
                i = SIZE(rubouts);
            else
                for (i = 0; i < SIZE(rubouts); i++)
                    if (*s == rubouts[i].wipefrom) {
                        /* 
                         * Pick one of the substitutes at random.
                         */
                        if (!seed)
                            j = rn2(strlen(rubouts[i].wipeto));
                        else {
                            seed *= 31;
                            seed %= 255;
                            j = seed % (strlen(rubouts[i].wipeto));
                            if (!seed)
                                seed++;
                        }
                        *s = rubouts[i].wipeto[j];
                        break;
                    }

            /* didn't pick rubout; use '?' for unreadable character */
            if (i == SIZE(rubouts))
                *s = '?';
        }
    }

    /* trim trailing spaces */
    while (lth && engr[lth - 1] == ' ')
        engr[--lth] = 0;
}

const char *
eroded_text(const char *engr, int cnt, unsigned seed) {
    int len = strlen(engr);

    char buf[len + 1];
    strcpy(buf, engr);
    wipeout_text(buf, cnt, seed);

    return msg_from_string(buf);
}

boolean
can_reach_floor(void)
{
    return (boolean) (!Engulfed &&
                      /* Restricted/unskilled riders can't reach the floor */
                      !(u.usteed && P_SKILL(P_RIDING) < P_BASIC) &&
                      (!Levitation || Is_airlevel(&u.uz) ||
                       Is_waterlevel(&u.uz)));
}


const char *
surface(int x, int y)
{
    struct rm *loc = &level->locations[x][y];

    if ((x == u.ux) && (y == u.uy) && Engulfed && is_animal(u.ustuck->data))
        return "maw";
    else if (IS_AIR(loc->typ) && Is_airlevel(&u.uz))
        return "air";
    else if (is_pool(level, x, y))
        return (Underwater && !Is_waterlevel(&u.uz)) ? "bottom" : "water";
    else if (is_ice(level, x, y))
        return "ice";
    else if (is_lava(level, x, y))
        return "lava";
    else if (loc->typ == DRAWBRIDGE_DOWN)
        return "bridge";
    else if (IS_ALTAR(level->locations[x][y].typ))
        return "altar";
    else if (IS_GRAVE(level->locations[x][y].typ))
        return "headstone";
    else if (IS_FOUNTAIN(level->locations[x][y].typ))
        return "fountain";
    else if ((IS_ROOM(loc->typ) && !Is_earthlevel(&u.uz)) || IS_WALL(loc->typ)
             || IS_DOOR(loc->typ) || loc->typ == SDOOR)
        return "floor";
    else
        return "ground";
}

const char *
ceiling(int x, int y)
{
    struct rm *loc = &level->locations[x][y];
    const char *what;

    /* other room types will no longer exist when we're interested -- see
       check_special_room() */
    if (*in_rooms(level, x, y, VAULT))
        what = "vault's ceiling";
    else if (*in_rooms(level, x, y, TEMPLE))
        what = "temple's ceiling";
    else if (*in_rooms(level, x, y, SHOPBASE))
        what = "shop's ceiling";
    else if (IS_AIR(loc->typ))
        what = "sky";
    else if (Underwater)
        what = "water's surface";
    else if ((IS_ROOM(loc->typ) && !Is_earthlevel(&u.uz)) || IS_WALL(loc->typ)
             || IS_DOOR(loc->typ) || loc->typ == SDOOR)
        what = "ceiling";
    else
        what = "rock above";

    return what;
}

struct engr *
engr_at(struct level *lev, xchar x, xchar y)
{
    struct engr *ep = lev->lev_engr;

    while (ep) {
        if (x == ep->engr_x && y == ep->engr_y)
            return ep;
        ep = ep->nxt_engr;
    }
    return NULL;
}

/* Decide whether a particular string is engraved at a specified
 * location; a case-insensitive substring match used.
 * Ignore headstones, in case the player names herself "Elbereth".
 */
int
sengr_at(const char *s, xchar x, xchar y)
{
    struct engr *ep = engr_at(level, x, y);

    return (ep && ep->engr_type != HEADSTONE && ep->engr_time <= moves &&
            strstri(ep->engr_txt, s) != 0);
}


void
u_wipe_engr(int cnt)
{
    if (can_reach_floor())
        wipe_engr_at(level, u.ux, u.uy, cnt);
}


void
wipe_engr_at(struct level *lev, xchar x, xchar y, xchar cnt)
{
    struct engr *ep = engr_at(lev, x, y);

    /* Headstones are indelible */
    if (ep && ep->engr_type != HEADSTONE) {
        if (ep->engr_type != BURN || is_ice(lev, x, y)) {
            if (ep->engr_type != DUST && ep->engr_type != ENGR_BLOOD) {
                cnt = rn2(1 + 50 / (cnt + 1)) ? 0 : 1;
            }
            wipeout_text(ep->engr_txt, (int)cnt, 0);
            while (ep->engr_txt[0] == ' ')
                ep->engr_txt++;
            if (!ep->engr_txt[0])
                del_engr(ep, lev);
        }
    }
}


void
read_engr_at(int x, int y)
{
    struct engr *ep = engr_at(level, x, y);
    int sensed = 0;
    
    /* Sensing an engraving does not require sight, nor does it necessarily
       imply comprehension (literacy). */
    if (ep && ep->engr_txt[0] &&
        /* Don't stop if travelling or autoexploring. */
        !(travelling() && level->locations[x][y].mem_stepped)) {
        switch (ep->engr_type) {
        case DUST:
            if (!Blind) {
                sensed = 1;
                pline(msgc_info, "Something is written here in the %s.",
                      is_ice(level, x, y) ? "frost" : "dust");
            }
            break;
        case ENGRAVE:
        case HEADSTONE:
            if (!Blind || can_reach_floor()) {
                sensed = 1;
                pline(msgc_info, "Something is engraved here on the %s.",
                      surface(x, y));
            }
            break;
        case BURN:
            if (!Blind || can_reach_floor()) {
                sensed = 1;
                pline(msgc_info, "Some text has been %s into the %s here.",
                      is_ice(level, x, y) ? "melted" : "burned", surface(x, y));
            }
            break;
        case MARK:
            if (!Blind) {
                sensed = 1;
                pline(msgc_info, "There's some graffiti on the %s here.",
                      surface(x, y));
            }
            break;
        case ENGR_BLOOD:
            /* "It's a message! Scrawled in blood!" "What's it say?" "It
               says... `See you next Wednesday.'" -- Thriller */
            if (!Blind) {
                sensed = 1;
                pline(msgc_info, "You see a message scrawled in blood here.");
            }
            break;
        default:
            impossible("Something is written in a very strange way.");
            sensed = 1;
        }
        if (sensed) {
            /* AIS: Bounds check removed, because pline can now handle
               arbitrary-length strings */
            char *et = ep->engr_txt;
            pline(msgc_info, "You %s: \"%s\".",
                  (Blind) ? "feel the words" : "read", et);
            /* TODO: this comment previously said "For now, engravings stop
               farmove, autoexplore, and travel. This can get quite spammy,
               though.", but maybe doesn't match reality any more; check to see
               if this code still makes sense */
            if (flags.occupation == occ_move ||
                flags.occupation == occ_travel ||
                flags.occupation == occ_autoexplore)
                action_interrupted();
        }
    }
}


void
make_engr_at(struct level *lev, int x, int y, const char *s, long e_time,
             xchar e_type)
{
    struct engr *ep;
    size_t engr_len;

    if (!s || !*s)
        return;

    engr_len = strlen(s);

    if ((ep = engr_at(lev, x, y)) != 0)
        del_engr(ep, lev);
    ep = newengr(engr_len + 1);
    memset(ep, 0, sizeof (struct engr) + engr_len + 1);

    ep->nxt_engr = lev->lev_engr;
    lev->lev_engr = ep;
    ep->engr_x = x;
    ep->engr_y = y;
    ep->engr_txt = (char *)(ep + 1);
    strncpy(ep->engr_txt, s, engr_len);
    ep->engr_txt[engr_len] = '\0';
    while (ep->engr_txt[0] == ' ')
        ep->engr_txt++;
    /* engraving Elbereth shows wisdom */
    if (!in_mklev && !strcmp(s, "Elbereth"))
        exercise(A_WIS, TRUE);
    ep->engr_time = e_time;
    /* the caller shouldn't be asking for a random engrave type except during
       polymorph; if they do anyway, using the poly_engrave RNG isn't the end of
       the world */
    ep->engr_type = e_type > 0 ? e_type :
        1 + rn2_on_rng(N_ENGRAVE - 1, rng_poly_engrave);
    ep->engr_lth = engr_len + 1;
}

/* delete any engraving at location <x,y> */
void
del_engr_at(struct level *lev, int x, int y)
{
    struct engr *ep = engr_at(lev, x, y);

    if (ep)
        del_engr(ep, lev);
}

/*
 * freehand - returns true if player has a free hand
 */
int
freehand(void)
{
    return (!uwep || !welded(uwep) ||
            (!bimanual(uwep) && (!uarms || !uarms->cursed)));
/* if ((uwep && bimanual(uwep)) ||
           (uwep && uarms))
       return 0;
   else
       return 1;*/
}

static const char styluses[] =
    { ALL_CLASSES, ALLOW_NONE, TOOL_CLASS, WEAPON_CLASS, WAND_CLASS,
    GEM_CLASS, RING_CLASS, 0
};

/* Mohs' Hardness Scale:
 *  1 - Talc             6 - Orthoclase
 *  2 - Gypsum           7 - Quartz
 *  3 - Calcite          8 - Topaz
 *  4 - Fluorite         9 - Corundum
 *  5 - Apatite         10 - Diamond
 *
 * Since granite is a igneous rock hardness ~ 7, anything >= 8 should
 * probably be able to scratch the rock.
 * Devaluation of less hard gems is not easily possible because obj struct
 * does not contain individual oc_cost currently. 7/91
 *
 * steel     -  5-8.5   (usu. weapon)
 * diamond    - 10                      * jade       -  5-6      (nephrite)
 * ruby       -  9      (corundum)      * turquoise  -  5-6
 * sapphire   -  9      (corundum)      * opal       -  5-6
 * topaz      -  8                      * glass      - ~5.5
 * emerald    -  7.5-8  (beryl)         * dilithium  -  4-5??
 * aquamarine -  7.5-8  (beryl)         * iron       -  4-5
 * garnet     -  7.25   (var. 6.5-8)    * fluorite   -  4
 * agate      -  7      (quartz)        * brass      -  3-4
 * amethyst   -  7      (quartz)        * gold       -  2.5-3
 * jasper     -  7      (quartz)        * silver     -  2.5-3
 * onyx       -  7      (quartz)        * copper     -  2.5-3
 * moonstone  -  6      (orthoclase)    * amber      -  2-2.5
 */

/* TODO: This should be rewritten as an occupation, rather than in terms of
   helplessness. */
/* return 1 if action took 1 (or more) moves, 0 if error or aborted */
static int
doengrave_core(const struct nh_cmd_arg *arg, int auto_elbereth)
{
    boolean dengr = FALSE;      /* TRUE if we wipe out the current engraving */
    boolean doblind = FALSE;    /* TRUE if engraving blinds the player */
    boolean doknown = FALSE;    /* TRUE if we identify the stylus */
    boolean doknown_after = FALSE;      /* TRUE if we identify the stylus after
                                           successfully engraving. */
    boolean eow = FALSE;        /* TRUE if we are overwriting oep */
    boolean jello = FALSE;      /* TRUE if we are engraving in slime */
    boolean ptext = TRUE;       /* TRUE if we must prompt for engrave text */
    boolean teleengr = FALSE;   /* TRUE if we move the old engraving */
    boolean zapwand = FALSE;    /* TRUE if we remove a wand charge */
    xchar type = DUST;          /* Type of engraving made */
    const char *buf;            /* Buffer for final/poly engraving text */
    const char *ebuf;           /* Buffer for initial engraving text */
    const char *qbuf;           /* Buffer for query text */
    const char *post_engr_text; /* Text displayed after engraving prompt */
    const char *everb;          /* Present tense of engraving type */
    const char *eloc;           /* Where the engraving is (ie dust/floor/...) */
    const char *esp;            /* Iterator over ebuf; mostly handles spaces */
    char *sp;                   /* Ditto for mutable copies of ebuf */
    int len;                    /* # of nonspace chars of new engraving text */
    int maxelen;                /* Max allowable length of engraving text */
    int helpless_time;          /* Temporary for calculating helplessness */
    const char *helpless_endmsg;/* Temporary for helpless end message */
    struct engr *oep = engr_at(level, u.ux, u.uy);
    struct obj *otmp;

    /* The current engraving */
    const char *writer;

    buf = "";
    ebuf = "";
    post_engr_text = "";
    maxelen = 255; /* same value as in 3.4.3 */

    if (is_demon(youmonst.data) || youmonst.data->mlet == S_VAMPIRE)
        type = ENGR_BLOOD;

    /* Can the adventurer engrave at all? */

    if (Engulfed) {
        if (is_animal(u.ustuck->data)) {
            pline(msgc_cancelled, "What would you write?  \"Jonah was here\"?");
            return 0;
        } else if (is_whirly(u.ustuck->data)) {
            pline(msgc_cancelled, "You can't reach the %s.",
                  surface(u.ux, u.uy));
            return 0;
        } else
            jello = TRUE;
    } else if (is_lava(level, u.ux, u.uy)) {
        pline(msgc_cancelled, "You can't write on the lava!");
        return 0;
    } else if (Underwater) {
        pline(msgc_cancelled, "You can't write underwater!");
        return 0;
    } else if (is_pool(level, u.ux, u.uy) ||
               IS_FOUNTAIN(level->locations[u.ux][u.uy].typ)) {
        pline(msgc_cancelled, "You can't write on the water!");
        return 0;
    }
    if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) /* in bubble */ ) {
        pline(msgc_cancelled, "You can't write in thin air!");
        return 0;
    }
    if (cantwield(youmonst.data)) {
        pline(msgc_cancelled, "You can't even hold anything!");
        return 0;
    }
    if (check_capacity(NULL))
        return 0;

    /* One may write with finger, or weapon, or wand, or..., or... Edited by
       GAN 10/20/86 so as not to change weapon wielded. */

    otmp = getargobj(arg, styluses, "write with");
    if (!otmp)
        return 0;       /* otmp == &zeroobj if fingers */

    if (otmp == &zeroobj)
        writer = makeplural(body_part(FINGER));
    else
        writer = xname(otmp);

    /* There's no reason you should be able to write with a wand while both
       your hands are tied up. */
    if (!freehand() && otmp != uwep && !otmp->owornmask) {
        pline(msgc_cancelled, "You have no free %s to write with!",
              body_part(HAND));
        return 0;
    }

    if (jello) {
        /* TODO: The flavour text here implies that it's a msgc_cancelled1 or
           perhaps msgc_yafm situation. But it's actually a zero time cancel. */
        pline(msgc_cancelled, "You tickle %s with your %s.", mon_nam(u.ustuck),
              writer);
        pline(msgc_cancelled, "Your message dissolves...");
        return 0;
    }
    if (otmp->oclass != WAND_CLASS && !can_reach_floor()) {
        pline(msgc_cancelled, "You can't reach the %s!", surface(u.ux, u.uy));
        return 0;
    }
    if (IS_ALTAR(level->locations[u.ux][u.uy].typ)) {
        /* TODO: This takes zero time but has game effects (sort-of the reverse
           of msgc_cancelled1). */
        pline(msgc_badidea, "You make a motion towards the altar with your %s.",
              writer);
        altar_wrath(u.ux, u.uy);
        return 0;
    }
    if (IS_GRAVE(level->locations[u.ux][u.uy].typ)) {
        if (otmp == &zeroobj) { /* using only finger */
            pline(msgc_cancelled,
                  "You would only make a small smudge on the %s.",
                  surface(u.ux, u.uy));
            return 0;
        } else if (!level->locations[u.ux][u.uy].disturbed) {
            pline(msgc_badidea, "You disturb the undead!");
            level->locations[u.ux][u.uy].disturbed = 1;
            makemon(&mons[PM_GHOUL], level, u.ux, u.uy, NO_MM_FLAGS);
            exercise(A_WIS, FALSE);
            return 1;
        }
    }

    /* SPFX for items */

    switch (otmp->oclass) {
    default:
    case AMULET_CLASS:
    case CHAIN_CLASS:
    case POTION_CLASS:
    case COIN_CLASS:
        break;

    case RING_CLASS:
        /* "diamond" rings and others should work */
    case GEM_CLASS:
        /* diamonds & other hard gems should work */
        if (objects[otmp->otyp].oc_tough) {
            type = ENGRAVE;
            break;
        }
        break;

    case ARMOR_CLASS:
        if (is_boots(otmp)) {
            type = DUST;
            break;
        }
        /* fall through */
        /* Objects too large to engrave with */
    case BALL_CLASS:
    case ROCK_CLASS:
        /* also msgc_cancelled1 */
        pline(msgc_mispaste, "You can't engrave with such a large object!");
        ptext = FALSE;
        break;

        /* Objects too silly to engrave with */
    case FOOD_CLASS:
    case SCROLL_CLASS:
    case SPBOOK_CLASS:
        /* also msgc_cancelled1 */
        pline(msgc_mispaste, "Your %s would get %s.", xname(otmp),
              is_ice(level, u.ux, u.uy) ? "all frosty" : "too dirty");
        ptext = FALSE;
        break;

    case RANDOM_CLASS: /* This should mean fingers */
        break;

        /* The charge is removed from the wand before prompting for the
           engraving text, because all kinds of setup decisions and
           pre-engraving messages are based upon knowing what type of engraving 
           the wand is going to do.  Also, the player will have potentially
           seen "You wrest .." message, and therefore will know they are using
           a charge. */
    case WAND_CLASS:
        if (zappable(otmp)) {
            check_unpaid(otmp);
            zapwand = TRUE;
            if (Levitation)
                ptext = FALSE;

            switch (otmp->otyp) {
                /* DUST wands */
            default:
                break;

                /* NODIR wands */
            case WAN_LIGHT:
            case WAN_SECRET_DOOR_DETECTION:
            case WAN_CREATE_MONSTER:
            case WAN_WISHING:
            case WAN_ENLIGHTENMENT:
                zapnodir(otmp);
                break;

                /* IMMEDIATE wands */
                /* If wand is "IMMEDIATE", remember to affect the previous
                   engraving even if turning to dust. */
            case WAN_STRIKING:
                post_engr_text =
                    "The wand unsuccessfully fights your attempt to write!";
                doknown_after = TRUE;
                break;
            case WAN_SLOW_MONSTER:
                if (!Blind) {
                    post_engr_text = msgprintf("The bugs on the %s slow down!",
                                               surface(u.ux, u.uy));
                    doknown_after = TRUE;
                }
                break;
            case WAN_SPEED_MONSTER:
                if (!Blind) {
                    post_engr_text = msgprintf("The bugs on the %s speed up!",
                                               surface(u.ux, u.uy));
                    doknown_after = TRUE;
                }
                break;
            case WAN_POLYMORPH:
                if (oep) {
                    if (!Blind) {
                        type = (xchar) 0;       /* random */
                        buf = random_engraving(rng_main);
                        doknown = TRUE;
                    }
                    dengr = TRUE;
                }
                break;
            case WAN_NOTHING:
            case WAN_UNDEAD_TURNING:
            case WAN_OPENING:
            case WAN_LOCKING:
            case WAN_PROBING:
                break;

                /* RAY wands */
            case WAN_MAGIC_MISSILE:
                ptext = TRUE;
                if (!Blind) {
                    post_engr_text = msgprintf(
                        "The %s is riddled by bullet holes!",
                        surface(u.ux, u.uy));
                    doknown_after = TRUE;
                }
                break;

                /* can't tell sleep from death - Eric Backus */
            case WAN_SLEEP:
            case WAN_DEATH:
                if (!Blind) {
                    post_engr_text = msgprintf(
                        "The bugs on the %s stop moving!", surface(u.ux, u.uy));
                }
                break;

            case WAN_COLD:
                if (!Blind) {
                    post_engr_text = "A few ice cubes drop from the wand.";
                    doknown_after = TRUE;
                }
                if (!oep || (oep->engr_type != BURN))
                    break;
            case WAN_CANCELLATION:
            case WAN_MAKE_INVISIBLE:
                if (oep && oep->engr_type != HEADSTONE) {
                    if (!Blind)
                        pline(msgc_info, "The engraving on the %s vanishes!",
                              surface(u.ux, u.uy));
                    dengr = TRUE;
                }
                break;
            case WAN_TELEPORTATION:
                if (oep && oep->engr_type != HEADSTONE) {
                    if (!Blind)
                        pline(msgc_info, "The engraving on the %s vanishes!",
                              surface(u.ux, u.uy));
                    teleengr = TRUE;
                }
                break;

                /* type = ENGRAVE wands */
            case WAN_DIGGING:
                ptext = TRUE;
                type = ENGRAVE;
                if (!objects[otmp->otyp].oc_name_known) {
                    pline_implied(msgc_info, "This %s is a wand of digging!",
                                  xname(otmp));
                    doknown = TRUE;
                }
                if (!Blind)
                    post_engr_text =
                        IS_GRAVE(level->locations[u.ux][u.uy].typ) ?
                        "Chips fly out from the headstone." :
                        is_ice(level, u.ux, u.uy) ?
                        "Ice chips fly up from the ice surface!" :
                        "Gravel flies up from the floor.";
                else
                    post_engr_text = "You hear drilling!";
                break;

                /* type = BURN wands */
            case WAN_FIRE:
                ptext = TRUE;
                type = BURN;
                if (!objects[otmp->otyp].oc_name_known) {
                    pline_implied(msgc_info, "This %s is a wand of fire!",
                                  xname(otmp));
                    doknown = TRUE;
                }
                post_engr_text = Blind ?
                    "You feel the wand heat up." :
                    "Flames fly from the wand.";
                break;
            case WAN_LIGHTNING:
                ptext = TRUE;
                type = BURN;
                if (!objects[otmp->otyp].oc_name_known) {
                    pline_implied(msgc_info,
                                  "This %s is a wand of lightning!",
                                  xname(otmp));
                    doknown = TRUE;
                }
                if (!Blind) {
                    post_engr_text = "Lightning arcs from the wand.";
                    doblind = TRUE;
                } else
                    post_engr_text = "You hear crackling!";
                break;
            
                /* type = MARK wands */
                /* type = ENGR_BLOOD wands */
            }
        } else { /* i.e. not zappable */
            if (!can_reach_floor()) {
                pline(wrestable(otmp) ? msgc_failrandom : msgc_cancelled,
                      "You can't reach the %s!", surface(u.ux, u.uy));
                /* If it's a wrestable wand, the player wasted a turn trying. */
                if (wrestable(otmp))
                    return 1;
                else
                    return 0;
            }
        }
        break;
    
    case WEAPON_CLASS:
        if (is_blade(otmp)) {
            if ((int)otmp->spe > -3)
                type = ENGRAVE;
            else {
                pline(otmp->known ? msgc_cancelled1 : msgc_failcurse,
                      "Your %s too dull for engraving.",
                      aobjnam(otmp, "are"));
                otmp->known = TRUE;
            }
        }
        break;

    case TOOL_CLASS:
        if (otmp == ublindf) {
            pline(msgc_mispaste,
                  "That is a bit difficult to engrave with, don't you think?");
            return 0;
        }
        switch (otmp->otyp) {
        case MAGIC_MARKER:
            if (otmp->spe <= 0) {
                pline(otmp->known ? msgc_cancelled1 : msgc_failcurse,
                      "Your marker has dried out.");
                otmp->known = TRUE;
            } else
                type = MARK;
            break;
        case TOWEL:
            /* Can't really engrave with a towel */
            ptext = FALSE;
            if (oep)
                if ((oep->engr_type == DUST) || (oep->engr_type == ENGR_BLOOD)
                    || (oep->engr_type == MARK)) {
                    if (!Blind)
                        pline(msgc_actionok, "You wipe out the message here.");
                    else
                        pline(msgc_yafm, "Your %s %s %s.", xname(otmp),
                              otense(otmp, "get"),
                              is_ice(level, u.ux, u.uy) ? "frosty" : "dusty");
                    dengr = TRUE;
                } else
                    pline(msgc_cancelled1,
                          "Your %s can't wipe out this engraving.",
                          xname(otmp));
            else
                pline(msgc_yafm, "Your %s %s %s.", xname(otmp),
                      otense(otmp, "get"),
                      is_ice(level, u.ux, u.uy) ? "frosty" : "dusty");
            break;
        default:
            break;
        }
        break;

    case VENOM_CLASS:
    case ILLOBJ_CLASS:
        impossible("You're engraving with an illegal object!");
        break;
    }

    if (IS_GRAVE(level->locations[u.ux][u.uy].typ)) {
        if (type == ENGRAVE || type == 0)
            type = HEADSTONE;
        else {
            /* ensures the "cannot wipe out" case */
            type = DUST;
            dengr = FALSE;
            teleengr = FALSE;
            buf = "";
        }
    }

    /* End of implement setup */

    /* Identify stylus */
    if (doknown) {
        makeknown(otmp->otyp);
        more_experienced(0, 10);
    }

    if (teleengr) {
        rloc_engr(oep);
        oep = NULL;
    }

    if (dengr) {
        del_engr(oep, level);
        oep = NULL;
    }

    /* Something has changed the engraving here */
    if (*buf) {
        make_engr_at(level, u.ux, u.uy, buf, moves, type);
        pline(msgc_info, "The engraving now reads: \"%s\".", buf);
        ptext = FALSE;
    }

    if (zapwand && (otmp->spe < 0)) {
        pline(msgc_itemloss, "%s %sturns to dust.", The(xname(otmp)),
              Blind ? "" : "glows violently, then ");
        if (!IS_GRAVE(level->locations[u.ux][u.uy].typ))
            pline(msgc_yafm,
                  "You are not going to get anywhere trying to write in the "
                  "%s with your dust.",
                  is_ice(level, u.ux, u.uy) ? "frost" : "dust");
        useup(otmp);
        ptext = FALSE;
    }

    if (!ptext) {       /* Early exit for some implements. */
        if (otmp->oclass == WAND_CLASS && !can_reach_floor())
            pline(msgc_cancelled1, "You can't reach the %s!",
                  surface(u.ux, u.uy));
        return 1;
    }

    /* Special effects should have deleted the current engraving (if possible)
       by now. */

    if (oep) {
        char c = 'n';

        /* Give player the choice to add to engraving. */

        if (type == HEADSTONE) {
            /* no choice, only append */
            c = 'y';
        } else if ((type == oep->engr_type) &&
                   (!Blind || (oep->engr_type == BURN) ||
                    (oep->engr_type == ENGRAVE))) {
            if (auto_elbereth)
                c = 'y';
            else
                c = yn_function("Do you want to add to the current engraving?",
                                ynqchars, 'y');
            if (c == 'q') {
                pline(msgc_cancelled, "Never mind.");
                return 0;
            }
        }

        if (c == 'n' || Blind) {

            if ((oep->engr_type == DUST) || (oep->engr_type == ENGR_BLOOD) ||
                (oep->engr_type == MARK)) {
                if (!Blind) {
                    pline(msgc_actionok,
                          "You wipe out the message that was %s here.",
                          ((oep->engr_type == DUST) ? "written in the dust" :
                           ((oep->engr_type == ENGR_BLOOD) ?
                            "scrawled in blood" : "written")));
                    del_engr(oep, level);
                    oep = NULL;
                } else
                    /* Don't delete engr until after we *know* we're
                       engraving */
                    eow = TRUE;
            } else if ((type == DUST) || (type == MARK) ||
                       (type == ENGR_BLOOD)) {
                pline(msgc_cancelled1,
                      "You cannot wipe out the message that is %s the %s here.",
                      oep->engr_type == BURN ? (is_ice(level, u.ux, u.uy) ?
                                                "melted into" : "burned into") :
                      "engraved in",
                      surface(u.ux, u.uy));
                return 1;
            } else if ((type != oep->engr_type) || (c == 'n')) {
                if (!Blind || can_reach_floor())
                    pline_implied(msgc_hint,
                                  "You will overwrite the current message.");
                eow = TRUE;
            }
        }
    }

    eloc = surface(u.ux, u.uy);
    switch (type) {
    default:
        everb = (oep &&
                 !eow ? "add to the weird writing on" : "write strangely on");
        break;
    case DUST:
        everb = (oep && !eow ? "add to the writing in" : "write in");
        eloc = is_ice(level, u.ux, u.uy) ? "frost" : "dust";
        break;
    case HEADSTONE:
        everb = (oep && !eow ? "add to the epitaph on" : "engrave on");
        break;
    case ENGRAVE:
        everb = (oep && !eow ? "add to the engraving in" : "engrave in");
        break;
    case BURN:
        everb = (oep && !eow ? (is_ice(level, u.ux, u.uy) ?
                                "add to the text melted into" :
                                "add to the text burned into") :
                 (is_ice(level, u.ux, u.uy) ? "melt into" : "burn into"));
        break;
    case MARK:
        everb = (oep && !eow ? "add to the graffiti on" : "scribble on");
        break;
    case ENGR_BLOOD:
        everb = (oep && !eow ? "add to the scrawl on" : "scrawl on");
        break;
    }

    /* Tell adventurer what is going on */
    if (otmp != &zeroobj)
        pline(msgc_occstart, "You %s the %s with %s.", everb, eloc,
              doname(otmp));
    else
        pline(msgc_occstart, "You %s the %s with your %s.", everb, eloc,
              makeplural(body_part(FINGER)));

    /* Prompt for engraving! */
    qbuf = msgprintf("What do you want to %s the %s here?", everb, eloc);
    if (auto_elbereth)
        ebuf = "Elbereth";
    else
        ebuf = getarglin(arg, qbuf);

    /* Count the actual # of chars engraved not including spaces */
    len = strlen(ebuf);
    for (esp = ebuf; *esp; esp++)
        if (isspace(*esp))
            len -= 1;

    if (len == 0 || strchr(ebuf, '\033')) {
        if (zapwand) {
            if (!Blind)
                pline(msgc_yafm, "%s, then %s.", Tobjnam(otmp, "glow"),
                      otense(otmp, "fade"));
            return 1;
        } else {
            if (otmp && otmp->oclass == WAND_CLASS && wrestable(otmp)) {
                pline(msgc_yafm, "Never mind.");
                return 1;       /* disallow zero turn wrest */
            } else {
                pline(msgc_cancelled, "Never mind.");
                return 0;
            }
        }
    }

    /* A single `x' is the traditional signature of an illiterate person */
    if (len != 1 || (!strchr(ebuf, 'x') && !strchr(ebuf, 'X')))
        break_conduct(conduct_illiterate);

    /* Mix up engraving if surface or state of mind is unsound. Note: this
       won't add or remove any spaces. */

    char ebuf_copy[strlen(ebuf) + 1];
    strcpy(ebuf_copy, ebuf);
    for (sp = ebuf_copy; *sp; sp++) {
        if (isspace(*sp))
            continue;
        if (((type == DUST || type == ENGR_BLOOD) && !rn2(25)) ||
            (Blind && !rn2(11)) || (Confusion && !rn2(7)) ||
            (Stunned && !rn2(4)) || (Hallucination && !rn2(2)))
            *sp = ' ' + rnd(96 - 2);    /* ASCII '!' thru '~' (excludes ' ' and 
                                           DEL) */
    }

    /* Previous engraving is overwritten */
    if (eow) {
        del_engr(oep, level);
        oep = NULL;
    }

    /* Figure out how long it took to engrave, and if player has engraved too
       much. */
    helpless_time = len / 10;
    helpless_endmsg = NULL;
    switch (type) {
    default:
        helpless_endmsg = "You finish your weird engraving.";
        break;
    case DUST:
        helpless_endmsg = "You finish writing in the dust.";
        break;
    case HEADSTONE:
    case ENGRAVE:
        if ((otmp->oclass == WEAPON_CLASS) &&
            ((otmp->otyp != ATHAME) || otmp->cursed)) {
            helpless_time = len;
            maxelen = ((otmp->spe + 3) * 2) + 1;
            /*
             * -2 = 3, -1 = 5, 0 = 7, +1 = 9, +2 = 11
             * Note: this does not allow a +0 anything (except an athame) to
             * engrave "Elbereth" all at once.  However, you could now engrave
             * "Elb", then "ere", then "th".
             */
            pline(msgc_badidea, "Your %s dull.", aobjnam(otmp, "get"));
            if (otmp->unpaid) {
                struct monst *shkp = shop_keeper(level, *u.ushops);

                if (shkp) {
                    pline(msgc_unpaid, "You damage it, you pay for it!");
                    bill_dummy_object(otmp);
                }
            }
            if (len > maxelen) {
                helpless_time = maxelen;
                otmp->spe = -3;
            } else if (len > 1)
                otmp->spe -= len >> 1;
            else
                otmp->spe -= 1; /* Prevent infinite engraving */
        } else if ((otmp->oclass == RING_CLASS) || (otmp->oclass == GEM_CLASS))
            helpless_time = len;
        helpless_endmsg = "You finish engraving.";
        break;
    case BURN:
        helpless_endmsg =  is_ice(level, u.ux, u.uy) ?
            "You finish melting your message into the ice." :
            "You finish burning your message into the floor.";
        break;
    case MARK:
        if ((otmp->oclass == TOOL_CLASS) && (otmp->otyp == MAGIC_MARKER)) {
            maxelen = (otmp->spe) * 2;  /* one charge / 2 letters */
            if (len > maxelen) {
                pline(msgc_failcurse, "Your marker dries out.");
                otmp->spe = 0;
                otmp->known = TRUE;
                helpless_time = maxelen / 10;
            } else if (len > 1)
                otmp->spe -= len >> 1;
            else
                otmp->spe -= 1; /* Prevent infinite grafitti */
        }
        helpless_endmsg = "You finish defacing the dungeon.";
        break;
    case ENGR_BLOOD:
        helpless_endmsg = "You finish scrawling.";
        break;
    }

    /* Chop engraving down to size if necessary */
    if (len > maxelen) {
        for (sp = ebuf_copy; (maxelen && *sp); sp++)
            if (!isspace(*sp))
                maxelen--;
        if (!maxelen && *sp) {
            *sp = (char)0;
            helpless_endmsg = "You cannot write any more.";
            pline(msgc_substitute,
                  "You are only able to write \"%s\".", ebuf_copy);
        }
    }

    helpless(helpless_time, hr_engraving, "engraving", helpless_endmsg);

    /* Add to existing engraving */
    if (oep)
        buf = msg_from_string(oep->engr_txt);

    buf = msgcat(buf, msg_from_string(ebuf_copy));

    make_engr_at(level, u.ux, u.uy, buf, moves + helpless_time, type);

    if (strstri(buf, "Elbereth")) {
        break_conduct(conduct_elbereth);
    }

    if (post_engr_text[0])
        pline(msgc_info, "%s", post_engr_text);

    if (doknown_after) {
        makeknown(otmp->otyp);
        more_experienced(0, 10);
    }

    if (doblind && !resists_blnd(&youmonst)) {
        pline(msgc_statusbad, "You are blinded by the flash!");
        make_blinded((long)rnd(50), FALSE);
        if (!Blind)
            pline(msgc_statusheal, "Your vision quickly clears.");
    }

    return 1;
}

int
doengrave(const struct nh_cmd_arg *arg)
{
    return doengrave_core(arg, 0);
}

int
doelbereth(const struct nh_cmd_arg *arg)
{
    (void) arg;
    /* TODO: Athame? */
    return doengrave_core(&(struct nh_cmd_arg){
            .argtype = CMD_ARG_OBJ, .invlet = '-'}, 1);
}

void
save_engravings(struct memfile *mf, struct level *lev)
{
    struct engr *ep;
    char *txtbase;      /* ep->engr_txt may have been incremented */

    mfmagic_set(mf, ENGRAVE_MAGIC);

    for (ep = lev->lev_engr; ep; ep = ep->nxt_engr) {
        if (ep->engr_lth && ep->engr_txt[0]) {
            /* To distinguish engravingss from each other in tags, we use x/y/z 
               coords */
            mtag(mf,
                 ledger_no(&lev->z) + ((int)ep->engr_x << 8) +
                 ((int)ep->engr_y << 16), MTAG_ENGRAVING);
            mwrite32(mf, ep->engr_lth);
            mwrite8(mf, ep->engr_x);
            mwrite8(mf, ep->engr_y);
            mwrite8(mf, ep->engr_type);
            txtbase = (char *)(ep + 1);
            mwrite(mf, txtbase, ep->engr_lth);
        }
    }
    mwrite32(mf, 0);    /* no more engravings */
}


void
free_engravings(struct level *lev)
{
    struct engr *ep2, *ep = lev->lev_engr;

    while (ep) {
        ep2 = ep->nxt_engr;
        dealloc_engr(ep);
        ep = ep2;
    }
    lev->lev_engr = NULL;
}


void
rest_engravings(struct memfile *mf, struct level *lev)
{
    struct engr *ep, *eprev, *enext;
    unsigned lth;

    mfmagic_check(mf, ENGRAVE_MAGIC);
    lev->lev_engr = NULL;
    while (1) {
        lth = mread32(mf);
        if (!lth)       /* no more engravings */
            break;

        ep = newengr(lth);
        ep->engr_lth = lth;
        ep->engr_x = mread8(mf);
        ep->engr_y = mread8(mf);
        ep->engr_type = mread8(mf);
        ep->engr_txt = (char *)(ep + 1);
        mread(mf, ep->engr_txt, lth);

        ep->nxt_engr = lev->lev_engr;
        lev->lev_engr = ep;
        while (ep->engr_txt[0] == ' ')
            ep->engr_txt++;
        /* mark as finished for bones levels -- no problem for normal levels as 
           the player must have finished engraving to be able to move again 
        
           TODO: This comment isn't correct due to lifesaving, but this whole
           code is a mess anyway, and the whole engr_time system needs a rewrite
           (and an occupation callback). */
        ep->engr_time = moves;
    }

    /* engravings loaded above are reversed, so put it back in the right order
       */
    ep = lev->lev_engr;
    eprev = NULL;
    while (ep) {
        enext = ep->nxt_engr;
        ep->nxt_engr = eprev;
        eprev = ep;
        ep = enext;
    }
    lev->lev_engr = eprev;
}

void
del_engr(struct engr *ep, struct level *lev)
{
    if (ep == lev->lev_engr) {
        lev->lev_engr = ep->nxt_engr;
    } else {
        struct engr *ept;

        for (ept = lev->lev_engr; ept; ept = ept->nxt_engr)
            if (ept->nxt_engr == ep) {
                ept->nxt_engr = ep->nxt_engr;
                break;
            }
        if (!ept) {
            impossible("Error in del_engr?");
            return;
        }
    }
    dealloc_engr(ep);
}

/* randomly relocate an engraving */
void
rloc_engr(struct engr *ep)
{
    int tx, ty, tryct = 200;

    do {
        if (--tryct < 0)
            return;
        tx = rn2(COLNO);
        ty = rn2(ROWNO);
    } while (engr_at(level, tx, ty) || !goodpos(level, tx, ty, NULL, 0));

    ep->engr_x = tx;
    ep->engr_y = ty;
}


/* Epitaphs for random headstones */
static const char *const epitaphs[] = {
    "Rest in peace",
    "R.I.P.",
    "Rest In Pieces",
    "Note -- there are NO valuable items in this grave",
    "1994-1995. The Longest-Lived Hacker Ever",
    "The Grave of the Unknown Hacker",
    "We weren't sure who this was, but we buried him here anyway",
    "Sparky -- he was a very good dog",
    "Beware of Electric Third Rail",
    "Made in Taiwan",
    "Og friend. Og good dude. Og died. Og now food",
    "Beetlejuice Beetlejuice Beetlejuice",
    "Look out below!",
    "Please don't dig me up. I'm perfectly happy down here. -- Resident",
    "Postman, please note forwarding address: Gehennom, Asmodeus's Fortress, "
        "fifth lemure on the left",
    "Mary had a little lamb/Its fleece was white as snow/When Mary was in "
        "trouble/The lamb was first to go",
    "Be careful, or this could happen to you!",
    "Soon you'll join this fellow in hell! -- the Wizard of Yendor",
    "Caution! This grave contains toxic waste",
    "Sum quod eris",
    "Here lies an Atheist, all dressed up and no place to go",
    "Here lies Ezekiel, age 102.  The good die young.",
    "Here lies my wife: Here let her lie! Now she's at rest and so am I.",
    "Here lies Johnny Yeast. Pardon me for not rising.",
    "He always lied while on the earth and now he's lying in it",
    "I made an ash of myself",
    "Soon ripe. Soon rotten. Soon gone. But not forgotten.",
    "Here lies the body of Jonathan Blake. Stepped on the gas instead of the "
        "brake.",
    "Go away!"
};

/* Create a headstone at the given location.
   
   This is mosly for use in level creation. It can also be called outside level
   creation; in that case, the caller must call newsym(), and must provide an
   appropriate epitaph in order to avoid disturbing the level creation RNG. */
void
make_grave(struct level *lev, int x, int y, const char *str)
{
    /* Can we put a grave here? */
    if ((lev->locations[x][y].typ != ROOM && lev->locations[x][y].typ != GRAVE)
        || t_at(lev, x, y))
        return;

    /* Make the grave */
    lev->locations[x][y].typ = GRAVE;

    /* Engrave the headstone. */
    if (!str)
        str = epitaphs[rn2_on_rng(SIZE(epitaphs), rng_for_level(&lev->z))];
    del_engr_at(lev, x, y);
    make_engr_at(lev, x, y, str, 0L, HEADSTONE);
    return;
}

/*engrave.c*/

