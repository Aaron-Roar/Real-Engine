/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "panels/editor_build_notifications.h"

#include "editor_workspace.h"

#include <stdio.h>

void editor_build_notification_codegen_success(EditorNotificationPanel *system,
        const EditorProject *project, bool tree_shown) {
    char detail[512];
    if(system == NULL || project == NULL) return;
    snprintf(detail, sizeof(detail),
        "Generated 2 C files:\n"
        "- src/generated/project_objects.h\n"
        "- src/generated/project_objects.c\n\n"
        "Generated %zu object%s.%s",
        project->object_count, project->object_count == 1 ? "" : "s",
        tree_shown ? " The generated object tree is shown in the terminal." :
            " Terminal tree output is disabled.");
    editor_notification_panel_push(system, "Generate C - SUCCESS", detail);
}

void editor_build_notification_compile_success(EditorNotificationPanel *system,
        const char *project_directory, bool terminal_output_shown) {
    char detail[EDITOR_WORKSPACE_PATH_MAX + 256];
    if(system == NULL || project_directory == NULL) return;
    snprintf(detail, sizeof(detail),
        "Project configuration and compilation completed successfully.\n\n"
        "Project: %s\n%s", project_directory,
        terminal_output_shown ?
            "The executed commands and complete build output are shown in the terminal." :
            "Build-operation terminal output is disabled.");
    editor_notification_panel_push(system, "Compile project - SUCCESS", detail);
}

void editor_build_notification_process_failure(EditorNotificationPanel *system,
        const char *stage, int exit_code, bool terminal_output_shown) {
    char detail[320];
    if(system == NULL || stage == NULL) return;
    snprintf(detail, sizeof(detail), "%s command exited with status %d.%s",
        stage, exit_code, terminal_output_shown ?
            "\n\nSee the terminal output for the complete build report." : "");
    editor_notification_panel_push(system, "Build project - FAIL", detail);
}

void editor_build_notification_start_failure(EditorNotificationPanel *system,
        const char *operation, const char *reason) {
    char summary[EDITOR_NOTIFICATION_SUMMARY_MAX];
    if(system == NULL || operation == NULL || reason == NULL) return;
    snprintf(summary, sizeof(summary), "%s - FAIL", operation);
    editor_notification_panel_push(system, summary, reason);
}

void editor_build_notification_configuration_failure(
        EditorNotificationPanel *system, const char *parser_error,
        const char *lua_error) {
    char detail[EDITOR_NOTIFICATION_DETAIL_MAX];
    if(system == NULL || parser_error == NULL) return;
    snprintf(detail, sizeof(detail), "Parser error:\n%s\n\nLua error:\n%s",
        parser_error, lua_error == NULL || lua_error[0] == '\0' ?
            "No Lua runtime error." : lua_error);
    editor_notification_panel_push(system,
        "Build configuration (GUI) - FAIL", detail);
}
