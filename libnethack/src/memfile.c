/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>
#include <zlib.h>

#ifdef IS_BIG_ENDIAN
static unsigned short
host_to_le16(unsigned short x)
{
    return _byteswap16(x);
}

static unsigned int
host_to_le32(unsigned int x)
{
    return _byteswap32(x);
}

static unsigned long long
host_to_le64(unsigned long long x)
{
    return _byteswap64(x);
}

static unsigned short
le16_to_host(unsigned short x)
{
    return _byteswap16(x);
}

static unsigned int
le32_to_host(unsigned int x)
{
    return _byteswap32(x);
}

static unsigned long long
le64_to_host(unsigned long long x)
{
    return _byteswap64(x);
}

#else
static unsigned short
host_to_le16(unsigned short x)
{
    return x;
}

static unsigned int
host_to_le32(unsigned int x)
{
    return x;
}

static unsigned long long
host_to_le64(unsigned long long x)
{
    return x;
}

static unsigned short
le16_to_host(unsigned short x)
{
    return x;
}

static unsigned int
le32_to_host(unsigned int x)
{
    return x;
}

static unsigned long long
le64_to_host(unsigned long long x)
{
    return x;
}
#endif

/* For debugging save file memory usage. There is no way for this value to be
   anything other than NULL during normal usage. However, it can be set to an
   actual file via use of a debugger (the "volatile" ensures this will work
   correctly, it pretty much literally means "this variable can change behind
   your back"), in which case information about what's consuming space in a
   save diff will be logged to the file in question. */
static FILE *volatile debuglog = NULL;

static void mdiffwrite(struct memfile *, const void *, unsigned int);

/* Creating and freeing memory files */
void
mnew(struct memfile *mf, struct memfile *relativeto)
{
    int i;

    static const char diffheader[2] = {MDIFF_HEADER_0, MDIFF_HEADER_1_BETA3};

    mf->buf = mf->diffbuf = NULL;
    mf->len = mf->pos = mf->difflen = mf->diffpos = mf->relativepos = 0;
    mf->relativeto = relativeto;

    mf->pending_edits = mf->pending_copies = mf->pending_seeks = 0;
    mf->last_command = (struct mdiff_command_instance)
        {.command = mdiff_copy, .arg1 = 0, .arg2 = 0};
    mf->coord_relative_to = 0;
    mf->mon_coord_hint = -2;
    for (i = 0; i < mdiff_command_count; i++) {
        mf->mdiff_command_mru[i] = i;
    }

    mdiffwrite(mf, diffheader, 2);

    for (i = 0; i < MEMFILE_HASHTABLE_SIZE; i++)
        mf->tags[i] = 0;
    mf->last_tag = 0;
}

/* Allocates to as a deep copy of from. */
void
mclone(struct memfile *to, const struct memfile *from)
{
    int i;

    *to = *from;

    if (from->buf) {
        to->buf = malloc(from->len);
        memcpy(to->buf, from->buf, from->len);
    }
    if (from->diffbuf) {
        to->diffbuf = malloc(to->difflen);
        memcpy(to->diffbuf, from->diffbuf, from->difflen);
    }

    for (i = 0; i < MEMFILE_HASHTABLE_SIZE; i++) {
        struct memfile_tag *fromtag, **totag;

        fromtag = from->tags[i];
        totag = &(to->tags[i]);

        while (fromtag) {
            *totag = malloc(sizeof (struct memfile_tag));
            **totag = *fromtag;
            fromtag = fromtag->next;
            totag = &((*totag)->next);
        }

        *totag = 0;
    }
}

void
mfree(struct memfile *mf)
{
    int i;

    free(mf->buf);
    mf->buf = 0;
    free(mf->diffbuf);
    mf->diffbuf = 0;
    for (i = 0; i < MEMFILE_HASHTABLE_SIZE; i++) {
        struct memfile_tag *tag, *otag;

        for ((tag = mf->tags[i]), (otag = NULL); tag; tag = tag->next) {
            free(otag);
            otag = tag;
        }
        free(otag);
        mf->tags[i] = 0;
    }
}

/* Functions for writing to a memory file.
   There are two sorts of memory files: linear files, which work like
   ordinary filesystem files, and diff files, which are recorded
   relative to a parent file. As well as containing data, memfiles
   also contain "tags" for the purpose of making diffing easier; these
   aren't saved to disk as they can always be reconstructed and anyway
   they improve efficiency rather than being required for correctness. */

static void
expand_memfile(struct memfile *mf, long newlen)
{
    if (mf->len < newlen) {
        mf->len = (newlen & ~4095L) + 4096L;
        mf->buf = realloc(mf->buf, mf->len);
    }
}

/* Returns a pointer to the internals of a memory file (analogous to how mmap()
   works on regular files). There is no mmunmap; rather, the pointer is only
   guaranteed to be valid up to the next call to a memory file manipulation
   function. If you plan to write to the resulting pointer, mf->relativeto must
   be NULL (i.e. not a diff-based file); if all you're doing is reading, any
   sort of memory file will work. The memory file pointer, mf->pos, will move
   to the end of the mapped area if it's within or before the mapped area
   (because it's used to measure the length of the file). */
void *
mmmap(struct memfile *mf, long len, long off)
{
    expand_memfile(mf, len + off);
    if (len + off > mf->pos)
        mf->pos = len + off;
    return mf->buf + off;
}

void
mwrite(struct memfile *mf, const void *buf, unsigned int num)
{
    expand_memfile(mf, mf->pos + num);
    memcpy(&mf->buf[mf->pos], buf, num);

    if (!mf->relativeto) {
        mf->pos += num;
    } else {
        /* calculate and record the diff as well */
        while (num--) {
            if (mf->relativepos < mf->relativeto->pos &&
                mf->buf[mf->pos] == mf->relativeto->buf[mf->relativepos]) {

                if (mf->pending_seeks || mf->pending_edits)
                    mdiffflush(mf, 0);

                mf->pending_copies++;

            } else {

                /* Note that mdiffflush is responsible for writing the actual
                   data that was edited, once we have a complete run of it. So
                   there's no need to record the data anywhere but in buf. */
                if (mf->pending_seeks)
                    mdiffflush(mf, 0);

                mf->pending_edits++;
            }
            mf->pos++;
            mf->relativepos++;
        }
    }
}

void
mwrite8(struct memfile *mf, int8_t value)
{
    mwrite(mf, &value, 1);
}

void
mwrite16(struct memfile *mf, int16_t value)
{
    int16_t le_value = host_to_le16(value);

    mwrite(mf, &le_value, 2);
}

void
mwrite32(struct memfile *mf, int32_t value)
{
    int32_t le_value = host_to_le32(value);

    mwrite(mf, &le_value, 4);
}

void
mwrite64(struct memfile *mf, int64_t value)
{
    int64_t le_value = host_to_le64(value);

    mwrite(mf, &le_value, 8);
}

void
store_mf(int fd, struct memfile *mf)
{
    int len, left, ret;

    len = left = mf->pos;
    while (left) {
        ret = write(fd, &mf->buf[len - left], left);
        if (ret == -1)  /* error */
            goto out;
        left -= ret;
    }

out:
    mfree(mf);
    mnew(mf, NULL);
}

/* Writing to the diff portion of memfiles; more complicated than
   regular writes, because it's RLEd. */
static void
mdiffwrite(struct memfile *mf, const void *buf, unsigned int num)
{
    boolean do_realloc = FALSE;

    while (mf->difflen < mf->diffpos + num) {
        mf->difflen += 4096;
        do_realloc = TRUE;
    }

    if (do_realloc)
        mf->diffbuf = realloc(mf->diffbuf, mf->difflen);

    memcpy(&mf->diffbuf[mf->diffpos], buf, num);
    mf->diffpos += num;
}

static void
mdiffwrite8(struct memfile *mf, uint8_t v)
{
    mdiffwrite(mf, &v, 1);
}

/* Data on the various commands. arg1 holds the minimum size of the flexible
   argument; arg2 holds the minimum size of the fixed-size argument. A negative
   argument means that that argument is signed. Also holds the name, for debug
   purposes. */
struct mdiff_command_size {
    int arg1;
    int arg2;
    const char *name;
};
static const struct mdiff_command_size mdiff_command_sizes[] = {
    [mdiff_copyedit]  = {12,  2, "copyedit"},
    [mdiff_copy]      = {6,   0, "copy"},
    [mdiff_edit]      = {4,   0, "edit"},
    [mdiff_seek]      = {-8,  0, "seek"},
    [mdiff_wider]     = {0,   0, "wider"},
    [mdiff_eof]       = {12,  0, "eof"},
    [mdiff_coord]     = {1,   3, "coord"},
    [mdiff_erase]     = {7,   3, "erase"},
    [mdiff_copyedit1] = {4,   0, "copyedit1"},
    [mdiff_increment] = {12,  0, "increment"},
    [mdiff_eof_crc32] = {12, 32, "eof_crc32"},
    [mdiff_rle]       = {2,   0, "rle"},
};

static_assert(ARRAY_SIZE(mdiff_command_sizes) == mdiff_command_count,
              "not all mdiff_commands have sizes specified");

/* Calculates the MRU encoding of a particular command. */
static unsigned long long
mdiffcmd_mru(const struct memfile *mf, enum mdiff_command cmd, int widenings,
             int *usedbits)
{
    /* Find the location in the list of the command and mdiff_wider. */
    int cmdpos = -1, widerpos = -1;
    enum mdiff_command i;
    for (i = 0; i < mdiff_command_count; i++) {
        if (mf->mdiff_command_mru[i] == cmd)
            cmdpos = i;
        if (mf->mdiff_command_mru[i] == mdiff_wider)
            widerpos = i;
    }
    if (cmdpos < 0 || widerpos < 0)
        panic("bad mdiff_command %d", (int)cmd);

    unsigned long long rv = 0;
    *usedbits = 0;
    int encodewidth, encodebase;

    /* Handle widenings. */
    if (widenings) {

        /* The encodewidth is the width of the '11..10' prefix, and also the
           width of the other half of the MRU encoding. */
        for (encodewidth = 1, encodebase = 0;
             (widerpos - encodebase) >= (1 << encodewidth);
             encodebase += (1 << encodewidth), encodewidth++)
            ;

        /* For example, with encodewidth 4: */
        rv++;                          /* 1         */
        rv <<= encodewidth;            /* 10000     */
        rv -= 2;                       /*  1110     */
        rv <<= encodewidth;            /*  11100000 */
        rv += (widerpos - encodebase); /*  1110xxxx */
        *usedbits += encodewidth * 2;

        /* If we have more than one widening, the others are encoded as 00. */
        rv <<= (widenings - 1) * 2;
        *usedbits += (widenings - 1) * 2;

        /* If the command was before mdiff_wider in the list, it's now after
           it. */
        if (cmdpos < widerpos)
            cmdpos++;
    }

    /* Handle the command itself. This is basically the same as above. */
    for (encodewidth = 1, encodebase = 0;
         (cmdpos - encodebase) >= (1 << encodewidth);
         encodebase += (1 << encodewidth), encodewidth++)
        ;

    rv++;
    rv <<= encodewidth;
    rv -= 2;
    rv <<= encodewidth;
    rv += (cmdpos - encodebase);
    *usedbits += encodewidth * 2;

    return rv;
}

/* Calculates how many bytes the given command will take up, and optionally how
   many mdiff_wider prefixes will be needed for it. Doesn't include any bytes
   of edit data that might be involved.

   Returns -1 if there's not enough room for the given arg2. */
static int
mdiffcmd_width(const struct memfile *mf, struct mdiff_command_instance *mdci,
               int *widenings)
{
    int w = 0;

    /* Some commands can be omitted altogether. */
    if ((mdci->command == mdiff_copy && mdci->arg1 == 0)  ||
        (mdci->command == mdiff_edit && mdci->arg1 == -1) ||
        (mdci->command == mdiff_seek && mdci->arg1 == 0)  ||
        (mdci->command == mdiff_command_count))
        return 0;

    /* Check that arg2 fits. */
    if (mdci->arg2 >= (1ULL << mdiff_command_sizes[mdci->command].arg2))
        return -1;

    /* Find an unsigned value with the same width as arg1.
       If arg1 is unsigned, this is just arg1.
       If arg1 is signed, this is abs(arg1) * 2 (i.e. one more bit than the
       absolute value of arg1, because we need a sign bit). */
    unsigned long long arg1width;
    if (mdiff_command_sizes[mdci->command].arg1 < 0)
        arg1width = 2 * (mdci->arg1 < 0 ? -mdci->arg1 : mdci->arg1);
    else
        arg1width = mdci->arg1;

    for (;;) {
        /* Calculate the number of bits to encode the command with w
           widenings. */
        int usedbits;
        (void) mdiffcmd_mru(mf, mdci->command, w, &usedbits);
        /* arg2 also uses up some bits, if present. */
        usedbits += mdiff_command_sizes[mdci->command].arg2;

        /* Calculate the number of bits of flexible data we have available.
           This is the minimum in mdiff_command_sizes, plus any spare that
           round up to a whole byte. */
        int arg1size = abs(mdiff_command_sizes[mdci->command].arg1);
        arg1size += w * 8;
        if ((arg1size + usedbits) % 8)
            arg1size += 8 - ((arg1size + usedbits) % 8);

        if (arg1width < (1ULL << arg1size)) {
            /* It fits. */
            if (widenings)
                *widenings = w;
            return (usedbits + arg1size) / 8;
        } else
            w++; /* It doesn't fit. */
    }
}

static void
mdiffwritecmd(struct memfile *mf, struct mdiff_command_instance *mdci)
{
    int widenings, usedbits, usedbytes, i;

    /* Count the number of widenings we need. */
    usedbytes = mdiffcmd_width(mf, mdci, &widenings);
    if (usedbytes < 0)
        panic("arg2 too large in mdiffwritecmd");
    if (usedbytes == 0)
        return; /* nothing to do */

    /* Find the encoding of this command. */
    unsigned long long encoding = mdiffcmd_mru(mf, mdci->command,
                                               widenings, &usedbits);

    /* Add the encoding of arg2. */
    encoding <<= mdiff_command_sizes[mdci->command].arg2;
    usedbits +=  mdiff_command_sizes[mdci->command].arg2;
    encoding |=  mdci->arg2;

    /* Add the encoding of arg1. */
    usedbits = usedbytes * 8 - usedbits;
    encoding <<= usedbits;
    unsigned long long arg1_2c = mdci->arg1 & ((1ULL << usedbits) - 1);
    encoding |= arg1_2c;

    i = usedbytes;
    while (i--)
        mdiffwrite8(mf, encoding >> (i * 8));

    if (debuglog) {
        fprintf(debuglog, "%s %" PRIdLEAST64 " %" PRIuLEAST64 " (",
                mdiff_command_sizes[mdci->command].name,
                (int_least64_t)mdci->arg1, (uint_least64_t)mdci->arg2);

        i = usedbytes*8;
        while (i--) {
            fprintf(debuglog, "%d", (int)(1 & (encoding >> i)));
            if (i && !(i%8))
                fprintf(debuglog, " ");
        }
    }

    /* Record that this command appears in the diff. */
    mf->last_command = *mdci;

    if (widenings) {
        for (i = 0; mf->mdiff_command_mru[i] != mdiff_wider; i++)
            ;
        while (i > 0) {
            mf->mdiff_command_mru[i] = mf->mdiff_command_mru[i-1];
            i--;
        }
        mf->mdiff_command_mru[0] = mdiff_wider;
    }

    for (i = 0; mf->mdiff_command_mru[i] != mdci->command; i++)
        if (i == mdiff_command_count)
            panic("unknown mdiff command %d", (int)mdci->command);
    while (i > 0) {
        mf->mdiff_command_mru[i] = mf->mdiff_command_mru[i-1];
        i--;
    }
    mf->mdiff_command_mru[0] = mdci->command;

    /* Edit and copyedit commands require additional data appended. */
    int editlen = 0;
    if (mdci->command == mdiff_edit)
        editlen = mdci->arg1 + 1;
    if (mdci->command == mdiff_copyedit)
        editlen = mdci->arg2 + 1;
    if (mdci->command == mdiff_copyedit1)
        editlen = 1;
    if (editlen)
        mdiffwrite(mf, mf->buf + mf->pos - editlen, editlen);

    if (debuglog) {
        if (editlen)
            fprintf(debuglog, " + %d data)  ", editlen);
        else
            fprintf(debuglog, ")  ");
    }
}

static void
mdiff_checklen(
    struct memfile *mf, struct mdiff_command_instance best[2],
    int *bestlen, int widthmod,
    enum mdiff_command cmd1, long long arg11, unsigned long long arg21,
    enum mdiff_command cmd2, long long arg12, unsigned long long arg22)
{
    struct mdiff_command_instance attempt1 = {cmd1, arg11, arg21};
    struct mdiff_command_instance attempt2 = {cmd2, arg12, arg22};
    int width1 = mdiffcmd_width(mf, &attempt1, NULL);
    /* TODO: This is only an approximation; the command might be one byte
       wider due to width1 pushing it further down the MRU list, or one
       byte shorter if both need widening and that brings widening up on
       the MRU list */
    int width2 = mdiffcmd_width(mf, &attempt2, NULL);
    if (width1 < 0 || width2 < 0)
        return;

    if (width1 + width2 + widthmod < *bestlen) {
        *bestlen = width1 + width2 + widthmod;
        best[0] = attempt1;
        best[1] = attempt2;
    }
}

void
mdiffflush(struct memfile *mf, boolean eof)
{
    /* Find the command with the shortest encoding. */
    struct mdiff_command_instance best[2];
    int bestlen = INT_MAX;

    /* For protection against mistakes in the diff algorithm, add a checksum at
       the end of the diff, that checksums the expected output from the
       diff. (We can go back to using a non-checksummed diff later, once this
       code seems to be working, if saving the extra space in the save file is
       important; mdiff_eof has a shorter encoding and the code for it has been
       tested.) */
    uLong crc = 0;
    if (eof)
        crc = crc32(crc32(0L, Z_NULL, 0), (Bytef *)mf->buf, mf->pos);

    if (mf->pending_seeks) {
        best[0] = (struct mdiff_command_instance)
            {.command = mdiff_seek, .arg1 = mf->pending_seeks};
        best[1] = (struct mdiff_command_instance)
            {.command = mdiff_command_count};
    } else {

        /* Work out information about what sort of commands are possible. */
        const int copies     = mf->pending_copies;
        const int edits      = mf->pending_edits;
        const int first_copy = mf->pos - copies - edits;
        uint8_t *first_edit =
            (uint8_t *)(mf->buf + mf->pos - edits);
        uint8_t *orig_edit  = NULL;
        if (mf->relativeto && mf->relativepos >= edits)
            orig_edit = (uint8_t *)(mf->relativeto->buf +
                                    mf->relativepos - edits);

        /* Can we interpret the edits as a 1-square movement on a grid? If so,
           and it's only a move in one coordinate, should we interpret it as an
           x or y coordinate? Also establish the first location we'll update
           (which might be one of the copies, updated by adding 0). */
        struct nh_cmd_arg arg;
        arg.dir = DIR_NONE;
        int coord_pos = mf->pos - edits;
        boolean prefer_x_to_y =
            mf->mon_coord_hint == mf->pos - 1 ||
            ((coord_pos - mf->coord_relative_to) % SAVE_SIZE_MONST) == 0;
        if (edits == 2 && first_edit[1] != orig_edit[1])
            arg_from_delta((schar)(first_edit[0] - orig_edit[0]),
                           (schar)(first_edit[1] - orig_edit[1]), 0, &arg);
        else if (edits == 1 && prefer_x_to_y)
            arg_from_delta((schar)(first_edit[0] - orig_edit[0]), 0, 0, &arg);
        else if (edits == 1 && copies >= 1) {
            arg_from_delta(0, (schar)(first_edit[0] - orig_edit[0]), 0, &arg);
            coord_pos--;
        }
        boolean prefer_coord = mf->mon_coord_hint == mf->pos -
            (edits == 1 && prefer_x_to_y ? 1 : 2);

        int crt_large = (coord_pos - first_copy) / SAVE_SIZE_MONST;
        int crt_small = (coord_pos - first_copy) % SAVE_SIZE_MONST;

        /* Encodings which should be preferred in the case of a tie in length
           come first. */
        if (1)
            mdiff_checklen(mf, best, &bestlen, edits + 7 * !!eof,
                           mdiff_copyedit,      copies,    edits - 1,
                           mdiff_command_count, 0,         0);
        if (1)
            mdiff_checklen(mf, best, &bestlen, edits + 7 * !!eof,
                           mdiff_copy,          copies,    0,
                           mdiff_edit,          edits - 1, 0);
        if (eof && !edits)
            mdiff_checklen(mf, best, &bestlen, 0,
                           mdiff_eof_crc32,     copies,    crc,
                           mdiff_command_count, 0,         0);
        if (onlynul(first_edit, edits))
            mdiff_checklen(mf, best, &bestlen, 0,
                           mdiff_erase,         copies,    edits - 3,
                           mdiff_command_count, 0,         0);
        if (arg.dir != DIR_NONE &&
            ((coord_pos - mf->coord_relative_to) % SAVE_SIZE_MONST) == 0)
            mdiff_checklen(mf, best, &bestlen, 7 * !!eof - 2 * prefer_coord,
                           mdiff_coord,         crt_large, arg.dir,
                           mdiff_command_count, 0,         0);
        if (arg.dir != DIR_NONE && crt_small != 0)
            mdiff_checklen(mf, best, &bestlen, 7 * !!eof - 2 * prefer_coord,
                           mdiff_copy,          crt_small, 0,
                           mdiff_coord,         crt_large, arg.dir);
        if (arg.dir != DIR_NONE && crt_large > 0)
            mdiff_checklen(mf, best, &bestlen, 7 * !!eof - 2 * prefer_coord,
                           mdiff_copy, SAVE_SIZE_MONST + crt_small, 0,
                           mdiff_coord,    -1 + crt_large, arg.dir);

        if (edits == 1 && (uint8_t)(*first_edit - *orig_edit) == 1)
            mdiff_checklen(mf, best, &bestlen, 7 * !!eof,
                           mdiff_increment,     copies,    0,
                           mdiff_command_count, 0,         0);
        if (edits == 1)
            mdiff_checklen(mf, best, &bestlen, 7 * !!eof,
                           mdiff_copyedit1,     copies,    0,
                           mdiff_command_count, 0,         0);

        /* TODO: rle */
        (void) orig_edit;
    }

    if (debuglog) {
        fprintf(debuglog, "seek %ld copy %ld edit %ld bytes [",
                mf->pending_seeks, mf->pending_copies, mf->pending_edits);
        int i;
        for (i = 0; i < mf->pending_edits; i++) {
            uint8_t newc = mf->buf[mf->pos - mf->pending_edits + i];
            uint8_t oldc = 0;
            if (mf->relativeto && mf->relativepos >= mf->pending_edits)
                oldc = mf->relativeto->buf[mf->relativepos -
                                           mf->pending_edits + i];
            fprintf(debuglog, "%02x", (unsigned)(uint8_t)(newc - oldc));
            if (i == 3 && mf->pending_edits > 4) {
                fprintf(debuglog, "...");
                break;
            }
        }
        /* "last copy" = the copy ends just before this point (so presumably
           this point will be edited or seeked away) */
        fprintf(debuglog, "] pos %d, last copy %d:%08lx%+d anchor %d\n> ",
                mf->pos,
                mf->last_tag ? mf->last_tag->tagtype : -1,
                mf->last_tag ? mf->last_tag->tagdata : 0,
                mf->pos - (int)mf->pending_edits -
                (mf->last_tag ? mf->last_tag->pos : 0),
                mf->coord_relative_to);
    }

    mdiffwritecmd(mf, best + 0);
    mdiffwritecmd(mf, best + 1);
    if (eof && best[0].command != mdiff_eof && best[1].command != mdiff_eof &&
        best[0].command != mdiff_eof_crc32 &&
        best[1].command != mdiff_eof_crc32)
        mdiffwritecmd(mf, &(struct mdiff_command_instance)
                      {mdiff_eof_crc32, 0, crc});

    /* Update coord_relative_to, if necessary. If we see it in the first
       position, we calculate the value of pos as of the previous flush, then
       work forwards from there. If we see it in the second position, we can
       just use the value of pos right now. */
    if (best[0].command == mdiff_copy && best[0].arg1)
        mf->coord_relative_to = mf->pos - mf->pending_edits -
            mf->pending_copies + best[0].arg1;
    if (best[1].command == mdiff_copy && best[1].arg1)
        mf->coord_relative_to = mf->pos;

    if (debuglog)
        fprintf(debuglog, "\n");

    mf->pending_seeks = 0;
    mf->pending_edits = 0;
    mf->pending_copies = 0;
}

void
mhint_mon_coordinates(struct memfile *mf)
{
    mf->mon_coord_hint = mf->pos;
}

/* Given a diff and the memfile (diff_base) it was made against, write the file
   that must have been used to create the diff at the end of new_memfile (which
   will typically be empty on entry to this function, and must be in the default
   diffing state, i.e. has never been written to with relative_to set).

   If something goes wrong, calls errfunction with an error message and diff as
   arguments. */
void
mdiffapply(char *diff, long difflen, struct memfile *diff_base,
           struct memfile *new_memfile,
           void (*errfunction)(const char *, char *))
{
    char *mfp;
    unsigned char *bufp = (unsigned char *)diff;
    int i;
    long dbpos = 0;

    /* SAVEBREAK (4.3-beta1 -> 4.3-beta3): this is unconditionally true in
       4.3-beta3 save files, so we can remove the if statement and get rid of
       the rest of the function, which is backwards compatibility code. */
    if (bufp[0] == MDIFF_HEADER_0 && bufp[1] == MDIFF_HEADER_1_BETA3) {

        bufp++; /* skip the header; note that we maintain bufp pointing to the
                   last byte processed, not the first byte unprocessed, because
                   it makes the loop easier to write */

        int widenings = 0;
        unsigned char bitp = 1;
        unsigned char *bufpmax = (unsigned char *)diff + difflen - 1;

        for (;;) {
            struct mdiff_command_instance mdci;

#define DIFF_READ_BIT() ((bitp = (bitp == 1 ? 128 : bitp / 2)),         \
                         (bufp += (bitp == 128 ? 1 : 0)),               \
                         (bufp <= bufpmax ? (void)0 :                   \
                          errfunction("diff ends in command header", diff)), \
                         (!!(*bufp & bitp)))

            int encodewidth = 0;
            int encodebase = 0;
            while (DIFF_READ_BIT()) {
                encodewidth++;
                encodebase += 1 << encodewidth;
            }
            for (i = encodewidth; i >= 0; i--)
                encodebase += DIFF_READ_BIT() << i;
            if (encodebase >= mdiff_command_count) {
                errfunction("diff contains unknown command", diff);
                return;
            }
            mdci.command = new_memfile->mdiff_command_mru[encodebase];

            /* Record this command in the MRU list (even if it's a widening);
               this matches the MRU state used when encoding. */
            i = encodebase;
            while (i > 0) {
                new_memfile->mdiff_command_mru[i] =
                    new_memfile->mdiff_command_mru[i-1];
                i--;
            }
            new_memfile->mdiff_command_mru[0] = mdci.command;

            /* Widenings are a special case: we go back round the loop
               immediately, even if in the middle of a byte. */
            if (mdci.command == mdiff_wider) {
                widenings++;
                continue;
            }

            /* Read our arg2. (This works even in the degenerate case where
               it's zero bits wide.) */
            mdci.arg2 = 0;
            for (i = 0; i < mdiff_command_sizes[mdci.command].arg2; i++)
                mdci.arg2 = (mdci.arg2 << 1) | DIFF_READ_BIT();

            /* Read the remaining bits in the current byte into arg1; they
               must be part of it. */
            if (mdiff_command_sizes[mdci.command].arg1 < 0 &&
                (*bufp & (bitp / 2) ||
                 (bufp < bufpmax && bitp == 1 && bufp[1] & 128)))
                /* We're reading in a negative signed number. */
                mdci.arg1 = (-1LL * (signed)bitp) | *bufp;
            else
                mdci.arg1 = *bufp & (bitp - 1);

            /* Read in any extra bytes that are part of arg1. */
            int arg1_bits_required = widenings * 8 +
                abs(mdiff_command_sizes[mdci.command].arg1);
            while (arg1_bits_required > 8 ||
                   (arg1_bits_required > 0 &&
                    (1 << arg1_bits_required) > bitp)) {
                if (bufp == bufpmax) {
                    errfunction("diff ends inside flexible argument", diff);
                    return;
                }
                mdci.arg1 *= 256;
                mdci.arg1 += *++bufp;
                arg1_bits_required -= 8;
            }

            if (debuglog)
                fprintf(debuglog, "< %s %" PRIdLEAST64 " %" PRIuLEAST64 "\n",
                        mdiff_command_sizes[mdci.command].name,
                        (int_least64_t)mdci.arg1, (uint_least64_t)mdci.arg2);

            /* Handle the "copy" part of a command, if any. */
            long long copy;
            switch (mdci.command) {
            case mdiff_copyedit:
            case mdiff_copyedit1:
            case mdiff_copy:
            case mdiff_increment:
            case mdiff_eof:
            case mdiff_eof_crc32:
            case mdiff_erase:
                copy = mdci.arg1;
                break;

            case mdiff_seek:
            case mdiff_edit:
                copy = 0;
                break;

            case mdiff_coord:
                copy = mdci.arg1 * SAVE_SIZE_MONST;
                if ((new_memfile->pos -
                     new_memfile->coord_relative_to) % SAVE_SIZE_MONST)
                    copy += SAVE_SIZE_MONST -
                        ((new_memfile->pos -
                          new_memfile->coord_relative_to) % SAVE_SIZE_MONST);
                break;

            case mdiff_rle:
                errfunction("TODO: mdiff_rle unimplemented", diff);
                return;

            case mdiff_wider:
            case mdiff_command_count:
            default:
                errfunction("this should be unreachable", diff);
                return;
            }

            if (debuglog)
                fprintf(debuglog, "= seek %" PRIdLEAST64
                        " copy %" PRIdLEAST64,
                        (uint_least64_t)
                        (mdci.command == mdiff_seek ? mdci.arg1 : 0),
                        (uint_least64_t)copy);

            mfp = mmmap(new_memfile, copy, new_memfile->pos);
            if (dbpos < 0 && copy) {
                errfunction("diff copies from before the start of file", diff);
                return;
            } else if (dbpos + copy > diff_base->pos && copy) {
                errfunction("diff copies from after the end of file", diff);
                return;
            }
            memcpy(mfp, diff_base->buf + dbpos, copy);
            dbpos += copy;
            if (mdci.command == mdiff_copy)
                new_memfile->coord_relative_to = new_memfile->pos;

            /* Handle the "edit" part of a command, if any. */
            long long edit;
            switch (mdci.command) {
            case mdiff_copyedit:
                edit = mdci.arg2 + 1;
                break;
            case mdiff_copyedit1:
            case mdiff_increment:
                edit = 1;
                break;
            case mdiff_edit:
                edit = mdci.arg1 + 1;
                break;
            case mdiff_erase:
                edit = mdci.arg2 + 3;
                break;

            case mdiff_copy:
            case mdiff_seek:
            case mdiff_eof:
            case mdiff_eof_crc32:
                edit = 0;
                break;

            case mdiff_coord:
                edit = 2;
                if (mdci.arg2 == DIR_W || mdci.arg2 == DIR_E)
                    edit = 1;
                break;

            case mdiff_rle:
                errfunction("TODO: mdiff_rle unimplemented", diff);
                return;

            case mdiff_wider:
            case mdiff_command_count:
            default:
                errfunction("this should be unreachable", diff);
                return;
            }

            if (debuglog)
                fprintf(debuglog, " edit %" PRIdLEAST64
                        " pos %" PRIdLEAST64 "\n", (uint_least64_t)edit,
                        (uint_least64_t)(new_memfile->pos + edit));

            mfp = mmmap(new_memfile, edit, new_memfile->pos);
            if (mdci.command == mdiff_edit || mdci.command == mdiff_copyedit ||
                mdci.command == mdiff_copyedit1) {
                if (bufp - (unsigned char *)diff >= difflen - edit) {
                    errfunction("diff ends during edit/copyedit data", diff);
                    return;
                }
                memcpy(mfp, bufp + 1, edit);
                bufp += edit;
            } else if (mdci.command == mdiff_erase) {
                memset(mfp, 0, edit);
            } else if (mdci.command == mdiff_increment ||
                       mdci.command == mdiff_coord) {
                if (dbpos < 0) {
                    errfunction("diff adjustment before the start of file",
                                diff);
                    return;
                } else if (dbpos + edit > diff_base->pos) {
                    errfunction("diff adjustment after the end of file",
                                diff);
                    return;
                }
                schar dx = 1;
                schar dy = 0;
                schar dz;

                if (mdci.command == mdiff_coord)
                    dir_to_delta(mdci.arg2, &dx, &dy, &dz);

                mfp[0] = (uint8_t)(dx + (uint8_t)diff_base->buf[dbpos]);
                if (edit == 2)
                    mfp[1] = (uint8_t)(dy + (uint8_t)diff_base->buf[dbpos + 1]);
            }

            dbpos += edit;    /* can legally go past the end of diff_base */

            if (mdci.command == mdiff_seek)
                dbpos -= mdci.arg1;

            /* Now the command's ended, prepare for the next command. */
            new_memfile->last_command = mdci;
            widenings = 0;
            bitp = 1;

            if (mdci.command == mdiff_eof)
                break;
            if (mdci.command == mdiff_eof_crc32) {
                uLong crc = crc32(crc32(0L, Z_NULL, 0),
                                  (Bytef *)new_memfile->buf, new_memfile->pos);
                if (crc != mdci.arg2) {
                    errfunction("diff produced output with wrong checksum",
                                diff);
                    return;
                }
                break;
            }
        }

    } else if (bufp[0] == MDIFF_HEADER_0 && bufp[1] == MDIFF_HEADER_1_BETA2) {
        /* Compatibility code for loading -beta2 saves */

#define REQUIRE_BUFP(req, m) do {                                       \
            if (bufp - (unsigned char *)diff > difflen - (req)) {       \
                errfunction("new-style binary diff ends unexpectedly: " m, \
                            diff);                                      \
                return;                                                 \
            }                                                           \
        } while(0)
#define PARSE_BUFP_INTO_CMD(req) do {                           \
            cmd = 0;                                            \
            REQUIRE_BUFP(req, "parsing " #req " bytes");        \
            for (i = 0; i < (req); i++)                         \
            { cmd *= 256; cmd += (uint8_t)*(bufp++); }          \
        } while(0)

        bufp += 2;

        for (;;) {
            long copy = 0, edit = 0, seek = 0;
            uint32_t cmd;

            switch ((((uint16_t)bufp[0]) << 8) & MDIFF_CMDMASK) {

            case MDIFF_LARGE_COPYEDIT:
                PARSE_BUFP_INTO_CMD(4);
                copy = cmd & 0x01FFFFFFUL;
                edit = (cmd & 0x1E000000UL) >> 25;
                break;

            case MDIFF_LARGE_EDIT:
                PARSE_BUFP_INTO_CMD(4);
                edit = cmd & 0x1FFFFFFFUL;
                break;

            case MDIFF_SEEK:
                PARSE_BUFP_INTO_CMD(2);
                /* parse unsigned into signed */
                if (cmd & 0x1000)
                    seek = (int)(cmd & 0x0FFFU) - (int)0x1000;
                else
                    seek = cmd & 0x0FFFU;
                break;

            case MDIFF_LARGE_SEEK:
                PARSE_BUFP_INTO_CMD(4);
                /* as above */
                if (cmd & 0x10000000)
                    seek = (long)(cmd & 0x0FFFFFFFUL) - (long)0x10000000L;
                else
                    seek = cmd & 0x0FFFFFFFUL;
                break;

            default: /* normal-sized copyedit */
                if (!bufp[0] && !bufp[1])
                    goto done; /* break out of two levels of loops */

                PARSE_BUFP_INTO_CMD(2);
                copy = cmd & 0x1FFFU;
                edit = ((cmd & 0x6FFFU) >> 13) + 1;
                break;
            }

            dbpos -= seek;

            if (copy) {
                if (dbpos + copy > diff_base->pos) {
                    errfunction("new-style binary diff reads past EOF", diff);
                    return;
                }

                mfp = mmmap(new_memfile, copy, new_memfile->pos);
                memcpy(mfp, diff_base->buf + dbpos, copy);

                dbpos += copy;
            }

            if (edit) {
                REQUIRE_BUFP(edit, "edit data");

                mfp = mmmap(new_memfile, edit, new_memfile->pos);
                memcpy(mfp, bufp, edit);

                dbpos += edit;    /* can legally go past the end of diff_base */
                bufp += edit;
            }
        }
    done:;

    } else {
        /* Compatibility code for loading -beta1 saves */

        while (bufp[0] || bufp[1]) {
            /* 0x0000 means "seek 0", which is never generated, and thus works
               as an EOF marker */

            signed short n = (unsigned char)(bufp[1]) & 0x3F;
            n *= 256;
            n += (unsigned char)(bufp[0]);

            bufp += 2;

            switch ((unsigned char)(bufp[-1]) >> 6) {
            case MDIFFOLD_SEEK:

                if (n >= 0x2000)
                    n -= 0x4000;

                if (dbpos < n) {
                    errfunction("binary diff seeks past start of file\n", diff);
                    return;
                }

                dbpos -= n;

                break;

            case MDIFFOLD_COPY:

                mfp = mmmap(new_memfile, n, new_memfile->pos);

                if (dbpos + n > diff_base->pos) {
                    errfunction("binary diff reads past EOF\n", diff);
                    return;
                }

                memcpy(mfp, diff_base->buf + dbpos, n);
                dbpos += n;

                break;

            case MDIFFOLD_EDIT:

                mfp = mmmap(new_memfile, n, new_memfile->pos);

                if (bufp - (unsigned char *)diff + n > difflen) {
                    errfunction("binary diff ends unexpectedly\n", diff);
                    return;
                }

                memcpy(mfp, bufp, n);
                dbpos += n;     /* can legally go past the end of diff_base! */
                bufp += n;

                break;

            default:
                errfunction("unknown command in binary diff\n", diff);
                return;
            }
        }
    }
}

/* Tagging memfiles. This remembers the correspondence between the tag
   and the file location. For a diff memfile, it also sets relativepos
   to the pos of the tag in relativeto, if it exists, and adds a seek
   command to the diff, unless it would be redundant. */
void
mtag(struct memfile *mf, long tagdata, enum memfile_tagtype tagtype)
{
    /* 619 is chosen here because it's a prime number, and it's approximately
       in the golden ratio with MEMFILE_HASHTABLE_SIZE. */
    int bucket = (tagdata * 619 + (int)tagtype) % MEMFILE_HASHTABLE_SIZE;
    struct memfile_tag *tag = malloc(sizeof (struct memfile_tag));

    tag->next = mf->tags[bucket];
    tag->tagdata = tagdata;
    tag->tagtype = tagtype;
    tag->pos = mf->pos;
    mf->tags[bucket] = tag;
    mf->last_tag = tag;

    if (mf->relativeto) {
        for (tag = mf->relativeto->tags[bucket]; tag; tag = tag->next) {
            if (tag->tagtype == tagtype && tag->tagdata == tagdata)
                break;
        }
        if (tag && mf->relativepos != tag->pos) {
            int offset = mf->relativepos - tag->pos;

            if (mf->pending_edits || mf->pending_copies)
                mdiffflush(mf, 0);

            mf->pending_seeks += offset;
            mf->relativepos = tag->pos;
        }
    }
}

void
mread(struct memfile *mf, void *buf, unsigned int len)
{
    int rlen = min(len, mf->len - mf->pos);

    memcpy(buf, &mf->buf[mf->pos], rlen);
    mf->pos += rlen;
    if ((unsigned)rlen != len)
        panic("Error reading game data.");
}


int8_t
mread8(struct memfile *mf)
{
    int8_t value;

    mread(mf, &value, 1);
    return value;
}


int16_t
mread16(struct memfile * mf)
{
    int16_t value;

    mread(mf, &value, 2);
    return le16_to_host(value);
}


int32_t
mread32(struct memfile * mf)
{
    int32_t value;

    mread(mf, &value, 4);
    return le32_to_host(value);
}


int64_t
mread64(struct memfile * mf)
{
    int64_t value;

    mread(mf, &value, 8);
    return le64_to_host(value);
}


/* move the file position forward until it is aligned (align=4: dword align etc)
 * aln MUST be a power of 2, otherwise the alignmask calculation breaks. */
static void
mfalign(struct memfile *mf, int aln)
{
    int i, alignbytes;
    unsigned int alignmask = ~(aln - 1);

    alignbytes = aln - (mf->pos & alignmask);
    for (i = 0; i < alignbytes; i++)
        mwrite8(mf, 0); /* go via mwrite to set up diffing properly */
}


void
mfmagic_check(struct memfile *mf, int32_t magic)
{
    int32_t m2;

    mfalign(mf, 4);
    m2 = mread32(mf);
    if (magic != m2)
        terminate(ERR_RESTORE_FAILED);
}


/* for symmetry with mfmagic_check */
void
mfmagic_set(struct memfile *mf, int32_t magic)
{
    /* don't start new sections of the save in the middle of a word - this will
       hopefully cut down on unaligned memory accesses */
    mfalign(mf, 4);
    mwrite32(mf, magic);
}

/* Returns TRUE if two memory files are equal. Optionally, returns the reason
   why they aren't equal in *difference_reason. */
boolean
mequal(struct memfile *mf1, struct memfile *mf2, const char **difference_reason)
{
    char *p1, *p2;
    long len, off;
    int bin;

    /* Compare the save files. If they're different lengths, we compare only the
       portion that fits into both files. */
    len = mf1->pos;
    if (len > mf2->pos)
        len = mf2->pos;

    p1 = mmmap(mf1, len, 0);
    p2 = mmmap(mf2, len, 0);

    if (difference_reason)
        *difference_reason = "files are equal";

    if (mf1->pos != mf2->pos || memcmp(p1, p2, len) != 0) {

        if (!difference_reason)
            return FALSE;

        /* Determine where the desyncs are. */
        for (off = 0; off < len; off++) {
            if (p1[off] != p2[off]) {
                struct memfile_tag *tag = NULL, *titer;
                for (bin = 0; bin < MEMFILE_HASHTABLE_SIZE; bin++)
                    for (titer = mf2->tags[bin]; titer; titer = titer->next)
                        if (titer->pos <= off)
                            if (!tag || tag->pos < titer->pos)
                                tag = titer;

                if (!tag) {

                    *difference_reason =
                        msgprintf("desync at %ld (untagged), "
                                  "was %02x is %02x", off,
                                  (int)(unsigned char)p1[off],
                                  (int)(unsigned char)p2[off]);

                } else if (tag->tagtype == MTAG_LOCATIONS) {

                    const int bpl = 8; /* bytes per location */
                    int which_location = (off - tag->pos) / bpl;

                    *difference_reason =
                        msgprintf("desync at %ld ((%d, %d) + %ld byte%s), "
                                  "was %02x is %02x", off,
                                  which_location / ROWNO,
                                  which_location % ROWNO,
                                  (off - tag->pos) % bpl,
                                  ((off - tag->pos) % bpl) == 1 ? "" : "s",
                                  (int)(unsigned char)p1[off],
                                  (int)(unsigned char)p2[off]);

                } else {

                    *difference_reason =
                        msgprintf("desync at %ld (%d:%lx + %ld byte%s), "
                                  "was %02x is %02x", off,
                                  (int)tag->tagtype, tag->tagdata,
                                  off - tag->pos,
                                  (off - tag->pos == 1) ? "" : "s",
                                  (int)(unsigned char)p1[off],
                                  (int)(unsigned char)p2[off]);
                }

                return FALSE; /* only report one issue to reduce spam */
            }

        }

        *difference_reason =
            msgprintf("lengths differ (was %d is %d)",
                      mf1->pos, mf2->pos);

        return FALSE;
    }
    return TRUE;
}



/* memfile.c */
