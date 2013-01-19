# CMakeLists.txt for NetHack4 4.2.0

cmake_minimum_required (VERSION 2.6)
project (NetHack4 C)

if (WIN32)
    set(INSTALL_BASE "$ENV{USERPROFILE}")
else ()
    set(INSTALL_BASE "$ENV{HOME}")
endif ()

# default installation paths in $HOME/nethack4
if (NOT BINDIR)
    set (BINDIR "${INSTALL_BASE}/nethack4/nethack4-data"
         CACHE PATH "binary location" )
endif()

if (NOT LIBDIR)
    set (LIBDIR "${INSTALL_BASE}/nethack4/nethack4-data"
         CACHE PATH "library location" )
endif()

if (NOT DATADIR)
    set (DATADIR "${INSTALL_BASE}/nethack4/nethack4-data"
         CACHE PATH "data location" )
endif()

if (NOT SHELLDIR)
    set (SHELLDIR "${INSTALL_BASE}/nethack4"
         CACHE PATH "launch script location" )
endif()

set (ALL_STATIC FALSE CACHE BOOL "Fully static build")
if (NOT ALL_STATIC)
    set (LIB_TYPE SHARED)
else ()
    add_definitions(-fPIC -DSTATIC_BUILD)
    set (LIB_TYPE STATIC)
endif ()

set (WIZARD "wizard" CACHE STRING "The person allowed to use the -D option")
add_definitions (-DWIZARD="${WIZARD}")

set (ENABLE_NETCLIENT TRUE CACHE BOOL "Enable network client mode")

# NetHack4 server currently uses several Linux-specific apis.
# Generalizing the code to the point where it will work on other systems
# is not impossible, but the work has not been done.
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set (ENABLE_SERVER TRUE CACHE BOOL "Build the NetHack4 network server")
else ()
    set (ENABLE_SERVER FALSE)
endif ()



if (CMAKE_COMPILER_IS_GNUCC)
    set (CMAKE_C_FLAGS_DEBUG "-Wall -g3 -Wold-style-definition -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith")
    if (UNIX)
	add_definitions (-fvisibility=hidden)
    endif ()
endif ()

# nethack4 core
add_subdirectory (libnethack)

# nethack4 network client
if (ENABLE_NETCLIENT)
    add_subdirectory (libnethack_client)
endif ()

# nethack4 text ui
add_subdirectory (nethack)

if (ENABLE_SERVER)
    add_subdirectory (nethack_server)
endif ()
