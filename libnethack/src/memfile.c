/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-02-04 */
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

    static const char diffheader[2] = {MDIFF_HEADER_0, MDIFF_HEADER_1};

    mf->buf = mf->diffbuf = NULL;
    mf->len = mf->pos = mf->difflen = mf->diffpos = mf->relativepos = 0;
    mf->relativeto = relativeto;

    mf->pending_edits = mf->pending_copies = mf->pending_seeks = 0;

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

                if (mf->pending_seeks || mf->pending_edits ||
                    mf->pending_copies >= 0x01FFFFFF)
                    mdiffflush(mf);

                mf->pending_copies++;

            } else {

                /* Note that mdiffflush is responsible for writing the actual
                   data that was edited, once we have a complete run of it. So
                   there's no need to record the data anywhere but in buf. */
                if (mf->pending_seeks || mf->pending_edits >= 0x1FFFFFFF ||
                    (mf->pending_edits >= 0xF && mf->pending_copies))
                    mdiffflush(mf);

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

void
mdiffflush(struct memfile *mf)
{
    uint16_t shortcmd = 0;
    uint32_t longcmd = 0;
    boolean using_longcmd = FALSE;
    uint8_t buf[4];

    if (mf->pending_seeks) {
        if (mf->pending_seeks >= -0x1000 && mf->pending_seeks <= 0x0FFF) {
            shortcmd = mf->pending_seeks;
            shortcmd &= 0x1FFF;
            shortcmd |= MDIFF_SEEK;
        } else {
            longcmd = mf->pending_seeks;
            longcmd &= 0x1FFFFFFF;
            longcmd |= MDIFF_LARGE_SEEK << 16;
            using_longcmd = TRUE;
        }
    } else if (mf->pending_copies <= 0x1FFF &&
               mf->pending_edits >= 1 && mf->pending_edits <= 4 &&
               (mf->pending_copies != 0 || mf->pending_edits != 1)) {
        shortcmd = mf->pending_copies;
        shortcmd |= (mf->pending_edits - 1) << 13;
    } else if (mf->pending_copies == 0 && mf->pending_edits &&
               mf->pending_edits <= 0x1FFFFFFF) {
        longcmd = mf->pending_edits;
        longcmd |= MDIFF_LARGE_EDIT << 16;
        using_longcmd = TRUE;
    } else if (mf->pending_copies <= 0x01FFFFFF && mf->pending_edits <= 0xF &&
               mf->pending_copies) {
        longcmd = mf->pending_copies;
        longcmd |= mf->pending_edits << 25;
        longcmd |= MDIFF_LARGE_COPYEDIT << 16;
        using_longcmd = TRUE;
    } else if (mf->pending_copies || mf->pending_edits) {
        panic("mdiffflush: too many copies (%ld) and/or edits (%ld)",
              mf->pending_copies, mf->pending_edits);
    } else
        return; /* nothing to do */

    if (using_longcmd) {
        buf[0] = (longcmd >> 24) & 255;
        buf[1] = (longcmd >> 16) & 255;
        buf[2] = (longcmd >>  8) & 255;
        buf[3] = (longcmd >>  0) & 255;
        mdiffwrite(mf, buf, 4);
    } else {
        buf[0] = (shortcmd >> 8) & 255;
        buf[1] = (shortcmd >> 0) & 255;
        mdiffwrite(mf, buf, 2);
    }

    if (mf->pending_edits) {
        if (mf->pending_edits > mf->pos || mf->pending_edits < 0)
            panic("mdiffflush: trying to edit with too much data");
        mdiffwrite(mf, mf->buf + mf->pos - mf->pending_edits,
                   mf->pending_edits);
    }

    if (debuglog) {
        fprintf(debuglog,
                "%p: seek %ld copy %ld edit %ld bytes, last copy %d:%08lx%+d\n",
                (void *)mf,
                mf->pending_seeks, mf->pending_copies, mf->pending_edits,
                mf->last_tag ? mf->last_tag->tagtype : -1,
                mf->last_tag ? mf->last_tag->tagdata : 0,
                mf->pos - (int)mf->pending_edits -
                (mf->last_tag ? mf->last_tag->pos : 0));
    }

    mf->pending_seeks = 0;
    mf->pending_edits = 0;
    mf->pending_copies = 0;
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
                mdiffflush(mf);

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
