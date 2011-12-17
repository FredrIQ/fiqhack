/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#ifdef IS_BIG_ENDIAN
static unsigned short host_to_le16(unsigned short x) { return _byteswap16(x); }
static unsigned int   host_to_le32(unsigned int x)   { return _byteswap32(x); }
static unsigned short le16_to_host(unsigned short x) { return _byteswap16(x); }
static unsigned int   le32_to_host(unsigned int x)   { return _byteswap32(x); }
#else
static unsigned short host_to_le16(unsigned short x) { return x; }
static unsigned int   host_to_le32(unsigned int x)   { return x; }
static unsigned short le16_to_host(unsigned short x) { return x; }
static unsigned int   le32_to_host(unsigned int x)   { return x; }
#endif


void mwrite(struct memfile *mf, const void *buf, unsigned int num)
{
	boolean do_realloc = FALSE;
	while (mf->len < mf->pos + num) {
	    mf->len += 4096;
	    do_realloc = TRUE;
	}
	
	if (do_realloc)
	    mf->buf = realloc(mf->buf, mf->len);
	memcpy(&mf->buf[mf->pos], buf, num);
	mf->pos += num;
}


void mwrite8(struct memfile *mf, int8_t value)
{
	mwrite(mf, &value, 1);
}


void mwrite16(struct memfile *mf, int16_t value)
{
	int16_t le_value = host_to_le16(value);
	mwrite(mf, &le_value, 2);
}


void mwrite32(struct memfile *mf, int32_t value)
{
	int32_t le_value = host_to_le32(value);
	mwrite(mf, &le_value, 4);
}


void store_mf(int fd, struct memfile *mf)
{
	int len, left, ret;
	
	len = left = mf->pos;
	while (left) {
	    ret = write(fd, &mf->buf[len - left], left);
	    if (ret == -1) /* error */
		goto out;
	    left -= ret;
	}

out:
	free(mf->buf);
	mf->buf = NULL;
	mf->pos = mf->len = 0;
}



void mread(struct memfile *mf, void *buf, unsigned int len)
{
	int rlen = min(len, mf->len - mf->pos);

	memcpy(buf, &mf->buf[mf->pos], rlen);
	mf->pos += rlen;
	if ((unsigned)rlen != len)
	    panic("Error reading game data.");
}


int8_t mread8(struct memfile *mf)
{
	int8_t value;
	mread(mf, &value, 1);
	return value;
}


int16_t mread16(struct memfile *mf)
{
	int16_t value;
	mread(mf, &value, 2);
	return le16_to_host(value);
}


int32_t mread32(struct memfile *mf)
{
	int32_t value;
	mread(mf, &value, 4);
	return le32_to_host(value);
}


/* move the file position forward until it is aligned (align=4: dword align etc)
 * aln MUST be a power of 2, otherwise the alignmask calculation breaks. */
static void mfalign(struct memfile *mf, int aln)
{
	int i, alignbytes;
	unsigned int alignmask = ~(aln - 1);
	
	alignbytes = aln - (mf->pos & alignmask);
	for (i = 0; i < alignbytes; i++)
	    mf->buf[mf->pos++] = 0;
}


void mfmagic_check(struct memfile *mf, int32_t magic)
{
	int32_t m2;
	mfalign(mf, 4);
	m2 = mread32(mf);
	if (magic != m2)
	    panic("damaged save!");
}


/* for symmetry with mfmagic_check */
void mfmagic_set(struct memfile *mf, int32_t magic)
{
	/* don't start new sections of the save in the middle of a word - this
	 * will hopefully cut down on unaligned memory acesses */
	mfalign(mf, 4);
	mwrite32(mf, magic);
}


/* memfile.c */
