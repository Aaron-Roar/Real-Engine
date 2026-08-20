/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_document.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

EditorResult editor_document_create(EditorDocument *document) {
    if(document == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "document output is null");
    memset(document, 0, sizeof(*document));
    document->project = calloc(1, sizeof(*document->project));
    if(document->project == NULL) return editor_result_error(EDITOR_ERROR_FILE_IO,
        "could not allocate editor document");
    editor_project_init(document->project);
    return editor_result_value(true);
}

EditorResult editor_document_load(EditorDocument *document, const char *path) {
    EditorProject *project;
    EditorResult result;
    if(document == NULL || document->project == NULL ||
            path == NULL || path[0] == '\0')
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "document load requires a path");
    if(strlen(path) >= sizeof(document->path))
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "document path is too long: %s", path);
    project = calloc(1, sizeof(*project));
    if(project == NULL) return editor_result_error(EDITOR_ERROR_FILE_IO,
        "could not allocate editor document load buffer");
    result = editor_project_load(project, path);
    if(editor_result_check(result)) {
        free(project);
        return result;
    }
    *document->project = *project;
    free(project);
    snprintf(document->path, sizeof(document->path), "%s", path);
    document->dirty = false;
    return editor_result_value(true);
}

static bool editor_document_replace(const char *temporary, const char *path) {
#if defined(_WIN32)
    return MoveFileExA(temporary, path,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, path) == 0;
#endif
}

EditorResult editor_document_save_as(EditorDocument *document, const char *path) {
    char temporary[EDITOR_DOCUMENT_PATH_MAX + 32];
    if(document == NULL || document->project == NULL ||
            path == NULL || path[0] == '\0')
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "document save requires a path");
    if(strlen(path) >= sizeof(document->path) ||
            snprintf(temporary, sizeof(temporary), "%s.tmp", path) >=
                (int)sizeof(temporary))
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "document path is too long: %s", path);
    if(!editor_project_save(document->project, temporary))
        return editor_result_error(EDITOR_ERROR_FILE_IO,
            "could not write temporary editor document: %s", temporary);
    if(!editor_document_replace(temporary, path)) {
        (void)remove(temporary);
        return editor_result_error(EDITOR_ERROR_FILE_IO,
            "could not atomically replace editor document: %s", path);
    }
    snprintf(document->path, sizeof(document->path), "%s", path);
    document->dirty = false;
    return editor_result_value(true);
}

EditorResult editor_document_save(EditorDocument *document) {
    if(document == NULL || document->path[0] == '\0')
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "document has no save path");
    return editor_document_save_as(document, document->path);
}

EditorProject *editor_document_project_get(EditorDocument *document) {
    return document == NULL ? NULL : document->project;
}

const EditorProject *editor_document_project_const_get(const EditorDocument *document) {
    return document == NULL ? NULL : document->project;
}

void editor_document_destroy(EditorDocument *document) {
    if(document == NULL) return;
    free(document->project);
    *document = (EditorDocument){0};
}
