# util/CMakeLists.txt : build nethack utilities

find_program(FLEX_EXECUTABLE NAMES flex flex.exe )
find_program(BISON_EXECUTABLE NAMES bison bison.exe )

set ( MAKEDEFS_SRC
    makedefs.c
    ${LNH_SRC}/monst.c
    ${LNH_SRC}/objects.c
    )
    
set ( DGN_COMP_SRC
    dgn_main.c
    panic.c
    ${LNH_UTIL_GEN}/dgn_parser.c
    ${LNH_UTIL_GEN}/dgn_scanner.c    
    )
    
set ( LEV_COMP_SRC
    lev_main.c
    panic.c
    ${LNH_SRC}/symclass.c
    ${LNH_SRC}/decl.c
    ${LNH_SRC}/monst.c
    ${LNH_SRC}/objects.c
    ${LNH_UTIL_GEN}/lev_parser.c
    ${LNH_UTIL_GEN}/lev_scanner.c    
    )
set ( MAKEDEFS_HEADERS
    ${LNH_INC_GEN}/date.h
    ${LNH_INC_GEN}/onames.h
    ${LNH_INC_GEN}/pm.h
    ${LNH_INC_GEN}/verinfo.h
    )
set ( DLB_SRC
    dlb_main.c
    ${LNH_SRC}/dlb.c
    )

file(MAKE_DIRECTORY ${LNH_INC_GEN})
file(MAKE_DIRECTORY ${LNH_DAT_GEN})

include_directories (${NetHack4_SOURCE_DIR}/include
                     ${NetHack4_SOURCE_DIR}/libnethack/include
                     ${LNH_INC_GEN}
                     ${LNH_UTIL_GEN} )

# targets
add_executable (makedefs ${MAKEDEFS_SRC})
add_executable (dgn_comp ${DGN_COMP_SRC})
add_executable (lev_comp ${LEV_COMP_SRC})
add_executable (dlb ${DLB_SRC})

get_property(MAKEDEFS_BIN TARGET makedefs PROPERTY LOCATION)


# create the headers named in MAKEDEFS_HEADERS
add_custom_command (OUTPUT ${LNH_INC_GEN}/date.h
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -v ${LNH_INC_GEN}/date.h
                    DEPENDS makedefs)

add_custom_command (OUTPUT ${LNH_INC_GEN}/verinfo.h
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -w ${LNH_INC_GEN}/verinfo.h
                    DEPENDS makedefs)

add_custom_command (OUTPUT ${LNH_INC_GEN}/onames.h
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -o ${LNH_INC_GEN}/onames.h
                    DEPENDS makedefs)
                    
add_custom_command (OUTPUT ${LNH_INC_GEN}/pm.h
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -p ${LNH_INC_GEN}/pm.h
                    DEPENDS makedefs)

# generate parser & scanner for dgn_comp
add_custom_command(OUTPUT ${LNH_UTIL_GEN}/dgn_parser.c
                          ${LNH_UTIL_GEN}/dgn_comp.h
                   COMMAND ${BISON_EXECUTABLE}
                   ARGS -y --defines=${LNH_UTIL_GEN}/dgn_comp.h
                        -o ${LNH_UTIL_GEN}/dgn_parser.c
                        ${LNH_UTIL}/dgn_comp.y
                   DEPENDS ${LNH_UTIL}/dgn_comp.y)

add_custom_command(OUTPUT ${LNH_UTIL_GEN}/dgn_scanner.c
                   COMMAND ${FLEX_EXECUTABLE}
                   ARGS -o${LNH_UTIL_GEN}/dgn_scanner.c
                        ${LNH_UTIL}/dgn_comp.l
                   DEPENDS ${LNH_UTIL}/dgn_comp.l)

# generate parser & scanner for lev_comp
add_custom_command(OUTPUT ${LNH_UTIL_GEN}/lev_parser.c
                          ${LNH_UTIL_GEN}/lev_comp.h
                   COMMAND ${BISON_EXECUTABLE}
                   ARGS -y --defines=${LNH_UTIL_GEN}/lev_comp.h
                        -o ${LNH_UTIL_GEN}/lev_parser.c
                        ${LNH_UTIL}/lev_comp.y
                   DEPENDS ${LNH_UTIL}/lev_comp.y)

add_custom_command(OUTPUT ${LNH_UTIL_GEN}/lev_scanner.c
                   COMMAND ${FLEX_EXECUTABLE}
                   ARGS -o${LNH_UTIL_GEN}/lev_scanner.c
                        ${LNH_UTIL}/lev_comp.l
                   DEPENDS ${LNH_UTIL}/lev_comp.l)

add_custom_target (makedefs_headers DEPENDS ${MAKEDEFS_HEADERS})
add_dependencies (dgn_comp makedefs_headers)
add_dependencies (lev_comp makedefs_headers)

