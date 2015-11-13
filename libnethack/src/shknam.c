/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-13 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* shknam.c -- initialize a shop */

#include "hack.h"
#include "eshk.h"

static void mkshobj_at(const struct shclass *shp, struct level *lev, int sx,
                       int sy);
static void nameshk(struct monst *shk, const char *const *nlp,
                    struct level *lev);
static int shkinit(const struct shclass *shp, struct level *lev,
                   struct mkroom *sroom);

static const char *const shkliquors[] = {
    /* Ukraine */
    "Njezjin", "Tsjernigof", "Ossipewsk", "Gorlowka",
    /* Belarus */
    "Gomel",
    /* N. Russia */
    "Konosja", "Weliki Oestjoeg", "Syktywkar", "Sablja",
    "Narodnaja", "Kyzyl",
    /* Silezie */
    "Walbrzych", "Swidnica", "Klodzko", "Raciborz", "Gliwice",
    "Brzeg", "Krnov", "Hradec Kralove",
    /* Schweiz */
    "Leuk", "Brig", "Brienz", "Thun", "Sarnen", "Burglen", "Elm",
    "Flims", "Vals", "Schuls", "Zum Loch",
    0
};

static const char *const shkbooks[] = {
    /* Eire */
    "Skibbereen", "Kanturk", "Rath Luirc", "Ennistymon", "Lahinch",
    "Kinnegad", "Lugnaquillia", "Enniscorthy", "Gweebarra",
    "Kittamagh", "Nenagh", "Sneem", "Ballingeary", "Kilgarvan",
    "Cahersiveen", "Glenbeigh", "Kilmihil", "Kiltamagh",
    "Droichead Atha", "Inniscrone", "Clonegal", "Lisnaskea",
    "Culdaff", "Dunfanaghy", "Inishbofin", "Kesh",
    0
};

static const char *const shkarmors[] = {
    /* Turquie */
    "Demirci", "Kalecik", "Boyabai", "Yildizeli", "Gaziantep",
    "Siirt", "Akhalataki", "Tirebolu", "Aksaray", "Ermenak",
    "Iskenderun", "Kadirli", "Siverek", "Pervari", "Malasgirt",
    "Bayburt", "Ayancik", "Zonguldak", "Balya", "Tefenni",
    "Artvin", "Kars", "Makharadze", "Malazgirt", "Midyat",
    "Birecik", "Kirikkale", "Alaca", "Polatli", "Nallihan",
    0
};

static const char *const shkwands[] = {
    /* Wales */
    "Yr Wyddgrug", "Trallwng", "Mallwyd", "Pontarfynach",
    "Rhaeader", "Llandrindod", "Llanfair-ym-muallt",
    "Y-Fenni", "Maesteg", "Rhydaman", "Beddgelert",
    "Curig", "Llanrwst", "Llanerchymedd", "Caergybi",
    /* Scotland */
    "Nairn", "Turriff", "Inverurie", "Braemar", "Lochnagar",
    "Kerloch", "Beinn a Ghlo", "Drumnadrochit", "Morven",
    "Uist", "Storr", "Sgurr na Ciche", "Cannich", "Gairloch",
    "Kyleakin", "Dunvegan",
    0
};

static const char *const shkrings[] = {
    /* Hollandse familienamen */
    "Feyfer", "Flugi", "Gheel", "Havic", "Haynin", "Hoboken",
    "Imbyze", "Juyn", "Kinsky", "Massis", "Matray", "Moy",
    "Olycan", "Sadelin", "Svaving", "Tapper", "Terwen", "Wirix",
    "Ypey",
    /* Skandinaviske navne */
    "Rastegaisa", "Varjag Njarga", "Kautekeino", "Abisko",
    "Enontekis", "Rovaniemi", "Avasaksa", "Haparanda",
    "Lulea", "Gellivare", "Oeloe", "Kajaani", "Fauske",
    0
};

static const char *const shkfoods[] = {
    /* Indonesia */
    "Djasinga", "Tjibarusa", "Tjiwidej", "Pengalengan",
    "Bandjar", "Parbalingga", "Bojolali", "Sarangan",
    "Ngebel", "Djombang", "Ardjawinangun", "Berbek",
    "Papar", "Baliga", "Tjisolok", "Siboga", "Banjoewangi",
    "Trenggalek", "Karangkobar", "Njalindoeng", "Pasawahan",
    "Pameunpeuk", "Patjitan", "Kediri", "Pemboeang", "Tringanoe",
    "Makin", "Tipor", "Semai", "Berhala", "Tegal", "Samoe",
    0
};

static const char *const shkweapons[] = {
    /* Perigord */
    "Voulgezac", "Rouffiac", "Lerignac", "Touverac", "Guizengeard",
    "Melac", "Neuvicq", "Vanzac", "Picq", "Urignac", "Corignac",
    "Fleac", "Lonzac", "Vergt", "Queyssac", "Liorac", "Echourgnac",
    "Cazelon", "Eypau", "Carignan", "Monbazillac", "Jonzac",
    "Pons", "Jumilhac", "Fenouilledes", "Laguiolet", "Saujon",
    "Eymoutiers", "Eygurande", "Eauze", "Labouheyre",
    0
};

static const char *const shktools[] = {
    /* Spmi */
    "Ymla", "Eed-morra", "Cubask", "Nieb", "Bnowr Falr", "Telloc Cyaj",
    "Sperc", "Noskcirdneh", "Yawolloh", "Hyeghu", "Niskal", "Trahnil",
    "Htargcm", "Enrobwem", "Kachzi Rellim", "Regien", "Donmyar",
    "Yelpur", "Nosnehpets", "Stewe", "Renrut", "_Zlaw", "Nosalnef",
    "Rewuorb", "Rellenk", "Yad", "Cire Htims", "Y-crad", "Nenilukah",
    "Corsh", "Aned", "Shimt", "Rathel",
#ifdef WIN32
    "Lechaim", "Lexa", "Niod",
#endif
    0
};

static const char *const shklight[] = {
    /* Romania */
    "Zarnesti", "Slanic", "Nehoiasu", "Ludus", "Sighisoara", "Nisipitu",
    "Razboieni", "Bicaz", "Dorohoi", "Vaslui", "Fetesti", "Tirgu Neamt",
    "Babadag", "Zimnicea", "Zlatna", "Jiu", "Eforie", "Mamaia",
    /* Bulgaria */
    "Silistra", "Tulovo", "Panagyuritshte", "Smolyan", "Kirklareli",
    "Pernik", "Lom", "Haskovo", "Dobrinishte", "Varvara", "Oryahovo",
    "Troyan", "Lovech", "Sliven",
    0
};

static const char *const shkgeneral[] = {
    /* Suriname */
    "Hebiwerie", "Possogroenoe", "Asidonhopo", "Manlobbi",
    "Adjama", "Pakka Pakka", "Kabalebo", "Wonotobo",
    "Akalapi", "Sipaliwini",
    /* Greenland */
    "Annootok", "Upernavik", "Angmagssalik",
    /* N. Canada */
    "Aklavik", "Inuvik", "Tuktoyaktuk",
    "Chicoutimi", "Ouiatchouane", "Chibougamau",
    "Matagami", "Kipawa", "Kinojevis",
    "Abitibi", "Maganasipi",
    /* Iceland */
    "Akureyri", "Kopasker", "Budereyri", "Akranes", "Bordeyri",
    "Holmavik",
    0
};

/*
 * To add new shop types, all that is necessary is to edit the shtypes[] array.
 * See mkroom.h for the structure definition.  Typically, you'll have to lower
 * some or all of the probability fields in old entries to free up some
 * percentage for the new type.
 *
 * The placement type field is not yet used but will be in the near future.
 *
 * The iprobs array in each entry defines the probabilities for various kinds
 * of objects to be present in the given shop type.  You can associate with
 * each percentage either a generic object type (represented by one of the
 * *_CLASS macros) or a specific object (represented by an onames.h define).
 * In the latter case, prepend it with a unary minus so the code can know
 * (by testing the sign) whether to use mkobj() or mksobj().
 */

const struct shclass shtypes[] = {
    {"general store", RANDOM_CLASS, 44,
     D_SHOP, {{100, RANDOM_CLASS}, {0, 0}, {0, 0}}, shkgeneral},
    {"used armor dealership", ARMOR_CLASS, 14,
     D_SHOP, {{90, ARMOR_CLASS}, {10, WEAPON_CLASS}, {0, 0}},
     shkarmors},
    {"second-hand bookstore", SCROLL_CLASS, 10, D_SHOP,
     {{90, SCROLL_CLASS}, {10, SPBOOK_CLASS}, {0, 0}}, shkbooks},
    {"liquor emporium", POTION_CLASS, 10, D_SHOP,
     {{100, POTION_CLASS}, {0, 0}, {0, 0}}, shkliquors},
    {"antique weapons outlet", WEAPON_CLASS, 5, D_SHOP,
     {{90, WEAPON_CLASS}, {10, ARMOR_CLASS}, {0, 0}}, shkweapons},
    {"delicatessen", FOOD_CLASS, 5, D_SHOP,
     {{83, FOOD_CLASS}, {5, -POT_FRUIT_JUICE}, {4, -POT_BOOZE},
      {5, -POT_WATER}, {3, -ICE_BOX}}, shkfoods},
    {"jewelers", RING_CLASS, 3, D_SHOP,
     {{85, RING_CLASS}, {10, GEM_CLASS}, {5, AMULET_CLASS}, {0, 0}},
     shkrings},
    {"quality apparel and accessories", WAND_CLASS, 3, D_SHOP,
     {{90, WAND_CLASS}, {5, -LEATHER_GLOVES}, {5, -ELVEN_CLOAK}, {0, 0}},
     shkwands},
    {"hardware store", TOOL_CLASS, 3, D_SHOP,
     {{100, TOOL_CLASS}, {0, 0}, {0, 0}}, shktools},
    /* Actually shktools is ignored; the code specifically chooses a random
       implementor name (along with candle shops having random shopkeepers) */
    {"rare books", SPBOOK_CLASS, 3, D_SHOP,
     {{90, SPBOOK_CLASS}, {10, SCROLL_CLASS}, {0, 0}}, shkbooks},
    /* Shops below this point are "unique".  That is they must all have a
       probability of zero.  They are only created via the special level
       loader. */
    {"lighting store", TOOL_CLASS, 0, D_SHOP,
     {{32, -WAX_CANDLE}, {50, -TALLOW_CANDLE},
      {5, -BRASS_LANTERN}, {10, -OIL_LAMP}, {3, -MAGIC_LAMP}}, shklight},
    {NULL, 0, 0, 0, {{0, 0}, {0, 0}, {0, 0}}, 0}
};


/* Make an object of the appropriate type for a shop square.

   Uses the level generation RNG. */
static void
mkshobj_at(const struct shclass *shp, struct level *lev, int sx, int sy)
{
    struct monst *mtmp;
    int atype;
    const struct permonst *ptr;
    enum rng rng = rng_for_level(&lev->z);

    if (rn2_on_rng(100, rng) < depth(&lev->z) && !MON_AT(lev, sx, sy) &&
        (ptr = mkclass(&lev->z, S_MIMIC, 0, rng)) &&
        (mtmp = makemon(ptr, lev, sx, sy, MM_ALLLEVRNG)) != 0) {
        /* note: makemon will set the mimic symbol to a shop item */
        if (rn2_on_rng(10, rng) >= depth(&lev->z)) {
            mtmp->m_ap_type = M_AP_OBJECT;
            mtmp->mappearance = STRANGE_OBJECT;
        }
    } else {
        atype = get_shop_item(shp - shtypes, rng);
        if (atype < 0)
            mksobj_at(-atype, lev, sx, sy, TRUE, TRUE, rng);
        else
            mkobj_at(atype, lev, sx, sy, TRUE, rng);
    }
}

/* extract a shopkeeper name for the given shop type */
static void
nameshk(struct monst *shk, const char *const *nlp, struct level *lev)
{
    int i, trycnt, names_avail;
    const char *shname = 0;
    struct monst *mtmp;
    int name_wanted;
    s_level *sptr;

    if (nlp == shklight && In_mines(&lev->z)
        && (sptr = Is_special(&lev->z)) != 0 && sptr->flags.town) {
        /* special-case minetown lighting shk */
        shname = "Izchak";
        shk->female = FALSE;
    } else {
        /* We want variation from game to game, without needing the save and
           restore support which would be necessary for randomization; thus use
           ubirthday for deterministic random numbers, and use ledger_no rather
           than depth to keep mine town distinct. */
        int nseed = ((unsigned)u.ubirthday / 257U);

        name_wanted = ledger_no(&lev->z) + (nseed % 13) - (nseed % 5);
        if (name_wanted < 0)
            name_wanted += (13 + 5);
        shk->female = name_wanted & 1;

        for (names_avail = 0; nlp[names_avail]; names_avail++)
            continue;

        for (trycnt = 0; trycnt < 50; trycnt++) {
            if (nlp == shktools) {
                shname = shktools[rn2(names_avail)];
                shk->female = (*shname == '_');
                if (shk->female)
                    shname++;
            } else if (name_wanted < names_avail) {
                shname = nlp[name_wanted];
            } else if ((i = rn2(names_avail)) != 0) {
                shname = nlp[i - 1];
            } else if (nlp != shkgeneral) {
                nlp = shkgeneral;       /* try general names */
                for (names_avail = 0; nlp[names_avail]; names_avail++)
                    continue;
                continue;       /* next `trycnt' iteration */
            } else {
                shname = shk->female ? "Lucrezia" : "Dirk";
            }

            /* is name already in use on this level? */
            for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp) || (mtmp == shk) || !mtmp->isshk)
                    continue;
                if (strcmp(ESHK(mtmp)->shknam, shname))
                    continue;
                break;
            }
            if (!mtmp)
                break;  /* new name */
        }
    }
    strncpy(ESHK(shk)->shknam, shname, PL_NSIZ);
    ESHK(shk)->shknam[PL_NSIZ - 1] = 0;
}

/* create a new shopkeeper in the given room; uses level creation RNG */
static int
shkinit(const struct shclass *shp, struct level *lev, struct mkroom *sroom)
{
    int sh, sx, sy;
    struct monst *shk;

    /* place the shopkeeper in the given room */
    sh = sroom->fdoor;
    sx = lev->doors[sh].x;
    sy = lev->doors[sh].y;

    /* check that the shopkeeper placement is sane */
    if (sroom->irregular) {
        int rmno = (sroom - lev->rooms) + ROOMOFFSET;

        if (isok(sx - 1, sy) && !lev->locations[sx - 1][sy].edge &&
            (int)lev->locations[sx - 1][sy].roomno == rmno)
            sx--;
        else if (isok(sx + 1, sy) && !lev->locations[sx + 1][sy].edge &&
                 (int)lev->locations[sx + 1][sy].roomno == rmno)
            sx++;
        else if (isok(sx, sy - 1) && !lev->locations[sx][sy - 1].edge &&
                 (int)lev->locations[sx][sy - 1].roomno == rmno)
            sy--;
        else if (isok(sx, sy + 1) && !lev->locations[sx][sy + 1].edge &&
                 (int)lev->locations[sx][sy + 1].roomno == rmno)
            sx++;
        else
            goto shk_failed;
    } else if (sx == sroom->lx - 1)
        sx++;
    else if (sx == sroom->hx + 1)
        sx--;
    else if (sy == sroom->ly - 1)
        sy++;
    else if (sy == sroom->hy + 1)
        sy--;
    else {
    shk_failed:
        return -1;
    }

    if (MON_AT(lev, sx, sy))
        rloc(m_at(lev, sx, sy), FALSE, lev); /* insurance */

    /* now initialize the shopkeeper monster structure */
    if (!(shk = makemon(&mons[PM_SHOPKEEPER], lev, sx, sy, MM_ALLLEVRNG)))
        return -1;
    shk->isshk = 1;
    msethostility(shk, FALSE, TRUE);
    shk->msleeping = 0;
    shk->mtrapseen = ~0;        /* we know all the traps already */
    ESHK(shk)->shoproom = (sroom - lev->rooms) + ROOMOFFSET;
    sroom->resident = shk;
    ESHK(shk)->shoptype = sroom->rtype;
    assign_level(&(ESHK(shk)->shoplevel), &lev->z);
    ESHK(shk)->shd = lev->doors[sh];
    ESHK(shk)->shk.x = sx;
    ESHK(shk)->shk.y = sy;
    ESHK(shk)->robbed = 0L;
    ESHK(shk)->credit = 0L;
    ESHK(shk)->debit = 0L;
    ESHK(shk)->loan = 0L;
    ESHK(shk)->visitct = 0;
    ESHK(shk)->following = 0;
    ESHK(shk)->billct = 0;
    ESHK(shk)->bill_inactive = FALSE;

    /* initial capital */
    mkmonmoney(shk, 1030L + 30L * mklev_rn2(100, lev), rng_for_level(&lev->z));

    if (shp->shknms == shkrings)
        mongets(shk, TOUCHSTONE, rng_for_level(&lev->z));
    nameshk(shk, shp->shknms, lev);

    return sh;
}

/* stock a newly-created room with objects */
void
stock_room(int shp_indx, struct level *lev, struct mkroom *sroom)
{
    /* 
     * Someday soon we'll dispatch on the shdist field of shclass to do
     * different placements in this routine. Currently it only supports
     * shop-style placement (all squares except a row nearest the first
     * door get objects).
     */
    int sx, sy, sh;
    int rmno = (sroom - lev->rooms) + ROOMOFFSET;
    const struct shclass *shp = &shtypes[shp_indx];

    /* first, try to place a shopkeeper in the room */
    if ((sh = shkinit(shp, lev, sroom)) < 0)
        return;

    /* make sure no doorways without doors, and no */
    /* trapped doors, in shops.  */
    sx = lev->doors[sroom->fdoor].x;
    sy = lev->doors[sroom->fdoor].y;

    if (lev->locations[sx][sy].doormask == D_NODOOR) {
        lev->locations[sx][sy].doormask = D_ISOPEN;
        if (lev == level)
            newsym(sx, sy);
    }
    if (lev->locations[sx][sy].typ == SDOOR) {
        cvt_sdoor_to_door(&lev->locations[sx][sy], &lev->z);   /* .typ = DOOR */
        if (lev == level)
            newsym(sx, sy);
    }
    if (lev->locations[sx][sy].doormask & D_TRAPPED)
        lev->locations[sx][sy].doormask = D_LOCKED;

    if (lev->locations[sx][sy].doormask == D_LOCKED) {
        int m = sx, n = sy;

        if (inside_shop(lev, sx + 1, sy))
            m--;
        else if (inside_shop(lev, sx - 1, sy))
            m++;
        if (inside_shop(lev, sx, sy + 1))
            n--;
        else if (inside_shop(lev, sx, sy - 1))
            n++;
        make_engr_at(lev, m, n, "Closed for inventory", 0L, DUST);
    }

    for (sx = sroom->lx; sx <= sroom->hx; sx++)
        for (sy = sroom->ly; sy <= sroom->hy; sy++) {
            if (sroom->irregular) {
                if (lev->locations[sx][sy].edge ||
                    (int)lev->locations[sx][sy].roomno != rmno ||
                    distmin(sx, sy, lev->doors[sh].x, lev->doors[sh].y) <= 1)
                    continue;
            } else if ((sx == sroom->lx && lev->doors[sh].x == sx - 1) ||
                       (sx == sroom->hx && lev->doors[sh].x == sx + 1) ||
                       (sy == sroom->ly && lev->doors[sh].y == sy - 1) ||
                       (sy == sroom->hy && lev->doors[sh].y == sy + 1))
                continue;
            mkshobj_at(shp, lev, sx, sy);
        }

    /* 
     * Special monster placements (if any) should go here: that way,
     * monsters will sit on top of objects and not the other way around.
     */
}


/* does shkp's shop stock this item type? */
boolean
saleable(struct monst *shkp, struct obj *obj)
{
    int i, shp_indx = ESHK(shkp)->shoptype - SHOPBASE;
    const struct shclass *shp = &shtypes[shp_indx];

    if (shp->symb == RANDOM_CLASS)
        return TRUE;
    else
        for (i = 0; i < SIZE(shtypes[0].iprobs) && shp->iprobs[i].iprob; i++)
            if (shp->iprobs[i].itype < 0 ? shp->iprobs[i].itype ==
                -obj->otyp : shp->iprobs[i].itype == obj->oclass)
                return TRUE;
    /* not found */
    return FALSE;
}

/* positive value: class; negative value: specific object type */
int
get_shop_item(int type, enum rng rng)
{
    const struct shclass *shp = shtypes + type;
    int i, j;

    /* select an appropriate object type at random */
    for (j = 1 + rn2_on_rng(100, rng), i = 0;
         (j -= shp->iprobs[i].iprob) > 0; i++)
        continue;

    return shp->iprobs[i].itype;
}

/*shknam.c*/

