/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
/* NetHack may be freely redistributed.  See license for details. */

/*
 *	This source file is compiled twice:
 *	once without TILETEXT defined to make tilemap.{o,obj},
 *	then again with it defined to produce tiletxt.{o,obj}.
 */

#include <stdlib.h>
#include <stdio.h>
#include "tilesequence.h"

const char *tilename(int, int);
void init_tilemap(void);
void process_substitutions(FILE *);

#define MON_GLYPH 1
#define OBJ_GLYPH 2
#define OTH_GLYPH 3     /* fortunately unnecessary */
#define COM_GLYPH 4     /* combined glyphs (for importing tile sets) */

/*
 * Some entries in glyph2tile[] should be substituted for on various levels.
 * The tiles used for the substitute entries will follow the usual ones in
 * other.til in the order given here, which should have every substitution
 * for the same set of tiles grouped together.  You will have to change
 * more code in process_substitutions()/substitute_tiles() if the sets
 * overlap in the future.
 */
struct substitute {
    int first_glyph, last_glyph;
    const char *sub_name;       /* for explanations */
    const char *level_test;
} substitutes[] = {
    { GLYPH_CMAP_OFF + S_vwall, GLYPH_CMAP_OFF + S_trwall, "mine walls",
      "In_mines(plev)"},
    { GLYPH_CMAP_OFF + S_vwall, GLYPH_CMAP_OFF + S_trwall, "gehennom walls",
      "In_hell(plev)"},
    { GLYPH_CMAP_OFF + S_vwall, GLYPH_CMAP_OFF + S_trwall, "knox walls",
      "Is_knox(plev)"},
    { GLYPH_CMAP_OFF + S_vwall, GLYPH_CMAP_OFF + S_trwall, "sokoban walls",
    "In_sokoban(plev)"}
};


#ifdef TILETEXT

/*
 * ALI
 *
 * The missing cmap names. These allow us to intelligently interpret
 * tilesets from other variants of NetHack (eg., Mitsuhiro Itakura's 32x32
 * tileset which is based on JNetHack).
 */

struct {
    int cmap;
    const char *name;
} cmaps[] = {
    { S_sw_tl, "swallow top left", },
    { S_sw_tc, "swallow top center", },
    { S_sw_tr, "swallow top right", },
    { S_sw_ml, "swallow middle left", },
    { S_sw_mr, "swallow middle right", },
    { S_sw_bl, "swallow bottom left", },
    { S_sw_bc, "swallow bottom center", },
    { S_sw_br, "swallow bottom right" },
};

/*
 * entry is the position of the tile within the monsters/objects/other set
 */
const char *
tilename(int set, int entry)
{
    int i, j, tilenum;
    int in_set, oth_origin;
    static char buf[BUFSZ];

    tilenum = 0;

    in_set = set == MON_GLYPH || set == COM_GLYPH;
    for (i = 0; i < NUMMONS; i++) {
        if (in_set && tilenum == entry)
            return mons[i].mname;
        tilenum++;
    }
    if (in_set && tilenum == entry)
        return "invisible monster";
    tilenum++;

    if (set != COM_GLYPH)
        tilenum = 0;    /* set-relative number */
    in_set = set == OBJ_GLYPH || set == COM_GLYPH;
    for (i = 0; i < NUM_OBJECTS; i++) {
        /* prefer to give the description - that's all the tile's appearance
           should reveal */
        if (in_set && tilenum == entry) {
            if (!obj_descr[i].oc_descr)
                return obj_descr[i].oc_name;
            if (!obj_descr[i].oc_name)
                return obj_descr[i].oc_descr;

            sprintf(buf, "%s / %s", obj_descr[i].oc_descr,
                    obj_descr[i].oc_name);
            return buf;
        }

        tilenum++;
    }

    if (set != COM_GLYPH)
        tilenum = 0;    /* set-relative number */
    in_set = set == OTH_GLYPH || set == COM_GLYPH;
    oth_origin = tilenum;
    for (i = 0; i < (MAXPCHARS - MAXEXPCHARS); i++) {
        if (in_set && tilenum == entry) {
            if (*defsyms[i].symname)
                return defsyms[i].symname;
            else {
                /* if SINKS are turned off, this string won't be there (and
                   can't be there to prevent symbol-identification and
                   special-level mimic appearances from thinking the items
                   exist) */
                switch (i) {
                case S_sink:
                    sprintf(buf, "sink");
                    break;
                default:
                    for (j = 0; j < SIZE(cmaps); j++)
                        if (cmaps[j].cmap == tilenum - oth_origin) {
                            sprintf(buf, "cmap / %s", cmaps[j].name);
                            break;
                        }
                    if (j == SIZE(cmaps))
                        sprintf(buf, "cmap %d", tilenum - oth_origin);
                    break;
                }
                return buf;
            }
        }
        tilenum++;
    }
    /* explosions */
    tilenum = MAXPCHARS - MAXEXPCHARS;
    i = entry - tilenum;
    if (i < (MAXEXPCHARS * EXPL_MAX)) {
        if (set == OTH_GLYPH) {
            static char *explosion_types[] = {
                "dark", "noxious", "muddy", "wet",
                "magical", "fiery", "frosty"
            };
            sprintf(buf, "explosion %s %d", explosion_types[i / MAXEXPCHARS],
                    i % MAXEXPCHARS);
            return buf;
        }
    }
    tilenum += (MAXEXPCHARS * EXPL_MAX);

    i = entry - tilenum;
    if (i < (NUM_ZAP << 2)) {
        if (in_set) {
            sprintf(buf, "zap %d %d", i / 4, i % 4);
            return buf;
        }
    }
    tilenum += (NUM_ZAP << 2);

    i = entry - tilenum;
    if (i < WARNCOUNT) {
        if (set == OTH_GLYPH) {
            sprintf(buf, "warning %d", i);
            return buf;
        }
    }
    tilenum += WARNCOUNT;

    for (i = 0; i < SIZE(substitutes); i++) {
        j = entry - tilenum;
        if (j <= substitutes[i].last_glyph - substitutes[i].first_glyph) {
            if (in_set) {
                sprintf(buf, "sub %s %d", substitutes[i].sub_name, j);
                return buf;
            }
        }
        tilenum += substitutes[i].last_glyph - substitutes[i].first_glyph + 1;
    }

    sprintf(buf, "unknown %d %d", set, entry);
    return buf;
}

#else /* TILETEXT */

short tilemap[MAX_GLYPH];
int lastmontile, lastobjtile, lastothtile;

static char in_line[256];

/* Number of tiles for invisible monsters */
# define NUM_INVIS_TILES 1

/*
 * ALI
 *
 * Compute the value of ceil(sqrt(c)) using only integer arithmetic.
 *
 * Newton-Raphson gives us the following algorithm for solving sqrt(c):
 *
 *            a[n]^2+c
 * a[n+1]  =  --------
 *             2*a[n]
 *
 * It would be tempting to use a[n+1] = (a[n]^2+c+2*a[n]-1) div 2*a[n]
 * to solve for ceil(sqrt(c)) but this does not converge correctly.
 * Instead we solve floor(sqrt(c)) first and then adjust as necessary.
 *
 * The proposed algorithm to solve floor(sqrt(c)):
 *
 * a[n+1] = a[n]^2+c div 2*a[n]
 *
 * If we define the deviation of approximation n as follows:
 *
 * e[n] = a[n] - sqrt(c)
 *
 * Then it follows that:
 *
 *              e[n]^2
 * e[n+1] = ---------------
 *          2(e[n]+sqrt(c))
 *
 * The sequence will converge to the solution if:
 *
 * | e[n+1] | < | e[n] |
 *
 * which becomes:
 *
 *                      |     e[n]^2      |
 *                      | --------------- | < | e[n] |
 *                      | 2(e[n]+sqrt(c)) |
 *
 * This splits into three cases:
 *
 * If e[n] > 0          * If 0 > e[n] >= -sqrt(c) * If e[n] < -sqrt(c)
 *                      *                         *
 * Converges iff:       * Converges iff:          * Converges iff:
 *                      *             2           *
 *    e[n] > -2*sqrt(c) *    e[n] > - - sqrt(c)   *    e[n] > -2*sqrt(c)
 *                      *             3           *
 *                      *                 sqrt(c) *
 * True for all cases.  * True iff a[n] > ------- * True iff 0 > a[n] > -sqrt(c)
 *                      *                    3    *
 *
 * Case 3 represents failure, but this can be avoided by choosing a positive
 * initial value. In both case 1 and case 2, e[n+1] is positive regardless
 * of the sign of e[n]. It therefore follows that even if an initial value
 * between 0 and sqrt(c)/3 is chosen, we will only diverge for one iteration.
 *
 * Therefore the algorithm will converge correctly as long as we start
 * with a positve inital value (it will converge to the negative root if
 * we start with a negative initial value and fail if we start with zero).
 *
 * We choose an initial value designed to be close to the solution we expect
 * for typical values of c. This also makes it unlikely that we will cause
 * a divergence. If we do, it will only take a few more iterations.
 */

static int
ceil_sqrt(int c)
{
    int a = c / 36, la; /* Approximation and last approximation */

    /* Compute floor(sqrt(c)) */
    do {
        la = a;
        a = (a * a + c) / (2 * a);
    } while (a != la);
    /* Adjust for ceil(sqrt(c)) */
    return a * a == c ? a : a + 1;
}

/*
 * set up array to map glyph numbers to tile numbers
 *
 * assumes tiles are numbered sequentially through monsters/objects/other,
 * with entries for all supported compilation options
 *
 * "other" contains cmap and zaps (the swallow sets are a repeated portion
 * of cmap), as well as the "flash" glyphs for the new warning system
 * introduced in 3.3.1.
 */
void
init_tilemap(void)
{
    int i, j, tilenum;
    int corpsetile, swallowbase;

    for (i = 0; i < MAX_GLYPH; i++) {
        tilemap[i] = -1;
    }

    corpsetile = NUMMONS + NUM_INVIS_TILES + CORPSE;
    swallowbase = NUMMONS + NUM_INVIS_TILES + NUM_OBJECTS + S_sw_tl;

    tilenum = 0;
    for (i = 0; i < NUMMONS; i++) {
        tilemap[GLYPH_MON_OFF + i] = tilenum;
        tilemap[GLYPH_PET_OFF + i] = tilenum;
        tilemap[GLYPH_DETECT_OFF + i] = tilenum;
        tilemap[GLYPH_RIDDEN_OFF + i] = tilenum;
        tilemap[GLYPH_BODY_OFF + i] = corpsetile;
        j = GLYPH_SWALLOW_OFF + 8 * i;
        tilemap[j] = swallowbase;
        tilemap[j + 1] = swallowbase + 1;
        tilemap[j + 2] = swallowbase + 2;
        tilemap[j + 3] = swallowbase + 3;
        tilemap[j + 4] = swallowbase + 4;
        tilemap[j + 5] = swallowbase + 5;
        tilemap[j + 6] = swallowbase + 6;
        tilemap[j + 7] = swallowbase + 7;
        tilenum++;
    }
    tilemap[GLYPH_INVIS_OFF] = tilenum++;
    lastmontile = tilenum - 1;

    for (i = 0; i < NUM_OBJECTS; i++) {
        tilemap[GLYPH_OBJ_OFF + i] = tilenum;
        tilenum++;
    }
    lastobjtile = tilenum - 1;

    for (i = 0; i < (MAXPCHARS - MAXEXPCHARS); i++) {
        tilemap[GLYPH_CMAP_OFF + i] = tilenum;
        tilenum++;
    }

    for (i = 0; i < (MAXEXPCHARS * EXPL_MAX); i++) {
        tilemap[GLYPH_EXPLODE_OFF + i] = tilenum;
        tilenum++;
    }

    for (i = 0; i < NUM_ZAP << 2; i++) {
        tilemap[GLYPH_ZAP_OFF + i] = tilenum;
        tilenum++;
    }

    for (i = 0; i < WARNCOUNT; i++) {
        tilemap[GLYPH_WARNING_OFF + i] = tilenum;
        tilenum++;
    }

    lastothtile = tilenum - 1;
}

const char *prolog[] = {
    "",
    "",
    "void",
    "substitute_tiles(plev)",
    "d_level *plev;",
    "{",
    "\tint i;",
    ""
};

const char *epilog[] = {
    "}"
};

/* write out the substitutions in an easily-used form. */
void
process_substitutions(FILE *ofp)
{
    int i, j, k, span, start;

    fprintf(ofp, "\n\n");

    j = 0;      /* unnecessary */
    span = -1;
    for (i = 0; i < SIZE(substitutes); i++) {
        if (i == 0 || substitutes[i].first_glyph != substitutes[j].first_glyph
            || substitutes[i].last_glyph != substitutes[j].last_glyph) {
            j = i;
            span++;
            fprintf(ofp, "short std_tiles%d[] = { ", span);
            for (k = substitutes[i].first_glyph; k < substitutes[i].last_glyph;
                 k++)
                fprintf(ofp, "%d, ", tilemap[k]);
            fprintf(ofp, "%d };\n", tilemap[substitutes[i].last_glyph]);
        }
    }

    for (i = 0; i < SIZE(prolog); i++) {
        fprintf(ofp, "%s\n", prolog[i]);
    }
    j = -1;
    span = -1;
    start = lastothtile + 1;
    for (i = 0; i < SIZE(substitutes); i++) {
        if (i == 0 || substitutes[i].first_glyph != substitutes[j].first_glyph
            || substitutes[i].last_glyph != substitutes[j].last_glyph) {
            if (i != 0) {       /* finish previous span */
                fprintf(ofp, "\t} else {\n");
                fprintf(ofp, "\t\tfor (i = %d; i <= %d; i++)\n",
                        substitutes[j].first_glyph, substitutes[j].last_glyph);
                fprintf(ofp, "\t\t\tglyph2tile[i] = std_tiles%d[i - %d];\n",
                        span, substitutes[j].first_glyph);
                fprintf(ofp, "\t}\n\n");
            }
            j = i;
            span++;
        }
        if (i != j)
            fprintf(ofp, "\t} else ");
        fprintf(ofp, "\tif (%s) {\n", substitutes[i].level_test);
        fprintf(ofp, "\t\tfor (i = %d; i <= %d; i++)\n",
                substitutes[i].first_glyph, substitutes[i].last_glyph);
        fprintf(ofp, "\t\t\tglyph2tile[i] = %d + i - %d;\n", start,
                substitutes[i].first_glyph);
        start += substitutes[i].last_glyph - substitutes[i].first_glyph + 1;
    }
    /* finish last span */
    fprintf(ofp, "\t} else {\n");
    fprintf(ofp, "\t\tfor (i = %d; i <= %d; i++)\n", substitutes[j].first_glyph,
            substitutes[j].last_glyph);
    fprintf(ofp, "\t\t\tglyph2tile[i] = std_tiles%d[i - %d];\n", span,
            substitutes[j].first_glyph);
    fprintf(ofp, "\t}\n\n");

    for (i = 0; i < SIZE(epilog); i++) {
        fprintf(ofp, "%s\n", epilog[i]);
    }

    fprintf(ofp, "\nint total_tiles_used = %d;\n", start);
    i = ceil_sqrt(start);
    fprintf(ofp, "int tiles_per_row = %d;\n", i);
    fprintf(ofp, "int tiles_per_col = %d;\n", (start + i - 1) / i);
    lastothtile = start - 1;
}

int
main(int argc, char** argv)
{
    register int i;
    FILE *ifp, *ofp;

    /* We take three arguments: header file template,
       source file output, header file output. */
    if (argc != 4) exit(1);

    init_tilemap();

    /* 
     * create the source file, "tile.c"
     */
    if (!(ofp = fopen(argv[2], "w"))) {
        perror(argv[2]);
        exit(EXIT_FAILURE);
    }
    fprintf(ofp, "/* This file is automatically generated.  Do not edit. */\n");
    fprintf(ofp, "\n#include \"tilesequence.h\"\n\n");
    fprintf(ofp, "short glyph2tile[MAX_GLYPH] = {\n");

    for (i = 0; i < MAX_GLYPH; i++) {
        fprintf(ofp, "%2d,%c", tilemap[i], (i % 12) ? ' ' : '\n');
    }
    fprintf(ofp, "%s};\n", (i % 12) ? "\n" : "");

    process_substitutions(ofp);

    fprintf(ofp, "\n#define MAXMONTILE %d\n", lastmontile);
    fprintf(ofp, "#define MAXOBJTILE %d\n", lastobjtile);
    fprintf(ofp, "#define MAXOTHTILE %d\n", lastothtile);

    fprintf(ofp, "\n/*tile.c*/\n");

    fclose(ofp);

    /* 
     * create the include file, "tile.h"
     */
    if (!(ifp = fopen(argv[1], "r"))) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }
    if (!(ofp = fopen(argv[3], "w"))) {
        perror(argv[3]);
        exit(EXIT_FAILURE);
    }
    fprintf(ofp, "/* This file is automatically generated.  Do not edit. */\n");

    fprintf(ofp, "\n#define TOTAL_TILES_USED %d\n", lastothtile + 1);
    i = ceil_sqrt(lastothtile + 1);
    fprintf(ofp, "#define TILES_PER_ROW %d\n", i);
    fprintf(ofp, "#define TILES_PER_COL %d\n\n", (lastothtile + i) / i);

    while (fgets(in_line, sizeof in_line, ifp) != 0)
        (void)fputs(in_line, ofp);

    fprintf(ofp, "\n/*tile.h*/\n");

    fclose(ofp);

    exit(EXIT_SUCCESS);
    /*NOTREACHED*/ return 0;
}

#endif /* TILETEXT */
