# CMakeLists.txt for NetHack 4.0.0

cmake_minimum_required (VERSION 2.6)
project (NetHack)

set ( CMAKE_BUILD_TYPE Debug )

if (WIN32)
    set(INSTALL_BASE "$ENV{USERPROFILE}")
else ()
    set(INSTALL_BASE "$ENV{HOME}")
endif ()

# default installation paths in $HOME/nethack
if (NOT BINDIR)
    set (BINDIR "${INSTALL_BASE}/nethack/nethack-data"
         CACHE PATH "binary location" )
endif()

if (NOT LIBDIR)
    set (LIBDIR "${INSTALL_BASE}/nethack/nethack-data"
         CACHE PATH "library location" )
endif()

if (NOT DATADIR)
    set (DATADIR "${INSTALL_BASE}/nethack/nethack-data"
         CACHE PATH "data location" )
endif()

if (NOT SHELLDIR)
    set (SHELLDIR "${INSTALL_BASE}/nethack"
         CACHE PATH "launch script location" )
endif()



if (CMAKE_COMPILER_IS_GNUCC)
    set (CMAKE_C_FLAGS_DEBUG "-Wall -g3 -Wold-style-definition -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith")
    if (UNIX)
	add_definitions (-fvisibility=hidden)
    endif ()
endif ()


add_subdirectory (util)
add_subdirectory (src)
add_subdirectory (win/curses)
add_subdirectory (dat)

install(FILES ${NetHack_BINARY_DIR}/dat/nhdat
        DESTINATION ${DATADIR})
install(FILES ${NetHack_SOURCE_DIR}/dat/license
        DESTINATION ${DATADIR})
        
if (UNIX)
    install(PROGRAMS ${NetHack_BINARY_DIR}/win/curses/nethack.sh
            DESTINATION ${SHELLDIR} RENAME nethack)
endif()

