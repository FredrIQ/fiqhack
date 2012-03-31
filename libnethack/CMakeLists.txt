# libnethack toplevel

#convenient names for inout directories
set (LNH_SRC  ${NetHack4_SOURCE_DIR}/libnethack/src)
set (LNH_DAT  ${NetHack4_SOURCE_DIR}/libnethack/dat)
set (LNH_UTIL ${NetHack4_SOURCE_DIR}/libnethack/util)
set (LNH_INC ${NetHack4_SOURCE_DIR}/libnethack/include)

# various generated files live here
set (LNH_SRC_GEN  ${NetHack4_BINARY_DIR}/libnethack/src)
set (LNH_DAT_GEN  ${NetHack4_BINARY_DIR}/libnethack/dat)
set (LNH_UTIL_GEN ${NetHack4_BINARY_DIR}/libnethack/util)
set (LNH_INC_GEN ${NetHack4_BINARY_DIR}/libnethack/include)

# actual target definitions etc.
add_subdirectory (util)
add_subdirectory (dat)
add_subdirectory (src)
