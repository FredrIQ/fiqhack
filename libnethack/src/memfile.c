/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

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
#endif

/* Creating and freeing memory files */
void
mnew(struct memfile *mf, struct memfile *relativeto)
{
    int i;

    mf->buf = mf->diffbuf = NULL;
    mf->len = mf->pos = mf->difflen = mf->diffpos = mf->relativepos = 0;
    mf->relativeto = relativeto;
    mf->curcmd = MDIFF_INVALID; /* no command yet */
    for (i = 0; i < MEMFILE_HASHTABLE_SIZE; i++)
        mf->tags[i] = 0;
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

void
mwrite(struct memfile *mf, const void *buf, unsigned int num)
{
    boolean do_realloc = FALSE;

    while (mf->len < mf->pos + num) {
        mf->len += 4096;
        do_realloc = TRUE;
    }

    if (do_realloc)
        mf->buf = realloc(mf->buf, mf->len);
    memcpy(&mf->buf[mf->pos], buf, num);

    if (!mf->relativeto) {
        mf->pos += num;
    } else {
        /* calculate and record the diff as well */
        while (num--) {
            if (mf->relativepos < mf->relativeto->pos &&
                mf->buf[mf->pos] == mf->relativeto->buf[mf->relativepos]) {
                if (mf->curcmd != MDIFF_COPY || mf->curcount == 0x3fff) {
                    mdiffflush(mf);
                    mf->curcount = 0;
                }
                mf->curcmd = MDIFF_COPY;
                mf->curcount++;
            } else {
                /* Note that mdiffflush is responsible for writing the actual
                   data that was edited, once we have a complete run of it. So
                   there's no need to record the data anywhere but in buf. */
                if (mf->curcmd != MDIFF_EDIT || mf->curcount == 0x3fff) {
                    mdiffflush(mf);
                    mf->curcount = 0;
                }
                mf->curcmd = MDIFF_EDIT;
                mf->curcount++;
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
mdiffwrite14(struct memfile *mf, uint8_t command, int16_t value)
{
    uint16_t le_value = value;

    le_value &= 0x3fff; /* in case it's negative */
    le_value |= command << 14;
    le_value = host_to_le16(le_value);
    mdiffwrite(mf, &le_value, 2);
}

void
mdiffflush(struct memfile *mf)
{
    if (mf->curcmd != MDIFF_INVALID)
        mdiffwrite14(mf, mf->curcmd, mf->curcount);
    if (mf->curcmd == MDIFF_EDIT) {
        /* We need to record the actual data to edit with, too. */
        if (mf->curcount > mf->pos || mf->curcount < 0)
            panic("mdiffflush: trying to edit with too much data");
        mdiffwrite(mf, mf->buf + mf->pos - mf->curcount, mf->curcount);
    }
    mf->curcmd = MDIFF_INVALID;
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
    if (mf->relativeto) {
        for (tag = mf->relativeto->tags[bucket]; tag; tag = tag->next) {
            if (tag->tagtype == tagtype && tag->tagdata == tagdata)
                break;
        }
        if (tag && mf->relativepos != tag->pos) {
            int offset = mf->relativepos - tag->pos;

            if (mf->curcmd != MDIFF_SEEK) {
                mdiffflush(mf);
                mf->curcount = 0;
            }
            while (offset + mf->curcount >= 1 << 13 ||
                   offset + mf->curcount <= -(1 << 13)) {
                if (offset + mf->curcount < 0) {
                    mdiffwrite14(mf, MDIFF_SEEK, -0x1fff);
                    offset += 0x1fff;
                } else {
                    mdiffwrite14(mf, MDIFF_SEEK, 0x1fff);
                    offset -= 0x1fff;
                }
            }
            mf->curcount += offset;
            mf->curcmd = (mf->curcount ? MDIFF_SEEK : MDIFF_INVALID);
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
        terminate();
}


/* for symmetry with mfmagic_check */
void
mfmagic_set(struct memfile *mf, int32_t magic)
{
    /* don't start new sections of the save in the middle of a word - this will 
       hopefully cut down on unaligned memory acesses */
    mfalign(mf, 4);
    mwrite32(mf, magic);
}


/* memfile.c */
