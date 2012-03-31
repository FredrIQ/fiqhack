# network client library for nethack

if (WIN32)
    set (JANSSON_INC_DIR "" CACHE PATH "Location of the jansson JSON headers")
    set (JANSSON_LIB_DIR "" CACHE PATH "Location of the jansson JSON library")
    include_directories(${JANSSON_INC_DIR})
    link_directories(${JANSSON_LIB_DIR})
endif ()


set (LIBNETHACK_CLIENT_SRC
    src/clientapi.c
    src/connection.c
    src/netcmd.c
    ${NetHack4_SOURCE_DIR}/libnethack/src/xmalloc.c
    )

include_directories (${NetHack4_SOURCE_DIR}/include
                     include)

link_directories (${NetHack4_BINARY_DIR}/libnethack/src)
add_library(libnethack_client ${LIB_TYPE} ${LIBNETHACK_CLIENT_SRC})
set_target_properties(libnethack_client PROPERTIES OUTPUT_NAME nethack_client)

if (NOT ALL_STATIC)
    target_link_libraries(libnethack_client nethack jansson)
    if (WIN32)
	target_link_libraries(libnethack_client Ws2_32)
    endif ()
endif ()

add_dependencies (libnethack_client libnethack)

install(TARGETS libnethack_client
        DESTINATION ${BINDIR})