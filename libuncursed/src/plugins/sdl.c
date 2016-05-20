/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-02-28 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */
/* This is a graphical backend for the uncursed rendering library, that uses SDL
   to do its rendering. */

#define PNG_SETJMP_SUPPORTED
#include <png.h>

#define SDL_MAIN_HANDLED        /* don't use SDL's main, use the calling
                                   process's */

#undef __MINGW32__              /* work around a bug in SDL_platform.h */

#include <SDL2/SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "uncursed.h"
#include "uncursed_hooks.h"
#include "uncursed_sdl.h"

#ifdef AIMAKE_BUILDOS_MSWin32
# include <Winsock2.h>
# include <Ws2def.h>
#else
# include <sys/select.h>
#endif

#include <signal.h>

#define debugprintf(...) do {                   \
        if(debug)                               \
            printf(__VA_ARGS__);                \
    } while(0)

static int fontwidth = 8;
static int fontheight = 14;
static int winwidth = 132;    /* width of the window, in units of fontwidth */
static int winheight = 39;    /* height of the window, in units of fontheight */

static int resize_queued = 0;
static int resized_recently = 0;

static int mouse_active = 0;
static int mouse_hovering = 0;
static int mouse_hover_x = 0;
static int mouse_hover_y = 0;

static int cursor_x = 0;
static int cursor_y = 0;
static int cursor_visible = 1;

/* We keep a timestamp that updates when the location of a region needs to
   change on screen, typically due to cursor movements (but which could also
   happen as the result of a font change). */
static unsigned short cursor_timestamp = 0;

static char *tileset_filename = NULL;
static int tileset_width;
static int tileset_height;

static int hangup_mode = 0;

static SDL_TimerID timerid;

static fd_set monitored_fds;
static int monitored_fds_count_or_max;

static int debug = 0;

static int sdl_hook_getkeyorcodepointx(int, int *);

/* force the minimum size as 80x24; many programs don't function properly with
   less than that */
#define MINCHARWIDTH 80
#define MINCHARHEIGHT 24

struct sdl_tile_region {
    int loc_w, loc_h, loc_l, loc_t;

    SDL_Texture *texture;
    SDL_Texture *cursor;
    SDL_Texture *tileset;
    int *tiles;

    int tilesize_w, tilesize_h;
    int texsize_w, texsize_h;
    int tilecount_w, tilecount_h;
    int tileset_cols, tileset_rows;

    int pixelshift_x, pixelshift_y;
    int cursortile_x, cursortile_y;
    unsigned short pixelshift_timestamp;

    char dirty; /* used as a boolean */
};

static void update_cell(int y, int x, struct sdl_tile_region *current_region);
static Uint32 timer_callback(Uint32, void *);

static SDL_Window *win = NULL;
static SDL_Renderer *render = NULL;
static SDL_Texture *font = NULL;
static SDL_Texture *screen = NULL;
static SDL_Texture *rendertarget = NULL; /* most recently used render target */


static SDL_Texture *
load_png_file_to_texture(const char *filename, int *w, int *h)
{
    /* Based on a public domain example that ships with libpng */
    volatile png_structp png_ptr;
    png_structp png_ptr_nv;
    volatile png_infop info_ptr;
    png_infop info_ptr_nv;

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, has_alpha, i;

    SDL_Surface *surface = NULL;
    SDL_Texture *volatile rv = NULL;

    /* The pointers are volatile. What they point to isn't. */
    unsigned char *volatile pixeldata = NULL;
    unsigned char **volatile rowpointers = NULL;

    if (!filename)
        goto cleanup_nothing;

    FILE *in = fopen(filename, "rb");

    if (!in) {
        perror(filename);
        goto cleanup_nothing;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "Error allocating png_ptr for image file\n");
        goto cleanup_fopen;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Error allocating info_ptr for image file\n");
        goto cleanup_png_ptr;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error reading image file: bad PNG format\n");
        goto cleanup_info_and_rowpointers;
    }

    png_init_io(png_ptr, in);

    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 &interlace_type, NULL, NULL);

    /* Change the data to a standard format (32bpp RGBA). */

    /* Force pixels into individual bits */
    if (bit_depth < 8)
        png_set_packing(png_ptr);

    /* Force to 8bpp */
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    /* Convert color key to alpha */
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
        has_alpha = 1;
    } else
        has_alpha = (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
                     color_type == PNG_COLOR_TYPE_RGB_ALPHA);

    /* Force to RGB */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
        color_type == PNG_COLOR_TYPE_GRAY)
        png_set_gray_to_rgb(png_ptr);

    /* TODO: Do we want gamma processing? */

    /* Add an alpha channel, if the image still hasn't got one */
    if (!has_alpha)
        png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

    /* Allocate memory. */
    png_read_update_info(png_ptr, info_ptr);
    if (png_get_rowbytes(png_ptr, info_ptr) != width * 4) {
        fprintf(stderr,
                "Error in PNG memory allocation: expected %ld bytes, "
                "found %ld bytes.\n", (long)width * 4,
                (long)png_get_rowbytes(png_ptr, info_ptr));
    }

    pixeldata = malloc(height * width * 4);
    rowpointers = malloc(sizeof *rowpointers * height);
    if (!pixeldata || !rowpointers) {
        fprintf(stderr, "Error allocating image memory for PNG file\n");
        goto cleanup_info_and_rowpointers;
    }

    for (i = 0; i < height; i++)
        rowpointers[i] = pixeldata + (i * width * 4);
    png_read_image(png_ptr, rowpointers);

    /* Create an SDL surface from the pixel data */
    surface =
        SDL_CreateRGBSurfaceFrom(pixeldata, width, height, 32 /* bpp */ ,
                                 width * 4 /* rowbytes */ ,
                                 0x000000FFU, 0x0000FF00U, 0x00FF0000U,
                                 0xFF000000U /* masks */ );
    if (!surface) {
        fprintf(stderr, "Error creating SDL image surface: %s\n",
                SDL_GetError());
        goto cleanup_info_and_rowpointers;
    }

    rv = SDL_CreateTextureFromSurface(render, surface);
    if (!rv) {
        fprintf(stderr, "Error creating SDL image texture: %s\n",
                SDL_GetError());
    } else {
        *w = width;
        *h = height;
    }

    /* Cleanup label chain; this C idiom is one of the few legitimate uses of
       goto, because it doesn't have an appropriate control flow structure */
    SDL_FreeSurface(surface);
cleanup_info_and_rowpointers:
    if (pixeldata)
        free(pixeldata);
    if (rowpointers)
        free(rowpointers);
    png_ptr_nv = png_ptr;
    info_ptr_nv = info_ptr;
    png_destroy_read_struct(&png_ptr_nv, &info_ptr_nv, NULL);
    goto cleanup_fopen;
cleanup_png_ptr:
    png_ptr_nv = png_ptr;
    png_destroy_read_struct(&png_ptr_nv, NULL, NULL);
cleanup_fopen:
    fclose(in);
cleanup_nothing:
    return rv;
}


void
sdl_hook_beep(void)
{
    /* TODO */
}

void
sdl_hook_setcursorsize(int size)
{
    cursor_visible = size;
    sdl_hook_update(cursor_y, cursor_x);
}

void
sdl_hook_positioncursor(int y, int x)
{
    int old_x = cursor_x;
    int old_y = cursor_y;

    cursor_x = x;
    cursor_y = y;
    cursor_timestamp++;
    sdl_hook_update(cursor_y, cursor_x);
    sdl_hook_update(old_y, old_x);
}

/* Convert locations from pixels to character units. This mostly exists so that
   if the user clicks the mouse, we can figure out what they clicked on. */
static void
winloc_to_charloc(int x_pixels, int y_pixels, int *x_chars, int *y_chars)
{
    struct sdl_tile_region *region;

    *x_chars = x_pixels / fontwidth;
    *y_chars = y_pixels / fontheight;

    region = uncursed_rhook_region_at(*y_chars, *x_chars);
    if (!region) /* clicked on a character */
        return;

    /* The user clicked on a tile. Change x_pixels and y_pixels to be relative
       to the top left corner of the tiles region. */
    x_pixels += region->pixelshift_x - region->loc_l * fontwidth;
    y_pixels += region->pixelshift_y - region->loc_t * fontheight;

    /* Now work out which tile they clicked on, and convert to a character
       region location. */
    *x_chars = x_pixels / region->tilesize_w + region->loc_l;
    *y_chars = y_pixels / region->tilesize_h + region->loc_t;
}

/* Called whenever the window or font size changes.  hard_update should be true
   whenever the update requires a total overhaul of the screen, as with a font
   size change. */
static void
update_window_sizes(int hard_update)
{
    /* We set the window's minimum size to 80x24 times the font size. We don't
       otherwise try to control the size; it causes too many problems. */
    int w, h, worig, horig;

    SDL_GetWindowSize(win, &w, &h);

    worig = w;
    horig = h;
    w /= fontwidth;
    h /= fontheight;

    if (hard_update && font)
        SDL_SetWindowMinimumSize(win, MINCHARWIDTH * fontwidth,
                                 MINCHARHEIGHT * fontheight);

    if (screen)
        SDL_DestroyTexture(screen);
    screen = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888,
                               SDL_TEXTUREACCESS_TARGET, worig, horig);

#if 0
    /* Now commented out because this is effectively equivalent to the
       default, but gives slightly more bizarre render behaviour */
    SDL_RenderSetLogicalSize(render, worig, horig);
#endif

    SDL_SetRenderTarget(render, screen);
    rendertarget = screen;

    /* Render a message to tell the user what's going on. There are two reasons
       to do this: a) especially when using tiles regions, the redraw can be
       very slow; b) this paints the whole screen black, meaning that any
       fractional characters at the right and bottom of the screen will be black
       rather than storing the previous screen contents. */
    SDL_SetRenderDrawColor(render, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(render,
                       &(SDL_Rect) {.x = 0, .y = 0, .w = worig, .h = horig});
    static const char *const resizing = "Resizing, please wait...";
    const char *r = resizing;

    if (font) {
        while (*r) {
            SDL_SetTextureColorMod(font, 255, 255, 255);
            SDL_RenderCopy(render, font,
                           &(SDL_Rect) {      /* source */
                               .x = (*r % 16) * fontwidth,
                               .y = (*r / 16) * fontheight,
                               .w = fontwidth,
                               .h = fontheight},
                           &(SDL_Rect) {      /* destination */
                               .x = (r - resizing) * fontwidth + 5,
                               .y = 5,
                               .w = fontwidth,
                               .h = fontheight});
            r++;
        }
    }

    if (w != winwidth || h != winheight) {
        /* Don't do a full redraw if the window size actually changed from the
           view of the uncursed core; that might involve reading OoB. This
           relies on the application redrawing in response to KEY_RESIZE, but
           any sane application will. */
        resize_queued = 1;
    } else if (!resize_queued) {
        /* With no resize queued, we need to draw now. */
        sdl_hook_fullredraw();
        sdl_hook_flush();
    }

    winwidth = w;
    winheight = h;

    SDL_SetRenderTarget(render, NULL);
    rendertarget = NULL;
    SDL_RenderCopy(render, screen,
                   &(SDL_Rect) { .x = 0, .y = 0, .w = worig, .h = horig },
                   &(SDL_Rect) { .x = 0, .y = 0, .w = worig, .h = horig });
    SDL_RenderPresent(render);
    SDL_SetRenderTarget(render, screen);
    rendertarget = screen;
}

static void
exit_handler(void)
{
    SDL_RemoveTimer(timerid);

#ifdef ORDERLY_SHUTDOWN
    /* The frees of "font" and "render" each cause crashes on Windows. Because
       the process is about to exit anyway, just let them leak for now. */
    if (font) {
        SDL_DestroyTexture(font);
        font = NULL;
    }
    if (screen) {
        SDL_DestroyTexture(screen);
        screen = NULL;
    }
    if (render) {
        SDL_DestroyRenderer(render);
        render = NULL;
    }
    if (win) {
        SDL_DestroyWindow(win);
        SDL_Quit();
    }
    win = NULL;
#endif
}

void
sdl_hook_init(int *h, int *w, const char *title)
{
    if (!win) {
        debug = !!getenv("UNCURSED_SDL_DEBUG");

        sdl_hook_resetpalette16();

#ifndef AIMAKE_BUILDOS_MSWin32
        /* It's possible that the program using libuncursed will want to install
           signal handlers. However, SDL doesn't pay attention to this, and
           creates threads unprotected; and there's a chance that the signals
           will go to one of SDL's threads, rather than one of the user
           program's. Thus, while initializing SDL, we mark all new threads as
           being unable to receive signals. */
        sigset_t sigset, oldsigset;
        sigfillset(&sigset);
        pthread_sigmask(SIG_SETMASK, &sigset, &oldsigset);
#endif

        if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
            exit(EXIT_FAILURE);
        }

#ifndef AIMAKE_BUILDOS_MSWin32
        pthread_sigmask(SIG_SETMASK, &oldsigset, NULL);
#endif

        win = SDL_CreateWindow(title,   /* title */
                               SDL_WINDOWPOS_UNDEFINED, /* initial x */
                               SDL_WINDOWPOS_UNDEFINED, /* initial y */
                               fontwidth * winwidth,
                               fontheight * winheight,  /* size */
                               SDL_WINDOW_RESIZABLE);
        if (!win) {
            fprintf(stderr, "Error creating an SDL window: %s\n",
                    SDL_GetError());
            exit(EXIT_FAILURE);
        }

        atexit(exit_handler);

        /* Sadly, some things are impossible in SDL without polling; it has no
           async-signal-safe way to send events (it's thread-safe, but cannot
           handle an interupted malloc). However, SDL does a bunch of polling
           anyway, on a 10ms interval. Thus, there isn't a huge penalty to
           contributing to the polling mess. */
        FD_ZERO(&monitored_fds);
        monitored_fds_count_or_max = 0;
        timerid = SDL_AddTimer(10, timer_callback, NULL);

        winwidth = winheight = 0;

        int i, bestrender = -1, bestrenderscore = 8;

        /* Look for desirable renderer properties. We /must/ have the ability
           to target textures. It's nice to have hardware acceleration and
           vertical sync, too, but those are not mandatory. */
        debugprintf("Loading renderers...\n");
        for (i = 0; i < SDL_GetNumRenderDrivers(); i++) {
            SDL_RendererInfo ri;
            int score = 1;

            SDL_GetRenderDriverInfo(i, &ri);
            debugprintf("Renderer %d ('%s'): supports ", i, ri.name);

            if (ri.flags & SDL_RENDERER_TARGETTEXTURE) {
                debugprintf("target texture");
                score += 10;
            }
            if (ri.flags & SDL_RENDERER_ACCELERATED) {
                if (score > 1)
                    debugprintf(", ");
                debugprintf("hardware acceleration");
                score++;
            }
            if (ri.flags & SDL_RENDERER_PRESENTVSYNC) {
                if (score > 1)
                    debugprintf(", ");
                debugprintf("vertical sync");
                score++;
            }
            if (ri.flags & SDL_RENDERER_SOFTWARE) {
                if (score > 1)
                    debugprintf(", ");
                debugprintf("pure software implementation");
                if (getenv("UNCURSED_SDL_PREFER_SOFTWARE"))
                    score += 5;
            }
            debugprintf("\n");

            if (score > bestrenderscore) {
                bestrender = i;
                bestrenderscore = score;
            }
        }

        if (bestrender != -1) {
            debugprintf("Chose renderer %d\n", bestrender);
            render =
                SDL_CreateRenderer(win, bestrender,
                                   SDL_RENDERER_ACCELERATED |
                                   SDL_RENDERER_PRESENTVSYNC |
                                   SDL_RENDERER_TARGETTEXTURE);
            rendertarget = NULL;
        } else {
            fprintf(stderr,
                    "Could not find a renderer that targets textures\n");
            exit(EXIT_FAILURE);
        }

        if (!render) {
            fprintf(stderr, "Error creating an SDL renderer: %s\n",
                    SDL_GetError());
            exit(EXIT_FAILURE);
        }

        /* We want to be able to parse text as well as keypresses. If someone
           presses, say, ' then e, we want to be able to interpret it as
           "'e" or "é" depending on their input method. */
        SDL_StartTextInput();

        /* We can't do this until after we have a working renderer. */
        update_window_sizes(1);
        resize_queued = 0;

        *w = winwidth;
        *h = winheight;
    }
}

void
sdl_hook_exit(void)
{
    /* Actually tearing down the window (or worse, quitting SDL) would be
       overkill, given that this is used to allow raw writing to the console,
       and it's possible to do that behind the SDL window anyway. So do
       nothing; the atexit wil shut it down at actual system exit if the hook
       is called because the program is exiting, and otherwise we're going to
       have init called in the near future. (Perhaps we should hide the window,
       in case the code takes console input while the window's hidden, but even
       that would look weird.) */
}

static void
initialize_cursor_texture(struct sdl_tile_region *region)
{
    /* Fill the cursor texture with transparent pixels. */
    if (rendertarget != region->cursor)
        SDL_SetRenderTarget(render, region->cursor);
    rendertarget = region->cursor;

    SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(render, 0, 0, 0, 0);              /* transparent */
    SDL_RenderFillRect(render, &(SDL_Rect) {
            .x = 0,
            .y = 0,
            .w = region->texsize_w,
            .h = region->texsize_h,
        });
    SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_BLEND);
}

void
sdl_hook_set_faketerm_font_file(const char *filename)
{
    int w, h;
    SDL_Texture *t = load_png_file_to_texture(filename, &w, &h);

    if (t) {
        if (font)
            SDL_DestroyTexture(font);
        font = t;
        /* Fonts are 16x16 grids. */
        fontwidth = w / 16;
        fontheight = h / 16;
        update_window_sizes(1);
        sdl_hook_fullredraw();  /* draw with the new font */
        cursor_timestamp++;
    }
}

void
sdl_hook_set_tiles_tile_file(const char *filename, int height, int width)
{
    if (tileset_filename)
        free(tileset_filename);
    tileset_filename = NULL;
    if (!filename)
        return;
    /* An allocation failure here can't be handled, and happens to do exactly
       what we want anyway. */
    tileset_filename = strdup(filename);
    tileset_height = height;
    tileset_width = width;
}

void
sdl_hook_get_tile_dimensions(int down, int across, int *height, int *width)
{
    int w, h;
    if (!tileset_filename) {
        *height = down;
        *width = across;
        return;
    }
    w = tileset_width;
    h = tileset_height;
    /* now w/h are the dimensions of a single tile, in pixels */
    w *= across;
    h *= down;
    /* now w/h are the dimensions of the tiles region, in pixels */
    w = (w + fontwidth - 1) / fontwidth;
    h = (h + fontheight - 1) / fontheight;
    /* now w/h are the dimensions of the tiles region, in characters */
    *height = h;
    *width = w;
}

void *
sdl_hook_allocate_tiles_region(int height, int width, int loc_h, int loc_w,
                               int loc_t, int loc_l)
{
    /* TODO: We could gain some performance by refcounting tilesets. This comes
       up rarely enough in practice that it's not a high priority to implement.
     */
    struct sdl_tile_region *region;

    region = malloc(sizeof *region);
    if (!region)
        return NULL;

    region->tiles = calloc(height * width, sizeof (int));
    if (!region->tiles) {
        free(region);
        return NULL;
    }

    region->tileset =
        load_png_file_to_texture(tileset_filename, &region->tilesize_w,
                                 &region->tilesize_h);
    if (!region->tileset) {
        free(region->tiles);
        free(region);
        return NULL;
    }

    region->tileset_cols = region->tilesize_w / tileset_width;
    region->tileset_rows = region->tilesize_h / tileset_height;
    region->tilesize_w = tileset_width;
    region->tilesize_h = tileset_height;
    region->texsize_w = tileset_width * width;
    region->texsize_h = tileset_height * height;

    SDL_SetRenderTarget(render, screen);
    rendertarget = screen;

    region->texture =
        SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_TARGET, region->texsize_w,
                          region->texsize_h);
    if (!region->texture) {
        SDL_DestroyTexture(region->tileset);
        free(region->tiles);
        free(region);
        return NULL;
    }

    region->cursor =
        SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_TARGET, region->texsize_w,
                          region->texsize_h);
    if (!region->cursor) {
        SDL_DestroyTexture(region->texture);
        SDL_DestroyTexture(region->tileset);
        free(region->tiles);
        free(region);
        return NULL;
    }
    SDL_SetTextureBlendMode(region->cursor, SDL_BLENDMODE_BLEND);

    region->loc_w = loc_w;
    region->loc_h = loc_h;
    region->loc_l = loc_l;
    region->loc_t = loc_t;
    region->pixelshift_x = region->pixelshift_y = 0;
    region->cursortile_x = region->cursortile_y = -1;
    region->tilecount_w = width;
    region->tilecount_h = height;
    region->pixelshift_timestamp = cursor_timestamp - 1;
    region->dirty = 1;

    initialize_cursor_texture(region);

    return region;
}

void
sdl_hook_deallocate_tiles_region(void *region)
{
    SDL_DestroyTexture(((struct sdl_tile_region *)region)->texture);
    SDL_DestroyTexture(((struct sdl_tile_region *)region)->cursor);
    SDL_DestroyTexture(((struct sdl_tile_region *)region)->tileset);
    free(((struct sdl_tile_region *)region)->tiles);
    free(region);
}

void
sdl_hook_draw_tile_at(int tile, void *r, int y, int x)
{
    struct sdl_tile_region *region = r;

    if (x < 0 || y < 0 || x >= region->tilecount_w || y >= region->tilecount_h)
        return;
    if (tile == region->tiles[y * region->tilecount_w + x])
        return;

    if (rendertarget != region->texture)
        SDL_SetRenderTarget(render, region->texture);
    rendertarget = region->texture;

    SDL_RenderCopy(render, region->tileset,
                   &(SDL_Rect) {       /* source */
                       .x = (tile % region->tileset_cols) * region->tilesize_w,
                       .y = (tile / region->tileset_cols) * region->tilesize_h,
                       .w = region->tilesize_w,
                       .h = region->tilesize_h },
                   &(SDL_Rect) {       /* destination */
                       .x = x * region->tilesize_w,
                       .y = y * region->tilesize_h,
                       .w = region->tilesize_w,
                       .h = region->tilesize_h});

    region->dirty = 1;
    region->tiles[y * region->tilecount_w + x] = tile;
}

void
sdl_hook_rawsignals(int raw)
{
    /* meaningless to this plugin */
}

void
sdl_hook_activatemouse(int active)
{
    mouse_active = active;
}

static volatile sig_atomic_t in_getkeyorcodepoint = 0;
static volatile sig_atomic_t getch_signal_count = 0;

static void
drain_userevents(void)
{
    SDL_Event event_buffer;

    in_getkeyorcodepoint = 0;

    while (SDL_PeepEvents(&event_buffer, 1, SDL_GETEVENT,
                          SDL_USEREVENT, SDL_USEREVENT) != 0) {
        if (event_buffer.user.code == KEY_SIGNAL)
            getch_signal_count++;
        /* drain KEY_OTHERFD events */
    }
}

#define TEXTEDITING_FILTER_TIME 2       /* milliseconds */

int
sdl_hook_getkeyorcodepoint(int timeout_ms)
{
    /* Screen redrawing causes major lag (TODO: figure out why).
       To avoid snowballing lag resulting by queueing up
       WINDOWEVENT_EXPOSED (what causes redraws) cases,
       only redraw once for every X events queued up. */
    int redraw = 0;
    int ret = sdl_hook_getkeyorcodepointx(timeout_ms, &redraw);
    if (redraw)
        sdl_hook_fullredraw();

    return ret;
}

static int
sdl_hook_getkeyorcodepointx(int timeout_ms, int *redraw)
{
    long tick_target = SDL_GetTicks() + timeout_ms;
    long key_tick_target = -1;
    long last_textediting_tick = -1 - TEXTEDITING_FILTER_TIME;
    int kc = KEY_INVALID + KEY_BIAS, i, j, k, x, y;

    if (hangup_mode)
        return KEY_HANGUP + KEY_BIAS;

    *redraw = 0;
    in_getkeyorcodepoint = 1;

    do {
        SDL_Event e;
        long ticks_left = tick_target - SDL_GetTicks();

        if (resize_queued) {
            update_window_sizes(0);
            resize_queued = 0;
            uncursed_rhook_setsize(winheight, winwidth);
            drain_userevents();
            return KEY_RESIZE + KEY_BIAS;
        }

        if (resized_recently && (timeout_ms == -1 || ticks_left > 150))
            ticks_left = 150;
        else if (timeout_ms == -1)
            ticks_left = -1;

        if ((key_tick_target != -1 ?
             SDL_WaitEventTimeout(&e, key_tick_target - SDL_GetTicks()) :
             ticks_left == -1 ? SDL_WaitEvent(&e) :
             SDL_WaitEventTimeout(&e, ticks_left)) == 0) {

            /* WaitEventTimeout returns 0 on timeout expiry; both functions
               return 0 on error. */

            if (resized_recently) {
                resized_recently = 0;
                update_window_sizes(0);
                continue;
            } else {
                drain_userevents();
                if (key_tick_target != -1)
                    return kc;
                return KEY_SILENCE + KEY_BIAS;
            }
        }

        switch (e.type) {
        case SDL_USEREVENT:

            /* We got a key from a different thread. */
            drain_userevents();
            return e.user.code + KEY_BIAS;

        case SDL_WINDOWEVENT:

            /* The events we're interested in here are closing the window, and
               resizing the window. We also need to redraw, if the window
               manager requests that.

               We have a problem implementing this, in that on some window
               managers, there are no resize signals sent during the resize, and
               then a huge number are sent all at once, but added to the SDL
               queue one at a time (so we can't just look ahead in the
               queue). So we set a small timeout, and handle the resize once the
               timeout is up. The timeout is implemented by this function, using
               the resized_recently global. */

            if (e.window.event == SDL_WINDOWEVENT_RESIZED ||
                e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                e.window.event == SDL_WINDOWEVENT_MAXIMIZED ||
                e.window.event == SDL_WINDOWEVENT_RESTORED)
                resized_recently = 1;

            if (e.window.event == SDL_WINDOWEVENT_EXPOSED)
                *redraw = 1;

            if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                hangup_mode = 1;
                drain_userevents();
                return KEY_HANGUP + KEY_BIAS;
            }

            break;

        case SDL_TEXTEDITING:

            key_tick_target = -1;
            last_textediting_tick = SDL_GetTicks();
            break;

        case SDL_TEXTINPUT:

            /* The user pressed a key that's interpreted as a printable.
               Convert it from UTF-8 to UTF-32. */
            k = 0;

            drain_userevents();

#define ett ((unsigned char)*e.text.text)
            if (ett < 0x80) {
                k = ett & 0x7F;
                i = 0;
            } else if (ett < 0xD0) {
                k = ett & 0x1F;
                i = 1;
            } else if (ett < 0xF0) {
                k = ett & 0x0F;
                i = 2;
            } else if (ett < 0xF8) {
                k = ett & 0x07;
                i = 3;
            }
            else
                return KEY_INVALID + KEY_BIAS;
#undef ett

            k <<= i * 6;
            for (j = 1; i > 0; i--) {
                k += (((unsigned char)e.text.text[i]) & 0x3F) * j;
                j <<= 6;
            }
            if (k > 0x10ffff) {
                return KEY_INVALID + KEY_BIAS;
            }

            /* Hack for X11 (i.e. most practical uses of this on Linux as of
               2013): If the user presses Alt + an ASCII printable, prefer to
               send the key combination (Alt+letter), rather than the Unicode
               key it produces (just the letter by itself). */
            if (k >= ' ' && k <= '~' && kc == (k | KEY_ALT) + KEY_BIAS)
                return kc;

            return k;

        case SDL_MOUSEMOTION:
            winloc_to_charloc(e.motion.x, e.motion.y, &x, &y);
            k = uncursed_rhook_mousekey_from_pos(y, x, uncursed_mbutton_hover);

            if (k == -1 && mouse_hovering) {

                mouse_hovering = 0;
                if (mouse_active) {
                    drain_userevents();
                    return KEY_UNHOVER + KEY_BIAS;
                }

            } else if (k != -1 && mouse_active &&
                       (!mouse_hovering ||
                        mouse_hover_x != x || mouse_hover_y != y)) {

                /* Don't repeatedly send hover events if the cursor keeps
                   moving but does not move to a different character. */
                mouse_hovering = 1;
                mouse_hover_x = x;
                mouse_hover_y = y;
                drain_userevents();
                return k;

            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (!mouse_active)
                break;

            if (e.button.button == SDL_BUTTON_LEFT)
                i = uncursed_mbutton_left;
            else if (e.button.button == SDL_BUTTON_MIDDLE)
                i = uncursed_mbutton_middle;
            else if (e.button.button == SDL_BUTTON_RIGHT)
                i = uncursed_mbutton_right;
            else break;

            winloc_to_charloc(e.button.x, e.button.y, &x, &y);

            k = uncursed_rhook_mousekey_from_pos(y, x, i);
            if (k != -1) {
                drain_userevents();
                return k;
            }

            break;

        case SDL_MOUSEWHEEL:
            if (!mouse_active)
                break;

            SDL_GetMouseState(&x, &y);
            winloc_to_charloc(x, y, &x, &y);

            /* TODO: Factor in how fast the wheel is being moved? */
            k = uncursed_rhook_mousekey_from_pos(
                y, x, e.wheel.y < 0 ? uncursed_mbutton_wheeldown :
                uncursed_mbutton_wheelup);
            if (k != -1) {
                drain_userevents();
                return k;
            }

            break;

        case SDL_KEYDOWN:

            /* We care about this if it's a function key (i.e. not
               corresponding to text), but not if it's used as part of an input
               method. The heuristic used is to try to handle the key itself if
               it didn't cause an SDL_TEXTEDITING or SDL_TEXTINPUT within 2 ms.

               First, though, in case that event doesn't happen, we try to
               calculate a code for the key itself, so that we can return it 2
               ms later, except in the easy case where there was a text
               editing/input event within the /previous/ 2ms. */
            if ((long)SDL_GetTicks() <
                last_textediting_tick + TEXTEDITING_FILTER_TIME) {
                break;
            }

            kc = 0;
#define K(x,y) case SDLK_##x: kc = KEY_BIAS + KEY_##y; break
#define KK(x) K(x,x)
            switch (e.key.keysym.sym) {

                /* nonprintables in SDL that correspond to control codes */
            case SDLK_RETURN:
                kc = '\x0d';
                break;
            case SDLK_TAB:
                kc = '\x09';
                break;

                /* nonprintables that exist in both SDL and uncursed */
                KK(F1); KK(F2); KK(F3); KK(F4); KK(F5); KK(F6); KK(F7); KK(F8);
                KK(F9); KK(F10); KK(F11); KK(F12); KK(F13); KK(F14); KK(F15);
                KK(F16); KK(F17); KK(F18); KK(F19); KK(F20);

                KK(ESCAPE); KK(BACKSPACE);
                K(PRINTSCREEN, PRINT); K(PAUSE, BREAK);
                KK(HOME); KK(END); K(INSERT, IC); K(DELETE, DC);
                K(PAGEUP, PPAGE); K(PAGEDOWN, NPAGE);
                KK(RIGHT); KK(LEFT); KK(UP); KK(DOWN);

                K(KP_DIVIDE, NUMDIVIDE); K(KP_MULTIPLY, NUMTIMES);
                K(KP_MINUS, NUMMINUS); K(KP_PLUS, NUMPLUS);
                K(KP_ENTER, ENTER);
                K(KP_1, C1); K(KP_2, C2); K(KP_3, C3);
                K(KP_4, B1); K(KP_5, B2); K(KP_6, B3);
                K(KP_7, A1); K(KP_8, A2); K(KP_9, A3);
                K(KP_0, D1); K(KP_PERIOD, D3);

                /* we intentionally ignore modifier keys */
            case SDLK_CAPSLOCK:
            case SDLK_SCROLLLOCK:
            case SDLK_NUMLOCKCLEAR:
            case SDLK_LCTRL:
            case SDLK_LSHIFT:
            case SDLK_LALT:
            case SDLK_RCTRL:
            case SDLK_RSHIFT:
            case SDLK_RALT:
            case SDLK_MODE:
            case SDLK_LGUI:
            case SDLK_RGUI:
                break;

            default:
                /* Other keys are either printables, or else keys that uncursed
                   doesn't know about. If they're printables, we just store
                   them as is in kc. Otherwise, we synthesize a number for them
                   via masking off SLK_SCANCODE_MASK and adding
                   KEY_LAST_FUNCTION. If that goes to 512 or higher, we give
                   up. */
                if (e.key.keysym.sym >= ' ' && e.key.keysym.sym <= '~')
                    kc = e.key.keysym.sym;
                else if ((e.key.keysym.sym & ~SDLK_SCANCODE_MASK) +
                         KEY_LAST_FUNCTION < 512)
                    kc = (e.key.keysym.sym & ~SDLK_SCANCODE_MASK) +
                        KEY_LAST_FUNCTION + KEY_BIAS;
                break;
            }
#undef KK
#undef K

            if (kc) {
                if (kc >= KEY_BIAS) {
                    if (e.key.keysym.mod & KMOD_ALT)
                        kc |= KEY_ALT;
                    if (e.key.keysym.mod & KMOD_CTRL)
                        kc |= KEY_CTRL;
                    if (e.key.keysym.mod & KMOD_SHIFT)
                        kc |= KEY_SHIFT;

                } else {
                    if (e.key.keysym.mod & KMOD_CTRL)
                        kc &= ~96;
                    else if (e.key.keysym.mod & KMOD_SHIFT) {
                        if (kc >= 'a' && kc <= 'z')
                            kc -= 'a' - 'A';
                    }
                }

                if (kc <= 127 && e.key.keysym.mod & KMOD_ALT) {
                    kc |= KEY_ALT;
                    kc += KEY_BIAS;
                }

                key_tick_target = SDL_GetTicks() + TEXTEDITING_FILTER_TIME;
            }
            break;

        case SDL_QUIT:
            hangup_mode = 1;
            drain_userevents();
            return KEY_HANGUP + KEY_BIAS;
        }

    } while (timeout_ms == -1 || SDL_GetTicks() < tick_target);

    drain_userevents();
    return KEY_SILENCE + KEY_BIAS;
}

static void
signal_event_loop(int key)
{
    /* Send a user message that will be picked up by the SDL event loop. */
    SDL_UserEvent ue;
    ue.type = SDL_USEREVENT;
    ue.timestamp = SDL_GetTicks();
    ue.windowID = 0;
    ue.code = key;
    SDL_PushEvent(&(union SDL_Event){.user = ue});
}


void
sdl_hook_delay(int ms)
{
    /* We want to discard keys for the given length of time. (If the window is
       resized during the delay, we keep quiet about the resize until the next
       key request, because otherwise the application wouldn't learn about it
       and might try to draw out of bounds. On a hangup, we end the delay
       early.)

       TODO: handle SDL_GetTicks overflow */
    long tick_target = SDL_GetTicks() + ms;

    while (SDL_GetTicks() < tick_target) {
        int k = sdl_hook_getkeyorcodepoint(tick_target - SDL_GetTicks());

        if (k == KEY_HANGUP)
            break;
        if (k == KEY_SIGNAL || k == KEY_OTHERFD) {
            signal_event_loop(k);
            break;
        }
    }
}

static Uint32
timer_callback(Uint32 interval, void *unused) {
    fd_set monitored_fds_copy;
    struct timeval zerotime = {0, 0};
    SDL_Event event_buffer;

    (void) unused;

    while (getch_signal_count) {
        getch_signal_count--;
        signal_event_loop(KEY_SIGNAL);
    }

    /* Add KEY_OTHERFD to the queue only if we don't have a user event pending
       already; we want to avoid spamming the queue with OTHERFD faster than the
       user program can handle it; also only add it if we're inside
       getkeyorcodepoint right now, in case the FD becomes unwritable in
       between */
    if (SDL_PeepEvents(&event_buffer, 1, SDL_PEEKEVENT,
                       SDL_USEREVENT, SDL_USEREVENT) == 0 &&
        in_getkeyorcodepoint) {
        memcpy(&monitored_fds_copy, &monitored_fds, sizeof monitored_fds);
        if (monitored_fds_count_or_max &&
            select(monitored_fds_count_or_max, &monitored_fds_copy, 0, 0,
                   &zerotime) > 0)
            signal_event_loop(KEY_OTHERFD);
    }

    return interval;
}

void
sdl_hook_signal_getch(void)
{
    getch_signal_count++;
}

void
sdl_hook_watch_fd(int fd, int watch)
{
    /* In a typical case of nonportability, select() has apparently the same
       signature on Windows and Linux/UNIX, but the arguments mean different
       things. In particular, monitored_fds_count_or_max needs to be the number
       of FDs on Windows, or the highest number among the FDs plus 1 on
       Linux/UNIX. */

#ifdef AIMAKE_BUILDOS_MSWin32
    if (watch && !FD_ISSET(fd, &monitored_fds))
        monitored_fds_count_or_max++;
    if (!watch && FD_ISSET(fd, &monitored_fds))
        monitored_fds_count_or_max--;
#else
    if (fd >= FD_SETSIZE)
        abort();
    if (fd >= monitored_fds_count_or_max)
        monitored_fds_count_or_max = fd + 1;
#endif
    if (watch)
        FD_SET(fd, &monitored_fds);
    else
        FD_CLR(fd, &monitored_fds);
}


typedef Uint8 Color[3];

/* The builtin palette used at startup and after reset */
static Color builtin_palette[16] = UNCURSED_DEFAULT_COLORS;

/* Storage area for the custom palette */
static Color custom_palette[16];

/* pointer to the current palette */
static Color * palette = builtin_palette;

void
sdl_hook_setpalette16( const uncursed_palette16 *p )
{
    int i;
    for (i=0;i<16;i++)
    {
        custom_palette[i][0] = (Uint8) p->color[i][0];
        custom_palette[i][1] = (Uint8) p->color[i][1];
        custom_palette[i][2] = (Uint8) p->color[i][2];
    }
    palette = custom_palette;
}

void
sdl_hook_resetpalette16(void)
{
    palette = builtin_palette;
}



/* Redraws an entire region. This is necessary if the cursor has moved, or if
   any tile in the region has changed. Returns 1 if the region had to be
   updated, or 0 if no updates were required. */
static int
update_region(struct sdl_tile_region *r)
{
    if (r->pixelshift_timestamp != cursor_timestamp && cursor_visible) {
        /* Recalculate the pixel shift. We want the relative position of the
           cursor within the region to be the same as the relative position of
           the region within its location. */
        r->pixelshift_timestamp = cursor_timestamp;

        int nctx = -1;
        int ncty = -1;

        if (cursor_x >= r->loc_l && cursor_x < r->loc_l + r->loc_w &&
            cursor_y >= r->loc_t && cursor_y < r->loc_t + r->loc_h &&
            uncursed_rhook_region_at(cursor_y, cursor_x) == r) {
            nctx = cursor_x - r->loc_l;
            ncty = cursor_y - r->loc_t;

            /* Graphics rendering is the main "legitimate" use of floats,
               because nobody cares if it isn't 100% accurate. */
            float xprop = 1.0f;
            float yprop = 1.0f;

            if (cursor_x - r->loc_l < r->loc_w - 1)
                xprop = (float)nctx / (float)(r->tilecount_w - 1);
            if (cursor_y - r->loc_t < r->loc_h - 1)
                yprop = (float)ncty / (float)(r->tilecount_h - 1);

            /* If it fits entirely within the location, centre it. */
            int locsize_w = r->loc_w * fontwidth;
            int locsize_h = r->loc_h * fontheight;

            if (locsize_w > r->texsize_w)
                xprop = 0.5;
            if (locsize_h > r->texsize_h)
                yprop = 0.5;

            int pixelshift_x = (r->texsize_w - locsize_w) * xprop;
            int pixelshift_y = (r->texsize_h - locsize_h) * yprop;

            if (pixelshift_x != r->pixelshift_x ||
                pixelshift_y != r->pixelshift_y)
                r->dirty = 1;
            r->pixelshift_x = pixelshift_x;
            r->pixelshift_y = pixelshift_y;
        }

        if (nctx != r->cursortile_x || ncty != r->cursortile_y) {

            /* Remove any existing cursor, if necessary. */
            if (r->cursortile_x > -1)
                initialize_cursor_texture(r);

            if (nctx > -1) {
                /* Draw a new cursor. */
                if (rendertarget != r->cursor)
                    SDL_SetRenderTarget(render, r->cursor);
                rendertarget = r->cursor;

                SDL_SetRenderDrawColor(render, 255, 255, 255, SDL_ALPHA_OPAQUE);
                SDL_RenderDrawRect(render,
                                   &(SDL_Rect) {
                                       .x = nctx * r->tilesize_w,
                                       .y = ncty * r->tilesize_h,
                                       .w = r->tilesize_w,
                                       .h = r->tilesize_h});
            }
            r->cursortile_x = nctx;
            r->cursortile_y = ncty;
            r->dirty = 1;
        }
    }

    if (r->dirty) {
        /* Draw the entire screen at once, to save on drawing lots of
           individual tiles. */
        SDL_SetRenderTarget(render, screen);
        rendertarget = screen;
        int lf = r->pixelshift_x;
        int tf = r->pixelshift_y;
        int lt = r->loc_l * fontwidth;
        int tt = r->loc_t * fontheight;
        int w = r->loc_w * fontwidth;
        int h = r->loc_h * fontheight;

        SDL_SetRenderDrawColor(render,
                               palette[0][0], palette[0][1], palette[0][2],
                               SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(render,
                           &(SDL_Rect) {.x = lt, .y = tt, .w = w, .h = h});
        if (lf < 0) {
            w -= -lf;
            lt += -lf;
            lf = 0;
        }
        if (tf < 0) {
            h -= -tf;
            tt += -tf;
            tf = 0;
        }

        if (lf + w > r->texsize_w)
            w -= (lf + w - r->texsize_w);
        if (tf + h > r->texsize_h)
            h -= (tf + h - r->texsize_h);

        if (w > 0 && h > 0) {


            SDL_RenderCopy(render, r->texture,
                           &(SDL_Rect) {.x = lf, .y = tf, .w = w, .h = h},
                           &(SDL_Rect) {.x = lt, .y = tt, .w = w, .h = h});
            SDL_RenderCopy(render, r->cursor,
                           &(SDL_Rect) {.x = lf, .y = tf, .w = w, .h = h},
                           &(SDL_Rect) {.x = lt, .y = tt, .w = w, .h = h});
        }

        /* Now draw any cells that contain something other than tiles. */
        int i, j;

        r->dirty = 0;
        for (i = 0; i < r->loc_w; i++)
            for (j = 0; j < r->loc_h; j++)
                update_cell(j + r->loc_t, i + r->loc_l, r);

        return 1;

    } else
        return 0;
}

static void
update_cell(int y, int x, struct sdl_tile_region *current_region)
{
    unsigned char ch = uncursed_rhook_cp437_at(y, x);
    int a = uncursed_rhook_color_at(y, x);
    struct sdl_tile_region *region = uncursed_rhook_region_at(y, x);

    if (region && region == current_region) {
        uncursed_rhook_updated(y, x);
        return;
    }

    if (region && !current_region && update_region(region))
        return;

    Uint8 *fgcolor = palette[a & 15];
    Uint8 *bgcolor = palette[(a >> 5) & 15];

    if (a & 16)
        fgcolor = palette[7];
    if (a & 512)
        bgcolor = palette[0];

    if (rendertarget != screen)
        SDL_SetRenderTarget(render, screen);
    rendertarget = screen;

    /* Draw the background. */
    if (!region) {
        SDL_SetRenderDrawColor(render, bgcolor[0], bgcolor[1], bgcolor[2],
                               SDL_ALPHA_OPAQUE);
    } else {
        SDL_SetRenderDrawColor(render, 0, 0, 0, SDL_ALPHA_OPAQUE);
    }
    SDL_RenderFillRect(render,
                       &(SDL_Rect) {
                           .x = x * fontwidth,
                           .y = y * fontheight,
                           .w = fontwidth,
                           .h = fontheight});

    /* Draw the foreground. */
    if (region) {
        /* We're drawing part of the tiles, but we're copying a character-sized
           block. If it's OoB, we don't want to copy that part of the block,
           instead letting the black background show through. */
        int lf = (x - region->loc_l) * fontwidth + region->pixelshift_x;
        int tf = (y - region->loc_t) * fontheight + region->pixelshift_y;
        int lt = x * fontwidth;
        int tt = y * fontheight;
        int w = fontwidth;
        int h = fontheight;

        if (lf < 0) {
            w -= -lf;
            lt += -lf;
            lf = 0;
        }
        if (tf < 0) {
            h -= -tf;
            tt += -tf;
            tf = 0;
        }

        if (lf + w > region->texsize_w)
            w -= (lf + w - region->texsize_w);
        if (tf + h > region->texsize_h)
            h -= (tf + h - region->texsize_h);

        if (w > 0 && h > 0) {
            SDL_RenderCopy(render, region->texture,
                           &(SDL_Rect) {.x = lf, .y = tf, .w = w, .h = h},
                           &(SDL_Rect) {.x = lt, .y = tt, .w = w, .h = h});
            SDL_RenderCopy(render, region->cursor,
                           &(SDL_Rect) {.x = lf, .y = tf, .w = w, .h = h},
                           &(SDL_Rect) {.x = lt, .y = tt, .w = w, .h = h});
        }
    } else if (!font) {
        /* Just draw blocks of color. */
        SDL_SetRenderDrawColor(render, fgcolor[0], fgcolor[1], fgcolor[2],
                               SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(render,
                           &(SDL_Rect) {
                               .x = x * fontwidth + 2,
                               .y = y * fontheight + 2,
                               .w = fontwidth - 4,
                               .h = fontheight - 4});
    } else {
        /* Blit from the font onto the screen. */
        SDL_SetTextureColorMod(font, fgcolor[0], fgcolor[1], fgcolor[2]);
        SDL_RenderCopy(render, font,
                       &(SDL_Rect) {      /* source */
                           .x = (ch % 16) * fontwidth,
                           .y = (ch / 16) * fontheight,
                           .w = fontwidth,
                           .h = fontheight},
                       &(SDL_Rect) {      /* destination */
                           .x = x * fontwidth,
                           .y = y * fontheight,
                           .w = fontwidth,
                           .h = fontheight});
    }

    /* Draw underlining if necessary. */
    if (a & 1024 && !region) {
        SDL_SetRenderDrawColor(render, fgcolor[0], fgcolor[1], fgcolor[2],
                               SDL_ALPHA_OPAQUE);
        SDL_RenderDrawLine(render, x * fontwidth, (y + 1) * fontheight - 2,
                           (x + 1) * fontwidth - 1, (y + 1) * fontheight - 2);
    }

    /* Draw a cursor, if necessary. */
    if (!region && x == cursor_x && y == cursor_y && cursor_visible) {

        SDL_SetRenderDrawColor(render, fgcolor[0], fgcolor[1], fgcolor[2],
                               SDL_ALPHA_OPAQUE);
        SDL_RenderDrawRect(render,
                           &(SDL_Rect) {
                               .x = x * fontwidth,
                               .y = y * fontheight,
                               .w = fontwidth,
                               .h = fontheight});
    }

    uncursed_rhook_updated(y, x);
}

void
sdl_hook_update(int y, int x)
{
    update_cell(y, x, NULL);
}

void
sdl_hook_fullredraw(void)
{
    int i, j;

    if (rendertarget != screen)
        SDL_SetRenderTarget(render, screen);
    rendertarget = screen;

    SDL_SetRenderDrawColor(render,
                           palette[0][0],palette[0][1],palette[0][2],
                           SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(render,
                       &(SDL_Rect) {
                           .x = 0,
                           .y = 0,
                           .w = fontwidth * winwidth,
                           .h = fontheight * winheight
                       });

    for (j = 0; j < winheight; j++)
        for (i = 0; i < winwidth; i++)
            sdl_hook_update(j, i);
    sdl_hook_flush();
}

void
sdl_hook_flush(void)
{
    if (rendertarget != NULL)
        SDL_SetRenderTarget(render, NULL);
    rendertarget = NULL;
    SDL_RenderCopy(render, screen,
                   &(SDL_Rect) {      /* source */
                       .x = 0,
                       .y = 0,
                       .w = fontwidth * winwidth,
                       .h = fontheight * winheight},
                   &(SDL_Rect) {      /* destination */
                       .x = 0,
                       .y = 0,
                       .w = fontwidth * winwidth,
                       .h = fontheight * winheight});
    SDL_RenderPresent(render);
}
