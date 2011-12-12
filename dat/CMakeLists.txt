# dat/CMakeLists.txt: generate game data files

set (LEVEL_DESCRIPTIONS
    ${NetHack_SOURCE_DIR}/dat/Arch.des
    ${NetHack_SOURCE_DIR}/dat/Barb.des
    ${NetHack_SOURCE_DIR}/dat/bigroom.des
    ${NetHack_SOURCE_DIR}/dat/castle.des
    ${NetHack_SOURCE_DIR}/dat/Caveman.des
    ${NetHack_SOURCE_DIR}/dat/endgame.des
    ${NetHack_SOURCE_DIR}/dat/gehennom.des
    ${NetHack_SOURCE_DIR}/dat/Healer.des
    ${NetHack_SOURCE_DIR}/dat/Knight.des
    ${NetHack_SOURCE_DIR}/dat/knox.des
    ${NetHack_SOURCE_DIR}/dat/medusa.des
    ${NetHack_SOURCE_DIR}/dat/mines.des
    ${NetHack_SOURCE_DIR}/dat/Monk.des
    ${NetHack_SOURCE_DIR}/dat/oracle.des
    ${NetHack_SOURCE_DIR}/dat/Priest.des
    ${NetHack_SOURCE_DIR}/dat/Ranger.des
    ${NetHack_SOURCE_DIR}/dat/Rogue.des
    ${NetHack_SOURCE_DIR}/dat/Samurai.des
    ${NetHack_SOURCE_DIR}/dat/sokoban.des
    ${NetHack_SOURCE_DIR}/dat/Tourist.des
    ${NetHack_SOURCE_DIR}/dat/tower.des
    ${NetHack_SOURCE_DIR}/dat/Valkyrie.des
    ${NetHack_SOURCE_DIR}/dat/Wizard.des
    ${NetHack_SOURCE_DIR}/dat/yendor.des 
    )

# the compiled levels that will be added to nhdat
set (COMPILED_LEVELS
    air.lev      Arc-fila.lev Arc-filb.lev Arc-goal.lev
    Arc-loca.lev Arc-strt.lev asmodeus.lev astral.lev
    baalz.lev    Bar-fila.lev Bar-filb.lev Bar-goal.lev
    Bar-loca.lev Bar-strt.lev bigrm-1.lev  bigrm-2.lev
    bigrm-3.lev  bigrm-4.lev  bigrm-5.lev  castle.lev
    Cav-fila.lev Cav-filb.lev Cav-goal.lev Cav-loca.lev
    Cav-strt.lev earth.lev    fakewiz1.lev fakewiz2.lev
    fire.lev     Hea-fila.lev Hea-filb.lev Hea-goal.lev
    Hea-loca.lev Hea-strt.lev juiblex.lev  Kni-fila.lev
    Kni-filb.lev Kni-goal.lev Kni-loca.lev Kni-strt.lev
    knox.lev     medusa-1.lev medusa-2.lev minefill.lev
    minend-1.lev minend-2.lev minend-3.lev minetn-1.lev
    minetn-2.lev minetn-3.lev minetn-4.lev minetn-5.lev
    minetn-6.lev minetn-7.lev Mon-fila.lev Mon-filb.lev
    Mon-goal.lev Mon-loca.lev Mon-strt.lev oracle.lev
    orcus.lev    Pri-fila.lev Pri-filb.lev Pri-goal.lev
    Pri-loca.lev Pri-strt.lev Ran-fila.lev Ran-filb.lev
    Ran-goal.lev Ran-loca.lev Ran-strt.lev Rog-fila.lev
    Rog-filb.lev Rog-goal.lev Rog-loca.lev Rog-strt.lev
    Sam-fila.lev Sam-filb.lev Sam-goal.lev Sam-loca.lev
    Sam-strt.lev sanctum.lev  soko1-1.lev  soko1-2.lev
    soko2-1.lev  soko2-2.lev  soko3-1.lev  soko3-2.lev
    soko4-1.lev  soko4-2.lev  Tou-fila.lev Tou-filb.lev
    Tou-goal.lev Tou-loca.lev Tou-strt.lev tower1.lev
    tower2.lev   tower3.lev   Val-fila.lev Val-filb.lev
    Val-goal.lev valley.lev   Val-loca.lev Val-strt.lev
    water.lev    wizard1.lev  wizard2.lev  wizard3.lev
    Wiz-fila.lev Wiz-filb.lev Wiz-goal.lev Wiz-loca.lev
    Wiz-strt.lev
    )

set ( NHDAT_SRC
    dungeon quest.dat rumors oracles ${COMPILED_LEVELS} history
    )

configure_file(${NetHack_SOURCE_DIR}/dat/history history COPYONLY)

# makedefs -d
add_custom_command (OUTPUT data
                    COMMAND makedefs
                    ARGS -d ${NetHack_SOURCE_DIR}/dat/data.base data
                    DEPENDS makedefs)
# makedefs -e 
add_custom_command (OUTPUT dungeon.pdf
                    COMMAND makedefs
                    ARGS -e ${NetHack_SOURCE_DIR}/dat/dungeon.def dungeon.pdf
                    DEPENDS makedefs)
# makedefs -q
add_custom_command (OUTPUT quest.dat
                    COMMAND makedefs
                    ARGS -q ${NetHack_SOURCE_DIR}/dat/quest.txt quest.dat
                    DEPENDS makedefs)
# makedefs -r
add_custom_command (OUTPUT rumors
                    COMMAND makedefs
                    ARGS -r ${NetHack_SOURCE_DIR}/dat/rumors.tru
                            ${NetHack_SOURCE_DIR}/dat/rumors.fal rumors
                    DEPENDS makedefs)
# makedefs -h
add_custom_command (OUTPUT oracles
                    COMMAND makedefs
                    ARGS -h ${NetHack_SOURCE_DIR}/dat/oracles.txt oracles
                    DEPENDS makedefs)

add_custom_command (OUTPUT dungeon
                    COMMAND dgn_comp
                    ARGS dungeon.pdf
                    DEPENDS dungeon.pdf)

add_custom_target (level_files
                    COMMAND lev_comp ${LEVEL_DESCRIPTIONS}
                    DEPENDS lev_comp)

add_custom_command (OUTPUT nhdat
                    COMMAND dlb
                    ARGS cf nhdat ${NHDAT_SRC}
                    DEPENDS level_files dungeon quest.dat rumors oracles)

# pseudo-target: depend on all other generated data
add_custom_target (nethack_data_files ALL
                   COMMAND touch logfile
                   DEPENDS nhdat)
