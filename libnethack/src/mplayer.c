/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-13 */
/* Copyright (c) Izchak Miller, 1992.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static const char *dev_name(void);
static void get_mplname(struct monst *, char *);
static void mk_mplayer_armor(struct monst *, short, enum rng);

/* These are the names of those who
 * contributed to the development of NetHack 3.2/3.3/3.4.
 *
 * Keep in alphabetical order within teams.
 * Same first name is entered once within each team.
 */
static const char *const developers[] = {
    /* devteam */
    "Dave", "Dean", "Eric", "Izchak", "Janet", "Jessie",
    "Ken", "Kevin", "Michael", "Mike", "Pat", "Paul", "Steve", "Timo",
    "Warwick",
    /* PC team */
    "Bill", "Eric", "Keizo", "Ken", "Kevin", "Michael", "Mike", "Paul",
    "Stephen", "Steve", "Timo", "Yitzhak",
    /* Amiga team */
    "Andy", "Gregg", "Janne", "Keni", "Mike", "Olaf", "Richard",
    /* Mac team */
    "Andy", "Chris", "Dean", "Jon", "Jonathan", "Kevin", "Wang",
    /* Atari team */
    "Eric", "Marvin", "Warwick",
    /* NT team */
    "Alex", "Dion", "Michael",
    /* OS/2 team */
    "Helge", "Ron", "Timo",
    /* VMS team */
    "Joshua", "Pat",
    ""
};


/* return a randomly chosen developer name */
static const char *
dev_name(void)
{
    int i, m = 0, n = SIZE(developers);
    struct monst *mtmp;
    boolean match;

    do {
        match = FALSE;
        i = rn2(n);
        for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
            if (!is_mplayer(mtmp->data) || !mtmp->mnamelth)
                continue;
            if (!strncmp(developers[i], NAME(mtmp), strlen(developers[i]))) {
                match = TRUE;
                break;
            }
        }
        m++;
    } while (match && m < 100); /* m for insurance */

    if (match)
        return NULL;
    return developers[i];
}

static void
get_mplname(struct monst *mtmp, char *nam)
{
    boolean fmlkind = is_female(mtmp->data);
    const char *devnam;

    devnam = dev_name();
    if (!devnam)
        strcpy(nam, fmlkind ? "Eve" : "Adam");
    else if (fmlkind && ! !strcmp(devnam, "Janet"))
        strcpy(nam, rn2(2) ? "Maud" : "Eve");
    else
        strcpy(nam, devnam);

    if (fmlkind || !strcmp(nam, "Janet"))
        mtmp->female = 1;
    else
        mtmp->female = 0;
    strcat(nam, " the ");
    strcat(nam, rank_of((int)mtmp->m_lev, monsndx(mtmp->data), mtmp->female));
}

static void
mk_mplayer_armor(struct monst *mon, short typ, enum rng rng)
{
    struct obj *obj;

    if (typ == STRANGE_OBJECT)
        return;
    obj = mksobj(mon->dlevel, typ, FALSE, FALSE, rng);
    if (!rn2_on_rng(3, rng))
        obj->oerodeproof = 1;
    if (!rn2_on_rng(3, rng))
        curse(obj);
    if (!rn2_on_rng(3, rng))
        bless(obj);
    /* Most players who get to the endgame who have cursed equipment have it
       because the wizard or other monsters cursed it, so its chances of having
       plusses is the same as usual.... */
    obj->spe = rn2_on_rng(10, rng) ?
        (rn2_on_rng(3, rng) ? rn2_on_rng(5, rng) : 4 + rn2_on_rng(4, rng)) :
        - 1 - rn2_on_rng(3, rng);
    mpickobj(mon, obj);
}

/* assumes rng is rng_main or rng_for_level(&lev->z) */
struct monst *
mk_mplayer(const struct permonst *ptr, struct level *lev, xchar x, xchar y,
           boolean special, enum rng rng)
{
    struct monst *mtmp;
    char nam[PL_NSIZ];

    if (!is_mplayer(ptr))
        return NULL;

    if (MON_AT(lev, x, y))
        rloc(m_at(lev, x, y), FALSE, lev);   /* insurance */

    if (!In_endgame(&u.uz))
        special = FALSE;

    if ((mtmp = makemon(ptr, lev, x, y,
                        rng == rng_main ? NO_MM_FLAGS : MM_ALLLEVRNG)) != 0) {
        short weapon = rn2_on_rng(2, rng) ?
            LONG_SWORD : rnd_class(SPEAR, BULLWHIP, rng);
        short armor =
            rnd_class(GRAY_DRAGON_SCALE_MAIL, YELLOW_DRAGON_SCALE_MAIL, rng);
        short cloak = !rn2_on_rng(8, rng) ? STRANGE_OBJECT :
            rnd_class(OILSKIN_CLOAK, CLOAK_OF_DISPLACEMENT, rng);
        short helm = !rn2_on_rng(8, rng) ? STRANGE_OBJECT :
            rnd_class(ELVEN_LEATHER_HELM, HELM_OF_TELEPATHY, rng);
        short shield = !rn2_on_rng(8, rng) ? STRANGE_OBJECT :
            rnd_class(ELVEN_SHIELD, SHIELD_OF_REFLECTION, rng);
        int quan;
        struct obj *otmp;

        mtmp->m_lev = rn2_on_rng(16, rng) + (special ? 15 : 1);
        mtmp->mhp = mtmp->mhpmax =
            4 * mtmp->m_lev + rn2_on_rng(mtmp->m_lev * 3 + 1, rng) + 30 +
            (special ? rn2_on_rng(30, rng) : 0);
        if (special) {
            get_mplname(mtmp, nam);
            mtmp = christen_monst(mtmp, nam);
            /* that's why they are "stuck" in the endgame :-) */
            mongets(mtmp, FAKE_AMULET_OF_YENDOR, rng);
        }
        msethostility(mtmp, TRUE, TRUE);

        switch (monsndx(ptr)) {
        case PM_ARCHEOLOGIST:
            if (rn2_on_rng(2, rng))
                weapon = BULLWHIP;
            break;
        case PM_BARBARIAN:
            if (rn2_on_rng(2, rng)) {
                weapon = rn2_on_rng(2, rng) ? TWO_HANDED_SWORD : BATTLE_AXE;
                shield = STRANGE_OBJECT;
            }
            if (rn2_on_rng(2, rng))
                armor = rnd_class(PLATE_MAIL, CHAIN_MAIL, rng);
            if (helm == HELM_OF_BRILLIANCE)
                helm = STRANGE_OBJECT;
            break;
        case PM_CAVEMAN:
        case PM_CAVEWOMAN:
            if (rn2_on_rng(4, rng))
                weapon = MACE;
            else if (rn2_on_rng(2, rng))
                weapon = CLUB;
            if (helm == HELM_OF_BRILLIANCE)
                helm = STRANGE_OBJECT;
            break;
        case PM_HEALER:
            if (rn2_on_rng(4, rng))
                weapon = QUARTERSTAFF;
            else if (rn2_on_rng(2, rng))
                weapon = rn2_on_rng(2, rng) ? UNICORN_HORN : SCALPEL;
            if (rn2_on_rng(4, rng))
                helm = rn2_on_rng(2, rng) ?
                    HELM_OF_BRILLIANCE : HELM_OF_TELEPATHY;
            if (rn2_on_rng(2, rng))
                shield = STRANGE_OBJECT;
            break;
        case PM_KNIGHT:
            if (rn2_on_rng(4, rng))
                weapon = LONG_SWORD;
            if (rn2_on_rng(2, rng))
                armor = rnd_class(PLATE_MAIL, CHAIN_MAIL, rng);
            break;
        case PM_MONK:
            weapon = STRANGE_OBJECT;
            armor = STRANGE_OBJECT;
            cloak = ROBE;
            if (rn2_on_rng(2, rng))
                shield = STRANGE_OBJECT;
            break;
        case PM_PRIEST:
        case PM_PRIESTESS:
            if (rn2_on_rng(2, rng))
                weapon = MACE;
            if (rn2_on_rng(2, rng))
                armor = rnd_class(PLATE_MAIL, CHAIN_MAIL, rng);
            if (rn2_on_rng(4, rng))
                cloak = ROBE;
            if (rn2_on_rng(4, rng))
                helm = rn2_on_rng(2, rng) ?
                    HELM_OF_BRILLIANCE : HELM_OF_TELEPATHY;
            if (rn2_on_rng(2, rng))
                shield = STRANGE_OBJECT;
            break;
        case PM_RANGER:
            if (rn2_on_rng(2, rng))
                weapon = ELVEN_DAGGER;
            break;
        case PM_ROGUE:
            if (rn2_on_rng(2, rng))
                weapon = SHORT_SWORD;
            break;
        case PM_SAMURAI:
            if (rn2_on_rng(2, rng))
                weapon = KATANA;
            break;
        case PM_TOURIST:
            /* Defaults are just fine */
            break;
        case PM_VALKYRIE:
            if (rn2_on_rng(2, rng))
                weapon = WAR_HAMMER;
            if (rn2_on_rng(2, rng))
                armor = rnd_class(PLATE_MAIL, CHAIN_MAIL, rng);
            break;
        case PM_WIZARD:
            if (rn2_on_rng(4, rng))
                weapon = rn2_on_rng(2, rng) ? QUARTERSTAFF : ATHAME;
            if (rn2_on_rng(2, rng)) {
                armor = rn2_on_rng(2, rng) ?
                    BLACK_DRAGON_SCALE_MAIL : SILVER_DRAGON_SCALE_MAIL;
                cloak = CLOAK_OF_MAGIC_RESISTANCE;
            }
            if (rn2_on_rng(4, rng))
                helm = HELM_OF_BRILLIANCE;
            shield = STRANGE_OBJECT;
            break;
        default:
            impossible("bad mplayer monster");
            weapon = 0;
            break;
        }

        if (weapon != STRANGE_OBJECT) {
            otmp = mksobj(level, weapon, TRUE, FALSE, rng);
            otmp->spe = (special ? 4 + rn2_on_rng(5, rng) : rn2_on_rng(4, rng));
            if (!rn2_on_rng(3, rng))
                otmp->oerodeproof = 1;
            else if (!rn2_on_rng(2, rng))
                otmp->greased = 1;
            if (special && rn2_on_rng(2, rng))
                otmp = mk_artifact(lev, otmp, A_NONE, rng);
            /* mplayers knew better than to overenchant Magicbane */
            if (otmp->oartifact == ART_MAGICBANE)
                otmp->spe = 1 + rn2_on_rng(4, rng);
            mpickobj(mtmp, otmp);
        }

        if (special) {
            if (!rn2_on_rng(10, rng))
                mongets(mtmp, rn2_on_rng(3, rng) ? LUCKSTONE : LOADSTONE, rng);
            mk_mplayer_armor(mtmp, armor, rng);
            mk_mplayer_armor(mtmp, cloak, rng);
            mk_mplayer_armor(mtmp, helm, rng);
            mk_mplayer_armor(mtmp, shield, rng);
            if (rn2_on_rng(8, rng))
                mk_mplayer_armor(mtmp, rnd_class(LEATHER_GLOVES,
                                                 GAUNTLETS_OF_DEXTERITY, rng),
                                 rng);
            if (rn2_on_rng(8, rng))
                mk_mplayer_armor(mtmp, rnd_class(LOW_BOOTS,
                                                 LEVITATION_BOOTS, rng), rng);
            m_dowear(mtmp, TRUE);

            quan = rn2_on_rng(3, rng) ?
                rn2_on_rng(3, rng) : rn2_on_rng(16, rng);
            while (quan--)
                mongets(mtmp, rnd_class(DILITHIUM_CRYSTAL, JADE, rng), rng);
            /* To get the gold "right" would mean a player can double his
               gold supply by killing one mplayer.  Not good. */
            mkmonmoney(mtmp, rn2_on_rng(1000, rng), rng);
            quan = rn2_on_rng(10, rng);
            while (quan--)
                mpickobj(mtmp, mkobj(level, RANDOM_CLASS, FALSE, rng));
        }
        quan = 1 + rn2_on_rng(3, rng);
        while (quan--)
            mongets(mtmp, rnd_offensive_item(mtmp, rng), rng);
        quan = 1 + rn2_on_rng(3, rng);
        while (quan--)
            mongets(mtmp, rnd_defensive_item(mtmp, rng), rng);
        quan = 1 + rn2_on_rng(3, rng);
        while (quan--)
            mongets(mtmp, rnd_misc_item(mtmp, rng), rng);
    }

    return mtmp;
}

/* Create the indicated number (num) of monster-players, randomly chosen, and in
   randomly chosen (free) locations on the level.  If "special", the size of num
   should not be bigger than the number of _non-repeated_ names in the
   developers array, otherwise a bunch of Adams and Eves will fill up the
   overflow.

   Uses the level generation RNG for the current level. */
void
create_mplayers(int num, boolean special)
{
    int pm, x, y;
    struct monst fakemon;

    enum rng rng = rng_for_level(&level->z);

    while (num) {
        int tryct = 0;

        /* roll for character class */
        pm = PM_ARCHEOLOGIST + rn2_on_rng(PM_WIZARD - PM_ARCHEOLOGIST + 1, rng);
        fakemon.data = &mons[pm];

        /* roll for an available location */
        do {
            x = 1 + rn2_on_rng(COLNO - 2, rng);
            y = 1 + rn2_on_rng(ROWNO - 2, rng);
            /* Special case: the player is on the map right now, don't desync
               the RNG by placing a monster on top of the player */
            while (x == u.ux && y == u.uy) {
                x = rnd(COLNO - 2);
                y = rnd(ROWNO - 2);
            }
        } while (!goodpos(level, x, y, &fakemon, 0) && tryct++ <= 50);

        /* if pos not found in 50 tries, don't bother to continue */
        if (tryct > 50)
            return;

        mk_mplayer(&mons[pm], level, (xchar) x, (xchar) y, special, rng);
        num--;
    }
}

void
mplayer_talk(struct monst *mtmp)
{
    static const char *const same_class_msg[3] = {
        "I can't win, and neither will you!",
        "You don't deserve to win!",
        "Mine should be the honor, not yours!",
    }, *const other_class_msg[3] = {
        "The low-life wants to talk, eh?",
        "Fight, scum!",
        "Here is what I have to say!",
    };

    if (mtmp->mpeaceful)
        return; /* will drop to humanoid talk */

    pline(msgc_npcvoice, "Talk? -- %s",
          (mtmp->data == &mons[urole.malenum] ||
           mtmp->data ==
           &mons[urole.femalenum]) ? same_class_msg[rn2(3)] :
          other_class_msg[rn2(3)]);
}

/*mplayer.c*/

