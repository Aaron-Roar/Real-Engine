#ifndef ROHR_EDITOR_NOTIFICATION_PANEL_H
#define ROHR_EDITOR_NOTIFICATION_PANEL_H

#include "rohr.h"

#define EDITOR_NOTIFICATION_SUMMARY_MAX 160
#define EDITOR_NOTIFICATION_DETAIL_MAX 2048

typedef struct EditorNotificationPanel {
    bool active;
    bool report_open;
    char summary[EDITOR_NOTIFICATION_SUMMARY_MAX];
    char detail[EDITOR_NOTIFICATION_DETAIL_MAX];
    TextAsset summary_text;
    TextAsset detail_text;
    TextAsset report_title;
    TextAsset close_label;
} EditorNotificationPanel;

bool editor_notification_panel_create(EditorNotificationPanel *panel,
    const FontAsset *font);
void editor_notification_panel_push(EditorNotificationPanel *panel,
    const char *summary, const char *detail);
void editor_notification_panel_toast_draw(EditorNotificationPanel *panel,
    float screen_height);
void editor_notification_panel_report_draw(EditorNotificationPanel *panel,
    UIRect bounds);
void editor_notification_panel_destroy(EditorNotificationPanel *panel);

#endif
