/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-03 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed. See license for details. */

/* Tileset compiler for NetHack 4.3.

   This program takes in any number of tilesets, each of which is in any of the
   accepted formats, and combines them into a single tileset, outputting it in
   any of the accepted formats. */

#include "tilecompile.h"

/* Global information about the tileset. */
char tileset_name[TILESET_NAME_SIZE + 1];
long tileset_width = -1;
long tileset_height = -1;

/* We create a palette based on the input we get.  If the palette is locked,
   then any colors outside the palette are forced onto the palette.
   Otherwise, any colors outside the palette are added to the palette, and if
   it overflows, we convert the image to RGB.

   The colors added are based on those present in the output image, normally.
   (Thus, creating this palette is normally one of the last things we do.)
   However, if we're locking the palette, we create this at the time that the
   palette is locked instead.  */
int palettesize, palettechannels;
pixel palette[MAX_PALETTE_SIZE];
bool palette_locking, palette_locked;

/* All the tile images used at any point are stored in one big array.  When we
   load a reference image, we split it into tiles and store them all here.

   This is an array of pointers, where each pointer points to a row-major 2D
   array of pixels.  (We do the palette processing later; everything's RGB
   internally until we do the final write-out.)

   For a cchar-based tileset, there's a conceptual array of 2**32 images that
   contains each possible cchar in images_seen.  The images_seen array itself
   is unused in this case (as are the palettes). */
pixel **images_seen;
int start_of_reference_image, seen_image_count;

/* Finally, the tiles themselves. This array is in the order in which we saw the
   tiles; it can contain duplicates, in which case the last matching entry must
   be used. (The duplicates are removed later.) */
tile *tiles_seen;
int seen_tile_count, allocated_tile_count;


/* Utility functions */

static int
compare_pixels_alpha_then_rgb(const void *p1, const void *p2)
{
    const pixel *p1c = p1, *p2c = p2;
    if (p1c->a != p2c->a)
        return (int)p1c->a - (int)p2c->a;
    return (int)p1c->r + (int)p1c->g + (int)p1c->b
        - (int)p2c->r - (int)p2c->g - (int)p2c->b;
}

static int
compare_pixels_red(const void *p1, const void *p2)
{
    const pixel *p1c = p1, *p2c = p2;
    return (int)p1c->r - (int)p2c->r;
}

/* Load an entire file into memory. Some bytes may already have been read, in
   which case they're provided as "header", "headerlen". Then calls the given
   callback and returns its result. */
static bool
slurp_file(FILE *in, png_byte *header, png_size_t headerlen,
           bool (*callback)(png_byte *, png_size_t))
{
    png_byte *storage = NULL;
    png_size_t storagelen = headerlen, curpos = headerlen;
    int c;
    bool rv;

    if (headerlen) {
        storage = malloc(headerlen);
        if (!storage) {
            fprintf(stderr, "Error: Could not allocate memory\n");
            return 0;
        }
        memcpy(storage, header, headerlen);
    }

    while ((c = getc(in)) != EOF) {
        if (curpos >= storagelen) {
            storagelen += 8;
            storagelen *= 2;
            storage = realloc(storage, storagelen);
            if (!storage) {
                fprintf(stderr, "Error: Could not allocate memory\n");
                return 0;
            }
        }
        storage[curpos++] = (png_byte)c;
    }

    fclose(in);

    rv = callback(storage, curpos);
    free(storage);
    return rv;
}

/* Loads a file and processes it appropriately.

   We determine the format by opening it in binary mode and reading the first 8
   characters. If it looks like a PNG file, we pass the rest to load_png_file.
   Otherwise, we read a bit more and compare to the binary tileset header. If
   that matches, we load the rest into memory and call load_binary_tileset. If
   it doesn't, we reopen the file in text mode, load into memory, and call
   load_text_tileset.

   Returns 1 on success, 0 on error. */
static bool
load_file(char *filename)
{
    FILE *in = fopen(filename, "rb");
    png_byte header[sizeof BINARY_TILESET_HEADER];
    if (!in) {
        perror(filename);
        return 0;
    }
    if (fread(header, sizeof (png_byte), PNG_HEADER_SIZE, in) ==
        PNG_HEADER_SIZE) {
        if (!png_sig_cmp(header, 0, PNG_HEADER_SIZE)) {
            /* It's a PNG file. load_png_file closes the FILE * it's given. */
            return load_png_file(in);
        }
        if (fread(header + PNG_HEADER_SIZE, sizeof (png_byte),
                  (sizeof BINARY_TILESET_HEADER) - PNG_HEADER_SIZE, in) ==
            ((sizeof BINARY_TILESET_HEADER) - PNG_HEADER_SIZE)) {

            if (memcmp(header, BINARY_TILESET_HEADER,
                       sizeof BINARY_TILESET_HEADER) == 0) {
                /* It's a binary file. */
                return slurp_file(in, header, sizeof BINARY_TILESET_HEADER,
                                  load_binary_tileset);
            }
        }
    }
    /* Didn't work, it must be a text file (or invalid, in which case we let
       the text file parser complain). */
    fclose(in);
    in = fopen(filename, "r");
    return slurp_file(in, 0, 0, load_text_tileset);
}

/* Checks to see if the given parsed value is believable as an integer.
   strtol returns 0 on error, LONG_MIN / LONG_MAX on overflow. */
static bool
in_range(long l)
{
    if (l < 1 || l >= INT_MAX)
        return 0;
    return 1;
}

static const char *const fn_strings[] = {
    [FN_TEXT] = "text",
    [FN_NH4CT] = "nh4ct",
    [FN_MAP] = "map",
    [FN_HEX] = "hex",
    [FN_BINARY] = "binary",
};

int
main(int argc, char *argv[])
{
    char *outfile = 0;
    int rv = EXIT_FAILURE;
    int formatnumber = -1;
    bool usage = 0, ignore_options = 0, keep_unused = 0,
        fuzz = 0, largepalette = 0;
    pixel transparent_color = {.r = 0, .g = 0, .b = 0, .a = 0};

    /* Parse command-line options. */
    if (*argv)
        argv++; /* ignore command name */
    while (*argv) {
        if (!strcmp(*argv, "-o") && argv[1] && !ignore_options) {
            outfile = argv[1];
            argv += 2;
        } else if (!strcmp(*argv, "-t") && argv[1] && !ignore_options) {
            int i;
            for (i = 0; i < (sizeof fn_strings / sizeof *fn_strings);
                 i++) {
                if (!strcmp(argv[1], fn_strings[i]))
                    formatnumber = i;
            }
            if (formatnumber == -1) {
                fprintf(stderr, "Error: unknown format '%s'", argv[1]);
                return EXIT_FAILURE;
            }
            argv += 2;
        } else if (!strcmp(*argv, "-n") && argv[1] && !ignore_options) {
            if (strlen(argv[1]) > TILESET_NAME_SIZE) {
                fprintf(stderr, "Error: tileset name too long");
                return EXIT_FAILURE;
            }
            strcpy(tileset_name, argv[1]);
            argv += 2;
        } else if (!strcmp(*argv, "-z") && argv[1] && argv[2] &&
                   !ignore_options) {
            tileset_width = strtol(argv[1], NULL, 10);
            tileset_height = strtol(argv[2], NULL, 10);
            if (!in_range(tileset_width) ||
                !in_range(tileset_height)) {
                fprintf(stderr, "Error: Invalid tileset size\n");
                return EXIT_FAILURE;
            }
            argv += 3;
        } else if (!strcmp(*argv, "-b") && argv[1] && argv[2] &&
                   argv[3] && !ignore_options) {
            long r = strtol(argv[1], NULL, 10);
            long g = strtol(argv[2], NULL, 10);
            long b = strtol(argv[3], NULL, 10);
            if (r < 0 || g < 0 || b < 0 ||
                r > 255 || g > 255 || b > 255) {
                fprintf(stderr, "Error: Invalid background color\n");
                return EXIT_FAILURE;
            }
            transparent_color.r = r;
            transparent_color.g = g;
            transparent_color.b = b;
            transparent_color.a = 255;
            argv += 4;
        } else if (!strcmp(*argv, "-p") && !ignore_options) {
            palette_locking = 1;
            argv++;
        } else if (!strcmp(*argv, "-k") && !ignore_options) {
            keep_unused = 1;
            argv++;
        } else if (!strcmp(*argv, "-f") && !ignore_options) {
            fuzz = 1;
            argv++;
        } else if (!strcmp(*argv, "-l") && !ignore_options) {
            largepalette = 1;
            argv++;
        } else if (!strcmp(*argv, "--help") && !ignore_options) {
            rv = EXIT_SUCCESS;
            usage = 1;
            break;
        } else if (!strcmp(*argv, "--") && !ignore_options) {
            argv++;
            ignore_options = 1;
            break;
        } else if (**argv != '-' || ignore_options) {
            /* It's not an option. Treat it as a filename. */
            if (!load_file(*argv)) {
                return EXIT_FAILURE;
            }
            argv++;
        } else {
            /* It's an unrecognised option. */
            fprintf(stderr, "Error: Unknown option '%s'\n", *argv);
            usage = 1;
            break;
        }
    }

    if (!outfile || formatnumber == -1 || usage) {
        if (formatnumber == -1 && !usage)
            fprintf(stderr, "Error: No format given (-t)\n");
        if (!outfile && !usage)
            fprintf(stderr, "Error: No output file given (-o)\n");
        fprintf(
            stderr, "Usage: tilecompile options file [file [...]]\n"
            "      This utility combines any number of tilesets into one,\n"
            "      and/or converts a tileset between formats.\n\n"
            "      All output formats are also valid input formats; when\n"
            "      using a two-file format, give the image first, then the\n"
            "      other file.\n\n"
            "      The following options must be given:\n"
            "      -o outfile        The file to output into\n"
            "                        [if outputting multiple files, the\n"
            "                         extension of each will be overriden]\n"
            "      -t format         The format to output in:\n"
            "          For image-based tilesets:\n"
            "              -t text      Slash'EM-style text format [.txt]\n"
            "              -t nh4ct     NetHack 4 compiled tileset [.nh4ct]\n"
            "              -t map       Image, tilemap for it [.png, .map]\n"
            "              -t hex       Image, references to it [.png, .txt]\n"
            "              -t binary    Like 'hex' but in binary [.png, .txt]\n"
            "          For text-based tilesets:\n"
            "              -t text      Editable text format [.txt]\n"
            "              -t hex       Compiled text format [.txt]\n"
            "              -t nh4ct     NetHack 4 compiled tileset [.nh4ct]\n"
            "              -t binary    Equivalent to 'nh4ct' [.bin]\n"
            "      The following options must be given if they aren't present\n"
            "      in the input files:\n"
            "      -z width height   The size of one tile\n"
            "      -n name           The name of the tileset\n\n"
            "      Other recognised options:\n"
            "      -p file           Lock the palette to the given file\n"
            "      -f                Adjust colors to match a locked palette\n"
            "      -b r g b          Edit the given color to transparent\n"
            "      -l                Allow large palettes\n"
            "      -k                Keep unused tile images\n"
            "      -u                Copy unrecognised tile names\n"
            "      -W                Warn if base tiles are missing\n");
        return rv;
    }

    /* We can't use large palettes with PNG right now, so just silently turn
       off large_palette if we're not generating FN_TEXT. */
    if (formatnumber != FN_TEXT)
        largepalette = 0;

    /* We might or might not be doing image processing. If an image was provided
       via any means, seen_image_count will be nonzero. In such a case, we need
       to delete unused images (unless -k), then calculate the palette (or
       enforce the palette if we have a locked palette). */

    if (seen_image_count) {
        if (tileset_width == 0 || tileset_height == 0) {
            /* can happen if a .nh4ct image has embedded cchars */
            fprintf(stderr,
                    "Error: cannot use images with a text-based tileset\n");
            return EXIT_FAILURE;
        }
        assert(tileset_width > 0 && tileset_height > 0);

        bool image_used[seen_image_count];
        int i;
        memset(image_used, 0, sizeof image_used);

        for (i = 0; i < seen_tile_count; i++) {
            if (tiles_seen[i].image_index >= seen_image_count) {
                /* e.g. a 5-tile image but references to tile 6 */
                fprintf(stderr, "Error: image index out of range\n");
                return 0;
            }
            image_used[tiles_seen[i].image_index] = 1;
        }

        /* If we have no palette yet, initialize a blank one. */
        if (!palette_locked) {
            palettesize = 0;
            palettechannels = 4;
        }

        for (i = 0; i < seen_image_count; i++) {
            int j;

            if (!keep_unused && !image_used[i]) {
                free(images_seen[i]);
                images_seen[i] = NULL;
                continue;
            }

            /* Some tilesets are converted to images without alpha, and some
               image formats cannot express a color key. Thus, we allow a
               color key to be given on the command line. This is where we
               replace it. */
            if (transparent_color.a) {
                for (j = 0; j < tileset_width * tileset_height; j++) {
                    pixel *p = &(images_seen[i][j]);
                    if (p->r == transparent_color.r &&
                        p->g == transparent_color.g &&
                        p->b == transparent_color.b) {
                        p->r = TRANSPARENT_R;
                        p->g = TRANSPARENT_G;
                        p->b = TRANSPARENT_B;
                        p->a = 0;
                    }
                }
            }

            /* Search the image for pixels or channels not in the palette.

               If we find a pixel or channel that isn't there, we add it.
               If this would overflow the palette, we downgrade a
               pixel-based palette to channel-based. (A channel-based
               palette cannot overflow because there are only 256 possible
               channel values.)

               Exception: if the palette is locked, we can't change it, so
               instead we have to find the nearest available color (or
               error out if !fuzz).

               Further exception: if we have a channel-based palette already
               and the format isn't FN_TEXT, ignore this; we're not going to
               use the resulting channel-based palette anyway. */
            if (palettechannels == 1 && formatnumber != FN_TEXT)
                continue;

            for (j = 0; j < tileset_width * tileset_height; j++) {
                pixel p = images_seen[i][j];
                int pi;
                bool foundr = 0, foundg = 0, foundb = 0, founda = 0;
                for (pi = 0; pi < palettesize; pi++) {
                    if (palettechannels == 4) {
                        if (palette[pi].r == p.r &&
                            palette[pi].g == p.g &&
                            palette[pi].b == p.b &&
                            palette[pi].a == p.a)
                            break;
                    } else {
                        if (palette[pi].r == p.r) foundr = 1;
                        if (palette[pi].r == p.g) foundg = 1;
                        if (palette[pi].r == p.b) foundb = 1;
                        if (palette[pi].r == p.a) founda = 1;
                        if (foundr && foundg && foundb && founda)
                            break;
                    }
                    /* TODO: fuzz */
                }
                if (pi == palettesize) {
                    /* It wasn't found. */
                    if (palette_locked) {
                        fprintf(stderr, "Error: pixel (%d, %d, %d, %d) "
                                "not in palette", p.r, p.g, p.b, p.a);
                        return EXIT_FAILURE;
                    }

                    /* Do we have to convert a pixel-based palette to being
                       channel-based? We do if a) the palette overflowed,
                       b) we're going to use it (formatnumber == FN_TEXT). */
                    if (palettesize ==
                        (largepalette ? MAX_PNG_PALETTE_SIZE :
                         MAX_PALETTE_SIZE) && formatnumber == FN_TEXT) {
                        assert(palettechannels == 4);
                        palettechannels = 1;
                        pixel palettecopy[MAX_PALETTE_SIZE];
                        memcpy(palettecopy, palette, sizeof palettecopy);
                        int pcsize = palettesize;
                        palettesize = 0;
                        for (pi = 0; pi < pcsize; pi++) {
                            int pj;
                            for (pj = 0; pj < palettesize; pj++)
                                if (palette[pj].r == palettecopy[pi].r)
                                    break;
                            if (pj == palettesize)
                                palette[palettesize++].r = palettecopy[pi].r;
                            for (pj = 0; pj < palettesize; pj++)
                                if (palette[pj].r == palettecopy[pi].g)
                                    break;
                            if (pj == palettesize)
                                palette[palettesize++].r = palettecopy[pi].g;
                            for (pj = 0; pj < palettesize; pj++)
                                if (palette[pj].r == palettecopy[pi].b)
                                    break;
                            if (pj == palettesize)
                                palette[palettesize++].r = palettecopy[pi].b;
                            for (pj = 0; pj < palettesize; pj++)
                                if (palette[pj].r == palettecopy[pi].a)
                                    break;
                            if (pj == palettesize)
                                palette[palettesize++].r = palettecopy[pi].a;
                        }
                        /* Now repeat the pixel; it might or might not already
                           be in the new channel-based palette. */
                        j--;
                        continue;
                    }

                    /* There's room in the palette; add the new pixel. */
                    if (palettechannels == 4)
                        palette[palettesize++] = p;
                    else {
                        if (!foundr)
                            palette[palettesize++].r = p.r;
                        if (!foundg)
                            palette[palettesize++].g = p.g;
                        if (!foundb)
                            palette[palettesize++].b = p.b;
                        if (!founda)
                            palette[palettesize++].a = p.a;
                    }
                }
            }
        }

        /* At this point, every pixel of every image (that we didn't just
           delete) exists in the palette (although as an optimization, if we're
           not going to use the palette (palettechannels == 1 && format !=
           FN_TEXT), we haven't actually calculated the palette itself). The
           next step is to sort the palette for aesthetics and compressability:
           we want lowest alpha first, tiebreak by lowest r+g+b. */
        if (palettechannels == 4) {
            qsort(palette, palettesize, sizeof (pixel),
                  compare_pixels_alpha_then_rgb);
        } else if (formatnumber == FN_TEXT) {
            qsort(palette, palettesize, sizeof (pixel),
                  compare_pixels_red);
        }
    }

    /* The various possible files that might be generated:

       - A reference image (all non-cchar formats but "text");
       - A map file ("map");
       - A reference-based source file ("hex", non-cchar "nh4ct");
       - A spelt-out source file ("text");
       - A binary-format file ("binary", "nh4ct").

       In the case of non-cchar nh4ct, we're generating three files, but packing
       them together into one. In other cases where we need multiple files, we
       generate them as separate files.
    */
    char fnbuf[strlen(outfile) + 7]; /* ".nh4ct" is 6 + NUL */
    strcpy(fnbuf, outfile);
    if (strrchr(fnbuf, '.'))
        *(strrchr(fnbuf, '.')) = '\0';
    char *extpos = fnbuf + strlen(fnbuf);
    rv = 1;
    switch(formatnumber) {
    case FN_TEXT:
        rv &= write_text_tileset(outfile, II_SPELTOUT);
        break;
    case FN_MAP:
        if (tileset_width <= 0 || tileset_height <= 0) {
            fprintf(stderr, "Error: Cannot write map + image with no image\n");
            return EXIT_FAILURE;
        }
        strcpy(extpos, ".png");
        rv &= write_png_file(fnbuf, 0);
        strcpy(extpos, ".map");
        rv &= write_text_tileset(fnbuf, II_FILEPOS);
        break;
    case FN_HEX:
        if (!seen_image_count)
            rv &= write_text_tileset(outfile, II_HEX);
        else {
            strcpy(extpos, ".png");
            rv &= write_png_file(fnbuf, 0);
            strcpy(extpos, ".txt");
            rv &= write_text_tileset(fnbuf, II_HEX);
        }
        break;
    case FN_BINARY:
        if (!seen_image_count)
            rv &= write_binary_tileset(outfile);
        else {
            strcpy(extpos, ".png");
            rv &= write_png_file(fnbuf, 0);
            strcpy(extpos, ".bin");
            rv &= write_binary_tileset(fnbuf);
        }
        break;
    case FN_NH4CT:
        rv &= !write_png_file(outfile, 1);
        break;
    }

    return rv ? EXIT_SUCCESS : EXIT_FAILURE;
}
