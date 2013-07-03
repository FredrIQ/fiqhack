# build the curses/uncursed window port for libnethack

set (ENABLE_NETCLIENT TRUE CACHE BOOL "Enable network client mode")

set (NH_CURSES_SRC
    src/main.c
    src/color.c
    src/dialog.c
    src/gameover.c
    src/getline.c
    src/keymap.c
    src/map.c
    src/menu.c
    src/messages.c
    src/options.c
    src/outchars.c
    src/playerselect.c
    src/replay.c
    src/rungame.c
    src/sidebar.c
    src/status.c
    src/topten.c
    src/windows.c)
if (ENABLE_NETCLIENT)
    set (NH_CURSES_SRC
        ${NH_CURSES_SRC}
        src/netgame.c
        src/netplay.c)
endif ()
    
if (WIN32)
    set (NH_CURSES_SRC ${NH_CURSES_SRC} rc/nh_ico.rc)
endif ()

include_directories (${NetHack4_SOURCE_DIR}/include
                     ${NetHack4_SOURCE_DIR}/libuncursed/include
                     include)

add_definitions(-DNETHACKDIR="${DATADIR}")
if (ENABLE_NETCLIENT)
    add_definitions(-DNETCLIENT)
endif ()

link_directories (${NetHack4_BINARY_DIR}/libnethack/src)
link_directories (${NetHack4_BINARY_DIR}/libuncursed)
link_directories (${NetHack4_BINARY_DIR}/libnethack_client)
add_executable (nethack_curses ${NH_CURSES_SRC} )
set_target_properties(nethack_curses PROPERTIES OUTPUT_NAME nethack )
target_link_libraries(nethack_curses nethack uncursed m z)

if (ENABLE_NETCLIENT)
    add_dependencies (nethack_curses libnethack_client)
    target_link_libraries (nethack_curses nethack_client m z)
    # this is a kludge; these libraries are required by libnethack_client, but
    # for static linking they must be specified here. I couldn't figure out how
    # to set the dependencies from the libnethack_client CMakeLists.txt
    if (ALL_STATIC)
	target_link_libraries(nethack_curses jansson)
	if (WIN32)
	    target_link_libraries(nethack_curses Ws2_32)
	endif ()
    endif()
endif ()

add_dependencies (nethack_curses libuncursed)
add_dependencies (nethack_curses libnethack)

if (UNIX)
    add_custom_command(OUTPUT nethack.sh
                       COMMAND printf "#!/bin/sh\\nLD_LIBRARY_PATH=${LIBDIR} exec ${BINDIR}/nethack $@" >> nethack.sh
                       VERBATIM)
    add_custom_target(nethack_curses_shell_script ALL
                      DEPENDS nethack.sh)
    install(PROGRAMS ${NetHack4_BINARY_DIR}/nethack/nethack.sh
            DESTINATION ${SHELLDIR} RENAME nethack)
endif()

install(TARGETS nethack_curses
        DESTINATION ${BINDIR})
