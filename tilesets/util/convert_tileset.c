/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-01-12 */
/*   Copyright (c) J. Ali Harlow 2000                               */
/*   NetHack may be freely redistributed.  See license for details. */

/* Given a map file and one or more .txt tile files, produces a .txt tile file
   containing only those tiles in the map file, in the same order. If one of the
   required tiles is found in multiple tilesets, the last one listed on the
   command line will be used. If one of the required tiles is not found at all,
   a warning will be given and a placeholder tile used as a substitute.

   This routine is intended to be able to convert tiles from other formats, so
   it has more leniency about the tile format than normal. In particular, it
   allows for unusual background colors for tiles (which will be converted to
   the default background), multiple tiles with the same name in the same
   file (which will be disambiguated using "s" and a number), and upscaling
   tiles from smaller resolutions. */

#include "tile.h"

struct tile {
    char *name;
    pixel *bitmap;
    unsigned char ph, scaled;
};

struct tilename {
    char *name;
    int nth; /* -1 = not duplicated, 0+ = duplicated */
};

static struct tile *tiles = NULL;
static int no_tiles = 0;
static int alloc_tiles = 0;

static struct tilename *tilenames = NULL;
static int no_tilenames = 0;
static int alloc_tilenames = 0;

static pixel file_bg = DEFAULT_BACKGROUND;
static int target_tile_x = 0;
static int target_tile_y = 0;

static void
set_background(pixel (*bitmap)[MAX_TILE_X])
{
    int x, y;
    const pixel bg = DEFAULT_BACKGROUND;

    if (file_bg.r == bg.r && file_bg.g == bg.g && file_bg.b == bg.b)
        return;
    for (y = 0; y < tile_y; y++)
        for (x = 0; x < tile_x; x++)
            if (bitmap[y][x].r == file_bg.r && bitmap[y][x].g == file_bg.g &&
                bitmap[y][x].b == file_bg.b) {
                bitmap[y][x].r = bg.r;
                bitmap[y][x].g = bg.g;
                bitmap[y][x].b = bg.b;
            }
}

static void
read_tilenames(void)
{
    char ttype[TILEBUFSZ];
    int number;
    char name[TILEBUFSZ];
    pixel tile[MAX_TILE_Y][MAX_TILE_X];
    int i, nth;

    while (no_tilenames--)
        free(tilenames[no_tilenames].name);
    no_tilenames = 0;

    while (read_text_tile_info(tile, ttype, &number, name)) {
        if (no_tilenames == alloc_tilenames) {
            if (alloc_tilenames)
                alloc_tilenames *= 2;
            else
                alloc_tilenames = 1024;
            tilenames = realloc(tilenames,
                                alloc_tilenames * sizeof (*tilenames));
            if (!tilenames) {
                Fprintf(stderr, "Not enough memory\n");
                exit(EXIT_FAILURE);
            }
        }
        tilenames[no_tilenames].nth = -1;
        tilenames[no_tilenames].name = strdup(name);
        if (!tilenames[no_tilenames].name) {
            Fprintf(stderr, "Not enough memory\n");
            exit(EXIT_FAILURE);
        }
        nth = 0;
        /* check for duplicates, and record nth appropriately */
        for (i = 0; i < no_tilenames; i++) {
            if (!strcmp(tilenames[i].name, name)) {
                tilenames[i].nth = nth++;
            }
        }
        if (nth > 0) tilenames[i].nth = nth;
        no_tilenames++;
    }
}

static void
read_mapfile(char *mapfile)
{
    FILE *mapfp = fopen(mapfile, "r");
    if (!mapfp) {
        perror(mapfile);
        exit(EXIT_FAILURE);
    }

    char name[TILEBUFSZ];
    for (;;) {
        int c, i = 0;
        unsigned j;
        c = getc(mapfp);
        while (c != EOF && c != '\n' && i < TILEBUFSZ-1) {
            name[i++] = c;
            c = getc(mapfp);
        }
        if (c == EOF && !i) break;
        if (!i) {
            Fprintf(stderr, "Unexpected blank line in map file\n");
            exit(EXIT_FAILURE);
        }
        name[i] = 0;

        if (*name == '!')
            continue;

        if (no_tiles == alloc_tiles) {
            if (alloc_tiles)
                alloc_tiles *= 2;
            else
                alloc_tiles = 1024;
            if (!tiles)
                tiles = malloc(alloc_tiles * sizeof (*tiles));
            else
                tiles = realloc(tiles, alloc_tiles * sizeof (*tiles));
            if (!tiles) {
                Fprintf(stderr, "Not enough memory\n");
                exit(EXIT_FAILURE);
            }
            for (i = no_tiles; i < alloc_tiles; i++) {
                tiles[i].bitmap = malloc(target_tile_x * target_tile_y *
                                         sizeof (pixel));
                if (!tiles[i].bitmap) {
                    Fprintf(stderr, "Not enough memory\n");
                    exit(EXIT_FAILURE);
                }
                for (j = 0; j < target_tile_x * target_tile_y; j++) {
                    /* draw a placeholder image */
                    tiles[i].bitmap[j].r = 255;
                    tiles[i].bitmap[j].g = 255;
                    tiles[i].bitmap[j].b = 255;
                }
            }
        }
        tiles[no_tiles].ph = 1;
        tiles[no_tiles].scaled = 0;
        tiles[no_tiles].name = strdup(name);
        if (!tiles[no_tiles].name) {
            Fprintf(stderr, "Not enough memory\n");
            exit(EXIT_FAILURE);
        }
        for (i = 0; i < no_tiles; i++) {
            if (!strcmp(tiles[i].name, name)) {
                Fprintf(stderr,
                        "Duplicate name '%s' in tilemap: lines %d, %d\n",
                        name, i+1, no_tiles+1);
                exit(EXIT_FAILURE);
            }
        }
        no_tiles++;
    }

    fclose(mapfp);
}

static void
merge_tiles(void)
{
    char ttype[TILEBUFSZ];
    int number;
    char name[TILEBUFSZ];
    pixel tile[MAX_TILE_Y][MAX_TILE_X], *p;
    int i, j, tile_in_file;

    tile_in_file = 0;

    while (read_text_tile_info(tile, ttype, &number, name)) {
        if (strcmp(name, tilenames[tile_in_file].name) != 0) {
            Fprintf(stderr,
                    "error: tile file changed while converting tilesets\n");
            exit(EXIT_FAILURE);
        }
        if (tilenames[tile_in_file].nth >= 0) {
            /* add the disambiguator */
            snprintf(name + strlen(name), TILEBUFSZ - strlen(name) - 1,
                     "s %d", tilenames[tile_in_file].nth);
        }
        tile_in_file++;

        /* So that the build system can distinguish between informational and
           warning output, we send the informational output to stdout and the
           warnings to stderr. */
        for (i = 0; i < no_tiles; i++)
            if (tiles[i].name && !strcmp(tiles[i].name, name))
                break;
        if (i != no_tiles) {
            int cur_tile_x = tile_x;
            int cur_tile_y = tile_y;
            tiles[i].ph = 0;
            tiles[i].scaled = 0;
            set_background(tile);

            /* We may have to scale the tile.  We have two scaling operations
               available: magnifying, doubling the size of the tile (which works
               on any size of tile); and embiggening, which slants and raises
               tiles as appropriate in order to change them from 32x32 flat to
               48x64 3D-effect. */
            while (cur_tile_x < target_tile_x &&
                   cur_tile_y < target_tile_y) {
                if (target_tile_x == 48 && target_tile_y == 64 &&
                    cur_tile_x == 32 && cur_tile_y == 32)
                    embiggen_tile_in_place(tile, name,
                                           &cur_tile_x, &cur_tile_y);
                else if (cur_tile_x * 2 <= target_tile_x &&
                         cur_tile_y * 2 <= target_tile_y)
                    magnify_tile_in_place(tile, name,
                                          &cur_tile_x, &cur_tile_y);
                else break;
            }

            if (cur_tile_x != tile_x || cur_tile_y != tile_y)
                tiles[i].scaled = 1;

            if (cur_tile_x != target_tile_x || cur_tile_y != target_tile_y) {
                Fprintf(stderr, "error: tile '%s' is the wrong size (%d,%d)"
                        " and cannot be scaled\n", name, tile_x, tile_y);
                exit(EXIT_FAILURE);
            }

            p = tiles[i].bitmap;
            for (j = 0; j < cur_tile_y; j++) {
                memcpy(p, &tile[j], cur_tile_x * sizeof (pixel));
                p += cur_tile_x;
            }
        } else
            Fprintf(stdout, "info: tile '%s' ignored\n", name);
    }
}

static void
write_tiles(void)
{
    const char *type;
    pixel tile[MAX_TILE_Y][MAX_TILE_X], *p;
    int i, j;

    tile_x = target_tile_x;
    tile_y = target_tile_y;
    for (i = 0; i < no_tiles; i++) {
        if (tiles[i].ph) {
            type = "placeholder";
            Fprintf(stderr, "warning: no tile found for name '%s'\n",
                    tiles[i].name);
        } else
            type = "tile";

        if (tiles[i].scaled)
            Fprintf(stdout, "info: tile '%s' was scaled up\n", tiles[i].name);

        p = tiles[i].bitmap;
        for (j = 0; j < tile_y; j++) {
            memcpy(&tile[j], p, tile_x * sizeof (pixel));
            p += tile_x;
        }
        write_text_tile_info(tile, type, i, tiles[i].name);
    }
}

int
main(int argc, char **argv)
{
    int argn = 1;
    char *outfile = NULL;
    char *mapfile = NULL;

    for (;;) {
        if (!strcmp(argv[argn], "-p")) {
            if (argn+1 >= argc) {outfile = 0; break;} /* force an error */
            if (!read_text_file_colormap(argv[argn+1])) {
                perror(argv[argn+1]);
                exit(EXIT_FAILURE);
            }
            init_colormap();
            argn += 2;
            continue;
        }
        if (!strcmp(argv[argn], "-o")) {
            if (argn+1 >= argc) {outfile = 0; break;}
            outfile = argv[argn+1];
            argn += 2;
            continue;
        }
        if (!strcmp(argv[argn], "-m")) {
            if (argn+1 >= argc) {outfile = 0; break;}
            mapfile = argv[argn+1];
            argn += 2;
            continue;
        }
        if (!strcmp(argv[argn], "-z")) {
            if (argn+2 >= argc) {outfile = 0; break;}
            target_tile_x = strtol(argv[argn+1], NULL, 10);
            target_tile_y = strtol(argv[argn+2], NULL, 10);
            argn += 3;
            continue;
        }
        break;
    }
    if (argc - argn < 1 || !outfile || !mapfile ||
        target_tile_x <= 0 || target_tile_y <= 0) {
        Fprintf(stderr,
                "usage: convert_tileset [-p palette-file] -o outfile -m mapfile "
                "-z width height [[-b<bg>] infile] ...\n");
        exit(EXIT_FAILURE);
    }

    read_mapfile(mapfile);

    for (; argn < argc; argn++) {
        if (argv[argn][0] == '-' && argv[argn][1] == 'b') {
            int r, g, b;
            pixel bg = DEFAULT_BACKGROUND;

            if (argv[argn][2]) {
                if (sscanf(argv[argn] + 2, "%02X%02X%02X", &r, &g, &b) != 3) {
                    Fprintf(stderr, "Background '%s' not understood.\n",
                            argv[argn] + 2);
                } else {
                    bg.r = (unsigned char)r;
                    bg.g = (unsigned char)g;
                    bg.b = (unsigned char)b;
                }
            }
            file_bg.r = bg.r;
            file_bg.g = bg.g;
            file_bg.b = bg.b;
        } else {
            /* tiletext complains if we handle multiple tilesets of different
               sizes unless we explicitly tell it that's OK. */
            tile_x = -1; tile_y = -1;

            /* We run through the file twice. The first time, we're
               discovering which names are used for tiles in the file.
               The second time, we do the actual merging. */
            if (!fopen_text_file(argv[argn], RDTMODE))
                exit(EXIT_FAILURE);
            read_tilenames();
            fclose_text_file();
            if (!fopen_text_file(argv[argn], RDTMODE))
                exit(EXIT_FAILURE);
            merge_tiles();
            fclose_text_file();
        }
    }

    if (!fopen_text_file(outfile, WRTMODE))
        exit(EXIT_FAILURE);
    write_tiles();
    fclose_text_file();

    exit(EXIT_SUCCESS);
     /*NOTREACHED*/ return 0;
}
