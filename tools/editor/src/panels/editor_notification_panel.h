#ifndef ROHR_EDITOR_NOTIFICATION_PANEL_H
#define ROHR_EDITOR_NOTIFICATION_PANEL_H

#include "rohr.h"

#include <stdint.h>

#define EDITOR_NOTIFICATION_SUMMARY_MAX 160
#define EDITOR_NOTIFICATION_DETAIL_MAX 2048
#define EDITOR_NOTIFICATION_LOG_MAX 100
#define EDITOR_NOTIFICATION_TOAST_MAX 3

typedef struct EditorNotificationEntry {
    uint64_t id;
    char summary[EDITOR_NOTIFICATION_SUMMARY_MAX];
    char detail[EDITOR_NOTIFICATION_DETAIL_MAX];
    TextAsset summary_text;
} EditorNotificationEntry;

typedef struct EditorNotificationPanel {
    EditorNotificationEntry entries[EDITOR_NOTIFICATION_LOG_MAX];
    size_t entry_start;
    size_t entry_count;
    uint64_t next_id;
    uint64_t toast_ids[EDITOR_NOTIFICATION_TOAST_MAX];
    size_t toast_count;
    uint64_t selected_id;
    bool report_open;
    bool log_open;
    float log_scroll_offset;
    const FontAsset *font;
    TextAsset detail_text;
    TextAsset report_title;
    TextAsset log_label;
    TextAsset log_title;
    TextAsset close_label;
} EditorNotificationPanel;

bool editor_notification_panel_create(EditorNotificationPanel *panel,
    const FontAsset *font);
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
