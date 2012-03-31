# libnethack toplevel

#convenient names for inout directories
set (LNH_SRC  ${NitroHack_SOURCE_DIR}/libnitrohack/src)
set (LNH_DAT  ${NitroHack_SOURCE_DIR}/libnitrohack/dat)
set (LNH_UTIL ${NitroHack_SOURCE_DIR}/libnitrohack/util)
set (LNH_INC ${NitroHack_SOURCE_DIR}/libnitrohack/include)

# various generated files live here
set (LNH_SRC_GEN  ${NitroHack_BINARY_DIR}/libnitrohack/src)
set (LNH_DAT_GEN  ${NitroHack_BINARY_DIR}/libnitrohack/dat)
set (LNH_UTIL_GEN ${NitroHack_BINARY_DIR}/libnitrohack/util)
set (LNH_INC_GEN ${NitroHack_BINARY_DIR}/libnitrohack/include)

# actual target definitions etc.
add_subdirectory (util)
add_subdirectory (dat)
add_subdirectory (src)
