#ifndef ROHR_EDITOR_BUILD_NOTIFICATIONS_H
#define ROHR_EDITOR_BUILD_NOTIFICATIONS_H

#include "editor_project.h"
#include "panels/editor_notification_panel.h"

void editor_build_notification_codegen_success(EditorNotificationPanel *system,
    const EditorProject *project, bool tree_shown);
void editor_build_notification_compile_success(EditorNotificationPanel *system,
    const char *project_directory, bool terminal_output_shown);
void editor_build_notification_process_failure(EditorNotificationPanel *system,
    const char *stage, int exit_code, bool terminal_output_shown);
void editor_build_notification_start_failure(EditorNotificationPanel *system,
    const char *operation, const char *reason);
void editor_build_notification_configuration_failure(
    EditorNotificationPanel *system, const char *parser_error,
    const char *lua_error);

#endif
