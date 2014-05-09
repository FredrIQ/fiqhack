/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-09 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "tilesequence.h"
#include "nhcurses.h"
#include "brandings.h"
#include <ctype.h>

#ifdef WIN32
# include <windows.h>
#else
# include <sys/time.h>
#endif

#define sgn(x) ((x) >= 0 ? 1 : -1)

struct coord {
    int x, y;
};

static struct nh_dbuf_entry display_buffer[ROWNO][COLNO];
static struct nh_dbuf_entry onscreen_display_buffer[ROWNO][COLNO];
static nh_bool fully_refresh_display_buffer = 1;
static const int mxdir[DIR_SELF + 1] = { -1, -1, 0, 1, 1, 1, 0, -1, 0, 0 };
static const int mydir[DIR_SELF + 1] = { 0, -1, -1, -1, 0, 1, 1, 1, 0, 0 };

/* GetTickCount() returns milliseconds since the system was started, with a
 * resolution of around 15ms. gettimeofday() returns a value since the start of
 * the epoch.
 * The difference doesn't matter here, since the value is only used to control
 * blinking.
 */
#ifdef WIN32
# define get_milliseconds GetTickCount
#else
static int
get_milliseconds(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif



int
get_map_key(int place_cursor)
{
    int key = ERR;

    if (settings.blink)
        wtimeout(mapwin, 666);  /* wait 2/3 of a second before switching */

    if (player.x && place_cursor) {     /* x == 0 is not a valid coordinate */
        wmove(mapwin, player.y, player.x);
        curs_set(1);
    }

    while (1) {
        key = nh_wgetch(mapwin);
        draw_map(player.x, player.y);
        doupdate();
        if (key != ERR)
            break;

    };
    wtimeout(mapwin, -1);

    return key;
}


void
curses_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO], int ux, int uy)
{
    memcpy(display_buffer, dbuf, sizeof (struct nh_dbuf_entry) * ROWNO * COLNO);
    draw_map(ux, uy);

    if (ux >= 0) {
        wmove(mapwin, uy, ux);
        curs_set(1);
    } else
        curs_set(0);
    wnoutrefresh(mapwin);
}

void
mark_mapwin_for_full_refresh(void)
{
    fully_refresh_display_buffer = 1;
}

void
draw_map(int cx, int cy)
{
    int x, y, symcount, attr, cursx, cursy;
    unsigned int frame;
    struct curses_symdef syms[4];

    if (!mapwin)
        return;

    getyx(mapwin, cursy, cursx);

    frame = 0;
    if (settings.blink) {
        frame = get_milliseconds() / 666;
        fully_refresh_display_buffer = 1;
    }

    for (y = 0; y < ROWNO; y++) {
        for (x = 0; x < COLNO; x++) {
            int bg_color = 0;
            struct nh_dbuf_entry *dbyx = &(display_buffer[y][x]);

            if (!fully_refresh_display_buffer &&
                memcmp(dbyx, &(onscreen_display_buffer[y][x]),
                       sizeof *dbyx) == 0)
                continue; /* no need to redraw an unchanged tile */

            onscreen_display_buffer[y][x] = *dbyx;

            /* set the position for each character to prevent incorrect
               positioning due to charset issues (IBM chars on a unicode term
               or vice versa) */
            wmove(mapwin, y, x);

            /* draw the tile first, because doing that doesn't move the cursor;
               backgrounds are special because they can be composed from
               multiple tiles (e.g. dark room + fountain), or have no
               correpondence to the API key (e.g. lit corridor) */
            print_background_tile(mapwin, dbyx);

            /* low-priority general brandings */
            print_low_priority_brandings(mapwin, dbyx);
            /* traps */
            if (dbyx->trap)
                print_tile(mapwin, cur_drawing->traps + dbyx->trap-1,
                           NULL, TILESEQ_TRAP_OFF);
            /* objects */
            if (dbyx->obj)
                print_tile(mapwin, cur_drawing->objects + dbyx->obj-1,
                           NULL, TILESEQ_OBJ_OFF);
            /* invisible monster symbol; just use the tile number directly, no
               need to go via an API name because there is only one */
            if (dbyx->invis)
                wset_tiles_tile(mapwin, TILESEQ_INVIS_OFF + 0);
            /* monsters */
            if (dbyx->mon && dbyx->mon <= cur_drawing->num_monsters)
                print_tile(mapwin, cur_drawing->monsters + dbyx->mon-1,
                           NULL, TILESEQ_MON_OFF);
            /* warnings */
            if (dbyx->mon > cur_drawing->num_monsters &&
                (dbyx->monflags & MON_WARNING))
                print_tile(mapwin, cur_drawing->warnings +
                               dbyx->mon-1-cur_drawing->num_monsters,
                           NULL, TILESEQ_WARN_OFF);
            /* high-priority brandings */
            print_high_priority_brandings(mapwin, dbyx);
            /* effects */
            if (dbyx->effect) {
                int id = NH_EFFECT_ID(dbyx->effect);
                switch (NH_EFFECT_TYPE(dbyx->effect)) {
                case E_EXPLOSION:
                    print_tile(mapwin,
                               cur_drawing->explsyms + (id % NUMEXPCHARS),
                               cur_drawing->expltypes + (id / NUMEXPCHARS),
                               TILESEQ_EXPLODE_OFF);
                    break;
                case E_SWALLOW:
                    print_tile(mapwin,
                               cur_drawing->swallowsyms + (id & 0x7),
                               NULL, TILESEQ_SWALLOW_OFF);
                    break;
                case E_ZAP:
                    print_tile(mapwin,
                               cur_drawing->zapsyms + (id & 0x3),
                               cur_drawing->zaptypes + (id >> 2),
                               TILESEQ_ZAP_OFF);
                    break;
                case E_MISC:
                    print_tile(mapwin,
                               cur_drawing->effects + id,
                               NULL, TILESEQ_EFFECT_OFF);
                    break;
                }
            }

            symcount = mapglyph(dbyx, syms, &bg_color);
            attr = A_NORMAL;
            if (!(COLOR_PAIRS >= 113 || (COLORS < 16 && COLOR_PAIRS >= 57))) {
                /* we don't have background colors available */
                bg_color = 0;
                if (((dbyx->monflags & MON_TAME) && settings.hilite_pet) ||
                    ((dbyx->monflags & MON_DETECTED) && settings.use_inverse))
                    attr |= A_REVERSE;
            } else if (bg_color == 0) {
                /* we do have background colors available */
                if ((dbyx->monflags & MON_DETECTED) && settings.use_inverse)
                    bg_color = CLR_MAGENTA;
                if ((dbyx->monflags & MON_PEACEFUL) && settings.hilite_pet)
                    bg_color = CLR_BROWN;
                if ((dbyx->monflags & MON_TAME) && settings.hilite_pet)
                    bg_color = CLR_BLUE;
            }
            print_sym(mapwin, &syms[frame % symcount], attr, bg_color);
        }
    }

    fully_refresh_display_buffer = 0;
    wmove(mapwin, cursy, cursx);
    wnoutrefresh(mapwin);
}


static int
compare_coord_dist(const void *p1, const void *p2)
{
    const struct coord *c1 = p1;
    const struct coord *c2 = p2;
    int dx, dy, dist1, dist2;

    dx = player.x - c1->x;
    dy = player.y - c1->y;
    dist1 = dx * dx + dy * dy;
    dx = player.x - c2->x;
    dy = player.y - c2->y;
    dist2 = dx * dx + dy * dy;

    return dist1 - dist2;
}

static void
place_desc_message(WINDOW * win, int *x, int *y, char *b)
{
    if (*b) {
        b[78] = 0;
        if (strlen(b) >= 40 && *x >= 40) {
            (*y)++;
            *x = 0;
        }
        if (*y <= 1 && statuswin)
            mvwaddstr(statuswin, *y, *x, b);
        (*x) += (strlen(b) >= 38 ? 80 : 40);
        if (*x > 40) {
            (*y)++;
            *x = 0;
        }
    }
}

struct nh_getpos_result
curses_getpos(int xorig, int yorig, nh_bool force, const char *goal)
{
    int result = NHCR_ACCEPTED;
    int cx, cy;
    int key, dx, dy;
    int sidx;
    static const char pick_chars[] = " \r\n.,;:";
    static const int pick_vals[] = {
        NHCR_ACCEPTED,
        NHCR_ACCEPTED,
        NHCR_ACCEPTED,
        NHCR_ACCEPTED,
        NHCR_CONTINUE,
        NHCR_MOREINFO_CONTINUE,
        NHCR_MOREINFO};
    const char *cp;
    char printbuf[BUFSZ];
    enum nh_direction dir;

    int moncount = 0, monidx = 0;
    int firstmove = 1;
    nh_bool first_monidx_change = TRUE;

    int i, j;

    for (i = 0; i < ROWNO; i++)
        for (j = 0; j < COLNO; j++)
            if (display_buffer[i][j].mon &&
                (j != player.x || i != player.y))
                moncount++;

    struct coord monpos[moncount * sizeof (struct coord)];
    monidx = 0;
    for (i = 0; i < ROWNO; i++)
        for (j = 0; j < COLNO; j++)
            if (display_buffer[i][j].mon &&
                (j != player.x || i != player.y)) {
                monpos[monidx].x = j;
                monpos[monidx].y = i;
                monidx++;
            }
    qsort(monpos, moncount, sizeof (struct coord),
          compare_coord_dist);
    monidx = 0;

    if (statuswin) {
        werase(statuswin);
        mvwaddstr(statuswin, 0, 0,
                  "Move the cursor with the direction keys. Press "
                  "the letter of a dungeon symbol");
        mvwaddstr(statuswin, 1, 0,
                  "to select it or use m/M to move to a nearby "
                  "monster. Finish with one of .,;:");
        wrefresh(statuswin);
    }

    cx = xorig >= 0 && xorig < COLNO ? xorig : player.x;
    cy = yorig >= 0 && yorig < ROWNO ? yorig : player.y;
    wmove(mapwin, cy, cx);

    while (1) {
        if (!firstmove) {
            struct nh_desc_buf descbuf;
            int mx = 0, my = 0;

            nh_describe_pos(cx, cy, &descbuf, NULL);

            if (statuswin) {
                werase(statuswin);
                place_desc_message(statuswin, &mx, &my, descbuf.effectdesc);
                place_desc_message(statuswin, &mx, &my, descbuf.invisdesc);
                place_desc_message(statuswin, &mx, &my, descbuf.mondesc);
                place_desc_message(statuswin, &mx, &my, descbuf.objdesc);
                place_desc_message(statuswin, &mx, &my, descbuf.trapdesc);
                place_desc_message(statuswin, &mx, &my, descbuf.bgdesc);
                wrefresh(statuswin);
            }

            wmove(mapwin, cy, cx);
        }
        firstmove = 0;
        dx = dy = 0;
        curs_set(1);
        key = get_map_key(FALSE);
        if (key == KEY_ESCAPE || key == '\x1b') {
            cx = cy = -10;
            result = NHCR_CLIENT_CANCEL;
            break;
        }
        if (key == KEY_SIGNAL) {
            cx = cy = -10;
            result = NHCR_SERVER_CANCEL;
            break;
        }

        if ((cp = strchr(pick_chars, (char)key)) != 0) {
            /* '.' => 0, ',' => 1, ';' => 2, ':' => 3 */
            result = pick_vals[cp - pick_chars];
            break;
        }

        dir = key_to_dir(key);
        if (dir != DIR_NONE) {
            dx = mxdir[dir];
            dy = mydir[dir];
        } else if ((dir = key_to_dir(tolower((char)key))) != DIR_NONE) {
            /* a shifted movement letter */
            dx = mxdir[dir] * 8;
            dy = mydir[dir] * 8;
        }

        if (dx || dy) {
            /* truncate at map edge */
            if (cx + dx < 0)
                dx = 0;
            if (cx + dx > COLNO - 1)
                dx = 0;
            if (cy + dy < 0)
                dy = 0;
            if (cy + dy > ROWNO - 1)
                dy = 0;
            cx += dx;
            cy += dy;
            goto nxtc;
        }

        if (key == 'm' || key == 'M') {
            if (moncount) {     /* there is at least one monster to move to */
                if (!first_monidx_change) {
                    if (key == 'm') {
                        monidx = (monidx + 1) % moncount;
                    } else {
                        monidx--;
                        if (monidx < 0)
                            monidx += moncount;
                    }
                }

                first_monidx_change = FALSE;

                cx = monpos[monidx].x;
                cy = monpos[monidx].y;
            }
        } else {
            int k = 0, tx, ty;
            int pass, lo_x, lo_y, hi_x, hi_y;

            char matching[default_drawing->num_bgelements];
            memset(matching, 0, default_drawing->num_bgelements);

            for (sidx = default_drawing->bg_feature_offset;
                 sidx < default_drawing->num_bgelements; sidx++)
                if (key == default_drawing->bgelements[sidx].ch)
                    matching[sidx] = (char)++k;

            if (k) {
                for (pass = 0; pass <= 1; pass++) {
                    /* pass 0: just past current pos to lower right; pass 1:
                       upper left corner to current pos */
                    lo_y = (pass == 0) ? cy : 0;
                    hi_y = (pass == 0) ? ROWNO - 1 : cy;
                    for (ty = lo_y; ty <= hi_y; ty++) {
                        lo_x = (pass == 0 && ty == lo_y) ? cx + 1 : 0;
                        hi_x = (pass == 1 && ty == hi_y) ? cx : COLNO - 1;
                        for (tx = lo_x; tx <= hi_x; tx++) {
                            k = display_buffer[ty][tx].bg;
                            if (k && matching[k]) {
                                cx = tx;
                                cy = ty;
                                goto nxtc;
                            }
                        }   /* column */
                    }   /* row */
                }   /* pass */

                sprintf(printbuf, "Can't find dungeon feature '%c'.",
                        (char)key);
                curses_msgwin(printbuf);
            } else {
                sprintf(printbuf, "Unknown targeting key%s.",
                        !force ? " (ESC to abort)" : "");
                curses_msgwin(printbuf);
            }
        }
        /* fall through; an invalid command at the direction screen shouldn't
           cause us to abort (that's what ESC is for) */

    nxtc:
        wmove(mapwin, cy, cx);
        wrefresh(mapwin);
    }

    curses_update_status(NULL); /* clear the help message */
    return (struct nh_getpos_result){.x = cx, .y = cy, .howclosed = result};
}
