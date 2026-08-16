if(NOT DEFINED CLI OR NOT DEFINED MOCK OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "CLI override test received incomplete paths")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
execute_process(
    COMMAND "${CLI}" --project "${TEST_ROOT}" create
    RESULT_VARIABLE create_result
    OUTPUT_VARIABLE create_output
    ERROR_VARIABLE create_error)
if(NOT create_result EQUAL 0)
    message(FATAL_ERROR
        "Could not create CLI override fixture: ${create_output}${create_error}")
endif()

file(TO_CMAKE_PATH "${MOCK}" mock_lua)
file(TO_CMAKE_PATH "${TEST_ROOT}" project_lua)
set(log_path "${TEST_ROOT}/mock-build.log")
file(TO_CMAKE_PATH "${log_path}" log_lua)
file(WRITE "${TEST_ROOT}/editor.lua"
"return {
    project = {
        configure = { \"project-configure-must-not-run\" },
        compile = { \"project-compile-must-not-run\" },
    },
    cli = {
        configure = {
            \"${mock_lua}\", \"${log_lua}\", \"cli-configure\",
            \"{project}\", \"{build}\"
        },
        compile = {
            \"${mock_lua}\", \"${log_lua}\", \"cli-compile\", \"{build}\"
        },
    },
}
")

execute_process(
    COMMAND "${CLI}" --project "${TEST_ROOT}" compile
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR
        "CLI override compile failed: ${compile_output}${compile_error}")
endif()

file(READ "${log_path}" log_contents)
set(expected
    "cli-configure|${project_lua}|${project_lua}/build\ncli-compile|${project_lua}/build\n")
if(NOT log_contents STREQUAL expected)
    message(FATAL_ERROR
        "CLI override arguments differ.\nExpected:\n${expected}Actual:\n${log_contents}")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
