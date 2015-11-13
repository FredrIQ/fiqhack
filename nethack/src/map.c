/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#ifdef WIN32
# define boolean save_boolean /* rpcndr.h defines "boolean" */
# include <windows.h> /* must be before compilers.h */
# undef boolean
#else
# include <sys/time.h>
#endif

#include "tilesequence.h"
#include "nhcurses.h"
#include "brandings.h"
#include <ctype.h>

#define sgn(x) ((x) >= 0 ? 1 : -1)

struct coord {
    int x, y;
};

static struct nh_dbuf_entry display_buffer[ROWNO][COLNO];
static struct nh_dbuf_entry onscreen_display_buffer[ROWNO][COLNO];
static nh_bool fully_refresh_display_buffer = 1;
static const int mxdir[DIR_SELF + 1] = { -1, -1, 0, 1, 1, 1, 0, -1, 0, 0 };
static const int mydir[DIR_SELF + 1] = { 0, -1, -1, -1, 0, 1, 1, 1, 0, 0 };

int
get_map_key(nh_bool place_cursor, nh_bool report_clicks,
            enum keyreq_context context)
{
    int key = ERR;

    static int consecutive = 0;
    static int last_x = 0;
    static int last_y = 0;

    if (context == krc_interrupt_long_action) {
        consecutive++;
        int timeout = settings.animation == ANIM_SLOW ? 5000 : 700;

        /* Before sleeping, update the message buffer. This might end up leaving
           room for a --More-- that isn't required, but it can't be helped; it's
           better than not leaving room for a --More-- that is required. */
        draw_messages_prekey(TRUE);
        wtimeout(mapwin, timeout / (consecutive + 10));
    } else
        consecutive = 0;

    if (player.x && place_cursor) {     /* x == 0 is not a valid coordinate */
        wmove(mapwin, player.y, player.x);
        nh_curs_set(1);
    }

    if (player.x != last_x || player.y != last_y)
        consecutive = 0;

    last_x = player.x;
    last_y = player.y;

    while (1) {
        key = nh_wgetch(mapwin, context);

        if (key == KEY_UNHOVER || key >= KEY_MAX + 256) {
            /* A mouse action on the map. */

            if (key == KEY_UNHOVER || !report_clicks ||
                key >= KEY_MAX + 256 + (ROWNO * COLNO * 2))
                continue;
        }

        draw_map(player.x, player.y);

        if (key != ERR || context == krc_interrupt_long_action)
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
        nh_curs_set(1);
    } else
        nh_curs_set(0);
    wnoutrefresh(mapwin);
}

void
mark_mapwin_for_full_refresh(void)
{
    fully_refresh_display_buffer = 1;
}

nh_bool
branding_is_at(short branding, int x, int y)
{
    return !!(display_buffer[y][x].branding & branding);
}

nh_bool
monflag_is_at(short monflag, int x, int y)
{
    return !!(display_buffer[y][x].monflags & monflag);
}

nh_bool
apikey_is_at(const char *apikey, int x, int y)
{
    struct nh_dbuf_entry *dbyx = &(display_buffer[y][x]);
    if (!strcmp(apikey, default_drawing->bgelements[dbyx->bg].symname))
        return TRUE;
    if (dbyx->trap && !strcmp(
            apikey, default_drawing->traps[dbyx->trap - 1].symname))
        return TRUE;
    if (dbyx->obj && !strcmp(
            apikey, default_drawing->objects[dbyx->obj - 1].symname))
        return TRUE;
    if (dbyx->mon && !(dbyx->monflags & MON_WARNING) && !strcmp(
            apikey, default_drawing->monsters[dbyx->mon - 1].symname))
        return TRUE;
    if (dbyx->mon && (dbyx->monflags & MON_WARNING) && !strcmp(apikey,
            default_drawing->monsters[dbyx->mon - 1 -
                                      default_drawing->num_monsters].symname))
        return TRUE;
    return FALSE;
}

void
draw_map(int cx, int cy)
{
    int x, y, cursx, cursy, mapwinw, mapwinh;

    if (!mapwin)
        return;

    getyx(mapwin, cursy, cursx);
    getmaxyx(mapwin, mapwinh, mapwinw);

    for (y = 0; y < mapwinh && y < ROWNO; y++) {
        for (x = 0; x < mapwinw && x < COLNO; x++) {
            struct nh_dbuf_entry *dbyx = &(display_buffer[y][x]);

            if (!fully_refresh_display_buffer &&
                memcmp(dbyx, &(onscreen_display_buffer[y][x]),
                       sizeof *dbyx) == 0)
                continue; /* no need to redraw an unchanged tile */

            unsigned long long substitution = dbe_substitution(dbyx);

            onscreen_display_buffer[y][x] = *dbyx;

            /* set the position for each character to prevent incorrect
               positioning due to charset issues (IBM chars on a unicode term
               or vice versa) */
            wmove(mapwin, y, x);

            /* a fallback for if rendering fails */
            {
                static const char errmsg_line1[] =
                    "Your selected interface cannot render this tileset.";
                static const char errmsg_line2[] =
                    "Select a different tileset with the 'tileset' option.";

                const char *errmsg = NULL;
                int errlen = 0;

                init_cchar(0);

                if (y == 9) {
                    errmsg = errmsg_line1;
                    errlen = sizeof errmsg_line1 - 1;
                } else if (y == 11) {
                    errmsg = errmsg_line2;
                    errlen = sizeof errmsg_line2 - 1;
                }
                if (errmsg) {
                    int xmod = (COLNO - errlen) / 2;
                    if (x - xmod >= 0 && x - xmod < errlen)
                        init_cchar(errmsg[x - xmod]);
                    else if (x - xmod == -1 || x - xmod == errlen)
                        init_cchar(' ');
                }
            }

            /* make the map mouse-active for left clicks, middle/right clicks
               (which are interchangeable, because some terminals don't allow
               right clicks), and hovers */
            wset_mouse_event(mapwin, uncursed_mbutton_left, KEY_MAX + 256 + 
                             (ROWNO * COLNO * 0) + x + y * COLNO, KEY_CODE_YES);
            wset_mouse_event(mapwin, uncursed_mbutton_middle, KEY_MAX + 256 + 
                             (ROWNO * COLNO * 1) + x + y * COLNO, KEY_CODE_YES);
            wset_mouse_event(mapwin, uncursed_mbutton_right, KEY_MAX + 256 + 
                             (ROWNO * COLNO * 1) + x + y * COLNO, KEY_CODE_YES);
            wset_mouse_event(mapwin, uncursed_mbutton_hover, KEY_MAX + 256 + 
                             (ROWNO * COLNO * 2) + x + y * COLNO, KEY_CODE_YES);
            
            /* draw the tile first, because doing that doesn't move the cursor;
               backgrounds are special because they can be composed from
               multiple tiles (e.g. furthest background + fountain) */
            print_background_tile(mapwin, dbyx);

            /* low-priority general brandings */
            print_low_priority_brandings(mapwin, dbyx);
            /* traps */
            if (dbyx->trap)
                print_tile(mapwin, default_drawing->traps + dbyx->trap-1,
                           NULL, TILESEQ_TRAP_OFF, substitution);
            /* objects */
            if (dbyx->obj) {
                /* Special cases: corpses and statues are substituted monsters,
                   not objects. Check to ensure that they have a valid monster
                   number before rendering it, because otherwise we can get
                   crashes. (The server should be checking this for validity, as
                   should the client API, but they aren't, and a redundant check
                   here will nonetheless never hurt.) */
                if (substitution & (NHCURSES_SUB_CORPSE | NHCURSES_SUB_STATUE |
                                    NHCURSES_SUB_FIGURINE)
                    && dbyx->obj_mn > 0
                    && dbyx->obj_mn <= default_drawing->num_monsters)
                    print_tile(mapwin, default_drawing->monsters +
                               dbyx->obj_mn-1, NULL,
                               TILESEQ_MON_OFF, substitution);
                else
                    print_tile(mapwin, default_drawing->objects + dbyx->obj-1,
                               NULL, TILESEQ_OBJ_OFF, substitution);
            }
            /* invisible monster symbol; just use the tile number directly, no
               need to go via an API name because there is only one */
            if (dbyx->invis)
                print_tile(mapwin, &(struct curses_symdef){
                        .symname = invismonexplain},
                           NULL, TILESEQ_INVIS_OFF, substitution);
            /* monsters */
            if (dbyx->mon && dbyx->mon <= default_drawing->num_monsters)
                print_tile(mapwin, default_drawing->monsters + dbyx->mon-1,
                           NULL, TILESEQ_MON_OFF, substitution &
                           ~(NHCURSES_SUB_CORPSE | NHCURSES_SUB_STATUE |
                             NHCURSES_SUB_FIGURINE));
            /* warnings */
            if (dbyx->mon > default_drawing->num_monsters &&
                (dbyx->monflags & MON_WARNING))
                print_tile(mapwin, default_drawing->warnings +
                               dbyx->mon-1-default_drawing->num_monsters,
                           NULL, TILESEQ_WARN_OFF, substitution);
            /* high-priority brandings */
            print_high_priority_brandings(mapwin, dbyx);
            /* effects */
            if (dbyx->effect) {
                int id = NH_EFFECT_ID(dbyx->effect);
                switch (NH_EFFECT_TYPE(dbyx->effect)) {
                case E_EXPLOSION:
                    print_tile(mapwin,
                               default_drawing->explsyms + (id % NUMEXPCHARS),
                               default_drawing->expltypes + (id / NUMEXPCHARS),
                               TILESEQ_EXPLODE_OFF, substitution);
                    break;
                case E_SWALLOW:
                    print_tile(mapwin,
                               default_drawing->swallowsyms + (id & 0x7),
                               NULL, TILESEQ_SWALLOW_OFF, substitution);
                    break;
                case E_ZAP:
                    print_tile(mapwin,
                               default_drawing->zapsyms + (id & 0x3),
                               default_drawing->zaptypes + (id >> 2),
                               TILESEQ_ZAP_OFF, substitution);
                    break;
                case E_MISC:
                    print_tile(mapwin,
                               default_drawing->effects + id,
                               NULL, TILESEQ_EFFECT_OFF, substitution);
                    break;
                }
            }

            print_cchar(mapwin);
        }
    }

    wset_mouse_event(mapwin, uncursed_mbutton_left, 0, ERR);
    wset_mouse_event(mapwin, uncursed_mbutton_middle, 0, ERR);
    wset_mouse_event(mapwin, uncursed_mbutton_right, 0, ERR);
    wset_mouse_event(mapwin, uncursed_mbutton_hover, 0, ERR);

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

    ui_flags.maphoverx = -1;
    ui_flags.maphovery = -1;

    cx = xorig >= 0 && xorig < COLNO ? xorig : player.x;
    cy = yorig >= 0 && yorig < ROWNO ? yorig : player.y;
    wmove(mapwin, cy, cx);

    while (1) {
        dx = dy = 0;
        nh_curs_set(1);
        key = get_map_key(FALSE, TRUE, krc_getpos);
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

        if (key >= KEY_MAX + 256) {
            nh_bool rightclick = FALSE;
            /* The user clicked on the map. For a left click, we'll accept the
               new location (NHCR_ACCEPTED). For a right click, we'll move the
               cursor there, but not accept it. */
            key -= KEY_MAX + 256;
            if (key > ROWNO * COLNO) {
                key -= ROWNO * COLNO;
                rightclick = TRUE;
            }

            cx = key % COLNO;
            cy = key / COLNO;

            if (rightclick)
                goto nxtc;

            result = NHCR_ACCEPTED;
            break;
        }

        if ((cp = strchr(pick_chars, (char)key)) != 0) {
            /* '.' => 0, ',' => 1, ';' => 2, ':' => 3 */
            result = pick_vals[cp - pick_chars];
            break;
        }

        int range;
        dir = key_to_dir(key, &range);
        if (dir != DIR_NONE) {
            dx = mxdir[dir] * range;
            dy = mydir[dir] * range;
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

                snprintf(printbuf, ARRAY_SIZE(printbuf),
                         "Can't find dungeon feature '%c'.", (char)key);
                curses_msgwin(printbuf, krc_notification);
            } else {
                snprintf(printbuf, ARRAY_SIZE(printbuf),
                         "Unknown targeting key%s.",
                         !force ? " (ESC to abort)" : "");
                curses_msgwin(printbuf, krc_notification);
            }
        }
        /* fall through; an invalid command at the direction screen shouldn't
           cause us to abort (that's what ESC is for) */

    nxtc:
        wmove(mapwin, cy, cx);
        wnoutrefresh(mapwin);

        /* If we get here, then the user must have pressed a key that didn't
           close the getpos prompt; and it must have been an actual key (or
           onscreen keyboard key), not a map mouse event (which would either
           have been handled by get_map_key or else closed the prompt). Thus, we
           hijack the map hover locations for the cursor location, rather than
           the mouse location, as the user's clearly trying to do things with
           keyboard controls.

           Exception: Right-clicks on the map move the cursor to the click
           location, so although that codepath reaches here, this code is still
           correct, if for a different reason. */
        ui_flags.maphoverx = cx;
        ui_flags.maphovery = cy;
    } /* while (1) */

    ui_flags.maphoverx = -1;
    ui_flags.maphovery = -1;

    curses_update_status(NULL); /* clear the help message */
    return (struct nh_getpos_result){.x = cx, .y = cy, .howclosed = result};
}
