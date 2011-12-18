# curses/CMakeLists.txt: build the curses window port for libnethack


if (NOT WIN32)
    set (USE_PDCURSES FALSE CACHE BOOL "Use PDCurses?")
    set (PDCURSES_LIBNAME "XCurses")
else ()
    set (USE_PDCURSES TRUE)
    set (PDCURSES_LIBNAME "pdcurses")
endif ()

if (USE_PDCURSES)
    set (PDCURSES_LIB_DIR "" CACHE STRING "Location of the pdcurses library")
    set (PDCURSES_INC_DIR "" CACHE STRING "Location of the pdcurses header")
    link_directories (${PDCURSES_LIB_DIR})
    include_directories (${PDCURSES_INC_DIR})
    add_definitions (-DPDCURSES)
endif ()


set (NH_CURSES_SRC
    main.c
    color.c
    dialog.c
    gameover.c
    getline.c
    keymap.c
    map.c
    menu.c
    messages.c
    options.c
    outchars.c
    playerselect.c
    replay.c
    rungame.c
    sidebar.c
    status.c
    topten.c
    windows.c)

if (WIN32)
    set (NH_CURSES_SRC ${NH_CURSES_SRC} rc/nh_ico.rc)
endif ()

include_directories (${NetHack_SOURCE_DIR}/include/public)
include_directories (include)

add_definitions(-DNETHACKDIR="${DATADIR}")

link_directories (${NetHack_BINARY_DIR}/src)
add_executable (nethack_curses ${NH_CURSES_SRC} )
set_target_properties(nethack_curses PROPERTIES OUTPUT_NAME nethack )
target_link_libraries(nethack_curses nethack)

if (USE_PDCURSES)
    target_link_libraries (nethack_curses ${PDCURSES_LIBNAME})
else ()
    target_link_libraries (nethack_curses ncursesw )
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # Linux has clock_gettime in librt. FreeBSD doesn't need extra libs: it has
    # clock_gettime in libc. OS X doesn't have the function at all.
    target_link_libraries (nethack_curses rt)
endif ()


if (UNIX)
    add_custom_command(OUTPUT nethack.sh
                       COMMAND echo -e "#!/bin/sh\\nLD_LIBRARY_PATH=${BINDIR} ${BINDIR}/nethack $@" >> nethack.sh
                       VERBATIM)
    add_custom_target(nethack_curses_shell_script ALL
                      DEPENDS nethack.sh)
endif()

install(TARGETS nethack_curses
        DESTINATION ${LIBDIR})
