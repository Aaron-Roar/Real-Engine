cmake_minimum_required(VERSION 3.20)

project(rohr_engine LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(ROHR_BUILD_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH
    "Directory for user-facing Rohr build artifacts")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ROHR_BUILD_OUTPUT_DIRECTORY}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ROHR_BUILD_OUTPUT_DIRECTORY}/lib")

option(ROHR_BUILD_EXAMPLES "Build Rohr example programs" ON)
option(ROHR_BUILD_TESTS "Build Rohr tests" ON)
option(ROHR_BUILD_EDITOR "Build Rohr Editor" ON)
option(ROHR_BUILD_TERMINAL "Build Rohr Terminal tool" ON)
option(ROHR_BUILD_CLI "Build Rohr project CLI" ON)
option(ROHR_ENABLE_DOCUMENTATION "Enable documentation build targets" ON)
option(ROHR_PORTABLE_SDK
    "Avoid development-package dependencies in installed Rohr SDKs" OFF)
option(ROHR_BUILD_SDK_CONSUMER_TESTS
    "Test an external consumer against the installed Rohr SDK" OFF)
set(ROHR_SDK_INSTALL_PREFIX "" CACHE PATH
    "Installed SDK prefix used by external consumer tests")

set(ROHR_LUA_SOURCES
    lib/lua/lapi.c
    lib/lua/lauxlib.c
    lib/lua/lbaselib.c
    lib/lua/lcode.c
    lib/lua/lcorolib.c
    lib/lua/lctype.c
    lib/lua/ldebug.c
    lib/lua/ldo.c
    lib/lua/ldump.c
    lib/lua/lfunc.c
    lib/lua/lgc.c
    lib/lua/linit.c
    lib/lua/llex.c
    lib/lua/lmathlib.c
    lib/lua/lmem.c
    lib/lua/lobject.c
    lib/lua/lopcodes.c
    lib/lua/lparser.c
    lib/lua/lstate.c
    lib/lua/lstring.c
    lib/lua/lstrlib.c
    lib/lua/ltable.c
    lib/lua/ltablib.c
    lib/lua/ltm.c
    lib/lua/lundump.c
    lib/lua/lutf8lib.c
    lib/lua/lvm.c
    lib/lua/lzio.c
)
add_library(rohr_lua STATIC ${ROHR_LUA_SOURCES})
target_include_directories(rohr_lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/lua)
if(NOT MSVC)
    target_link_libraries(rohr_lua PUBLIC m)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SDL_SHARED OFF CACHE BOOL "Build SDL3 as a shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL3 as a static library" FORCE)
set(ROHR_SDL_DEPS_SHARED OFF)
if(ROHR_PORTABLE_SDK AND UNIX AND NOT APPLE)
    set(ROHR_SDL_DEPS_SHARED ON)
endif()
set(SDL_DEPS_SHARED ${ROHR_SDL_DEPS_SHARED} CACHE BOOL
    "Load SDL platform dependencies dynamically" FORCE)
set(SDL_ALSA_SHARED ${ROHR_SDL_DEPS_SHARED} CACHE BOOL "Load ALSA dynamically" FORCE)
set(SDL_PIPEWIRE_SHARED ${ROHR_SDL_DEPS_SHARED} CACHE BOOL
    "Load PipeWire dynamically" FORCE)
set(SDL_PULSEAUDIO_SHARED ${ROHR_SDL_DEPS_SHARED} CACHE BOOL
    "Load PulseAudio dynamically" FORCE)
if(UNIX AND NOT APPLE)
    set(SDL_WAYLAND ON CACHE BOOL "Build the native Wayland video driver" FORCE)
    set(SDL_WAYLAND_SHARED ${ROHR_SDL_DEPS_SHARED} CACHE BOOL
        "Load Wayland dynamically" FORCE)
    set(SDL_X11 ON CACHE BOOL "Build the X11 fallback video driver" FORCE)
endif()
set(SDL_X11_SHARED ${ROHR_SDL_DEPS_SHARED} CACHE BOOL "Load X11 dynamically" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3 test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
set(SDL_INSTALL ON CACHE BOOL "Install SDL3" FORCE)
add_subdirectory(lib/SDL)

set(ROHR_SDL3_TARGET SDL3::SDL3-static)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled libraries statically" FORCE)
set(SDLIMAGE_INSTALL ON CACHE BOOL "Install SDL3_image" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "Build SDL3_image samples" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "Build SDL3_image tests" FORCE)
set(SDLIMAGE_VENDORED OFF CACHE BOOL "Use SDL_image codec submodules" FORCE)
add_subdirectory(lib/SDL_image)

set(SDLTTF_INSTALL ON CACHE BOOL "Install SDL3_ttf" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "Build SDL3_ttf samples" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "Build SDL_ttf color emoji support" FORCE)
if(WIN32 OR ROHR_PORTABLE_SDK)
    set(SDLTTF_VENDORED ON CACHE BOOL "Use bundled SDL_ttf dependencies" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "Build SDL_ttf HarfBuzz support" FORCE)
else()
    set(SDLTTF_VENDORED OFF CACHE BOOL "Use bundled SDL_ttf dependencies" FORCE)
    set(SDLTTF_HARFBUZZ ON CACHE BOOL "Build SDL_ttf HarfBuzz support" FORCE)
endif()
add_subdirectory(lib/SDL_ttf)
if(WIN32 AND SDLTTF_VENDORED)
    install(SCRIPT
        "${CMAKE_CURRENT_BINARY_DIR}/lib/SDL_ttf/external/freetype/cmake_install.cmake")
endif()

set(ROHR_SDK_RUNTIME_LIBRARY_DIRS)
if(ROHR_PORTABLE_SDK AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(runtime_library_variable
            ALSA_LIBRARY
            PIPEWIRE_0.3_LIB
            PULSE_LIB
            WAYLAND_CLIENT_LIB
            WAYLAND_CURSOR_LIB
            WAYLAND_EGL_LIB
            DECOR_0_LIB
            XKBCOMMON_LIB
            X11_X11_LIB
            X11_Xcursor_LIB
            X11_Xext_LIB
            X11_Xfixes_LIB
            X11_Xi_LIB
            X11_Xrandr_LIB
            X11_Xss_LIB
            X11_Xtst_LIB)
        if(DEFINED ${runtime_library_variable} AND
                "${${runtime_library_variable}}" MATCHES "^/nix/store/")
            get_filename_component(runtime_library_directory
                "${${runtime_library_variable}}" DIRECTORY)
            list(APPEND ROHR_SDK_RUNTIME_LIBRARY_DIRS
                "${runtime_library_directory}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES ROHR_SDK_RUNTIME_LIBRARY_DIRS)
    set(CMAKE_INSTALL_RPATH "${ROHR_SDK_RUNTIME_LIBRARY_DIRS}")
endif()

set(ROHR_SDL3_IMAGE_TARGET SDL3_image::SDL3_image-static)
set(ROHR_SDL3_TTF_TARGET SDL3_ttf::SDL3_ttf-static)

set(ROHR_ENGINE_SOURCES
    src/core/console.c
    src/core/engine.c
    src/core/error.c
    src/core/platform_process.c
    src/core/rohr.c
    src/core/systems.c
    src/entity/entity_components.c
    src/entity/entity_pair_set.c
    src/graphics/graphics.c
    src/graphics/graphics_layer_order.c
    src/graphics/window_presentation.c
    src/graphics/ui.c
    src/input/controller.c
    src/math/math2d.c
    src/physics/broadphase/aabb_tree.c
    src/physics/collision/contact_constraint.c
    src/physics/collision/contact_manifold.c
    src/physics/collision/interaction_set.c
    src/physics/constraints/constraint_solver.c
    src/physics/joints/joint_constraint.c
    src/physics/joints/joint_solver.c
    src/physics/particles/particle.c
    src/physics/physics.c
    src/physics/physics_step.c
    src/physics/pipeline/physics_pipeline.c
    src/physics/pipeline/physics_stages.c
    src/physics/rigid_body/rigid_body.c
    src/physics/rigid_body/rigid_body_step.c
    src/physics/soft_body/soft_body.c
    src/physics/soft_body/soft_body_solver.c
    src/state/game_state.c
    src/tools/tools.c
    lib/yyjson.c
)

add_library(rohr_engine STATIC ${ROHR_ENGINE_SOURCES})
add_library(Rohr::Engine ALIAS rohr_engine)
set_target_properties(rohr_engine PROPERTIES EXPORT_NAME Engine)
target_include_directories(rohr_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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
foreach(runtime_library_directory IN LISTS ROHR_SDK_RUNTIME_LIBRARY_DIRS)
    target_link_options(rohr_engine INTERFACE
        "$<INSTALL_INTERFACE:-Wl,-rpath,${runtime_library_directory}>")
endforeach()

if(NOT MSVC)
    target_link_libraries(rohr_engine PUBLIC m)
endif()

add_library(rohr_tools STATIC
    src/tools/component_codegen.c
)
add_library(Rohr::Tools ALIAS rohr_tools)
set_target_properties(rohr_tools PROPERTIES EXPORT_NAME Tools)
target_include_directories(rohr_tools
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS rohr_engine rohr_tools
    EXPORT RohrTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT RohrTargets
    FILE RohrTargets.cmake
    NAMESPACE Rohr::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Rohr)
configure_package_config_file(
    cmake/RohrConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/RohrConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Rohr)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/RohrConfigVersion.cmake
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/RohrConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/RohrConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Rohr)
install(FILES
    config/editor.lua
    DESTINATION ${CMAKE_INSTALL_DATADIR}/rohr)
install(FILES
    config/project-editor.lua
    DESTINATION ${CMAKE_INSTALL_DATADIR}/rohr)
install(FILES
    examples/flies-around-ball/assets/elder-fly/flying/f1.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/rohr/starter-assets
    RENAME tutorial_frame_1.png)
install(FILES
    examples/flies-around-ball/assets/elder-fly/flying/f2.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/rohr/starter-assets
    RENAME tutorial_frame_2.png)
install(FILES
    config/editor_gui_settings.lua
    DESTINATION config)
install(FILES lib/lua/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/rohr
    RENAME Lua-LICENSE)

include(cmake/RohrHostTools.cmake)

if(ROHR_BUILD_TESTS OR ROHR_BUILD_SDK_CONSUMER_TESTS)
    include(CTest)
endif()

if(ROHR_BUILD_TERMINAL)
    add_subdirectory(tools/terminal)
endif()

add_subdirectory(tools/editor_core)

if(ROHR_BUILD_CLI)
    add_subdirectory(tools/cli)
endif()

if(ROHR_BUILD_EDITOR)
    if(NOT ROHR_BUILD_TERMINAL)
        message(FATAL_ERROR "ROHR_BUILD_EDITOR requires ROHR_BUILD_TERMINAL")
    endif()
    add_subdirectory(tools/editor)
endif()

if(ROHR_BUILD_EXAMPLES)
    add_subdirectory(examples)

    add_custom_target(examples)
    add_dependencies(examples
        view_port
        flies_in_pit
        flies_around_ball
        fly_to_finish
        game_state
        user_interface
        joints
        soft_body
        pong
    )

endif()

if(ROHR_BUILD_SDK_CONSUMER_TESTS)
    if(NOT ROHR_SDK_INSTALL_PREFIX)
        message(FATAL_ERROR
            "ROHR_BUILD_SDK_CONSUMER_TESTS requires ROHR_SDK_INSTALL_PREFIX")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(ROHR_SDK_TEST_PLATFORM windows)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(ROHR_SDK_TEST_PLATFORM linux)
    else()
        message(FATAL_ERROR
            "Installed SDK consumer tests do not support ${CMAKE_SYSTEM_NAME}")
    endif()
    add_test(
        NAME installed_sdk_consumer_${ROHR_SDK_TEST_PLATFORM}
        COMMAND ${CMAKE_COMMAND}
            "-DROHR_SDK_PREFIX=${ROHR_SDK_INSTALL_PREFIX}"
            "-DROHR_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/installed_sdk_consumer"
            "-DROHR_CONSUMER_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/installed_sdk_consumer_${ROHR_SDK_TEST_PLATFORM}"
            "-DROHR_GENERATOR=${CMAKE_GENERATOR}"
            "-DROHR_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DROHR_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
            "-DROHR_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_installed_sdk_consumer.cmake")
    set_tests_properties(installed_sdk_consumer_${ROHR_SDK_TEST_PLATFORM}
        PROPERTIES LABELS "sdk;${ROHR_SDK_TEST_PLATFORM}")
endif()

if(ROHR_BUILD_TESTS)
    add_executable(engine_lifecycle_test tests/engine_lifecycle.c)
    target_include_directories(engine_lifecycle_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(engine_lifecycle_test PRIVATE rohr_engine)
    set_target_properties(engine_lifecycle_test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )

    add_test(NAME engine_lifecycle COMMAND engine_lifecycle_test)

    add_executable(controller_mappings_test tests/controller_mappings.c)
    target_link_libraries(controller_mappings_test PRIVATE rohr_engine)
    set_target_properties(controller_mappings_test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )
    add_test(NAME controller_mappings COMMAND controller_mappings_test)

    add_executable(cameras_test tests/cameras.c)
    target_link_libraries(cameras_test PRIVATE rohr_engine)
    set_target_properties(cameras_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME cameras COMMAND cameras_test)

    add_executable(graphics_layer_order_test
        tests/graphics_layer_order.c
        src/graphics/graphics_layer_order.c)
    target_include_directories(graphics_layer_order_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(graphics_layer_order_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME graphics_layer_order COMMAND graphics_layer_order_test)

    add_executable(window_presentation_test tests/window_presentation.c)
    target_link_libraries(window_presentation_test PRIVATE rohr_engine)
    set_target_properties(window_presentation_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)

    add_executable(timing_test tests/timing.c)
    target_link_libraries(timing_test PRIVATE rohr_engine)
    set_target_properties(timing_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME timing COMMAND timing_test)

    add_executable(ui_field_test tests/ui_field.c)
    target_link_libraries(ui_field_test PRIVATE rohr_engine)
    set_target_properties(ui_field_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME ui_field COMMAND ui_field_test)

    add_executable(joints_test tests/joints.c)
    target_link_libraries(joints_test PRIVATE rohr_engine)
    set_target_properties(joints_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME joints COMMAND joints_test)

    add_executable(soft_body_test tests/soft_body.c)
    target_link_libraries(soft_body_test PRIVATE rohr_engine)
    set_target_properties(soft_body_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME soft_body COMMAND soft_body_test)

    add_executable(collision_sensor_test tests/collision_sensor.c)
    target_include_directories(collision_sensor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(collision_sensor_test PRIVATE rohr_engine)
    set_target_properties(collision_sensor_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME collision_sensor COMMAND collision_sensor_test)

    add_executable(entity_pair_set_test tests/entity_pair_set.c)
    target_include_directories(entity_pair_set_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(entity_pair_set_test PRIVATE rohr_engine)
    set_target_properties(entity_pair_set_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME entity_pair_set COMMAND entity_pair_set_test)

    add_executable(aabb_tree_test tests/aabb_tree.c)
    target_include_directories(aabb_tree_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(aabb_tree_test PRIVATE rohr_engine)
    set_target_properties(aabb_tree_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME aabb_tree COMMAND aabb_tree_test)

    add_executable(physics_interaction_set_test tests/physics_interaction_set.c)
    target_include_directories(physics_interaction_set_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(physics_interaction_set_test PRIVATE rohr_engine)
    set_target_properties(physics_interaction_set_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME physics_interaction_set COMMAND physics_interaction_set_test)

    add_executable(constraint_solver_test tests/constraint_solver.c)
    target_include_directories(constraint_solver_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(constraint_solver_test PRIVATE rohr_engine)
    set_target_properties(constraint_solver_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME constraint_solver COMMAND constraint_solver_test)

    add_executable(contact_manifold_test tests/contact_manifold.c)
    target_include_directories(contact_manifold_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(contact_manifold_test PRIVATE rohr_engine)
    set_target_properties(contact_manifold_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME contact_manifold COMMAND contact_manifold_test)

    add_executable(component_registry_test
        tests/component_registry.c
    )
    target_link_libraries(component_registry_test PRIVATE rohr_tools)
    set_target_properties(component_registry_test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )

    add_test(
        NAME component_registry
        COMMAND component_registry_test
    )

    set(ROHR_GENERATED_COMPONENT_FIXTURE_DIR
        ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests/generated_components
    )
    set(ROHR_GENERATED_COMPONENT_HEADER
        ${ROHR_GENERATED_COMPONENT_FIXTURE_DIR}/game_components.h
    )
    set(ROHR_GENERATED_COMPONENT_SOURCE
        ${ROHR_GENERATED_COMPONENT_FIXTURE_DIR}/game_components.c
    )

    rohr_add_component_generator(component_fixture_generator
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_component_fixture.c")

    add_custom_command(
        OUTPUT
            ${ROHR_GENERATED_COMPONENT_HEADER}
            ${ROHR_GENERATED_COMPONENT_SOURCE}
        COMMAND
            ${CMAKE_COMMAND} -E make_directory
            ${ROHR_GENERATED_COMPONENT_FIXTURE_DIR}
        COMMAND
            ${component_fixture_generator_COMMAND}
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
            RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )

    add_test(NAME generated_components COMMAND generated_components_test)
    add_test(
        NAME generated_components_deterministic
        COMMAND
            ${CMAKE_COMMAND}
            -DGENERATOR=${component_fixture_generator_COMMAND}
            -DOUTPUT_DIRECTORY=${ROHR_BUILD_OUTPUT_DIRECTORY}/tests/deterministic_components
            -P ${CMAKE_SOURCE_DIR}/tests/check_component_generation.cmake
    )

    set_tests_properties(
        engine_lifecycle
        controller_mappings
        cameras
        graphics_layer_order
        timing
        joints
        soft_body
        collision_sensor
        entity_pair_set
        aabb_tree
        physics_interaction_set
        constraint_solver
        contact_manifold
        component_registry
        generated_components
        PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    )

    add_custom_target(rohr_tests)
    add_dependencies(
        rohr_tests
        engine_lifecycle_test
        controller_mappings_test
        cameras_test
        graphics_layer_order_test
        timing_test
        joints_test
        soft_body_test
        collision_sensor_test
        entity_pair_set_test
        aabb_tree_test
        physics_interaction_set_test
        constraint_solver_test
        contact_manifold_test
        component_registry_test
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

        add_custom_target(generate_tools_api
            COMMAND
                ${PERL_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/scripts/generate_public_api_markdown.pl
                ${CMAKE_SOURCE_DIR}/include/rohr_tools.h
                ${CMAKE_SOURCE_DIR}/docs/tools_api.md
                rohr_tools_
            DEPENDS
                ${CMAKE_SOURCE_DIR}/include/rohr_tools.h
                ${CMAKE_SOURCE_DIR}/scripts/generate_public_api_markdown.pl
            COMMENT "Generating tools API Markdown"
            VERBATIM
        )

        add_custom_target(generate_public_api)
        add_dependencies(generate_public_api generate_engine_api generate_tools_api)

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
