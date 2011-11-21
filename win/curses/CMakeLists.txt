# curses/CMakeLists.txt: build the curses window port for libnethack

set (NH_CURSES_SRC
    main.c
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
    rungame.c
    sidebar.c
    status.c
    topten.c
    windows.c)

include_directories (${NetHack_SOURCE_DIR}/include/public)
include_directories (include)

add_definitions (-Wall -fvisibility=hidden -g3 -DNETHACKDIR="${DATADIR}" -Wno-unused-but-set-variable)
link_directories (${NetHack_BINARY_DIR}/src)
add_executable (nethack_curses ${NH_CURSES_SRC} )
set_target_properties(nethack_curses PROPERTIES OUTPUT_NAME nethack-curses )
target_link_libraries(nethack_curses nethack ncursesw)

add_dependencies(nethack_curses nethack)


if (UNIX)
    add_custom_command(OUTPUT nethack-curses.sh
                       COMMAND echo -e "#!/bin/sh\\nLD_LIBRARY_PATH=${BINDIR} ${BINDIR}/nethack-curses $@" >> nethack-curses.sh
                       VERBATIM)
    add_custom_target(nethack_curses_shell_script ALL
                      DEPENDS nethack-curses.sh)
endif()
