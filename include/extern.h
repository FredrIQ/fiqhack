/*	SCCS Id: @(#)extern.h	3.4	2003/03/10	*/
/* Copyright (c) Steve Creps, 1988.				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef EXTERN_H
#define EXTERN_H

/* ### alloc.c ### */

extern char *fmt_ptr(const void *,char *);

/* This next pre-processor directive covers almost the entire file,
 * interrupted only occasionally to pick up specific functions as needed. */
#if !defined(MAKEDEFS_C) && !defined(LEV_LEX_C)

/* ### allmain.c ### */

extern void moveloop(void);
extern void stop_occupation(void);
extern void display_gamewindows(void);
extern void newgame(void);
extern void welcome(boolean);

/* ### apply.c ### */

extern int doapply(void);
extern int dorub(void);
extern int dojump(void);
extern int jump(int);
extern int number_leashed(void);
extern void o_unleash(struct obj *);
extern void m_unleash(struct monst *, boolean);
extern void unleash_all(void);
extern boolean next_to_u(void);
extern struct obj *get_mleash(struct monst *);
extern void check_leash(xchar, xchar);
extern boolean um_dist(xchar, xchar, xchar);
extern boolean snuff_candle(struct obj *);
extern boolean snuff_lit(struct obj *);
extern boolean catch_lit(struct obj *);
extern void use_unicorn_horn(struct obj *);
extern boolean tinnable(struct obj *);
extern void reset_trapset(void);
extern void fig_transform(void *, long);
extern int unfixable_trouble_count(boolean);

/* ### artifact.c ### */

extern void init_artifacts(void);
extern void save_artifacts(int);
extern void restore_artifacts(int);
extern const char *artiname(int);
extern struct obj *mk_artifact(struct obj *, aligntyp);
extern const char *artifact_name(const char *,short *);
extern boolean exist_artifact(int,const char *);
extern void artifact_exists(struct obj *,const char *, boolean);
extern int nartifact_exist(void);
extern boolean spec_ability(struct obj *,unsigned long);
extern boolean confers_luck(struct obj *);
extern boolean arti_reflects(struct obj *);
extern boolean restrict_name(struct obj *,const char *);
extern boolean defends(int,struct obj *);
extern boolean protects(int,struct obj *);
extern void set_artifact_intrinsic(struct obj *, boolean, long);
extern int touch_artifact(struct obj *,struct monst *);
extern int spec_abon(struct obj *,struct monst *);
extern int spec_dbon(struct obj *,struct monst *,int);
extern void discover_artifact(xchar);
extern boolean undiscovered_artifact(xchar);
extern int disp_artifact_discoveries(winid);
extern boolean artifact_hit(struct monst *,struct monst *,struct obj *,int *,int);
extern int doinvoke(void);
extern void arti_speak(struct obj *);
extern boolean artifact_light(struct obj *);
extern long spec_m2(struct obj *);
extern boolean artifact_has_invprop(struct obj *, uchar);
extern long arti_cost(struct obj *);

/* ### attrib.c ### */

extern boolean adjattrib(int,int,int);
extern void change_luck(schar);
extern int stone_luck(boolean);
extern void set_moreluck(void);
extern void gainstr(struct obj *,int);
extern void losestr(int);
extern void restore_attrib(void);
extern void exercise(int,boolean);
extern void exerchk(void);
extern void reset_attribute_clock(void);
extern void init_attr(int);
extern void redist_attr(void);
extern void adjabil(int,int);
extern int newhp(void);
extern schar acurr(int);
extern schar acurrstr(void);
extern void adjalign(int);

/* ### ball.c ### */

extern void ballfall(void);
extern void placebc(void);
extern void unplacebc(void);
extern void set_bc(int);
extern void move_bc(int,int,xchar,xchar,xchar,xchar);
extern boolean drag_ball(xchar,xchar,
		int *,xchar *,xchar *,xchar *,xchar *, boolean *,boolean);
extern void drop_ball(xchar,xchar);
extern void drag_down(void);

/* ### bones.c ### */

extern boolean can_make_bones(void);
extern void savebones(struct obj *);
extern int getbones(void);

/* ### botl.c ### */

extern int xlev_to_rank(int);
extern int title_to_mon(const char *,int *,int *);
extern void max_rank_sz(void);
#ifdef SCORE_ON_BOTL
extern long botl_score(void);
#endif
extern int describe_level(char *);
extern const char *rank_of(int, short, boolean);
extern void bot(void);

/* ### cmd.c ### */

extern void reset_occupations(void);
extern void set_occupation(int (*)(void),const char *,int);
#ifdef REDO
extern char pgetchar(void);
extern void pushch(CHAR_P);
extern void savech(CHAR_P);
#endif
extern void add_debug_extended_commands(void);
extern void rhack(char *);
extern int doextlist(void);
extern int extcmd_via_menu(void);
extern void enlightenment(int);
extern void show_conduct(int);
extern int xytod(SCHAR_P,SCHAR_P);
extern void dtoxy(coord *,int);
extern int movecmd(CHAR_P);
extern int getdir(const char *);
extern void confdir(void);
extern int isok(int,int);
extern int get_adjacent_loc(const char *, const char *, XCHAR_P, XCHAR_P, coord *);
extern const char *click_to_cmd(int,int,int);
extern char readchar(void);
extern void sanity_check(void);
extern char yn_function(const char *, const char *, CHAR_P);

/* ### dbridge.c ### */

extern boolean is_pool(int,int);
extern boolean is_lava(int,int);
extern boolean is_ice(int,int);
extern int is_drawbridge_wall(int,int);
extern boolean is_db_wall(int,int);
extern boolean find_drawbridge(int *,int*);
extern boolean create_drawbridge(int,int,int,BOOLEAN_P);
extern void open_drawbridge(int,int);
extern void close_drawbridge(int,int);
extern void destroy_drawbridge(int,int);

/* ### decl.c ### */

extern void decl_init(void);

/* ### detect.c ### */

extern struct obj *o_in(struct obj*,CHAR_P);
extern struct obj *o_material(struct obj*,unsigned);
extern int gold_detect(struct obj *);
extern int food_detect(struct obj *);
extern int object_detect(struct obj *,int);
extern int monster_detect(struct obj *,int);
extern int trap_detect(struct obj *);
extern const char *level_distance(d_level *);
extern void use_crystal_ball(struct obj *);
extern void do_mapping(void);
extern void do_vicinity_map(void);
extern void cvt_sdoor_to_door(struct rm *);
extern int findit(void);
extern int openit(void);
extern void find_trap(struct trap *);
extern int dosearch0(int);
extern int dosearch(void);
extern void sokoban_detect(void);

/* ### dig.c ### */

extern boolean is_digging(void);
extern int holetime(void);
extern boolean dig_check(struct monst *, BOOLEAN_P, int, int);
extern void digactualhole(int,int,struct monst *,int);
extern boolean dighole(BOOLEAN_P);
extern int use_pick_axe(struct obj *);
extern int use_pick_axe2(struct obj *);
extern boolean mdig_tunnel(struct monst *);
extern void watch_dig(struct monst *,XCHAR_P,XCHAR_P,BOOLEAN_P);
extern void zap_dig(void);
extern struct obj *bury_an_obj(struct obj *);
extern void bury_objs(int,int);
extern void unearth_objs(int,int);
extern void rot_organic(void *, long);
extern void rot_corpse(void *, long);

/* ### display.c ### */

#ifdef INVISIBLE_OBJECTS
extern struct obj * vobj_at(XCHAR_P,XCHAR_P);
#endif /* INVISIBLE_OBJECTS */
extern void magic_map_background(XCHAR_P,XCHAR_P,int);
extern void map_background(XCHAR_P,XCHAR_P,int);
extern void map_trap(struct trap *,int);
extern void map_object(struct obj *,int);
extern void map_invisible(XCHAR_P,XCHAR_P);
extern void unmap_object(int,int);
extern void map_location(int,int,int);
extern void feel_location(XCHAR_P,XCHAR_P);
extern void newsym(int,int);
extern void shieldeff(XCHAR_P,XCHAR_P);
extern void tmp_at(int,int);
extern void swallowed(int);
extern void under_ground(int);
extern void under_water(int);
extern void see_monsters(void);
extern void set_mimic_blocking(void);
extern void see_objects(void);
extern void see_traps(void);
extern void curs_on_u(void);
extern int doredraw(void);
extern void docrt(void);
extern void show_glyph(int,int,int);
extern void clear_glyph_buffer(void);
extern void row_refresh(int,int,int);
extern void cls(void);
extern void flush_screen(int);
extern int back_to_glyph(XCHAR_P,XCHAR_P);
extern int zapdir_to_glyph(int,int,int);
extern int glyph_at(XCHAR_P,XCHAR_P);
extern void set_wall_state(void);

/* ### do.c ### */

extern int dodrop(void);
extern boolean boulder_hits_pool(struct obj *,int,int,BOOLEAN_P);
extern boolean flooreffects(struct obj *,int,int,const char *);
extern void doaltarobj(struct obj *);
extern boolean canletgo(struct obj *,const char *);
extern void dropx(struct obj *);
extern void dropy(struct obj *);
extern void obj_no_longer_held(struct obj *);
extern int doddrop(void);
extern int dodown(void);
extern int doup(void);
#ifdef INSURANCE
extern void save_currentstate(void);
#endif
extern void goto_level(d_level *,BOOLEAN_P,BOOLEAN_P,BOOLEAN_P);
extern void schedule_goto(d_level *,BOOLEAN_P,BOOLEAN_P,int,
			     const char *,const char *);
extern void deferred_goto(void);
extern boolean revive_corpse(struct obj *);
extern void revive_mon(void *, long);
extern int donull(void);
extern int dowipe(void);
extern void set_wounded_legs(long,int);
extern void heal_legs(void);

/* ### do_name.c ### */

extern int getpos(coord *,BOOLEAN_P,const char *);
extern struct monst *christen_monst(struct monst *,const char *);
extern int do_mname(void);
extern struct obj *oname(struct obj *,const char *);
extern int ddocall(void);
extern void docall(struct obj *);
extern const char *rndghostname(void);
extern char *x_monnam(struct monst *,int,const char *,int,BOOLEAN_P);
extern char *l_monnam(struct monst *);
extern char *mon_nam(struct monst *);
extern char *noit_mon_nam(struct monst *);
extern char *Monnam(struct monst *);
extern char *noit_Monnam(struct monst *);
extern char *m_monnam(struct monst *);
extern char *y_monnam(struct monst *);
extern char *Adjmonnam(struct monst *,const char *);
extern char *Amonnam(struct monst *);
extern char *a_monnam(struct monst *);
extern char *distant_monnam(struct monst *,int,char *);
extern const char *rndmonnam(void);
extern const char *hcolor(const char *);
extern const char *rndcolor(void);
#ifdef REINCARNATION
extern const char *roguename(void);
#endif
extern struct obj *realloc_obj(struct obj *, int, void *, int, const char *);
extern char *coyotename(struct monst *,char *);

/* ### do_wear.c ### */

extern void off_msg(struct obj *);
extern void set_wear(void);
extern boolean donning(struct obj *);
extern void cancel_don(void);
extern int Armor_off(void);
extern int Armor_gone(void);
extern int Helmet_off(void);
extern int Gloves_off(void);
extern int Boots_off(void);
extern int Cloak_off(void);
extern int Shield_off(void);
#ifdef TOURIST
extern int Shirt_off(void);
#endif
extern void Amulet_off(void);
extern void Ring_on(struct obj *);
extern void Ring_off(struct obj *);
extern void Ring_gone(struct obj *);
extern void Blindf_on(struct obj *);
extern void Blindf_off(struct obj *);
extern int dotakeoff(void);
extern int doremring(void);
extern int cursed(struct obj *);
extern int armoroff(struct obj *);
extern int canwearobj(struct obj *, long *, BOOLEAN_P);
extern int dowear(void);
extern int doputon(void);
extern void find_ac(void);
extern void glibr(void);
extern struct obj *some_armor(struct monst *);
extern void erode_armor(struct monst *,BOOLEAN_P);
extern struct obj *stuck_ring(struct obj *,int);
extern struct obj *unchanger(void);
extern void reset_remarm(void);
extern int doddoremarm(void);
extern int destroy_arm(struct obj *);
extern void adj_abon(struct obj *,SCHAR_P);

/* ### dog.c ### */

extern void initedog(struct monst *);
extern struct monst *make_familiar(struct obj *,XCHAR_P,XCHAR_P,BOOLEAN_P);
extern struct monst *makedog(void);
extern void update_mlstmv(void);
extern void losedogs(void);
extern void mon_arrive(struct monst *,BOOLEAN_P);
extern void mon_catchup_elapsed_time(struct monst *,long);
extern void keepdogs(BOOLEAN_P);
extern void migrate_to_level(struct monst *,XCHAR_P,XCHAR_P,coord *);
extern int dogfood(struct monst *,struct obj *);
extern struct monst *tamedog(struct monst *,struct obj *);
extern void abuse_dog(struct monst *);
extern void wary_dog(struct monst *, BOOLEAN_P);

/* ### dogmove.c ### */

extern int dog_nutrition(struct monst *,struct obj *);
extern int dog_eat(struct monst *,struct obj *,int,int,BOOLEAN_P);
extern int dog_move(struct monst *,int);

/* ### dokick.c ### */

extern boolean ghitm(struct monst *,struct obj *);
extern void container_impact_dmg(struct obj *);
extern int dokick(void);
extern boolean ship_object(struct obj *,XCHAR_P,XCHAR_P,BOOLEAN_P);
extern void obj_delivery(void);
extern schar down_gate(XCHAR_P,XCHAR_P);
extern void impact_drop(struct obj *,XCHAR_P,XCHAR_P,XCHAR_P);

/* ### dothrow.c ### */

extern int dothrow(void);
extern int dofire(void);
extern void hitfloor(struct obj *);
extern void hurtle(int,int,int,BOOLEAN_P);
extern void mhurtle(struct monst *,int,int,int);
extern void throwit(struct obj *,long,BOOLEAN_P);
extern int omon_adj(struct monst *,struct obj *,BOOLEAN_P);
extern int thitmonst(struct monst *,struct obj *);
extern int hero_breaks(struct obj *,XCHAR_P,XCHAR_P,BOOLEAN_P);
extern int breaks(struct obj *,XCHAR_P,XCHAR_P);
extern boolean breaktest(struct obj *);
extern boolean walk_path(coord *, coord *, boolean (*)(void *,int,int), void *);
extern boolean hurtle_step(void *, int, int);

/* ### drawing.c ### */
#endif /* !MAKEDEFS_C && !LEV_LEX_C */
extern int def_char_to_objclass(CHAR_P);
extern int def_char_to_monclass(CHAR_P);
#if !defined(MAKEDEFS_C) && !defined(LEV_LEX_C)
extern void assign_graphics(uchar *,int,int,int);
extern void switch_graphics(int);
#ifdef REINCARNATION
extern void assign_rogue_graphics(BOOLEAN_P);
#endif

/* ### dungeon.c ### */

extern void save_dungeon(int,BOOLEAN_P,BOOLEAN_P);
extern void restore_dungeon(int);
extern void insert_branch(branch *,BOOLEAN_P);
extern void init_dungeons(void);
extern s_level *find_level(const char *);
extern s_level *Is_special(d_level *);
extern branch *Is_branchlev(d_level *);
extern xchar ledger_no(d_level *);
extern xchar maxledgerno(void);
extern schar depth(d_level *);
extern xchar dunlev(d_level *);
extern xchar dunlevs_in_dungeon(d_level *);
extern xchar ledger_to_dnum(XCHAR_P);
extern xchar ledger_to_dlev(XCHAR_P);
extern xchar deepest_lev_reached(BOOLEAN_P);
extern boolean on_level(d_level *,d_level *);
extern void next_level(BOOLEAN_P);
extern void prev_level(BOOLEAN_P);
extern void u_on_newpos(int,int);
extern void u_on_sstairs(void);
extern void u_on_upstairs(void);
extern void u_on_dnstairs(void);
extern boolean On_stairs(XCHAR_P,XCHAR_P);
extern void get_level(d_level *,int);
extern boolean Is_botlevel(d_level *);
extern boolean Can_fall_thru(d_level *);
extern boolean Can_dig_down(d_level *);
extern boolean Can_rise_up(int,int,d_level *);
extern boolean In_quest(d_level *);
extern boolean In_mines(d_level *);
extern branch *dungeon_branch(const char *);
extern boolean at_dgn_entrance(const char *);
extern boolean In_hell(d_level *);
extern boolean In_V_tower(d_level *);
extern boolean On_W_tower_level(d_level *);
extern boolean In_W_tower(int,int,d_level *);
extern void find_hell(d_level *);
extern void goto_hell(BOOLEAN_P,BOOLEAN_P);
extern void assign_level(d_level *,d_level *);
extern void assign_rnd_level(d_level *,d_level *,int);
extern int induced_align(int);
extern boolean Invocation_lev(d_level *);
extern xchar level_difficulty(void);
extern schar lev_by_name(const char *);
extern schar print_dungeon(BOOLEAN_P,schar *,xchar *);

/* ### eat.c ### */

extern boolean is_edible(struct obj *);
extern void init_uhunger(void);
extern int Hear_again(void);
extern void reset_eat(void);
extern int doeat(void);
extern void gethungry(void);
extern void morehungry(int);
extern void lesshungry(int);
extern boolean is_fainted(void);
extern void reset_faint(void);
extern void violated_vegetarian(void);
extern void newuhs(BOOLEAN_P);
extern struct obj *floorfood(const char *,int);
extern void vomit(void);
extern int eaten_stat(int,struct obj *);
extern void food_disappears(struct obj *);
extern void food_substitution(struct obj *,struct obj *);
extern void fix_petrification(void);
extern void consume_oeaten(struct obj *,int);
extern boolean maybe_finished_meal(BOOLEAN_P);

/* ### end.c ### */

extern void done1(int);
extern int done2(void);
extern void done_in_by(struct monst *);
#endif /* !MAKEDEFS_C && !LEV_LEX_C */
extern void panic(const char *,...);
#if !defined(MAKEDEFS_C) && !defined(LEV_LEX_C)
extern void done(int);
extern void container_contents(struct obj *,BOOLEAN_P,BOOLEAN_P);
extern void terminate(int);
extern int num_genocides(void);

/* ### engrave.c ### */

extern char *random_engraving(char *);
extern void wipeout_text(char *,int,unsigned);
extern boolean can_reach_floor(void);
extern const char *surface(int,int);
extern const char *ceiling(int,int);
extern struct engr *engr_at(XCHAR_P,XCHAR_P);
#ifdef ELBERETH
extern int sengr_at(const char *,XCHAR_P,XCHAR_P);
#endif
extern void u_wipe_engr(int);
extern void wipe_engr_at(XCHAR_P,XCHAR_P,XCHAR_P);
extern void read_engr_at(int,int);
extern void make_engr_at(int,int,const char *,long,XCHAR_P);
extern void del_engr_at(int,int);
extern int freehand(void);
extern int doengrave(void);
extern void save_engravings(int,int);
extern void rest_engravings(int);
extern void del_engr(struct engr *);
extern void rloc_engr(struct engr *);
extern void make_grave(int,int,const char *);

/* ### exper.c ### */

extern int experience(struct monst *,int);
extern void more_experienced(int,int);
extern void losexp(const char *);
extern void newexplevel(void);
extern void pluslvl(BOOLEAN_P);
extern long rndexp(BOOLEAN_P);

/* ### explode.c ### */

extern void explode(int,int,int,int,CHAR_P,int);
extern long scatter(int, int, int, unsigned int, struct obj *);
extern void splatter_burning_oil(int, int);

/* ### extralev.c ### */

#ifdef REINCARNATION
extern void makeroguerooms(void);
extern void corr(int,int);
extern void makerogueghost(void);
#endif

/* ### files.c ### */

extern char *fname_encode(const char *, CHAR_P, char *, char *, int);
extern char *fname_decode(CHAR_P, char *, char *, int);
extern const char *fqname(const char *, int, int);
extern FILE *fopen_datafile(const char *,const char *,int);
extern boolean uptodate(int,const char *);
extern void store_version(int);
extern void set_levelfile_name(char *,int);
extern int create_levelfile(int,char *);
extern int open_levelfile(int,char *);
extern void delete_levelfile(int);
extern void clearlocks(void);
extern int create_bonesfile(d_level*,char **, char *);
extern void commit_bonesfile(d_level *);
extern int open_bonesfile(d_level*,char **);
extern int delete_bonesfile(d_level*);
extern void set_savefile_name(void);
#ifdef INSURANCE
extern void save_savefile_name(int);
#endif
extern void set_error_savefile(void);
extern int create_savefile(void);
extern int open_savefile(void);
extern int delete_savefile(void);
extern int restore_saved_game(void);
extern boolean lock_file(const char *,int,int);
extern void unlock_file(const char *);
extern void read_config_file(const char *);
extern void check_recordfile(const char *);
extern void read_wizkit(void);
extern void paniclog(const char *, const char *);
extern int validate_prefix_locations(char *);
extern void free_saved_games(char**);
#ifdef SELF_RECOVER
extern boolean recover_savefile(void);
#endif
#ifdef HOLD_LOCKFILE_OPEN
extern void really_close(void);
#endif

/* ### fountain.c ### */

extern void floating_above(const char *);
extern void dogushforth(int);
extern void dryup(XCHAR_P,XCHAR_P, BOOLEAN_P);
extern void drinkfountain(void);
extern void dipfountain(struct obj *);
extern void breaksink(int,int);
extern void drinksink(void);

/* ### hack.c ### */

extern boolean revive_nasty(int,int,const char*);
extern void movobj(struct obj *,XCHAR_P,XCHAR_P);
extern boolean may_dig(XCHAR_P,XCHAR_P);
extern boolean may_passwall(XCHAR_P,XCHAR_P);
extern boolean bad_rock(struct permonst *,XCHAR_P,XCHAR_P);
extern boolean invocation_pos(XCHAR_P,XCHAR_P);
extern boolean test_move(int, int, int, int, int);
extern void domove(void);
extern void invocation_message(void);
extern void spoteffects(BOOLEAN_P);
extern char *in_rooms(XCHAR_P,XCHAR_P,int);
extern boolean in_town(int,int);
extern void check_special_room(BOOLEAN_P);
extern int dopickup(void);
extern void lookaround(void);
extern int monster_nearby(void);
extern void nomul(int);
extern void unmul(const char *);
extern void losehp(int,const char *,BOOLEAN_P);
extern int weight_cap(void);
extern int inv_weight(void);
extern int near_capacity(void);
extern int calc_capacity(int);
extern int max_capacity(void);
extern boolean check_capacity(const char *);
extern int inv_cnt(void);
#ifdef GOLDOBJ
extern long money_cnt(struct obj *);
#endif

/* ### hacklib.c ### */

extern boolean digit(CHAR_P);
extern boolean letter(CHAR_P);
extern char highc(CHAR_P);
extern char lowc(CHAR_P);
extern char *lcase(char *);
extern char *upstart(char *);
extern char *mungspaces(char *);
extern char *eos(char *);
extern char *strkitten(char *,CHAR_P);
extern char *s_suffix(const char *);
extern char *xcrypt(const char *,char *);
extern boolean onlyspace(const char *);
extern char *tabexpand(char *);
extern char *visctrl(CHAR_P);
extern const char *ordin(int);
extern char *sitoa(int);
extern int sgn(int);
extern int rounddiv(long,int);
extern int dist2(int,int,int,int);
extern int distmin(int,int,int,int);
extern boolean online2(int,int,int,int);
extern boolean pmatch(const char *,const char *);
#ifndef STRNCMPI
extern int strncmpi(const char *,const char *,int);
#endif
#ifndef STRSTRI
extern char *strstri(const char *,const char *);
#endif
extern boolean fuzzymatch(const char *,const char *,const char *,BOOLEAN_P);
extern void setrandom(void);
extern int getyear(void);
extern long yyyymmdd(time_t);
extern int phase_of_the_moon(void);
extern boolean friday_13th(void);
extern int night(void);
extern int midnight(void);

/* ### invent.c ### */

extern void assigninvlet(struct obj *);
extern struct obj *merge_choice(struct obj *,struct obj *);
extern int merged(struct obj **,struct obj **);
extern void addinv_core1(struct obj *);
extern void addinv_core2(struct obj *);
extern struct obj *addinv(struct obj *);
extern struct obj *hold_another_object
		(struct obj *,const char *,const char *,const char *);
extern void useupall(struct obj *);
extern void useup(struct obj *);
extern void consume_obj_charge(struct obj *,BOOLEAN_P);
extern void freeinv_core(struct obj *);
extern void freeinv(struct obj *);
extern void delallobj(int,int);
extern void delobj(struct obj *);
extern struct obj *sobj_at(int,int,int);
extern struct obj *carrying(int);
extern boolean have_lizard(void);
extern struct obj *o_on(unsigned int,struct obj *);
extern boolean obj_here(struct obj *,int,int);
extern boolean wearing_armor(void);
extern boolean is_worn(struct obj *);
extern struct obj *g_at(int,int);
extern struct obj *mkgoldobj(long);
extern struct obj *getobj(const char *,const char *);
extern int ggetobj(const char *,int (*)(struct obj*),int,BOOLEAN_P,unsigned *);
extern void fully_identify_obj(struct obj *);
extern int identify(struct obj *);
extern void identify_pack(int);
extern int askchain(struct obj **,const char *,int,int (*)(struct obj*),
			int (*)(struct obj*),int,const char *);
extern void prinv(const char *,struct obj *,long);
extern char *xprname(struct obj *,const char *,CHAR_P,BOOLEAN_P,long,long);
extern int ddoinv(void);
extern char display_inventory(const char *,BOOLEAN_P);
extern int display_binventory(int,int,BOOLEAN_P);
extern struct obj *display_cinventory(struct obj *);
extern struct obj *display_minventory(struct monst *,int,char *);
extern int dotypeinv(void);
extern const char *dfeature_at(int,int,char *);
extern int look_here(int,BOOLEAN_P);
extern int dolook(void);
extern boolean will_feel_cockatrice(struct obj *,BOOLEAN_P);
extern void feel_cockatrice(struct obj *,BOOLEAN_P);
extern void stackobj(struct obj *);
extern int doprgold(void);
extern int doprwep(void);
extern int doprarm(void);
extern int doprring(void);
extern int dopramulet(void);
extern int doprtool(void);
extern int doprinuse(void);
extern void useupf(struct obj *,long);
extern char *let_to_name(CHAR_P,BOOLEAN_P);
extern void free_invbuf(void);
extern void reassign(void);
extern int doorganize(void);
extern int count_unpaid(struct obj *);
extern int count_buc(struct obj *,int);
extern void carry_obj_effects(struct obj *);
extern const char *currency(long);
extern void silly_thing(const char *,struct obj *);

/* ### ioctl.c ### */

#if defined(UNIX)
extern void getwindowsz(void);
extern void getioctls(void);
extern void setioctls(void);
#endif /* UNIX */

/* ### light.c ### */

extern void new_light_source(XCHAR_P, XCHAR_P, int, int, void *);
extern void del_light_source(int, void *);
extern void do_light_sources(char **);
extern struct monst *find_mid(unsigned, unsigned);
extern void save_light_sources(int, int, int);
extern void restore_light_sources(int);
extern void relink_light_sources(BOOLEAN_P);
extern void obj_move_light_source(struct obj *, struct obj *);
extern boolean any_light_source(void);
extern void snuff_light_source(int, int);
extern boolean obj_sheds_light(struct obj *);
extern boolean obj_is_burning(struct obj *);
extern void obj_split_light_source(struct obj *, struct obj *);
extern void obj_merge_light_sources(struct obj *,struct obj *);
extern int candle_light_range(struct obj *);
extern int wiz_light_sources(void);

/* ### lock.c ### */

extern boolean picking_lock(int *,int *);
extern boolean picking_at(int,int);
extern void reset_pick(void);
extern int pick_lock(struct obj *);
extern int doforce(void);
extern boolean boxlock(struct obj *,struct obj *);
extern boolean doorlock(struct obj *,int,int);
extern int doopen(void);
extern int doclose(void);

/* ### makemon.c ### */

extern boolean is_home_elemental(struct permonst *);
extern struct monst *clone_mon(struct monst *,XCHAR_P,XCHAR_P);
extern struct monst *makemon(struct permonst *,int,int,int);
extern boolean create_critters(int,struct permonst *);
extern struct permonst *rndmonst(void);
extern void reset_rndmonst(int);
extern struct permonst *mkclass(CHAR_P,int);
extern int adj_lev(struct permonst *);
extern struct permonst *grow_up(struct monst *,struct monst *);
extern int mongets(struct monst *,int);
extern int golemhp(int);
extern boolean peace_minded(struct permonst *);
extern void set_malign(struct monst *);
extern void set_mimic_sym(struct monst *);
extern int mbirth_limit(int);
extern void mimic_hit_msg(struct monst *, SHORT_P);
#ifdef GOLDOBJ
extern void mkmonmoney(struct monst *, long);
#endif
extern void bagotricks(struct obj *);
extern boolean propagate(int, BOOLEAN_P,BOOLEAN_P);

/* ### mapglyph.c ### */

extern void mapglyph(int, int *, int *, unsigned *, int, int);

/* ### mcastu.c ### */

extern int castmu(struct monst *,struct attack *,BOOLEAN_P,BOOLEAN_P);
extern int buzzmu(struct monst *,struct attack *);

/* ### mhitm.c ### */

extern int fightm(struct monst *);
extern int mattackm(struct monst *,struct monst *);
extern int noattacks(struct permonst *);
extern int sleep_monst(struct monst *,int,int);
extern void slept_monst(struct monst *);
extern long attk_protection(int);

/* ### mhitu.c ### */

extern const char *mpoisons_subj(struct monst *,struct attack *);
extern void u_slow_down(void);
extern struct monst *cloneu(void);
extern void expels(struct monst *,struct permonst *,BOOLEAN_P);
extern struct attack *getmattk(struct permonst *,int,int *,struct attack *);
extern int mattacku(struct monst *);
extern int magic_negation(struct monst *);
extern int gazemu(struct monst *,struct attack *);
extern void mdamageu(struct monst *,int);
extern int could_seduce(struct monst *,struct monst *,struct attack *);
#ifdef SEDUCE
extern int doseduce(struct monst *);
#endif

/* ### minion.c ### */

extern void msummon(struct monst *);
extern void summon_minion(ALIGNTYP_P,BOOLEAN_P);
extern int demon_talk(struct monst *);
extern long bribe(struct monst *);
extern int dprince(ALIGNTYP_P);
extern int dlord(ALIGNTYP_P);
extern int llord(void);
extern int ndemon(ALIGNTYP_P);
extern int lminion(void);

/* ### mklev.c ### */

extern void sort_rooms(void);
extern void add_room(int,int,int,int,BOOLEAN_P,SCHAR_P,BOOLEAN_P);
extern void add_subroom(struct mkroom *,int,int,int,int,
			   BOOLEAN_P,SCHAR_P,BOOLEAN_P);
extern void makecorridors(void);
extern void add_door(int,int,struct mkroom *);
extern void mklev(void);
#ifdef SPECIALIZATION
extern void topologize(struct mkroom *,BOOLEAN_P);
#else
extern void topologize(struct mkroom *);
#endif
extern void place_branch(branch *,XCHAR_P,XCHAR_P);
extern boolean occupied(XCHAR_P,XCHAR_P);
extern int okdoor(XCHAR_P,XCHAR_P);
extern void dodoor(int,int,struct mkroom *);
extern void mktrap(int,int,struct mkroom *,coord*);
extern void mkstairs(XCHAR_P,XCHAR_P,CHAR_P,struct mkroom *);
extern void mkinvokearea(void);

/* ### mkmap.c ### */

void flood_fill_rm(int,int,int,BOOLEAN_P,BOOLEAN_P);
void remove_rooms(int,int,int,int);

/* ### mkmaze.c ### */

extern void wallification(int,int,int,int);
extern void walkfrom(int,int);
extern void makemaz(const char *);
extern void mazexy(coord *);
extern void bound_digging(void);
extern void mkportal(XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P);
extern boolean bad_location(XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P);
extern void place_lregion(XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,
			     XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,
			     XCHAR_P,d_level *);
extern void movebubbles(void);
extern void water_friction(void);
extern void save_waterlevel(int,int);
extern void restore_waterlevel(int);
extern const char *waterbody_name(XCHAR_P,XCHAR_P);

/* ### mkobj.c ### */

extern struct obj *mkobj_at(CHAR_P,int,int,BOOLEAN_P);
extern struct obj *mksobj_at(int,int,int,BOOLEAN_P,BOOLEAN_P);
extern struct obj *mkobj(CHAR_P,BOOLEAN_P);
extern int rndmonnum(void);
extern struct obj *splitobj(struct obj *,long);
extern void replace_object(struct obj *,struct obj *);
extern void bill_dummy_object(struct obj *);
extern struct obj *mksobj(int,BOOLEAN_P,BOOLEAN_P);
extern int bcsign(struct obj *);
extern int weight(struct obj *);
extern struct obj *mkgold(long,int,int);
extern struct obj *mkcorpstat
	(int,struct monst *,struct permonst *,int,int,BOOLEAN_P);
extern struct obj *obj_attach_mid(struct obj *, unsigned);
extern struct monst *get_mtraits(struct obj *, BOOLEAN_P);
extern struct obj *mk_tt_object(int,int,int);
extern struct obj *mk_named_object
	(int,struct permonst *,int,int,const char *);
extern struct obj *rnd_treefruit_at(int, int);
extern void start_corpse_timeout(struct obj *);
extern void bless(struct obj *);
extern void unbless(struct obj *);
extern void curse(struct obj *);
extern void uncurse(struct obj *);
extern void blessorcurse(struct obj *,int);
extern boolean is_flammable(struct obj *);
extern boolean is_rottable(struct obj *);
extern void place_object(struct obj *,int,int);
extern void remove_object(struct obj *);
extern void discard_minvent(struct monst *);
extern void obj_extract_self(struct obj *);
extern void extract_nobj(struct obj *, struct obj **);
extern void extract_nexthere(struct obj *, struct obj **);
extern int add_to_minv(struct monst *, struct obj *);
extern struct obj *add_to_container(struct obj *, struct obj *);
extern void add_to_migration(struct obj *);
extern void add_to_buried(struct obj *);
extern void dealloc_obj(struct obj *);
extern void obj_ice_effects(int, int, BOOLEAN_P);
extern long peek_at_iced_corpse_age(struct obj *);
extern void obj_sanity_check(void);

/* ### mkroom.c ### */

extern void mkroom(int);
extern void fill_zoo(struct mkroom *);
extern boolean nexttodoor(int,int);
extern boolean has_dnstairs(struct mkroom *);
extern boolean has_upstairs(struct mkroom *);
extern int somex(struct mkroom *);
extern int somey(struct mkroom *);
extern boolean inside_room(struct mkroom *,XCHAR_P,XCHAR_P);
extern boolean somexy(struct mkroom *,coord *);
extern void mkundead(coord *,BOOLEAN_P,int);
extern struct permonst *courtmon(void);
extern void save_rooms(int);
extern void rest_rooms(int);
extern struct mkroom *search_special(SCHAR_P);

/* ### mon.c ### */

extern int undead_to_corpse(int);
extern int genus(int,int);
extern int pm_to_cham(int);
extern int minliquid(struct monst *);
extern int movemon(void);
extern int meatmetal(struct monst *);
extern int meatobj(struct monst *);
extern void mpickgold(struct monst *);
extern boolean mpickstuff(struct monst *,const char *);
extern int curr_mon_load(struct monst *);
extern int max_mon_load(struct monst *);
extern boolean can_carry(struct monst *,struct obj *);
extern int mfndpos(struct monst *,coord *,long *,long);
extern boolean monnear(struct monst *,int,int);
extern void dmonsfree(void);
extern int mcalcmove(struct monst*);
extern void mcalcdistress(void);
extern void replmon(struct monst *,struct monst *);
extern void relmon(struct monst *);
extern struct obj *mlifesaver(struct monst *);
extern boolean corpse_chance(struct monst *,struct monst *,BOOLEAN_P);
extern void mondead(struct monst *);
extern void mondied(struct monst *);
extern void mongone(struct monst *);
extern void monstone(struct monst *);
extern void monkilled(struct monst *,const char *,int);
extern void unstuck(struct monst *);
extern void killed(struct monst *);
extern void xkilled(struct monst *,int);
extern void mon_to_stone(struct monst*);
extern void mnexto(struct monst *);
extern boolean mnearto(struct monst *,XCHAR_P,XCHAR_P,BOOLEAN_P);
extern void poisontell(int);
extern void poisoned(const char *,int,const char *,int);
extern void m_respond(struct monst *);
extern void setmangry(struct monst *);
extern void wakeup(struct monst *);
extern void wake_nearby(void);
extern void wake_nearto(int,int,int);
extern void seemimic(struct monst *);
extern void rescham(void);
extern void restartcham(void);
extern void restore_cham(struct monst *);
extern void mon_animal_list(BOOLEAN_P);
extern int newcham(struct monst *,struct permonst *,BOOLEAN_P,BOOLEAN_P);
extern int can_be_hatched(int);
extern int egg_type_from_parent(int,BOOLEAN_P);
extern boolean dead_species(int,BOOLEAN_P);
extern void kill_genocided_monsters(void);
extern void golemeffects(struct monst *,int,int);
extern boolean angry_guards(BOOLEAN_P);
extern void pacify_guards(void);

/* ### mondata.c ### */

extern void set_mon_data(struct monst *,struct permonst *,int);
extern struct attack *attacktype_fordmg(struct permonst *,int,int);
extern boolean attacktype(struct permonst *,int);
extern boolean poly_when_stoned(struct permonst *);
extern boolean resists_drli(struct monst *);
extern boolean resists_magm(struct monst *);
extern boolean resists_blnd(struct monst *);
extern boolean can_blnd(struct monst *,struct monst *,UCHAR_P,struct obj *);
extern boolean ranged_attk(struct permonst *);
extern boolean hates_silver(struct permonst *);
extern boolean passes_bars(struct permonst *);
extern boolean can_track(struct permonst *);
extern boolean breakarm(struct permonst *);
extern boolean sliparm(struct permonst *);
extern boolean sticks(struct permonst *);
extern int num_horns(struct permonst *);
/* E boolean canseemon(struct monst *); */
extern struct attack *dmgtype_fromattack(struct permonst *,int,int);
extern boolean dmgtype(struct permonst *,int);
extern int max_passive_dmg(struct monst *,struct monst *);
extern int monsndx(struct permonst *);
extern int name_to_mon(const char *);
extern int gender(struct monst *);
extern int pronoun_gender(struct monst *);
extern boolean levl_follower(struct monst *);
extern int little_to_big(int);
extern int big_to_little(int);
extern const char *locomotion(const struct permonst *,const char *);
extern const char *stagger(const struct permonst *,const char *);
extern const char *on_fire(struct permonst *,struct attack *);
extern const struct permonst *raceptr(struct monst *);

/* ### monmove.c ### */

extern boolean itsstuck(struct monst *);
extern boolean mb_trapped(struct monst *);
extern void mon_regen(struct monst *,BOOLEAN_P);
extern int dochugw(struct monst *);
extern boolean onscary(int,int,struct monst *);
extern void monflee(struct monst *, int, BOOLEAN_P, BOOLEAN_P);
extern int dochug(struct monst *);
extern int m_move(struct monst *,int);
extern boolean closed_door(int,int);
extern boolean accessible(int,int);
extern void set_apparxy(struct monst *);
extern boolean can_ooze(struct monst *);

/* ### monst.c ### */

extern void monst_init(void);

/* ### monstr.c ### */

extern void monstr_init(void);

/* ### mplayer.c ### */

extern struct monst *mk_mplayer(struct permonst *,XCHAR_P,
				   XCHAR_P,BOOLEAN_P);
extern void create_mplayers(int,BOOLEAN_P);
extern void mplayer_talk(struct monst *);

#if defined(WIN32)

/* ### msdos.c,os2.c,tos.c,winnt.c ### */

extern char switchar(void);
extern long freediskspace(char *);
extern int findfirst(char *);
extern int findnext(void);
extern long filesize(char *);
extern char *foundfile_buffer(void);
extern void chdrive(char *);
extern void disable_ctrlP(void);
extern void enable_ctrlP(void);
extern char *get_username(int *);
extern void nt_regularize(char *);
extern int (*nt_kbhit)(void);
extern void Delay(int);
#endif /* WIN32 */

/* ### mthrowu.c ### */

extern int thitu(int,int,struct obj *,const char *);
extern int ohitmon(struct monst *,struct obj *,int,BOOLEAN_P);
extern void thrwmu(struct monst *);
extern int spitmu(struct monst *,struct attack *);
extern int breamu(struct monst *,struct attack *);
extern boolean linedup(XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P);
extern boolean lined_up(struct monst *);
extern struct obj *m_carrying(struct monst *,int);
extern void m_useup(struct monst *,struct obj *);
extern void m_throw(struct monst *,int,int,int,int,int,struct obj *);
extern boolean hits_bars(struct obj **,int,int,int,int);

/* ### muse.c ### */

extern boolean find_defensive(struct monst *);
extern int use_defensive(struct monst *);
extern int rnd_defensive_item(struct monst *);
extern boolean find_offensive(struct monst *);
extern int use_offensive(struct monst *);
extern int rnd_offensive_item(struct monst *);
extern boolean find_misc(struct monst *);
extern int use_misc(struct monst *);
extern int rnd_misc_item(struct monst *);
extern boolean searches_for_item(struct monst *,struct obj *);
extern boolean mon_reflects(struct monst *,const char *);
extern boolean ureflects(const char *,const char *);
extern boolean munstone(struct monst *,BOOLEAN_P);

/* ### music.c ### */

extern void awaken_soldiers(void);
extern int do_play_instrument(struct obj *);

/* ### nhlan.c ### */
#ifdef LAN_FEATURES
extern void init_lan_features(void);
extern char *lan_username(void);
#endif

/* ### nttty.c ### */

#ifdef WIN32CON
extern void get_scr_size(void);
extern int nttty_kbhit(void);
extern void nttty_open(void);
extern void nttty_rubout(void);
extern int tgetch(void);
extern int ntposkey(int *, int *, int *);
extern void set_output_mode(int);
extern void synch_cursor(void);
#endif

/* ### o_init.c ### */

extern void init_objects(void);
extern int find_skates(void);
extern void oinit(void);
extern void savenames(int,int);
extern void restnames(int);
extern void discover_object(int,BOOLEAN_P,BOOLEAN_P);
extern void undiscover_object(int);
extern int dodiscovered(void);

/* ### objects.c ### */

extern void objects_init(void);

/* ### objnam.c ### */

extern char *obj_typename(int);
extern char *simple_typename(int);
extern boolean obj_is_pname(struct obj *);
extern char *distant_name(struct obj *,char *(*)(struct obj*));
extern char *fruitname(BOOLEAN_P);
extern char *xname(struct obj *);
extern char *mshot_xname(struct obj *);
extern boolean the_unique_obj(struct obj *obj);
extern char *doname(struct obj *);
extern boolean not_fully_identified(struct obj *);
extern char *corpse_xname(struct obj *,BOOLEAN_P);
extern char *cxname(struct obj *);
extern char *killer_xname(struct obj *);
extern const char *singular(struct obj *,char *(*)(struct obj*));
extern char *an(const char *);
extern char *An(const char *);
extern char *The(const char *);
extern char *the(const char *);
extern char *aobjnam(struct obj *,const char *);
extern char *Tobjnam(struct obj *,const char *);
extern char *otense(struct obj *,const char *);
extern char *vtense(const char *,const char *);
extern char *Doname2(struct obj *);
extern char *yname(struct obj *);
extern char *Yname2(struct obj *);
extern char *ysimple_name(struct obj *);
extern char *Ysimple_name2(struct obj *);
extern char *makeplural(const char *);
extern char *makesingular(const char *);
extern struct obj *readobjnam(char *,struct obj *,BOOLEAN_P);
extern int rnd_class(int,int);
extern const char *cloak_simple_name(struct obj *);
extern const char *mimic_obj_name(struct monst *);

/* ### options.c ### */

extern boolean match_optname(const char *,const char *,int,BOOLEAN_P);
extern void initoptions(void);
extern void parseoptions(char *,BOOLEAN_P,BOOLEAN_P);
extern int doset(void);
extern int dotogglepickup(void);
extern void option_help(void);
extern void next_opt(winid,const char *);
extern int fruitadd(char *);
extern int choose_classes_menu(const char *,int,BOOLEAN_P,char *,char *);
extern void add_menu_cmd_alias(CHAR_P, CHAR_P);
extern char map_menu_cmd(CHAR_P);
extern void assign_warnings(uchar *);
extern char *nh_getenv(const char *);
extern void set_duplicate_opt_detection(int);
extern void set_wc_option_mod_status(unsigned long, int);
extern void set_wc2_option_mod_status(unsigned long, int);
extern void set_option_mod_status(const char *,int);
#ifdef AUTOPICKUP_EXCEPTIONS
extern int add_autopickup_exception(const char *);
extern void free_autopickup_exceptions(void);
#endif /* AUTOPICKUP_EXCEPTIONS */

/* ### pager.c ### */

extern int dowhatis(void);
extern int doquickwhatis(void);
extern int doidtrap(void);
extern int dowhatdoes(void);
extern char *dowhatdoes_core(CHAR_P, char *);
extern int dohelp(void);
extern int dohistory(void);

/* ### pcmain.c ### */

#if defined(WIN32)
# ifdef CHDIR
extern void chdirx(char *,BOOLEAN_P);
# endif /* CHDIR */
#endif /* WIN32 */

/* ### pcsys.c ### */

#if defined(WIN32)
extern void flushout(void);
extern int dosh(void);
extern void append_slash(char *);
extern void getreturn(const char *);
extern void msmsg(const char *,...);
extern FILE *fopenp(const char *,const char *);
#endif /* WIN32 */

/* ### pctty.c ### */

#if defined(WIN32)
extern void gettty(void);
extern void settty(const char *);
extern void setftty(void);
extern void error(const char *,...);
#if defined(TIMED_DELAY) && defined(_MSC_VER)
extern void msleep(unsigned);
#endif
#endif /* WIN32 */

/* ### pcunix.c ### */

#if defined(PC_LOCKING)
extern void getlock(void);
#endif

/* ### pickup.c ### */

#ifdef GOLDOBJ
extern int collect_obj_classes
	(char *,struct obj *,BOOLEAN_P,boolean (*)(struct obj*), int *);
#else
extern int collect_obj_classes
	(char *,struct obj *,BOOLEAN_P,BOOLEAN_P,boolean (*)(struct obj*), int *);
#endif
extern void add_valid_menu_class(int);
extern boolean allow_all(struct obj *);
extern boolean allow_category(struct obj *);
extern boolean is_worn_by_type(struct obj *);
extern int pickup(int);
extern int pickup_object(struct obj *, long, BOOLEAN_P);
extern int query_category(const char *, struct obj *, int,
				menu_item **, int);
extern int query_objlist(const char *, struct obj *, int,
				menu_item **, int, boolean (*)(struct obj*));
extern struct obj *pick_obj(struct obj *);
extern int encumber_msg(void);
extern int doloot(void);
extern int use_container(struct obj *,int);
extern int loot_mon(struct monst *,int *,boolean *);
extern const char *safe_qbuf(const char *,unsigned,
				const char *,const char *,const char *);
extern boolean is_autopickup_exception(struct obj *, BOOLEAN_P);

/* ### pline.c ### */

extern void pline(const char *,...);
extern void Norep(const char *,...);
extern void free_youbuf(void);
extern void You(const char *,...);
extern void Your(const char *,...);
extern void You_feel(const char *,...);
extern void You_cant(const char *,...);
extern void You_hear(const char *,...);
extern void pline_The(const char *,...);
extern void There(const char *,...);
extern void verbalize(const char *,...);
extern void raw_printf(const char *,...);
extern void impossible(const char *,...);
extern const char *align_str(ALIGNTYP_P);
extern void mstatusline(struct monst *);
extern void ustatusline(void);
extern void self_invis_message(void);

/* ### polyself.c ### */

extern void set_uasmon(void);
extern void change_sex(void);
extern void polyself(BOOLEAN_P);
extern int polymon(int);
extern void rehumanize(void);
extern int dobreathe(void);
extern int dospit(void);
extern int doremove(void);
extern int dospinweb(void);
extern int dosummon(void);
extern int dogaze(void);
extern int dohide(void);
extern int domindblast(void);
extern void skinback(BOOLEAN_P);
extern const char *mbodypart(struct monst *,int);
extern const char *body_part(int);
extern int poly_gender(void);
extern void ugolemeffects(int,int);

/* ### potion.c ### */

extern void set_itimeout(long *,long);
extern void incr_itimeout(long *,int);
extern void make_confused(long,BOOLEAN_P);
extern void make_stunned(long,BOOLEAN_P);
extern void make_blinded(long,BOOLEAN_P);
extern void make_sick(long, const char *, BOOLEAN_P,int);
extern void make_vomiting(long,BOOLEAN_P);
extern boolean make_hallucinated(long,BOOLEAN_P,long);
extern int dodrink(void);
extern int dopotion(struct obj *);
extern int peffects(struct obj *);
extern void healup(int,int,BOOLEAN_P,BOOLEAN_P);
extern void strange_feeling(struct obj *,const char *);
extern void potionhit(struct monst *,struct obj *,BOOLEAN_P);
extern void potionbreathe(struct obj *);
extern boolean get_wet(struct obj *);
extern int dodip(void);
extern void djinni_from_bottle(struct obj *);
extern struct monst *split_mon(struct monst *,struct monst *);
extern const char *bottlename(void);

/* ### pray.c ### */

extern int dosacrifice(void);
extern boolean can_pray(BOOLEAN_P);
extern int dopray(void);
extern const char *u_gname(void);
extern int doturn(void);
extern const char *a_gname(void);
extern const char *a_gname_at(XCHAR_P x,XCHAR_P y);
extern const char *align_gname(ALIGNTYP_P);
extern const char *halu_gname(ALIGNTYP_P);
extern const char *align_gtitle(ALIGNTYP_P);
extern void altar_wrath(int,int);


/* ### priest.c ### */

extern int move_special(struct monst *,BOOLEAN_P,SCHAR_P,BOOLEAN_P,BOOLEAN_P,
			   XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P);
extern char temple_occupied(char *);
extern int pri_move(struct monst *);
extern void priestini(d_level *,struct mkroom *,int,int,BOOLEAN_P);
extern char *priestname(struct monst *,char *);
extern boolean p_coaligned(struct monst *);
extern struct monst *findpriest(CHAR_P);
extern void intemple(int);
extern void priest_talk(struct monst *);
extern struct monst *mk_roamer(struct permonst *,ALIGNTYP_P,
				  XCHAR_P,XCHAR_P,BOOLEAN_P);
extern void reset_hostility(struct monst *);
extern boolean in_your_sanctuary(struct monst *,XCHAR_P,XCHAR_P);
extern void ghod_hitsu(struct monst *);
extern void angry_priest(void);
extern void clearpriests(void);
extern void restpriest(struct monst *,BOOLEAN_P);

/* ### quest.c ### */

extern void onquest(void);
extern void nemdead(void);
extern void artitouch(void);
extern boolean ok_to_quest(void);
extern void leader_speaks(struct monst *);
extern void nemesis_speaks(void);
extern void quest_chat(struct monst *);
extern void quest_talk(struct monst *);
extern void quest_stat_check(struct monst *);
extern void finish_quest(struct obj *);

/* ### questpgr.c ### */

extern void load_qtlist(void);
extern void unload_qtlist(void);
extern short quest_info(int);
extern const char *ldrname(void);
extern boolean is_quest_artifact(struct obj*);
extern void com_pager(int);
extern void qt_pager(int);
extern struct permonst *qt_montype(void);

/* ### random.c ### */

#if defined(RANDOM)
extern void srandom(unsigned);
extern char *initstate(unsigned,char *,int);
extern char *setstate(char *);
extern long random(void);
#endif /* RANDOM */

/* ### read.c ### */

extern int doread(void);
extern boolean is_chargeable(struct obj *);
extern void recharge(struct obj *,int);
extern void forget_objects(int);
extern void forget_levels(int);
extern void forget_traps(void);
extern void forget_map(int);
extern int seffects(struct obj *);
extern void litroom(BOOLEAN_P,struct obj *);
extern void do_genocide(int);
extern void punish(struct obj *);
extern void unpunish(void);
extern boolean cant_create(int *, BOOLEAN_P);
extern boolean create_particular(void);

/* ### rect.c ### */

extern void init_rect(void);
extern NhRect *get_rect(NhRect *);
extern NhRect *rnd_rect(void);
extern void remove_rect(NhRect *);
extern void add_rect(NhRect *);
extern void split_rects(NhRect *,NhRect *);

/* ## region.c ### */
extern void clear_regions(void);
extern void run_regions(void);
extern boolean in_out_region(XCHAR_P,XCHAR_P);
extern boolean m_in_out_region(struct monst *,XCHAR_P,XCHAR_P);
extern void update_player_regions(void);
extern void update_monster_region(struct monst *);
extern NhRegion *visible_region_at(XCHAR_P,XCHAR_P);
extern void show_region(NhRegion*, XCHAR_P, XCHAR_P);
extern void save_regions(int,int);
extern void rest_regions(int,BOOLEAN_P);
extern NhRegion* create_gas_cloud(XCHAR_P, XCHAR_P, int, long);

/* ### restore.c ### */

extern void inven_inuse(BOOLEAN_P);
extern int dorecover(int);
extern void trickery(char *);
extern void getlev(int,int,XCHAR_P,BOOLEAN_P);
extern boolean lookup_id_mapping(unsigned, unsigned *);
extern void mread(int,void *,unsigned int);

/* ### rip.c ### */

extern void genl_outrip(winid,int);

/* ### rnd.c ### */

extern int rn2(int);
extern int rnl(int);
extern int rnd(int);
extern int d(int,int);
extern int rne(int);
extern int rnz(int);

/* ### role.c ### */

extern boolean validrole(int);
extern boolean validrace(int, int);
extern boolean validgend(int, int, int);
extern boolean validalign(int, int, int);
extern int randrole(void);
extern int randrace(int);
extern int randgend(int, int);
extern int randalign(int, int);
extern int str2role(char *);
extern int str2race(char *);
extern int str2gend(char *);
extern int str2align(char *);
extern boolean ok_role(int, int, int, int);
extern int pick_role(int, int, int, int);
extern boolean ok_race(int, int, int, int);
extern int pick_race(int, int, int, int);
extern boolean ok_gend(int, int, int, int);
extern int pick_gend(int, int, int, int);
extern boolean ok_align(int, int, int, int);
extern int pick_align(int, int, int, int);
extern void role_init(void);
extern void rigid_role_checks(void);
extern void plnamesuffix(void);
extern const char *Hello(struct monst *);
extern const char *Goodbye(void);
extern char *build_plselection_prompt(char *, int, int, int, int, int);
extern char *root_plselection_prompt(char *, int, int, int, int, int);

/* ### rumors.c ### */

extern char *getrumor(int,char *, BOOLEAN_P);
extern void outrumor(int,int);
extern void outoracle(BOOLEAN_P, BOOLEAN_P);
extern void save_oracles(int,int);
extern void restore_oracles(int);
extern int doconsult(struct monst *);

/* ### save.c ### */

extern int dosave(void);
#if defined(UNIX) || defined(WIN32)
extern void hangup(int);
#endif
extern int dosave0(void);
#ifdef INSURANCE
extern void savestateinlock(void);
#endif
extern void savelev(int,XCHAR_P,int);
extern void bufon(int);
extern void bufoff(int);
extern void bflush(int);
extern void bwrite(int,void *,unsigned int);
extern void bclose(int);
extern void savefruitchn(int,int);
extern void free_dungeons(void);
extern void freedynamicdata(void);

/* ### shk.c ### */

#ifdef GOLDOBJ
extern long money2mon(struct monst *, long);
extern void money2u(struct monst *, long);
#endif
extern char *shkname(struct monst *);
extern void shkgone(struct monst *);
extern void set_residency(struct monst *,BOOLEAN_P);
extern void replshk(struct monst *,struct monst *);
extern void restshk(struct monst *,BOOLEAN_P);
extern char inside_shop(XCHAR_P,XCHAR_P);
extern void u_left_shop(char *,BOOLEAN_P);
extern void remote_burglary(XCHAR_P,XCHAR_P);
extern void u_entered_shop(char *);
extern boolean same_price(struct obj *,struct obj *);
extern void shopper_financial_report(void);
extern int inhishop(struct monst *);
extern struct monst *shop_keeper(CHAR_P);
extern boolean tended_shop(struct mkroom *);
extern void delete_contents(struct obj *);
extern void obfree(struct obj *,struct obj *);
extern void home_shk(struct monst *,BOOLEAN_P);
extern void make_happy_shk(struct monst *,BOOLEAN_P);
extern void hot_pursuit(struct monst *);
extern void make_angry_shk(struct monst *,XCHAR_P,XCHAR_P);
extern int dopay(void);
extern boolean paybill(int);
extern void finish_paybill(void);
extern struct obj *find_oid(unsigned);
extern long contained_cost(struct obj *,struct monst *,long,BOOLEAN_P, BOOLEAN_P);
extern long contained_gold(struct obj *);
extern void picked_container(struct obj *);
extern long unpaid_cost(struct obj *);
extern void addtobill(struct obj *,BOOLEAN_P,BOOLEAN_P,BOOLEAN_P);
extern void splitbill(struct obj *,struct obj *);
extern void subfrombill(struct obj *,struct monst *);
extern long stolen_value(struct obj *,XCHAR_P,XCHAR_P,BOOLEAN_P,BOOLEAN_P);
extern void sellobj_state(int);
extern void sellobj(struct obj *,XCHAR_P,XCHAR_P);
extern int doinvbill(int);
extern struct monst *shkcatch(struct obj *,XCHAR_P,XCHAR_P);
extern void add_damage(XCHAR_P,XCHAR_P,long);
extern int repair_damage(struct monst *,struct damage *,BOOLEAN_P);
extern int shk_move(struct monst *);
extern void after_shk_move(struct monst *);
extern boolean is_fshk(struct monst *);
extern void shopdig(int);
extern void pay_for_damage(const char *,BOOLEAN_P);
extern boolean costly_spot(XCHAR_P,XCHAR_P);
extern struct obj *shop_object(XCHAR_P,XCHAR_P);
extern void price_quote(struct obj *);
extern void shk_chat(struct monst *);
extern void check_unpaid_usage(struct obj *,BOOLEAN_P);
extern void check_unpaid(struct obj *);
extern void costly_gold(XCHAR_P,XCHAR_P,long);
extern boolean block_door(XCHAR_P,XCHAR_P);
extern boolean block_entry(XCHAR_P,XCHAR_P);
extern char *shk_your(char *,struct obj *);
extern char *Shk_Your(char *,struct obj *);

/* ### shknam.c ### */

extern void stock_room(int,struct mkroom *);
extern boolean saleable(struct monst *,struct obj *);
extern int get_shop_item(int);

/* ### sit.c ### */

extern void take_gold(void);
extern int dosit(void);
extern void rndcurse(void);
extern void attrcurse(void);

/* ### sounds.c ### */

extern void dosounds(void);
extern const char *growl_sound(struct monst *);
extern void growl(struct monst *);
extern void yelp(struct monst *);
extern void whimper(struct monst *);
extern void beg(struct monst *);
extern int dotalk(void);

/* ### sp_lev.c ### */

extern boolean check_room(xchar *,xchar *,xchar *,xchar *,BOOLEAN_P);
extern boolean create_room(XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,
			XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P);
extern void create_secret_door(struct mkroom *,XCHAR_P);
extern boolean dig_corridor(coord *,coord *,BOOLEAN_P,SCHAR_P,SCHAR_P);
extern void fill_room(struct mkroom *,BOOLEAN_P);
extern boolean load_special(const char *);

/* ### spell.c ### */

extern int study_book(struct obj *);
extern void book_disappears(struct obj *);
extern void book_substitution(struct obj *,struct obj *);
extern void age_spells(void);
extern int docast(void);
extern int spell_skilltype(int);
extern int spelleffects(int,BOOLEAN_P);
extern void losespells(void);
extern int dovspell(void);
extern void initialspell(struct obj *);

/* ### steal.c ### */

#ifdef GOLDOBJ
extern long somegold(long);
#else
extern long somegold(void);
#endif
extern void stealgold(struct monst *);
extern void remove_worn_item(struct obj *,BOOLEAN_P);
extern int steal(struct monst *, char *);
extern int mpickobj(struct monst *,struct obj *);
extern void stealamulet(struct monst *);
extern void mdrop_special_objs(struct monst *);
extern void relobj(struct monst *,int,BOOLEAN_P);
#ifdef GOLDOBJ
extern struct obj *findgold(struct obj *);
#endif

/* ### steed.c ### */

extern void rider_cant_reach(void);
extern boolean can_saddle(struct monst *);
extern int use_saddle(struct obj *);
extern boolean can_ride(struct monst *);
extern int doride(void);
extern boolean mount_steed(struct monst *, BOOLEAN_P);
extern void exercise_steed(void);
extern void kick_steed(void);
extern void dismount_steed(int);
extern void place_monster(struct monst *,int,int);

/* ### teleport.c ### */

extern boolean goodpos(int,int,struct monst *,unsigned);
extern boolean enexto(coord *,XCHAR_P,XCHAR_P,struct permonst *);
extern boolean enexto_core(coord *,XCHAR_P,XCHAR_P,struct permonst *,unsigned);
extern void teleds(int,int,BOOLEAN_P);
extern boolean safe_teleds(BOOLEAN_P);
extern boolean teleport_pet(struct monst *,BOOLEAN_P);
extern void tele(void);
extern int dotele(void);
extern void level_tele(void);
extern void domagicportal(struct trap *);
extern void tele_trap(struct trap *);
extern void level_tele_trap(struct trap *);
extern void rloc_to(struct monst *,int,int);
extern boolean rloc(struct monst *, BOOLEAN_P);
extern boolean tele_restrict(struct monst *);
extern void mtele_trap(struct monst *, struct trap *,int);
extern int mlevel_tele_trap(struct monst *, struct trap *,BOOLEAN_P,int);
extern void rloco(struct obj *);
extern int random_teleport_level(void);
extern boolean u_teleport_mon(struct monst *,BOOLEAN_P);

/* ### tile.c ### */
#ifdef USE_TILES
extern void substitute_tiles(d_level *);
#endif

/* ### timeout.c ### */

extern void burn_away_slime(void);
extern void nh_timeout(void);
extern void fall_asleep(int, BOOLEAN_P);
extern void attach_egg_hatch_timeout(struct obj *);
extern void attach_fig_transform_timeout(struct obj *);
extern void kill_egg(struct obj *);
extern void hatch_egg(void *, long);
extern void learn_egg_type(int);
extern void burn_object(void *, long);
extern void begin_burn(struct obj *, BOOLEAN_P);
extern void end_burn(struct obj *, BOOLEAN_P);
extern void do_storms(void);
extern boolean start_timer(long, SHORT_P, SHORT_P, void *);
extern long stop_timer(SHORT_P, void *);
extern void run_timers(void);
extern void obj_move_timers(struct obj *, struct obj *);
extern void obj_split_timers(struct obj *, struct obj *);
extern void obj_stop_timers(struct obj *);
extern boolean obj_is_local(struct obj *);
extern void save_timers(int,int,int);
extern void restore_timers(int,int,BOOLEAN_P,long);
extern void relink_timers(BOOLEAN_P);
extern int wiz_timeout_queue(void);
extern void timer_sanity_check(void);

/* ### topten.c ### */

extern void topten(int);
extern void prscore(int,char **);
extern struct obj *tt_oname(struct obj *);

/* ### track.c ### */

extern void initrack(void);
extern void settrack(void);
extern coord *gettrack(int,int);

/* ### trap.c ### */

extern boolean burnarmor(struct monst *);
extern boolean rust_dmg(struct obj *,const char *,int,BOOLEAN_P,struct monst *);
extern void grease_protect(struct obj *,const char *,struct monst *);
extern struct trap *maketrap(int,int,int);
extern void fall_through(BOOLEAN_P);
extern struct monst *animate_statue(struct obj *,XCHAR_P,XCHAR_P,int,int *);
extern struct monst *activate_statue_trap(struct trap *,XCHAR_P,XCHAR_P,BOOLEAN_P);
extern void dotrap(struct trap *, unsigned);
extern void seetrap(struct trap *);
extern int mintrap(struct monst *);
extern void instapetrify(const char *);
extern void minstapetrify(struct monst *,BOOLEAN_P);
extern void selftouch(const char *);
extern void mselftouch(struct monst *,const char *,BOOLEAN_P);
extern void float_up(void);
extern void fill_pit(int,int);
extern int float_down(long, long);
extern int fire_damage(struct obj *,BOOLEAN_P,BOOLEAN_P,XCHAR_P,XCHAR_P);
extern void water_damage(struct obj *,BOOLEAN_P,BOOLEAN_P);
extern boolean drown(void);
extern void drain_en(int);
extern int dountrap(void);
extern int untrap(BOOLEAN_P);
extern boolean chest_trap(struct obj *,int,BOOLEAN_P);
extern void deltrap(struct trap *);
extern boolean delfloortrap(struct trap *);
extern struct trap *t_at(int,int);
extern void b_trapped(const char *,int);
extern boolean unconscious(void);
extern boolean lava_effects(void);
extern void blow_up_landmine(struct trap *);
extern int launch_obj(SHORT_P,int,int,int,int,int);

/* ### u_init.c ### */

extern void u_init(void);

/* ### uhitm.c ### */

extern void hurtmarmor(struct monst *,int);
extern boolean attack_checks(struct monst *,struct obj *);
extern void check_caitiff(struct monst *);
extern schar find_roll_to_hit(struct monst *);
extern boolean attack(struct monst *);
extern boolean hmon(struct monst *,struct obj *,int);
extern int damageum(struct monst *,struct attack *);
extern void missum(struct monst *,struct attack *);
extern int passive(struct monst *,BOOLEAN_P,int,UCHAR_P);
extern void passive_obj(struct monst *,struct obj *,struct attack *);
extern void stumble_onto_mimic(struct monst *);
extern int flash_hits_mon(struct monst *,struct obj *);

/* ### unixmain.c ### */

#ifdef UNIX
# ifdef PORT_HELP
extern void port_help(void);
# endif
#endif /* UNIX */


/* ### unixtty.c ### */

#if defined(UNIX)
extern void gettty(void);
extern void settty(const char *);
extern void setftty(void);
extern void intron(void);
extern void introff(void);
extern void error(const char *,...);
#endif /* UNIX */

/* ### unixunix.c ### */

#ifdef UNIX
extern void getlock(void);
extern void regularize(char *);
# if defined(TIMED_DELAY) && !defined(msleep) && defined(SYSV)
extern void msleep(unsigned);
# endif
#endif /* UNIX */

/* ### vault.c ### */

extern boolean grddead(struct monst *);
extern char vault_occupied(char *);
extern void invault(void);
extern int gd_move(struct monst *);
extern void paygd(void);
extern long hidden_gold(void);
extern boolean gd_sound(void);

/* ### version.c ### */

extern char *version_string(char *);
extern char *getversionstring(char *);
extern int doversion(void);
extern int doextversion(void);
extern boolean check_version(struct version_info *, const char *, BOOLEAN_P);
extern unsigned long get_feature_notice_ver(char *);
extern unsigned long get_current_feature_ver(void);
#ifdef RUNTIME_PORT_ID
extern void append_port_id(char *);
#endif

/* ### vision.c ### */

extern void vision_init(void);
extern int does_block(int,int,struct rm*);
extern void vision_reset(void);
extern void vision_recalc(int);
extern void block_point(int,int);
extern void unblock_point(int,int);
extern boolean clear_path(int,int,int,int);
extern void do_clear_area(int,int,int, void (*)(int,int,void *),void *);

/* ### weapon.c ### */

extern int hitval(struct obj *,struct monst *);
extern int dmgval(struct obj *,struct monst *);
extern struct obj *select_rwep(struct monst *);
extern struct obj *select_hwep(struct monst *);
extern void possibly_unwield(struct monst *,BOOLEAN_P);
extern int mon_wield_item(struct monst *);
extern int abon(void);
extern int dbon(void);
extern int enhance_weapon_skill(void);
extern void unrestrict_weapon_skill(int);
extern void use_skill(int,int);
extern void add_weapon_skill(int);
extern void lose_weapon_skill(int);
extern int weapon_type(struct obj *);
extern int uwep_skill_type(void);
extern int weapon_hit_bonus(struct obj *);
extern int weapon_dam_bonus(struct obj *);
extern void skill_init(const struct def_skill *);

/* ### were.c ### */

extern void were_change(struct monst *);
extern void new_were(struct monst *);
extern int were_summon(struct permonst *,BOOLEAN_P,int *,char *);
extern void you_were(void);
extern void you_unwere(BOOLEAN_P);

/* ### wield.c ### */

extern void setuwep(struct obj *);
extern void setuqwep(struct obj *);
extern void setuswapwep(struct obj *);
extern int dowield(void);
extern int doswapweapon(void);
extern int dowieldquiver(void);
extern boolean wield_tool(struct obj *,const char *);
extern int can_twoweapon(void);
extern void drop_uswapwep(void);
extern int dotwoweapon(void);
extern void uwepgone(void);
extern void uswapwepgone(void);
extern void uqwepgone(void);
extern void untwoweapon(void);
extern void erode_obj(struct obj *,BOOLEAN_P,BOOLEAN_P);
extern int chwepon(struct obj *,int);
extern int welded(struct obj *);
extern void weldmsg(struct obj *);
extern void setmnotwielded(struct monst *,struct obj *);

/* ### windows.c ### */

extern void choose_windows(const char *);
extern char genl_message_menu(CHAR_P,int,const char *);
extern void genl_preference_update(const char *);

/* ### wizard.c ### */

extern void amulet(void);
extern int mon_has_amulet(struct monst *);
extern int mon_has_special(struct monst *);
extern int tactics(struct monst *);
extern void aggravate(void);
extern void clonewiz(void);
extern int pick_nasty(void);
extern int nasty(struct monst*);
extern void resurrect(void);
extern void intervene(void);
extern void wizdead(void);
extern void cuss(struct monst *);

/* ### worm.c ### */

extern int get_wormno(void);
extern void initworm(struct monst *,int);
extern void worm_move(struct monst *);
extern void worm_nomove(struct monst *);
extern void wormgone(struct monst *);
extern void wormhitu(struct monst *);
extern void cutworm(struct monst *,XCHAR_P,XCHAR_P,struct obj *);
extern void see_wsegs(struct monst *);
extern void detect_wsegs(struct monst *,BOOLEAN_P);
extern void save_worm(int,int);
extern void rest_worm(int);
extern void place_wsegs(struct monst *);
extern void remove_worm(struct monst *);
extern void place_worm_tail_randomly(struct monst *,XCHAR_P,XCHAR_P);
extern int count_wsegs(struct monst *);
extern boolean worm_known(struct monst *);

/* ### worn.c ### */

extern void setworn(struct obj *,long);
extern void setnotworn(struct obj *);
extern void mon_set_minvis(struct monst *);
extern void mon_adjust_speed(struct monst *,int,struct obj *);
extern void update_mon_intrinsics(struct monst *,struct obj *,BOOLEAN_P,BOOLEAN_P);
extern int find_mac(struct monst *);
extern void m_dowear(struct monst *,BOOLEAN_P);
extern struct obj *which_armor(struct monst *,long);
extern void mon_break_armor(struct monst *,BOOLEAN_P);
extern void bypass_obj(struct obj *);
extern void clear_bypasses(void);
extern int racial_exception(struct monst *, struct obj *);

/* ### write.c ### */

extern int dowrite(struct obj *);

/* ### zap.c ### */

extern int bhitm(struct monst *,struct obj *);
extern void probe_monster(struct monst *);
extern boolean get_obj_location(struct obj *,xchar *,xchar *,int);
extern boolean get_mon_location(struct monst *,xchar *,xchar *,int);
extern struct monst *get_container_location(struct obj *obj, int *, int *);
extern struct monst *montraits(struct obj *,coord *);
extern struct monst *revive(struct obj *);
extern int unturn_dead(struct monst *);
extern void cancel_item(struct obj *);
extern boolean drain_item(struct obj *);
extern struct obj *poly_obj(struct obj *, int);
extern boolean obj_resists(struct obj *,int,int);
extern boolean obj_shudders(struct obj *);
extern void do_osshock(struct obj *);
extern int bhito(struct obj *,struct obj *);
extern int bhitpile(struct obj *,int (*)(struct obj*,struct obj*),int,int);
extern int zappable(struct obj *);
extern void zapnodir(struct obj *);
extern int dozap(void);
extern int zapyourself(struct obj *,BOOLEAN_P);
extern boolean cancel_monst(struct monst *,struct obj *,
			       BOOLEAN_P,BOOLEAN_P,BOOLEAN_P);
extern void weffects(struct obj *);
extern int spell_damage_bonus(void);
extern const char *exclam(int force);
extern void hit(const char *,struct monst *,const char *);
extern void miss(const char *,struct monst *);
extern struct monst *bhit(int,int,int,int,int (*)(struct monst*,struct obj*),
			     int (*)(struct obj*,struct obj*),struct obj *);
extern struct monst *boomhit(int,int);
extern int burn_floor_paper(int,int,BOOLEAN_P,BOOLEAN_P);
extern void buzz(int,int,XCHAR_P,XCHAR_P,int,int);
extern void melt_ice(XCHAR_P,XCHAR_P);
extern int zap_over_floor(XCHAR_P,XCHAR_P,int,boolean *);
extern void fracture_rock(struct obj *);
extern boolean break_statue(struct obj *);
extern void destroy_item(int,int);
extern int destroy_mitem(struct monst *,int,int);
extern int resist(struct monst *,CHAR_P,int,int);
extern void makewish(void);

#endif /* !MAKEDEFS_C && !LEV_LEX_C */

#endif /* EXTERN_H */
