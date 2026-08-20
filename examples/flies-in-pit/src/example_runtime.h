/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EXAMPLE_RUNTIME_H
#define EXAMPLE_RUNTIME_H

#include <stdbool.h>

/** Set the process working directory to the directory containing the executable. */
bool example_use_executable_directory(void);

#endif
