/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "patchlevel.h"
#include "dlb.h"

#include <fcntl.h>

/* 10000 highscore entries should be enough for _anybody_
 * <500 bytes per entry * 10000 ~= 5MB max file size. Seems reasonable. */
#define TTLISTLEN 10000

/* maximum number of highscore entries per player */
#define PLAYERMAX 1000

#define ROLESZ	3
#define NAMSZ	15
#define DTHSZ	99

struct toptenentry {
    int points;
    int deathdnum, deathlev;
    int maxlvl, hp, maxhp, deaths;
    int ver_major, ver_minor, patchlevel;
    int deathdate, birthdate;
    int uid;
    int moves;
    int how;
    char plrole[ROLESZ+1];
    char plrace[ROLESZ+1];
    char plgend[ROLESZ+1];
    char plalign[ROLESZ+1];
    char name[NAMSZ+1];
    char death[DTHSZ+1];
};

#define validentry(x) ((x).points > 0 || (x).deathlev)

static void writeentry(int fd, const struct toptenentry *tt);
static void write_topten(const struct toptenentry *ttlist);
static void update_log(const struct toptenentry *newtt);
static boolean readentry(char *line, struct toptenentry *tt);
static struct toptenentry *read_topten(int limit);
static void fill_topten_entry(struct toptenentry *newtt, int how);
static boolean toptenlist_insert(struct toptenentry *ttlist, struct toptenentry *newtt);
static int classmon(char *plch, boolean fem);
static void topten_death_description(struct toptenentry *in, char *outbuf);
static void fill_nh_score_entry(struct toptenentry *in, struct nh_topten_entry *out,
				int rank, boolean highlight);


/* must fit with end.c; used in rip.c */
const char * const killed_by_prefix[] = {
    "killed by ", "choked on ", "poisoned by ", "died of ", "drowned in ",
    "burned by ", "dissolved in ", "crushed to death by ", "petrified by ",
    "turned to slime by ", "killed by ", "", "", "", "", ""
};

static int end_how;


static void writeentry(int fd, const struct toptenentry *tt)
{
    char buf[1024];
    int len;
    
    snprintf(buf, 1024, "%d.%d.%d %d %d %d %d %d %d %d %d %d %d ",
	     tt->ver_major, tt->ver_minor, tt->patchlevel,
	     tt->points, tt->deathdnum, tt->deathlev,
	     tt->maxlvl, tt->hp, tt->maxhp, tt->deaths,
	     tt->deathdate, tt->birthdate, tt->uid);
    len = strlen(buf);
    if (write(fd, buf, len) != len) panic("Failed to write topten. Out of disk?");
    
    snprintf(buf, 1024, "%d %d %s %s %s %s %s,%s\n",
	    tt->moves, tt->how, tt->plrole, tt->plrace, tt->plgend, tt->plalign,
	    onlyspace(tt->name) ? "_" : tt->name, tt->death);
    len = strlen(buf);
    if (write(fd, buf, len) != len) panic("Failed to write topten. Out of disk?");
}


static void write_topten(const struct toptenentry *ttlist)
{
    int i, fd;
    
    fd = open_datafile(RECORD, O_CREAT | O_TRUNC | O_WRONLY, SCOREPREFIX);
    
    for (i = 0; i < TTLISTLEN && validentry(ttlist[i]); i++)
	writeentry(fd, &ttlist[i]);
    
    close(fd);
}


static void update_log(const struct toptenentry *newtt)
{
#ifdef LOGFILE		/* used for debugging (who dies of what, where) */
    int fd = open_datafile(LOGFILE, O_CREAT | O_APPEND | O_WRONLY, SCOREPREFIX);
    if (lock_fd(fd, 10)) {
	writeentry(fd, newtt);
	close(fd);
	unlock_fd(fd);
    }
#endif
}


static boolean readentry(char *line, struct toptenentry *tt)
{
    /* 
     * "3.4.3 77 0 1 1 0 15 1 20110727 20110727 1000 Bar Orc Mal Cha daniel,killed by a newt"
     */
    static const char fmt[] = "%d.%d.%d %d %d %d %d %d %d %d %d"
                              " %d %d %d %d %3s %3s %3s %3s %15[^,],%99[^\n]%*c";
    int rv;
    
    rv = sscanf(line, fmt, &tt->ver_major, &tt->ver_minor, &tt->patchlevel,
		&tt->points, &tt->deathdnum, &tt->deathlev, &tt->maxlvl,
		&tt->hp, &tt->maxhp, &tt->deaths, &tt->deathdate,
		&tt->birthdate, &tt->uid, &tt->moves, &tt->how, tt->plrole,
		tt->plrace, tt->plgend, tt->plalign, tt->name, tt->death);
    
    return rv == 21;
}


static struct toptenentry *read_topten(int limit)
{
    int i, fd, size;
    struct toptenentry *ttlist;
    char *data, *line;
    
    fd = open_datafile(RECORD, O_RDONLY, SCOREPREFIX);
    lseek(fd, 0, SEEK_SET);
    data = loadfile(fd, &size);
    if (!data)
	return NULL;
    
    close(fd);
   
    ttlist = calloc(limit + 1, sizeof(struct toptenentry));
    line = data;
    for (i = 0; i < limit; i++) {
	if (!readentry(line, &ttlist[i]))
	    break;
	line = strchr(line, '\n') + 1;
    }
    
    free(data);
    return ttlist;
}


static void fill_topten_entry(struct toptenentry *newtt, int how)
{
    int uid = getuid();
    memset(newtt, 0, sizeof(struct toptenentry));
    
    /* deepest_lev_reached() is in terms of depth(), and reporting the
     * deepest level reached in the dungeon death occurred in doesn't
     * seem right, so we have to report the death level in depth() terms
     * as well (which also seems reasonable since that's all the player
     * sees on the screen anyway)
     */
    newtt->ver_major = VERSION_MAJOR;
    newtt->ver_minor = VERSION_MINOR;
    newtt->patchlevel = PATCHLEVEL;
    newtt->points = u.urexp;
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
    strncpy(newtt->plgend, genders[flags.female].filecode, ROLESZ);
    newtt->plgend[ROLESZ] = '\0';
    strncpy(newtt->plalign, aligns[1-u.ualign.type].filecode, ROLESZ);
    newtt->plalign[ROLESZ] = '\0';
    strncpy(newtt->name, plname, NAMSZ);
    newtt->name[NAMSZ] = '\0';
    newtt->death[0] = '\0';
    switch (killer_format) {
	default: impossible("bad killer format?");
	case KILLED_BY_AN:
	    strcat(newtt->death, killed_by_prefix[how]);
	    strncat(newtt->death, an(killer),
				    DTHSZ-strlen(newtt->death));
	    break;
	case KILLED_BY:
	    strcat(newtt->death, killed_by_prefix[how]);
	    strncat(newtt->death, killer, DTHSZ-strlen(newtt->death));
	    break;
	case NO_KILLER_PREFIX:
	    strncat(newtt->death, killer, DTHSZ);
	    break;
    }
    newtt->birthdate = yyyymmdd(u.ubirthday);
    newtt->deathdate = yyyymmdd((time_t)0L);
}


static boolean toptenlist_insert(struct toptenentry *ttlist, struct toptenentry *newtt)
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
    
    /* If the player already has PLAYERMAX entries in the list, find the last one.
     * Otherwise del is either the first empty entry or TTLISTLEN if the list is full */
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
	ttlist[i] = ttlist[i-1];
    
    ttlist[ins] = *newtt;
    
    return TRUE;
}

/*
 * Add the result of the current game to the score list
 */
void update_topten(int how)
{
    struct toptenentry *toptenlist, newtt;
    boolean need_rewrite;
    int  fd;
    
    if (program_state.panicking)
	return;
    
    end_how = how; /* save how for nh_get_topten */
    
    fill_topten_entry(&newtt, how);
    update_log(&newtt);
    
    /* nothing more to do for non-scoring games */
    if (wizard || discover)
	return;

    fd = open_datafile(RECORD, O_RDWR | O_CREAT, SCOREPREFIX);
    if (!lock_fd(fd, 60)) {
	close(fd);
	return;
    }
    
    toptenlist = read_topten(TTLISTLEN);
    if (!toptenlist) {
	raw_print("Cannot open record file!\n");
	unlock_fd(fd);
	close(fd);
	return;
    }
    
    /* possibly rearrange the score list to include the new entry */
    need_rewrite = toptenlist_insert(toptenlist, &newtt);
    if (need_rewrite)
	write_topten(toptenlist);
    
    unlock_fd(fd);
    close(fd);
    free(toptenlist);
}


static int classmon(char *plch, boolean fem)
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
struct obj *tt_oname(struct obj *otmp)
{
    int rank;
    struct toptenentry *toptenlist, *tt;

    if (!otmp)
	return NULL;
    
    toptenlist = read_topten(100); /* load the top 100 scores */
    
    /* try to find a valid entry, reducing the value range for rank each time */
    rank = rn2(100);
    while (!validentry(toptenlist[rank]) && rank)
	rank = rn2(rank);
    
    tt = &toptenlist[rank];
    
    if (!validentry(toptenlist[rank]))
	otmp = NULL; /* the topten list is empty */
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
void topten_level_name(int dnum, int dlev, char *outbuf)
{
    if (dnum == astral_level.dnum) {
	const char *arg, *fmt = "on the Plane of %s";

	switch (dlev) {
	case -5:
		fmt = "on the %s Plane";
		arg = "Astral";	break;
	case -4:
		arg = "Water";	break;
	case -3:
		arg = "Fire";	break;
	case -2:
		arg = "Air";	break;
	case -1:
		arg = "Earth";	break;
	default:
		arg = "Void";	break;
	}
	sprintf(eos(outbuf), fmt, arg);
    } else {
	sprintf(eos(outbuf), "in %s", dungeons[dnum].dname);
	if (dnum != knox_level.dnum)
	    sprintf(eos(outbuf), " on level %d", dlev);
    }
}


static void topten_death_description(struct toptenentry *in, char *outbuf)
{
    char *bp;
    boolean second_line = TRUE;

    outbuf[0] = '\0';

    sprintf(eos(outbuf), "%.16s %s-%s-%s-%s ", in->name, in->plrole, in->plrace, in->plgend, in->plalign);

    if (!strncmp("escaped", in->death, 7)) {
	sprintf(eos(outbuf), "escaped the dungeon %s[max level %d]",
		!strncmp(" (", in->death + 7, 2) ? in->death + 7 + 2 : "",
		in->maxlvl);
	/* fixup for closing paren in "escaped... with...Amulet)[max..." */
	if ((bp = strchr(outbuf, ')')) != 0)
	    *bp = (in->deathdnum == astral_level.dnum) ? '\0' : ' ';
	second_line = FALSE;
    } else if (!strncmp("ascended", in->death, 8)) {
	sprintf(eos(outbuf), "ascended to demigod%s-hood",
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
	    sprintf(eos(outbuf), "choked on h%s food",
		    (in->plgend[0] == 'F') ? "er" : "is");
	} else if (!strncmp(in->death, "poisoned", 8)) {
	    strcat(outbuf, "was poisoned");
	} else if (!strncmp(in->death, "crushed", 7)) {
	    strcat(outbuf, "was crushed to death");
	} else if (!strncmp(in->death, "petrified by ", 13)) {
	    strcat(outbuf, "turned to stone");
	} else strcat(outbuf, "died");

	strcat(outbuf, " ");
	topten_level_name(in->deathdnum, in->deathlev, outbuf);
	if (in->deathlev != in->maxlvl)
	    sprintf(eos(outbuf), " [max %d]", in->maxlvl);

	/* kludge for "quit while already on Charon's boat" */
	if (!strncmp(in->death, "quit ", 5))
	    strcat(outbuf, in->death + 4);
    }
    strcat(outbuf, ".");

    /* Quit, starved, ascended, and escaped contain no second line */
    if (second_line)
	sprintf(eos(outbuf), "  %c%s.", highc(*(in->death)), in->death+1);
}


static void fill_nh_score_entry(struct toptenentry *in, struct nh_topten_entry *out,
				int rank, boolean highlight)
{
    
    int rolenum = nh_str2role(in->plrole);
    int racenum = nh_str2race(in->plrace);
    int gendnum = nh_str2gend(in->plgend);
    int alignnum = nh_str2align(in->plalign);
    
    if (rolenum == ROLE_NONE || racenum == ROLE_NONE ||
	gendnum == ROLE_NONE || alignnum == ROLE_NONE)
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


struct nh_topten_entry *nh_get_topten(int *out_len, char *statusbuf,
				      char *player, int top, int around, boolean own)
{
    struct toptenentry *ttlist, newtt;
    struct nh_topten_entry *score_list;
    boolean game_inited = (wiz1_level.dlevel != 0);
    boolean game_complete = game_inited && moves && program_state.gameover;
    int rank = -1; /* index of the completed game in the topten list */
    int i, j, sel_count;
    boolean *selected;
    
    statusbuf[0] = '\0';
    *out_len = 0;
    
    if (!api_entry_checkpoint())
	return NULL;
    
    if (!game_inited) {
	/* If nh_get_topten() isn't called after a game, we never went through
	 * initialization. */
	dlb_init();
	init_dungeons();
    }
    
    if (!player) {
	if (game_complete)
	    player = plname;
	else
	    player = "";
    }
    
    ttlist = read_topten(TTLISTLEN);
    if (!ttlist) {
	strcpy(statusbuf, "Cannot open record file!");
	api_exit();
	return NULL;
    }
    
    /* find the rank of a completed game in the score list */
    if (game_complete && !strcmp(player, plname)) {
	fill_topten_entry(&newtt, end_how);
	
	/* find this entry in the list */
	for (i = 0; i < TTLISTLEN && validentry(ttlist[i]); i++)
	    if (!memcmp(&ttlist[i], &newtt, sizeof(struct toptenentry)))
		rank = i;
	
	if (wizard || discover)
	    sprintf(statusbuf, "Since you were in %s mode, your game was not "
	            "added to the score list.", wizard ? "wizard" : "discover");
	else if (rank >= 0 && rank < 10)
	    sprintf(statusbuf, "You made the top ten list!");
	else if (rank)
	    sprintf(statusbuf, "You reached the %d%s place on the score list.",
		    rank+1, ordin(rank+1));
    }
    
    /* select scores for display */
    sel_count = 0;
    selected = calloc(TTLISTLEN, sizeof(boolean));
    
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
    
    
    score_list = xmalloc(sel_count * sizeof(struct nh_topten_entry));
    memset(score_list, 0, sel_count * sizeof(struct nh_topten_entry));
    *out_len = sel_count;
    j = 0;
    for (i = 0; i < TTLISTLEN && validentry(ttlist[i]); i++) {
	if (selected[i])
	    fill_nh_score_entry(&ttlist[i], &score_list[j++], i+1, i == rank);
    }
    
    if (!game_inited) {
	    free_dungeon();
	    dlb_cleanup();
    }
    
    free(selected);
    free(ttlist);
    
    api_exit();
    return score_list;
}


/* append the topten entry for the completed game to that game's logfile */
void write_log_toptenentry(int fd, int how)
{
    struct toptenentry tt;
    
    fill_topten_entry(&tt, how);
    writeentry(fd, &tt);
}


/* read the toptenentry appended to the end of a game log */
void read_log_toptenentry(int fd, struct nh_topten_entry *entry)
{
    struct toptenentry tt;
    int size;
    char *line = loadfile(fd, &size);
    if (!line)
	return;
    
    readentry(line, &tt);
    fill_nh_score_entry(&tt, entry, -1, FALSE);
    free(line);
}

/* topten.c */
