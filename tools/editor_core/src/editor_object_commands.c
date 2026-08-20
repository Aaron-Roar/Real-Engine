/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_object_commands.h"

#include <stdio.h>
#include <string.h>

EditorObject *editor_object_query_get(EditorProject *project, EditorObjectId object) {
    if(project == NULL || object == EDITOR_OBJECT_INVALID) return NULL;
    for(size_t i = 0; i < project->object_count; i += 1)
        if(project->objects[i].id == object) return &project->objects[i];
    return NULL;
}

const EditorObject *editor_object_query_const_get(const EditorProject *project,
        EditorObjectId object) {
    return editor_object_query_get((EditorProject *)project, object);
}

EditorObjectIdResult editor_object_command_add(EditorProject *project,
        const EditorObjectAddArgs *arguments) {
    EditorObject *object;
    char formatted[EDITOR_OBJECT_NAME_MAX];
    if(project == NULL || arguments == NULL)
        return (EditorObjectIdResult){.kind = ERROR_RESULT_ERROR,
            .result.error = {EDITOR_ERROR_INVALID_ARGUMENT,
                "object add requires a project and name"}};
    if(arguments->name != NULL)
        editor_project_object_name_format(formatted, sizeof(formatted), arguments->name);
    else formatted[0] = '\0';
    if(arguments->name != NULL && formatted[0] == '\0')
        return (EditorObjectIdResult){.kind = ERROR_RESULT_ERROR,
            .result.error = {EDITOR_ERROR_NAME_INVALID,
                "object name does not contain a valid identifier"}};
    object = editor_project_object_add(project, arguments->position);
    if(object == NULL)
        return (EditorObjectIdResult){.kind = ERROR_RESULT_ERROR,
            .result.error = {EDITOR_ERROR_CAPACITY,
                "editor object capacity is exhausted"}};
    if(formatted[0] != '\0')
        snprintf(object->name, sizeof(object->name), "%s", formatted);
    return (EditorObjectIdResult){.kind = ERROR_RESULT_VALUE,
        .result.value = object->id};
}

EditorResult editor_object_command_remove(EditorProject *project,
        EditorObjectId object) {
    if(project == NULL)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "object remove requires a project");
    if(!editor_project_object_remove(project, object))
        return editor_result_error(EDITOR_ERROR_NOT_FOUND,
            "object %u was not found", object);
    return editor_result_value(true);
}

EditorResult editor_object_command_rename(EditorProject *project,
        EditorObjectId object, const char *name) {
    EditorObject *value = editor_object_query_get(project, object);
    char formatted[EDITOR_OBJECT_NAME_MAX];
    if(value == NULL) return editor_result_error(EDITOR_ERROR_NOT_FOUND,
        "object %u was not found", object);
    editor_project_object_name_format(formatted, sizeof(formatted), name);
    if(formatted[0] == '\0') return editor_result_error(EDITOR_ERROR_NAME_INVALID,
        "object name does not contain a valid identifier");
    snprintf(value->name, sizeof(value->name), "%s", formatted);
    return editor_result_value(true);
}
