#include "editor_document.h"
#include "editor_object_commands.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "/tmp/rohr-editor-core-test.json";
    EditorDocument document;
    EditorDocument loaded;
    EditorObjectIdResult added;
    EditorResult result;

    result = editor_document_create(&document);
    if(editor_result_check(result)) return 1;
    added = editor_object_command_add(document.project,
        &(EditorObjectAddArgs){
            .name = "fast car",
            .position = {12.0f, 34.0f}
        });
    if(added.kind != ERROR_RESULT_VALUE) return 1;
    result = editor_object_command_rename(
        document.project, added.result.value, "faster car");
    if(editor_result_check(result)) return 1;
    result = editor_document_save_as(&document, path);
    if(editor_result_check(result)) return 1;

    result = editor_document_create(&loaded);
    if(editor_result_check(result)) return 1;
    result = editor_document_load(&loaded, path);
    if(editor_result_check(result) || loaded.project->object_count != 1 ||
            strcmp(loaded.project->objects[0].name, "FasterCar") != 0 ||
            loaded.project->objects[0].position.x != 12.0f ||
            loaded.project->objects[0].position.y != 34.0f) return 1;
    result = editor_object_command_remove(loaded.project, added.result.value);
    if(editor_result_check(result) || loaded.project->object_count != 0) return 1;
    editor_document_destroy(&loaded);
    editor_document_destroy(&document);
    (void)remove(path);
    return 0;
}
