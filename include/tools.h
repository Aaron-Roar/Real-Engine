/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef TOOLS_H
#define TOOLS_H
#include "stdint.h"
#include "stddef.h"
#include <time.h>

/** Delay execution for a whole number of seconds. */
void delay(int seconds);
/** Convert an integer bit pattern to a string buffer. */
void binary_to_string(uint32_t value, char* buffer, size_t size);
/** Append one string buffer into another with explicit sizes. */
void tools_append_string(char* src, char* dst, size_t src_size, size_t dst_size);
/** Count characters in a string until a delimiter. */
uint32_t tool_sizeof_string(char* str, char delimiter);
/** Return a random integer in the given range. */
int tools_random_range(int min, int max);
/** Return a random float in the given range. */
float tools_random_range_float(float min, float max);
#endif
