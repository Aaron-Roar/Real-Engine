#include "window_presentation.h"

EngineResult graphics_window_presentation_apply(SDL_Window *window,
        GraphicsWindowPresentationConfig config) {
    bool applied = false;
    if(window == NULL || config.window_width <= 0 || config.window_height <= 0)
        return error_result_error(ERROR_ENGINE_GRAPHICS_WINDOW_PRESENTATION_FAILED);
    switch(config.mode) {
        case GRAPHICS_WINDOW_MODE_WINDOWED:
            applied = SDL_SetWindowFullscreen(window, false) &&
                SDL_SetWindowFullscreenMode(window, NULL) &&
                SDL_SetWindowSize(window, config.window_width, config.window_height);
            break;
        case GRAPHICS_WINDOW_MODE_BORDERLESS_FULLSCREEN:
            applied = SDL_SetWindowFullscreenMode(window, NULL) &&
                SDL_SetWindowFullscreen(window, true);
            break;
        case GRAPHICS_WINDOW_MODE_FULLSCREEN: {
            SDL_DisplayID display = SDL_GetDisplayForWindow(window);
            SDL_DisplayMode mode;
            applied = display != 0 && SDL_GetClosestFullscreenDisplayMode(display,
                config.window_width, config.window_height, 0.0f, true, &mode) &&
                SDL_SetWindowFullscreenMode(window, &mode) &&
                SDL_SetWindowFullscreen(window, true);
            break;
        }
        default:
            break;
    }
    return applied ? error_result_value(true) :
        error_result_error_detail(
            ERROR_ENGINE_GRAPHICS_WINDOW_PRESENTATION_FAILED, SDL_GetError());
}
