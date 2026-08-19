#include "panels/editor_error_notifications.h"

#include <stdio.h>

void editor_error_notification_failure(EditorNotificationPanel *system,
        const char *operation, EditorResult result) {
    char summary[EDITOR_NOTIFICATION_SUMMARY_MAX];
    char detail[EDITOR_NOTIFICATION_DETAIL_MAX];

    if(system == NULL || operation == NULL || !editor_result_check(result)) return;
    snprintf(summary, sizeof(summary), "%s - FAIL", operation);
    snprintf(detail, sizeof(detail), "Editor error code: %d\n\n%s",
        (int)result.result.error.code,
        result.result.error.message[0] == '\0' ?
            "No detailed editor error was provided." : result.result.error.message);
    editor_notification_panel_push(system, summary, detail);
}
