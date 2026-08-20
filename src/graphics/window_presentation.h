/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_GRAPHICS_WINDOW_PRESENTATION_H
#define ROHR_GRAPHICS_WINDOW_PRESENTATION_H

#include "graphics.h"

EngineResult graphics_window_presentation_apply(SDL_Window *window,
    GraphicsWindowPresentationConfig config);

#endif
