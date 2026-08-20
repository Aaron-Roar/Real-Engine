/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_error.h"

#include <stdarg.h>
#include <stdio.h>

EditorResult editor_result_value(bool value) {
    return (EditorResult){
        .kind = ERROR_RESULT_VALUE,
        .result.value = value
    };
}

EditorResult editor_result_error(EditorErrorCode code, const char *format, ...) {
    EditorResult result = {
        .kind = ERROR_RESULT_ERROR,
        .result.error.code = code
    };
    va_list arguments;

    if(format == NULL) return result;
    va_start(arguments, format);
    (void)vsnprintf(result.result.error.message,
        sizeof(result.result.error.message), format, arguments);
    va_end(arguments);
    return result;
}

bool editor_result_check(EditorResult result) {
    return result.kind == ERROR_RESULT_ERROR;
}

void editor_result_stderr_print(EditorResult result) {
    if(editor_result_check(result)) fprintf(stderr, "%s\n", result.result.error.message);
}
