cmake_minimum_required(VERSION 3.20)

project(rohr_engine LANGUAGES C)

option(ROHR_BUILD_EXAMPLES "Build Rohr example programs" ON)
option(ROHR_BUILD_TESTS "Build Rohr tests" ON)
option(ROHR_ENABLE_DOCUMENTATION "Enable documentation build targets" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(SDL3 CONFIG QUIET)
find_package(SDL3_image CONFIG QUIET)
find_package(SDL3_ttf CONFIG QUIET)

set(ROHR_SDL3_TARGET "")
foreach(candidate SDL3::SDL3 SDL3::SDL3-shared SDL3::SDL3-static)
    if(TARGET ${candidate})
        set(ROHR_SDL3_TARGET ${candidate})
        break()
    endif()
endforeach()

set(ROHR_SDL3_IMAGE_TARGET "")
foreach(candidate
    SDL3_image::SDL3_image
    SDL3_image::SDL3_image-shared
    SDL3_image::SDL3_image-static
)
    if(TARGET ${candidate})
        set(ROHR_SDL3_IMAGE_TARGET ${candidate})
        break()
    endif()
endforeach()

set(ROHR_SDL3_TTF_TARGET "")
foreach(candidate
    SDL3_ttf::SDL3_ttf
    SDL3_ttf::SDL3_ttf-shared
    SDL3_ttf::SDL3_ttf-static
)
    if(TARGET ${candidate})
        set(ROHR_SDL3_TTF_TARGET ${candidate})
        break()
    endif()
endforeach()

if(NOT ROHR_SDL3_TARGET OR NOT ROHR_SDL3_IMAGE_TARGET OR NOT ROHR_SDL3_TTF_TARGET)
    find_package(PkgConfig REQUIRED)

    if(NOT ROHR_SDL3_TARGET)
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
        set(ROHR_SDL3_TARGET PkgConfig::SDL3)
    endif()

    if(NOT ROHR_SDL3_IMAGE_TARGET)
        pkg_check_modules(SDL3_IMAGE REQUIRED IMPORTED_TARGET sdl3-image)
        set(ROHR_SDL3_IMAGE_TARGET PkgConfig::SDL3_IMAGE)
    endif()

    if(NOT ROHR_SDL3_TTF_TARGET)
        pkg_check_modules(SDL3_TTF REQUIRED IMPORTED_TARGET sdl3-ttf)
        set(ROHR_SDL3_TTF_TARGET PkgConfig::SDL3_TTF)
    endif()
endif()

set(ROHR_ENGINE_SOURCES
    src/console.c
    src/engine.c
    src/error.c
    src/entity_components.c
    src/graphics.c
    src/grid.c
    src/math2d.c
    src/physics.c
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
    )

    add_custom_target(editor_examples)
    add_dependencies(editor_examples basic_editor pong)

    add_custom_target(examples)
    add_dependencies(examples core_examples editor_examples)
endif()

if(ROHR_BUILD_TESTS)
    include(CTest)

    add_executable(engine_lifecycle_test tests/engine_lifecycle.c)
    target_link_libraries(engine_lifecycle_test PRIVATE rohr_engine)
    set_target_properties(engine_lifecycle_test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    add_test(NAME engine_lifecycle COMMAND engine_lifecycle_test)
    set_tests_properties(engine_lifecycle
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    )

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

    add_custom_target(rohr_tests)
    add_dependencies(
        rohr_tests
        engine_lifecycle_test
        controller_mappings_test
        cameras_test
        timing_test
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
