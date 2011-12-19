# CMakeLists.txt for NitroHack 4.0.0

cmake_minimum_required (VERSION 2.6)
project (NitroHack)

if (WIN32)
    set(INSTALL_BASE "$ENV{USERPROFILE}")
else ()
    set(INSTALL_BASE "$ENV{HOME}")
endif ()

# default installation paths in $HOME/nitrohack
if (NOT BINDIR)
    set (BINDIR "${INSTALL_BASE}/nitrohack/nitrohack-data"
         CACHE PATH "binary location" )
endif()

if (NOT LIBDIR)
    set (LIBDIR "${INSTALL_BASE}/nitrohack/nitrohack-data"
         CACHE PATH "library location" )
endif()

if (NOT DATADIR)
    set (DATADIR "${INSTALL_BASE}/nitrohack/nitrohack-data"
         CACHE PATH "data location" )
endif()

if (NOT SHELLDIR)
    set (SHELLDIR "${INSTALL_BASE}/nitrohack"
         CACHE PATH "launch script location" )
endif()



if (CMAKE_COMPILER_IS_GNUCC)
    set (CMAKE_C_FLAGS_DEBUG "-Wall -g3 -Wold-style-definition -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith")
    if (UNIX)
	add_definitions (-fvisibility=hidden)
    endif ()
endif ()

add_subdirectory (libnitrohack)
add_subdirectory (nitrohack)
