/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "patchlevel.h"
#include "quest.h"
#include "dlb.h"

#include <fcntl.h>
#include <inttypes.h>

/* 10000 highscore entries should be enough for _anybody_
 * <500 bytes per entry * 10000 ~= 5MB max file size. Seems reasonable. */
#define TTLISTLEN 10000

/* maximum number of highscore entries per player */
#define PLAYERMAX 1000

#define ROLESZ  3
#define NAMSZ   15
#define DTHSZ   99

struct toptenentry {
    int points;
    int deathdnum, deathlev;
    int maxlvl, hp, maxhp, deaths;
    int ver_major, ver_minor, patchlevel;
    int deathdate, birthdate;
    microseconds deathtime;
    int uid;
    int moves;
    int how;
    char plrole[ROLESZ + 1];
    char plrace[ROLESZ + 1];
    char plgend[ROLESZ + 1];
    char plalign[ROLESZ + 1];
    char name[NAMSZ + 1];
    char death[DTHSZ + 1];
};

#define validentry(x) ((x).points > 0 || (x).deathlev)

static void writeentry(int fd, const struct toptenentry *tt);
static void write_topten(int fd, const struct toptenentry *ttlist);
static void update_log(const struct toptenentry *newtt);
static boolean readentry(char *line, struct toptenentry *tt);
static struct toptenentry *read_topten(int fd, int limit);
static void fill_topten_entry(struct toptenentry *newtt, int how,
                              const char *killer);
static boolean toptenlist_insert(struct toptenentry *ttlist,
                                 struct toptenentry *newtt);
static int classmon(char *plch, boolean fem);
static void topten_death_description(struct toptenentry *in, char *outbuf);
static void fill_nh_score_entry(struct toptenentry *in,
                                struct nh_topten_entry *out, int rank,
                                boolean highlight);


static int end_how;
static char end_killer[DTHSZ + 1] = {0};

/* xlogfile writing. Based on the xlogfile patch by Aardvark Joe. */

#define SEP ":"
#define SEPC (SEP[0])

static void
munge_xlstring(char *dest, const char *src, int n)
{
    int i;

    for (i = 0; i < (n - 1) && src[i] != '\0'; i++) {
        if (src[i] == SEPC || src[i] == '\n')
            dest[i] = '_';
        else
            dest[i] = src[i];
    }

    dest[i] = '\0';

    return;
}

static unsigned long
encode_uevent(void)
{
    unsigned long c = 0UL;

    /* game plot events */
    if (u.uevent.minor_oracle || u.uevent.major_oracle)
        c |= 0x0001UL;  /* any Oracle consultation */
    if (u.uevent.qcalled)
        c |= 0x0002UL;  /* reached quest portal level */
    if (u.quest_status.got_quest || u.quest_status.got_thanks)
        c |= 0x0004UL;  /* was accepted for quest */
    if (u.uevent.qcompleted)
        c |= 0x0008UL;  /* showed quest arti to leader */
    if (u.uevent.uopened_dbridge)
        c |= 0x0010UL;  /* opened/destroyed Castle drawbridge */
    if (u.uevent.gehennom_entered)
        c |= 0x0020UL;  /* entered Gehennom the front way */
    if (u.uevent.udemigod)
        c |= 0x0040UL;  /* provoked Rodney's wrath */
    if (u.uevent.invoked)
        c |= 0x0080UL;  /* did the invocation */
    if (u.uevent.ascended)
        c |= 0x0100UL;  /* someone needs to use this variable */

    /* notable other events */
    if (u.uevent.uhand_of_elbereth)
        c |= 0x0200UL;  /* was crowned */

    /* boss kills */
    if (u.quest_status.killed_nemesis)
        c |= 0x0400UL;  /* defeated quest nemesis */
    if (mvitals[PM_CROESUS].died)
        c |= 0x0800UL;  /* defeated Croesus */
    if (mvitals[PM_MEDUSA].died)
        c |= 0x1000UL;  /* defeated Medusa */
    if (mvitals[PM_VLAD_THE_IMPALER].died)
        c |= 0x2000UL;  /* defeated Vlad */
    if (mvitals[PM_WIZARD_OF_YENDOR].died)
        c |= 0x4000UL;  /* defeated Rodney */
    if (mvitals[PM_HIGH_PRIEST].died)
        c |= 0x8000UL;  /* defeated a high priest */

    return c;
}

unsigned long
encode_carried(void)
{
    unsigned long c = 0UL;

    /* this encodes important items potentially owned by the player at the time
       of death */
    if (Uhave_amulet)
        c |= 0x0001UL;  /* real Amulet of Yendor */
    if (Uhave_bell)
        c |= 0x0002UL;  /* Bell of Opening */
    if (Uhave_book)
        c |= 0x0004UL;  /* Book of the Dead */
    if (Uhave_menorah)
        c |= 0x0008UL;  /* Candelabrum of Invocation */
    if (Uhave_questart)
        c |= 0x0010UL;  /* own quest artifact */

    return c;
}

static unsigned long
encode_birthoptions(void)
{
    /* This function encodes birth options (excluding ones that have their own
       xlog fields). Compare to the list in options.c. */
    unsigned long c = 0UL;
    if (flags.elbereth_enabled)
        c |= 0x0001UL;
    if (flags.rogue_enabled)
        c |= 0x0002UL;
    if (flags.seduce_enabled)
        c |= 0x0004UL;
    if (flags.bones_enabled)
        c |= 0x0008UL;
    if (flags.permablind)
        c |= 0x0010UL;
    if (flags.permahallu)
        c |= 0x0020UL;
    /* leaving bits open here for permaconf and one other impairment */
    if (flags.polyinit_mnum != -1)
        c |= 0x0100UL;

    return c;
}

static_assert(num_conducts <= 32,
              "Too many conducts for encode_conduct to encode");
static unsigned long
encode_conduct(void)
{
    enum player_conduct cond = conduct_first;
    unsigned long c = 0UL;

    for(; cond < num_conducts; cond++) {
        if(!u.uconduct[cond])
            c |= 1UL << cond;
    }
    return c;
}

static void
write_xlentry(FILE * rfile, const struct toptenentry *tt,
              unsigned long carried, const char *dumpname)
{
    char buf[DTHSZ + 1];
    char rngseedbuf[RNG_SEED_SIZE_BASE64];
    int i;
    const char *uname;

    /* regular logfile data */
    fprintf(rfile,
            "version=%d.%d.%d" SEP "points=%d" SEP "deathdnum=%d" SEP
            "deathlev=%d" SEP "maxlvl=%d" SEP "hp=%d" SEP "maxhp=%d" SEP
            "deaths=%d" SEP "deathdate=%ld" SEP "birthdate=%ld" SEP "uid=%d",
            tt->ver_major, tt->ver_minor, tt->patchlevel, tt->points,
            tt->deathdnum, tt->deathlev, tt->maxlvl, tt->hp, tt->maxhp,
            tt->deaths, (unsigned long)tt->deathdate,
            (unsigned long)tt->birthdate, tt->uid);

    get_initial_rng_seed(rngseedbuf);

    fprintf(rfile, SEP "rngseed=%.*s", RNG_SEED_SIZE_BASE64, rngseedbuf);

    fprintf(rfile, SEP "role=%s" SEP "race=%s" SEP "gender=%s" SEP "align=%s",
            tt->plrole, tt->plrace, tt->plgend, tt->plalign);

    uname = nh_getenv("NH4SERVERUSER");
    if (!uname)
        uname = nh_getenv("USER");
    if (!uname)
        uname = "";
    munge_xlstring(buf, uname, DTHSZ + 1);
    fprintf(rfile, SEP "name=%s", buf);

    munge_xlstring(buf, u.uplname, DTHSZ + 1);
    fprintf(rfile, SEP "charname=%s", buf);

    munge_xlstring(buf, tt->death, DTHSZ + 1);
    fprintf(rfile, SEP "death=%s", buf);

    char buf2[strlen(dumpname) + 2];
    munge_xlstring(buf2, dumpname, sizeof buf2);
    fprintf(rfile, SEP "dumplog=%s", buf2);

    fprintf(rfile, SEP "conduct=%ld", encode_conduct());

    fprintf(rfile, SEP "birthoption=%ld", encode_birthoptions());

    fprintf(rfile, SEP "extrinsic=0x");
    i = SIZE(u.ever_extrinsic);
    while(i--)
        fprintf(rfile, "%02x", u.ever_extrinsic[i]);
    fprintf(rfile, SEP "intrinsic=0x");
    i = SIZE(u.ever_intrinsic);
    while(i--)
        fprintf(rfile, "%02x", u.ever_intrinsic[i]);
    fprintf(rfile, SEP "temporary=0x");
    i = SIZE(u.ever_temporary);
    while(i--)
        fprintf(rfile, "%02x", u.ever_temporary[i]);

    fprintf(rfile, SEP "turns=%u", moves);

    /* AceHack's equivalent of achieve has rather different semantics from
       vanilla's. So give it a different name. */
    fprintf(rfile, SEP "event=%ld", encode_uevent());
    fprintf(rfile, SEP "carried=%ld", carried);

    /* This uses the same trick to print long longs portably as in log.c.
       These are now microsecond-accurate, so we use "starttimeus",
       "endtimeus". */

    fprintf(rfile, SEP "starttimeus=%" PRIdLEAST64
                   SEP "endtimeus=%" PRIdLEAST64
                   SEP "starttime=%" PRIdLEAST64
                   SEP "endtime=%" PRIdLEAST64,
            (int_least64_t)u.ubirthday, (int_least64_t)tt->deathtime,
            (int_least64_t)u.ubirthday / 1000000L,
            (int_least64_t)tt->deathtime / 1000000L);

    fprintf(rfile, SEP "gender0=%s", genders[u.initgend].filecode);
    fprintf(rfile, SEP "align0=%s",
            aligns[1 - u.ualignbase[A_ORIGINAL]].filecode);

    fprintf(rfile, SEP "xplevel=%d", u.ulevel);
    fprintf(rfile, SEP "exp=%d", u.uexp);

    fprintf(rfile, SEP "mode=%s",
            (flags.debug ? "debug" : flags.explore ? "explore" :
             *flags.setseed ? "setseed" :
             flags.polyinit_mnum != -1 ? "polyinit" : "normal"));

    fprintf(rfile, "\n");
}

#undef SEP
#undef SEPC

static void
writeentry(int fd, const struct toptenentry *tt)
{
    char buf[1024];
    int len;

    snprintf(buf, 1024, "%d.%d.%d %d %d %d %d %d %d %d %d %d %d ",
             tt->ver_major, tt->ver_minor, tt->patchlevel, tt->points,
             tt->deathdnum, tt->deathlev, tt->maxlvl, tt->hp, tt->maxhp,
             tt->deaths, tt->deathdate, tt->birthdate, tt->uid);
    len = strlen(buf);
    if (write(fd, buf, len) != len)
        panic("Failed to write topten. Out of disk?");

    snprintf(buf, 1024, "%d %d %s %s %s %s %s,%s\n", tt->moves, tt->how,
             tt->plrole, tt->plrace, tt->plgend, tt->plalign,
             onlyspace(tt->name) ? "_" : tt->name, tt->death);
    len = strlen(buf);
    if (write(fd, buf, len) != len)
        panic("Failed to write topten. Out of disk?");
}


static void
write_topten(int fd, const struct toptenentry *ttlist)
{
    int i;

    lseek(fd, 0, SEEK_SET);
    if (ftruncate(fd, 0) >= 0)
        for (i = 0; i < TTLISTLEN && validentry(ttlist[i]); i++)
            writeentry(fd, &ttlist[i]);
    else
        panic("Failed to write topten. Is the record file writable?");
}


static void
update_log(const struct toptenentry *newtt)
{
    /* used for debugging (who dies of what, where) */
    int fd = open_datafile(LOGFILE, O_CREAT | O_APPEND | O_WRONLY, SCOREPREFIX);

    if (fd < 0)
        panic("Failed to write logfile. Is it writable?");

    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        writeentry(fd, newtt);
        change_fd_lock(fd, FALSE, LT_NONE, 0);
        close(fd);
    }
}

static void
update_xlog(const struct toptenentry *newtt,
            unsigned long carried, const char *dumpname)
{
    /* used for statistical purposes and tournament scoring */
    int fd =
        open_datafile(XLOGFILE, O_CREAT | O_APPEND | O_WRONLY, SCOREPREFIX);

    if (fd < 0)
        panic("Failed to write xlogfile. Is it writable?");

    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        FILE *xlfile = fdopen(fd, "a");

        write_xlentry(xlfile, newtt, carried, dumpname);
        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(xlfile); /* also closes fd */
    }
}

static boolean
readentry(char *line, struct toptenentry *tt)
{
    /*
     * "3.4.3 77 0 1 1 0 15 1 20110727 20110727 \
     * 1000 Bar Orc Mal Cha daniel,killed by a newt"
     */
    static const char fmt[] =
        "%d.%d.%d %d %d %d %d %d %d %d %d"
        " %d %d %d %d %3s %3s %3s %3s %15[^,],%99[^\n]%*c";
    int rv;

    rv = sscanf(line, fmt, &tt->ver_major, &tt->ver_minor, &tt->patchlevel,
                &tt->points, &tt->deathdnum, &tt->deathlev, &tt->maxlvl,
                &tt->hp, &tt->maxhp, &tt->deaths, &tt->deathdate,
                &tt->birthdate, &tt->uid, &tt->moves, &tt->how, tt->plrole,
                tt->plrace, tt->plgend, tt->plalign, tt->name, tt->death);

    return rv == 21;
}


static struct toptenentry *
read_topten(int fd, int limit)
{
    int i, size;
    struct toptenentry *ttlist;
    char *data, *line;

    lseek(fd, 0, SEEK_SET);
    data = loadfile(fd, &size);
    if (!data)
        /* the only sensible reason for not getting any data is that the record
           file doesn't exist or is empty. If it does have data but is
           unreadable for some other reason, writing will fail later on, so
           pretending it's empty won't hurt anything. */
        return calloc(limit + 1, sizeof (struct toptenentry));

    ttlist = calloc(limit + 1, sizeof (struct toptenentry));
    line = data;
    for (i = 0; i < limit; i++) {
        if (!readentry(line, &ttlist[i]))
            break;
        line = strchr(line, '\n') + 1;
    }

    free(data);
    return ttlist;
}


static void
fill_topten_entry(struct toptenentry *newtt, int how, const char *killer)
{
    int uid = getuid();

    memset(newtt, 0, sizeof (struct toptenentry));

    /* deepest_lev_reached() is in terms of depth(), and reporting the deepest
       level reached in the dungeon death occurred in doesn't seem right, so we
       have to report the death level in depth() terms as well (which also
       seems reasonable since that's all the player sees on the screen anyway)
       */
    newtt->ver_major = VERSION_MAJOR;
    newtt->ver_minor = VERSION_MINOR;
    newtt->patchlevel = PATCHLEVEL;
    newtt->points = u.urexp > -1 ?      /* u.urexp stores score once invent is
                                           invalid */
        u.urexp : calc_score(how, FALSE, money_cnt(invent) + hidden_gold());
    newtt->deathdnum = u.uz.dnum;
    newtt->deathlev = depth(&u.uz);
    newtt->maxlvl = deepest_lev_reached(TRUE);
    newtt->hp = u.uhp;
    newtt->maxhp = u.uhpmax;
    newtt->deaths = u.umortality;
    newtt->uid = uid;
    newtt->moves = moves;
    newtt->how = how;
    strncpy(newtt->plrole, urole.filecode, ROLESZ);
    newtt->plrole[ROLESZ] = '\0';
    strncpy(newtt->plrace, urace.filecode, ROLESZ);
    newtt->plrace[ROLESZ] = '\0';
    strncpy(newtt->plgend, genders[u.ufemale].filecode, ROLESZ);
    newtt->plgend[ROLESZ] = '\0';
    strncpy(newtt->plalign, aligns[1 - u.ualign.type].filecode, ROLESZ);
    newtt->plalign[ROLESZ] = '\0';
    strncpy(newtt->name, u.uplname, NAMSZ);
    newtt->name[NAMSZ] = '\0';
    strncpy(newtt->death, killer, DTHSZ);
    newtt->death[DTHSZ] = '\0';
    newtt->deathtime = utc_time();
    newtt->birthdate = yyyymmdd(u.ubirthday);
    newtt->deathdate = yyyymmdd(newtt->deathtime);
}


static boolean
toptenlist_insert(struct toptenentry *ttlist, struct toptenentry *newtt)
{
    int i, ins, del, occ_cnt;

    occ_cnt = 0;

    for (ins = 0; ins < TTLISTLEN && validentry(ttlist[ins]); ins++) {
        if (newtt->points > ttlist[ins].points)
            break;

        if (!strncmp(newtt->name, ttlist[ins].name, NAMSZ))
            occ_cnt++;
    }

    if (occ_cnt >= PLAYERMAX || ins == TTLISTLEN)
        /* this game doesn't get onto the list */
        return FALSE;

    /* If the player already has PLAYERMAX entries in the list, find the last
       one. Otherwise del is either the first empty entry or TTLISTLEN if the
       list is full */
    for (del = ins; del < TTLISTLEN && validentry(ttlist[del]); del++) {
        if (!strncmp(newtt->name, ttlist[ins].name, NAMSZ)) {
            occ_cnt++;
            if (occ_cnt >= PLAYERMAX)
                break;
        }
    }

    if (del == TTLISTLEN)
        del--;

    /* shift the entries in the range [ins;del[ down by one, overwriting del */
    for (i = del; i > ins; i--)
        ttlist[i] = ttlist[i - 1];

    ttlist[ins] = *newtt;

    return TRUE;
}

/*
 * Add the result of the current game to the score list
 */
void
update_topten(int how, const char *killer, unsigned long carried,
              const char *dumpname)
{
    struct toptenentry *toptenlist, newtt;
    boolean need_rewrite;
    int fd;

    if (program_state.panicking)
        return;

    end_how = how;      /* save how for nh_get_topten */
    strncpy(end_killer, killer, DTHSZ);
    end_killer[DTHSZ] = '\0';

    fill_topten_entry(&newtt, how, killer);
    update_log(&newtt);
    update_xlog(&newtt, carried, dumpname);

    /* nothing more to do for non-scoring games */
    if (wizard || discover || *flags.setseed || flags.polyinit_mnum != -1)
        return;

    fd = open_datafile(RECORD, O_RDWR | O_CREAT, SCOREPREFIX);

    if (fd < 0)
        panic("Failed to write record. Is it writable?");

    if (!change_fd_lock(fd, FALSE, LT_WRITE, 30)) {
        close(fd);
        return;
    }

    toptenlist = read_topten(fd, TTLISTLEN);

    /* possibly rearrange the score list to include the new entry */
    need_rewrite = toptenlist_insert(toptenlist, &newtt);
    if (need_rewrite)
        write_topten(fd, toptenlist);

    change_fd_lock(fd, FALSE, LT_NONE, 0);
    close(fd);
    free(toptenlist);
}


static int
classmon(char *plch, boolean fem)
{
    int i;

    /* Look for this role in the role table */
    for (i = 0; roles[i].name.m; i++)
        if (!strncmp(plch, roles[i].filecode, ROLESZ)) {
            if (fem && roles[i].femalenum != NON_PM)
                return roles[i].femalenum;
            else if (roles[i].malenum != NON_PM)
                return roles[i].malenum;
            else
                return PM_HUMAN;
        }

    impossible("What weird role is this? (%s)", plch);
    return PM_HUMAN_MUMMY;
}


/*
 * Get a random player name and class from the high score list,
 * and attach them to an object (for statues or morgue corpses).
 */
struct obj *
tt_oname(struct obj *otmp)
{
    int rank, fd;
    struct toptenentry *toptenlist, *tt;

    if (!otmp)
        return NULL;

    fd = open_datafile(RECORD, O_RDONLY, SCOREPREFIX);
    if (fd < 0)
        return NULL;   /* the topten list is missing */

    toptenlist = read_topten(fd, 100);  /* load the top 100 scores */
    close(fd);

    /* try to find a valid entry, reducing the value range for rank each time */
    rank = rn2(100);
    while (!validentry(toptenlist[rank]) && rank)
        rank = rn2(rank);

    tt = &toptenlist[rank];

    if (!validentry(toptenlist[rank]))
        otmp = NULL;    /* the topten list is empty */
    else {
        /* reset timer in case corpse started out as lizard or troll */
        if (otmp->otyp == CORPSE)
            obj_stop_timers(otmp);
        otmp->corpsenm = classmon(tt->plrole, (tt->plgend[0] == 'F'));
        otmp->owt = weight(otmp);
        otmp = oname(otmp, tt->name);
        if (otmp->otyp == CORPSE)
            start_corpse_timeout(otmp);
    }

    free(toptenlist);
    return otmp;
}


/* append the level name to outbuf */
static void
topten_level_name(int dnum, int dlev, char *outbuf)
{
    if (dnum == astral_level.dnum) {
        const char *arg, *fmt = "on the Plane of %s";

        switch (dlev) {
        case -5:
            fmt = "on the %s Plane";
            arg = "Astral";
            break;
        case -4:
            arg = "Water";
            break;
        case -3:
            arg = "Fire";
            break;
        case -2:
            arg = "Air";
            break;
        case -1:
            arg = "Earth";
            break;
        default:
            arg = "Void";
            break;
        }
        sprintf(outbuf + strlen(outbuf), fmt, arg);
    } else {
        sprintf(outbuf + strlen(outbuf), "in %s",
                gamestate.dungeons[dnum].dname);
        if (dnum != knox_level.dnum)
            sprintf(outbuf + strlen(outbuf), " on level %d", dlev);
    }
}


static void
topten_death_description(struct toptenentry *in, char *outbuf)
{
    char *bp;
    boolean second_line = TRUE;

    outbuf[0] = '\0';

    sprintf(outbuf, "%.16s %s-%s-%s-%s ", in->name, in->plrole, in->plrace,
            in->plgend, in->plalign);

    if (!strncmp("escaped", in->death, 7)) {
        sprintf(outbuf + strlen(outbuf),
                "escaped the dungeon %s[max level %d]",
                !strncmp(" (", in->death + 7, 2) ? in->death + 7 + 2 : "",
                in->maxlvl);
        /* fixup for closing paren in "escaped... with...Amulet)[max..." */
        if ((bp = strchr(outbuf, ')')) != 0)
            *bp = (in->deathdnum == astral_level.dnum) ? '\0' : ' ';
        second_line = FALSE;
    } else if (!strncmp("ascended", in->death, 8)) {
        sprintf(outbuf + strlen(outbuf), "ascended to demigod%s-hood",
                (in->plgend[0] == 'F') ? "dess" : "");
        second_line = FALSE;
    } else {
        if (!strncmp(in->death, "quit", 4)) {
            strcat(outbuf, "quit");
            second_line = FALSE;
        } else if (!strncmp(in->death, "died of st", 10)) {
            strcat(outbuf, "starved to death");
            second_line = FALSE;
        } else if (!strncmp(in->death, "choked", 6)) {
            sprintf(outbuf + strlen(outbuf), "choked on h%s food",
                    (in->plgend[0] == 'F') ? "er" : "is");
        } else if (!strncmp(in->death, "poisoned", 8)) {
            strcat(outbuf, "was poisoned");
        } else if (!strncmp(in->death, "crushed", 7)) {
            strcat(outbuf, "was crushed to death");
        } else if (!strncmp(in->death, "petrified by ", 13)) {
            strcat(outbuf, "turned to stone");
        } else
            strcat(outbuf, "died");

        strcat(outbuf, " ");
        topten_level_name(in->deathdnum, in->deathlev, outbuf);
        if (in->deathlev != in->maxlvl)
            sprintf(outbuf + strlen(outbuf), " [max %d]", in->maxlvl);

        /* kludge for "quit while already on Charon's boat" */
        if (!strncmp(in->death, "quit ", 5))
            strcat(outbuf, in->death + 4);
    }
    strcat(outbuf, ".");

    /* Quit, starved, ascended, and escaped contain no second line */
    if (second_line)
        sprintf(outbuf + strlen(outbuf),
                "  %c%s.", highc(*(in->death)), in->death + 1);
}


static void
fill_nh_score_entry(struct toptenentry *in, struct nh_topten_entry *out,
                    int rank, boolean highlight)
{

    int rolenum = str2role(in->plrole);
    int racenum = str2race(in->plrace);
    int gendnum = str2gend(in->plgend);
    int alignnum = str2align(in->plalign);

    if (rolenum == ROLE_NONE || racenum == ROLE_NONE || gendnum == ROLE_NONE ||
        alignnum == ROLE_NONE)
        return;

    out->rank = rank;
    out->points = in->points;
    out->maxlvl = in->maxlvl;
    out->hp = in->hp;
    out->maxhp = in->maxhp;
    out->deaths = in->deaths;
    out->ver_major = in->ver_major;
    out->ver_minor = in->ver_minor;
    out->patchlevel = in->patchlevel;
    out->highlight = highlight;
    out->moves = in->moves;
    out->end_how = in->how;

    strncpy(out->name, in->name, NAMSZ);
    strncpy(out->death, in->death, DTHSZ);

    if (gendnum == 1 && roles[rolenum].name.f)
        strncpy(out->plrole, roles[rolenum].name.f, PLRBUFSZ);
    else
        strncpy(out->plrole, roles[rolenum].name.m, PLRBUFSZ);

    strncpy(out->plrace, races[racenum].noun, PLRBUFSZ);
    strncpy(out->plgend, genders[gendnum].adj, PLRBUFSZ);
    strncpy(out->plalign, aligns[alignnum].adj, PLRBUFSZ);

    topten_death_description(in, out->entrytxt);
}


struct nh_topten_entry *
nh_get_topten(int *out_len, char *statusbuf, const char *volatile player,
              int top, int around, boolean own)
{
    struct toptenentry *ttlist, newtt;
    struct nh_topten_entry *score_list;
    boolean game_inited = (wiz1_level.dlevel != 0);
    boolean game_complete = game_inited && moves && program_state.gameover;
    int rank = -1;      /* index of the completed game in the topten list */
    int fd, i, j, sel_count;
    boolean *selected;
    volatile boolean off_list = FALSE;

    statusbuf[0] = '\0';
    *out_len = 0;

    API_ENTRY_CHECKPOINT_RETURN_ON_ERROR(NULL);

    xmalloc_cleanup(&api_blocklist);

    if (!game_inited) {
        /* If nh_get_topten() isn't called after a game, we never went through
           initialization. */
        dlb_init();
        init_dungeons();
    }

    if (!player) {
        if (game_complete)
            player = u.uplname;
        else
            player = "";
    }

    fd = open_datafile(RECORD, O_RDONLY, SCOREPREFIX);
    if (fd >= 0) {
        ttlist = read_topten(fd, TTLISTLEN);
        close(fd);
    } else
        ttlist = NULL;

    if (!ttlist) {
        strcpy(statusbuf, "Cannot open record file!");

        API_EXIT();
        return NULL;
    }

    /* find the rank of a completed game in the score list */
    if (game_complete && !strcmp(player, u.uplname)) {
        fill_topten_entry(&newtt, end_how, end_killer);

        /* find this entry in the list */
        for (i = 0; i < TTLISTLEN && validentry(ttlist[i]); i++)
            if (!memcmp(&ttlist[i], &newtt, sizeof (struct toptenentry)))
                rank = i;

        /* TODO: Perhaps we could have a different top ten list for play on a
           particular set seed (seed of the week, as it were). But there's too
           much scope for cheating involved in that, so it's probably best that
           people make their leaderboards "unofficially". */
        if (wizard || discover || *flags.setseed || flags.polyinit_mnum != -1)
            sprintf(statusbuf,
                    "Since you were in %s mode, your game was not "
                    "added to the score list.",
                    wizard ? "debug" : discover ? "explore" :
                    *flags.setseed ? "set seed" : "polyinit");
        else if (rank >= 0 && rank < 10)
            sprintf(statusbuf, "You made the top ten list!");
        else if (rank >= 0)
            sprintf(statusbuf, "You reached the %d%s place on the score list.",
                    rank + 1, ordin(rank + 1));
    }

    /* select scores for display */
    sel_count = 0;
    selected = calloc(TTLISTLEN, sizeof (boolean));

    for (i = 0; i < TTLISTLEN && validentry(ttlist[i]); i++) {
        if (top == -1 || i < top)
            selected[i] = TRUE;

        if (own && !strcmp(player, ttlist[i].name))
            selected[i] = TRUE;

        if (rank != -1 && rank - around <= i && i <= rank + around)
            selected[i] = TRUE;

        if (selected[i])
            sel_count++;
    }

    if (game_complete && sel_count == 0) {
        /* didn't make it onto the list and nothing else is selected */
        ttlist[0] = newtt;
        selected[0] = TRUE;
        sel_count++;
        off_list = TRUE;
    }


    score_list = xmalloc(&api_blocklist,
                         sel_count * sizeof (struct nh_topten_entry));
    memset(score_list, 0, sel_count * sizeof (struct nh_topten_entry));
    *out_len = sel_count;
    j = 0;
    for (i = 0; i < TTLISTLEN && validentry(ttlist[i]); i++) {
        if (selected[i])
            fill_nh_score_entry(&ttlist[i], &score_list[j++], i + 1, i == rank);
    }

    if (off_list) {
        score_list[0].rank = -1;
        score_list[0].highlight = TRUE;
    }

    if (!game_inited) {
        free_dungeon();
        dlb_cleanup();
    }

    free(selected);
    free(ttlist);

    API_EXIT();
    return score_list;
}


/* topten.c */

