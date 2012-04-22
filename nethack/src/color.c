/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

#define array_size(x) (sizeof(x)/sizeof(x[0]))

short colorlist[] = {COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
                     COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, -1,
                     COLOR_WHITE, COLOR_RED+8, COLOR_GREEN+8, COLOR_YELLOW+8,
                     COLOR_BLUE+8, COLOR_MAGENTA+8, COLOR_CYAN+8, COLOR_WHITE+8};

const char *colorNames[] = { "black", "red", "green", "yellow", "blue", 
                             "magenta", "cyan", "gray", "white",
                             "hired", "higreen", "hiyellow", "hiblue",
                             "himagenta", "hicyan", "hiwhite" };
struct ColorMap {
  short fgColors[16];
  short bgColors[16];
};

static struct ColorMap color_map;

/* Load color map from colormap.conf config file. */
static void read_colormap( struct ColorMap *map )
{
    fnchar filename[BUFSZ];
    FILE *fp;

    char line[BUFSZ];

    int pos, idx;
    char *colorname;

    int defType, colorIndex, colorValue;

    /* Initialize the map to default values. */
    for(idx = 0; idx < 16; idx++) {
        map->fgColors[idx] = colorlist[idx];
        map->bgColors[idx] = colorlist[idx];
    }

    filename[0] = '\0';
    if(ui_flags.connection_only || !get_gamedir(CONFIG_DIR, filename))
        return;
    fnncat(filename, FN("colormap.conf"), BUFSZ);

    fp = fopen(filename, "r");
    if(!fp)
        return;

    while(fgets(line, BUFSZ, fp)) {
        if(sscanf(line, " %n%*s %i", &pos, &colorValue) != 1) continue;

        colorname = &line[pos];

        /* Skip comments. */
        if(colorname[0] == '#') continue;

        /* If the color name starts with "fg." or "bg.", then it only
         * applies to the foreground or background color definition.
         * Otherwise it applies to both. */
        if(!strncmp(colorname, "fg.", 3)) {
            defType = 1;
            colorname += 3;
        } else if(!strncmp(colorname, "bg.", 3)) {
            defType = 2;
            colorname += 3;
        } else {
            defType = 0;
        }

        for(idx = 0, colorIndex = -1; idx < array_size(colorNames); idx++) {
            if(colorNames[idx] && !strncmp(colorname, colorNames[idx], 
                                           strlen(colorNames[idx]))) {
                colorIndex = idx;
                break;
            }
        }

        /* If color couldn't be matched, then skip the line. */
        if(colorIndex == -1) continue;

        if(colorValue < COLORS && colorValue >= 0) {
            if(defType == 0 || defType == 1) {
                map->fgColors[colorIndex] = colorValue;
            }
            if(defType == 0 || defType == 2) {
                map->bgColors[colorIndex] = colorValue;
            }
        }
    }

    fclose(fp);

    return;
}

/* Initialize curses color pairs based on the color map provided. */
static void apply_colormap( struct ColorMap *map )
{
    int bg, fg;
    short bgColor, fgColor;

    /* Set up all color pairs.
     * If using bold, then set up color pairs for foreground colors
     *   0-7; if not, then set up color pairs for foreground colors
     *   0-15.
     * If there are sufficient color pairs, then set them up for 6
     * possible non-default background colors (don't use white, there 
     * are terminals that hate it).  So there are 112 pairs required
     * for 16 colors, or 56 required for 8 colors. */
    for (bg = 0; bg <= 6; bg++) {

        /* Do not set up background colors if there are not enough
         * color pairs. */
        if(bg == 1 &&
           ((COLOR_PAIRS < 57) ||
            (COLORS >= 16 && COLOR_PAIRS < 113)))
            break;
        
        /* For no background, use the default background color;
         * otherwise use the color from the color map. */
        bgColor = bg ? map->bgColors[bg] : -1;

        for (fg = 0; fg <= (COLORS >= 16 ? 16 : 8); fg++) {

            /* Replace black with blue if darkgray is not set. */
            fgColor = map->fgColors[fg];
            if(fgColor == COLOR_BLACK && !settings.darkgray)
                fgColor = COLOR_BLUE;
            if(fgColor == bgColor && fgColor != -1)
                fgColor = COLOR_BLACK;

            init_pair(bg * (COLORS >= 16 ? 16 : 8) + fg + 1,
                      fgColor, bgColor);
        }
    }

    return;
}

/*
 * Initialize curses colors to colors used by NetHack
 * (from Karl Garrison's curses UI for Nethack 3.4.3)
 */
void init_nhcolors(void)
{
    if (!has_colors())
	return;

    ui_flags.color = TRUE;

    start_color();
    use_default_colors();
    read_colormap(&color_map);
    apply_colormap(&color_map);
}


int curses_color_attr(int nh_color, int bg_color)
{
    int color = nh_color + 1;
    int cattr = A_NORMAL;

    if(color_map.fgColors[nh_color] == COLOR_BLACK && settings.darkgray)
        cattr |= A_BOLD;

    if (COLORS < 16 && color > 8) {
	color -= 8;
	cattr = A_BOLD;
    }
    if (COLOR_PAIRS >= 113 || (COLORS < 16 && COLOR_PAIRS >= 57)) {
        color += bg_color * (COLORS >= 16 ? 16 : 8);
    }
    cattr |= COLOR_PAIR(color);

    return cattr;
}


void set_darkgray(void)
{
    apply_colormap(&color_map);
}

/* color.c */
