/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-04-02 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

/* NOTE: This file is utf-8 encoded; saving with a non utf-8 aware editor WILL
 * damage some symbols */

#include "nhcurses.h"
#include "tilesequence.h"
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

struct curses_drawing_info *default_drawing;

static unsigned long curcchar = '?' | (CLR_RED << 21);

int curses_level_display_mode;

char *tiletable;
int tiletable_len;
nh_bool tiletable_is_cchar = 1;

#define listlen(list) (sizeof(list)/sizeof(list[0]))

static void print_tile_number(WINDOW *, int, unsigned long long);

static struct curses_symdef *
load_nh_symarray(const struct nh_symdef *src, int len)
{
    int i;
    struct curses_symdef *copy = malloc(len * sizeof (struct curses_symdef));

    memset(copy, 0, len * sizeof (struct curses_symdef));

    for (i = 0; i < len; i++) {
        copy[i].symname = strdup(src[i].symname);
        copy[i].ch = src[i].ch;
        copy[i].color = src[i].color;

        /* this works because ASCII 0x?? (for ?? < 128) == Unicode U+00?? */
        copy[i].unichar[0] = (wchar_t) src[i].ch;
    }

    return copy;
}


static struct curses_drawing_info *
load_nh_drawing_info(const struct nh_drawing_info *orig)
{
    struct curses_drawing_info *copy =
        malloc(sizeof (struct curses_drawing_info));

    copy->num_bgelements = orig->num_bgelements;
    copy->num_traps = orig->num_traps;
    copy->num_objects = orig->num_objects;
    copy->num_monsters = orig->num_monsters;
    copy->num_warnings = orig->num_warnings;
    copy->num_expltypes = orig->num_expltypes;
    copy->num_zaptypes = orig->num_zaptypes;
    copy->num_effects = orig->num_effects;
    copy->bg_feature_offset = orig->bg_feature_offset;

    copy->bgelements = load_nh_symarray(orig->bgelements, orig->num_bgelements);
    copy->traps = load_nh_symarray(orig->traps, orig->num_traps);
    copy->objects = load_nh_symarray(orig->objects, orig->num_objects);
    copy->monsters = load_nh_symarray(orig->monsters, orig->num_monsters);
    copy->warnings = load_nh_symarray(orig->warnings, orig->num_warnings);
    copy->invis = load_nh_symarray(orig->invis, 1);
    copy->effects = load_nh_symarray(orig->effects, orig->num_effects);
    copy->expltypes = load_nh_symarray(orig->expltypes, orig->num_expltypes);
    copy->explsyms = load_nh_symarray(orig->explsyms, NUMEXPCHARS);
    copy->zaptypes = load_nh_symarray(orig->zaptypes, orig->num_zaptypes);
    copy->zapsyms = load_nh_symarray(orig->zapsyms, NUMZAPCHARS);
    copy->swallowsyms = load_nh_symarray(orig->swallowsyms, NUMSWALLOWCHARS);

    return copy;
}

void
init_displaychars(void)
{
    struct nh_drawing_info *dinfo = nh_get_drawing_info();

    default_drawing = load_nh_drawing_info(dinfo);
}


static void
free_symarray(struct curses_symdef *array, int len)
{
    int i;

    for (i = 0; i < len; i++)
        free((char *)array[i].symname);

    free(array);
}


static void
free_drawing_info(struct curses_drawing_info *di)
{
    free_symarray(di->bgelements, di->num_bgelements);
    free_symarray(di->traps, di->num_traps);
    free_symarray(di->objects, di->num_objects);
    free_symarray(di->monsters, di->num_monsters);
    free_symarray(di->warnings, di->num_warnings);
    free_symarray(di->invis, 1);
    free_symarray(di->effects, di->num_effects);
    free_symarray(di->expltypes, di->num_expltypes);
    free_symarray(di->explsyms, NUMEXPCHARS);
    free_symarray(di->zaptypes, di->num_zaptypes);
    free_symarray(di->zapsyms, NUMZAPCHARS);
    free_symarray(di->swallowsyms, NUMSWALLOWCHARS);

    free(di);
}


void
free_displaychars(void)
{
    free_drawing_info(default_drawing);
    default_drawing = NULL;
}


void
print_low_priority_brandings(WINDOW *win, struct nh_dbuf_entry *dbe)
{
    enum nhcurses_brandings branding = nhcurses_no_branding;
    if (!strcmp("vcdoor", default_drawing->bgelements[dbe->bg].symname) ||
        !strcmp("hcdoor", default_drawing->bgelements[dbe->bg].symname)) {
        if (dbe->branding & NH_BRANDING_LOCKED)
            branding = nhcurses_genbranding_locked;
        else if (dbe->branding & NH_BRANDING_UNLOCKED)
            branding = nhcurses_genbranding_unlocked;
    }
    if (!strcmp("room", default_drawing->bgelements[dbe->bg].symname) ||
        !strcmp("corr", default_drawing->bgelements[dbe->bg].symname)) {
        if (settings.floorcolor) {
            if (dbe->branding & NH_BRANDING_STEPPED)
                branding = nhcurses_genbranding_stepped;
        }
    }
    if (branding != nhcurses_no_branding) {
        print_tile_number(win, TILESEQ_GENBRAND_OFF +
                          branding - nhcurses_genbranding_first,
                          dbe_substitution(dbe));
    }
}

void
print_high_priority_brandings(WINDOW *win, struct nh_dbuf_entry *dbe)
{
    enum nhcurses_brandings branding = nhcurses_no_branding;
    unsigned long long substitution = dbe_substitution(dbe);

    if ((dbe->monflags & MON_TAME) && settings.hilite_pet)
        branding = nhcurses_monbranding_tame;
    if ((dbe->monflags & MON_PEACEFUL) && settings.hilite_pet)
        branding = nhcurses_monbranding_peaceful;
    if ((dbe->monflags & MON_DETECTED) && settings.use_inverse)
        branding = nhcurses_monbranding_detected;

    if (branding != nhcurses_no_branding) {
        print_tile_number(win, TILESEQ_MONBRAND_OFF + branding -
                          nhcurses_monbranding_first, substitution);
    }

    /* Traps get trap brandings, apart from magic portals and the vibrating
       square. */
    if ((dbe->trap &&
         strcmp("magic portal",
                default_drawing->traps[dbe->trap - 1].symname) != 0 &&
         strcmp("vibrating square",
                default_drawing->traps[dbe->trap - 1].symname) != 0) ||
        (dbe->branding & NH_BRANDING_TRAPPED))
        print_tile_number(win, TILESEQ_GENBRAND_OFF +
                          nhcurses_genbranding_trapped -
                          nhcurses_genbranding_first, substitution);
}

/* What is the bottom-most, opaque background of a map square? */
static enum {
    fb_room,
    fb_corr,
}
furthest_background(const struct nh_dbuf_entry *dbe)
{
    boolean corr =
        !strcmp("corr", default_drawing->bgelements[dbe->bg].symname);

    return corr ? fb_corr : fb_room;
}

void
curses_notify_level_changed(int dmode)
{
    mark_mapwin_for_full_refresh();
    curses_level_display_mode = dmode;
}


unsigned long long
dbe_substitution(struct nh_dbuf_entry *dbe)
{
    int ldm = settings.dungeoncolor ? curses_level_display_mode : LDM_DEFAULT;
    unsigned long long s = NHCURSES_SUB_LDM(ldm);

    /* TODO: Do we want this behaviour (that approximates 3.4.3 behaviour) for
       the "lit" substitution? Do we want it to be customizable?

       Another option is to have multiple substitutions, but that's starting to
       get silly. */
    short lit_branding =
        !strcmp("corr", default_drawing->bgelements[dbe->bg].symname) ?
        (NH_BRANDING_LIT | NH_BRANDING_TEMP_LIT) :
        (NH_BRANDING_LIT | NH_BRANDING_TEMP_LIT | NH_BRANDING_SEEN);

    s |= (dbe->branding & lit_branding) ? NHCURSES_SUB_LIT : NHCURSES_SUB_UNLIT;

    /* Corpses/statues/figurines have their own substitution (that's turned off
       on layers above "object" by map.c to avoid substituting a monster that's
       standing on a corpse or statue). */
    if (dbe->obj) {
        if (!strcmp("corpse", default_drawing->objects[dbe->obj-1].symname))
            s |= NHCURSES_SUB_CORPSE;
        if (!strcmp("statue", default_drawing->objects[dbe->obj-1].symname))
            s |= NHCURSES_SUB_STATUE;
        if (!strcmp("figurine", default_drawing->objects[dbe->obj-1].symname))
            s |= NHCURSES_SUB_FIGURINE;
    }

    char tempsub[PL_NSIZ + 5]; /* "sub  " and a \0 */

    /* Substitutions for the Quest this tile is on. */
    if (ldm == LDM_QUESTHOME ||
        ldm == LDM_QUESTFILL1 ||
        ldm == LDM_QUESTLOCATE ||
        ldm == LDM_QUESTFILL2 ||
        ldm == LDM_QUESTGOAL) {
        snprintf(tempsub, sizeof tempsub, "sub %.3s ",
                 player.rolename);
        tempsub[4] |= 32; /* convert to lowercase */
        s |= substitution_from_name(&(const char *){tempsub});
    }

    /* Race/gender of the player. TODO: For now we do this on every tile; we
       should only be doing it on the player's so as to not affect
       player-monsters on other tiles. */
    snprintf(tempsub, sizeof tempsub, "sub %s ", player.gendername);
    s |= substitution_from_name(&(const char *){tempsub});
    snprintf(tempsub, sizeof tempsub, "sub %s ", player.racename);
    s |= substitution_from_name(&(const char *){tempsub});

    return s;
}

void
print_cchar(WINDOW *win)
{
    int attr;
    cchar_t uni_out;

    wchar_t unichar[2] = {curcchar & 0x1fffff, 0};
    int fgcolor = (curcchar >> 21) & 0x0f;
    int bgcolor = (curcchar >> 26) & 0x07;

    attr = A_NORMAL;

    if (curcchar & (1UL << 30))
        attr |= A_UNDERLINE;

    attr |= curses_color_attr(fgcolor, bgcolor);

    int color = PAIR_NUMBER(attr);

    setcchar(&uni_out, unichar, attr, color, NULL);
    wadd_wch(win, &uni_out);

    /* Don't let the cchar leak from one cell to another, even if init_cchar
       isn't called */
    curcchar = '?' | (CLR_RED << 21);
}

void
init_cchar(char c)
{
    if (c == 0)
        curcchar = '?' | (CLR_RED << 21);
    else
        curcchar = c | (CLR_YELLOW << 21);
}

static unsigned long
combine_cchar(unsigned long cchar_old, unsigned long cchar_new)
{
    if (cchar_new & 0x1fffffUL) {
        cchar_old &= ~0x1fffffUL;
        cchar_old |= cchar_new & 0x1fffffUL;
    }

    if (((cchar_new >> 21) & 31) == 17) {
        /* "disturb"; cycle colors in cchar_old */
        unsigned long fgcolor = (cchar_old >> 21) & 15;
        switch (fgcolor) {
        case CLR_DARK_GRAY:
            fgcolor = settings.darkgray ? CLR_BLUE : CLR_CYAN;
            break;
        case CLR_BLUE: fgcolor = CLR_CYAN; break;
        case CLR_CYAN: fgcolor = CLR_GREEN; break;
        case CLR_GREEN: fgcolor = CLR_BROWN; break;
        case CLR_BROWN: fgcolor = CLR_RED; break;
        case CLR_RED: fgcolor = CLR_MAGENTA; break;
        case CLR_ORANGE: fgcolor = CLR_BRIGHT_MAGENTA; break;
        case CLR_BRIGHT_BLUE: fgcolor = CLR_BRIGHT_MAGENTA; break;
        case CLR_YELLOW: fgcolor = CLR_ORANGE; break;
        case CLR_WHITE: fgcolor = CLR_YELLOW; break;
        default: fgcolor = CLR_BROWN; break;
        }
        cchar_old &= ~(31UL << 21);
        cchar_old |= fgcolor << 21;
    } else if (((cchar_new >> 21) & 31) < 16) {
        cchar_old &= ~(31UL << 21);
        cchar_old |= cchar_new & (31UL << 21);
    }

    if (((cchar_new >> 26) & 15) < 8) {
        cchar_old &= ~(15UL << 26);
        cchar_old |= cchar_new & (15UL << 26);
    }

    if (!(cchar_new & (1UL << 31))) {
        cchar_old &= ~(3UL << 30);
        cchar_old |= cchar_new & (3UL << 30);
    }

    return cchar_old;
}

static inline unsigned long
get_tt_number(int tt_offset)
{
    unsigned long l = 0;
    l += (unsigned long)(unsigned char)tiletable[tt_offset + 0] << 0;
    l += (unsigned long)(unsigned char)tiletable[tt_offset + 1] << 8;
    l += (unsigned long)(unsigned char)tiletable[tt_offset + 2] << 16;
    l += (unsigned long)(unsigned char)tiletable[tt_offset + 3] << 24;
    return l;
}

static void
print_tile_number(WINDOW *win, int tileno, unsigned long long substitutions)
{
    /* Find the tile in question in the tile table. The rules:
       - The tile numbers must be equal;
       - The substitutions of the tile in the table must be a subset of
         the substitutions of the tile we want to draw;
       - If multiple tiles fit these constraints, draw the one with the
         numerically largest substitution number.

       The tile table itself is formatted as follows:
       4 bytes: tile number
       8 bytes: substiutitions
       4 bytes: image index (i.e. wset_tiles_tile argument)

       All these numbers are little-endian, regardless of the endianness of the
       system we're running on.

       The tile table is sorted by number then substitutions, so we can use a
       binary search. */

    int ttelements = tiletable_len / 16;
    int low = 0, high = ttelements;

    if (ttelements == 0)
        return; /* no tile table */

    /* Invariant: tiles not in low .. high inclusive are definitely not the
       tile we're looking for */
    while (low < high) {
        int pivot = (low + high) / 2;
        int table_tileno = get_tt_number(pivot * 16);

        if (table_tileno < tileno)
            low = pivot + 1;
        else if (table_tileno > tileno)
            high = pivot - 1;
        else {
            /* We're somewhere in the section for this tile. Find its bounds. */
            low = high = pivot;
            while (low >= 0 && get_tt_number(low * 16) == tileno)
                low--;
            low++;
            while (high < ttelements && get_tt_number(high * 16) == tileno)
                high++;
            high--;

            for (pivot = high; pivot >= low; pivot--) {
                unsigned long long table_substitutions =
                    ((unsigned long long)get_tt_number(pivot * 16 + 4)) +
                    ((unsigned long long)get_tt_number(pivot * 16 + 8) << 32);

                if ((table_substitutions & substitutions) ==
                    table_substitutions) {
                    low = pivot;
                    break;
                }
            }
            break;
        }
    }

    /* can happen if the tileset is missing high-numbered tiles */
    if (low >= ttelements)
        low = ttelements - 1;

    if (tiletable_is_cchar)
        curcchar = combine_cchar(curcchar, get_tt_number(low * 16 + 12));
    else
        wset_tiles_tile(win, get_tt_number(low * 16 + 12));
}

void
print_tile(WINDOW *win, struct curses_symdef *api_name, 
           struct curses_symdef *api_type, int offset,
           unsigned long long substitutions)
{
    int tileno = tileno_from_api_name(
        api_name->symname, api_type ? api_type->symname : NULL, offset);
    /* TODO: better rendition for missing tiles than just using the unexplored
       area tile */
    if (tileno == TILESEQ_INVALID_OFF) tileno = 0;

    print_tile_number(win, tileno, substitutions);
}

static const char *const furthest_backgrounds[] = {
    [fb_room] = "the floor of a room",
    [fb_corr] = "corridor",
};

static int furthest_background_tileno[sizeof furthest_backgrounds /
                                      sizeof *furthest_backgrounds];
static nh_bool furthest_background_tileno_needs_initializing = 1;

void
print_background_tile(WINDOW *win, struct nh_dbuf_entry *dbe)
{
    unsigned long long substitutions = dbe_substitution(dbe);
    if (furthest_background_tileno_needs_initializing) {
        int i;
        for (i = 0; i < sizeof furthest_backgrounds /
                 sizeof *furthest_backgrounds; i++) {
            furthest_background_tileno[i] =
                tileno_from_name(furthest_backgrounds[i], TILESEQ_CMAP_OFF);
        }
        furthest_background_tileno_needs_initializing = 0;
    }

    print_tile_number(win, furthest_background_tileno[furthest_background(dbe)],
                      substitutions);

    if (!settings.visible_rock &&
        !strcmp("stone", default_drawing->bgelements[dbe->bg].symname))
        print_tile_number(win, tileno_from_name("unexplored area",
                                                TILESEQ_CMAP_OFF),
                          substitutions);
    else
        print_tile(win, default_drawing->bgelements + dbe->bg,
                   NULL, TILESEQ_CMAP_OFF, substitutions);
}

/* outchars.c */
