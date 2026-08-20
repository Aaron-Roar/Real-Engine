/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_ERROR_H
#define ROHR_EDITOR_ERROR_H

#include "rohr.h"

#define EDITOR_ERROR_MESSAGE_MAX 512

typedef enum EditorErrorCode {
    EDITOR_ERROR_NONE = 0,
    EDITOR_ERROR_INVALID_ARGUMENT,
    EDITOR_ERROR_FILE_IO,
    EDITOR_ERROR_JSON_PARSE,
    EDITOR_ERROR_SCHEMA_VERSION,
    EDITOR_ERROR_SCHEMA_INVALID,
    EDITOR_ERROR_REFERENCE_INVALID,
    EDITOR_ERROR_PROJECT_ROOT_NOT_FOUND,
    EDITOR_ERROR_NOT_FOUND,
    EDITOR_ERROR_CAPACITY,
    EDITOR_ERROR_NAME_INVALID
} EditorErrorCode;

typedef struct EditorError {
    EditorErrorCode code;
    char message[EDITOR_ERROR_MESSAGE_MAX];
} EditorError;

typedef struct EditorResult {
    ErrorResultKind kind;
    union {
        bool value;
        EditorError error;
    } result;
} EditorResult;

EditorResult editor_result_value(bool value);
EditorResult editor_result_error(EditorErrorCode code, const char *format, ...);
bool editor_result_check(EditorResult result);
void editor_result_stderr_print(EditorResult result);

#endif
