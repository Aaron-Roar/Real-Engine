/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "rohr.h"

#include <stdio.h>
#include <string.h>

static bool presentation_set(GraphicsWindowPresentationConfig *config,
        GraphicsWindowMode mode) {
    EngineResult result;
    config->mode = mode;
    result = rohr_graphics_window_presentation_set(*config);
    if(rohr_error_check(result)) {
        fprintf(stderr, "%s\n", rohr_error_message_get(result));
        return false;
    }
    return rohr_graphics_window_presentation_get().mode == mode;
}

int main(void) {
    GraphicsWindowPresentationConfig config =
        rohr_graphics_window_presentation_default_get();
    GraphicsWindowPresentationConfig active;
    if(rohr_error_check(rohr_engine_init()) ||
            rohr_error_check(rohr_graphics_start())) return 1;
    if(!presentation_set(&config, GRAPHICS_WINDOW_MODE_WINDOWED)) goto fail;
    if(SDL_GetCurrentVideoDriver() != NULL &&
            strcmp(SDL_GetCurrentVideoDriver(), "dummy") == 0) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 0;
    }
    if(!presentation_set(&config, GRAPHICS_WINDOW_MODE_BORDERLESS_FULLSCREEN))
        goto fail;
    active = rohr_graphics_window_presentation_get();
    config.window_width = active.window_width;
    config.window_height = active.window_height;
    config.logical_width = active.window_width;
    config.logical_height = active.window_height;
    if(!presentation_set(&config, GRAPHICS_WINDOW_MODE_FULLSCREEN) ||
            !presentation_set(&config, GRAPHICS_WINDOW_MODE_WINDOWED)) goto fail;
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;
fail:
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
