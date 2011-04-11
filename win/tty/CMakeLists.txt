# tty/CMakeLists.txt: build the tty window port for libnethack

set (NH_TTY_SRC
    getline.c 
    termcap.c
    topl.c
    wintty.c
    ioctl.c
    unixtty.c)

if (WIN32)
    # untested and probably incomplete
    set (NH_SYS_SRC
        ${NetHack_SOURCE_DIR}/sys/winnt/winnt.c
        ${NetHack_SOURCE_DIR}/sys/winnt/nttty.c
        )
else()
    set (NH_SYS_SRC
        ${NetHack_SOURCE_DIR}/sys/unix/unixmain.c
        ${NetHack_SOURCE_DIR}/sys/unix/unixunix.c
        ${NetHack_SOURCE_DIR}/sys/unix/unixres.c)
endif()

#include_directories (${NetHack_SOURCE_DIR}/include)
#include_directories (${NetHack_BINARY_DIR}/include)
include_directories (${NetHack_SOURCE_DIR}/include/public)
include_directories (include)

add_definitions (-Wall)
link_directories (${NetHack_BINARY_DIR}/src)
add_executable (nethack_tty ${NH_TTY_SRC} ${NH_SYS_SRC} )
set_target_properties(nethack_tty PROPERTIES OUTPUT_NAME nethack )
target_link_libraries(nethack_tty nethack ncurses)

add_dependencies(nethack_tty nethack)


if (UNIX)
    add_custom_command(OUTPUT nethack.sh
                       COMMAND echo -e "#!/bin/sh\\nLD_LIBRARY_PATH=${BINDIR} HACKDIR=${DATADIR} ${BINDIR}/nethack $@" >> nethack.sh
                       VERBATIM)
    add_custom_target(nethack_shell_script ALL
                      DEPENDS nethack.sh)
endif()
