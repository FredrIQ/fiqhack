/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-17 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed. See license for details. */

#include "tilecompile.h"
#include "tilesequence.h"
#include "utf8conv.h"
#include "hacklib.h"
#include <string.h>

#define EPRINT(s, ...)                                       \
    do {                                                     \
        fprintf(stderr, "line %d: " s, lineno, __VA_ARGS__); \
        return 0;                                            \
    } while (0)

#define EPRINTN(s)                                           \
    do {                                                     \
        fprintf(stderr, "line %d: " s, lineno);              \
        return 0;                                            \
    } while (0)

/* Utility functions */

static int
palette_key_to_int(uint8_t key)
{
    if (key == '_')
        return 0;
    else if (key == '$')
        return 63;
    else if (key >= 'A' && key <= 'Z')
        return key - 'A' + 1;
    else if (key >= 'a' && key <= 'z')
        return key - 'a' + 27;
    else if (key >= '0' && key <= '9')
        return key - '0' + 53;
    else
        return -1;
}

static int
image_transformation(const double t[static 5], const pixel p)
{
    double d = t[4] +
        t[0] * (double) p.r +
        t[1] * (double) p.g +
        t[2] * (double) p.b +
        t[3] * (double) p.a;
    if (d < 0.0)
        d = 0.0;
    if (d > 255.0)
        d = 255.0;
    return (int) (d + 0.5);
}

/* Binary loading. Returns 1 on success, 0 on error. */
bool
load_binary_tileset(uint8_t *data, size_t size)
{
    uint8_t *dp;
    long tw, th;

    if (size < TILESET_NAME_SIZE + 2 + 2 + sizeof BINARY_TILESET_HEADER) {
        fprintf(stderr, "Binary tileset too short");
        return 0;
    }
    if (memcmp(data, BINARY_TILESET_HEADER,
               sizeof BINARY_TILESET_HEADER) != 0) {
        fprintf(stderr, "Bad binary tileset header");
        return 0;
    }

    /* Load the fields from the header. */
    memcpy(tileset_name, data + sizeof BINARY_TILESET_HEADER,
           TILESET_NAME_SIZE);
    tileset_name[TILESET_NAME_SIZE] = '\0';

    dp = data + (sizeof BINARY_TILESET_HEADER) + TILESET_NAME_SIZE;
    tw = dp[0] + (dp[1] << 8);
    th = dp[2] + (dp[3] << 8);

    if (tileset_width != -1 && tileset_width != tw) {
        fprintf(stderr, "Error: inconsistent width (%ld, %ld)\n",
                tileset_width, tw);
        return 0;
    }
    if (tileset_height != -1 && tileset_height != th) {
        fprintf(stderr, "Error: inconsistent width (%ld, %ld)\n",
                tileset_height, th);
        return 0;
    }

    tileset_width = tw;
    tileset_height = th;

    for (dp += 4; dp - data < size; dp += 16) {
        if (seen_tile_count >= allocated_tile_count) {
            allocated_tile_count += 8;
            allocated_tile_count *= 2;
            tiles_seen = realloc(tiles_seen, allocated_tile_count *
                                 sizeof *tiles_seen);
            if (!tiles_seen) {
                fprintf(stderr, "Error: not enough memory for tiles\n");
                return 0;
            }
        }

        uint32_t tileno =
            (((uint32_t)dp[0]) << 0) +
            (((uint32_t)dp[1]) << 8) +
            (((uint32_t)dp[2]) << 16) +
            (((uint32_t)dp[3]) << 24);
        uint64_t substitution =
            (((uint64_t)dp[4]) << 0) +
            (((uint64_t)dp[5]) << 8) +
            (((uint64_t)dp[6]) << 16) +
            (((uint64_t)dp[7]) << 24) +
            (((uint64_t)dp[8]) << 32) +
            (((uint64_t)dp[9]) << 40) +
            (((uint64_t)dp[10]) << 48) +
            (((uint64_t)dp[11]) << 56);
        uint32_t image_index =
            (((uint32_t)dp[12]) << 0) +
            (((uint32_t)dp[13]) << 8) +
            (((uint32_t)dp[14]) << 16) +
            (((uint32_t)dp[15]) << 24);

        tiles_seen[seen_tile_count++] =
            (tile){.tilenumber = tileno,
                   .substitution = substitution,
                   .image_index = image_index + start_of_reference_image};
    }

    return 1;
}

/* Text loading. This has the same API as binary loading. */
bool
load_text_tileset(uint8_t *data, size_t size)
{
    size_t i;

    /* We also need to store the palette for the current file, so that we can
       interpret what it means. */
    int filepalettechannels = 0, filekeywidth = 0;
    pixel filepalette[MAX_PALETTE_SIZE];
    bool filepalettevalid[MAX_PALETTE_SIZE] = {0};

    /* Sometimes, tiles are given without the s-number suffix. For such cases,
       we need to record how many times we've seen the unsuffixed tile within
       this file. */
    int unsuffixed_seen_count[TILESEQ_COUNT] = {0};

    /* Some tilesets specify image references via file position. */
    int filepos = 0;
    bool using_filepos = 0, avoiding_filepos = 0;

    int lineno = 1;

    int pk1, pk2, pk, pw;

    assert(data);
    assert(size);

    /* Run through the entirety of data. We convert newline to NUL to make
       parsing easier, and also check for illegal characters. Tile keys never
       contain Unicode, but cchar definitions can, so we disallow characters
       outside the ASCII range except for immediately after an apostrophe. */
    for (i = 0; i < size; i++) {
        if (data[i] == '\n') {
            data[i] = 0;
            lineno++;
        } else if (data[i] == '\t') {
            data[i] = ' ';
        } else if (data[i] == '\'') {
            while (i < size && data[i+1] > '~')
                i++;
        } else if (data[i] < ' ' || data[i] > '~')
            EPRINT("Error: Invalid byte '\\x%02X' in text file\n",
                   (int)data[i]);
    }

    /* Requiring text files to end with newlines, like they're supposed to,
       makes things much simpler. */
    if (data[size-1])
        EPRINTN("Error: text file does not end with a newline\n");

    /* Run through the file again, parsing each line.

       (Using strlen rather than memchr would be simpler, but we can't guarantee
       that the pointer has the correct signedness. rawmemchr would also be
       simpler, but it's a GNU extension.) */
    for (i = 0, lineno = 1; i < size;
         i = (uint8_t *)memchr(data + i, 0, size - i) - data + 1, lineno++) {

        int len = (uint8_t *)memchr(data + i, 0, size - i) - data;

        /* Is it a comment line (leading !)? */
        if (data[i] == '!')
            continue;

        /* Or a blank line? */
        if (!data[i])
            continue;

        /* Is it a palette line? These are one or two characters followed by
           " = (". */
        pw = 0;
        pk = 0; /* at low optimization levels, gcc can't see that this is only
                   valid when pw is, so initialize it explicitly */
        pk1 = palette_key_to_int(data[i + 0]);
        pk2 = palette_key_to_int(data[i + 1]);

        if (pk1 >= 0 && memcmp(data + i + 1, " = (", 4) == 0) {
            pw = 1;
            pk = pk1;
        } else if (pk1 >= 0 && pk2 >= 0 &&
                   memcmp(data + i + 2, " = (", 4) == 0) {
            pw = 2;
            pk = pk1 * 64 + pk2;
        }

        if (pw) {
            if (filekeywidth && filekeywidth != pw)
                EPRINTN("Error: text file has inconsistent palette keys\n");

            filekeywidth = pw;

            /* Now parse the numbers given. */
            uint8_t *keylocs[] =
                { &(filepalette[pk].r), &(filepalette[pk].g),
                  &(filepalette[pk].b), &(filepalette[pk].a) };
            int curchannel = 0;
            unsigned long curchannelval = 0;
            uint8_t *dp = data + i + pw + 4;
            for (;;) {
                if (*dp >= '0' && *dp <= '9') {
                    curchannelval *= 10; curchannelval += *dp - '0';
                } else if (*dp == ',' || *dp == ')') {
                    if (curchannel >= 4)
                        EPRINTN("Error: images are limited to 4 channels\n");
                    if (curchannelval > 255)
                        EPRINTN("Error: channel value above 255\n");
                    *(keylocs[curchannel]) = (uint8_t) curchannelval;
                    curchannelval = 0;
                    curchannel++;
                    if (*dp == ')')
                        break;
                } else if (*dp == ' ') {
                    /* do nothing */
                } else if (!*dp) {
                    EPRINTN("Error: missing ')' in palette\n");
                } else {
                    EPRINT("Error: unexpected '%c' in palette\n", (int)*dp);
                }
                dp++;
            }
            if (curchannel == 3) {
                /* Add an alpha channel based on Slash'EM's transparency
                   rules. */
                curchannel = 4;
                *(keylocs[3]) = (*keylocs[0] == TRANSPARENT_R &&
                                 *keylocs[1] == TRANSPARENT_G &&
                                 *keylocs[2] == TRANSPARENT_B) ? 0 : 255;
            }
            if (curchannel != 1 && curchannel != 4)
                EPRINTN("Error: palette entries must be "
                        "1, 3, or 4 channels wide\n");
            if (filepalettechannels && filepalettechannels != curchannel)
                EPRINTN("Error: image is both paletted and RGBA\n");
            if (filepalettevalid[pk])
                EPRINT("Error: duplicate palette key '%.*s'\n", pw, data + i);
            filepalettevalid[pk] = true;
            filepalettechannels = curchannel;
            continue;
        }

        /* It's not a palette. Is it a name/width/height directive? */

        if (len > 7 && memcmp(data + i, "# name ", 7) == 0) {
            if (len > TILESET_NAME_SIZE)
                EPRINTN("Error: tileset specifies overlong name\n");
            memcpy(tileset_name, data + i + 7, len + 1);
            continue;
        }

        /* We combine the code for width and height directives, as it's
           almost the same. */
        {
            uint8_t *dp = 0;
            long *td = 0;
            if (len > 8 && memcmp(data + i, "# width ", 8) == 0) {
                td = &tileset_width;
                dp = data + i + 8;
            } else if (len > 9 && memcmp(data + i, "# height ", 9) == 0) {
                td = &tileset_height;
                dp = data + i + 9;
            }
            if (td) {
                long v = 0;
                while (*dp) {
                    if (*dp >= '0' && *dp <= '9') {
                        v *= 10;
                        v += *dp - '0';
                        if (v > INT_MAX)
                            EPRINTN("Error: width/height too large\n");
                    } else if (*dp != ' ') {
                        EPRINT("Error: unexpected '%c' in width/height\n",
                               (int)*dp);
                    }
                    dp++;
                }
                if (*td != -1 && *td != v)
                    EPRINT("Error: inconsistent width/height "
                           "(%ld, %ld)\n", *td, v);
                *td = v;
                continue;
            }
        }

        /* At this point, it must be a tile definition (or a syntax error).

           First, find the tile name. It's in one of these formats:

           # tile 0 (tile name)\0
           # tile 0 (tile name):
           # tile name\0
           # tile name:

           We can have multiple tilenames, in which case they're separated
           by semicolon-space tokens. */
        {
            char *tilename = malloc(len + 1);
            uint8_t *dp = data + i;
            char *tp = tilename;

            if (!tilename)
                EPRINTN("Error: Could not allocate memory.\n");

            if (len > 6 && memcmp(data + i, "# tile ", 6) == 0) {
                while (*dp && *dp != '(')
                    dp++;
                if (*dp == '(')
                    dp++;
            }
            while (*dp && *dp != ')' && *dp != ':') {
                *(tp++) = *(dp++);
            }
            *tp = '\0';

            /* Remove any comment from the tilename. */
            if (strncmp(tilename, "cmap / ", 7) == 0)
                memmove(tilename , tilename + 7, strlen(tilename) - 6);
            else if (strstr(tilename, " / "))
                *(strstr(tilename, " / ")) = '\0';

            tile *intiles = NULL;
            int intile_alloccount = 0;
            int intilecount = 0;
            bool firsttile = 1;
            char *tiletoken;

            unsigned long long substar_substitution = 0;
            bool substar = 0;

            while ((tiletoken = strtok(firsttile ? tilename : NULL, ";"))) {

                /* Do a sanity check on the tile name we found, in case it's
                   someone trying to use syntax from elsewhere in the file and
                   getting it wrong. */
                if (strspn(tiletoken, "-,'?* 0123456789"
                           "abcdefghijklmnopqrstuvwxyz"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ") != strlen(tiletoken))
                    EPRINT("Error: Syntax error or bad tile name '%s'\n",
                           tiletoken);

                const char *tp2 = tiletoken;

                /* Remove leading spaces; discard tiles with empty names */
                while (*tp2 == ' ')
                    tp2++;
                if (!*tp2)
                    continue;

                unsigned long long substitution = substitution_from_name(&tp2);

                /* The "sub *" syntax causes us to copy substitutions from
                   matching tiles seen so far. This must be alone on the
                   line. */

                if (strstr(tp2, "sub * ") == tp2) {
                    if (!firsttile)
                        EPRINT("Error: 'sub *' tile '%s' is not alone on "
                               "its line\n", tiletoken);

                    substar = 1;
                    substar_substitution = substitution;
                    tp2 += strlen("sub * ");
                    substitution = substitution_from_name(&tp2);
                } else if (substar) {
                    EPRINT("Error: Extra tile name '%s' seen after a "
                           "'sub *' tile\n", tiletoken);
                }

                firsttile = 0;

                int exact_match_tilenumber =
                    tileno_from_name(tp2, TILESEQ_INVALID_OFF);

                /* If it isn't a valid name, there are two possibilities:

                   a) It's a 3.4.3/Slash'EM style ambiguous tile name. If this
                      is the case, we can append "s 0" to get a valid tile name;
                      if that works, we can use its number to get at the
                      unsuffixed seen count and get at the tile number that way.

                   b) It contains wildcard matches. If this is the case, we need
                      to loop over all tile names to see which ones match. We
                      can discover that we're in this case by looking for a
                      '?' or '*'.

                   To avoid code duplication, we enter the loop unconditionally,
                   but if we aren't in case b), we only run one iteration. */
                bool caseb = (strchr(tp2, '?') || strchr(tp2, '*'));
                int tiletry = caseb ? 0 : exact_match_tilenumber;
                int loopmax = caseb ? TILESEQ_COUNT : tiletry + 1;
                bool tilefound = 0;

                for (; tiletry < loopmax; tiletry++) {

                    bool tileok = 0;

                    /* If we're in case a), then the only tile we "test" will
                       be TILESEQ_INVALID_OFF. It's OK to change tiletry in
                       this case; it'll never become smaller than loopmax - 1,
                       because TILESEQ_INVALID_OFF is negative. */
                    if (tiletry == TILESEQ_INVALID_OFF) {
                        /* strlen("s 4294967295") == 12, plus 1 for the NUL */
                        char *tiletoken2 = malloc(strlen(tp2) + 13);
                        strcpy(tiletoken2, tp2);
                        tp = tiletoken2 + strlen(tiletoken2);
                        strcpy(tp, "s 0");
                        tiletry =
                            tileno_from_name(tiletoken2, TILESEQ_INVALID_OFF);
                        if (tiletry != TILESEQ_INVALID_OFF) {
                            sprintf(tp, "s %d",
                                    unsuffixed_seen_count[tiletry]++);
                            tiletry = tileno_from_name(tiletoken2,
                                                       TILESEQ_INVALID_OFF);
                        }
                        free(tiletoken2);

                        if (tiletry != TILESEQ_INVALID_OFF)
                            tileok = 1;
                    } else {
                        /* If we get here, the tilename can't be ambiguous;
                           either there was an exact match already (against an
                           unambiguous tile name), or else we have wildcards,
                           which don't permit ambiguous matches. All strings
                           pattern-match against themeselves, so we can just
                           do a pattern match here.

                           Ignore nonsensical substitution tiles, so that it's
                           possible to write, e.g., "sub corpse *" and only
                           change monster corpses, without also changing, say,
                           "floor of a room with a corpse on" (i.e. "sub corpse
                           the floor of a room", which literally matches the
                           pattern, but probably wasn't intended). */
                        unsigned long long s =
                            substar_substitution | substitution;
                        if ((sensible_substitutions(tiletry) & s) == s)
                            tileok = pmatch(tp2, name_from_tileno(tiletry));
                    }

                    /* If this is the last iteration of the loop and we still
                       haven't found a valid match, we need to invent a new
                       number for the reference if -u. Otherwise, the default
                       behaviour (discarding the tile reference) is correct, but
                       we should produce a warning. */
                    if (tiletry >= loopmax - 1 && !tileok && !tilefound) {
                        if (copy_unknown_tile_names) {
                            if (unknown_name_count == allocated_name_count) {
                                allocated_name_count += 8;
                                allocated_name_count *= 2;
                                unknown_tile_names =
                                    realloc(unknown_tile_names,
                                            allocated_name_count *
                                            sizeof *unknown_tile_names);
                                if (!unknown_tile_names)
                                    EPRINTN(
                                        "Error: Could not allocate memory\n");
                            }
                            unknown_tile_names[unknown_name_count] =
                                malloc(strlen(tp2) + 1);
                            if (!unknown_tile_names[unknown_name_count])
                                EPRINTN("Error: Could not allocate memory\n");
                            strcpy(unknown_tile_names[unknown_name_count], tp2);
                            tiletry = TILESEQ_COUNT + 1 + unknown_name_count;
                            unknown_name_count++;
                            tileok = 1;
                        } else {
                            /* note: tiletoken not tp2 because we want to
                               include any substitution in the error message */
                            printf("Note: deleting unknown reference '%s'\n",
                                   tiletoken);
                        }
                    }

                    /* If this is the tile we're looking for, add it to the
                       array. In the case of substar, add all existing matching
                       tiles to the array. As before, we loop unconditionally,
                       but with !substar, only have one iteration. */
                    if (tileok) {
                        int j;
                        for (j = (substar ? seen_tile_count - 1 : 0);
                             j >= 0; j--) {

                            unsigned long long jsub = substitution;
                            if (substar) {
                                /* The based-on tile must contain every
                                   substitution in 'substitution', no
                                   substitutions in 'substar_substitution', and
                                   use the same base tile. */
                                if (tiles_seen[j].tilenumber != tiletry)
                                    continue;
                                jsub = tiles_seen[j].substitution;
                                if ((jsub & substitution) != substitution)
                                    continue;
                                /* Also ignore tiles with ridiculous
                                   combinations of substitutons. */
                                if (mutually_exclusive_substitutions(
                                        jsub | substar_substitution))
                                    continue;
                            }

                            if (intilecount >= intile_alloccount) {
                                intile_alloccount += 4;
                                intile_alloccount *= 2;
                                intiles = realloc(intiles, intile_alloccount *
                                                  sizeof *intiles);
                                if (!intiles)
                                    EPRINTN("Error: "
                                            "Could not allocate memory\n");
                            }

                            tilefound = 1;
                            intiles[intilecount].tilenumber = tiletry;
                            intiles[intilecount].substitution =
                                jsub | substar_substitution;
                            intilecount++;
                        }
                    }
                }
            }

            free(tilename);

            /* Now we have to parse the tile data. dp is looking either at
               the data, or a closing paren just before it. */
            if (*dp == ')')
                dp++;

            /* Possible formats for the data:
               "\0{" followed by a literal image
               ": " followed by a hexadecimal number
               ": " followed by a cchar string
               absent, in which case we use file position

               No valid cchar string starts "0x", so we can just look at
               the next few characters to disambiguate. */
            uint32_t ii;
            double transformation_matrix[4][5];
            if (*dp == ':' && dp[1] == ' ') {
                if (using_filepos)
                    EPRINTN("Error: cannot mix map and tileset syntax\n");
                avoiding_filepos = 1;

                if (dp[2] == '0' && dp[3] == 'x') {
                    dp += 4;
                    ii = 0;
                    while (*dp) {
                        ii *= 16;
                        if (*dp >= '0' && *dp <= '9')
                            ii += *dp - '0';
                        else if (*dp >= 'a' && *dp <= 'f')
                            ii += *dp - 'a' + 10;
                        else if (*dp >= 'A' && *dp <= 'F')
                            ii += *dp - 'A' + 10;
                        else
                            EPRINTN("Error: bad hex number\n");
                        dp++;
                    }
                    /* Note: this assumes that start_of_reference_image is
                       always 0 for cchar-based tilesets. We verify this
                       fact later, when /writing/ the tileset. For now, it's
                       harmless, and we may not have enough information to
                       check (e.g. the first file is a list of name:ii pairs,
                       which is ambiguous). */
                    ii += start_of_reference_image;
                } else {
                    if (tileset_width == -1)
                        tileset_width = tileset_height = 0;
                    else if (tileset_width || tileset_height)
                        EPRINTN("Error: Image-based tilesets cannot have text "
                                "data\n");

                    /* Make a temporary copy of the data, so that we can
                       tokenize it. */
                    char *cchardef = malloc(len+1);
                    char *cp;
                    for (cp = cchardef, dp += 2; *dp; cp++, dp++)
                        *cp = *dp;
                    *cp = *dp;

                    bool first = 1;
                    unsigned long cchar =
                        (CCHAR_FGCOLOR_TRANSPARENT << 21) +
                        (CCHAR_BGCOLOR_TRANSPARENT << 26) +
                        (1UL << 31);

                    while ((cp = strtok(first ? cchardef : NULL, " "))) {
                        first = 0;

                        if (*cp == '\'') {

                            if (cp[1] == '\0') {
                                /* Mistokenization: ' ' tokenizes as two
                                   apostrophes, not one space surrounded by
                                   apostrophes. Just special-case this. */
                                cchar &= ~0x1fffffLU;
                                cchar |= ' ';

                                continue;
                            }

                            /* Unicode character. Convert it to UTF-32 then
                               back to UTF-8 and verify we have a match, to
                               disallow non-canonical encodings, etc.. */
                            char ob[7];
                            unsigned long uc = utf8towc(cp + 1);
                            wctoutf8(uc, ob);
                            int i;
                            for (i = 0; i < strlen(ob); i++) {
                                if (cp[i+1] != ob[i] || uc > 0x10ffff) {
                                    free(cchardef);
                                    EPRINTN("Error: bad Unicode character\n");
                                }
                            }
                            if (cp[i+1] != '\'') {
                                free(cchardef);
                                EPRINTN("Error: multiple characters given for "
                                        "one text cell\n");
                            }

                            cchar &= ~0x1fffffLU;
                            cchar |= uc;

                        } else if (!strcmp(cp, "invisible")) {
                            cchar &= ~0x1fffffLU;
                        } else if (!strcmp(cp, "basechar")) {
                            cchar &= ~0x1fffffLU;
                            cchar |= CCHAR_CHAR_BASE;
                        } else if (!strcmp(cp, "regular")) {
                            cchar &= ~(3UL << 30);
                        } else if (!strcmp(cp, "underlined")) {
                            cchar &= ~(3UL << 30);
                            cchar |= 1UL << 30;
                        } else if (!strcmp(cp, "same_underline")) {
                            cchar &= ~(3UL << 30);
                            cchar |= 2UL << 30;
                        } else if (!strcmp(cp, "base_underline")) {
                            cchar &= ~(3UL << 30);
                            cchar |= 3UL << 30;
                        } else if (!strcmp(cp, "samebg")) {
                            cchar &= ~(15UL << 26);
                            cchar |= CCHAR_BGCOLOR_TRANSPARENT << 26;
                        } else if (!strcmp(cp, "basebg")) {
                            cchar &= ~(15UL << 26);
                            cchar |= CCHAR_BGCOLOR_BASE << 26;
                        } else {

                            /* It should be a color code. */
                            bool bg = false;
                            bool found = false;
                            if (cp[0] == 'b' && cp[1] == 'g') {
                                cp += 2;
                                bg = true;
                            }

                            int i;
                            for (i = 0; i < (bg ? CCHAR_BGCOLOR_TRANSPARENT :
                                             CCHAR_COLOR_COUNT); i++) {
                                if (!strcmp(cchar_color_names[i], cp)) {
                                    found = true;
                                    if (bg) {
                                        cchar &= ~(15UL << 26);
                                        cchar |= i << 26;
                                    } else {
                                        cchar &= ~(31UL << 21);
                                        cchar |= i << 21;
                                    }
                                }
                            }

                            if (!found) {
                                /* Note: this leaks cchardef; probably not a big
                                   deal as we're about to exit anyway */
                                if (bg)
                                    cp -= 2;
                                EPRINT("Error: Unknown text description '%s'\n",
                                       cp);
                            }
                        }
                    }

                    free(cchardef);
                    ii = cchar;
                }
            } else if (!*dp) {
                if (dp - data < size - 1 && dp[1] == '(') {
                    /* This is an image transformation. */
                    if (using_filepos)
                        EPRINTN("Error: cannot mix map and tileset syntax\n");
                    avoiding_filepos = 1;

                    if (tileset_width == 0 || tileset_height == 0)
                        EPRINTN("Error: Text-based tilesets cannot use image "
                                "transformations\n");
                    else if (tileset_width < 0 || tileset_height < 0)
                        EPRINTN("Error: Image transformation seen without "
                                "known tile dimensions");

                    int nthrow = 0;
                    for (dp += 3; dp >= data + size || *dp != ')';) {
                        if (dp >= data + size)
                            EPRINTN("Error: unmatched '('\n");
                        if (nthrow > 4)
                            EPRINTN("Error: image transformation has more than "
                                    "4 channels\n");
                        double *t = transformation_matrix[nthrow];
                        if (sscanf((const char *) dp, "%lf,%lf,%lf,%lf,%lf",
                                   t + 0, t + 1, t + 2, t + 3, t + 4) != 5)
                            EPRINTN("Error: could not parse image "
                                    "transformation\n");
                        dp = (uint8_t *) memchr(dp, 0, data + size - dp) + 1;
                        nthrow++;
                    }
                    if (nthrow == 3) {
                        transformation_matrix[3][0] = 0.0;
                        transformation_matrix[3][1] = 0.0;
                        transformation_matrix[3][2] = 0.0;
                        transformation_matrix[3][3] = 1.0;
                        transformation_matrix[3][4] = 0.0;
                        nthrow++;
                    }
                    if (nthrow != 4)
                        EPRINTN("Error: image transformation has fewer than "
                                "3 channels\n");

                    /* We can't allocate the image right now; we need a separate
                       image for each tile that this tile definition is based
                       on. So initialize ii to an invalid value and fix the
                       situation later. */
                    ii = (uint32_t) -1;
                    i = dp - data;

                } else if (dp - data < size - 1 && dp[1] == '{') {
                    if (using_filepos)
                        EPRINTN("Error: cannot mix map and tileset syntax\n");
                    avoiding_filepos = 1;

                    /* Allocate some memory to store the new image in. If this
                       is the very first image, we might not know how large it
                       is, but it can't possibly have any more pixels than size,
                       and overallocating isn't an issue if we only do it once.
                       If it isn't, we know how much memory to allocate. */
                    int pixelcount;
                    if (tileset_width == -1)
                        pixelcount = size;
                    else if (tileset_width == 0 || tileset_height == 0)
                        EPRINTN("Error: Text-based tilesets cannot have image "
                                "data\n");
                    else
                        pixelcount = tileset_width * tileset_height;

                    if (!filekeywidth)
                        EPRINTN("Error: Textual images need a palette\n");

                    pixel *image = malloc(pixelcount * sizeof (pixel));
                    int x = 0, y = -1, w = tileset_width, c = 0;
                    lineno++;
                    for (dp += 2; dp >= data + size || *dp != '}'; dp++) {
                        if (dp >= data + size)
                            EPRINTN("Error: unmatched '{'\n");
                        if (*dp == ' ')
                            continue;
                        if (*dp == 0) {
                            y++;
                            lineno++;
                            if (w == -1 && y)
                                w = x;
                            else if (w != x && y)
                                EPRINTN("Error: Image width is not consistent\n");
                            if (!w)
                                EPRINTN("Error: Image has zero width\n");
                            x = 0;
                            continue;
                        }
                        if (y < 0)
                            EPRINTN("Error: Junk after opening brace\n");
                        if (tileset_height != -1 && y >= tileset_height)
                            EPRINTN("Error: Image height is not consistent\n");
                        if (x > w && w != -1)
                            EPRINTN("Error: Image width is not consistent\n");

                        int pk = palette_key_to_int(*dp);
                        if (pk < 0)
                            EPRINT("Error: Invalid '%c' in image\n", *dp);
                        if (filekeywidth == 2) {
                            pk *= 64;
                            int pk2 = palette_key_to_int(*++dp);
                            if (pk2 < 0)
                                EPRINT("Error: Invalid '%c' in image\n", *dp);
                            pk += pk2;
                        }

                        if (!filepalettevalid[pk])
                            EPRINTN("Error: Undefined palette key\n");

                        if (filepalettechannels == 4)
                            image[y * w + x] = filepalette[pk];
                        else if (c == 0)
                            image[y * w + x].r = filepalette[pk].r;
                        else if (c == 1)
                            image[y * w + x].g = filepalette[pk].r;
                        else if (c == 2)
                            image[y * w + x].b = filepalette[pk].r;
                        else if (c == 3)
                            image[y * w + x].a = filepalette[pk].r;

                        c += filepalettechannels;
                        if (c == 4) {
                            c = 0;
                            x++;
                        }
                    }

                    if (y == 0)
                        EPRINTN("Error: Image has zero width\n");

                    if (tileset_height == -1)
                        tileset_height = y;
                    else if (y != tileset_height)
                        EPRINTN("Error: Image height is not consistent\n");

                    if (tileset_width == -1)
                        tileset_width = w;
                    else if (w != tileset_width)
                        EPRINTN("Error: Image width is not consistent\n");

                    images_seen = realloc(images_seen, (seen_image_count + 1)
                                          * sizeof *images_seen);
                    if (!images_seen)
                        EPRINTN("Error allocating memory for tile images\n");

                    ii = seen_image_count;
                    images_seen[seen_image_count++] = image;

                    i = dp - data;

                } else {
                    if (avoiding_filepos)
                        EPRINTN("Error: cannot mix map and tileset syntax\n");
                    using_filepos = 1;
                    ii = filepos++ + start_of_reference_image;
                }
            } else {
                EPRINTN("Error: junk after tile name\n");
            }

            /* We now have our tile. */
            if (seen_tile_count + intilecount > allocated_tile_count) {
                allocated_tile_count += 8;
                allocated_tile_count *= 2;
                if (allocated_tile_count < seen_tile_count + intilecount)
                    allocated_tile_count = seen_tile_count + intilecount;
                tiles_seen = realloc(tiles_seen, allocated_tile_count *
                                     sizeof *tiles_seen);
                if (!tiles_seen)
                    EPRINTN("Error: not enough memory for tiles\n");
            }

            int last_basetile = -1;
            int i;
            for (i = 0; i < intilecount; i++) {

                /* We have to resolve any basechar, basefg, basebg, or
                   base_underline directives, or image transformations, in
                   ii. We do this by looking for the based-on tile
                   unconditionally, but only erroring out on failure to find
                   it if we actually need it. */
                int ii2 = ii;
                int j;
                unsigned long long base_substitution = 0;
                if (substar)
                    base_substitution = intiles[i].substitution &
                        (~substar_substitution);
                for (j = seen_tile_count - 1; j >= 0; j--)
                    if (tiles_seen[j].substitution ==
                        base_substitution &&
                        tiles_seen[j].tilenumber ==
                        intiles[i].tilenumber)
                        break;

#define ENSURE_J if (j < 0) EPRINTN("Error: 'base...' with no based-on tile\n")

                if (!tileset_width) {

                    if ((ii & 0x1ffffLU) == CCHAR_CHAR_BASE) {
                        ENSURE_J;
                        ii2 &= ~0x1ffffLU;
                        ii2 |= tiles_seen[j].image_index & 0x1ffffLU;
                    }

                    if (((ii >> 21) & 31) == CCHAR_FGCOLOR_BASE) {
                        ENSURE_J;
                        ii2 &= ~(31LU << 21);
                        ii2 |= tiles_seen[j].image_index & (31LU << 21);
                    }

                    if (((ii >> 26) & 16) == CCHAR_BGCOLOR_BASE) {
                        ENSURE_J;
                        ii2 &= ~(15LU << 26);
                        ii2 |= tiles_seen[j].image_index & (15LU << 26);
                    }

                    if (((ii >> 30) & 3) == 3) {
                        ENSURE_J;
                        ii2 &= ~(3LU << 30);
                        ii2 |= tiles_seen[j].image_index & (3LU << 30);
                    }

                } else if (ii == (uint32_t) -1) {
                    if (j < 0)
                        EPRINTN("Error: image transformation with "
                                "no based-on tile\n");

                    if (tiles_seen[j].image_index == last_basetile) {
                        /* Reuse an existing tile if we're doing the same
                           transformation on the same base image as the last
                           tile we saw (base images will group together if we're
                           using a map-based or otherwise semicolon-based
                           input). Not perfect, but hopefully good enough for
                           now, and we'd need some sort of hash table to get it
                           perfect. (Failure simply causes the output to have a
                           larger file size, rather than producing incorrect
                           output.) */

                        ii2 = seen_image_count - 1;

                    } else {
                        last_basetile = tiles_seen[j].image_index;

                        pixel *baseimage =
                            images_seen[tiles_seen[j].image_index];
                        pixel *image = malloc(tileset_width * tileset_height *
                                              sizeof (pixel));
                        images_seen = realloc(images_seen,
                                              (seen_image_count + 1) *
                                              sizeof *images_seen);
                        if (!images_seen)
                            EPRINTN("Error allocating memory for tile "
                                    "images\n");

                        int nthpixel;
                        for (nthpixel = 0;
                             nthpixel < tileset_width * tileset_height;
                             nthpixel++) {
                            /* This would be a great place for a nested
                               function, but those aren't properly portable. So
                               instead of a partial application on
                               baseimage[nthpixel], we just mention it each
                               time. */
                            image[nthpixel].r = image_transformation(
                                transformation_matrix[0], baseimage[nthpixel]);
                            image[nthpixel].g = image_transformation(
                                transformation_matrix[1], baseimage[nthpixel]);
                            image[nthpixel].b = image_transformation(
                                transformation_matrix[2], baseimage[nthpixel]);
                            image[nthpixel].a = image_transformation(
                                transformation_matrix[3], baseimage[nthpixel]);
                        }

                        ii2 = seen_image_count;
                        images_seen[seen_image_count++] = image;
                    }
                }

                intiles[i].image_index = ii2;
                tiles_seen[seen_tile_count++] = intiles[i];
            }
            free(intiles);
        }
    }

    if (palette_locking)
    {
        if (palette_locked) {
            /* not EPRINT; this has nothing to do with the current line */
            fprintf(stderr, "Error: attempt to lock the palette twice\n");
            return 0;
        }

        /* Copy the palette we've been working from to the global palette. */
        palettesize = 0;
        palettechannels = filepalettechannels;

        int i;
        for (i = 0; i < MAX_PALETTE_SIZE; i++) {
            if (filepalettevalid[i])
                palette[palettesize++] = filepalette[i];
        }

        palette_locking = 0;
        palette_locked = 1;
    }

    return 1;
}
