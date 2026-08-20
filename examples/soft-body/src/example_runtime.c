/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "example_runtime.h"
#include <SDL3/SDL.h>
#if defined(_WIN32)
#include <direct.h>
#define example_chdir _chdir
#else
#include <unistd.h>
#define example_chdir chdir
#endif
bool example_use_executable_directory(void) {
    const char *base_path = SDL_GetBasePath();
    return base_path != NULL && example_chdir(base_path) == 0;
}
