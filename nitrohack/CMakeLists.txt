# curses/CMakeLists.txt: build the curses window port for libnitrohack


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

if (WIN32)
    set (NH_CURSES_SRC ${NH_CURSES_SRC} rc/nh_ico.rc)
endif ()

include_directories (${NitroHack_SOURCE_DIR}/include
                     include)

add_definitions(-DNITROHACKDIR="${DATADIR}")

link_directories (${NitroHack_BINARY_DIR}/libnitrohack/src)
add_executable (nitrohack_curses ${NH_CURSES_SRC} )
set_target_properties(nitrohack_curses PROPERTIES OUTPUT_NAME nitrohack )
target_link_libraries(nitrohack_curses nitrohack)

if (USE_PDCURSES)
    target_link_libraries (nitrohack_curses ${PDCURSES_LIBNAME})
else ()
    target_link_libraries (nitrohack_curses ncursesw )
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # Linux has clock_gettime in librt. FreeBSD doesn't need extra libs: it has
    # clock_gettime in libc. OS X doesn't have the function at all.
    target_link_libraries (nitrohack_curses rt)
endif ()


if (UNIX)
    add_custom_command(OUTPUT nitrohack.sh
                       COMMAND echo -e "#!/bin/sh\\nLD_LIBRARY_PATH=${BINDIR} ${BINDIR}/nitrohack $@" >> nitrohack.sh
                       VERBATIM)
    add_custom_target(nitrohack_curses_shell_script ALL
                      DEPENDS nitrohack.sh)
    install(PROGRAMS ${NitroHack_BINARY_DIR}/nitrohack/nitrohack.sh
            DESTINATION ${SHELLDIR} RENAME nitrohack)
endif()

install(TARGETS nitrohack_curses
        DESTINATION ${LIBDIR})
