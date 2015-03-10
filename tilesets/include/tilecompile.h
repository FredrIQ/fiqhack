/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-10 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed. See license for details. */

#ifndef TILECOMPILE_H
# define TILECOMPILE_H

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
    FN_BINARY,
    FN_IMAGE,
    FN_PALETTE,
};

enum iiformat {
    II_HEX,
    II_SPELTOUT,
    II_FILEPOS,
    II_NONE,
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
/* The PNG header itself. */
# define PNG_HEADER "\x89PNG\x0d\x0a\x1a\x0a"

/* Slash'EM's transparent color key. We need to know what this is for backwards
   compatibility. */
# define TRANSPARENT_R 71
# define TRANSPARENT_G 108
# define TRANSPARENT_B 108

# define CCHAR_COLOR_COUNT 19
# define CCHAR_FGCOLOR_TRANSPARENT 16
# define CCHAR_FGCOLOR_DISTURB     17
# define CCHAR_FGCOLOR_BASE        18
# define CCHAR_BGCOLOR_TRANSPARENT 8
# define CCHAR_BGCOLOR_BASE        9
# define CCHAR_CHAR_BASE           0xd800

/* Constant arrays */
extern const char *const cchar_color_names[CCHAR_COLOR_COUNT];

/* Globals; these are defined in tilecompile.c */
extern char tileset_name[TILESET_NAME_SIZE + 1];
extern long tileset_width;
extern long tileset_height;

extern int palettesize, palettechannels;
extern pixel palette[MAX_PALETTE_SIZE];
extern bool palette_locking, palette_locked;

extern pixel **locked_images_seen;
extern int locked_image_count;
extern bool image_locking, image_locked;

extern pixel **images_seen;
extern int start_of_reference_image, seen_image_count;

extern tile *tiles_seen;
extern int seen_tile_count, allocated_tile_count;

extern bool copy_unknown_tile_names;
extern char **unknown_tile_names;
extern int unknown_name_count, allocated_name_count;

/* Extern functions */

/* tileset-read.c */
extern bool load_text_tileset(uint8_t *, size_t);
extern bool load_binary_tileset(uint8_t *, size_t);

/* tileset-image.c */
extern bool load_png_file(FILE *);
extern bool write_png_file(const char *, bool);

/* tileset-write.c */
extern bool write_text_tileset(const char *, enum iiformat);
extern bool write_binary_tileset(const char *);
extern bool callback_with_text_tileset(enum iiformat,
                                       bool (*)(uint8_t *, size_t));
extern bool callback_with_binary_tileset(bool (*)(uint8_t *, size_t));

/* tilecompile.c */
extern bool slurp_file(FILE *, uint8_t *, size_t, const char *,
                       bool (*)(uint8_t *, size_t));

#endif
