set (LIBUNCURSED_SRC src/uncursed.c src/tty.c)

include_directories (include)

if (CMAKE_COMPILER_IS_GNUCC)
    if (UNIX)
	remove_definitions (-fvisibility=hidden)
    endif ()
endif ()

add_library (libuncursed ${LIB_TYPE} ${LIBUNCURSED_SRC})
set_target_properties(libuncursed PROPERTIES OUTPUT_NAME uncursed)

install (TARGETS libuncursed DESTINATION ${LIBDIR})
