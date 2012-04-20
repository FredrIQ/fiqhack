# build the network server for libnethack

include (CheckIncludeFiles)

check_include_files(postgresql/libpq-fe.h LIBPQFE_IN_SUBDIR)

find_library(JANSSON_LIB jansson DOC "Library for encoding and decoding JSON data")
find_library(PQ_LIB pq DOC "PostgreSQL client library")

set (NH_SERVER_CONFIG_FILE "/etc/NetHack_server/nhserver.conf"
     CACHE STRING "default config file")
set (NH_SERVER_LOG_FILE "/var/log/nhserver.log"
     CACHE STRING "default log file")
set (NH_SERVER_WORK_DIR "/var/lib/NetHack4/"
     CACHE STRING "default work directory")

set (NH_SERVER_SRC
     src/auth.c
     src/clientcmd.c
     src/clientmain.c
     src/db_pgsql.c
     src/config.c
     src/kill.c
     src/log.c
     src/miscsetup.c
     src/server.c
     src/srvmain.c
     src/winprocs.c
     )

include_directories (${NetHack4_SOURCE_DIR}/include
                     include)

add_definitions(-DDEFAULT_CONFIG_FILE="${NH_SERVER_CONFIG_FILE}")
add_definitions(-DDEFAULT_LOG_FILE="${NH_SERVER_LOG_FILE}")
add_definitions(-DDEFAULT_WORK_DIR="${NH_SERVER_WORK_DIR}")

add_definitions(-DNETHACKDIR="${DATADIR}")
if (LIBPQFE_IN_SUBDIR)
    add_definitions(-DLIBPQFE_IN_SUBDIR)
endif ()

link_directories (${NetHack4_BINARY_DIR}/libnethack/src)
add_executable (nethack_server ${NH_SERVER_SRC} )
target_link_libraries(nethack_server nethack ${JANSSON_LIB} ${PQ_LIB} m z)

add_dependencies (nethack_server libnethack)

install(TARGETS nethack_server
        DESTINATION ${BINDIR})
