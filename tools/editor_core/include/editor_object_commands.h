#ifndef ROHR_EDITOR_OBJECT_COMMANDS_H
#define ROHR_EDITOR_OBJECT_COMMANDS_H

#include "editor_project.h"

typedef struct EditorObjectAddArgs {
    const char *name;
    Position position;
} EditorObjectAddArgs;

typedef struct EditorObjectIdResult {
    ErrorResultKind kind;
    union {
        EditorObjectId value;
        EditorError error;
    } result;
} EditorObjectIdResult;

EditorObjectIdResult editor_object_command_add(EditorProject *project,
    const EditorObjectAddArgs *arguments);
EditorResult editor_object_command_remove(EditorProject *project,
    EditorObjectId object);
EditorResult editor_object_command_rename(EditorProject *project,
    EditorObjectId object, const char *name);
EditorObject *editor_object_query_get(EditorProject *project, EditorObjectId object);
const EditorObject *editor_object_query_const_get(const EditorProject *project,
    EditorObjectId object);

#endif
