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
	update_status();
	botl = botlx = 0;
}


void nh_get_player_info(struct nh_player_info *pi)
{
	int cap, advskills, i;
	
	memset(pi, 0, sizeof(struct nh_player_info));
	
	pi->moves = moves;
	strncpy(pi->plname, plname, sizeof(pi->plname));
	
	/* This function could be called before the game is fully inited.
	 * Test youmonst.data as it is required for near_capacity().
	 * program_state.game_started is no good, as we need this data before 
	 * game_started is set */
	if (!youmonst.data)
	    return;
	
	pi->x = u.ux;
	pi->y = u.uy;
	pi->z = u.uz.dlevel;
	
	if (Upolyd) {
		char mbot[BUFSZ];
		int k = 0;

		strcpy(mbot, mons[u.umonnum].mname);
		while (mbot[k] != 0) {
		    if ((k == 0 || (k > 0 && mbot[k-1] == ' ')) &&
					'a' <= mbot[k] && mbot[k] <= 'z')
			mbot[k] += 'A' - 'a';
		    k++;
		}
		strncpy(pi->rank, mbot, sizeof(pi->rank));
	} else
		strncpy(pi->rank, rank(), sizeof(pi->rank));
	
	pi->max_rank_sz = mrank_sz;
	
	/* abilities */
	pi->st = ACURR(A_STR);
	pi->st_extra = 0;
	if (pi->st > 118) {
		pi->st = pi->st - 100;
		pi->st_extra = 0;
	} else if (pi->st > 18) {
		pi->st_extra = pi->st - 18;
		pi->st = 18;
	}
	
	pi->dx = ACURR(A_DEX);
	pi->co = ACURR(A_CON);
	pi->in = ACURR(A_INT);
	pi->wi = ACURR(A_WIS);
	pi->ch = ACURR(A_CHA);
	
	pi->score = botl_score();
	
	/* hp and energy */
	pi->hp = Upolyd ? u.mh : u.uhp;
	pi->hpmax = Upolyd ? u.mhmax : u.uhpmax;
	if (pi->hp < 0)
	    pi->hp = 0;
	
	pi->en = u.uen;
	pi->enmax = u.uenmax;
	pi->ac = u.uac;
	
#ifndef GOLDOBJ
	pi->gold = u.ugold;
#else
	pi->gold = money_cnt(invent);
#endif
	pi->coinsym = def_oc_syms[COIN_CLASS];
	describe_level(pi->level_desc);
	
	pi->monnum = u.umonster;
	pi->cur_monnum = u.umonnum;
	
	/* level and exp points */
	if (Upolyd)
	    pi->level = mons[u.umonnum].mlevel;
	else
	    pi->level = u.ulevel;
	pi->xp = u.uexp;

	cap = near_capacity();
	
	/* check if any skills could be anhanced */
	advskills = 0;
	for (i = 0; i < P_NUM_SKILLS; i++) {
	    if (P_RESTRICTED(i))
		continue;
	    if (can_advance(i, FALSE))
		advskills++;
	}
	pi->enhance_possible = advskills > 0;
	
	/* add status items for various problems */
	if (strcmp(hu_stat[u.uhs], "        "))
	    strncpy(pi->statusitems[pi->nr_items++], hu_stat[u.uhs], ITEMLEN);
	
	if (Confusion)
	    strncpy(pi->statusitems[pi->nr_items++], "Conf", ITEMLEN);
	
	if (Sick) {
	    if (u.usick_type & SICK_VOMITABLE)
		strncpy(pi->statusitems[pi->nr_items++], "FoodPois", ITEMLEN);
	    if (u.usick_type & SICK_NONVOMITABLE)
		strncpy(pi->statusitems[pi->nr_items++], "Ill", ITEMLEN);
	}
	if (Blind)
	    strncpy(pi->statusitems[pi->nr_items++], "Blind", ITEMLEN);
	if (Stunned)
	    strncpy(pi->statusitems[pi->nr_items++], "Stun", ITEMLEN);
	if (Hallucination)
	    strncpy(pi->statusitems[pi->nr_items++], "Hallu", ITEMLEN);
	if (Slimed)
	    strncpy(pi->statusitems[pi->nr_items++], "Slime", ITEMLEN);
	if (cap > UNENCUMBERED)
	    strncpy(pi->statusitems[pi->nr_items++], enc_stat[cap], ITEMLEN);
}

/*botl.c*/
