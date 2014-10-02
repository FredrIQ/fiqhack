/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-03 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed. See license for details. */

#ifndef TILECOMPILE_H
# define TILECOMPILE_H

#define PNG_SETJMP_SUPPORTED
#include <png.h> /* must be included first, for complicated reasons to do with
                    <setjmp.h> */
#include <zlib.h>

# include <assert.h>
# include <limits.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <stdbool.h>

/* Definitions */

typedef struct {
    uint8_t r, g, b, a;
} pixel;

typedef struct {
    int tilenumber;
    unsigned long long substitution;
    uint32_t image_index;    /* or cchar */
} tile;

enum formatname {
    FN_TEXT,
    FN_NH4CT,
    FN_MAP,
    FN_HEX,
    FN_BINARY
};

enum iiformat {
    II_HEX,
    II_SPELTOUT,
    II_FILEPOS
};

/* The number of possible single-character keys. */
# define KEYSPACE 64
/* Keys can be up to two characters wide. */
# define MAX_PALETTE_SIZE (KEYSPACE * KEYSPACE)
/* The maximum length of a tileset name. */
# define TILESET_NAME_SIZE 80
/* The header for a binary tileset. This ends with two NULs; one is implicit,
   we add the other here. */
# define BINARY_TILESET_HEADER "NH4TILESET\0"

/* Palettes larger than a certain size must be converted to RGB to write them
   into a PNG image. */
# define MAX_PNG_PALETTE_SIZE 256
/* The number of bytes in a PNG header. */
# define PNG_HEADER_SIZE 8

/* Slash'EM's transparent color key. We need to know what this is for backwards
   compatibility. */
# define TRANSPARENT_R 71
# define TRANSPARENT_G 108
# define TRANSPARENT_B 108

/* Globals; these are defined in tilecompile.c */
extern char tileset_name[TILESET_NAME_SIZE + 1];
extern long tileset_width;
extern long tileset_height;

extern int palettesize, palettechannels;
extern pixel palette[MAX_PALETTE_SIZE];
extern bool palette_locking, palette_locked;

extern pixel **images_seen;
extern int start_of_reference_image, seen_image_count;

extern tile *tiles_seen;
extern int seen_tile_count, allocated_tile_count;

/* Extern functions */

/* tileset-read.c */
extern bool load_text_tileset(png_byte *, png_size_t);
extern bool load_binary_tileset(png_byte *, png_size_t);

/* tileset-image.c */
extern bool load_png_file(FILE *);
extern bool write_png_file(const char *, bool);

/* tileset-write.c */
extern bool write_text_tileset(const char *, enum iiformat);
extern bool write_binary_tileset(const char *);

#endif
