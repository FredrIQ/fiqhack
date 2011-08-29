# CMakeLists.txt for NetHack post3.4.3

cmake_minimum_required (VERSION 2.6)
project (NetHack)

# default installation paths in $HOME/nethack
if (NOT BINDIR)
    set (BINDIR "$ENV{HOME}/nethack/nethack-data"
         CACHE PATH "binary location" )
endif()

if (NOT LIBDIR)
    set (LIBDIR "$ENV{HOME}/nethack/nethack-data"
         CACHE PATH "library location" )
endif()

if (NOT DATADIR)
    set (DATADIR "$ENV{HOME}/nethack/nethack-data"
         CACHE PATH "data location" )
endif()

if (NOT SHELLDIR)
    set (SHELLDIR "$ENV{HOME}/nethack"
         CACHE PATH "launch script location" )
endif()


message(${DATADIR})


add_subdirectory (util)
add_subdirectory (src)
add_subdirectory (win/tty)
add_subdirectory (dat)


install(DIRECTORY ${NetHack_BINARY_DIR}/dat/
        DESTINATION ${DATADIR}
        FILES_MATCHING PATTERN "*.lev"
        PATTERN "save"
        PATTERN "CMakeFiles" EXCLUDE)
install(FILES ${NetHack_BINARY_DIR}/dat/dungeon ${NetHack_BINARY_DIR}/dat/options
              ${NetHack_BINARY_DIR}/dat/quest.dat ${NetHack_BINARY_DIR}/dat/rumors
              ${NetHack_BINARY_DIR}/dat/data ${NetHack_BINARY_DIR}/dat/record
              ${NetHack_BINARY_DIR}/dat/logfile ${NetHack_BINARY_DIR}/dat/oracles
        DESTINATION ${DATADIR})
install(FILES ${NetHack_SOURCE_DIR}/dat/license ${NetHack_SOURCE_DIR}/dat/help
              ${NetHack_SOURCE_DIR}/dat/hh ${NetHack_SOURCE_DIR}/dat/cmdhelp
              ${NetHack_SOURCE_DIR}/dat/history ${NetHack_SOURCE_DIR}/dat/opthelp
              ${NetHack_SOURCE_DIR}/dat/wizhelp
        DESTINATION ${DATADIR})
install(FILES ${NetHack_BINARY_DIR}/src/libnethack.so
        DESTINATION ${LIBDIR})
install(PROGRAMS ${NetHack_BINARY_DIR}/win/tty/nethack
        DESTINATION ${BINDIR})
        
if (UNIX)
    install(PROGRAMS ${NetHack_BINARY_DIR}/win/tty/nethack.sh
            DESTINATION ${SHELLDIR} RENAME nethack)
endif()

