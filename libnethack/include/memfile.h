/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-22 */
/* Copyright (c) 2015 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MEMFILE_H
# define MEMFILE_H

# define MEMFILE_HASHTABLE_SIZE 1009

/* SAVEBREAK (4.3-beta1 -> 4.3-beta2): these constants are only needed to parse
   the old -beta1 diff format. */
# define MDIFFOLD_SEEK    0
# define MDIFFOLD_EDIT    2
# define MDIFFOLD_COPY    3
# define MDIFFOLD_INVALID 255

/* SAVEBREAK (4.3-beta2 -> 4.3-beta3): and these commands are only needed to
   parse the old -beta2 diff format. */
# define MDIFF_CMDMASK        0xE000u
# define MDIFF_LARGE_COPYEDIT 0x8000u
# define MDIFF_LARGE_EDIT     0xA000u
# define MDIFF_LARGE_SEEK     0xC000u
# define MDIFF_SEEK           0xE000u
/* and the other four possibilities are all normal-sized copyedits */

/* Save diff format magic numbers. */
# define MDIFF_HEADER_0       0x01
# define MDIFF_HEADER_1_BETA2 0x40 /* SAVEBREAK (4.3-beta2 -> 4.3-beta3) */
# define MDIFF_HEADER_1_BETA3 0x43

/* The number of bytes that save_mon most commonly outputs.

   Some special-purpose commands, like mdiff_coord, use this to optimize the
   representation of distances that are likely to be between a particular field
   of one monster and the same field of another. */
# define SAVE_SIZE_MONST      85

/* Commands in a 4.3-beta3-format save diff. See saves.txt (section "4.3-beta3")
   for full information on how the format works. Note that the order of this
   array affects save compatibility. (Adding new items will just before
   mdiff_command_count will not prevent old saves being loaded correctly,
   although it will prevent new saves being loaded correctly on old
   versions.) */
enum mdiff_command {
    /* General-purpose commands. */
    mdiff_copyedit,         /* 12+ bits copy size; 2 bits edit size (+1); edited
                               data is given explicitly */

    /* Fallbacks, so that we can encode anything if need be. */
    mdiff_copy,             /* 6+ bits copy size */
    mdiff_edit,             /* 4+ bits edit size (+1); edited data is given
                               explicitly */
    mdiff_seek,             /* 8+ bits signed seek distance */
    mdiff_wider,            /* prefix on a command to increase the minimum size
                               of the varying-sized argument by 8 */

    /* Special-purpose commands. */
    mdiff_eof,              /* end of the diff; 12+ bits copy size before
                               ending the diff */
    mdiff_coord,            /* 1+ bits copy size in SAVE_SIZE_MONST units from
                               the start of the last mdiff_coord or end of the
                               last mdiff_copy (with 0 being the smallest
                               possible such copy); then edits 1 byte (for DIR_W
                               and DIR_E) or 2 bytes (in other cases), with
                               their values being changed according to 3 bits of
                               movement direction (DIR_W .. DIR_SW) */
    mdiff_erase,            /* 7+ bits copy size; 3 bits edit size (+3); edited
                               data is all zeroes */
    mdiff_increment,        /* 12+ bits copy size; then edits one byte,
                               incrementing it by 1 */
    mdiff_copyedit1,        /* 4+ bits copy size; 0 bits edit size (+1);
                               basically a copyedit with different sizes, which
                               can lead to shorter encodings sometimes */
    mdiff_eof_crc32,        /* end of the diff, plus consistency check; 12+ bits
                               copy size before ending the diff, 32 bits CRC */
    mdiff_rle,              /* repeat previous command 2+ bits (+1) more
                               times */
    mdiff_command_count     /* fencepost, comes last; also means "no command" */
};
struct mdiff_command_instance {
    enum mdiff_command command;
    long long arg1;
    unsigned long long arg2;
};

enum memfile_tagtype {
    MTAG_START,             /* 0 */
    MTAG_WATERLEVEL,
    MTAG_DUNGEONSTRUCT,
    MTAG_BRANCH,
    MTAG_DUNGEON,
    MTAG_REGION,            /* 5 */
    MTAG_YOU,
    MTAG_VERSION,
    MTAG_WORMS,
    MTAG_ROOMS,
    MTAG_HISTORY,           /* 10 */
    MTAG_ORACLES,
    MTAG_TIMER,
    MTAG_TIMERS,
    MTAG_LIGHT,
    MTAG_LIGHTS,            /* 15 */
    MTAG_OBJ,
    MTAG_TRACK,
    MTAG_OCLASSES,
    MTAG_RNDMONST,
    MTAG_MON,               /* 20 */
    MTAG_STEAL,
    MTAG_ARTIFACT,
    MTAG_RNGSTATE,
    MTAG_LEVEL,
    MTAG_LEVELS,            /* 25 */
    MTAG_MVITALS,
    MTAG_GAMESTATE,
    MTAG_DAMAGE,
    MTAG_DAMAGEVALUE,
    MTAG_TRAP,              /* 30 */
    MTAG_FRUIT,
    MTAG_ENGRAVING,
    MTAG_FLAGS,
    MTAG_STAIRWAYS,
    MTAG_LFLAGS,            /* 35 */
    MTAG_LOCATIONS,
    MTAG_OPTION,
    MTAG_OPTIONS,
    MTAG_AUTOPICKUP_RULE,
    MTAG_AUTOPICKUP_RULES,  /* 40 */
    MTAG_DUNGEON_TOPOLOGY,
    MTAG_SPELLBOOK,
};
struct memfile_tag {
    struct memfile_tag *next;
    long tagdata;
    enum memfile_tagtype tagtype;
    int pos;
};
struct memfile {
    /* The basic information: the buffer, its length, and the file position */
    char *buf;
    int len;
    int pos;

    /* Difference memfiles are relative to another memfile; and they contain
       both the actual data in buf, and the diffed data in diffbuf */
    struct memfile *relativeto;
    char *diffbuf;
    int difflen;
    int diffpos;
    int relativepos;    /* pos corresponds to relativepos in diffbuf */

    /* Working space for diff encoding. We can have both pending copies and
       pending edits (in which case the copies come first). Pending seeks are
       mutually exclusive with anything else. We therefore flush if we need to
       copy or edit when we have pending seeks, copy when we have pending edits
       or seeks, or seek when we have pending edits or copies. */
    long pending_copies;
    long pending_edits;
    long pending_seeks;

    /* In order to be able to encode "copy the last command", we need to know
       what the last command was. */
    struct mdiff_command_instance last_command;
    /* And in order to encode any commands, we need to know what order they're
       in. */
    enum mdiff_command mdiff_command_mru[mdiff_command_count];

    /* The coord command is based on the end position of the last mdiff_copy, or
       start position of the last mdiff_coord. If this field is ever lower than
       pos, it's conceptually increased by the smallest multiple of
       SAVE_SIZE_MONST that makes it greater than or equal to pos. In order to
       save half the code needing to know about this, we do this update lazily
       (luckily, it's idempotent).

       This also means that updating this on mdiff_coord is pointless; because
       that can only move the value it "should" have forwards by a multiple of
       SAVE_SIZE_MONST anyway, we can leave it at its old value without
       problems. Thus, the only times it needs updating are if you need an
       up-to-date value, and when handling an mdiff_copy instruction. */
    int coord_relative_to;
    /* We can compress the save file better if we know, for monsters, what's an
       x coordinate and what's a y coordinate. This stores that information;
       it's the position of the last hinted x coordinate (the corresponding y
       coordinate is in the byte afterwards). */
    int mon_coord_hint;

    /* Tags to help in diffing. This is a hashtable for efficiency, using
       chaining in the case of collisions. */
    struct memfile_tag *tags[MEMFILE_HASHTABLE_SIZE];

    /* Where we are "semantically", for debug purposes. (It's possible this
       could someday be used to construct better error messages, too, but so
       far it isn't.) */
    struct memfile_tag *last_tag;
};

#endif
