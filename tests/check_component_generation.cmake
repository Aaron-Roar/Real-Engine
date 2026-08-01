if(NOT DEFINED GENERATOR OR NOT DEFINED OUTPUT_DIRECTORY)
    message(FATAL_ERROR "GENERATOR and OUTPUT_DIRECTORY are required")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}/first" "${OUTPUT_DIRECTORY}/second")

execute_process(
    COMMAND
        "${GENERATOR}"
        "${OUTPUT_DIRECTORY}/first/game_components.h"
        "${OUTPUT_DIRECTORY}/first/game_components.c"
    RESULT_VARIABLE first_result
)
if(NOT first_result EQUAL 0)
    message(FATAL_ERROR "First component generation failed")
endif()

execute_process(
    COMMAND
        "${GENERATOR}"
        "${OUTPUT_DIRECTORY}/second/game_components.h"
        "${OUTPUT_DIRECTORY}/second/game_components.c"
    RESULT_VARIABLE second_result
)
if(NOT second_result EQUAL 0)
    message(FATAL_ERROR "Second component generation failed")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E compare_files
        "${OUTPUT_DIRECTORY}/first/game_components.h"
        "${OUTPUT_DIRECTORY}/second/game_components.h"
    RESULT_VARIABLE header_result
)
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E compare_files
        "${OUTPUT_DIRECTORY}/first/game_components.c"
        "${OUTPUT_DIRECTORY}/second/game_components.c"
    RESULT_VARIABLE source_result
)

if(NOT header_result EQUAL 0 OR NOT source_result EQUAL 0)
    message(FATAL_ERROR "Generated component output is not deterministic")
endif()
