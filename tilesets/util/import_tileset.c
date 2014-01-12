/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-01-12 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed. See license for details. */

/* Given a PNG image and a map, produces a text-format tileset. */

#define PNG_SETJMP_SUPPORTED
#include <png.h> /* must be included first, for complicated reasons to do with
                    <setjmp.h> */
#include <zlib.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "tile.h"

static void
add_color_to_colormap(pixel *p)
{
    int i;
    for (i = 0; i < colorsinmap; i++) {
        if (ColorMap[CM_RED][i] == p->r &&
            ColorMap[CM_GREEN][i] == p->g &&
            ColorMap[CM_BLUE][i] == p->b)
            return;
    }
    if (colorsinmap == MAXCOLORMAPSIZE) {
        static int warned = 0;
        if (!warned)
            fprintf(stderr, "warning: Too many colors in input image\n");
        warned = 1;
    } else {
        ColorMap[CM_RED][colorsinmap] = p->r;
        ColorMap[CM_GREEN][colorsinmap] = p->g;
        ColorMap[CM_BLUE][colorsinmap] = p->b;
        colorsinmap++;
    }
}

/* Code based on the PNG reading code in libuncursed (which I, Alex Smith,
   originally wrote based on public domain sources, thus no copyright
   issues). The return value is returned via an argument because C doesn't seem
   to provide a way to define a function returning a pointer to an array. */
static void
load_png_file(char *filename, int w, int h, int *tilecount,
              pixel (**rv)[MAX_TILE_Y][MAX_TILE_X])
{
    /* Based on a public domain example that ships with libpng */
    volatile png_structp png_ptr;
    png_structp png_ptr_nv;
    volatile png_infop info_ptr;
    png_infop info_ptr_nv;

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, has_alpha, i, j, x, y, t;

    /* The pointers are volatile. What they point to isn't. */
    unsigned char *volatile pixeldata = NULL;
    unsigned char **volatile rowpointers = NULL;

    FILE *in = fopen(filename, "rb");

    *rv = NULL;

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

    if ((width % w) != 0) {
        fprintf(stderr, "Error: The width of one tile does not divide into "
                "the width of the tile image\n");
        goto cleanup_info_and_rowpointers;
    }
    if ((height % h) != 0) {
        fprintf(stderr, "Error: The height of one tile does not divide into "
                "the height of the tile image\n");
        goto cleanup_info_and_rowpointers;
    }
    *tilecount = width / w * height / h;

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

    /* Add an alpha channel, if the image still hasn't got one */
    if (!has_alpha)
        png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

    /* Allocate memory for the PNG image */
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

    /* Allocate the pixel array we return into */
    *rv = malloc(MAX_TILE_X * MAX_TILE_Y * *tilecount * sizeof (pixel));
    if (!*rv) {
        fprintf(stderr, "Error allocating memory for tiles\n");
        goto cleanup_info_and_rowpointers;
    }

    /* Copy the PNG data into the tiles. */
    colorsinmap = 0;

    for (t = 0; t < *tilecount; t++) {

        i = (t % (width / w)) * w;
        j = (t / (width / w)) * h;

        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                
                if (rowpointers[j + y][(i + x) * 4 + 3] < 128) {
                    /* It's transparent.
                       Replace it with the default color key. */
                    (*rv)[t][y][x] = (pixel)DEFAULT_BACKGROUND;
                } else {
                    (*rv)[t][y][x].r = rowpointers[j + y][(i + x) * 4 + 0];
                    (*rv)[t][y][x].g = rowpointers[j + y][(i + x) * 4 + 1];
                    (*rv)[t][y][x].b = rowpointers[j + y][(i + x) * 4 + 2];
                }
                add_color_to_colormap(&((*rv)[t][y][x]));

            }
    }

    /* Cleanup label chain; this C idiom is one of the few legitimate uses of
       goto, because it doesn't have an appropriate control flow structure */
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
    ;
}

/* Checks to see if the given parsed value is believable as an integer.
   strtol returns 0 on error, LONG_MIN / LONG_MAX on overflow. */
static int
in_range(long l)
{
    if (l < 1 || l >= INT_MAX)
        return 0;
    return 1;
}

int
main(int argc, char *argv[])
{
    char *infile = NULL, *outfile = NULL, *mapfile = NULL;
    long w = 0, h = 0, rv = EXIT_FAILURE;
    int t;

    /* Parse command-line options. */
    if (*argv)
        argv++; /* ignore command name */
    while (*argv) {
        if (!strcmp(*argv, "-o")) {
            outfile = argv[1];
            argv += 2;
        } else if (!strcmp(*argv, "-m")) {
            mapfile = argv[1];
            argv += 2;
        } else if (!strcmp(*argv, "-z")) {
            w = strtol(argv[1], NULL, 10);
            h = strtol(argv[2], NULL, 10);
            argv += 3;
        } else if (!strcmp(*argv, "--help")) {
            rv = EXIT_SUCCESS;
            while (*argv)
                argv++; /* move to the end of the arg list to force usage */
        } else if (!strcmp(*argv, "--")) {
            argv++;
            break;
        } else if (**argv != '-') {
            /* It's not an option. */
            break;
        } else {
            /* It's an unrecognised option. */
            while (*argv)
                argv++; /* move to the end of the arg list to force usage */
        }
    }

    /* There should now be exactly one argument left, the input file. */
    if (*argv && !argv[1])
        infile = *argv;

    if (!infile || !outfile || !mapfile || !in_range(w) || !in_range(h)) {
        fprintf(stderr, "Usage: import_tileset options infile.png\n"
                "      The following options must all be given:\n"
                "      -o outfile.txt    The file to output in\n"
                "      -m mapfile.map    The map specifying the tile sequence\n"
                "      -z width height   The size of one tile\n");
        return rv;
    }

    /* Load the .png file. */
    int tilecount;
    pixel (*tiles)[MAX_TILE_Y][MAX_TILE_X] = NULL;
    load_png_file(infile, w, h, &tilecount, &tiles);
    if (!tiles)
        goto cleanup_nothing;

    /* Open the output file. */
    tile_x = w;
    tile_y = h;
    init_colormap();        /* Initializes palette from the ColorMap global */
    if (!fopen_text_file(outfile, WRTMODE))
        goto cleanup_tiles;

    /* Load the map file. */
    if (!set_tile_map(mapfile))
        goto cleanup_tiles_and_files;

    /* Write the output file. */
    for (t = 0; t < tilecount; t++)
        write_text_tile(tiles[t]);

cleanup_tiles_and_files:
    fclose_text_file();
cleanup_tiles:
    free(tiles);
cleanup_nothing:
    return rv;
}
