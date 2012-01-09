# network client library for nitrohack

if (WIN32)
    set (JANSSON_INC_DIR "" CACHE PATH "Location of the jansson JSON headers")
    set (JANSSON_LIB_DIR "" CACHE PATH "Location of the jansson JSON library")
    include_directories(${JANSSON_INC_DIR})
    link_directories(${JANSSON_LIB_DIR})
endif ()


set (LIBNITROHACK_CLIENT_SRC
    src/clientapi.c
    src/connection.c
    src/netcmd.c
    ${NitroHack_SOURCE_DIR}/libnitrohack/src/xmalloc.c
    )

include_directories (${NitroHack_SOURCE_DIR}/include
                     include)

link_directories (${NitroHack_BINARY_DIR}/libnitrohack/src)
add_library(libnitrohack_client SHARED ${LIBNITROHACK_CLIENT_SRC})
set_target_properties(libnitrohack_client PROPERTIES OUTPUT_NAME nitrohack_client)

target_link_libraries(libnitrohack_client nitrohack jansson)
if (WIN32)
    target_link_libraries(libnitrohack_client Ws2_32)
endif ()

add_dependencies (libnitrohack_client libnitrohack)

install(TARGETS libnitrohack_client
        DESTINATION ${BINDIR})