/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "rohr.h"

#include <stddef.h>

int main(void) {
    return rohr_error_code_message_get(ERROR_NONE) == NULL;
}
