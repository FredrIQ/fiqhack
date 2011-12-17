# util/CMakeLists.txt : build nethack utilities

find_program(FLEX_EXECUTABLE NAMES flex flex.exe )
find_program(BISON_EXECUTABLE NAMES bison bison.exe )

set ( MAKEDEFS_SRC
    makedefs.c
    ${NetHack_SOURCE_DIR}/src/monst.c
    ${NetHack_SOURCE_DIR}/src/objects.c
    )
    
set ( DGN_COMP_SRC
    dgn_main.c
    panic.c
    ${NetHack_BINARY_DIR}/util/dgn_parser.c
    ${NetHack_BINARY_DIR}/util/dgn_scanner.c    
    )
    
set ( LEV_COMP_SRC
    lev_main.c
    panic.c
    ${NetHack_SOURCE_DIR}/src/symclass.c
    ${NetHack_SOURCE_DIR}/src/decl.c
    ${NetHack_SOURCE_DIR}/src/monst.c
    ${NetHack_SOURCE_DIR}/src/objects.c
    ${NetHack_BINARY_DIR}/util/lev_parser.c
    ${NetHack_BINARY_DIR}/util/lev_scanner.c    
    )
set ( MAKEDEFS_HEADERS
    ${NetHack_BINARY_DIR}/include/date.h
    ${NetHack_BINARY_DIR}/include/onames.h
    ${NetHack_BINARY_DIR}/include/pm.h
    )
set ( DLB_SRC
    dlb_main.c
    ${NetHack_SOURCE_DIR}/src/dlb.c
    )

file(MAKE_DIRECTORY ${NetHack_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${NetHack_BINARY_DIR}/dat)

include_directories (${NetHack_SOURCE_DIR}/include)
include_directories (${NetHack_SOURCE_DIR}/include/public)
include_directories (${NetHack_BINARY_DIR}/include)
include_directories (${NetHack_BINARY_DIR}/util)

# targets
add_executable (makedefs ${MAKEDEFS_SRC})
add_executable (dgn_comp ${DGN_COMP_SRC})
add_executable (lev_comp ${LEV_COMP_SRC})
add_executable (dlb ${DLB_SRC})

get_property(MAKEDEFS_BIN TARGET makedefs PROPERTY LOCATION)


# create the headers named in MAKEDEFS_HEADERS
add_custom_command (OUTPUT ${NetHack_BINARY_DIR}/include/date.h
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -v ${NetHack_BINARY_DIR}/include/date.h
                    DEPENDS makedefs)
                    
add_custom_command (OUTPUT ${NetHack_BINARY_DIR}/include/onames.h
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -o ${NetHack_BINARY_DIR}/include/onames.h
                    DEPENDS makedefs)
                    
add_custom_command (OUTPUT ${NetHack_BINARY_DIR}/include/pm.h
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -p ${NetHack_BINARY_DIR}/include/pm.h
                    DEPENDS makedefs)

# generate parser & scanner for dgn_comp
add_custom_command(OUTPUT ${NetHack_BINARY_DIR}/util/dgn_parser.c
                          ${NetHack_BINARY_DIR}/util/dgn_comp.h
                   COMMAND ${BISON_EXECUTABLE}
                   ARGS -y --defines=${NetHack_BINARY_DIR}/util/dgn_comp.h
                        -o ${NetHack_BINARY_DIR}/util/dgn_parser.c
                        ${NetHack_SOURCE_DIR}/util/dgn_comp.y
                   DEPENDS ${NetHack_SOURCE_DIR}/util/dgn_comp.y)

add_custom_command(OUTPUT ${NetHack_BINARY_DIR}/util/dgn_scanner.c
                   COMMAND ${FLEX_EXECUTABLE}
                   ARGS -o${NetHack_BINARY_DIR}/util/dgn_scanner.c
                        ${NetHack_SOURCE_DIR}/util/dgn_comp.l
                   DEPENDS ${NetHack_SOURCE_DIR}/util/dgn_comp.l)

# generate parser & scanner for lev_comp
add_custom_command(OUTPUT ${NetHack_BINARY_DIR}/util/lev_parser.c
                          ${NetHack_BINARY_DIR}/util/lev_comp.h
                   COMMAND ${BISON_EXECUTABLE}
                   ARGS -y --defines=${NetHack_BINARY_DIR}/util/lev_comp.h
                        -o ${NetHack_BINARY_DIR}/util/lev_parser.c
                        ${NetHack_SOURCE_DIR}/util/lev_comp.y
                   DEPENDS ${NetHack_SOURCE_DIR}/util/lev_comp.y)

add_custom_command(OUTPUT ${NetHack_BINARY_DIR}/util/lev_scanner.c
                   COMMAND ${FLEX_EXECUTABLE}
                   ARGS -o${NetHack_BINARY_DIR}/util/lev_scanner.c
                        ${NetHack_SOURCE_DIR}/util/lev_comp.l
                   DEPENDS ${NetHack_SOURCE_DIR}/util/lev_comp.l)

if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions (-Wall -g3 -Wold-style-definition -Wstrict-prototypes
                     -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith)
    if (UNIX)
	add_definitions (-fvisibility=hidden)
    endif ()
endif ()

add_custom_target (makedefs_headers DEPENDS ${MAKEDEFS_HEADERS})
add_dependencies (dgn_comp makedefs_headers)
add_dependencies (lev_comp makedefs_headers)

