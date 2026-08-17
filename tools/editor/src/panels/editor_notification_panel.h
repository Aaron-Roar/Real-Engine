#ifndef ROHR_EDITOR_NOTIFICATION_PANEL_H
#define ROHR_EDITOR_NOTIFICATION_PANEL_H

#include "rohr.h"
#include "editor_notification_store.h"

#include <stdint.h>

typedef struct EditorNotificationPanel {
    EditorNotificationStore store;
    TextAsset summary_texts[EDITOR_NOTIFICATION_LOG_MAX];
    TextAsset toast_texts[EDITOR_NOTIFICATION_LOG_MAX];
    uint64_t selected_id;
    bool report_open;
    bool log_open;
    float log_scroll_offset;
    float report_scroll_offset;
    const FontAsset *font;
    const FontAsset *toast_font;
    TextAsset detail_text;
    TextAsset report_title;
    TextAsset log_label;
    TextAsset log_title;
    TextAsset close_label;
} EditorNotificationPanel;

bool editor_notification_panel_create(EditorNotificationPanel *panel,
    const FontAsset *font, const FontAsset *toast_font);
void editor_notification_panel_push(EditorNotificationPanel *panel,
    const char *summary, const char *detail);
void editor_notification_panel_toast_draw(EditorNotificationPanel *panel,
    float screen_height);
void editor_notification_panel_log_draw(EditorNotificationPanel *panel,
    UIRect bounds);
void editor_notification_panel_report_draw(EditorNotificationPanel *panel,
    UIRect bounds);
void editor_notification_panel_destroy(EditorNotificationPanel *panel);

#endif
