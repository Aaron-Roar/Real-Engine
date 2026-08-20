# Copyright 2026 Aaron Rohrer
# SPDX-License-Identifier: LGPL-3.0-only

foreach(required_variable
        ROHR_SDK_PREFIX
        ROHR_CONSUMER_SOURCE_DIR
        ROHR_CONSUMER_BINARY_DIR
        ROHR_GENERATOR)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT EXISTS "${ROHR_SDK_PREFIX}")
    message(FATAL_ERROR "Installed SDK does not exist: ${ROHR_SDK_PREFIX}")
endif()

foreach(license_file
        COPYING
        COPYING.LESSER
        LICENSE.md
        THIRD_PARTY_LICENSES.md
        SDL3-LICENSE.txt
        SDL3_image-LICENSE.txt
        SDL3_ttf-LICENSE.txt
        JetBrains-Mono-OFL.txt
        Lua-LICENSE
        yyjson-LICENSE
        freetype/LICENSE.TXT
        freetype/FTL.TXT)
    if(NOT EXISTS
            "${ROHR_SDK_PREFIX}/share/licenses/rohr/${license_file}")
        message(FATAL_ERROR
            "Installed SDK license is missing: ${license_file}")
    endif()
endforeach()

file(REMOVE_RECURSE "${ROHR_CONSUMER_BINARY_DIR}")

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${ROHR_CONSUMER_SOURCE_DIR}"
    -B "${ROHR_CONSUMER_BINARY_DIR}"
    -G "${ROHR_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_PREFIX_PATH=${ROHR_SDK_PREFIX}")
if(DEFINED ROHR_GENERATOR_PLATFORM AND NOT "${ROHR_GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND configure_command -A "${ROHR_GENERATOR_PLATFORM}")
endif()
if(DEFINED ROHR_GENERATOR_TOOLSET AND NOT "${ROHR_GENERATOR_TOOLSET}" STREQUAL "")
    list(APPEND configure_command -T "${ROHR_GENERATOR_TOOLSET}")
endif()
if(DEFINED ROHR_TOOLCHAIN_FILE AND NOT "${ROHR_TOOLCHAIN_FILE}" STREQUAL "")
    list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${ROHR_TOOLCHAIN_FILE}")
endif()

execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Installed SDK consumer configuration failed:\n"
        "${configure_output}${configure_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${ROHR_CONSUMER_BINARY_DIR}"
        --config Release --parallel
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Installed SDK consumer build failed:\n${build_output}${build_error}")
endif()
