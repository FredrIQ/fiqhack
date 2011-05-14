/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

extern const char *hu_stat[];	/* defined in eat.c */

const char * const enc_stat[] = {
	"",
	"Burdened",
	"Stressed",
	"Strained",
	"Overtaxed",
	"Overloaded"
};



static int mrank_sz = 0; /* loaded by max_rank_sz (from u_init) */
static const char *rank(void);


/* convert experience level (1..30) to rank index (0..8) */
int xlev_to_rank(int xlev)
{
	return (xlev <= 2) ? 0 : (xlev <= 30) ? ((xlev + 2) / 4) : 8;
}

const char *rank_of(int lev, short monnum, boolean female)
{
	struct Role *role;
	int i;


	/* Find the role */
	for (role = (struct Role *) roles; role->name.m; role++)
	    if (monnum == role->malenum || monnum == role->femalenum)
	    	break;
	if (!role->name.m)
	    role = &urole;

	/* Find the rank */
	for (i = xlev_to_rank((int)lev); i >= 0; i--) {
	    if (female && role->rank[i].f) return role->rank[i].f;
	    if (role->rank[i].m) return role->rank[i].m;
	}

	/* Try the role name, instead */
	if (female && role->name.f) return role->name.f;
	else if (role->name.m) return role->name.m;
	return "Player";
}


static const char *rank(void)
{
	return rank_of(u.ulevel, Role_switch, flags.female);
}

int title_to_mon(const char *str, int *rank_indx, int *title_length)
{
	int i, j;


	/* Loop through each of the roles */
	for (i = 0; roles[i].name.m; i++)
	    for (j = 0; j < 9; j++) {
	    	if (roles[i].rank[j].m && !strncmpi(str,
	    			roles[i].rank[j].m, strlen(roles[i].rank[j].m))) {
	    	    if (rank_indx) *rank_indx = j;
	    	    if (title_length) *title_length = strlen(roles[i].rank[j].m);
	    	    return roles[i].malenum;
	    	}
	    	if (roles[i].rank[j].f && !strncmpi(str,
	    			roles[i].rank[j].f, strlen(roles[i].rank[j].f))) {
	    	    if (rank_indx) *rank_indx = j;
	    	    if (title_length) *title_length = strlen(roles[i].rank[j].f);
	    	    return ((roles[i].femalenum != NON_PM) ?
	    	    		roles[i].femalenum : roles[i].malenum);
	    	}
	    }
	return NON_PM;
}


void max_rank_sz(void)
{
	int i, r, maxr = 0;
	for (i = 0; i < 9; i++) {
	    if (urole.rank[i].m && (r = strlen(urole.rank[i].m)) > maxr) maxr = r;
	    if (urole.rank[i].f && (r = strlen(urole.rank[i].f)) > maxr) maxr = r;
	}
	mrank_sz = maxr;
	return;
}


long botl_score(void)
{
    int deepest = deepest_lev_reached(FALSE);
#ifndef GOLDOBJ
    long ugold = u.ugold + hidden_gold();

    if ((ugold -= u.ugold0) < 0L) ugold = 0L;
    return ugold + u.urexp + (long)(50 * (deepest - 1))
#else
    long umoney = money_cnt(invent) + hidden_gold();

    if ((umoney -= u.umoney0) < 0L) umoney = 0L;
    return umoney + u.urexp + (long)(50 * (deepest - 1))
#endif
			  + (long)(deepest > 30 ? 10000 :
				   deepest > 20 ? 1000*(deepest - 20) : 0);
}


/* provide the name of the current level for display by various ports */
int describe_level(char *buf)
{
	int ret = 1;

	/* TODO:	Add in dungeon name */
	if (Is_knox(&u.uz))
		sprintf(buf, "%s ", dungeons[u.uz.dnum].dname);
	else if (In_quest(&u.uz))
		sprintf(buf, "Home %d ", dunlev(&u.uz));
	else if (In_endgame(&u.uz))
		sprintf(buf,
			Is_astralevel(&u.uz) ? "Astral Plane " : "End Game ");
	else {
		/* ports with more room may expand this one */
		sprintf(buf, "Dlvl:%-2d ", depth(&u.uz));
		ret = 0;
	}
	return ret;
}


void bot(void)
{
	struct nh_status_info status;
	int cap = near_capacity();

	memset(&status, 0, sizeof(status));
	
	strncpy(status.plname, plname, sizeof(status.plname));
	
	if (Upolyd) {
		char mbot[BUFSZ];
		int k = 0;

		strcpy(mbot, mons[u.umonnum].mname);
		while(mbot[k] != 0) {
		    if ((k == 0 || (k > 0 && mbot[k-1] == ' ')) &&
					'a' <= mbot[k] && mbot[k] <= 'z')
			mbot[k] += 'A' - 'a';
		    k++;
		}
		strncpy(status.rank, mbot, sizeof(status.rank));
	} else
		strncpy(status.rank, rank(), sizeof(status.rank));
	
	status.mrank_sz = mrank_sz;
	
	status.st = ACURR(A_STR);
	status.st_extra = 0;
	if (ACURR(A_STR) > 18) {
		status.st = ACURR(A_STR) - 100;
		status.st_extra = ACURR(A_STR) - 18;
	}
	
	status.dx = ACURR(A_DEX);
	status.co = ACURR(A_CON);
	status.in = ACURR(A_INT);
	status.wi = ACURR(A_WIS);
	status.ch = ACURR(A_CHA);
	
	status.score = botl_score();
	
	status.hp = Upolyd ? u.mh : u.uhp;
	status.hpmax = Upolyd ? u.mhmax : u.uhpmax;
	if (status.hp < 0)
	    status.hp = 0;
	
	status.en = u.uen;
	status.enmax = u.uenmax;
	status.ac = u.uac;
	
#ifndef GOLDOBJ
	status.gold = u.ugold;
#else
	status.gold = money_cnt(invent);
#endif
	status.coinsym = oc_syms[COIN_CLASS];
	describe_level(status.level_desc);
	
	status.polyd = Upolyd;
	if (Upolyd)
	    status.level = mons[u.umonnum].mlevel;
	else
	    status.level = u.ulevel;
	status.xp = u.uexp;

	
	if(strcmp(hu_stat[u.uhs], "        "))
	    strncpy(status.items[status.nr_items++], hu_stat[u.uhs], ITEMLEN);
	
	if(Confusion)
	    strncpy(status.items[status.nr_items++], "Conf", ITEMLEN);
	
	if(Sick) {
	    if (u.usick_type & SICK_VOMITABLE)
		strncpy(status.items[status.nr_items++], "FoodPois", ITEMLEN);
	    if (u.usick_type & SICK_NONVOMITABLE)
		strncpy(status.items[status.nr_items++], "Ill", ITEMLEN);
	}
	if(Blind)
	    strncpy(status.items[status.nr_items++], "Blind", ITEMLEN);
	if(Stunned)
	    strncpy(status.items[status.nr_items++], "Stun", ITEMLEN);
	if(Hallucination)
	    strncpy(status.items[status.nr_items++], "Hallu", ITEMLEN);
	if(Slimed)
	    strncpy(status.items[status.nr_items++], "Slime", ITEMLEN);
	if(cap > UNENCUMBERED)
	    strncpy(status.items[status.nr_items++], enc_stat[cap], ITEMLEN);
	
	update_status(&status);
	botl = botlx = 0;
}

/*botl.c*/
