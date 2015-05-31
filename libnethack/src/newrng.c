/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-05-31 */
/* Copyright (c) 2014 Alex Smith */
/* NetHack may be freely redistributed.  See license for details. */

/* This random number generator was newly designed by Alex Smith in order to get
   around both practical issues with the old RNG (it bloated the save files),
   and legal issues (the previously used RNG was licensed under LGPL2+, which is
   not compatible with the NGPL).

   The SHA-256 code here is based on that from the NGPL AdeonRNG (as submitted
   to /dev/null/nethack), which was in turn based on that in the public domain
   LibTomCrypt. The entropy collectors are also based on AdeonRNG, and as far as
   I know, originated there. For NetHack 4, the code was reformatted and
   simplified via removing unused codepaths. */

#ifdef AIMAKE_BUILDOS_MSWin32
# define WIN32_LEAN_AND_MEAN /* else windows.h tries to define "boolean" */
# include <windows.h> /* wincrypt.h is broken and doesn't include this itself */
# include <wincrypt.h>
#endif

#include "rnd.h"
#include "flag.h"
#include "you.h"
#include "extern.h"
#include "rm.h"
#include <sys/time.h>

struct sha256_state {
    uint64_t length;
    uint32_t state[8], curlen;
    unsigned char buf[64];
};

/* the K array */
static const uint32_t K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
    0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
    0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
    0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
    0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
    0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
    0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
    0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
    0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

static void
sha256_init(struct sha256_state *ss)
{
    ss->curlen = 0;
    ss->length = 0;
    ss->state[0] = 0x6A09E667UL;
    ss->state[1] = 0xBB67AE85UL;
    ss->state[2] = 0x3C6EF372UL;
    ss->state[3] = 0xA54FF53AUL;
    ss->state[4] = 0x510E527FUL;
    ss->state[5] = 0x9B05688CUL;
    ss->state[6] = 0x1F83D9ABUL;
    ss->state[7] = 0x5BE0CD19UL;
}

/* Note: these assume uint32_t arguments. */
#define S(x, n)      (((x) >> (n)) | ((x) << (32 - (n))))
#define R(x, n)      ((x) >> (n))
#define Ch(x, y, z)  (z ^ (x & (y ^ z)))
#define Maj(x, y, z) (((x | y) & z) | (x & y)) 
#define Sigma0(x)    (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)    (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)    (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)    (S(x, 17) ^ S(x, 19) ^ R(x, 10))

static void
sha256_compress(struct sha256_state *ss, const unsigned char buf[64])
{
    uint32_t S[8], W[64];
    int i;

    /* copy state into S */
    for (i = 0; i < 8; i++) {
        S[i] = ss->state[i];
    }

    /* copy the state into 512-bits into W[0..15] */
    const unsigned char *bp = buf;
    for (i = 0; i < 16; i++) {
        W[i]  = *bp++ << 24;
        W[i] |= *bp++ << 16;
        W[i] |= *bp++ <<  8;
        W[i] |= *bp++ <<  0;
    }

    /* fill W[16..63] */
    for (i = 16; i < 64; i++) {
        W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
    }

    /* Compress */
    for (i = 0; i < 64; ++i) {
        uint32_t t, t0, t1;

        t0 = S[7] + Sigma1(S[4]) + Ch(S[4], S[5], S[6]) + K[i] + W[i];
        t1 = Sigma0(S[0]) + Maj(S[0], S[1], S[2]);
        S[3] += t0;
        S[7] = t0 + t1;

        t = S[7]; S[7] = S[6]; S[6] = S[5]; S[5] = S[4];
        S[4] = S[3]; S[3] = S[2]; S[2] = S[1]; S[1] = S[0]; S[0] = t;
    }

    /* feedback */
    for (i = 0; i < 8; i++) {
        ss->state[i] += S[i];
    }
}

static const int sha256_block_size = 64;
static void
sha256_process(struct sha256_state *ss,
               const unsigned char *in, unsigned long inlen)
{
    unsigned long n;

    if (ss->curlen > sizeof(ss->buf))
        panic("corrupted sha256 state in sha256_process");

    while (inlen > 0) {

        if (ss->curlen == 0 && inlen >= sha256_block_size) {

            sha256_compress(ss, in);

            ss->length += sha256_block_size * 8;
            in += sha256_block_size;
            inlen -= sha256_block_size;

        } else {

            n = sha256_block_size - ss->curlen;
            if (n > inlen)
                n = inlen;

            memcpy(ss->buf + ss->curlen, in, (size_t)n);
            ss->curlen += n;
            in += n;
            inlen -= n;

            if (ss->curlen == sha256_block_size) {

                sha256_compress(ss, ss->buf);
                ss->length += sha256_block_size * 8;
                ss->curlen = 0;
            }
        }
    }
}

/* Note: the output lifetime of the return pointer (which points to eight 32-bit
   numbers) is equal to that of the input struct sha256_state. */
static uint32_t *
sha256_done(struct sha256_state *ss)
{
    if (ss->curlen >= sizeof(ss->buf))
        panic("corrupted sha256 state in sha256-done");

    /* increase the length of the message */
    ss->length += ss->curlen * 8;

    /* append the '1' bit */
    ss->buf[ss->curlen++] = (unsigned char)0x80;

    /* if the length is currently above 56 bytes we append zeros then compress.
       Then we can fall back to padding zeros and length encoding like
       normal. */
    if (ss->curlen > 56) {
        while (ss->curlen < 64) {
            ss->buf[ss->curlen++] = (unsigned char)0;
        }
        sha256_compress(ss, ss->buf);
        ss->curlen = 0;
    }

    /* pad up to 56 bytes of zeroes */
    while (ss->curlen < 56) {
        ss->buf[ss->curlen++] = (unsigned char)0;
    }

    /* store length */
    ss->buf[56] = (ss->length >> 56) & 255;
    ss->buf[57] = (ss->length >> 48) & 255;
    ss->buf[58] = (ss->length >> 40) & 255;
    ss->buf[59] = (ss->length >> 32) & 255;
    ss->buf[60] = (ss->length >> 24) & 255;
    ss->buf[61] = (ss->length >> 16) & 255;
    ss->buf[62] = (ss->length >>  8) & 255;
    ss->buf[63] = (ss->length >>  0) & 255;
    sha256_compress(ss, ss->buf);

    return ss->state;
}

/* End of SHA-256 code. Start of entropy collectors (based on AdeonRNG by Mikko
   Juola, modified for NetHack 4). */

/* Entropy collector 1: OS entropy source. */
static void
collect_entropy1(unsigned char data[static RNG_SEED_SIZE_BYTES])
{
#ifdef AIMAKE_BUILDOS_MSWin32

    HCRYPTPROV crypto_provider;
    /* Typical Windows: you need to initialize a crypto provider in order to use
       the cryptographic RNG, and in order to do this, you need to tell it what
       signing and hashing algorithms you want it to use for signing and hashing
       operations, even though we aren't doing any.  So there's a need to make
       an arbitrary choice of crypto provider.  We use PROV_RSA_FULL because
       it's defined as "general-purpose".

       Just for fun, this also has different error codes from most Windows API
       functions (TRUE/FALSE, not S_OK/...). TRUE means success. */
    if (!CryptAcquireContext(&crypto_provider, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        paniclog("entropy", "CryptAquireContext failed");
        return;
    }

    CryptGenRandom(crypto_provider, RNG_SEED_SIZE_BYTES, data);

    CryptReleaseContext(crypto_provider, 0);

#else

    unsigned char holding_area[RNG_SEED_SIZE_BYTES];
    boolean holding_area_valid = 0;

    FILE* f = fopen("/dev/urandom", "rb");
    if (!f)
        f = fopen("/dev/random", "rb");
    if (!f) {
        paniclog("entropy", "could not open /dev/urandom or /dev/random");
        return; /* can't do much about this */
    }

    if (fread(holding_area, RNG_SEED_SIZE_BYTES, 1, f) == 1)
        holding_area_valid = 1;

    fclose(f);

    if (!holding_area_valid) {
        paniclog("entropy", "could not read /dev/urandom or /dev/random");
        return;
    }

    int i;
    for (i = 0; i < RNG_SEED_SIZE_BYTES; i++)
        data[i] ^= holding_area[i];

#endif
}

/* Entropy collector 2: High-resolution timer. (We need a second entropy
   collector because the first one might fail due to, say, a missing
   /dev/urandom.) */
static void
collect_entropy2(unsigned char data[static RNG_SEED_SIZE_BYTES])
{
    struct timeval tv;
    gettimeofday(&tv, (struct timezone*) 0);

    /* We only have 8 bytes of entropy (if that; some aren't very high quality),
       so we mix in with the second- to ninth-most significant bytes of the data
       (the most significant is mixed by the RNG number). */
    unsigned char *d = data + RNG_SEED_SIZE_BYTES - 1;

    *--d ^= (unsigned char)((tv.tv_sec >>  0) & 255);
    *--d ^= (unsigned char)((tv.tv_sec >>  8) & 255);
    *--d ^= (unsigned char)((tv.tv_sec >> 16) & 255);
    *--d ^= (unsigned char)((tv.tv_sec >> 24) & 255);

    *--d ^= (unsigned char)((tv.tv_usec >>  0) & 255);
    *--d ^= (unsigned char)((tv.tv_usec >>  8) & 255);
    *--d ^= (unsigned char)((tv.tv_usec >> 16) & 255);
    *--d ^= (unsigned char)((tv.tv_usec >> 24) & 255);
}

/* End of entropy collectors, everything beyond here is NH4-specific.

   The basic idea behind our RNG is to repeatedly increment a seed number, using
   the SHA-256 of the seed as the actual RNG output. This is not
   cryptographically secure (it's missing forward perfect secrecy), but it does
   save space in the save file, because although there's a lot of state, most of
   it hardly ever changes.

   The seedspace must be exactly 2508 bytes long, to avoid desyncs when loading
   save files from 4.3-beta1 (we have 2508 arbitrary bytes in the save file, and
   on zero-time commands, want to preserve them). So, instead, we split the RNG
   into 209 separate RNGs, each with 12 bytes (96 bits) of seed. Events that
   have particularly strong balance impacts (e.g. number of charges in a wand of
   wishing) each have their own RNG, so that if multiple games are played on the
   same seed, there will be approximate balance between them.

   All 209 RNGs start with almost the same state (so that we only need 96 bits
   of entropy to start things off). However, the most significant byte has the
   RNG number added during seeding (with wrapping), so that the RNGs have
   different sequences from each other. This initial state has the advantage of
   compressing well (11 out of every 12 bytes are identical). */

/* Seeding. */

static_assert(rng_initialseed == 0, "initial seed is not first in RNG array");
static void
seed_rng_from_initial_seed(void)
{
    unsigned char *s;
    unsigned char i;
    for (s = flags.rngstate + RNG_SEED_SIZE_BYTES, i = 1;
         s < flags.rngstate + RNG_SEEDSPACE;
         s += RNG_SEED_SIZE_BYTES, i++) {

        memcpy(s, flags.rngstate, RNG_SEED_SIZE_BYTES);
        s[RNG_SEED_SIZE_BYTES - 1] += i;
    }
}

void
seed_rng_from_entropy(void)
{
    memset(flags.rngstate, 0, RNG_SEED_SIZE_BYTES);
    collect_entropy1(flags.rngstate);
    collect_entropy2(flags.rngstate);
    seed_rng_from_initial_seed();
}

static char
rng_base64_encode_hextet(unsigned char hextet)
{
    if (hextet < 10)
        return '0' + hextet;
    else if (hextet < 36)
        return 'A' + (hextet - 10);
    else if (hextet < 62)
        return 'a' + (hextet - 36);
    else if (hextet == 62)
        return '-';
    else if (hextet == 63)
        return '_';
    else
        return 0; /* error sentinel */
}

static unsigned char
rng_base64_decode_hextet(char encoded_hextet)
{
    if (encoded_hextet >= '0' && encoded_hextet <= '9')
        return encoded_hextet - '0';
    else if (encoded_hextet >= 'A' && encoded_hextet <= 'Z')
        return encoded_hextet - 'A' + 10;
    else if (encoded_hextet >= 'a' && encoded_hextet <= 'z')
        return encoded_hextet - 'a' + 36;
    else if (encoded_hextet == '-')
        return 62;
    else if (encoded_hextet == '_')
        return 63;
    else
        return 255; /* error sentinel */
}

/* Returns false on parse failure. */
boolean
seed_rng_from_base64(const char encoded[static RNG_SEED_SIZE_BASE64])
{
    int i, j;
    unsigned char c;

    unsigned char working[RNG_SEED_SIZE_BYTES];

    for (i = 0, j = 0; i < RNG_SEED_SIZE_BYTES; i += 3, j += 4)
    {
        uint32_t w = 0;

        if (((c = rng_base64_decode_hextet(encoded[j + 0]))) == 255)
            return FALSE;
        w += ((uint32_t)c) << 0;
        if (((c = rng_base64_decode_hextet(encoded[j + 1]))) == 255)
            return FALSE;
        w += ((uint32_t)c) << 6;
        if (((c = rng_base64_decode_hextet(encoded[j + 2]))) == 255)
            return FALSE;
        w += ((uint32_t)c) << 12;
        if (((c = rng_base64_decode_hextet(encoded[j + 3]))) == 255)
            return FALSE;
        w += ((uint32_t)c) << 18;

        working[i + 0] = (w >>  0) & 255;
        working[i + 1] = (w >>  8) & 255;
        working[i + 2] = (w >> 16) & 255;
    }

    memcpy(flags.rngstate, working, RNG_SEED_SIZE_BYTES);
    seed_rng_from_initial_seed();

    return TRUE;
}

void
get_initial_rng_seed(char out[static RNG_SEED_SIZE_BASE64])
{
    int i, j;

    for (i = 0, j = 0; i < RNG_SEED_SIZE_BYTES; i += 3, j += 4)
    {
        uint32_t w = 0;
        w += ((uint32_t)flags.rngstate[i + 0]) <<  0;
        w += ((uint32_t)flags.rngstate[i + 1]) <<  8;
        w += ((uint32_t)flags.rngstate[i + 2]) << 16;

        out[j + 0] = rng_base64_encode_hextet((w >>  0) & 63);
        out[j + 1] = rng_base64_encode_hextet((w >>  6) & 63);
        out[j + 2] = rng_base64_encode_hextet((w >> 12) & 63);
        out[j + 3] = rng_base64_encode_hextet((w >> 18) & 63);
    }    
}


/* Generation. */

static uint32_t
rn2_from_seedarray(uint32_t maxplus1,
                   unsigned char seedarray[static RNG_SEED_SIZE_BYTES])
{
    struct sha256_state ss;

    if (maxplus1 == 0) {
        impossible("Impossible range 0 <= x < 0 for a random number");
        maxplus1 = 1;
    }

    /* Calculate the SHA-256 of the current seed. */
    sha256_init(&ss);
    sha256_process(&ss, seedarray, RNG_SEED_SIZE_BYTES);
    uint32_t *out = sha256_done(&ss);

    /* Increase the seed. We treat it as one big little-endian number. */
    int s;
    for (s = 0; s < RNG_SEED_SIZE_BYTES; s++) {
        seedarray[s]++;
        if (seedarray[s])
            break;
    }

    /* Produce output in the range 0..maxplus1-1. We look through the 32-bit
       numbers that the SHA-256 algorithm calculated, trying each one in turn to
       see if it produces a result that can be used without modulo bias. If it
       does, we use it. If not, we move onto the next one. If none of them work,
       we call the RNG another time, meaning there's no modulo bias at all in
       the output. (There might or might not be other sources of bias; for
       example, I don't know for certain that SHA-256 is uniformly distributed,
       although it seems likely.)

       This means that the RNG may potentially desync if called with a different
       argument on different codepaths; however, the odds are very small. Doing
       that on rng_main is just fine, because it isn't expected to sync. For
       special-purpose RNGs, though, there's a very slight chance of desync; in
       some cases (e.g. pet untaming), this is tolerable because the alternative
       is not being able to match up the RNG effects between games at all.

       In order to maximize the RNG correlation between games in cases where the
       argument changes, we use scaling rather than modulo to produce an answer
       inside the range requested.  This means that high return values in one
       game will tend to correspond to high return values in other games. */
    uint64_t unbiased_maximum =
        ((uint64_t)0x100000000LLU / maxplus1) * maxplus1;
    for (s = 0; s < 8; s++) {
        if (out[s] < unbiased_maximum)
            return out[s] / (unbiased_maximum / maxplus1);
    }

    return rn2_from_seedarray(maxplus1, seedarray);
}

int
rn2_on_rng(int maxplus1, enum rng rng)
{
    if (maxplus1 <= 0) {
        impossible("RNG range has less than 1 value");
        return 0;
    }

    if (maxplus1 > (int)0x7FFFFFFFL)
        panic("Randomizing over an unportably large range");

    if (rng == rng_display) {

        /* A special case; the seed's not stored in the save file, nor is it
           meant to be. So we can use a static variable. Also, there's no reason
           for the sequence to be particularly secure, so we can start at 0. */
        static unsigned char display_rng_seed[RNG_SEED_SIZE_BYTES] = {0};

        return (int)rn2_from_seedarray(maxplus1, display_rng_seed);

    } else if (rng == rng_initialseed) {

        panic("Attempt to change the initial RNG seed using rn2_on_rng");

    } else if (rng < first_level_rng + NUMBER_OF_LEVEL_RNGS && rng >= 0) {

        if (program_state.in_zero_time_command &&
            !program_state.gameover && !turnstate.generating_bones)
            impossible("Zero-time command used main RNG");

        return (int)rn2_from_seedarray(maxplus1,
            flags.rngstate + rng * RNG_SEED_SIZE_BYTES);

    } else {

        panic("Random numbers are being read from nonexistent RNG");

    }
}

/* Wrapper for functions that take an RNG as an argument. */
int
rn2_on_display_rng(int x)
{
    return rn2_on_rng(x, rng_display);
}


/* Moved here from rnd.h, because a) it's perhaps a little too complex to be
   inline, and b) so that rnd.h doesn't need dependencies on things like
   you.h. */
int
rnl(int x)
{
    int i;

    i = rn2(x);

    if (Luck && rn2(50 - Luck)) {
        i -= (x <= 15 && Luck >= -5 ? Luck / 3 : Luck);
        if (i < 0)
            i = 0;
        else if (i >= x)
            i = x - 1;
    }

    return i;
}

enum rng
rng_for_level(const d_level *lev)
{
    return (ledger_no(lev) % NUMBER_OF_LEVEL_RNGS) + first_level_rng;
}

int
mklev_rn2(int x, struct level *lev)
{
    return rn2_on_rng(x, rng_for_level(&lev->z));
}
