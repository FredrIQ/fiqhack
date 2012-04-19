# build the curses window port for libnethack


if (NOT WIN32)
    set (USE_PDCURSES FALSE CACHE BOOL "Use PDCurses?")
    set (PDCURSES_LIBNAME "XCurses")
else ()
    set (USE_PDCURSES TRUE)
    set (PDCURSES_LIBNAME "pdcurses")
endif ()

if (USE_PDCURSES)
    set (PDCURSES_LIB_DIR "" CACHE PATH "Location of the pdcurses library")
    set (PDCURSES_INC_DIR "" CACHE PATH "Location of the pdcurses header")
    link_directories (${PDCURSES_LIB_DIR})
    include_directories (${PDCURSES_INC_DIR})
    add_definitions (-DPDCURSES)
endif ()

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
                     include)

add_definitions(-DNETHACKDIR="${DATADIR}")
if (ENABLE_NETCLIENT)
    add_definitions(-DNETCLIENT)
endif ()

link_directories (${NetHack4_BINARY_DIR}/libnethack/src)
link_directories (${NetHack4_BINARY_DIR}/libnethack_client)
add_executable (nethack_curses ${NH_CURSES_SRC} )
set_target_properties(nethack_curses PROPERTIES OUTPUT_NAME nethack )
target_link_libraries(nethack_curses nethack m)

if (USE_PDCURSES)
    target_link_libraries (nethack_curses ${PDCURSES_LIBNAME})
else ()
    if (APPLE)
        target_link_libraries (nethack_curses ncurses)
        add_definitions (-DHOMEBREW_CURSES)
    else ()
        target_link_libraries (nethack_curses ncursesw)
    endif ()
endif ()

if (ENABLE_NETCLIENT)
    add_dependencies (nethack_curses libnethack_client)
    target_link_libraries (nethack_curses nethack_client)
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


add_dependencies (nethack_curses libnethack)

if (UNIX)
    add_custom_command(OUTPUT nethack.sh
                       COMMAND printf "#!/bin/sh\\nLD_LIBRARY_PATH=${LIBDIR} ${BINDIR}/nethack $@" >> nethack.sh
                       VERBATIM)
    add_custom_target(nethack_curses_shell_script ALL
                      DEPENDS nethack.sh)
    install(PROGRAMS ${NetHack4_BINARY_DIR}/nethack/nethack.sh
            DESTINATION ${SHELLDIR} RENAME nethack)
endif()

install(TARGETS nethack_curses
        DESTINATION ${BINDIR})
