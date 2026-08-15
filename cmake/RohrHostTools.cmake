set(ROHR_HOST_TOOLS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(ROHR_HOST_C_COMPILER "${ROHR_HOST_C_COMPILER}" CACHE FILEPATH
    "C compiler used to build tools that run on the build host")

function(rohr_add_component_generator target source)
    if(CMAKE_CROSSCOMPILING)
        if(NOT ROHR_HOST_C_COMPILER)
            find_program(ROHR_HOST_C_COMPILER
                NAMES cc gcc clang
                NO_CMAKE_FIND_ROOT_PATH
            )
        endif()
        if(NOT ROHR_HOST_C_COMPILER)
            message(FATAL_ERROR
                "Cross-compiling component generators requires a host C compiler. "
                "Set ROHR_HOST_C_COMPILER to a compiler that runs on the build host."
            )
        endif()
        if(CMAKE_HOST_WIN32)
            set(host_suffix .exe)
        else()
            set(host_suffix "")
        endif()
        set(host_directory "${CMAKE_BINARY_DIR}/host-tools")
        set(host_executable "${host_directory}/${target}${host_suffix}")
        add_custom_command(
            OUTPUT "${host_executable}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${host_directory}"
            COMMAND "${ROHR_HOST_C_COMPILER}"
                -std=c99
                "-I${ROHR_HOST_TOOLS_ROOT}/include"
                "${source}"
                "${ROHR_HOST_TOOLS_ROOT}/src/tools/component_codegen.c"
                -o "${host_executable}"
            DEPENDS
                "${source}"
                "${ROHR_HOST_TOOLS_ROOT}/src/tools/component_codegen.c"
                "${ROHR_HOST_TOOLS_ROOT}/include/rohr_tools.h"
            COMMENT "Building ${target} for the host"
            VERBATIM
        )
        add_custom_target(${target} DEPENDS "${host_executable}")
        set(${target}_COMMAND "${host_executable}" PARENT_SCOPE)
    else()
        add_executable(${target} "${source}")
        target_link_libraries(${target} PRIVATE rohr_tools)
        set(${target}_COMMAND "$<TARGET_FILE:${target}>" PARENT_SCOPE)
    endif()
endfunction()
