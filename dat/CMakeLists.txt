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

# pseudo-target: depend on all other generated data
add_custom_target (nethack_data_files ALL
                   COMMAND lev_comp ${LEVEL_DESCRIPTIONS}
                   COMMAND touch logfile
                   DEPENDS lev_comp data dungeon quest.dat rumors oracles)
