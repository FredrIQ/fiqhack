/*	SCCS Id: @(#)extern.h	3.4	2003/03/10	*/
/* Copyright (c) Steve Creps, 1988.				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef EXTERN_H
#define EXTERN_H

#define E extern

/* ### alloc.c ### */

#if 0
E long *FDECL(alloc, (unsigned int));
#endif
E char *FDECL(fmt_ptr, (const void *,char *));

/* This next pre-processor directive covers almost the entire file,
 * interrupted only occasionally to pick up specific functions as needed. */
#if !defined(MAKEDEFS_C) && !defined(LEV_LEX_C)

/* ### allmain.c ### */

E void moveloop(void);
E void stop_occupation(void);
E void display_gamewindows(void);
E void newgame(void);
E void FDECL(welcome, (BOOLEAN_P));

/* ### apply.c ### */

E int doapply(void);
E int dorub(void);
E int dojump(void);
E int FDECL(jump, (int));
E int number_leashed(void);
E void FDECL(o_unleash, (struct obj *));
E void FDECL(m_unleash, (struct monst *,BOOLEAN_P));
E void unleash_all(void);
E boolean next_to_u(void);
E struct obj *FDECL(get_mleash, (struct monst *));
E void FDECL(check_leash, (XCHAR_P,XCHAR_P));
E boolean FDECL(um_dist, (XCHAR_P,XCHAR_P,XCHAR_P));
E boolean FDECL(snuff_candle, (struct obj *));
E boolean FDECL(snuff_lit, (struct obj *));
E boolean FDECL(catch_lit, (struct obj *));
E void FDECL(use_unicorn_horn, (struct obj *));
E boolean FDECL(tinnable, (struct obj *));
E void reset_trapset(void);
E void FDECL(fig_transform, (void *, long));
E int FDECL(unfixable_trouble_count,(BOOLEAN_P));

/* ### artifact.c ### */

E void init_artifacts(void);
E void FDECL(save_artifacts, (int));
E void FDECL(restore_artifacts, (int));
E const char *FDECL(artiname, (int));
E struct obj *FDECL(mk_artifact, (struct obj *,ALIGNTYP_P));
E const char *FDECL(artifact_name, (const char *,short *));
E boolean FDECL(exist_artifact, (int,const char *));
E void FDECL(artifact_exists, (struct obj *,const char *,BOOLEAN_P));
E int nartifact_exist(void);
E boolean FDECL(spec_ability, (struct obj *,unsigned long));
E boolean FDECL(confers_luck, (struct obj *));
E boolean FDECL(arti_reflects, (struct obj *));
E boolean FDECL(restrict_name, (struct obj *,const char *));
E boolean FDECL(defends, (int,struct obj *));
E boolean FDECL(protects, (int,struct obj *));
E void FDECL(set_artifact_intrinsic, (struct obj *,BOOLEAN_P,long));
E int FDECL(touch_artifact, (struct obj *,struct monst *));
E int FDECL(spec_abon, (struct obj *,struct monst *));
E int FDECL(spec_dbon, (struct obj *,struct monst *,int));
E void FDECL(discover_artifact, (XCHAR_P));
E boolean FDECL(undiscovered_artifact, (XCHAR_P));
E int FDECL(disp_artifact_discoveries, (winid));
E boolean FDECL(artifact_hit, (struct monst *,struct monst *,
				struct obj *,int *,int));
E int doinvoke(void);
E void FDECL(arti_speak, (struct obj *));
E boolean FDECL(artifact_light, (struct obj *));
E long FDECL(spec_m2, (struct obj *));
E boolean FDECL(artifact_has_invprop, (struct obj *,UCHAR_P));
E long FDECL(arti_cost, (struct obj *));

/* ### attrib.c ### */

E boolean FDECL(adjattrib, (int,int,int));
E void FDECL(change_luck, (SCHAR_P));
E int FDECL(stone_luck, (BOOLEAN_P));
E void set_moreluck(void);
E void FDECL(gainstr, (struct obj *,int));
E void FDECL(losestr, (int));
E void restore_attrib(void);
E void FDECL(exercise, (int,BOOLEAN_P));
E void exerchk(void);
E void reset_attribute_clock(void);
E void FDECL(init_attr, (int));
E void redist_attr(void);
E void FDECL(adjabil, (int,int));
E int newhp(void);
E schar FDECL(acurr, (int));
E schar acurrstr(void);
E void FDECL(adjalign, (int));

/* ### ball.c ### */

E void ballfall(void);
E void placebc(void);
E void unplacebc(void);
E void FDECL(set_bc, (int));
E void FDECL(move_bc, (int,int,XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P));
E boolean FDECL(drag_ball, (XCHAR_P,XCHAR_P,
		int *,xchar *,xchar *,xchar *,xchar *, boolean *,BOOLEAN_P));
E void FDECL(drop_ball, (XCHAR_P,XCHAR_P));
E void drag_down(void);

/* ### bones.c ### */

E boolean can_make_bones(void);
E void FDECL(savebones, (struct obj *));
E int getbones(void);

/* ### botl.c ### */

E int FDECL(xlev_to_rank, (int));
E int FDECL(title_to_mon, (const char *,int *,int *));
E void max_rank_sz(void);
#ifdef SCORE_ON_BOTL
E long botl_score(void);
#endif
E int FDECL(describe_level, (char *));
E const char *FDECL(rank_of, (int,SHORT_P,BOOLEAN_P));
E void bot(void);

/* ### cmd.c ### */

E void reset_occupations(void);
E void FDECL(set_occupation, (int (*)(void),const char *,int));
#ifdef REDO
E char pgetchar(void);
E void FDECL(pushch, (CHAR_P));
E void FDECL(savech, (CHAR_P));
#endif
#ifdef WIZARD
E void add_debug_extended_commands(void);
#endif /* WIZARD */
E void FDECL(rhack, (char *));
E int doextlist(void);
E int extcmd_via_menu(void);
E void FDECL(enlightenment, (int));
E void FDECL(show_conduct, (int));
E int FDECL(xytod, (SCHAR_P,SCHAR_P));
E void FDECL(dtoxy, (coord *,int));
E int FDECL(movecmd, (CHAR_P));
E int FDECL(getdir, (const char *));
E void confdir(void);
E int FDECL(isok, (int,int));
E int FDECL(get_adjacent_loc, (const char *, const char *, XCHAR_P, XCHAR_P, coord *));
E const char *FDECL(click_to_cmd, (int,int,int));
E char readchar(void);
#ifdef WIZARD
E void sanity_check(void);
#endif
E char FDECL(yn_function, (const char *, const char *, CHAR_P));

/* ### dbridge.c ### */

E boolean FDECL(is_pool, (int,int));
E boolean FDECL(is_lava, (int,int));
E boolean FDECL(is_ice, (int,int));
E int FDECL(is_drawbridge_wall, (int,int));
E boolean FDECL(is_db_wall, (int,int));
E boolean FDECL(find_drawbridge, (int *,int*));
E boolean FDECL(create_drawbridge, (int,int,int,BOOLEAN_P));
E void FDECL(open_drawbridge, (int,int));
E void FDECL(close_drawbridge, (int,int));
E void FDECL(destroy_drawbridge, (int,int));

/* ### decl.c ### */

E void decl_init(void);

/* ### detect.c ### */

E struct obj *FDECL(o_in, (struct obj*,CHAR_P));
E struct obj *FDECL(o_material, (struct obj*,unsigned));
E int FDECL(gold_detect, (struct obj *));
E int FDECL(food_detect, (struct obj *));
E int FDECL(object_detect, (struct obj *,int));
E int FDECL(monster_detect, (struct obj *,int));
E int FDECL(trap_detect, (struct obj *));
E const char *FDECL(level_distance, (d_level *));
E void FDECL(use_crystal_ball, (struct obj *));
E void do_mapping(void);
E void do_vicinity_map(void);
E void FDECL(cvt_sdoor_to_door, (struct rm *));
E int findit(void);
E int openit(void);
E void FDECL(find_trap, (struct trap *));
E int FDECL(dosearch0, (int));
E int dosearch(void);
E void sokoban_detect(void);

/* ### dig.c ### */

E boolean is_digging(void);
E int holetime(void);
E boolean FDECL(dig_check, (struct monst *, BOOLEAN_P, int, int));
E void FDECL(digactualhole, (int,int,struct monst *,int));
E boolean FDECL(dighole, (BOOLEAN_P));
E int FDECL(use_pick_axe, (struct obj *));
E int FDECL(use_pick_axe2, (struct obj *));
E boolean FDECL(mdig_tunnel, (struct monst *));
E void FDECL(watch_dig, (struct monst *,XCHAR_P,XCHAR_P,BOOLEAN_P));
E void zap_dig(void);
E struct obj *FDECL(bury_an_obj, (struct obj *));
E void FDECL(bury_objs, (int,int));
E void FDECL(unearth_objs, (int,int));
E void FDECL(rot_organic, (void *, long));
E void FDECL(rot_corpse, (void *, long));
#if 0
E void FDECL(bury_monst, (struct monst *));
E void bury_you(void);
E void unearth_you(void);
E void escape_tomb(void);
E void FDECL(bury_obj, (struct obj *));
#endif

/* ### display.c ### */

#ifdef INVISIBLE_OBJECTS
E struct obj * FDECL(vobj_at, (XCHAR_P,XCHAR_P));
#endif /* INVISIBLE_OBJECTS */
E void FDECL(magic_map_background, (XCHAR_P,XCHAR_P,int));
E void FDECL(map_background, (XCHAR_P,XCHAR_P,int));
E void FDECL(map_trap, (struct trap *,int));
E void FDECL(map_object, (struct obj *,int));
E void FDECL(map_invisible, (XCHAR_P,XCHAR_P));
E void FDECL(unmap_object, (int,int));
E void FDECL(map_location, (int,int,int));
E void FDECL(feel_location, (XCHAR_P,XCHAR_P));
E void FDECL(newsym, (int,int));
E void FDECL(shieldeff, (XCHAR_P,XCHAR_P));
E void FDECL(tmp_at, (int,int));
E void FDECL(swallowed, (int));
E void FDECL(under_ground, (int));
E void FDECL(under_water, (int));
E void see_monsters(void);
E void set_mimic_blocking(void);
E void see_objects(void);
E void see_traps(void);
E void curs_on_u(void);
E int doredraw(void);
E void docrt(void);
E void FDECL(show_glyph, (int,int,int));
E void clear_glyph_buffer(void);
E void FDECL(row_refresh, (int,int,int));
E void cls(void);
E void FDECL(flush_screen, (int));
E int FDECL(back_to_glyph, (XCHAR_P,XCHAR_P));
E int FDECL(zapdir_to_glyph, (int,int,int));
E int FDECL(glyph_at, (XCHAR_P,XCHAR_P));
E void set_wall_state(void);

/* ### do.c ### */

E int dodrop(void);
E boolean FDECL(boulder_hits_pool, (struct obj *,int,int,BOOLEAN_P));
E boolean FDECL(flooreffects, (struct obj *,int,int,const char *));
E void FDECL(doaltarobj, (struct obj *));
E boolean FDECL(canletgo, (struct obj *,const char *));
E void FDECL(dropx, (struct obj *));
E void FDECL(dropy, (struct obj *));
E void FDECL(obj_no_longer_held, (struct obj *));
E int doddrop(void);
E int dodown(void);
E int doup(void);
#ifdef INSURANCE
E void save_currentstate(void);
#endif
E void FDECL(goto_level, (d_level *,BOOLEAN_P,BOOLEAN_P,BOOLEAN_P));
E void FDECL(schedule_goto, (d_level *,BOOLEAN_P,BOOLEAN_P,int,
			     const char *,const char *));
E void deferred_goto(void);
E boolean FDECL(revive_corpse, (struct obj *));
E void FDECL(revive_mon, (void *, long));
E int donull(void);
E int dowipe(void);
E void FDECL(set_wounded_legs, (long,int));
E void heal_legs(void);

/* ### do_name.c ### */

E int FDECL(getpos, (coord *,BOOLEAN_P,const char *));
E struct monst *FDECL(christen_monst, (struct monst *,const char *));
E int do_mname(void);
E struct obj *FDECL(oname, (struct obj *,const char *));
E int ddocall(void);
E void FDECL(docall, (struct obj *));
E const char *rndghostname(void);
E char *FDECL(x_monnam, (struct monst *,int,const char *,int,BOOLEAN_P));
E char *FDECL(l_monnam, (struct monst *));
E char *FDECL(mon_nam, (struct monst *));
E char *FDECL(noit_mon_nam, (struct monst *));
E char *FDECL(Monnam, (struct monst *));
E char *FDECL(noit_Monnam, (struct monst *));
E char *FDECL(m_monnam, (struct monst *));
E char *FDECL(y_monnam, (struct monst *));
E char *FDECL(Adjmonnam, (struct monst *,const char *));
E char *FDECL(Amonnam, (struct monst *));
E char *FDECL(a_monnam, (struct monst *));
E char *FDECL(distant_monnam, (struct monst *,int,char *));
E const char *rndmonnam(void);
E const char *FDECL(hcolor, (const char *));
E const char *rndcolor(void);
#ifdef REINCARNATION
E const char *roguename(void);
#endif
E struct obj *FDECL(realloc_obj,
		(struct obj *, int, void *, int, const char *));
E char *FDECL(coyotename, (struct monst *,char *));

/* ### do_wear.c ### */

E void FDECL(off_msg, (struct obj *));
E void set_wear(void);
E boolean FDECL(donning, (struct obj *));
E void cancel_don(void);
E int Armor_off(void);
E int Armor_gone(void);
E int Helmet_off(void);
E int Gloves_off(void);
E int Boots_off(void);
E int Cloak_off(void);
E int Shield_off(void);
#ifdef TOURIST
E int Shirt_off(void);
#endif
E void Amulet_off(void);
E void FDECL(Ring_on, (struct obj *));
E void FDECL(Ring_off, (struct obj *));
E void FDECL(Ring_gone, (struct obj *));
E void FDECL(Blindf_on, (struct obj *));
E void FDECL(Blindf_off, (struct obj *));
E int dotakeoff(void);
E int doremring(void);
E int FDECL(cursed, (struct obj *));
E int FDECL(armoroff, (struct obj *));
E int FDECL(canwearobj, (struct obj *, long *, BOOLEAN_P));
E int dowear(void);
E int doputon(void);
E void find_ac(void);
E void glibr(void);
E struct obj *FDECL(some_armor,(struct monst *));
E void FDECL(erode_armor, (struct monst *,BOOLEAN_P));
E struct obj *FDECL(stuck_ring, (struct obj *,int));
E struct obj *unchanger(void);
E void reset_remarm(void);
E int doddoremarm(void);
E int FDECL(destroy_arm, (struct obj *));
E void FDECL(adj_abon, (struct obj *,SCHAR_P));

/* ### dog.c ### */

E void FDECL(initedog, (struct monst *));
E struct monst *FDECL(make_familiar, (struct obj *,XCHAR_P,XCHAR_P,BOOLEAN_P));
E struct monst *makedog(void);
E void update_mlstmv(void);
E void losedogs(void);
E void FDECL(mon_arrive, (struct monst *,BOOLEAN_P));
E void FDECL(mon_catchup_elapsed_time, (struct monst *,long));
E void FDECL(keepdogs, (BOOLEAN_P));
E void FDECL(migrate_to_level, (struct monst *,XCHAR_P,XCHAR_P,coord *));
E int FDECL(dogfood, (struct monst *,struct obj *));
E struct monst *FDECL(tamedog, (struct monst *,struct obj *));
E void FDECL(abuse_dog, (struct monst *));
E void FDECL(wary_dog, (struct monst *, BOOLEAN_P));

/* ### dogmove.c ### */

E int FDECL(dog_nutrition, (struct monst *,struct obj *));
E int FDECL(dog_eat, (struct monst *,struct obj *,int,int,BOOLEAN_P));
E int FDECL(dog_move, (struct monst *,int));

/* ### dokick.c ### */

E boolean FDECL(ghitm, (struct monst *,struct obj *));
E void FDECL(container_impact_dmg, (struct obj *));
E int dokick(void);
E boolean FDECL(ship_object, (struct obj *,XCHAR_P,XCHAR_P,BOOLEAN_P));
E void obj_delivery(void);
E schar FDECL(down_gate, (XCHAR_P,XCHAR_P));
E void FDECL(impact_drop, (struct obj *,XCHAR_P,XCHAR_P,XCHAR_P));

/* ### dothrow.c ### */

E int dothrow(void);
E int dofire(void);
E void FDECL(hitfloor, (struct obj *));
E void FDECL(hurtle, (int,int,int,BOOLEAN_P));
E void FDECL(mhurtle, (struct monst *,int,int,int));
E void FDECL(throwit, (struct obj *,long,BOOLEAN_P));
E int FDECL(omon_adj, (struct monst *,struct obj *,BOOLEAN_P));
E int FDECL(thitmonst, (struct monst *,struct obj *));
E int FDECL(hero_breaks, (struct obj *,XCHAR_P,XCHAR_P,BOOLEAN_P));
E int FDECL(breaks, (struct obj *,XCHAR_P,XCHAR_P));
E boolean FDECL(breaktest, (struct obj *));
E boolean FDECL(walk_path, (coord *, coord *, boolean (*)(void *,int,int), void *));
E boolean FDECL(hurtle_step, (void *, int, int));

/* ### drawing.c ### */
#endif /* !MAKEDEFS_C && !LEV_LEX_C */
E int FDECL(def_char_to_objclass, (CHAR_P));
E int FDECL(def_char_to_monclass, (CHAR_P));
#if !defined(MAKEDEFS_C) && !defined(LEV_LEX_C)
E void FDECL(assign_graphics, (uchar *,int,int,int));
E void FDECL(switch_graphics, (int));
#ifdef REINCARNATION
E void FDECL(assign_rogue_graphics, (BOOLEAN_P));
#endif

/* ### dungeon.c ### */

E void FDECL(save_dungeon, (int,BOOLEAN_P,BOOLEAN_P));
E void FDECL(restore_dungeon, (int));
E void FDECL(insert_branch, (branch *,BOOLEAN_P));
E void init_dungeons(void);
E s_level *FDECL(find_level, (const char *));
E s_level *FDECL(Is_special, (d_level *));
E branch *FDECL(Is_branchlev, (d_level *));
E xchar FDECL(ledger_no, (d_level *));
E xchar maxledgerno(void);
E schar FDECL(depth, (d_level *));
E xchar FDECL(dunlev, (d_level *));
E xchar FDECL(dunlevs_in_dungeon, (d_level *));
E xchar FDECL(ledger_to_dnum, (XCHAR_P));
E xchar FDECL(ledger_to_dlev, (XCHAR_P));
E xchar FDECL(deepest_lev_reached, (BOOLEAN_P));
E boolean FDECL(on_level, (d_level *,d_level *));
E void FDECL(next_level, (BOOLEAN_P));
E void FDECL(prev_level, (BOOLEAN_P));
E void FDECL(u_on_newpos, (int,int));
E void u_on_sstairs(void);
E void u_on_upstairs(void);
E void u_on_dnstairs(void);
E boolean FDECL(On_stairs, (XCHAR_P,XCHAR_P));
E void FDECL(get_level, (d_level *,int));
E boolean FDECL(Is_botlevel, (d_level *));
E boolean FDECL(Can_fall_thru, (d_level *));
E boolean FDECL(Can_dig_down, (d_level *));
E boolean FDECL(Can_rise_up, (int,int,d_level *));
E boolean FDECL(In_quest, (d_level *));
E boolean FDECL(In_mines, (d_level *));
E branch *FDECL(dungeon_branch, (const char *));
E boolean FDECL(at_dgn_entrance, (const char *));
E boolean FDECL(In_hell, (d_level *));
E boolean FDECL(In_V_tower, (d_level *));
E boolean FDECL(On_W_tower_level, (d_level *));
E boolean FDECL(In_W_tower, (int,int,d_level *));
E void FDECL(find_hell, (d_level *));
E void FDECL(goto_hell, (BOOLEAN_P,BOOLEAN_P));
E void FDECL(assign_level, (d_level *,d_level *));
E void FDECL(assign_rnd_level, (d_level *,d_level *,int));
E int FDECL(induced_align, (int));
E boolean FDECL(Invocation_lev, (d_level *));
E xchar level_difficulty(void);
E schar FDECL(lev_by_name, (const char *));
#ifdef WIZARD
E schar FDECL(print_dungeon, (BOOLEAN_P,schar *,xchar *));
#endif

/* ### eat.c ### */

E boolean FDECL(is_edible, (struct obj *));
E void init_uhunger(void);
E int Hear_again(void);
E void reset_eat(void);
E int doeat(void);
E void gethungry(void);
E void FDECL(morehungry, (int));
E void FDECL(lesshungry, (int));
E boolean is_fainted(void);
E void reset_faint(void);
E void violated_vegetarian(void);
#if 0
E void sync_hunger(void);
#endif
E void FDECL(newuhs, (BOOLEAN_P));
E struct obj *FDECL(floorfood, (const char *,int));
E void vomit(void);
E int FDECL(eaten_stat, (int,struct obj *));
E void FDECL(food_disappears, (struct obj *));
E void FDECL(food_substitution, (struct obj *,struct obj *));
E void fix_petrification(void);
E void FDECL(consume_oeaten, (struct obj *,int));
E boolean FDECL(maybe_finished_meal, (BOOLEAN_P));

/* ### end.c ### */

E void FDECL(done1, (int));
E int done2(void);
E void FDECL(done_in_by, (struct monst *));
#endif /* !MAKEDEFS_C && !LEV_LEX_C */
E void VDECL(panic, (const char *,...)) PRINTF_F(1,2);
#if !defined(MAKEDEFS_C) && !defined(LEV_LEX_C)
E void FDECL(done, (int));
E void FDECL(container_contents, (struct obj *,BOOLEAN_P,BOOLEAN_P));
E void FDECL(terminate, (int));
E int num_genocides(void);

/* ### engrave.c ### */

E char *FDECL(random_engraving, (char *));
E void FDECL(wipeout_text, (char *,int,unsigned));
E boolean can_reach_floor(void);
E const char *FDECL(surface, (int,int));
E const char *FDECL(ceiling, (int,int));
E struct engr *FDECL(engr_at, (XCHAR_P,XCHAR_P));
#ifdef ELBERETH
E int FDECL(sengr_at, (const char *,XCHAR_P,XCHAR_P));
#endif
E void FDECL(u_wipe_engr, (int));
E void FDECL(wipe_engr_at, (XCHAR_P,XCHAR_P,XCHAR_P));
E void FDECL(read_engr_at, (int,int));
E void FDECL(make_engr_at, (int,int,const char *,long,XCHAR_P));
E void FDECL(del_engr_at, (int,int));
E int freehand(void);
E int doengrave(void);
E void FDECL(save_engravings, (int,int));
E void FDECL(rest_engravings, (int));
E void FDECL(del_engr, (struct engr *));
E void FDECL(rloc_engr, (struct engr *));
E void FDECL(make_grave, (int,int,const char *));

/* ### exper.c ### */

E int FDECL(experience, (struct monst *,int));
E void FDECL(more_experienced, (int,int));
E void FDECL(losexp, (const char *));
E void newexplevel(void);
E void FDECL(pluslvl, (BOOLEAN_P));
E long FDECL(rndexp, (BOOLEAN_P));

/* ### explode.c ### */

E void FDECL(explode, (int,int,int,int,CHAR_P,int));
E long FDECL(scatter, (int, int, int, unsigned int, struct obj *));
E void FDECL(splatter_burning_oil, (int, int));

/* ### extralev.c ### */

#ifdef REINCARNATION
E void makeroguerooms(void);
E void FDECL(corr, (int,int));
E void makerogueghost(void);
#endif

/* ### files.c ### */

E char *FDECL(fname_encode, (const char *, CHAR_P, char *, char *, int));
E char *FDECL(fname_decode, (CHAR_P, char *, char *, int));
E const char *FDECL(fqname, (const char *, int, int));
E FILE *FDECL(fopen_datafile, (const char *,const char *,int));
E boolean FDECL(uptodate, (int,const char *));
E void FDECL(store_version, (int));
E void FDECL(set_levelfile_name, (char *,int));
E int FDECL(create_levelfile, (int,char *));
E int FDECL(open_levelfile, (int,char *));
E void FDECL(delete_levelfile, (int));
E void clearlocks(void);
E int FDECL(create_bonesfile, (d_level*,char **, char *));
E void FDECL(commit_bonesfile, (d_level *));
E int FDECL(open_bonesfile, (d_level*,char **));
E int FDECL(delete_bonesfile, (d_level*));
E void set_savefile_name(void);
#ifdef INSURANCE
E void FDECL(save_savefile_name, (int));
#endif
#if defined(WIZARD)
E void set_error_savefile(void);
#endif
E int create_savefile(void);
E int open_savefile(void);
E int delete_savefile(void);
E int restore_saved_game(void);
E boolean FDECL(lock_file, (const char *,int,int));
E void FDECL(unlock_file, (const char *));
#ifdef USER_SOUNDS
E boolean FDECL(can_read_file, (const char *));
#endif
E void FDECL(read_config_file, (const char *));
E void FDECL(check_recordfile, (const char *));
#if defined(WIZARD)
E void read_wizkit(void);
#endif
E void FDECL(paniclog, (const char *, const char *));
E int FDECL(validate_prefix_locations, (char *));
E void FDECL(free_saved_games, (char**));
#ifdef SELF_RECOVER
E boolean recover_savefile(void);
#endif
#ifdef HOLD_LOCKFILE_OPEN
E void really_close(void);
#endif

/* ### fountain.c ### */

E void FDECL(floating_above, (const char *));
E void FDECL(dogushforth, (int));
E void FDECL(dryup, (XCHAR_P,XCHAR_P, BOOLEAN_P));
E void drinkfountain(void);
E void FDECL(dipfountain, (struct obj *));
#ifdef SINKS
E void FDECL(breaksink, (int,int));
E void drinksink(void);
#endif

/* ### hack.c ### */

E boolean FDECL(revive_nasty, (int,int,const char*));
E void FDECL(movobj, (struct obj *,XCHAR_P,XCHAR_P));
E boolean FDECL(may_dig, (XCHAR_P,XCHAR_P));
E boolean FDECL(may_passwall, (XCHAR_P,XCHAR_P));
E boolean FDECL(bad_rock, (struct permonst *,XCHAR_P,XCHAR_P));
E boolean FDECL(invocation_pos, (XCHAR_P,XCHAR_P));
E boolean FDECL(test_move, (int, int, int, int, int));
E void domove(void);
E void invocation_message(void);
E void FDECL(spoteffects, (BOOLEAN_P));
E char *FDECL(in_rooms, (XCHAR_P,XCHAR_P,int));
E boolean FDECL(in_town, (int,int));
E void FDECL(check_special_room, (BOOLEAN_P));
E int dopickup(void);
E void lookaround(void);
E int monster_nearby(void);
E void FDECL(nomul, (int));
E void FDECL(unmul, (const char *));
E void FDECL(losehp, (int,const char *,BOOLEAN_P));
E int weight_cap(void);
E int inv_weight(void);
E int near_capacity(void);
E int FDECL(calc_capacity, (int));
E int max_capacity(void);
E boolean FDECL(check_capacity, (const char *));
E int inv_cnt(void);
#ifdef GOLDOBJ
E long FDECL(money_cnt, (struct obj *));
#endif

/* ### hacklib.c ### */

E boolean FDECL(digit, (CHAR_P));
E boolean FDECL(letter, (CHAR_P));
E char FDECL(highc, (CHAR_P));
E char FDECL(lowc, (CHAR_P));
E char *FDECL(lcase, (char *));
E char *FDECL(upstart, (char *));
E char *FDECL(mungspaces, (char *));
E char *FDECL(eos, (char *));
E char *FDECL(strkitten, (char *,CHAR_P));
E char *FDECL(s_suffix, (const char *));
E char *FDECL(xcrypt, (const char *,char *));
E boolean FDECL(onlyspace, (const char *));
E char *FDECL(tabexpand, (char *));
E char *FDECL(visctrl, (CHAR_P));
E const char *FDECL(ordin, (int));
E char *FDECL(sitoa, (int));
E int FDECL(sgn, (int));
E int FDECL(rounddiv, (long,int));
E int FDECL(dist2, (int,int,int,int));
E int FDECL(distmin, (int,int,int,int));
E boolean FDECL(online2, (int,int,int,int));
E boolean FDECL(pmatch, (const char *,const char *));
#ifndef STRNCMPI
E int FDECL(strncmpi, (const char *,const char *,int));
#endif
#ifndef STRSTRI
E char *FDECL(strstri, (const char *,const char *));
#endif
E boolean FDECL(fuzzymatch, (const char *,const char *,const char *,BOOLEAN_P));
E void setrandom(void);
E int getyear(void);
#if 0
E char *FDECL(yymmdd, (time_t));
#endif
E long FDECL(yyyymmdd, (time_t));
E int phase_of_the_moon(void);
E boolean friday_13th(void);
E int night(void);
E int midnight(void);

/* ### invent.c ### */

E void FDECL(assigninvlet, (struct obj *));
E struct obj *FDECL(merge_choice, (struct obj *,struct obj *));
E int FDECL(merged, (struct obj **,struct obj **));
E void FDECL(addinv_core1, (struct obj *));
E void FDECL(addinv_core2, (struct obj *));
E struct obj *FDECL(addinv, (struct obj *));
E struct obj *FDECL(hold_another_object,
			(struct obj *,const char *,const char *,const char *));
E void FDECL(useupall, (struct obj *));
E void FDECL(useup, (struct obj *));
E void FDECL(consume_obj_charge, (struct obj *,BOOLEAN_P));
E void FDECL(freeinv_core, (struct obj *));
E void FDECL(freeinv, (struct obj *));
E void FDECL(delallobj, (int,int));
E void FDECL(delobj, (struct obj *));
E struct obj *FDECL(sobj_at, (int,int,int));
E struct obj *FDECL(carrying, (int));
E boolean have_lizard(void);
E struct obj *FDECL(o_on, (unsigned int,struct obj *));
E boolean FDECL(obj_here, (struct obj *,int,int));
E boolean wearing_armor(void);
E boolean FDECL(is_worn, (struct obj *));
E struct obj *FDECL(g_at, (int,int));
E struct obj *FDECL(mkgoldobj, (long));
E struct obj *FDECL(getobj, (const char *,const char *));
E int FDECL(ggetobj, (const char *,int (*)(OBJ_P),int,BOOLEAN_P,unsigned *));
E void FDECL(fully_identify_obj, (struct obj *));
E int FDECL(identify, (struct obj *));
E void FDECL(identify_pack, (int));
E int FDECL(askchain, (struct obj **,const char *,int,int (*)(OBJ_P),
			int (*)(OBJ_P),int,const char *));
E void FDECL(prinv, (const char *,struct obj *,long));
E char *FDECL(xprname, (struct obj *,const char *,CHAR_P,BOOLEAN_P,long,long));
E int ddoinv(void);
E char FDECL(display_inventory, (const char *,BOOLEAN_P));
E int FDECL(display_binventory, (int,int,BOOLEAN_P));
E struct obj *FDECL(display_cinventory,(struct obj *));
E struct obj *FDECL(display_minventory,(struct monst *,int,char *));
E int dotypeinv(void);
E const char *FDECL(dfeature_at, (int,int,char *));
E int FDECL(look_here, (int,BOOLEAN_P));
E int dolook(void);
E boolean FDECL(will_feel_cockatrice, (struct obj *,BOOLEAN_P));
E void FDECL(feel_cockatrice, (struct obj *,BOOLEAN_P));
E void FDECL(stackobj, (struct obj *));
E int doprgold(void);
E int doprwep(void);
E int doprarm(void);
E int doprring(void);
E int dopramulet(void);
E int doprtool(void);
E int doprinuse(void);
E void FDECL(useupf, (struct obj *,long));
E char *FDECL(let_to_name, (CHAR_P,BOOLEAN_P));
E void free_invbuf(void);
E void reassign(void);
E int doorganize(void);
E int FDECL(count_unpaid, (struct obj *));
E int FDECL(count_buc, (struct obj *,int));
E void FDECL(carry_obj_effects, (struct obj *));
E const char *FDECL(currency, (long));
E void FDECL(silly_thing, (const char *,struct obj *));

/* ### ioctl.c ### */

#if defined(UNIX)
E void getwindowsz(void);
E void getioctls(void);
E void setioctls(void);
#endif /* UNIX */

/* ### light.c ### */

E void FDECL(new_light_source, (XCHAR_P, XCHAR_P, int, int, void *));
E void FDECL(del_light_source, (int, void *));
E void FDECL(do_light_sources, (char **));
E struct monst *FDECL(find_mid, (unsigned, unsigned));
E void FDECL(save_light_sources, (int, int, int));
E void FDECL(restore_light_sources, (int));
E void FDECL(relink_light_sources, (BOOLEAN_P));
E void FDECL(obj_move_light_source, (struct obj *, struct obj *));
E boolean any_light_source(void);
E void FDECL(snuff_light_source, (int, int));
E boolean FDECL(obj_sheds_light, (struct obj *));
E boolean FDECL(obj_is_burning, (struct obj *));
E void FDECL(obj_split_light_source, (struct obj *, struct obj *));
E void FDECL(obj_merge_light_sources, (struct obj *,struct obj *));
E int FDECL(candle_light_range, (struct obj *));
#ifdef WIZARD
E int wiz_light_sources(void);
#endif

/* ### lock.c ### */

E boolean FDECL(picking_lock, (int *,int *));
E boolean FDECL(picking_at, (int,int));
E void reset_pick(void);
E int FDECL(pick_lock, (struct obj *));
E int doforce(void);
E boolean FDECL(boxlock, (struct obj *,struct obj *));
E boolean FDECL(doorlock, (struct obj *,int,int));
E int doopen(void);
E int doclose(void);

/* ### makemon.c ### */

E boolean FDECL(is_home_elemental, (struct permonst *));
E struct monst *FDECL(clone_mon, (struct monst *,XCHAR_P,XCHAR_P));
E struct monst *FDECL(makemon, (struct permonst *,int,int,int));
E boolean FDECL(create_critters, (int,struct permonst *));
E struct permonst *rndmonst(void);
E void FDECL(reset_rndmonst, (int));
E struct permonst *FDECL(mkclass, (CHAR_P,int));
E int FDECL(adj_lev, (struct permonst *));
E struct permonst *FDECL(grow_up, (struct monst *,struct monst *));
E int FDECL(mongets, (struct monst *,int));
E int FDECL(golemhp, (int));
E boolean FDECL(peace_minded, (struct permonst *));
E void FDECL(set_malign, (struct monst *));
E void FDECL(set_mimic_sym, (struct monst *));
E int FDECL(mbirth_limit, (int));
E void FDECL(mimic_hit_msg, (struct monst *, SHORT_P));
#ifdef GOLDOBJ
E void FDECL(mkmonmoney, (struct monst *, long));
#endif
E void FDECL(bagotricks, (struct obj *));
E boolean FDECL(propagate, (int, BOOLEAN_P,BOOLEAN_P));

/* ### mapglyph.c ### */

E void FDECL(mapglyph, (int, int *, int *, unsigned *, int, int));

/* ### mcastu.c ### */

E int FDECL(castmu, (struct monst *,struct attack *,BOOLEAN_P,BOOLEAN_P));
E int FDECL(buzzmu, (struct monst *,struct attack *));

/* ### mhitm.c ### */

E int FDECL(fightm, (struct monst *));
E int FDECL(mattackm, (struct monst *,struct monst *));
E int FDECL(noattacks, (struct permonst *));
E int FDECL(sleep_monst, (struct monst *,int,int));
E void FDECL(slept_monst, (struct monst *));
E long FDECL(attk_protection, (int));

/* ### mhitu.c ### */

E const char *FDECL(mpoisons_subj, (struct monst *,struct attack *));
E void u_slow_down(void);
E struct monst *cloneu(void);
E void FDECL(expels, (struct monst *,struct permonst *,BOOLEAN_P));
E struct attack *FDECL(getmattk, (struct permonst *,int,int *,struct attack *));
E int FDECL(mattacku, (struct monst *));
E int FDECL(magic_negation, (struct monst *));
E int FDECL(gazemu, (struct monst *,struct attack *));
E void FDECL(mdamageu, (struct monst *,int));
E int FDECL(could_seduce, (struct monst *,struct monst *,struct attack *));
#ifdef SEDUCE
E int FDECL(doseduce, (struct monst *));
#endif

/* ### minion.c ### */

E void FDECL(msummon, (struct monst *));
E void FDECL(summon_minion, (ALIGNTYP_P,BOOLEAN_P));
E int FDECL(demon_talk, (struct monst *));
E long FDECL(bribe, (struct monst *));
E int FDECL(dprince, (ALIGNTYP_P));
E int FDECL(dlord, (ALIGNTYP_P));
E int llord(void);
E int FDECL(ndemon, (ALIGNTYP_P));
E int lminion(void);

/* ### mklev.c ### */

E void sort_rooms(void);
E void FDECL(add_room, (int,int,int,int,BOOLEAN_P,SCHAR_P,BOOLEAN_P));
E void FDECL(add_subroom, (struct mkroom *,int,int,int,int,
			   BOOLEAN_P,SCHAR_P,BOOLEAN_P));
E void makecorridors(void);
E void FDECL(add_door, (int,int,struct mkroom *));
E void mklev(void);
#ifdef SPECIALIZATION
E void FDECL(topologize, (struct mkroom *,BOOLEAN_P));
#else
E void FDECL(topologize, (struct mkroom *));
#endif
E void FDECL(place_branch, (branch *,XCHAR_P,XCHAR_P));
E boolean FDECL(occupied, (XCHAR_P,XCHAR_P));
E int FDECL(okdoor, (XCHAR_P,XCHAR_P));
E void FDECL(dodoor, (int,int,struct mkroom *));
E void FDECL(mktrap, (int,int,struct mkroom *,coord*));
E void FDECL(mkstairs, (XCHAR_P,XCHAR_P,CHAR_P,struct mkroom *));
E void mkinvokearea(void);

/* ### mkmap.c ### */

void FDECL(flood_fill_rm, (int,int,int,BOOLEAN_P,BOOLEAN_P));
void FDECL(remove_rooms, (int,int,int,int));

/* ### mkmaze.c ### */

E void FDECL(wallification, (int,int,int,int));
E void FDECL(walkfrom, (int,int));
E void FDECL(makemaz, (const char *));
E void FDECL(mazexy, (coord *));
E void bound_digging(void);
E void FDECL(mkportal, (XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P));
E boolean FDECL(bad_location, (XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P));
E void FDECL(place_lregion, (XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,
			     XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,
			     XCHAR_P,d_level *));
E void movebubbles(void);
E void water_friction(void);
E void FDECL(save_waterlevel, (int,int));
E void FDECL(restore_waterlevel, (int));
E const char *FDECL(waterbody_name, (XCHAR_P,XCHAR_P));

/* ### mkobj.c ### */

E struct obj *FDECL(mkobj_at, (CHAR_P,int,int,BOOLEAN_P));
E struct obj *FDECL(mksobj_at, (int,int,int,BOOLEAN_P,BOOLEAN_P));
E struct obj *FDECL(mkobj, (CHAR_P,BOOLEAN_P));
E int rndmonnum(void);
E struct obj *FDECL(splitobj, (struct obj *,long));
E void FDECL(replace_object, (struct obj *,struct obj *));
E void FDECL(bill_dummy_object, (struct obj *));
E struct obj *FDECL(mksobj, (int,BOOLEAN_P,BOOLEAN_P));
E int FDECL(bcsign, (struct obj *));
E int FDECL(weight, (struct obj *));
E struct obj *FDECL(mkgold, (long,int,int));
E struct obj *FDECL(mkcorpstat,
		(int,struct monst *,struct permonst *,int,int,BOOLEAN_P));
E struct obj *FDECL(obj_attach_mid, (struct obj *, unsigned));
E struct monst *FDECL(get_mtraits, (struct obj *, BOOLEAN_P));
E struct obj *FDECL(mk_tt_object, (int,int,int));
E struct obj *FDECL(mk_named_object,
			(int,struct permonst *,int,int,const char *));
E struct obj *FDECL(rnd_treefruit_at, (int, int));
E void FDECL(start_corpse_timeout, (struct obj *));
E void FDECL(bless, (struct obj *));
E void FDECL(unbless, (struct obj *));
E void FDECL(curse, (struct obj *));
E void FDECL(uncurse, (struct obj *));
E void FDECL(blessorcurse, (struct obj *,int));
E boolean FDECL(is_flammable, (struct obj *));
E boolean FDECL(is_rottable, (struct obj *));
E void FDECL(place_object, (struct obj *,int,int));
E void FDECL(remove_object, (struct obj *));
E void FDECL(discard_minvent, (struct monst *));
E void FDECL(obj_extract_self, (struct obj *));
E void FDECL(extract_nobj, (struct obj *, struct obj **));
E void FDECL(extract_nexthere, (struct obj *, struct obj **));
E int FDECL(add_to_minv, (struct monst *, struct obj *));
E struct obj *FDECL(add_to_container, (struct obj *, struct obj *));
E void FDECL(add_to_migration, (struct obj *));
E void FDECL(add_to_buried, (struct obj *));
E void FDECL(dealloc_obj, (struct obj *));
E void FDECL(obj_ice_effects, (int, int, BOOLEAN_P));
E long FDECL(peek_at_iced_corpse_age, (struct obj *));
#ifdef WIZARD
E void obj_sanity_check(void);
#endif

/* ### mkroom.c ### */

E void FDECL(mkroom, (int));
E void FDECL(fill_zoo, (struct mkroom *));
E boolean FDECL(nexttodoor, (int,int));
E boolean FDECL(has_dnstairs, (struct mkroom *));
E boolean FDECL(has_upstairs, (struct mkroom *));
E int FDECL(somex, (struct mkroom *));
E int FDECL(somey, (struct mkroom *));
E boolean FDECL(inside_room, (struct mkroom *,XCHAR_P,XCHAR_P));
E boolean FDECL(somexy, (struct mkroom *,coord *));
E void FDECL(mkundead, (coord *,BOOLEAN_P,int));
E struct permonst *courtmon(void);
E void FDECL(save_rooms, (int));
E void FDECL(rest_rooms, (int));
E struct mkroom *FDECL(search_special, (SCHAR_P));

/* ### mon.c ### */

E int FDECL(undead_to_corpse, (int));
E int FDECL(genus, (int,int));
E int FDECL(pm_to_cham, (int));
E int FDECL(minliquid, (struct monst *));
E int movemon(void);
E int FDECL(meatmetal, (struct monst *));
E int FDECL(meatobj, (struct monst *));
E void FDECL(mpickgold, (struct monst *));
E boolean FDECL(mpickstuff, (struct monst *,const char *));
E int FDECL(curr_mon_load, (struct monst *));
E int FDECL(max_mon_load, (struct monst *));
E boolean FDECL(can_carry, (struct monst *,struct obj *));
E int FDECL(mfndpos, (struct monst *,coord *,long *,long));
E boolean FDECL(monnear, (struct monst *,int,int));
E void dmonsfree(void);
E int FDECL(mcalcmove, (struct monst*));
E void mcalcdistress(void);
E void FDECL(replmon, (struct monst *,struct monst *));
E void FDECL(relmon, (struct monst *));
E struct obj *FDECL(mlifesaver, (struct monst *));
E boolean FDECL(corpse_chance,(struct monst *,struct monst *,BOOLEAN_P));
E void FDECL(mondead, (struct monst *));
E void FDECL(mondied, (struct monst *));
E void FDECL(mongone, (struct monst *));
E void FDECL(monstone, (struct monst *));
E void FDECL(monkilled, (struct monst *,const char *,int));
E void FDECL(unstuck, (struct monst *));
E void FDECL(killed, (struct monst *));
E void FDECL(xkilled, (struct monst *,int));
E void FDECL(mon_to_stone, (struct monst*));
E void FDECL(mnexto, (struct monst *));
E boolean FDECL(mnearto, (struct monst *,XCHAR_P,XCHAR_P,BOOLEAN_P));
E void FDECL(poisontell, (int));
E void FDECL(poisoned, (const char *,int,const char *,int));
E void FDECL(m_respond, (struct monst *));
E void FDECL(setmangry, (struct monst *));
E void FDECL(wakeup, (struct monst *));
E void wake_nearby(void);
E void FDECL(wake_nearto, (int,int,int));
E void FDECL(seemimic, (struct monst *));
E void rescham(void);
E void restartcham(void);
E void FDECL(restore_cham, (struct monst *));
E void FDECL(mon_animal_list, (BOOLEAN_P));
E int FDECL(newcham, (struct monst *,struct permonst *,BOOLEAN_P,BOOLEAN_P));
E int FDECL(can_be_hatched, (int));
E int FDECL(egg_type_from_parent, (int,BOOLEAN_P));
E boolean FDECL(dead_species, (int,BOOLEAN_P));
E void kill_genocided_monsters(void);
E void FDECL(golemeffects, (struct monst *,int,int));
E boolean FDECL(angry_guards, (BOOLEAN_P));
E void pacify_guards(void);

/* ### mondata.c ### */

E void FDECL(set_mon_data, (struct monst *,struct permonst *,int));
E struct attack *FDECL(attacktype_fordmg, (struct permonst *,int,int));
E boolean FDECL(attacktype, (struct permonst *,int));
E boolean FDECL(poly_when_stoned, (struct permonst *));
E boolean FDECL(resists_drli, (struct monst *));
E boolean FDECL(resists_magm, (struct monst *));
E boolean FDECL(resists_blnd, (struct monst *));
E boolean FDECL(can_blnd, (struct monst *,struct monst *,UCHAR_P,struct obj *));
E boolean FDECL(ranged_attk, (struct permonst *));
E boolean FDECL(hates_silver, (struct permonst *));
E boolean FDECL(passes_bars, (struct permonst *));
E boolean FDECL(can_track, (struct permonst *));
E boolean FDECL(breakarm, (struct permonst *));
E boolean FDECL(sliparm, (struct permonst *));
E boolean FDECL(sticks, (struct permonst *));
E int FDECL(num_horns, (struct permonst *));
/* E boolean FDECL(canseemon, (struct monst *)); */
E struct attack *FDECL(dmgtype_fromattack, (struct permonst *,int,int));
E boolean FDECL(dmgtype, (struct permonst *,int));
E int FDECL(max_passive_dmg, (struct monst *,struct monst *));
E int FDECL(monsndx, (struct permonst *));
E int FDECL(name_to_mon, (const char *));
E int FDECL(gender, (struct monst *));
E int FDECL(pronoun_gender, (struct monst *));
E boolean FDECL(levl_follower, (struct monst *));
E int FDECL(little_to_big, (int));
E int FDECL(big_to_little, (int));
E const char *FDECL(locomotion, (const struct permonst *,const char *));
E const char *FDECL(stagger, (const struct permonst *,const char *));
E const char *FDECL(on_fire, (struct permonst *,struct attack *));
E const struct permonst *FDECL(raceptr, (struct monst *));

/* ### monmove.c ### */

E boolean FDECL(itsstuck, (struct monst *));
E boolean FDECL(mb_trapped, (struct monst *));
E void FDECL(mon_regen, (struct monst *,BOOLEAN_P));
E int FDECL(dochugw, (struct monst *));
E boolean FDECL(onscary, (int,int,struct monst *));
E void FDECL(monflee, (struct monst *, int, BOOLEAN_P, BOOLEAN_P));
E int FDECL(dochug, (struct monst *));
E int FDECL(m_move, (struct monst *,int));
E boolean FDECL(closed_door, (int,int));
E boolean FDECL(accessible, (int,int));
E void FDECL(set_apparxy, (struct monst *));
E boolean FDECL(can_ooze, (struct monst *));

/* ### monst.c ### */

E void monst_init(void);

/* ### monstr.c ### */

E void monstr_init(void);

/* ### mplayer.c ### */

E struct monst *FDECL(mk_mplayer, (struct permonst *,XCHAR_P,
				   XCHAR_P,BOOLEAN_P));
E void FDECL(create_mplayers, (int,BOOLEAN_P));
E void FDECL(mplayer_talk, (struct monst *));

#if defined(WIN32)

/* ### msdos.c,os2.c,tos.c,winnt.c ### */

E char switchar(void);
E long FDECL(freediskspace, (char *));
E int FDECL(findfirst, (char *));
E int findnext(void);
E long FDECL(filesize, (char *));
E char *foundfile_buffer(void);
E void FDECL(chdrive, (char *));
E void disable_ctrlP(void);
E void enable_ctrlP(void);
E char *FDECL(get_username, (int *));
E void FDECL(nt_regularize, (char *));
E int (*nt_kbhit)(void);
E void FDECL(Delay, (int));
#endif /* WIN32 */

/* ### mthrowu.c ### */

E int FDECL(thitu, (int,int,struct obj *,const char *));
E int FDECL(ohitmon, (struct monst *,struct obj *,int,BOOLEAN_P));
E void FDECL(thrwmu, (struct monst *));
E int FDECL(spitmu, (struct monst *,struct attack *));
E int FDECL(breamu, (struct monst *,struct attack *));
E boolean FDECL(linedup, (XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P));
E boolean FDECL(lined_up, (struct monst *));
E struct obj *FDECL(m_carrying, (struct monst *,int));
E void FDECL(m_useup, (struct monst *,struct obj *));
E void FDECL(m_throw, (struct monst *,int,int,int,int,int,struct obj *));
E boolean FDECL(hits_bars, (struct obj **,int,int,int,int));

/* ### muse.c ### */

E boolean FDECL(find_defensive, (struct monst *));
E int FDECL(use_defensive, (struct monst *));
E int FDECL(rnd_defensive_item, (struct monst *));
E boolean FDECL(find_offensive, (struct monst *));
E int FDECL(use_offensive, (struct monst *));
E int FDECL(rnd_offensive_item, (struct monst *));
E boolean FDECL(find_misc, (struct monst *));
E int FDECL(use_misc, (struct monst *));
E int FDECL(rnd_misc_item, (struct monst *));
E boolean FDECL(searches_for_item, (struct monst *,struct obj *));
E boolean FDECL(mon_reflects, (struct monst *,const char *));
E boolean FDECL(ureflects, (const char *,const char *));
E boolean FDECL(munstone, (struct monst *,BOOLEAN_P));

/* ### music.c ### */

E void awaken_soldiers(void);
E int FDECL(do_play_instrument, (struct obj *));

/* ### nhlan.c ### */
#ifdef LAN_FEATURES
E void init_lan_features(void);
E char *lan_username(void);
#endif

/* ### nttty.c ### */

#ifdef WIN32CON
E void get_scr_size(void);
E int nttty_kbhit(void);
E void nttty_open(void);
E void nttty_rubout(void);
E int tgetch(void);
E int FDECL(ntposkey,(int *, int *, int *));
E void FDECL(set_output_mode, (int));
E void synch_cursor(void);
#endif

/* ### o_init.c ### */

E void init_objects(void);
E int find_skates(void);
E void oinit(void);
E void FDECL(savenames, (int,int));
E void FDECL(restnames, (int));
E void FDECL(discover_object, (int,BOOLEAN_P,BOOLEAN_P));
E void FDECL(undiscover_object, (int));
E int dodiscovered(void);

/* ### objects.c ### */

E void objects_init(void);

/* ### objnam.c ### */

E char *FDECL(obj_typename, (int));
E char *FDECL(simple_typename, (int));
E boolean FDECL(obj_is_pname, (struct obj *));
E char *FDECL(distant_name, (struct obj *,char *(*)(OBJ_P)));
E char *FDECL(fruitname, (BOOLEAN_P));
E char *FDECL(xname, (struct obj *));
E char *FDECL(mshot_xname, (struct obj *));
E boolean FDECL(the_unique_obj, (struct obj *obj));
E char *FDECL(doname, (struct obj *));
E boolean FDECL(not_fully_identified, (struct obj *));
E char *FDECL(corpse_xname, (struct obj *,BOOLEAN_P));
E char *FDECL(cxname, (struct obj *));
E char *FDECL(killer_xname, (struct obj *));
E const char *FDECL(singular, (struct obj *,char *(*)(OBJ_P)));
E char *FDECL(an, (const char *));
E char *FDECL(An, (const char *));
E char *FDECL(The, (const char *));
E char *FDECL(the, (const char *));
E char *FDECL(aobjnam, (struct obj *,const char *));
E char *FDECL(Tobjnam, (struct obj *,const char *));
E char *FDECL(otense, (struct obj *,const char *));
E char *FDECL(vtense, (const char *,const char *));
E char *FDECL(Doname2, (struct obj *));
E char *FDECL(yname, (struct obj *));
E char *FDECL(Yname2, (struct obj *));
E char *FDECL(ysimple_name, (struct obj *));
E char *FDECL(Ysimple_name2, (struct obj *));
E char *FDECL(makeplural, (const char *));
E char *FDECL(makesingular, (const char *));
E struct obj *FDECL(readobjnam, (char *,struct obj *,BOOLEAN_P));
E int FDECL(rnd_class, (int,int));
E const char *FDECL(cloak_simple_name, (struct obj *));
E const char *FDECL(mimic_obj_name, (struct monst *));

/* ### options.c ### */

E boolean FDECL(match_optname, (const char *,const char *,int,BOOLEAN_P));
E void initoptions(void);
E void FDECL(parseoptions, (char *,BOOLEAN_P,BOOLEAN_P));
E int doset(void);
E int dotogglepickup(void);
E void option_help(void);
E void FDECL(next_opt, (winid,const char *));
E int FDECL(fruitadd, (char *));
E int FDECL(choose_classes_menu, (const char *,int,BOOLEAN_P,char *,char *));
E void FDECL(add_menu_cmd_alias, (CHAR_P, CHAR_P));
E char FDECL(map_menu_cmd, (CHAR_P));
E void FDECL(assign_warnings, (uchar *));
E char *FDECL(nh_getenv, (const char *));
E void FDECL(set_duplicate_opt_detection, (int));
E void FDECL(set_wc_option_mod_status, (unsigned long, int));
E void FDECL(set_wc2_option_mod_status, (unsigned long, int));
E void FDECL(set_option_mod_status, (const char *,int));
#ifdef AUTOPICKUP_EXCEPTIONS
E int FDECL(add_autopickup_exception, (const char *));
E void free_autopickup_exceptions(void);
#endif /* AUTOPICKUP_EXCEPTIONS */

/* ### pager.c ### */

E int dowhatis(void);
E int doquickwhatis(void);
E int doidtrap(void);
E int dowhatdoes(void);
E char *FDECL(dowhatdoes_core,(CHAR_P, char *));
E int dohelp(void);
E int dohistory(void);

/* ### pcmain.c ### */

#if defined(WIN32)
# ifdef CHDIR
E void FDECL(chdirx, (char *,BOOLEAN_P));
# endif /* CHDIR */
#endif /* WIN32 */

/* ### pcsys.c ### */

#if defined(WIN32)
E void flushout(void);
E int dosh(void);
E void FDECL(append_slash, (char *));
E void FDECL(getreturn, (const char *));
E void VDECL(msmsg, (const char *,...));
E FILE *FDECL(fopenp, (const char *,const char *));
#endif /* WIN32 */

/* ### pctty.c ### */

#if defined(WIN32)
E void gettty(void);
E void FDECL(settty, (const char *));
E void setftty(void);
E void VDECL(error, (const char *,...));
#if defined(TIMED_DELAY) && defined(_MSC_VER)
E void FDECL(msleep, (unsigned));
#endif
#endif /* WIN32 */

/* ### pcunix.c ### */

#if defined(PC_LOCKING)
E void getlock(void);
#endif

/* ### pickup.c ### */

#ifdef GOLDOBJ
E int FDECL(collect_obj_classes,
	(char *,struct obj *,BOOLEAN_P,boolean FDECL((*),(OBJ_P)), int *));
#else
E int FDECL(collect_obj_classes,
	(char *,struct obj *,BOOLEAN_P,BOOLEAN_P,boolean FDECL((*),(OBJ_P)), int *));
#endif
E void FDECL(add_valid_menu_class, (int));
E boolean FDECL(allow_all, (struct obj *));
E boolean FDECL(allow_category, (struct obj *));
E boolean FDECL(is_worn_by_type, (struct obj *));
E int FDECL(pickup, (int));
E int FDECL(pickup_object, (struct obj *, long, BOOLEAN_P));
E int FDECL(query_category, (const char *, struct obj *, int,
				menu_item **, int));
E int FDECL(query_objlist, (const char *, struct obj *, int,
				menu_item **, int, boolean (*)(OBJ_P)));
E struct obj *FDECL(pick_obj, (struct obj *));
E int encumber_msg(void);
E int doloot(void);
E int FDECL(use_container, (struct obj *,int));
E int FDECL(loot_mon, (struct monst *,int *,boolean *));
E const char *FDECL(safe_qbuf, (const char *,unsigned,
				const char *,const char *,const char *));
E boolean FDECL(is_autopickup_exception, (struct obj *, BOOLEAN_P));

/* ### pline.c ### */

E void VDECL(pline, (const char *,...)) PRINTF_F(1,2);
E void VDECL(Norep, (const char *,...)) PRINTF_F(1,2);
E void free_youbuf(void);
E void VDECL(You, (const char *,...)) PRINTF_F(1,2);
E void VDECL(Your, (const char *,...)) PRINTF_F(1,2);
E void VDECL(You_feel, (const char *,...)) PRINTF_F(1,2);
E void VDECL(You_cant, (const char *,...)) PRINTF_F(1,2);
E void VDECL(You_hear, (const char *,...)) PRINTF_F(1,2);
E void VDECL(pline_The, (const char *,...)) PRINTF_F(1,2);
E void VDECL(There, (const char *,...)) PRINTF_F(1,2);
E void VDECL(verbalize, (const char *,...)) PRINTF_F(1,2);
E void VDECL(raw_printf, (const char *,...)) PRINTF_F(1,2);
E void VDECL(impossible, (const char *,...)) PRINTF_F(1,2);
E const char *FDECL(align_str, (ALIGNTYP_P));
E void FDECL(mstatusline, (struct monst *));
E void ustatusline(void);
E void self_invis_message(void);

/* ### polyself.c ### */

E void set_uasmon(void);
E void change_sex(void);
E void FDECL(polyself, (BOOLEAN_P));
E int FDECL(polymon, (int));
E void rehumanize(void);
E int dobreathe(void);
E int dospit(void);
E int doremove(void);
E int dospinweb(void);
E int dosummon(void);
E int dogaze(void);
E int dohide(void);
E int domindblast(void);
E void FDECL(skinback, (BOOLEAN_P));
E const char *FDECL(mbodypart, (struct monst *,int));
E const char *FDECL(body_part, (int));
E int poly_gender(void);
E void FDECL(ugolemeffects, (int,int));

/* ### potion.c ### */

E void FDECL(set_itimeout, (long *,long));
E void FDECL(incr_itimeout, (long *,int));
E void FDECL(make_confused, (long,BOOLEAN_P));
E void FDECL(make_stunned, (long,BOOLEAN_P));
E void FDECL(make_blinded, (long,BOOLEAN_P));
E void FDECL(make_sick, (long, const char *, BOOLEAN_P,int));
E void FDECL(make_vomiting, (long,BOOLEAN_P));
E boolean FDECL(make_hallucinated, (long,BOOLEAN_P,long));
E int dodrink(void);
E int FDECL(dopotion, (struct obj *));
E int FDECL(peffects, (struct obj *));
E void FDECL(healup, (int,int,BOOLEAN_P,BOOLEAN_P));
E void FDECL(strange_feeling, (struct obj *,const char *));
E void FDECL(potionhit, (struct monst *,struct obj *,BOOLEAN_P));
E void FDECL(potionbreathe, (struct obj *));
E boolean FDECL(get_wet, (struct obj *));
E int dodip(void);
E void FDECL(djinni_from_bottle, (struct obj *));
E struct monst *FDECL(split_mon, (struct monst *,struct monst *));
E const char *bottlename(void);

/* ### pray.c ### */

E int dosacrifice(void);
E boolean FDECL(can_pray, (BOOLEAN_P));
E int dopray(void);
E const char *u_gname(void);
E int doturn(void);
E const char *a_gname(void);
E const char *FDECL(a_gname_at, (XCHAR_P x,XCHAR_P y));
E const char *FDECL(align_gname, (ALIGNTYP_P));
E const char *FDECL(halu_gname, (ALIGNTYP_P));
E const char *FDECL(align_gtitle, (ALIGNTYP_P));
E void FDECL(altar_wrath, (int,int));


/* ### priest.c ### */

E int FDECL(move_special, (struct monst *,BOOLEAN_P,SCHAR_P,BOOLEAN_P,BOOLEAN_P,
			   XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P));
E char FDECL(temple_occupied, (char *));
E int FDECL(pri_move, (struct monst *));
E void FDECL(priestini, (d_level *,struct mkroom *,int,int,BOOLEAN_P));
E char *FDECL(priestname, (struct monst *,char *));
E boolean FDECL(p_coaligned, (struct monst *));
E struct monst *FDECL(findpriest, (CHAR_P));
E void FDECL(intemple, (int));
E void FDECL(priest_talk, (struct monst *));
E struct monst *FDECL(mk_roamer, (struct permonst *,ALIGNTYP_P,
				  XCHAR_P,XCHAR_P,BOOLEAN_P));
E void FDECL(reset_hostility, (struct monst *));
E boolean FDECL(in_your_sanctuary, (struct monst *,XCHAR_P,XCHAR_P));
E void FDECL(ghod_hitsu, (struct monst *));
E void angry_priest(void);
E void clearpriests(void);
E void FDECL(restpriest, (struct monst *,BOOLEAN_P));

/* ### quest.c ### */

E void onquest(void);
E void nemdead(void);
E void artitouch(void);
E boolean ok_to_quest(void);
E void FDECL(leader_speaks, (struct monst *));
E void nemesis_speaks(void);
E void FDECL(quest_chat, (struct monst *));
E void FDECL(quest_talk, (struct monst *));
E void FDECL(quest_stat_check, (struct monst *));
E void FDECL(finish_quest, (struct obj *));

/* ### questpgr.c ### */

E void load_qtlist(void);
E void unload_qtlist(void);
E short FDECL(quest_info, (int));
E const char *ldrname(void);
E boolean FDECL(is_quest_artifact, (struct obj*));
E void FDECL(com_pager, (int));
E void FDECL(qt_pager, (int));
E struct permonst *qt_montype(void);

/* ### random.c ### */

#if defined(RANDOM)
E void FDECL(srandom, (unsigned));
E char *FDECL(initstate, (unsigned,char *,int));
E char *FDECL(setstate, (char *));
E long random(void);
#endif /* RANDOM */

/* ### read.c ### */

E int doread(void);
E boolean FDECL(is_chargeable, (struct obj *));
E void FDECL(recharge, (struct obj *,int));
E void FDECL(forget_objects, (int));
E void FDECL(forget_levels, (int));
E void forget_traps(void);
E void FDECL(forget_map, (int));
E int FDECL(seffects, (struct obj *));
E void FDECL(litroom, (BOOLEAN_P,struct obj *));
E void FDECL(do_genocide, (int));
E void FDECL(punish, (struct obj *));
E void unpunish(void);
E boolean FDECL(cant_create, (int *, BOOLEAN_P));
#ifdef WIZARD
E boolean create_particular(void);
#endif

/* ### rect.c ### */

E void init_rect(void);
E NhRect *FDECL(get_rect, (NhRect *));
E NhRect *rnd_rect(void);
E void FDECL(remove_rect, (NhRect *));
E void FDECL(add_rect, (NhRect *));
E void FDECL(split_rects, (NhRect *,NhRect *));

/* ## region.c ### */
E void clear_regions(void);
E void run_regions(void);
E boolean FDECL(in_out_region, (XCHAR_P,XCHAR_P));
E boolean FDECL(m_in_out_region, (struct monst *,XCHAR_P,XCHAR_P));
E void update_player_regions(void);
E void FDECL(update_monster_region, (struct monst *));
E NhRegion *FDECL(visible_region_at, (XCHAR_P,XCHAR_P));
E void FDECL(show_region, (NhRegion*, XCHAR_P, XCHAR_P));
E void FDECL(save_regions, (int,int));
E void FDECL(rest_regions, (int,BOOLEAN_P));
E NhRegion* FDECL(create_gas_cloud, (XCHAR_P, XCHAR_P, int, int));

/* ### restore.c ### */

E void FDECL(inven_inuse, (BOOLEAN_P));
E int FDECL(dorecover, (int));
E void FDECL(trickery, (char *));
E void FDECL(getlev, (int,int,XCHAR_P,BOOLEAN_P));
E boolean FDECL(lookup_id_mapping, (unsigned, unsigned *));
E void FDECL(mread, (int,void *,unsigned int));

/* ### rip.c ### */

E void FDECL(genl_outrip, (winid,int));

/* ### rnd.c ### */

E int FDECL(rn2, (int));
E int FDECL(rnl, (int));
E int FDECL(rnd, (int));
E int FDECL(d, (int,int));
E int FDECL(rne, (int));
E int FDECL(rnz, (int));

/* ### role.c ### */

E boolean FDECL(validrole, (int));
E boolean FDECL(validrace, (int, int));
E boolean FDECL(validgend, (int, int, int));
E boolean FDECL(validalign, (int, int, int));
E int randrole(void);
E int FDECL(randrace, (int));
E int FDECL(randgend, (int, int));
E int FDECL(randalign, (int, int));
E int FDECL(str2role, (char *));
E int FDECL(str2race, (char *));
E int FDECL(str2gend, (char *));
E int FDECL(str2align, (char *));
E boolean FDECL(ok_role, (int, int, int, int));
E int FDECL(pick_role, (int, int, int, int));
E boolean FDECL(ok_race, (int, int, int, int));
E int FDECL(pick_race, (int, int, int, int));
E boolean FDECL(ok_gend, (int, int, int, int));
E int FDECL(pick_gend, (int, int, int, int));
E boolean FDECL(ok_align, (int, int, int, int));
E int FDECL(pick_align, (int, int, int, int));
E void role_init(void);
E void rigid_role_checks(void);
E void plnamesuffix(void);
E const char *FDECL(Hello, (struct monst *));
E const char *Goodbye(void);
E char *FDECL(build_plselection_prompt, (char *, int, int, int, int, int));
E char *FDECL(root_plselection_prompt, (char *, int, int, int, int, int));

/* ### rumors.c ### */

E char *FDECL(getrumor, (int,char *, BOOLEAN_P));
E void FDECL(outrumor, (int,int));
E void FDECL(outoracle, (BOOLEAN_P, BOOLEAN_P));
E void FDECL(save_oracles, (int,int));
E void FDECL(restore_oracles, (int));
E int FDECL(doconsult, (struct monst *));

/* ### save.c ### */

E int dosave(void);
#if defined(UNIX) || defined(WIN32)
E void FDECL(hangup, (int));
#endif
E int dosave0(void);
#ifdef INSURANCE
E void savestateinlock(void);
#endif
E void FDECL(savelev, (int,XCHAR_P,int));
E void FDECL(bufon, (int));
E void FDECL(bufoff, (int));
E void FDECL(bflush, (int));
E void FDECL(bwrite, (int,void *,unsigned int));
E void FDECL(bclose, (int));
E void FDECL(savefruitchn, (int,int));
E void free_dungeons(void);
E void freedynamicdata(void);

/* ### shk.c ### */

#ifdef GOLDOBJ
E long FDECL(money2mon, (struct monst *, long));
E void FDECL(money2u, (struct monst *, long));
#endif
E char *FDECL(shkname, (struct monst *));
E void FDECL(shkgone, (struct monst *));
E void FDECL(set_residency, (struct monst *,BOOLEAN_P));
E void FDECL(replshk, (struct monst *,struct monst *));
E void FDECL(restshk, (struct monst *,BOOLEAN_P));
E char FDECL(inside_shop, (XCHAR_P,XCHAR_P));
E void FDECL(u_left_shop, (char *,BOOLEAN_P));
E void FDECL(remote_burglary, (XCHAR_P,XCHAR_P));
E void FDECL(u_entered_shop, (char *));
E boolean FDECL(same_price, (struct obj *,struct obj *));
E void shopper_financial_report(void);
E int FDECL(inhishop, (struct monst *));
E struct monst *FDECL(shop_keeper, (CHAR_P));
E boolean FDECL(tended_shop, (struct mkroom *));
E void FDECL(delete_contents, (struct obj *));
E void FDECL(obfree, (struct obj *,struct obj *));
E void FDECL(home_shk, (struct monst *,BOOLEAN_P));
E void FDECL(make_happy_shk, (struct monst *,BOOLEAN_P));
E void FDECL(hot_pursuit, (struct monst *));
E void FDECL(make_angry_shk, (struct monst *,XCHAR_P,XCHAR_P));
E int dopay(void);
E boolean FDECL(paybill, (int));
E void finish_paybill(void);
E struct obj *FDECL(find_oid, (unsigned));
E long FDECL(contained_cost, (struct obj *,struct monst *,long,BOOLEAN_P, BOOLEAN_P));
E long FDECL(contained_gold, (struct obj *));
E void FDECL(picked_container, (struct obj *));
E long FDECL(unpaid_cost, (struct obj *));
E void FDECL(addtobill, (struct obj *,BOOLEAN_P,BOOLEAN_P,BOOLEAN_P));
E void FDECL(splitbill, (struct obj *,struct obj *));
E void FDECL(subfrombill, (struct obj *,struct monst *));
E long FDECL(stolen_value, (struct obj *,XCHAR_P,XCHAR_P,BOOLEAN_P,BOOLEAN_P));
E void FDECL(sellobj_state, (int));
E void FDECL(sellobj, (struct obj *,XCHAR_P,XCHAR_P));
E int FDECL(doinvbill, (int));
E struct monst *FDECL(shkcatch, (struct obj *,XCHAR_P,XCHAR_P));
E void FDECL(add_damage, (XCHAR_P,XCHAR_P,long));
E int FDECL(repair_damage, (struct monst *,struct damage *,BOOLEAN_P));
E int FDECL(shk_move, (struct monst *));
E void FDECL(after_shk_move, (struct monst *));
E boolean FDECL(is_fshk, (struct monst *));
E void FDECL(shopdig, (int));
E void FDECL(pay_for_damage, (const char *,BOOLEAN_P));
E boolean FDECL(costly_spot, (XCHAR_P,XCHAR_P));
E struct obj *FDECL(shop_object, (XCHAR_P,XCHAR_P));
E void FDECL(price_quote, (struct obj *));
E void FDECL(shk_chat, (struct monst *));
E void FDECL(check_unpaid_usage, (struct obj *,BOOLEAN_P));
E void FDECL(check_unpaid, (struct obj *));
E void FDECL(costly_gold, (XCHAR_P,XCHAR_P,long));
E boolean FDECL(block_door, (XCHAR_P,XCHAR_P));
E boolean FDECL(block_entry, (XCHAR_P,XCHAR_P));
E char *FDECL(shk_your, (char *,struct obj *));
E char *FDECL(Shk_Your, (char *,struct obj *));

/* ### shknam.c ### */

E void FDECL(stock_room, (int,struct mkroom *));
E boolean FDECL(saleable, (struct monst *,struct obj *));
E int FDECL(get_shop_item, (int));

/* ### sit.c ### */

E void take_gold(void);
E int dosit(void);
E void rndcurse(void);
E void attrcurse(void);

/* ### sounds.c ### */

E void dosounds(void);
E const char *FDECL(growl_sound, (struct monst *));
E void FDECL(growl, (struct monst *));
E void FDECL(yelp, (struct monst *));
E void FDECL(whimper, (struct monst *));
E void FDECL(beg, (struct monst *));
E int dotalk(void);
#ifdef USER_SOUNDS
E int FDECL(add_sound_mapping, (const char *));
E void FDECL(play_sound_for_message, (const char *));
#endif

/* ### sp_lev.c ### */

E boolean FDECL(check_room, (xchar *,xchar *,xchar *,xchar *,BOOLEAN_P));
E boolean FDECL(create_room, (XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P,
			      XCHAR_P,XCHAR_P,XCHAR_P,XCHAR_P));
E void FDECL(create_secret_door, (struct mkroom *,XCHAR_P));
E boolean FDECL(dig_corridor, (coord *,coord *,BOOLEAN_P,SCHAR_P,SCHAR_P));
E void FDECL(fill_room, (struct mkroom *,BOOLEAN_P));
E boolean FDECL(load_special, (const char *));

/* ### spell.c ### */

E int FDECL(study_book, (struct obj *));
E void FDECL(book_disappears, (struct obj *));
E void FDECL(book_substitution, (struct obj *,struct obj *));
E void age_spells(void);
E int docast(void);
E int FDECL(spell_skilltype, (int));
E int FDECL(spelleffects, (int,BOOLEAN_P));
E void losespells(void);
E int dovspell(void);
E void FDECL(initialspell, (struct obj *));

/* ### steal.c ### */

#ifdef GOLDOBJ
E long FDECL(somegold, (long));
#else
E long somegold(void);
#endif
E void FDECL(stealgold, (struct monst *));
E void FDECL(remove_worn_item, (struct obj *,BOOLEAN_P));
E int FDECL(steal, (struct monst *, char *));
E int FDECL(mpickobj, (struct monst *,struct obj *));
E void FDECL(stealamulet, (struct monst *));
E void FDECL(mdrop_special_objs, (struct monst *));
E void FDECL(relobj, (struct monst *,int,BOOLEAN_P));
#ifdef GOLDOBJ
E struct obj *FDECL(findgold, (struct obj *));
#endif

/* ### steed.c ### */

#ifdef STEED
E void rider_cant_reach(void);
E boolean FDECL(can_saddle, (struct monst *));
E int FDECL(use_saddle, (struct obj *));
E boolean FDECL(can_ride, (struct monst *));
E int doride(void);
E boolean FDECL(mount_steed, (struct monst *, BOOLEAN_P));
E void exercise_steed(void);
E void kick_steed(void);
E void FDECL(dismount_steed, (int));
E void FDECL(place_monster, (struct monst *,int,int));
#endif

/* ### teleport.c ### */

E boolean FDECL(goodpos, (int,int,struct monst *,unsigned));
E boolean FDECL(enexto, (coord *,XCHAR_P,XCHAR_P,struct permonst *));
E boolean FDECL(enexto_core, (coord *,XCHAR_P,XCHAR_P,struct permonst *,unsigned));
E void FDECL(teleds, (int,int,BOOLEAN_P));
E boolean FDECL(safe_teleds, (BOOLEAN_P));
E boolean FDECL(teleport_pet, (struct monst *,BOOLEAN_P));
E void tele(void);
E int dotele(void);
E void level_tele(void);
E void FDECL(domagicportal, (struct trap *));
E void FDECL(tele_trap, (struct trap *));
E void FDECL(level_tele_trap, (struct trap *));
E void FDECL(rloc_to, (struct monst *,int,int));
E boolean FDECL(rloc, (struct monst *, BOOLEAN_P));
E boolean FDECL(tele_restrict, (struct monst *));
E void FDECL(mtele_trap, (struct monst *, struct trap *,int));
E int FDECL(mlevel_tele_trap, (struct monst *, struct trap *,BOOLEAN_P,int));
E void FDECL(rloco, (struct obj *));
E int random_teleport_level(void);
E boolean FDECL(u_teleport_mon, (struct monst *,BOOLEAN_P));

/* ### tile.c ### */
#ifdef USE_TILES
E void FDECL(substitute_tiles, (d_level *));
#endif

/* ### timeout.c ### */

E void burn_away_slime(void);
E void nh_timeout(void);
E void FDECL(fall_asleep, (int, BOOLEAN_P));
E void FDECL(attach_egg_hatch_timeout, (struct obj *));
E void FDECL(attach_fig_transform_timeout, (struct obj *));
E void FDECL(kill_egg, (struct obj *));
E void FDECL(hatch_egg, (void *, long));
E void FDECL(learn_egg_type, (int));
E void FDECL(burn_object, (void *, long));
E void FDECL(begin_burn, (struct obj *, BOOLEAN_P));
E void FDECL(end_burn, (struct obj *, BOOLEAN_P));
E void do_storms(void);
E boolean FDECL(start_timer, (long, SHORT_P, SHORT_P, void *));
E long FDECL(stop_timer, (SHORT_P, void *));
E void run_timers(void);
E void FDECL(obj_move_timers, (struct obj *, struct obj *));
E void FDECL(obj_split_timers, (struct obj *, struct obj *));
E void FDECL(obj_stop_timers, (struct obj *));
E boolean FDECL(obj_is_local, (struct obj *));
E void FDECL(save_timers, (int,int,int));
E void FDECL(restore_timers, (int,int,BOOLEAN_P,long));
E void FDECL(relink_timers, (BOOLEAN_P));
#ifdef WIZARD
E int wiz_timeout_queue(void);
E void timer_sanity_check(void);
#endif

/* ### topten.c ### */

E void FDECL(topten, (int));
E void FDECL(prscore, (int,char **));
E struct obj *FDECL(tt_oname, (struct obj *));

/* ### track.c ### */

E void initrack(void);
E void settrack(void);
E coord *FDECL(gettrack, (int,int));

/* ### trap.c ### */

E boolean FDECL(burnarmor,(struct monst *));
E boolean FDECL(rust_dmg, (struct obj *,const char *,int,BOOLEAN_P,struct monst *));
E void FDECL(grease_protect, (struct obj *,const char *,struct monst *));
E struct trap *FDECL(maketrap, (int,int,int));
E void FDECL(fall_through, (BOOLEAN_P));
E struct monst *FDECL(animate_statue, (struct obj *,XCHAR_P,XCHAR_P,int,int *));
E struct monst *FDECL(activate_statue_trap,
			(struct trap *,XCHAR_P,XCHAR_P,BOOLEAN_P));
E void FDECL(dotrap, (struct trap *, unsigned));
E void FDECL(seetrap, (struct trap *));
E int FDECL(mintrap, (struct monst *));
E void FDECL(instapetrify, (const char *));
E void FDECL(minstapetrify, (struct monst *,BOOLEAN_P));
E void FDECL(selftouch, (const char *));
E void FDECL(mselftouch, (struct monst *,const char *,BOOLEAN_P));
E void float_up(void);
E void FDECL(fill_pit, (int,int));
E int FDECL(float_down, (long, long));
E int FDECL(fire_damage, (struct obj *,BOOLEAN_P,BOOLEAN_P,XCHAR_P,XCHAR_P));
E void FDECL(water_damage, (struct obj *,BOOLEAN_P,BOOLEAN_P));
E boolean drown(void);
E void FDECL(drain_en, (int));
E int dountrap(void);
E int FDECL(untrap, (BOOLEAN_P));
E boolean FDECL(chest_trap, (struct obj *,int,BOOLEAN_P));
E void FDECL(deltrap, (struct trap *));
E boolean FDECL(delfloortrap, (struct trap *));
E struct trap *FDECL(t_at, (int,int));
E void FDECL(b_trapped, (const char *,int));
E boolean unconscious(void);
E boolean lava_effects(void);
E void FDECL(blow_up_landmine, (struct trap *));
E int FDECL(launch_obj,(SHORT_P,int,int,int,int,int));

/* ### u_init.c ### */

E void u_init(void);

/* ### uhitm.c ### */

E void FDECL(hurtmarmor,(struct monst *,int));
E boolean FDECL(attack_checks, (struct monst *,struct obj *));
E void FDECL(check_caitiff, (struct monst *));
E schar FDECL(find_roll_to_hit, (struct monst *));
E boolean FDECL(attack, (struct monst *));
E boolean FDECL(hmon, (struct monst *,struct obj *,int));
E int FDECL(damageum, (struct monst *,struct attack *));
E void FDECL(missum, (struct monst *,struct attack *));
E int FDECL(passive, (struct monst *,BOOLEAN_P,int,UCHAR_P));
E void FDECL(passive_obj, (struct monst *,struct obj *,struct attack *));
E void FDECL(stumble_onto_mimic, (struct monst *));
E int FDECL(flash_hits_mon, (struct monst *,struct obj *));

/* ### unixmain.c ### */

#ifdef UNIX
# ifdef PORT_HELP
E void port_help(void);
# endif
#endif /* UNIX */


/* ### unixtty.c ### */

#if defined(UNIX)
E void gettty(void);
E void FDECL(settty, (const char *));
E void setftty(void);
E void intron(void);
E void introff(void);
E void VDECL(error, (const char *,...)) PRINTF_F(1,2);
#endif /* UNIX */

/* ### unixunix.c ### */

#ifdef UNIX
E void getlock(void);
E void FDECL(regularize, (char *));
# if defined(TIMED_DELAY) && !defined(msleep) && defined(SYSV)
E void FDECL(msleep, (unsigned));
# endif
# if defined(DEF_PAGER)
E int FDECL(child, (int));
# endif
#endif /* UNIX */

/* ### vault.c ### */

E boolean FDECL(grddead, (struct monst *));
E char FDECL(vault_occupied, (char *));
E void invault(void);
E int FDECL(gd_move, (struct monst *));
E void paygd(void);
E long hidden_gold(void);
E boolean gd_sound(void);

/* ### version.c ### */

E char *FDECL(version_string, (char *));
E char *FDECL(getversionstring, (char *));
E int doversion(void);
E int doextversion(void);
E boolean FDECL(check_version, (struct version_info *,
				const char *,BOOLEAN_P));
E unsigned long FDECL(get_feature_notice_ver, (char *));
E unsigned long get_current_feature_ver(void);
#ifdef RUNTIME_PORT_ID
E void FDECL(append_port_id, (char *));
#endif

/* ### vision.c ### */

E void vision_init(void);
E int FDECL(does_block, (int,int,struct rm*));
E void vision_reset(void);
E void FDECL(vision_recalc, (int));
E void FDECL(block_point, (int,int));
E void FDECL(unblock_point, (int,int));
E boolean FDECL(clear_path, (int,int,int,int));
E void FDECL(do_clear_area, (int,int,int,
			     void (*)(int,int,void *),void *));

/* ### weapon.c ### */

E int FDECL(hitval, (struct obj *,struct monst *));
E int FDECL(dmgval, (struct obj *,struct monst *));
E struct obj *FDECL(select_rwep, (struct monst *));
E struct obj *FDECL(select_hwep, (struct monst *));
E void FDECL(possibly_unwield, (struct monst *,BOOLEAN_P));
E int FDECL(mon_wield_item, (struct monst *));
E int abon(void);
E int dbon(void);
E int enhance_weapon_skill(void);
E void FDECL(unrestrict_weapon_skill, (int));
E void FDECL(use_skill, (int,int));
E void FDECL(add_weapon_skill, (int));
E void FDECL(lose_weapon_skill, (int));
E int FDECL(weapon_type, (struct obj *));
E int uwep_skill_type(void);
E int FDECL(weapon_hit_bonus, (struct obj *));
E int FDECL(weapon_dam_bonus, (struct obj *));
E void FDECL(skill_init, (const struct def_skill *));

/* ### were.c ### */

E void FDECL(were_change, (struct monst *));
E void FDECL(new_were, (struct monst *));
E int FDECL(were_summon, (struct permonst *,BOOLEAN_P,int *,char *));
E void you_were(void);
E void FDECL(you_unwere, (BOOLEAN_P));

/* ### wield.c ### */

E void FDECL(setuwep, (struct obj *));
E void FDECL(setuqwep, (struct obj *));
E void FDECL(setuswapwep, (struct obj *));
E int dowield(void);
E int doswapweapon(void);
E int dowieldquiver(void);
E boolean FDECL(wield_tool, (struct obj *,const char *));
E int can_twoweapon(void);
E void drop_uswapwep(void);
E int dotwoweapon(void);
E void uwepgone(void);
E void uswapwepgone(void);
E void uqwepgone(void);
E void untwoweapon(void);
E void FDECL(erode_obj, (struct obj *,BOOLEAN_P,BOOLEAN_P));
E int FDECL(chwepon, (struct obj *,int));
E int FDECL(welded, (struct obj *));
E void FDECL(weldmsg, (struct obj *));
E void FDECL(setmnotwielded, (struct monst *,struct obj *));

/* ### windows.c ### */

E void FDECL(choose_windows, (const char *));
E char FDECL(genl_message_menu, (CHAR_P,int,const char *));
E void FDECL(genl_preference_update, (const char *));

/* ### wizard.c ### */

E void amulet(void);
E int FDECL(mon_has_amulet, (struct monst *));
E int FDECL(mon_has_special, (struct monst *));
E int FDECL(tactics, (struct monst *));
E void aggravate(void);
E void clonewiz(void);
E int pick_nasty(void);
E int FDECL(nasty, (struct monst*));
E void resurrect(void);
E void intervene(void);
E void wizdead(void);
E void FDECL(cuss, (struct monst *));

/* ### worm.c ### */

E int get_wormno(void);
E void FDECL(initworm, (struct monst *,int));
E void FDECL(worm_move, (struct monst *));
E void FDECL(worm_nomove, (struct monst *));
E void FDECL(wormgone, (struct monst *));
E void FDECL(wormhitu, (struct monst *));
E void FDECL(cutworm, (struct monst *,XCHAR_P,XCHAR_P,struct obj *));
E void FDECL(see_wsegs, (struct monst *));
E void FDECL(detect_wsegs, (struct monst *,BOOLEAN_P));
E void FDECL(save_worm, (int,int));
E void FDECL(rest_worm, (int));
E void FDECL(place_wsegs, (struct monst *));
E void FDECL(remove_worm, (struct monst *));
E void FDECL(place_worm_tail_randomly, (struct monst *,XCHAR_P,XCHAR_P));
E int FDECL(count_wsegs, (struct monst *));
E boolean FDECL(worm_known, (struct monst *));

/* ### worn.c ### */

E void FDECL(setworn, (struct obj *,long));
E void FDECL(setnotworn, (struct obj *));
E void FDECL(mon_set_minvis, (struct monst *));
E void FDECL(mon_adjust_speed, (struct monst *,int,struct obj *));
E void FDECL(update_mon_intrinsics,
		(struct monst *,struct obj *,BOOLEAN_P,BOOLEAN_P));
E int FDECL(find_mac, (struct monst *));
E void FDECL(m_dowear, (struct monst *,BOOLEAN_P));
E struct obj *FDECL(which_armor, (struct monst *,long));
E void FDECL(mon_break_armor, (struct monst *,BOOLEAN_P));
E void FDECL(bypass_obj, (struct obj *));
E void clear_bypasses(void);
E int FDECL(racial_exception, (struct monst *, struct obj *));

/* ### write.c ### */

E int FDECL(dowrite, (struct obj *));

/* ### zap.c ### */

E int FDECL(bhitm, (struct monst *,struct obj *));
E void FDECL(probe_monster, (struct monst *));
E boolean FDECL(get_obj_location, (struct obj *,xchar *,xchar *,int));
E boolean FDECL(get_mon_location, (struct monst *,xchar *,xchar *,int));
E struct monst *FDECL(get_container_location, (struct obj *obj, int *, int *));
E struct monst *FDECL(montraits, (struct obj *,coord *));
E struct monst *FDECL(revive, (struct obj *));
E int FDECL(unturn_dead, (struct monst *));
E void FDECL(cancel_item, (struct obj *));
E boolean FDECL(drain_item, (struct obj *));
E struct obj *FDECL(poly_obj, (struct obj *, int));
E boolean FDECL(obj_resists, (struct obj *,int,int));
E boolean FDECL(obj_shudders, (struct obj *));
E void FDECL(do_osshock, (struct obj *));
E int FDECL(bhito, (struct obj *,struct obj *));
E int FDECL(bhitpile, (struct obj *,int (*)(OBJ_P,OBJ_P),int,int));
E int FDECL(zappable, (struct obj *));
E void FDECL(zapnodir, (struct obj *));
E int dozap(void);
E int FDECL(zapyourself, (struct obj *,BOOLEAN_P));
E boolean FDECL(cancel_monst, (struct monst *,struct obj *,
			       BOOLEAN_P,BOOLEAN_P,BOOLEAN_P));
E void FDECL(weffects, (struct obj *));
E int spell_damage_bonus(void);
E const char *FDECL(exclam, (int force));
E void FDECL(hit, (const char *,struct monst *,const char *));
E void FDECL(miss, (const char *,struct monst *));
E struct monst *FDECL(bhit, (int,int,int,int,int (*)(MONST_P,OBJ_P),
			     int (*)(OBJ_P,OBJ_P),struct obj *));
E struct monst *FDECL(boomhit, (int,int));
E int FDECL(burn_floor_paper, (int,int,BOOLEAN_P,BOOLEAN_P));
E void FDECL(buzz, (int,int,XCHAR_P,XCHAR_P,int,int));
E void FDECL(melt_ice, (XCHAR_P,XCHAR_P));
E int FDECL(zap_over_floor, (XCHAR_P,XCHAR_P,int,boolean *));
E void FDECL(fracture_rock, (struct obj *));
E boolean FDECL(break_statue, (struct obj *));
E void FDECL(destroy_item, (int,int));
E int FDECL(destroy_mitem, (struct monst *,int,int));
E int FDECL(resist, (struct monst *,CHAR_P,int,int));
E void makewish(void);

#endif /* !MAKEDEFS_C && !LEV_LEX_C */

#undef E

#endif /* EXTERN_H */
