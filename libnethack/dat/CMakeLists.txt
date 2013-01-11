# dat/CMakeLists.txt: generate game data files
include(CMakeParseArguments)

configure_file(history history COPYONLY)

# makedefs -d
add_custom_command (OUTPUT data
                    COMMAND makedefs
                    ARGS -d ${LNH_DAT}/data.base data
                    MAIN_DEPENDENCY data.base)
# makedefs -e
add_custom_command (OUTPUT dungeon.pdf
                    COMMAND makedefs
                    ARGS -e ${LNH_DAT}/dungeon.def dungeon.pdf
                    MAIN_DEPENDENCY dungeon.def)
# makedefs -q
add_custom_command (OUTPUT quest.dat
                    COMMAND makedefs
                    ARGS -q ${LNH_DAT}/quest.txt quest.dat
                    MAIN_DEPENDENCY quest.txt)
# makedefs -r
add_custom_command (OUTPUT rumors
                    COMMAND makedefs
                    ARGS -r ${LNH_DAT}/rumors.tru
                            ${LNH_DAT}/rumors.fal rumors
                    DEPENDS rumors.tru rumors.fal)
# makedefs -h
add_custom_command (OUTPUT oracles
                    COMMAND makedefs
                    ARGS -h ${LNH_DAT}/oracles.txt oracles
                    MAIN_DEPENDENCY oracles.txt)

add_custom_command (OUTPUT dungeon
                    COMMAND dgn_comp
                    ARGS dungeon.pdf
                    MAIN_DEPENDENCY dungeon.pdf)

# Level files
macro(level_file)
    cmake_parse_arguments(LEVEL_FILE "" "INPUT" "OUTPUTS" ${ARGN})
    set (REAL_OUTPUTS)
    foreach (OUTPUT ${LEVEL_FILE_OUTPUTS})
        set(REAL_OUTPUTS ${REAL_OUTPUTS} ${OUTPUT}.lev)
    endforeach(OUTPUT)
    set (COMPILED_LEVELS ${COMPILED_LEVELS} ${REAL_OUTPUTS})
    add_custom_command (OUTPUT ${REAL_OUTPUTS}
                        COMMAND lev_comp
                        ARGS ${LNH_DAT}/${LEVEL_FILE_INPUT}
                        MAIN_DEPENDENCY ${LEVEL_FILE_INPUT})
endmacro(level_file)

macro(quest_file BASE INPUT)
    level_file(INPUT ${INPUT}
               OUTPUTS ${BASE}-strt ${BASE}-fila ${BASE}-loca ${BASE}-filb
                       ${BASE}-goal)
endmacro(quest_file)

# DoD
level_file(INPUT oracle.des OUTPUTS oracle)
level_file(INPUT bigroom.des OUTPUTS bigrm-1 bigrm-2 bigrm-3 bigrm-4 bigrm-5)
level_file(INPUT medusa.des OUTPUTS medusa-1 medusa-2)
level_file(INPUT castle.des OUTPUTS castle)

# Mines
level_file(INPUT mines.des
           OUTPUTS minefill minend-1 minend-2 minend-3
                   minetn-1 minetn-2 minetn-3 minetn-4
                   minetn-5 minetn-6 minetn-7)

# Sokoban
level_file(INPUT sokoban.des
           OUTPUTS soko1-1 soko1-2 soko2-1 soko2-2
                   soko3-1 soko3-2 soko4-1 soko4-2)

# Quests
quest_file(Arc Arch.des)
quest_file(Bar Barb.des)
quest_file(Cav Caveman.des)
quest_file(Hea Healer.des)
quest_file(Kni Knight.des)
quest_file(Mon Monk.des)
quest_file(Pri Priest.des)
quest_file(Ran Ranger.des)
quest_file(Rog Rogue.des)
quest_file(Sam Samurai.des)
quest_file(Tou Tourist.des)
quest_file(Val Valkyrie.des)
quest_file(Wiz Wizard.des)

# Knox
level_file(INPUT knox.des OUTPUTS knox)

# Gehennom
level_file(INPUT gehennom.des OUTPUTS asmodeus baalz juiblex orcus sanctum valley)
level_file(INPUT yendor.des OUTPUTS fakewiz1 fakewiz2 wizard1 wizard2 wizard3)

# Vlad's
level_file(INPUT tower.des OUTPUTS tower1 tower2 tower3)

# End Game
level_file(INPUT endgame.des OUTPUTS earth air fire water astral)

set (NHDAT_SRC dungeon quest.dat rumors oracles ${COMPILED_LEVELS} history data)

add_custom_command (OUTPUT nhdat
                    COMMAND dlb
                    ARGS cf nhdat ${NHDAT_SRC}
                    DEPENDS ${NHDAT_SRC})

# pseudo-target: depend on all other generated data
add_custom_target (nethack_data_files ALL
                   DEPENDS nhdat)

install(FILES ${LNH_DAT_GEN}/nhdat
        DESTINATION ${DATADIR})
install(FILES license
        DESTINATION ${DATADIR})
