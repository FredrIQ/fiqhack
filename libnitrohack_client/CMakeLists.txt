# network client library for nitrohack

find_library(JANSSON_LIB jansson DOC "Library for encoding and decoding JSON data")


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

target_link_libraries(libnitrohack_client nitrohack ${JANSSON_LIB})

add_dependencies (libnitrohack_client libnitrohack)

install(TARGETS libnitrohack_client
        DESTINATION ${BINDIR})