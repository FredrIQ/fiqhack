/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Nathan Eady, 2015-04-04 */
/* Copyright (c) 2014 Patric Mueller */
/* Retrieved from https://github.com/bhaak/nethack-de/blob/master/patches/nethack.unfoog.de-livelog.patch */

/* Write live game progress changes to a log file */

#include "hack.h"
#include "epri.h"
#include <fcntl.h>

void livelog_write_string(const char *);

/* The original livelog patch has achieve() stuff here, but we're
 * going to skip that and use the historic_event mechanism instead. */

/* Locks the live log file and writes 'buffer' */
void
livelog_write_string(const char *buffer) {
    FILE* livelogfile;
    int fd = open_datafile(LIVELOG,
                           O_CREAT | O_APPEND | O_WRONLY,
                           SCOREPREFIX);
    if (fd < 0)
        panic("Failed to write to livelog.  Is it writable?");

    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        livelogfile = fdopen(fd, "a");

        fprintf(livelogfile, buffer);
        fprintf(livelogfile, "\n");

        change_fd_lock(fd, FALSE, LT_NONE, 0);
        (void) fclose(livelogfile); /* also closes fd */
    }
}

#define ENTRYSIZE 99

void
livelog_write_event(const char *buffer) {
    const char *uname;
    uname = nh_getenv("NH4SERVERUSER");
    char buf[ENTRYSIZE + 1];
    int i;
    if (!uname)
        uname = nh_getenv("USER");
    if (!uname)
        uname = "";
    for (i = 0; i < ENTRYSIZE && uname[1] != '\0'; i++) {
        if (uname[i] == ':' || uname[i] == '\n')
            buf[i] = '_';
        else
            buf[i] = uname[i];
    }
    livelog_write_string(msgprintf(
                             "player=%s:character=%s:turns=%1d:depth=%1d:%s",
                             buf, u.uplname, moves, depth(&u.uz), buffer));
}

void
livelog_unique_monster(const struct monst *mon) {
    int pm    = monsndx(mon->data);
#ifdef LIVELOG_BONES_KILLER
    const char *name = NAME(mon);
#endif
    if ((pm == PM_MEDUSA || pm == PM_WIZARD_OF_YENDOR ||
         pm == PM_VLAD_THE_IMPALER || pm == PM_DEMOGORGON ||
         pm == PM_CROESUS || pm == urole.neminum ||
         (pm == PM_HIGH_PRIEST &&
          CONST_EPRI(mon)->shralign == A_NONE))
        /* If high priests were followers, it would be possible to kill the High
         * Priest of Moloch without getting it livelogged, by A) stealing the
         * amulet from him and B) getting him to follow you to Astral so you can
         * C) kill one of the other high priests first.  This seems
         * unimportant. */
        && (mvitals[monsndx(mon->data)].died == 1))
        livelog_write_event(msgprintf("defeated=%s", noit_mon_nam(mon)));
#ifdef LIVELOG_BONES_KILLER
    /* We abuse the fact that ROLE_MALE, ROLE_FEMALE, and ROLE_NEUTER
     * are all true values (1, 2, and 4, respectively).  It's done that
     * way because they're used in a bitmask to control which genders
     * a given role is allowed to be, so none of them can be zero.
     * This has the pleasant side-effect of allowing us to test the
     * former_gender here, since the bones code sets former_gender
     * to the allow field of the gender structure.  The consequence
     * is, we have to loop through the genders and find the right one. */
    else if (name && mon->former_gender) {
        /* $player killed the $bones_monst of $bones_killed the former
         * $bones_rank on $turns on dungeon level $dlev! */
        int gend, i;
        const struct Role frole = roles[mon->former_role];
        for (i = 0; i <= 2; i++)
            if (genders[i].allow == mon->former_gender)
                gend = i;
        livelog_write_event(msgprintf("bones_killed=%s:bones_align=%s:"
                                      "bones_race=%s:bones_gender=%s:"
                                      "bones_role=%s:bones_monst=%s",
                                      name, aligns[mon->former_align].adj,
                                      races[mon->former_race].adj,
                                      genders[gend].adj,
                                      ((((short)mon->former_gender)
                                        == (short) ROLE_FEMALE)
                                       ? (frole.name.f ? frole.name.f
                                                       : frole.name.m)
                                       : (frole.name.m)),
                                      mons[monsndx(mon->data)].mname));
    }
#endif /* LIVELOG_BONES_KILLER */
}


