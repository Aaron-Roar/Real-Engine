cmake_minimum_required(VERSION 3.20)

project(rohr_engine LANGUAGES C)

option(ROHR_BUILD_EXAMPLES "Build Rohr example programs" ON)
option(ROHR_BUILD_TESTS "Build Rohr tests" ON)
option(ROHR_ENABLE_DOCUMENTATION "Enable documentation build targets" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SDL_SHARED OFF CACHE BOOL "Build SDL3 as a shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL3 as a static library" FORCE)
set(SDL_DEPS_SHARED OFF CACHE BOOL "Load SDL platform dependencies dynamically" FORCE)
set(SDL_ALSA_SHARED OFF CACHE BOOL "Load ALSA dynamically" FORCE)
set(SDL_PIPEWIRE_SHARED OFF CACHE BOOL "Load PipeWire dynamically" FORCE)
set(SDL_PULSEAUDIO_SHARED OFF CACHE BOOL "Load PulseAudio dynamically" FORCE)
set(SDL_X11_SHARED OFF CACHE BOOL "Load X11 dynamically" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3 test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "Install SDL3" FORCE)
add_subdirectory(lib/SDL EXCLUDE_FROM_ALL)

set(ROHR_SDL3_TARGET SDL3::SDL3-static)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled libraries statically" FORCE)
set(SDLIMAGE_INSTALL OFF CACHE BOOL "Install SDL3_image" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "Build SDL3_image samples" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "Build SDL3_image tests" FORCE)
set(SDLIMAGE_VENDORED OFF CACHE BOOL "Use SDL_image codec submodules" FORCE)
add_subdirectory(lib/SDL_image EXCLUDE_FROM_ALL)

set(SDLTTF_INSTALL OFF CACHE BOOL "Install SDL3_ttf" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "Build SDL3_ttf samples" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "Build SDL_ttf color emoji support" FORCE)
if(WIN32)
    set(SDLTTF_VENDORED ON CACHE BOOL "Use bundled SDL_ttf dependencies" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "Build SDL_ttf HarfBuzz support" FORCE)
else()
    set(SDLTTF_VENDORED OFF CACHE BOOL "Use bundled SDL_ttf dependencies" FORCE)
    set(SDLTTF_HARFBUZZ ON CACHE BOOL "Build SDL_ttf HarfBuzz support" FORCE)
endif()
add_subdirectory(lib/SDL_ttf EXCLUDE_FROM_ALL)

set(ROHR_SDL3_IMAGE_TARGET SDL3_image::SDL3_image-static)
set(ROHR_SDL3_TTF_TARGET SDL3_ttf::SDL3_ttf-static)

set(ROHR_ENGINE_SOURCES
    src/aabb_tree.c
    src/console.c
    src/contact_manifold.c
    src/contact_constraint.c
    src/constraint_solver.c
    src/joint_constraint.c
    src/engine.c
    src/error.c
    src/entity_components.c
    src/entity_pair_set.c
    src/graphics.c
    src/math2d.c
    src/physics.c
    src/physics_interaction_set.c
    src/platform_process.c
    src/rohr.c
    src/systems.c
    src/tools.c
    src/ui.c
    src/controller.c
    src/game_state.c
    lib/yyjson.c
)

add_library(rohr_engine STATIC ${ROHR_ENGINE_SOURCES})
target_include_directories(rohr_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
)
target_compile_definitions(rohr_engine PUBLIC SDL_MAIN_HANDLED)
target_link_libraries(rohr_engine
    PUBLIC
        ${ROHR_SDL3_TARGET}
        ${ROHR_SDL3_IMAGE_TARGET}
        ${ROHR_SDL3_TTF_TARGET}
)

if(NOT MSVC)
    target_link_libraries(rohr_engine PUBLIC m)
endif()

add_library(rohr_editor STATIC
    src/component_codegen.c
    src/level_editor.c
    src/rohr_editor.c
)
target_include_directories(rohr_editor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(rohr_editor PUBLIC rohr_engine)

if(ROHR_BUILD_EXAMPLES)
    add_subdirectory(examples)

    add_custom_target(core_examples)
    add_dependencies(core_examples
        view_port
        flies_in_pit
        flies_around_ball
        fly_to_finish
        game_state
        user_interface
        joints
        soft_body
    )

    add_custom_target(editor_examples)
    add_dependencies(editor_examples basic_editor pong)

    add_custom_target(examples)
    add_dependencies(examples core_examples editor_examples)
endif()

if(ROHR_BUILD_TESTS)
    include(CTest)

    add_executable(engine_lifecycle_test tests/engine_lifecycle.c)
    target_include_directories(engine_lifecycle_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(engine_lifecycle_test PRIVATE rohr_engine)
    set_target_properties(engine_lifecycle_test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    add_test(NAME engine_lifecycle COMMAND engine_lifecycle_test)

    add_executable(controller_mappings_test tests/controller_mappings.c)
    target_link_libraries(controller_mappings_test PRIVATE rohr_engine)
    set_target_properties(controller_mappings_test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    add_test(NAME controller_mappings COMMAND controller_mappings_test)

    add_executable(cameras_test tests/cameras.c)
    target_link_libraries(cameras_test PRIVATE rohr_engine)
    set_target_properties(cameras_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME cameras COMMAND cameras_test)

    add_executable(timing_test tests/timing.c)
    target_link_libraries(timing_test PRIVATE rohr_engine)
    set_target_properties(timing_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME timing COMMAND timing_test)

    add_executable(joints_test tests/joints.c)
    target_link_libraries(joints_test PRIVATE rohr_engine)
    set_target_properties(joints_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME joints COMMAND joints_test)

    add_executable(soft_body_test tests/soft_body.c)
    target_link_libraries(soft_body_test PRIVATE rohr_engine)
    set_target_properties(soft_body_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME soft_body COMMAND soft_body_test)

    add_executable(collision_sensor_test tests/collision_sensor.c)
    target_include_directories(collision_sensor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(collision_sensor_test PRIVATE rohr_engine)
    set_target_properties(collision_sensor_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME collision_sensor COMMAND collision_sensor_test)

    add_executable(entity_pair_set_test tests/entity_pair_set.c)
    target_include_directories(entity_pair_set_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(entity_pair_set_test PRIVATE rohr_engine)
    set_target_properties(entity_pair_set_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME entity_pair_set COMMAND entity_pair_set_test)

    add_executable(aabb_tree_test tests/aabb_tree.c)
    target_include_directories(aabb_tree_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(aabb_tree_test PRIVATE rohr_engine)
    set_target_properties(aabb_tree_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME aabb_tree COMMAND aabb_tree_test)

    add_executable(physics_interaction_set_test tests/physics_interaction_set.c)
    target_include_directories(physics_interaction_set_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(physics_interaction_set_test PRIVATE rohr_engine)
    set_target_properties(physics_interaction_set_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME physics_interaction_set COMMAND physics_interaction_set_test)

    add_executable(constraint_solver_test tests/constraint_solver.c)
    target_include_directories(constraint_solver_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(constraint_solver_test PRIVATE rohr_engine)
    set_target_properties(constraint_solver_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME constraint_solver COMMAND constraint_solver_test)

    add_executable(contact_manifold_test tests/contact_manifold.c)
    target_include_directories(contact_manifold_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(contact_manifold_test PRIVATE rohr_engine)
    set_target_properties(contact_manifold_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME contact_manifold COMMAND contact_manifold_test)

    add_executable(editor_component_registry_test
        tests/editor_component_registry.c
    )
    target_link_libraries(editor_component_registry_test PRIVATE rohr_editor)
    set_target_properties(editor_component_registry_test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    add_test(
        NAME editor_component_registry
        COMMAND editor_component_registry_test
    )

    set(ROHR_GENERATED_COMPONENT_FIXTURE_DIR
        ${CMAKE_BINARY_DIR}/tests/generated_components
    )
    set(ROHR_GENERATED_COMPONENT_HEADER
        ${ROHR_GENERATED_COMPONENT_FIXTURE_DIR}/game_components.h
    )
    set(ROHR_GENERATED_COMPONENT_SOURCE
        ${ROHR_GENERATED_COMPONENT_FIXTURE_DIR}/game_components.c
    )

    add_executable(component_fixture_generator
        tests/generate_component_fixture.c
    )
    target_link_libraries(component_fixture_generator PRIVATE rohr_editor)

    add_custom_command(
        OUTPUT
            ${ROHR_GENERATED_COMPONENT_HEADER}
            ${ROHR_GENERATED_COMPONENT_SOURCE}
        COMMAND
            ${CMAKE_COMMAND} -E make_directory
            ${ROHR_GENERATED_COMPONENT_FIXTURE_DIR}
        COMMAND
            $<TARGET_FILE:component_fixture_generator>
            ${ROHR_GENERATED_COMPONENT_HEADER}
            ${ROHR_GENERATED_COMPONENT_SOURCE}
        DEPENDS component_fixture_generator
        COMMENT "Generating sparse-set component fixture"
        VERBATIM
    )

    add_executable(generated_components_test
        tests/generated_components_runtime.c
        ${ROHR_GENERATED_COMPONENT_SOURCE}
        ${ROHR_GENERATED_COMPONENT_HEADER}
    )
    target_include_directories(generated_components_test
        PRIVATE ${ROHR_GENERATED_COMPONENT_FIXTURE_DIR}
    )
    target_link_libraries(generated_components_test PRIVATE rohr_engine)
    set_target_properties(generated_components_test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    add_test(NAME generated_components COMMAND generated_components_test)
    add_test(
        NAME generated_components_deterministic
        COMMAND
            ${CMAKE_COMMAND}
            -DGENERATOR=$<TARGET_FILE:component_fixture_generator>
            -DOUTPUT_DIRECTORY=${CMAKE_BINARY_DIR}/tests/deterministic_components
            -P ${CMAKE_SOURCE_DIR}/tests/check_component_generation.cmake
    )

    set_tests_properties(
        engine_lifecycle
        controller_mappings
        cameras
        timing
        joints
        soft_body
        collision_sensor
        entity_pair_set
        aabb_tree
        physics_interaction_set
        constraint_solver
        contact_manifold
        editor_component_registry
        generated_components
        PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    )

    add_custom_target(rohr_tests)
    add_dependencies(
        rohr_tests
        engine_lifecycle_test
        controller_mappings_test
        cameras_test
        timing_test
        joints_test
        soft_body_test
        collision_sensor_test
        entity_pair_set_test
        aabb_tree_test
        physics_interaction_set_test
        constraint_solver_test
        contact_manifold_test
        editor_component_registry_test
        generated_components_test
    )
endif()

if(ROHR_ENABLE_DOCUMENTATION)
    find_package(Doxygen QUIET)
    find_package(Perl QUIET)
    find_program(PANDOC_EXECUTABLE pandoc)

    if(DOXYGEN_FOUND AND PERL_FOUND AND PANDOC_EXECUTABLE)
        set(ROHR_DOCS_OUTPUT ${CMAKE_SOURCE_DIR}/build/docs)
        set(ROHR_STATIC_OUTPUT ${CMAKE_SOURCE_DIR}/build/static)
        set(ROHR_README_HTML ${ROHR_DOCS_OUTPUT}/readme.html)
        set(ROHR_STATIC_README_HTML ${ROHR_STATIC_OUTPUT}/readme.html)

        add_custom_target(generate_engine_api
            COMMAND
                ${PERL_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/scripts/generate_public_api_markdown.pl
                ${CMAKE_SOURCE_DIR}/include/rohr.h
                ${CMAKE_SOURCE_DIR}/docs/engine_api.md
                rohr_
            DEPENDS
                ${CMAKE_SOURCE_DIR}/include/rohr.h
                ${CMAKE_SOURCE_DIR}/scripts/generate_public_api_markdown.pl
            COMMENT "Generating engine API Markdown"
            VERBATIM
        )

        add_custom_target(generate_editor_api
            COMMAND
                ${PERL_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/scripts/generate_public_api_markdown.pl
                ${CMAKE_SOURCE_DIR}/include/rohr_editor.h
                ${CMAKE_SOURCE_DIR}/docs/editor_api.md
                RE_
            DEPENDS
                ${CMAKE_SOURCE_DIR}/include/rohr_editor.h
                ${CMAKE_SOURCE_DIR}/scripts/generate_public_api_markdown.pl
            COMMENT "Generating editor API Markdown"
            VERBATIM
        )

        add_custom_target(generate_public_api)
        add_dependencies(generate_public_api generate_engine_api generate_editor_api)

        add_custom_command(
            OUTPUT ${ROHR_README_HTML}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ROHR_DOCS_OUTPUT}
            COMMAND
                ${PANDOC_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/README.md
                --standalone
                --embed-resources
                --metadata title=Rohr\ Engine
                -o ${ROHR_README_HTML}
            DEPENDS
                ${CMAKE_SOURCE_DIR}/README.md
                ${CMAKE_SOURCE_DIR}/docs/README.md
                ${CMAKE_SOURCE_DIR}/docs/assets/flies_in_pit.gif
                ${CMAKE_SOURCE_DIR}/docs/assets/flies_around_ball.gif
            COMMENT "Rendering README HTML"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_command(
            OUTPUT ${ROHR_STATIC_README_HTML}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ROHR_STATIC_OUTPUT}/docs/assets
            COMMAND
                ${PANDOC_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/README.md
                --standalone
                --embed-resources
                --metadata title=Rohr\ Engine
                -o ${ROHR_STATIC_README_HTML}
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/docs/assets/flies_in_pit.mp4
                ${ROHR_STATIC_OUTPUT}/docs/assets/flies_in_pit.mp4
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/docs/assets/flies_around_ball.mp4
                ${ROHR_STATIC_OUTPUT}/docs/assets/flies_around_ball.mp4
            DEPENDS
                ${CMAKE_SOURCE_DIR}/README.md
                ${CMAKE_SOURCE_DIR}/docs/README.md
                ${CMAKE_SOURCE_DIR}/docs/assets/flies_in_pit.gif
                ${CMAKE_SOURCE_DIR}/docs/assets/flies_around_ball.gif
                ${CMAKE_SOURCE_DIR}/docs/assets/flies_in_pit.mp4
                ${CMAKE_SOURCE_DIR}/docs/assets/flies_around_ball.mp4
            COMMENT "Rendering standalone README preview"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_target(readme_html DEPENDS ${ROHR_README_HTML})
        add_custom_target(static_readme DEPENDS ${ROHR_STATIC_README_HTML})

        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/docs/Doxyfile
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS generate_public_api readme_html static_readme
            COMMENT "Generating Doxygen documentation"
            VERBATIM
        )
    else()
        message(WARNING
            "Documentation targets are disabled because Doxygen, Perl, or Pandoc was not found"
        )
    endif()
endif()
