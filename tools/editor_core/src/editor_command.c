#include "editor_command.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EditorCommandResult editor_command_error(EditorError error) {
    return (EditorCommandResult){.kind = ERROR_RESULT_ERROR,
        .result.error = error};
}

static EditorCommandResult editor_command_result_from(EditorResult result) {
    if(editor_result_check(result)) return editor_command_error(result.result.error);
    return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
}

EditorCommandResult editor_command_execute(EditorProject *project,
        const EditorCommand *command) {
    if(project == NULL || command == NULL)
        return editor_command_error((EditorError){EDITOR_ERROR_INVALID_ARGUMENT,
            "command execution requires a project and command"});
    switch(command->type) {
        case EDITOR_COMMAND_OBJECT_ADD: {
            EditorObjectIdResult result = editor_object_command_add(project,
                &(EditorObjectAddArgs){
                    .name = command->data.object_add.name,
                    .position = command->data.object_add.position
                });
            if(result.kind == ERROR_RESULT_ERROR)
                return editor_command_error(result.result.error);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                .result.object = result.result.value};
        }
        case EDITOR_COMMAND_OBJECT_RENAME:
            return editor_command_result_from(editor_object_command_rename(project,
                command->data.object_rename.object,
                command->data.object_rename.name));
        case EDITOR_COMMAND_OBJECT_REMOVE:
            return editor_command_result_from(editor_object_command_remove(project,
                command->data.object_remove.object));
    }
    return editor_command_error((EditorError){EDITOR_ERROR_INVALID_ARGUMENT,
        "unknown editor command"});
}

static bool editor_command_uint_parse(const char *text, uint32_t *value) {
    char *end;
    unsigned long parsed;
    if(text == NULL || value == NULL || text[0] == '\0') return false;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if(errno != 0 || *end != '\0' || parsed > UINT32_MAX) return false;
    *value = (uint32_t)parsed;
    return true;
}

static bool editor_command_float_parse(const char *text, float *value) {
    char *end;
    float parsed;
    if(text == NULL || value == NULL || text[0] == '\0') return false;
    errno = 0;
    parsed = strtof(text, &end);
    if(errno != 0 || *end != '\0') return false;
    *value = parsed;
    return true;
}

EditorResult editor_command_cli_parse(int count, char **arguments,
        const char **document_path, EditorCommand *command) {
    const char *action;
    if(arguments == NULL || document_path == NULL || command == NULL || count < 4 ||
            strcmp(arguments[1], "object") != 0)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "expected an object mutation command");
    action = arguments[2];
    *document_path = arguments[3];
    memset(command, 0, sizeof(*command));
    if(strcmp(action, "add") == 0) {
        if(count != 5 && count != 7)
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "object add expects <file> <name> [x y]");
        command->type = EDITOR_COMMAND_OBJECT_ADD;
        snprintf(command->data.object_add.name,
            sizeof(command->data.object_add.name), "%s", arguments[4]);
        if(count == 7 && (!editor_command_float_parse(arguments[5],
                    &command->data.object_add.position.x) ||
                !editor_command_float_parse(arguments[6],
                    &command->data.object_add.position.y)))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "object position must contain valid numbers");
        return editor_result_value(true);
    }
    if(strcmp(action, "rename") == 0) {
        if(count != 6 || !editor_command_uint_parse(arguments[4],
                &command->data.object_rename.object))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "object rename expects <file> <id> <name>");
        command->type = EDITOR_COMMAND_OBJECT_RENAME;
        snprintf(command->data.object_rename.name,
            sizeof(command->data.object_rename.name), "%s", arguments[5]);
        return editor_result_value(true);
    }
    if(strcmp(action, "delete") == 0) {
        if(count != 5 || !editor_command_uint_parse(arguments[4],
                &command->data.object_remove.object))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "object delete expects <file> <id>");
        command->type = EDITOR_COMMAND_OBJECT_REMOVE;
        return editor_result_value(true);
    }
    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "unknown object mutation '%s'", action);
}

static bool editor_command_text_append(char *output, size_t capacity,
        size_t *used, const char *text) {
    size_t length = strlen(text);
    if(*used >= capacity || length >= capacity - *used) return false;
    memcpy(output + *used, text, length);
    *used += length;
    output[*used] = '\0';
    return true;
}

static bool editor_command_shell_text_append(char *output, size_t capacity,
        size_t *used, const char *text) {
    if(!editor_command_text_append(output, capacity, used, "'")) return false;
    for(const char *at = text; *at != '\0'; at += 1) {
        const char *part = *at == '\'' ? "'\\''" : NULL;
        char character[2] = {*at, '\0'};
        if(!editor_command_text_append(output, capacity, used,
                part != NULL ? part : character)) return false;
    }
    return editor_command_text_append(output, capacity, used, "'");
}

EditorResult editor_command_cli_write(const EditorCommand *command,
        const char *document_path, char *output, size_t output_capacity) {
    char values[128];
    size_t used = 0;
    if(command == NULL || document_path == NULL || output == NULL ||
            output_capacity == 0)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "command serialization requires a command, path, and output buffer");
    output[0] = '\0';
    if(!editor_command_text_append(output, output_capacity, &used, "editor-cli object "))
        goto capacity_error;
    switch(command->type) {
        case EDITOR_COMMAND_OBJECT_ADD:
            if(!editor_command_text_append(output, output_capacity, &used, "add ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path) ||
                    !editor_command_text_append(output, output_capacity, &used, " ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        command->data.object_add.name)) goto capacity_error;
            snprintf(values, sizeof(values), " %.9g %.9g",
                command->data.object_add.position.x,
                command->data.object_add.position.y);
            if(!editor_command_text_append(output, output_capacity, &used, values))
                goto capacity_error;
            return editor_result_value(true);
        case EDITOR_COMMAND_OBJECT_RENAME:
            snprintf(values, sizeof(values), "rename ");
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u ",
                command->data.object_rename.object);
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        command->data.object_rename.name)) goto capacity_error;
            return editor_result_value(true);
        case EDITOR_COMMAND_OBJECT_REMOVE:
            if(!editor_command_text_append(output, output_capacity, &used, "delete ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u",
                command->data.object_remove.object);
            if(!editor_command_text_append(output, output_capacity, &used, values))
                goto capacity_error;
            return editor_result_value(true);
    }
    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "unknown editor command");

capacity_error:
    return editor_result_error(EDITOR_ERROR_CAPACITY,
        "CLI command output buffer is too small");
}
