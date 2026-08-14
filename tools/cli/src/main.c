#include "editor_document.h"
#include "editor_object_commands.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cli_error(EditorResult result) {
    editor_result_stderr_print(result);
    return 1;
}

static bool cli_uint_parse(const char *text, uint32_t *value) {
    char *end;
    unsigned long parsed;
    if(text == NULL || value == NULL || text[0] == '\0') return false;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if(errno != 0 || *end != '\0' || parsed > UINT32_MAX) return false;
    *value = (uint32_t)parsed;
    return true;
}

static bool cli_float_parse(const char *text, float *value) {
    char *end;
    float parsed;
    if(text == NULL || value == NULL || text[0] == '\0') return false;
    errno = 0;
    parsed = strtof(text, &end);
    if(errno != 0 || *end != '\0') return false;
    *value = parsed;
    return true;
}

static void cli_usage_print(void) {
    puts("usage:\n"
        "  rohr project validate <project.rohr.json>\n"
        "  rohr object list <project.rohr.json>\n"
        "  rohr object add <project.rohr.json> <name> [x y]\n"
        "  rohr object rename <project.rohr.json> <id> <name>\n"
        "  rohr object delete <project.rohr.json> <id>");
}

static int cli_object_command(int count, char **arguments) {
    EditorDocument document;
    EditorResult result;
    const char *action;
    const char *path;
    if(count < 4) {
        cli_usage_print();
        return 1;
    }
    action = arguments[2];
    path = arguments[3];
    result = editor_document_create(&document);
    if(editor_result_check(result)) return cli_error(result);
    result = editor_document_load(&document, path);
    if(editor_result_check(result)) return cli_error(result);
    if(strcmp(action, "list") == 0) {
        const EditorProject *project = editor_document_project_const_get(&document);
        for(size_t i = 0; i < project->object_count; i += 1)
            printf("%u\t%s\n", project->objects[i].id, project->objects[i].name);
        return 0;
    }
    if(strcmp(action, "add") == 0) {
        EditorObjectAddArgs add;
        EditorObjectIdResult add_result;
        if(count != 5 && count != 7) {
            cli_usage_print();
            return 1;
        }
        add = (EditorObjectAddArgs){.name = arguments[4]};
        if(count == 7 && (!cli_float_parse(arguments[5], &add.position.x) ||
                !cli_float_parse(arguments[6], &add.position.y))) {
            fputs("object position must contain valid numbers\n", stderr);
            return 1;
        }
        add_result = editor_object_command_add(document.project, &add);
        if(add_result.kind == ERROR_RESULT_ERROR) {
            EditorResult command_error = {.kind = ERROR_RESULT_ERROR,
                .result.error = add_result.result.error};
            return cli_error(command_error);
        }
        printf("added object %u\n", add_result.result.value);
    } else if(strcmp(action, "rename") == 0) {
        uint32_t object;
        if(count != 6 || !cli_uint_parse(arguments[4], &object)) {
            cli_usage_print();
            return 1;
        }
        result = editor_object_command_rename(
            document.project, object, arguments[5]);
        if(editor_result_check(result)) return cli_error(result);
    } else if(strcmp(action, "delete") == 0) {
        uint32_t object;
        if(count != 5 || !cli_uint_parse(arguments[4], &object)) {
            cli_usage_print();
            return 1;
        }
        result = editor_object_command_remove(document.project, object);
        if(editor_result_check(result)) return cli_error(result);
    } else {
        cli_usage_print();
        return 1;
    }
    document.dirty = true;
    result = editor_document_save(&document);
    return editor_result_check(result) ? cli_error(result) : 0;
}

int main(int count, char **arguments) {
    if(count == 4 && strcmp(arguments[1], "project") == 0 &&
            strcmp(arguments[2], "validate") == 0) {
        EditorDocument document;
        EditorResult result;
        result = editor_document_create(&document);
        if(editor_result_check(result)) return cli_error(result);
        result = editor_document_load(&document, arguments[3]);
        if(editor_result_check(result)) return cli_error(result);
        puts("valid");
        return 0;
    }
    if(count >= 2 && strcmp(arguments[1], "object") == 0)
        return cli_object_command(count, arguments);
    cli_usage_print();
    return 1;
}
