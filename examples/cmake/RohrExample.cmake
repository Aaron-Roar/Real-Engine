set(ROHR_EXAMPLES_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

function(rohr_example_bootstrap)
    if(NOT TARGET rohr_engine)
        set(ROHR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ROHR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(ROHR_ENABLE_DOCUMENTATION OFF CACHE BOOL "" FORCE)
        add_subdirectory(${ROHR_EXAMPLES_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/rohr-engine)
        set(ROHR_EXAMPLE_STANDALONE ON PARENT_SCOPE)
    endif()
endfunction()

function(rohr_add_example_runtime target)
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/example_runtime.c)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endfunction()

function(rohr_example_runtime_dir output project_dir)
    if(ROHR_EXAMPLE_STANDALONE)
        set(${output} ${CMAKE_BINARY_DIR} PARENT_SCOPE)
    else()
        set(${output} ${CMAKE_BINARY_DIR}/examples PARENT_SCOPE)
    endif()
endfunction()

function(rohr_stage_example_assets target project_dir)
    rohr_example_runtime_dir(runtime_dir ${project_dir})
    get_filename_component(project_name ${project_dir} NAME)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${runtime_dir})
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${runtime_dir}/assets/debug
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ROHR_EXAMPLES_SOURCE_DIR}/user-interface/assets/JetBrainsMono-BoldItalic.ttf
            ${runtime_dir}/assets/debug/JetBrainsMono-BoldItalic.ttf
        VERBATIM
    )
    if(EXISTS ${ROHR_EXAMPLES_SOURCE_DIR}/${project_dir}/assets)
        add_custom_target(${target}_assets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ROHR_EXAMPLES_SOURCE_DIR}/${project_dir}/assets
                ${runtime_dir}/assets/${project_name}
            VERBATIM
        )
        add_dependencies(${target} ${target}_assets)
    endif()
endfunction()
