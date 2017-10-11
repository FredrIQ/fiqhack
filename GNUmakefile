GAME = fiqhack
DESTDIR =
BINDIR = $(HOME)/fiqhackdir
DATADIR = $(BINDIR)/data
STATEDIR = $(BINDIR)/save

FLEX = flex
BISON = bison

.DELETE_ON_ERROR:
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
.PHONY: clean distclean

.PHONY: all
all: nethack/src/main libnethack/dat/license libnethack/dat/nhdat tilesets/dat/textascii.nh4ct tilesets/dat/textunicode.nh4ct

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(DATADIR) $(DESTDIR)$(STATEDIR)
	install nethack/src/main $(DESTDIR)$(BINDIR)/$(GAME)
	install -m 644 libnethack/dat/license $(DESTDIR)$(DATADIR)/license
	install -m 644 libnethack/dat/nhdat $(DESTDIR)$(DATADIR)/nhdat
	install -m 644 tilesets/dat/textascii.nh4ct $(DESTDIR)$(DATADIR)/textascii.nh4ct
	install -m 644 tilesets/dat/textunicode.nh4ct $(DESTDIR)$(DATADIR)/textunicode.nh4ct

CFLAGS = -g -O2
CXXFLAGS = -g -O2

CFLAGS += --std=c11 -DAIMAKE_NORETURN=_Noreturn

EXTRAS = -pthread -ldl
ifeq ($(shell uname), Linux)
	CPPFLAGS += -DAIMAKE_BUILDOS_linux
	CPPFLAGS += -D_XOPEN_SOURCE=700
	CPPFLAGS += -D_REENTRANT
else
	CPPFLAGS += -DAIMAKE_BUILDOS_freebsd
	EXTRAS =
endif

CPPFLAGS += -DDUMBMAKE
CPPFLAGS += -DGAMESDATADIR=\"$(DATADIR)\"
CPPFLAGS += -DGAMESSTATEDIR=\"$(STATEDIR)\"
CPPFLAGS += -include dumbmake/dumbmake.h

CPPFLAGS += -Ilibnethack/include
CPPFLAGS += -Ilibnethack_common/include
CPPFLAGS += -Inethack/include
CPPFLAGS += -Itilesets/include
CPPFLAGS += -Ilibuncursed/include


### BINARIES ###

# nethack: everything but netgame and netplay
GAME_O = $(addprefix nethack/src/,brandings.o color.o dialog.o extrawin.o gameover.o getline.o keymap.o main.o map.o menu.o messages.o motd.o options.o outchars.o playerselect.o replay.o rungame.o sidebar.o status.o topten.o windows.o)
# libnethack: everything plus readonly
GAME_O += $(addprefix libnethack/src/,allmain.o apply.o artifact.o attrib.o ball.o bones.o botl.o cmd.o dbridge.o decl.o detect.o dig.o display.o dlb.o do.o do_name.o do_wear.o dog.o dogmove.o dokick.o dothrow.o drawing.o dump.o dungeon.o eat.o end.o engrave.o exper.o explode.o extralev.o files.o fountain.o hack.o history.o invent.o level.o light.o livelog.o localtime.o lock.o log.o mail.o makemon.o memfile.o memobj.o messages.o mextra.o mhitm.o mhitq.o mhitu.o minion.o mklev.o mkmap.o mkmaze.o mkobj.o mkroom.o mon.o mondata.o monmove.o monst.o mplayer.o mthrowu.o muse.o music.o newrng.o o_init.o objects.o objnam.o options.o pager.o pickup.o pline.o polyself.o potion.o pray.o priest.o prop.o quest.o questpgr.o read.o readonly.o rect.o region.o restore.o role.o rumors.o save.o shk.o shknam.o sit.o sounds.o sp_lev.o spell.o spoiler.o steal.o steed.o symclass.o teleport.o timeout.o topten.o track.o trap.o u_init.o uhitm.o vault.o version.o vision.o weapon.o were.o wield.o windows.o wizard.o worm.o worn.o write.o zap.o)
# libnethack_common: everything but netconnect
GAME_O += $(addprefix libnethack_common/src/,common_options.o hacklib.o menulist.o trietable.o utf8conv.o xmalloc.o)
GAME_O += tilesets/src/tilesequence.o
# libuncursed with tty
GAME_O += $(addprefix libuncursed/src/,libuncursed.o plugins.o plugins/tty.o plugins/wrap_tty.o)
GAME_O += dumbmake/dumbmake_get_option.o

MAKEDEFS_O = libnethack/util/makedefs.o
MAKEDEFS_O += $(addprefix libnethack/src/,objects.o monst.o)

DGN_COMP_O = $(addprefix libnethack/util/,dgn_main.o dgn_lex.o dgn_yacc.o)

LEV_COMP_O = $(addprefix libnethack/util/,lev_main.o lev_lex.o lev_yacc.o)
LEV_COMP_O += $(addprefix libnethack/src/,monst.o objects.o readonly.o symclass.o)

DLB_O = libnethack/util/dlb_main.o
DLB_O += libnethack/src/dlb.o

TILEC_O = $(addprefix tilesets/util/,tilecompile.o tileset-read.o tileset-write.o)
TILEC_O += $(addprefix tilesets/src/,fallback-tileset-image.o tilesequence.o)
TILEC_O += $(addprefix libnethack/src/,drawing.o monst.o objects.o symclass.o)
TILEC_O += $(addprefix libnethack_common/src/,hacklib.o utf8conv.o xmalloc.o)
TILEC_O += nethack/src/brandings.o

BASECC_O = tilesets/util/basecchar.o
BASECC_O += tilesets/src/tilesequence.o
BASECC_O += $(addprefix libnethack/src/,drawing.o monst.o objects.o symclass.o)
BASECC_O += $(addprefix libnethack_common/src/,hacklib.o xmalloc.o)
BASECC_O += nethack/src/brandings.o

nethack/src/main: $(GAME_O)
	$(CXX) $(LDFLAGS) $^ $(EXTRAS) -lz -o $@
clean:: ; rm -f nethack/src/main $(GAME_O)

libnethack/util/makedefs: $(MAKEDEFS_O)
	$(CC) $(LDFLAGS) $^ -o $@
clean:: ; rm -f libnethack/util/makedefs $(MAKEDEFS_O)

libnethack/util/dgn_comp: $(DGN_COMP_O)
	$(CC) $(LDFLAGS) $^ -o $@
clean:: ; rm -f libnethack/util/dgn_comp $(DGN_COMP_O)

libnethack/util/lev_comp: $(LEV_COMP_O)
	$(CC) $(LDFLAGS) $^ -o $@
clean:: ; rm -f libnethack/util/lev_comp $(LEV_COMP_O)

libnethack/util/dlb: $(DLB_O)
	$(CC) $(LDFLAGS) $^ -o $@
clean:: ; rm -f libnethack/util/dlb $(DLB_O)

tilesets/util/tilecompile: $(TILEC_O)
	$(CC) $(LDFLAGS) $^ -o $@
clean:: ; rm -f tilesets/util/tilecompile $(TILEC_O)

tilesets/util/basecchar: $(BASECC_O)
	$(CC) $(LDFLAGS) $^ -o $@
clean:: ; rm -f tilesets/util/basecchar $(BASECC_O)


ALL_O = $(GAME_O) $(MAKEDEFS_O) $(DGN_COMP_O) $(LEV_COMP_O) $(DLB_O) $(TILEC_O) $(BASECC_O)


##### BASIC RULES AND AUTOMATIC DEPENDENCIES #####

libuncursed/src/plugins/wrap_%.o: libuncursed/src/plugins/%.cxx
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c -o $@ $<

libuncursed/src/plugins/wrap_%.d:
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MM -MP -MG -MT libuncursed/src/plugins/wrap_$*.o -MF libuncursed/src/plugins/wrap_$*.d libuncursed/src/plugins/$*.cxx

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c -o $@ $<

%.d:
	@$(CC) $(CFLAGS) $(CPPFLAGS) -MM -MP -MG -MT $*.o -MF $@ $*.c

ALL_D = $(ALL_O:%.o=%.d)
-include $(ALL_D)
distclean: clean ; rm -f $(ALL_D)


##### AUTOGENERATED SOURCE FILES #####

# autogenerated headers need a blank recipe that calls upon
# the correct full path for the header, and autogenerated
# source files need explicit dependencies for their .o and a
# blank recipe for the matching .d

artinames.h: libnethack/include/artinames.h ;
libnethack/include/artinames.h: libnethack/include/artilist.h
	$(CPP) $(CPPFLAGS) -DARTINAMES_H -o $@ $<
clean:: ; rm -f libnethack/include/artinames.h


onames.h: libnethack/include/onames.h ;
libnethack/include/onames.h: libnethack/util/makedefs
	libnethack/util/makedefs -o $@
clean:: ; rm -f libnethack/include/onames.h

libnethack/src/readonly.c: libnethack/util/makedefs
	libnethack/util/makedefs -m $@
libnethack/src/readonly.o: libnethack/include/onames.h libnethack/include/pm.h
libnethack/src/readonly.d: ;
clean:: ; rm -f libnethack/src/readonly.c

# we want to regenerate date.h whenever we're about to link the game
# essentially, so we depend on GAME_O except for version.o for obvious
# reasons
date.h: libnethack/include/date.h ;
libnethack/include/date.h: libnethack/util/makedefs $(filter-out libnethack/src/version.o,$(GAME_O))
	$< -v $@
clean:: ; rm -f libnethack/include/date.h

verinfo.h: libnethack/include/verinfo.h ;
libnethack/include/verinfo.h: libnethack/util/makedefs
	$< -w $@
clean:: ; rm -f libnethack/include/verinfo.h

pm.h: libnethack/include/pm.h ;
libnethack/include/pm.h: libnethack/util/makedefs
	$< -p $@
clean:: ; rm -f libnethack/include/pm.h


libnethack/util/dgn_lex.c: libnethack/util/dgn_comp.l
	$(FLEX) -o$@ $<
libnethack/util/dgn_lex.o: libnethack/util/dgn_comp.h
libnethack/util/dgn_lex.d: ;
clean:: ; rm -f libnethack/util/dgn_lex.c

dgn_comp.h: libnethack/util/dgn_comp.h ;
libnethack/util/dgn_comp.h: libnethack/util/dgn_yacc.c
libnethack/util/dgn_yacc.c: libnethack/util/dgn_comp.y
	$(BISON) --defines=libnethack/util/dgn_comp.h -o libnethack/util/dgn_yacc.c libnethack/util/dgn_comp.y
libnethack/util/dgn_yacc.o: libnethack/include/verinfo.h
libnethack/util/dgn_yacc.d: ;
clean:: ; rm -f libnethack/util/dgn_comp.h libnethack/util/dgn_yacc.c


libnethack/util/lev_lex.c: libnethack/util/lev_comp.l
	$(FLEX) -o$@ $<
libnethack/util/lev_lex.o: libnethack/include/onames.h libnethack/include/verinfo.h libnethack/include/pm.h libnethack/util/lev_comp.h
libnethack/util/lev_lex.d: ;
clean:: ; rm -f libnethack/util/lev_lex.c

lev_comp.h: libnethack/util/lev_comp.h ;
libnethack/util/lev_comp.h: libnethack/util/lev_yacc.c
libnethack/util/lev_yacc.c: libnethack/util/lev_comp.y
	$(BISON) --defines=libnethack/util/lev_comp.h -o libnethack/util/lev_yacc.c libnethack/util/lev_comp.y
libnethack/util/lev_yacc.o: libnethack/include/onames.h libnethack/include/verinfo.h libnethack/include/pm.h
libnethack/util/lev_yacc.d: ;
clean:: ; rm -f libnethack/util/lev_comp.h libnethack/util/lev_yacc.c


### DATA FILES ###

# $> is every prerequisite but the first
> = $(filter-out $<,$^)

libnethack/dat/data: libnethack/util/makedefs libnethack/dat/data.base
	$< -d $> $@
clean:: ; rm -f libnethack/dat/data

libnethack/dat/dungeon.pdf: libnethack/util/makedefs libnethack/dat/dungeon.def
	$< -e $> $@
clean:: ; rm -f libnethack/dat/dungeon.pdf

libnethack/dat/quest.dat: libnethack/util/makedefs libnethack/dat/quest.txt
	$< -q $> $@
clean:: ; rm -f libnethack/dat/quest.dat

libnethack/dat/rumors: libnethack/util/makedefs libnethack/dat/rumors.tru libnethack/dat/rumors.fal
	$< -r $> $@
clean:: ; rm -f libnethack/dat/rumors

libnethack/dat/oracles: libnethack/util/makedefs libnethack/dat/oracles.txt
	$< -h $> $@
clean:: ; rm -f libnethack/dat/oracles


libnethack/dat/dungeon: libnethack/util/dgn_comp libnethack/dat/dungeon.pdf
	$< < $> > $@
clean:: ; rm -f libnethack/dat/dungeon


DESFILES = Arch.des Barb.des Caveman.des Healer.des Knight.des Monk.des Priest.des Ranger.des Rogue.des Samurai.des Tourist.des Valkyrie.des Wizard.des bigroom.des castle.des endgame.des gehennom.des knox.des medusa.des mines.des oracle.des sokoban.des tower.des yendor.des
TAGFILES = $(DESFILES:%.des=libnethack/dat/%.tag)

$(TAGFILES): libnethack/dat/%.tag: libnethack/util/lev_comp libnethack/dat/%.des
	cd libnethack/dat && ../util/lev_comp $*.des && touch $*.tag
clean:: ; rm -f $(TAGFILES) libnethack/dat/*.lev

DATFILES = data dungeon history oracles quest.dat rumors

libnethack/dat/nhdat: libnethack/util/dlb $(addprefix libnethack/dat/,$(DATFILES)) $(TAGFILES)
	cd libnethack/dat && ../util/dlb cf nhdat $(DATFILES) *.lev
clean:: ; rm -f libnethack/dat/nhdat


tilesets/dat/text/base.txt: tilesets/util/basecchar
	$< -o $@
clean:: ; rm -f tilesets/dat/text/base.txt

tilesets/dat/textascii.nh4ct: tilesets/util/tilecompile $(addprefix tilesets/dat/text/,base.txt ascii_overrides.txt dungeoncolors.txt rogue_overrides.txt)
	$< -W -t nh4ct -n ASCII -z 0 0 -o $@ $>
clean:: ; rm -f tilesets/dat/textascii.nh4ct

tilesets/dat/textunicode.nh4ct: tilesets/util/tilecompile $(addprefix tilesets/dat/text/,base.txt ascii_overrides.txt unicode_overrides.txt dungeoncolors.txt rogue_overrides.txt)
	$< -W -t nh4ct -n Unicode -z 0 0 -o $@ $>
clean:: ; rm -f tilesets/dat/textunicode.nh4ct
