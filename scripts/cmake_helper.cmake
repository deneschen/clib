macro(inc_subdir)
    file(GLOB subdirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt)
    foreach(subdir IN LISTS subdirs)
        string(REGEX MATCH .*/ subd ${subdir})
        add_subdirectory(${subd})
    endforeach()
endmacro()

macro(add_exec)
    set(opts)
    set(one_val_args)
    set(multi_val_args SRCS EXT_CCFLAGS EXT_LDFLAGS EXT_LDLIBS)
    cmake_parse_arguments(ARG "${opts}" "${one_val_args}" "${multi_val_args}" ${ARGN})
    add_executable(${ARG_UNPARSED_ARGUMENTS})
    target_sources(${ARG_UNPARSED_ARGUMENTS}
        PUBLIC
        ${ARG_SRCS})
    target_compile_options(${ARG_UNPARSED_ARGUMENTS}
        PUBLIC
        ${ARG_EXT_CCFLAGS})
    target_link_options(${ARG_UNPARSED_ARGUMENTS}
        PUBLIC
        ${ARG_EXT_LDFLAGS})
    target_link_libraries(${ARG_UNPARSED_ARGUMENTS}
        PUBLIC
        ${ARG_EXT_LDLIBS})
endmacro()

macro(add_lib)
    set(opts SHARED_LIB)
    set(one_val_args)
    set(multi_val_args SRCS EXT_CCFLAGS EXT_LDFLAGS EXT_LDLIBS)
    cmake_parse_arguments(ARG "${opts}" "${one_val_args}" "${multi_val_args}" ${ARGN})

    if(${ARG_SHARED_LIB})
        add_library(${ARG_UNPARSED_ARGUMENTS} SHARED)
    else()
        add_library(${ARG_UNPARSED_ARGUMENTS} STATIC)
    endif()

    target_sources(${ARG_UNPARSED_ARGUMENTS}
        PUBLIC
        ${ARG_SRCS})
    target_compile_options(${ARG_UNPARSED_ARGUMENTS}
        PUBLIC
        ${ARG_EXT_CCFLAGS})
    target_link_options(${ARG_UNPARSED_ARGUMENTS}
        PUBLIC
        ${ARG_EXT_LDFLAGS})
    target_link_libraries(${ARG_UNPARSED_ARGUMENTS}
        PUBLIC
        ${ARG_EXT_LDLIBS})
endmacro()