set(ROHR_TERMINAL_PLATFORM_SOURCE src/terminal_windows.c)
if(UNIX)
    set(ROHR_TERMINAL_PLATFORM_SOURCE src/terminal_linux.c)
endif()

add_library(rohr_terminal STATIC
    src/terminal.c
    ${ROHR_TERMINAL_PLATFORM_SOURCE}
)
target_include_directories(rohr_terminal
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(UNIX AND NOT APPLE)
    target_link_libraries(rohr_terminal PRIVATE util)
endif()

if(ROHR_BUILD_TESTS AND UNIX)
    add_executable(rohr_terminal_test tests/terminal.c)
    target_link_libraries(rohr_terminal_test PRIVATE rohr_terminal)
    set_target_properties(rohr_terminal_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME rohr_terminal COMMAND rohr_terminal_test)
endif()
