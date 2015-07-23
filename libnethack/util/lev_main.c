/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright (c) 1989 by Jean-Christophe Collet */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * This file contains the main function for the parser
 * and some useful functions needed by yacc
 */
#define SPEC_LEV        /* for MPW */
/* although, why don't we move those special defines here.. and in dgn_main? */

#include "lev_compiler.h"

#ifndef O_WRONLY
# include <fcntl.h>
#endif
#ifndef O_CREAT /* some older BSD systems do not define O_CREAT in <fcntl.h> */
# include <sys/file.h>
#endif
#ifndef O_BINARY        /* used for micros, no-op for others */
# define O_BINARY 0
#endif

#if defined(WIN32)
# define OMASK FCMASK
#else
# define OMASK 0644
#endif

#include <inttypes.h>

#define ERR                   (-1)

#define NewTab(type, size)    malloc(sizeof(type *) * size)
#define Free(ptr)             if (ptr) free((ptr))
#define Write(fd, item, size) \
    if (write(fd, (item), size) != (intmax_t) size) return FALSE;

#define MAX_ERRORS            25

static boolean write_common_data(int, int, lev_init *, long);
static boolean write_monsters(int, char *, monster ***);
static boolean write_objects(int, char *, object ***);
static boolean write_engravings(int, char *, engraving ***);
static boolean write_maze(int, specialmaze *);
static boolean write_rooms(int, splev *);

static struct {
    const char *name;
    int type;
} trap_types[] = {
    {
    "arrow", ARROW_TRAP}, {
    "dart", DART_TRAP}, {
    "falling rock", ROCKTRAP}, {
    "board", SQKY_BOARD}, {
    "bear", BEAR_TRAP}, {
    "land mine", LANDMINE}, {
    "rolling boulder", ROLLING_BOULDER_TRAP}, {
    "sleep gas", SLP_GAS_TRAP}, {
    "rust", RUST_TRAP}, {
    "fire", FIRE_TRAP}, {
    "pit", PIT}, {
    "spiked pit", SPIKED_PIT}, {
    "hole", HOLE}, {
    "trap door", TRAPDOOR}, {
    "vibrating square", VIBRATING_SQUARE}, {
    "teleport", TELEP_TRAP}, {
    "level teleport", LEVEL_TELEP}, {
    "magic portal", MAGIC_PORTAL}, {
    "web", WEB}, {
    "statue", STATUE_TRAP}, {
    "magic", MAGIC_TRAP}, {
    "anti magic", ANTI_MAGIC}, {
    "polymorph", POLY_TRAP}, {
    0, 0}
};

static struct {
    const char *name;
    int type;
} room_types[] = {
    /* for historical reasons, room types are not contiguous numbers */
    /* (type 1 is skipped) */
    {
    "ordinary", OROOM}, {
    "throne", COURT}, {
    "swamp", SWAMP}, {
    "vault", VAULT}, {
    "beehive", BEEHIVE}, {
    "morgue", MORGUE}, {
    "barracks", BARRACKS}, {
    "zoo", ZOO}, {
    "delphi", DELPHI}, {
    "temple", TEMPLE}, {
    "anthole", ANTHOLE}, {
    "cocknest", COCKNEST}, {
    "leprehall", LEPREHALL}, {
    "shop", SHOPBASE}, {
    "armor shop", ARMORSHOP}, {
    "scroll shop", SCROLLSHOP}, {
    "potion shop", POTIONSHOP}, {
    "weapon shop", WEAPONSHOP}, {
    "food shop", FOODSHOP}, {
    "ring shop", RINGSHOP}, {
    "wand shop", WANDSHOP}, {
    "tool shop", TOOLSHOP}, {
    "book shop", BOOKSHOP}, {
    "candle shop", CANDLESHOP}, {
    0, 0}
};

static const char *outprefix = "";

const char *fname = "(stdin)";
int fatal_error = 0;
int want_warnings = 0;

int
main(int argc, char **argv)
{
    FILE *fin;
    int i;
    boolean errors_encountered = FALSE;

    init_objlist();

    init_yyout(stdout);
    if (argc == 1) {    /* Read standard input */
        init_yyin(stdin);
        yyparse();
        if (fatal_error > 0) {
            errors_encountered = TRUE;
        }
    } else {
        /* first two args may be "-o outprefix" */
        i = 1;
        if (!strcmp(argv[1], "-o") && argc > 3) {
            outprefix = argv[2];
            i = 3;
        }
        /* Otherwise every argument is a filename */
        for (; i < argc; i++) {
            fname = argv[i];
            if (!strcmp(fname, "-w")) {
                want_warnings++;
                continue;
            }

            fin = freopen(fname, "r", stdin);
            if (!fin) {
                fprintf(stderr, "Can't open \"%s\" for input.\n", fname);
                perror(fname);
                errors_encountered = TRUE;
            } else {
                init_yyin(fin);
                yyparse();
                line_number = 1;
                if (fatal_error > 0) {
                    errors_encountered = TRUE;
                    fatal_error = 0;
                }
            }
        }
    }

    free(objects);
    exit(errors_encountered ? EXIT_FAILURE : EXIT_SUCCESS);
     /*NOTREACHED*/ return 0;
}

/*
 * Each time the parser detects an error, it uses this function.
 * Here we take count of the errors. To continue farther than
 * MAX_ERRORS wouldn't be reasonable.
 * Assume that explicit calls from lev_comp.y have the 1st letter
 * capitalized, to allow printing of the line containing the start of
 * the current declaration, instead of the beginning of the next declaration.
 */
void
yyerror(const char *s)
{
    fprintf(stderr, "%s: line %d : %s\n", fname,
            (*s >= 'A' && *s <= 'Z') ? colon_line_number : line_number, s);
    if (++fatal_error > MAX_ERRORS) {
        fprintf(stderr, "Too many errors, good bye!\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Just display a warning (that is : a non fatal error)
 */
void
yywarning(const char *s)
{
    fprintf(stderr, "%s: line %d : WARNING : %s\n", fname, colon_line_number,
            s);
}

/*
 * Stub needed for lex interface.
 */
int
yywrap(void)
{
    return 1;
}

/*
 * Find the type of floor, knowing its char representation.
 */
int
get_floor_type(char c)
{
    int val;

    val = what_map_char(c);
    if (val == INVALID_TYPE) {
        val = ERR;
        yywarning("Invalid fill character in MAZE declaration");
    }
    return val;
}

/*
 * Find the type of a room in the table, knowing its name.
 */
int
get_room_type(char *s)
{
    int i;

    for (i = 0; room_types[i].name; i++)
        if (!strcmp(s, room_types[i].name))
            return (int)room_types[i].type;
    return ERR;
}

/*
 * Find the type of a trap in the table, knowing its name.
 */
int
get_trap_type(char *s)
{
    int i;

    for (i = 0; trap_types[i].name; i++)
        if (!strcmp(s, trap_types[i].name))
            return trap_types[i].type;
    return ERR;
}

/*
 * Find the index of a monster in the table, knowing its name.
 */
int
get_monster_id(char *s, char c)
{
    int i, class;

    class = c ? def_char_to_monclass(c) : 0;
    if (class == MAXMCLASSES)
        return ERR;

    for (i = LOW_PM; i < NUMMONS; i++)
        if (!class || class == mons[i].mlet)
            if (!strcmp(s, mons[i].mname))
                return i;
    return ERR;
}

/*
 * Find the index of an object in the table, knowing its name.
 */
int
get_object_id(char *s, char c)
{
    int i, class;
    const char *objname;

    class = (c > 0) ? def_char_to_objclass(c) : 0;
    if (class == MAXOCLASSES)
        return ERR;

    for (i = (class ? bases[class] : 0); i < NUM_OBJECTS; i++) {
        if (class && objects[i].oc_class != class) {
            if (i == bases[class]) {
                /* Sanity check: ensure we're using a current copy of
                   readonly.c */
                char msg[256];
                snprintf(msg, SIZE(msg), "Object class %d has base %d with class %d",
                        class, i, objects[i].oc_class);
                yyerror(msg);
            }
            break;
        }
        objname = obj_descr[i].oc_name;
        if (objname && !strcmp(s, objname))
            return i;
    }
    return ERR;
}

/*
 * Is the character 'c' a valid monster class ?
 */
boolean
check_monster_char(char c)
{
    return def_char_to_monclass(c) != MAXMCLASSES;
}

/*
 * Is the character 'c' a valid object class ?
 */
boolean
check_object_char(char c)
{
    return def_char_to_objclass(c) != MAXOCLASSES;
}

/*
 * Convert .des map letter into floor type.
 */
char
what_map_char(char c)
{
    switch (c) {
    case ' ':
        return STONE;
    case '#':
        return CORR;
    case '.':
        return ROOM;
    case '-':
        return HWALL;
    case '|':
        return VWALL;
    case '+':
        return DOOR;
    case 'A':
        return AIR;
    case 'B':
        return CROSSWALL;       /* hack: boundary location */
    case 'C':
        return CLOUD;
    case 'S':
        return SDOOR;
    case 'H':
        return SCORR;
    case '{':
        return FOUNTAIN;
    case '\\':
        return THRONE;
    case 'K':
        return SINK;
    case '}':
        return MOAT;
    case 'P':
        return POOL;
    case 'L':
        return LAVAPOOL;
    case 'I':
        return ICE;
    case 'W':
        return WATER;
    case 'T':
        return TREE;
    case 'F':
        return IRONBARS;        /* Fe = iron */
    }
    return INVALID_TYPE;
}

/*
 * Yep! LEX gives us the map in a raw mode.
 * Just analyze it here.
 */
void
scan_map(char *map)
{
    int i, len;
    char *s1, *s2;
    int max_len = 0;
    int max_hig = 0;
    char msg[256];

    /* First, strip out digits 0-9 (line numbering) */
    for (s1 = s2 = map; *s1; s1++)
        if (*s1 < '0' || *s1 > '9')
            *s2++ = *s1;
    *s2 = '\0';

    /* Second, find the max width of the map */
    s1 = map;
    while (s1 && *s1) {
        s2 = strchr(s1, '\n');
        if (s2) {
            len = (int)(s2 - s1);
            s1 = s2 + 1;
        } else {
            len = (int)strlen(s1);
            s1 = NULL;
        }
        if (len > max_len)
            max_len = len;
    }

    /* Then parse it now */
    while (map && *map) {
        tmpmap[max_hig] = malloc(max_len);
        s1 = strchr(map, '\n');
        if (s1) {
            len = (int)(s1 - map);
            s1++;
        } else {
            len = (int)strlen(map);
            s1 = map + len;
        }
        for (i = 0; i < len; i++)
            if ((tmpmap[max_hig][i] = what_map_char(map[i])) == INVALID_TYPE) {
                snprintf(msg, SIZE(msg),
                        "Invalid character @ (%d, %d) - replacing with stone",
                        max_hig, i);
                yywarning(msg);
                tmpmap[max_hig][i] = STONE;
            }
        while (i < max_len)
            tmpmap[max_hig][i++] = STONE;
        map = s1;
        max_hig++;
    }

    /* Memorize boundaries */

    max_x_map = max_len - 1;
    max_y_map = max_hig - 1;

    /* Store the map into the mazepart structure */

    if (max_len > MAP_X_LIM || max_hig > MAP_Y_LIM) {
        snprintf(msg, SIZE(msg), "Map too large! (max %d x %d)", MAP_X_LIM, MAP_Y_LIM);
        yyerror(msg);
    }

    tmppart[npart]->xsize = max_len;
    tmppart[npart]->ysize = max_hig;
    tmppart[npart]->map = malloc(max_hig * sizeof (char *));
    for (i = 0; i < max_hig; i++)
        tmppart[npart]->map[i] = tmpmap[i];
}

/*
 * If we have drawn a map without walls, this allows us to
 * auto-magically wallify it.
 */
#define Map_point(x,y) *(tmppart[npart]->map[y] + x)

void
wallify_map(void)
{
    unsigned int x, y, xx, yy, lo_xx, lo_yy, hi_xx, hi_yy;

    for (y = 0; y <= max_y_map; y++) {
        lo_yy = (y > 0) ? y - 1 : 0;
        hi_yy = (y < max_y_map) ? y + 1 : max_y_map;
        for (x = 0; x <= max_x_map; x++) {
            if (Map_point(x, y) != STONE)
                continue;
            lo_xx = (x > 0) ? x - 1 : 0;
            hi_xx = (x < max_x_map) ? x + 1 : max_x_map;
            for (yy = lo_yy; yy <= hi_yy; yy++)
                for (xx = lo_xx; xx <= hi_xx; xx++)
                    if (IS_ROOM(Map_point(xx, yy)) ||
                        Map_point(xx, yy) == CROSSWALL) {
                        Map_point(x, y) = (yy != y) ? HWALL : VWALL;
                        yy = hi_yy;     /* end `yy' loop */
                        break;  /* end `xx' loop */
                    }
        }
    }
}

/*
 * We need to check the subrooms apartenance to an existing room.
 */
boolean
check_subrooms(void)
{
    unsigned i, j, n_subrooms;
    boolean found, ok = TRUE;
    char *last_parent, msg[256];

    for (i = 0; i < nrooms; i++)
        if (tmproom[i]->parent) {
            found = FALSE;
            for (j = 0; j < nrooms; j++)
                if (tmproom[j]->name &&
                    !strcmp(tmproom[i]->parent, tmproom[j]->name)) {
                    found = TRUE;
                    break;
                }
            if (!found) {
                snprintf(msg, SIZE(msg), "Subroom error : parent room '%s' not found!",
                        tmproom[i]->parent);
                yyerror(msg);
                ok = FALSE;
            }
        }

    msg[0] = '\0';
    last_parent = msg;
    for (i = 0; i < nrooms; i++)
        if (tmproom[i]->parent) {
            n_subrooms = 0;
            for (j = i; j < nrooms; j++) {
/*
 * This is by no means perfect, but should cut down the duplicate error
 * messages by over 90%.  The only problem will be when either subrooms
 * are mixed in the level definition (not likely but possible) or rooms
 * have subrooms that have subrooms.
 */
                if (!strcmp(tmproom[i]->parent, last_parent))
                    continue;
                if (tmproom[j]->parent &&
                    !strcmp(tmproom[i]->parent, tmproom[j]->parent)) {
                    n_subrooms++;
                    if (n_subrooms > MAX_SUBROOMS) {

                        snprintf(msg, SIZE(msg), "Subroom error: too many subrooms "
                                "attached to parent room '%s'!",
                                tmproom[i]->parent);
                        yyerror(msg);
                        last_parent = tmproom[i]->parent;
                        ok = FALSE;
                        break;
                    }
                }
            }
        }
    return ok;
}

/*
 * Check that coordinates (x,y) are roomlike locations.
 * Print warning "str" if they aren't.
 */
void
check_coord(int x, int y, const char *str)
{
    char ebuf[60];

    if (x >= 0 && y >= 0 && x <= (int)max_x_map && y <= (int)max_y_map &&
        (IS_ROCK(tmpmap[y][x]) || IS_DOOR(tmpmap[y][x]))) {
        snprintf(ebuf, SIZE(ebuf), "%s placed in wall at (%02d,%02d)?!", str, x, y);
        yywarning(ebuf);
    }
}

/*
 * Here we want to store the maze part we just got.
 */
void
store_part(void)
{
    unsigned i;

    /* Ok, We got the whole part, now we store it. */

    /* The Regions */

    if ((tmppart[npart]->nreg = nreg) != 0) {
        tmppart[npart]->regions = NewTab(region, nreg);
        for (i = 0; i < nreg; i++)
            tmppart[npart]->regions[i] = tmpreg[i];
    }
    nreg = 0;

    /* The Level Regions */

    if ((tmppart[npart]->nlreg = nlreg) != 0) {
        tmppart[npart]->lregions = NewTab(lev_region, nlreg);
        for (i = 0; i < nlreg; i++)
            tmppart[npart]->lregions[i] = tmplreg[i];
    }
    nlreg = 0;

    /* the doors */

    if ((tmppart[npart]->ndoor = ndoor) != 0) {
        tmppart[npart]->doors = NewTab(door, ndoor);
        for (i = 0; i < ndoor; i++)
            tmppart[npart]->doors[i] = tmpdoor[i];
    }
    ndoor = 0;

    /* the drawbridges */

    if ((tmppart[npart]->ndrawbridge = ndb) != 0) {
        tmppart[npart]->drawbridges = NewTab(drawbridge, ndb);
        for (i = 0; i < ndb; i++)
            tmppart[npart]->drawbridges[i] = tmpdb[i];
    }
    ndb = 0;

    /* The walkmaze directives */

    if ((tmppart[npart]->nwalk = nwalk) != 0) {
        tmppart[npart]->walks = NewTab(walk, nwalk);
        for (i = 0; i < nwalk; i++)
            tmppart[npart]->walks[i] = tmpwalk[i];
    }
    nwalk = 0;

    /* The non_diggable directives */

    if ((tmppart[npart]->ndig = ndig) != 0) {
        tmppart[npart]->digs = NewTab(digpos, ndig);
        for (i = 0; i < ndig; i++)
            tmppart[npart]->digs[i] = tmpdig[i];
    }
    ndig = 0;

    /* The non_passwall directives */

    if ((tmppart[npart]->npass = npass) != 0) {
        tmppart[npart]->passs = NewTab(digpos, npass);
        for (i = 0; i < npass; i++)
            tmppart[npart]->passs[i] = tmppass[i];
    }
    npass = 0;

    /* The ladders */

    if ((tmppart[npart]->nlad = nlad) != 0) {
        tmppart[npart]->lads = NewTab(lad, nlad);
        for (i = 0; i < nlad; i++)
            tmppart[npart]->lads[i] = tmplad[i];
    }
    nlad = 0;

    /* The stairs */

    if ((tmppart[npart]->nstair = nstair) != 0) {
        tmppart[npart]->stairs = NewTab(stair, nstair);
        for (i = 0; i < nstair; i++)
            tmppart[npart]->stairs[i] = tmpstair[i];
    }
    nstair = 0;

    /* The altars */
    if ((tmppart[npart]->naltar = naltar) != 0) {
        tmppart[npart]->altars = NewTab(altar, naltar);
        for (i = 0; i < naltar; i++)
            tmppart[npart]->altars[i] = tmpaltar[i];
    }
    naltar = 0;

    /* The fountains */

    if ((tmppart[npart]->nfountain = nfountain) != 0) {
        tmppart[npart]->fountains = NewTab(fountain, nfountain);
        for (i = 0; i < nfountain; i++)
            tmppart[npart]->fountains[i] = tmpfountain[i];
    }
    nfountain = 0;

    /* the traps */

    if ((tmppart[npart]->ntrap = ntrap) != 0) {
        tmppart[npart]->traps = NewTab(trap, ntrap);
        for (i = 0; i < ntrap; i++)
            tmppart[npart]->traps[i] = tmptrap[i];
    }
    ntrap = 0;

    /* the monsters */

    if ((tmppart[npart]->nmonster = nmons) != 0) {
        tmppart[npart]->monsters = NewTab(monster, nmons);
        for (i = 0; i < nmons; i++)
            tmppart[npart]->monsters[i] = tmpmonst[i];
    } else
        tmppart[npart]->monsters = 0;
    nmons = 0;

    /* the objects */

    if ((tmppart[npart]->nobject = nobj) != 0) {
        tmppart[npart]->objects = NewTab(object, nobj);
        for (i = 0; i < nobj; i++)
            tmppart[npart]->objects[i] = tmpobj[i];
    } else
        tmppart[npart]->objects = 0;
    nobj = 0;

    /* The gold piles */

    if ((tmppart[npart]->ngold = ngold) != 0) {
        tmppart[npart]->golds = NewTab(gold, ngold);
        for (i = 0; i < ngold; i++)
            tmppart[npart]->golds[i] = tmpgold[i];
    }
    ngold = 0;

    /* The engravings */

    if ((tmppart[npart]->nengraving = nengraving) != 0) {
        tmppart[npart]->engravings = NewTab(engraving, nengraving);
        for (i = 0; i < nengraving; i++)
            tmppart[npart]->engravings[i] = tmpengraving[i];
    } else
        tmppart[npart]->engravings = 0;
    nengraving = 0;

    npart++;
    n_plist = n_mlist = n_olist = 0;
}

/*
 * Here we want to store the room part we just got.
 */
void
store_room(void)
{
    unsigned i;

    /* Ok, We got the whole room, now we store it. */

    /* the doors */

    if ((tmproom[nrooms]->ndoor = ndoor) != 0) {
        tmproom[nrooms]->doors = NewTab(room_door, ndoor);
        for (i = 0; i < ndoor; i++)
            tmproom[nrooms]->doors[i] = tmprdoor[i];
    }
    ndoor = 0;

    /* The stairs */

    if ((tmproom[nrooms]->nstair = nstair) != 0) {
        tmproom[nrooms]->stairs = NewTab(stair, nstair);
        for (i = 0; i < nstair; i++)
            tmproom[nrooms]->stairs[i] = tmpstair[i];
    }
    nstair = 0;

    /* The altars */
    if ((tmproom[nrooms]->naltar = naltar) != 0) {
        tmproom[nrooms]->altars = NewTab(altar, naltar);
        for (i = 0; i < naltar; i++)
            tmproom[nrooms]->altars[i] = tmpaltar[i];
    }
    naltar = 0;

    /* The fountains */

    if ((tmproom[nrooms]->nfountain = nfountain) != 0) {
        tmproom[nrooms]->fountains = NewTab(fountain, nfountain);
        for (i = 0; i < nfountain; i++)
            tmproom[nrooms]->fountains[i] = tmpfountain[i];
    }
    nfountain = 0;

    /* The sinks */

    if ((tmproom[nrooms]->nsink = nsink) != 0) {
        tmproom[nrooms]->sinks = NewTab(sink, nsink);
        for (i = 0; i < nsink; i++)
            tmproom[nrooms]->sinks[i] = tmpsink[i];
    }
    nsink = 0;

    /* The pools */

    if ((tmproom[nrooms]->npool = npool) != 0) {
        tmproom[nrooms]->pools = NewTab(pool, npool);
        for (i = 0; i < npool; i++)
            tmproom[nrooms]->pools[i] = tmppool[i];
    }
    npool = 0;

    /* the traps */

    if ((tmproom[nrooms]->ntrap = ntrap) != 0) {
        tmproom[nrooms]->traps = NewTab(trap, ntrap);
        for (i = 0; i < ntrap; i++)
            tmproom[nrooms]->traps[i] = tmptrap[i];
    }
    ntrap = 0;

    /* the monsters */

    if ((tmproom[nrooms]->nmonster = nmons) != 0) {
        tmproom[nrooms]->monsters = NewTab(monster, nmons);
        for (i = 0; i < nmons; i++)
            tmproom[nrooms]->monsters[i] = tmpmonst[i];
    } else
        tmproom[nrooms]->monsters = 0;
    nmons = 0;

    /* the objects */

    if ((tmproom[nrooms]->nobject = nobj) != 0) {
        tmproom[nrooms]->objects = NewTab(object, nobj);
        for (i = 0; i < nobj; i++)
            tmproom[nrooms]->objects[i] = tmpobj[i];
    } else
        tmproom[nrooms]->objects = 0;
    nobj = 0;

    /* The gold piles */

    if ((tmproom[nrooms]->ngold = ngold) != 0) {
        tmproom[nrooms]->golds = NewTab(gold, ngold);
        for (i = 0; i < ngold; i++)
            tmproom[nrooms]->golds[i] = tmpgold[i];
    }
    ngold = 0;

    /* The engravings */

    if ((tmproom[nrooms]->nengraving = nengraving) != 0) {
        tmproom[nrooms]->engravings = NewTab(engraving, nengraving);
        for (i = 0; i < nengraving; i++)
            tmproom[nrooms]->engravings[i] = tmpengraving[i];
    } else
        tmproom[nrooms]->engravings = 0;
    nengraving = 0;

    nrooms++;
}

/*
 * Output some info common to all special levels.
 */
static boolean
write_common_data(int fd, int typ, lev_init * init, long flgs)
{
    char c;
    uchar len;

    static struct version_info version_data = {
        VERSION_NUMBER, VERSION_FEATURES,
        VERSION_SANITY1
    };

    Write(fd, &version_data, sizeof version_data);
    c = typ;
    Write(fd, &c, sizeof (c));  /* 1 byte header */
    Write(fd, init, sizeof (lev_init));
    Write(fd, &flgs, sizeof flgs);

    len = (uchar) strlen(tmpmessage);
    Write(fd, &len, sizeof len);
    if (len)
        Write(fd, tmpmessage, (int)len);
    tmpmessage[0] = '\0';
    len = (uchar) strlen(tmphallumsg);
    Write(fd, &len, sizeof len);
    if (len)
        Write(fd, tmphallumsg, (int)len);
    tmphallumsg[0] = '\0';
    return TRUE;
}

/*
 * Output monster info, which needs string fixups, then release memory.
 */
static boolean
write_monsters(int fd, char *nmonster_p, monster *** monsters_p)
{
    monster *m;
    char *name, *appr;
    int j, n = (int)*nmonster_p;

    Write(fd, nmonster_p, sizeof *nmonster_p);
    for (j = 0; j < n; j++) {
        m = (*monsters_p)[j];
        name = m->name.str;
        appr = m->appear_as.str;
        m->name.str = m->appear_as.str = 0;
        m->name.len = name ? strlen(name) : 0;
        m->appear_as.len = appr ? strlen(appr) : 0;
        Write(fd, m, sizeof *m);
        if (name) {
            Write(fd, name, m->name.len);
            Free(name);
        }
        if (appr) {
            Write(fd, appr, m->appear_as.len);
            Free(appr);
        }
        Free(m);
    }
    if (*monsters_p) {
        Free(*monsters_p);
        *monsters_p = 0;
    }
    *nmonster_p = 0;
    return TRUE;
}

/*
 * Output object info, which needs string fixup, then release memory.
 */
static boolean
write_objects(int fd, char *nobject_p, object *** objects_p)
{
    object *o;
    char *name;
    int j, n = (int)*nobject_p;

    Write(fd, nobject_p, sizeof *nobject_p);
    for (j = 0; j < n; j++) {
        o = (*objects_p)[j];
        name = o->name.str;
        o->name.str = 0;        /* reset in case `len' is narrower */
        o->name.len = name ? strlen(name) : 0;
        Write(fd, o, sizeof *o);
        if (name) {
            Write(fd, name, o->name.len);
            Free(name);
        }
        Free(o);
    }
    if (*objects_p) {
        Free(*objects_p);
        *objects_p = 0;
    }
    *nobject_p = 0;
    return TRUE;
}

/*
 * Output engraving info, which needs string fixup, then release memory.
 */
static boolean
write_engravings(int fd, char *nengraving_p, engraving *** engravings_p)
{
    engraving *e;
    char *engr;
    int j, n = (int)*nengraving_p;

    Write(fd, nengraving_p, sizeof *nengraving_p);
    for (j = 0; j < n; j++) {
        e = (*engravings_p)[j];
        engr = e->engr.str;
        e->engr.str = 0;        /* reset in case `len' is narrower */
        e->engr.len = strlen(engr);
        Write(fd, e, sizeof *e);
        Write(fd, engr, e->engr.len);
        Free(engr);
        Free(e);
    }
    if (*engravings_p) {
        Free(*engravings_p);
        *engravings_p = 0;
    }
    *nengraving_p = 0;
    return TRUE;
}

/*
 * Open and write maze or rooms file, based on which pointer is non-null.
 * Return TRUE on success, FALSE on failure.
 */
boolean
write_level_file(char *filename, splev * room_level, specialmaze * maze_level)
{
    int fout;
    char lbuf[1024];

    lbuf[0] = '\0';
    strcat(lbuf, outprefix);
    if ((lbuf + strlen(lbuf))[-1] == ' ')
        (lbuf + strlen(lbuf))[-1] = '\0';
    strcat(lbuf, filename);
    strcat(lbuf, LEV_EXT);

    fout = open(lbuf, O_WRONLY | O_CREAT | O_BINARY, OMASK);
    if (fout < 0)
        return FALSE;

    if (room_level) {
        if (!write_rooms(fout, room_level))
            return FALSE;
    } else if (maze_level) {
        if (!write_maze(fout, maze_level))
            return FALSE;
    } else {
        fputs("Error: cannot write level file", stderr);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    close(fout);
    fprintf(stdout, "Wrote '%s'.\n", lbuf);
    return TRUE;
}

/*
 * Here we write the structure of the maze in the specified file (fd).
 * Also, we have to free the memory allocated via alloc().
 */
static boolean
write_maze(int fd, specialmaze * maze)
{
    short i, j;
    mazepart *pt;

    if (!write_common_data(fd, SP_LEV_MAZE, &(maze->init_lev), maze->flags))
        return FALSE;

    Write(fd, &(maze->filling), sizeof (maze->filling));
    Write(fd, &(maze->numpart), sizeof (maze->numpart));
    /* Number of parts */
    for (i = 0; i < maze->numpart; i++) {
        pt = maze->parts[i];

        /* First, write the map */

        Write(fd, &(pt->halign), sizeof (pt->halign));
        Write(fd, &(pt->valign), sizeof (pt->valign));
        Write(fd, &(pt->xsize), sizeof (pt->xsize));
        Write(fd, &(pt->ysize), sizeof (pt->ysize));
        for (j = 0; j < pt->ysize; j++) {
            if (!maze->init_lev.init_present || pt->xsize > 1 ||
                pt->ysize > 1) {
#if !defined(_MSC_VER)
                Write(fd, pt->map[j], pt->xsize * sizeof *pt->map[j]);
#else
                /* 
                 * On MSVC and Borland C compilers the Write macro above caused:
                 * warning '!=' : signed/unsigned mismatch
                 */
                unsigned reslt, sz = pt->xsize * sizeof *pt->map[j];

                reslt = write(fd, (pt->map[j]), sz);
                if (reslt != sz)
                    return FALSE;
#endif
            }
            Free(pt->map[j]);
        }
        Free(pt->map);

        /* level region stuff */
        Write(fd, &pt->nlreg, sizeof pt->nlreg);
        for (j = 0; j < pt->nlreg; j++) {
            lev_region *l = pt->lregions[j];
            char *rname = l->rname.str;

            l->rname.str = 0;   /* reset in case `len' is narrower */
            l->rname.len = rname ? strlen(rname) : 0;
            Write(fd, l, sizeof *l);
            if (rname) {
                Write(fd, rname, l->rname.len);
                Free(rname);
            }
            Free(l);
        }
        if (pt->nlreg > 0)
            Free(pt->lregions);

        /* The random registers */
        Write(fd, &(pt->nrobjects), sizeof (pt->nrobjects));
        if (pt->nrobjects) {
            Write(fd, pt->robjects, pt->nrobjects);
            Free(pt->robjects);
        }
        Write(fd, &(pt->nloc), sizeof (pt->nloc));
        if (pt->nloc) {
            Write(fd, pt->rloc_x, pt->nloc);
            Write(fd, pt->rloc_y, pt->nloc);
            Free(pt->rloc_x);
            Free(pt->rloc_y);
        }
        Write(fd, &(pt->nrmonst), sizeof (pt->nrmonst));
        if (pt->nrmonst) {
            Write(fd, pt->rmonst, pt->nrmonst);
            Free(pt->rmonst);
        }

        /* subrooms */
        Write(fd, &(pt->nreg), sizeof (pt->nreg));
        for (j = 0; j < pt->nreg; j++) {
            Write(fd, pt->regions[j], sizeof (region));
            Free(pt->regions[j]);
        }
        if (pt->nreg > 0)
            Free(pt->regions);

        /* the doors */
        Write(fd, &(pt->ndoor), sizeof (pt->ndoor));
        for (j = 0; j < pt->ndoor; j++) {
            Write(fd, pt->doors[j], sizeof (door));
            Free(pt->doors[j]);
        }
        if (pt->ndoor > 0)
            Free(pt->doors);

        /* The drawbridges */
        Write(fd, &(pt->ndrawbridge), sizeof (pt->ndrawbridge));
        for (j = 0; j < pt->ndrawbridge; j++) {
            Write(fd, pt->drawbridges[j], sizeof (drawbridge));
            Free(pt->drawbridges[j]);
        }
        if (pt->ndrawbridge > 0)
            Free(pt->drawbridges);

        /* The mazewalk directives */
        Write(fd, &(pt->nwalk), sizeof (pt->nwalk));
        for (j = 0; j < pt->nwalk; j++) {
            Write(fd, pt->walks[j], sizeof (walk));
            Free(pt->walks[j]);
        }
        if (pt->nwalk > 0)
            Free(pt->walks);

        /* The non_diggable directives */
        Write(fd, &(pt->ndig), sizeof (pt->ndig));
        for (j = 0; j < pt->ndig; j++) {
            Write(fd, pt->digs[j], sizeof (digpos));
            Free(pt->digs[j]);
        }
        if (pt->ndig > 0)
            Free(pt->digs);

        /* The non_passwall directives */
        Write(fd, &(pt->npass), sizeof (pt->npass));
        for (j = 0; j < pt->npass; j++) {
            Write(fd, pt->passs[j], sizeof (digpos));
            Free(pt->passs[j]);
        }
        if (pt->npass > 0)
            Free(pt->passs);

        /* The ladders */
        Write(fd, &(pt->nlad), sizeof (pt->nlad));
        for (j = 0; j < pt->nlad; j++) {
            Write(fd, pt->lads[j], sizeof (lad));
            Free(pt->lads[j]);
        }
        if (pt->nlad > 0)
            Free(pt->lads);

        /* The stairs */
        Write(fd, &(pt->nstair), sizeof (pt->nstair));
        for (j = 0; j < pt->nstair; j++) {
            Write(fd, pt->stairs[j], sizeof (stair));
            Free(pt->stairs[j]);
        }
        if (pt->nstair > 0)
            Free(pt->stairs);

        /* The altars */
        Write(fd, &(pt->naltar), sizeof (pt->naltar));
        for (j = 0; j < pt->naltar; j++) {
            Write(fd, pt->altars[j], sizeof (altar));
            Free(pt->altars[j]);
        }
        if (pt->naltar > 0)
            Free(pt->altars);

        /* The fountains */
        Write(fd, &(pt->nfountain), sizeof (pt->nfountain));
        for (j = 0; j < pt->nfountain; j++) {
            Write(fd, pt->fountains[j], sizeof (fountain));
            Free(pt->fountains[j]);
        }
        if (pt->nfountain > 0)
            Free(pt->fountains);

        /* The traps */
        Write(fd, &(pt->ntrap), sizeof (pt->ntrap));
        for (j = 0; j < pt->ntrap; j++) {
            Write(fd, pt->traps[j], sizeof (trap));
            Free(pt->traps[j]);
        }
        if (pt->ntrap)
            Free(pt->traps);

        /* The monsters */
        if (!write_monsters(fd, &pt->nmonster, &pt->monsters))
            return FALSE;

        /* The objects */
        if (!write_objects(fd, &pt->nobject, &pt->objects))
            return FALSE;

        /* The gold piles */
        Write(fd, &(pt->ngold), sizeof (pt->ngold));
        for (j = 0; j < pt->ngold; j++) {
            Write(fd, pt->golds[j], sizeof (gold));
            Free(pt->golds[j]);
        }
        if (pt->ngold > 0)
            Free(pt->golds);

        /* The engravings */
        if (!write_engravings(fd, &pt->nengraving, &pt->engravings))
            return FALSE;

        Free(pt);
    }

    Free(maze->parts);
    maze->parts = NULL;
    maze->numpart = 0;
    return TRUE;
}

/*
 * Here we write the structure of the room level in the specified file (fd).
 */
static boolean
write_rooms(int fd, splev * lev)
{
    short i, j, size;
    room *pt;

    if (!write_common_data(fd, SP_LEV_ROOMS, &(lev->init_lev), lev->flags))
        return FALSE;

    /* Random registers */

    Write(fd, &lev->nrobjects, sizeof (lev->nrobjects));
    if (lev->nrobjects)
        Write(fd, lev->robjects, lev->nrobjects);
    Write(fd, &lev->nrmonst, sizeof (lev->nrmonst));
    if (lev->nrmonst)
        Write(fd, lev->rmonst, lev->nrmonst);

    Write(fd, &(lev->nroom), sizeof (lev->nroom));
    /* Number of rooms */
    for (i = 0; i < lev->nroom; i++) {
        pt = lev->rooms[i];

        /* Room characteristics */

        size = (short)(pt->name ? strlen(pt->name) : 0);
        Write(fd, &size, sizeof (size));
        if (size)
            Write(fd, pt->name, size);

        size = (short)(pt->parent ? strlen(pt->parent) : 0);
        Write(fd, &size, sizeof (size));
        if (size)
            Write(fd, pt->parent, size);

        Write(fd, &(pt->x), sizeof (pt->x));
        Write(fd, &(pt->y), sizeof (pt->y));
        Write(fd, &(pt->w), sizeof (pt->w));
        Write(fd, &(pt->h), sizeof (pt->h));
        Write(fd, &(pt->xalign), sizeof (pt->xalign));
        Write(fd, &(pt->yalign), sizeof (pt->yalign));
        Write(fd, &(pt->rtype), sizeof (pt->rtype));
        Write(fd, &(pt->chance), sizeof (pt->chance));
        Write(fd, &(pt->rlit), sizeof (pt->rlit));
        Write(fd, &(pt->filled), sizeof (pt->filled));

        /* the doors */
        Write(fd, &(pt->ndoor), sizeof (pt->ndoor));
        for (j = 0; j < pt->ndoor; j++)
            Write(fd, pt->doors[j], sizeof (room_door));

        /* The stairs */
        Write(fd, &(pt->nstair), sizeof (pt->nstair));
        for (j = 0; j < pt->nstair; j++)
            Write(fd, pt->stairs[j], sizeof (stair));

        /* The altars */
        Write(fd, &(pt->naltar), sizeof (pt->naltar));
        for (j = 0; j < pt->naltar; j++)
            Write(fd, pt->altars[j], sizeof (altar));

        /* The fountains */
        Write(fd, &(pt->nfountain), sizeof (pt->nfountain));
        for (j = 0; j < pt->nfountain; j++)
            Write(fd, pt->fountains[j], sizeof (fountain));

        /* The sinks */
        Write(fd, &(pt->nsink), sizeof (pt->nsink));
        for (j = 0; j < pt->nsink; j++)
            Write(fd, pt->sinks[j], sizeof (sink));

        /* The pools */
        Write(fd, &(pt->npool), sizeof (pt->npool));
        for (j = 0; j < pt->npool; j++)
            Write(fd, pt->pools[j], sizeof (pool));

        /* The traps */
        Write(fd, &(pt->ntrap), sizeof (pt->ntrap));
        for (j = 0; j < pt->ntrap; j++)
            Write(fd, pt->traps[j], sizeof (trap));

        /* The monsters */
        if (!write_monsters(fd, &pt->nmonster, &pt->monsters))
            return FALSE;

        /* The objects */
        if (!write_objects(fd, &pt->nobject, &pt->objects))
            return FALSE;

        /* The gold piles */
        Write(fd, &(pt->ngold), sizeof (pt->ngold));
        for (j = 0; j < pt->ngold; j++)
            Write(fd, pt->golds[j], sizeof (gold));

        /* The engravings */
        if (!write_engravings(fd, &pt->nengraving, &pt->engravings))
            return FALSE;

    }

    /* The corridors */
    Write(fd, &lev->ncorr, sizeof (lev->ncorr));
    for (i = 0; i < lev->ncorr; i++)
        Write(fd, lev->corrs[i], sizeof (corridor));
    return TRUE;
}

/*
 * Release memory allocated to a rooms-style special level; maze-style
 * levels have the fields freed as they're written; monsters, objects, and
 * engravings are freed as written for both styles, so not handled here.
 */
void
free_rooms(splev * lev)
{
    room *r;
    int j, n = lev->nroom;

    while (n--) {
        r = lev->rooms[n];
        Free(r->name);
        Free(r->parent);
        if ((j = r->ndoor) != 0) {
            while (j--)
                Free(r->doors[j]);
            Free(r->doors);
        }
        if ((j = r->nstair) != 0) {
            while (j--)
                Free(r->stairs[j]);
            Free(r->stairs);
        }
        if ((j = r->naltar) != 0) {
            while (j--)
                Free(r->altars[j]);
            Free(r->altars);
        }
        if ((j = r->nfountain) != 0) {
            while (j--)
                Free(r->fountains[j]);
            Free(r->fountains);
        }
        if ((j = r->nsink) != 0) {
            while (j--)
                Free(r->sinks[j]);
            Free(r->sinks);
        }
        if ((j = r->npool) != 0) {
            while (j--)
                Free(r->pools[j]);
            Free(r->pools);
        }
        if ((j = r->ntrap) != 0) {
            while (j--)
                Free(r->traps[j]);
            Free(r->traps);
        }
        if ((j = r->ngold) != 0) {
            while (j--)
                Free(r->golds[j]);
            Free(r->golds);
        }
        Free(r);
        lev->rooms[n] = NULL;
    }
    Free(lev->rooms);
    lev->rooms = NULL;
    lev->nroom = 0;

    for (j = 0; j < lev->ncorr; j++) {
        Free(lev->corrs[j]);
        lev->corrs[j] = NULL;
    }
    Free(lev->corrs);
    lev->corrs = NULL;
    lev->ncorr = 0;

    Free(lev->robjects);
    lev->robjects = NULL;
    lev->nrobjects = 0;
    Free(lev->rmonst);
    lev->rmonst = NULL;
    lev->nrmonst = 0;
}

#ifdef STRICT_REF_DEF
/*
 * Any globals declared in hack.h and descendents which aren't defined
 * in the modules linked into lev_comp should be defined here.  These
 * definitions can be dummies:  their sizes shouldn't matter as long as
 * as their types are correct; actual values are irrelevant.
 */
# define ARBITRARY_SIZE 1
/* attrib.c */
struct attribs attrmax, attrmin;

/* files.c */
char lock[ARBITRARY_SIZE];
char SAVEF[ARBITRARY_SIZE];

/* termcap.c */
struct tc_lcl_data tc_lcl_data;
char *hilites[CLR_MAX];

/* trap.c */
const char *traps[TRAPNUM];

/* xxxtty.c */
# ifdef DEFINE_OSPEED
short ospeed;
# endif
#endif /* STRICT_REF_DEF */

/*lev_main.c*/

