cmake_minimum_required(VERSION 3.20)

project(rohr_engine LANGUAGES C)

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

set(ROHR_EXAMPLE_ASSET_SOURCE
    examples/test-assets/elder-fly/elderfly_descriptors.c
)

function(rohr_add_example target_name source_file)
    add_executable(${target_name}
        ${source_file}
        ${ROHR_EXAMPLE_ASSET_SOURCE}
    )
    target_link_libraries(${target_name} PRIVATE rohr_engine)
    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/test-assets
    )
    set_target_properties(${target_name}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endfunction()

rohr_add_example(view_port examples/view-port/view_port.c)
rohr_add_example(flies_in_pit examples/flies-in-pit/flies_in_pit.c)
rohr_add_example(flies_around_ball examples/flies-around-ball/flies_around_ball.c)
rohr_add_example(game_state examples/game-state/game_state.c)

add_executable(pong examples/pong/pong.c)
target_link_libraries(pong PRIVATE rohr_engine)
target_include_directories(pong PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(pong
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

add_executable(basic_editor examples/editor/basic_editor.c)
target_link_libraries(basic_editor PRIVATE rohr_editor)
set_target_properties(basic_editor
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/editor
)
