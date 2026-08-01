set(ROHR_EXAMPLES_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(rohr_add_example_runtime target)
    target_sources(${target} PRIVATE ${ROHR_EXAMPLES_SOURCE_DIR}/shared/example_runtime.c)
    target_include_directories(${target} PRIVATE ${ROHR_EXAMPLES_SOURCE_DIR}/shared)
endfunction()

function(rohr_stage_example_assets target project_dir)
    set(runtime_dir ${CMAKE_BINARY_DIR}/example-projects/${project_dir})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${runtime_dir})
    if(EXISTS ${ROHR_EXAMPLES_SOURCE_DIR}/${project_dir}/assets)
        add_custom_target(${target}_assets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ROHR_EXAMPLES_SOURCE_DIR}/${project_dir}/assets
                ${runtime_dir}/assets
            VERBATIM
        )
        add_dependencies(${target} ${target}_assets)
    endif()
endfunction()

function(rohr_stage_shared_elder_fly target project_dir)
    set(runtime_dir ${CMAKE_BINARY_DIR}/example-projects/${project_dir})
    add_custom_target(${target}_shared_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ROHR_EXAMPLES_SOURCE_DIR}/shared/elder-fly
            ${runtime_dir}/assets/elder-fly
        VERBATIM
    )
    if(TARGET ${target}_assets)
        add_dependencies(${target}_shared_assets ${target}_assets)
    endif()
    add_dependencies(${target} ${target}_shared_assets)
endfunction()

add_subdirectory(engine-core/view-port ${CMAKE_BINARY_DIR}/example-projects/engine-core/view-port/build)
add_subdirectory(engine-core/flies-in-pit ${CMAKE_BINARY_DIR}/example-projects/engine-core/flies-in-pit/build)
add_subdirectory(engine-core/flies-around-ball ${CMAKE_BINARY_DIR}/example-projects/engine-core/flies-around-ball/build)
add_subdirectory(engine-core/fly-to-finish ${CMAKE_BINARY_DIR}/example-projects/engine-core/fly-to-finish/build)
add_subdirectory(engine-core/game-state ${CMAKE_BINARY_DIR}/example-projects/engine-core/game-state/build)
add_subdirectory(engine-core/user-interface ${CMAKE_BINARY_DIR}/example-projects/engine-core/user-interface/build)
add_subdirectory(editor/basic-editor ${CMAKE_BINARY_DIR}/example-projects/editor/basic-editor/build)
add_subdirectory(editor/pong ${CMAKE_BINARY_DIR}/example-projects/editor/pong/build)
