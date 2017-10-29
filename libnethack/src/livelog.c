/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-29 */
/* Copyright (c) 2014 Patric Mueller */
/* Retrieved from https://github.com/bhaak/nethack-de/blob/master/patches/nethack.unfoog.de-livelog.patch */

/* Write live game progress changes to a log file */

#include "hack.h"
#include <fcntl.h>
#include <inttypes.h>

void livelog_write_string(const char *);
static void munge_llstring(char *, const char *, int);

#define ENTRYSIZE 99

static void
munge_llstring(char *dest, const char *src, int n)
{
    int i;
    for (i = 0; i < (n - 1) && src[i] != '\0'; i++) {
        if (src[i] == ':' || src[i] == '\n')
            dest[i] = '_';
        else
            dest[i] = src[i];
    }
    dest[i] = '\0';
    return;
}


/* The original livelog patch has achieve() stuff here, but we're
 * going to skip that and use the historic_event mechanism instead. */

/* Locks the live log file and writes 'buffer' */
void
livelog_write_string(const char *buffer) {
    if (program_state.followmode != FM_PLAY)
        return;

    FILE* livelogfile;
    
    int fd = open_datafile(LIVELOG,
                           O_CREAT | O_APPEND | O_WRONLY,
                           SCOREPREFIX);
    if (fd < 0)
        panic("Failed to write to livelog.  Is it writable?");

    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        livelogfile = fdopen(fd, "a");

        fprintf(livelogfile, "%s", buffer);
        fprintf(livelogfile, "\n");

        change_fd_lock(fd, FALSE, LT_NONE, 0);
        (void) fclose(livelogfile); /* also closes fd */
    }
}

void
livelog_write_event(const char *buffer) {
    const char *uname;
    uname = nh_getenv("NH4SERVERUSER");
    char buf[ENTRYSIZE + 1];
    if (!uname)
        uname = nh_getenv("USER");
    if (!uname)
        uname = "";
    munge_llstring(buf, uname, ENTRYSIZE + 1);
    livelog_write_string(msgprintf("version=%d.%d.%d:player=%s:charname=%s:"
                                   "turns=%1d:depth=%1d:%s:eventdate=%ld:"
                                   "uid=%d:role=%s:race=%s:gender=%s:"
                                   "align=%s:birthdate=%ld:"
                                   "starttime=%" PRIdLEAST64 ":"
                                   "eventtime=%" PRIdLEAST64 ":"
                                   "xplevel=%1d:mode=%s",
                                   VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL,
                                   buf, u.uplname, moves, depth(&u.uz), buffer,
                                   yyyymmdd(utc_time()), getuid(),
                                   urole.filecode, urace.filecode,
                                   genders[u.ufemale].filecode,
                                   aligns[1 - u.ualign.type].filecode,
                                   (unsigned long)yyyymmdd(u.ubirthday) ,
                                   ((int_least64_t)u.ubirthday / 1000000L),
                                   ((int_least64_t)utc_time() / 1000000L),
                                   u.ulevel, (flags.debug ? "debug" :
                                              flags.explore ? "explore" :
                                              *flags.setseed ? "setseed" :
                                              (flags.polyinit_mnum != -1) ?
                                              "polyinit" : "normal")));
}

void
livelog_wish(const char *wishstring)
{
    char buf[ENTRYSIZE + 1];
    munge_llstring(buf, wishstring, ENTRYSIZE + 1);
    livelog_write_event(msgprintf("type=wish:wish_count=%d:wish=%s",
                                  (u.uconduct[conduct_wish] + 1), wishstring));
}

void livelog_flubbed_wish(const char *wishstring, const struct obj *result)
{
    char buf[ENTRYSIZE + 1];
    munge_llstring(buf, wishstring, ENTRYSIZE + 1);
    livelog_write_event(msgprintf("type=flubbedwish:wish_count=%d:wish=%s:result=%s",
                                  (u.uconduct[conduct_wish] + 1), wishstring,
                                  aobjnam(result, NULL)));
}

void
livelog_unique_monster(const struct monst *mon) {
    int pm    = monsndx(mon->data);
    const char *name = mx_name(mon);
    if ((pm == PM_MEDUSA || pm == PM_WIZARD_OF_YENDOR ||
         pm == PM_VLAD_THE_IMPALER || pm == PM_DEMOGORGON ||
         pm == PM_CROESUS || pm == urole.neminum ||
         (pm == PM_HIGH_PRIEST &&
          mon->maligntyp == A_NONE))
        /* If high priests were followers, it would be possible to kill the High
         * Priest of Moloch without getting it livelogged, by A) stealing the
         * amulet from him and B) getting him to follow you to Astral so you can
         * C) kill one of the other high priests first.  This seems
         * unimportant. */
        && (mvitals[monsndx(mon->data)].died == 1))
        livelog_write_event(msgprintf("defeated=%s", noit_mon_nam(mon)));
    else if (mon->former_player > 0) {
        /* $player killed the $bones_monst of $bones_killed the former
         * $bones_rank on $turns on dungeon level $dlev! */
        int   frace, falign, fgend, frank;
        short fplayer = mon->former_player;
        struct Role frole;
        fplayer       = fplayer - (fplayer % 2); /* Throw away the 1. */
        fgend         = (fplayer % 8) / 2;
        fplayer       = fplayer - fgend;
        falign        = (fplayer % 32) / 8;
        fplayer       = fplayer - falign;
        frace         = (fplayer % 256) / 32;
        frole         = roles[(fplayer - frace) / 256];
        frank         = xlev_to_rank(mon->m_lev); /* properly handles levels beyond 1-30 */

        livelog_write_event(msgprintf("bones_killed=%s:bones_align=%s:"
                                      "bones_race=%s:bones_gender=%s:"
                                      "bones_role=%s:bones_rank=%s:"
                                      "bones_monst=%s",
                                      name, aligns[falign].adj,
                                      races[frace].adj, genders[fgend].adj,
                                      (genders[fgend].allow == ROLE_FEMALE
                                       ? (frole.name.f ? frole.name.f
                                                       : frole.name.m)
                                       : (frole.name.m)),
                                      (genders[fgend].allow == ROLE_FEMALE
                                       ? (frole.rank[frank].f ? frole.rank[frank].f
                                                              : frole.rank[frank].m)
                                       : (frole.rank[frank].m)),
                                      pm_name(mon)));
    }
}
