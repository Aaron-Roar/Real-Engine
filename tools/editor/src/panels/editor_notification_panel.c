#include "editor_notification_panel.h"

#include <stdio.h>

static const UIButtonStyle notification_style = {
    .idle = {91, 32, 38, 255},
    .hovered = {124, 43, 51, 255},
    .pressed = {70, 22, 27, 255},
    .disabled = {70, 70, 70, 255}
};

static bool editor_notification_text_create(const FontAsset *font,
        const char *value, TextAsset *text) {
    TextAssetResult result = rohr_graphics_text_create(font, value,
        (Color){238, 241, 247, 255});
    if(rohr_error_check(result)) return false;
    *text = result.result.value;
    return true;
}

static EditorNotificationEntry *editor_notification_entry_get(
        EditorNotificationPanel *panel, uint64_t id) {
    if(panel == NULL || id == 0) return NULL;
    for(size_t i = 0; i < panel->entry_count; i += 1) {
        size_t index = (panel->entry_start + i) % EDITOR_NOTIFICATION_LOG_MAX;
        if(panel->entries[index].id == id) return &panel->entries[index];
    }
    return NULL;
}

static void editor_notification_select(EditorNotificationPanel *panel,
        uint64_t id) {
    EditorNotificationEntry *entry = editor_notification_entry_get(panel, id);
    if(entry == NULL) return;
    panel->selected_id = id;
    (void)rohr_graphics_text_value_set(&panel->report_title, entry->summary);
    (void)rohr_graphics_text_value_set(&panel->detail_text, entry->detail);
    panel->log_open = false;
    panel->report_open = true;
}

static void editor_notification_toast_remove(EditorNotificationPanel *panel,
        uint64_t id) {
    size_t write = 0;
    for(size_t i = 0; i < panel->toast_count; i += 1) {
        if(panel->toast_ids[i] != id)
            panel->toast_ids[write++] = panel->toast_ids[i];
    }
    panel->toast_count = write;
}

bool editor_notification_panel_create(EditorNotificationPanel *panel,
        const FontAsset *font) {
    if(panel == NULL || font == NULL) return false;
    *panel = (EditorNotificationPanel){0};
    panel->font = font;
    if(editor_notification_text_create(font, "", &panel->detail_text) &&
            editor_notification_text_create(font, "Notification Report",
                &panel->report_title) &&
            editor_notification_text_create(font, "Notification Log",
                &panel->log_label) &&
            editor_notification_text_create(font, "Notification Log",
                &panel->log_title) &&
            editor_notification_text_create(font, "Close", &panel->close_label))
        return true;
    editor_notification_panel_destroy(panel);
    return false;
}

void editor_notification_panel_push(EditorNotificationPanel *panel,
        const char *summary, const char *detail) {
    EditorNotificationEntry *entry;
    TextAsset summary_text = {0};
    size_t index;
    if(panel == NULL || panel->font == NULL || summary == NULL || detail == NULL)
        return;
    if(!editor_notification_text_create(panel->font, summary, &summary_text)) return;
    if(panel->entry_count < EDITOR_NOTIFICATION_LOG_MAX) {
        index = (panel->entry_start + panel->entry_count) %
            EDITOR_NOTIFICATION_LOG_MAX;
        panel->entry_count += 1;
    } else {
        index = panel->entry_start;
        panel->entry_start = (panel->entry_start + 1) % EDITOR_NOTIFICATION_LOG_MAX;
        rohr_graphics_text_destroy(&panel->entries[index].summary_text);
    }
    entry = &panel->entries[index];
    *entry = (EditorNotificationEntry){0};
    panel->next_id += 1;
    if(panel->next_id == 0) panel->next_id = 1;
    entry->id = panel->next_id;
    snprintf(entry->summary, sizeof(entry->summary), "%s", summary);
    snprintf(entry->detail, sizeof(entry->detail), "%s", detail);
    entry->summary_text = summary_text;
    if(panel->toast_count < EDITOR_NOTIFICATION_TOAST_MAX) {
        panel->toast_ids[panel->toast_count++] = entry->id;
    } else {
        for(size_t i = 1; i < EDITOR_NOTIFICATION_TOAST_MAX; i += 1)
            panel->toast_ids[i - 1] = panel->toast_ids[i];
        panel->toast_ids[EDITOR_NOTIFICATION_TOAST_MAX - 1] = entry->id;
    }
}

void editor_notification_panel_toast_draw(EditorNotificationPanel *panel,
        float screen_height) {
    UIRect log_bounds = {14.0f, screen_height - 42.0f, 190.0f, 28.0f};
    if(panel == NULL) return;
    rohr_ui_modal_controls_begin();
    if(rohr_ui_button("editor.notification.log", &panel->log_label,
            log_bounds, NULL).clicked) {
        panel->report_open = false;
        panel->log_open = true;
    }
    if(!panel->report_open && !panel->log_open) {
        for(size_t slot = 0; slot < panel->toast_count; slot += 1) {
            size_t toast_index = panel->toast_count - slot - 1;
            EditorNotificationEntry *entry = editor_notification_entry_get(panel,
                panel->toast_ids[toast_index]);
            char id[64];
            UIRect bounds = {14.0f, log_bounds.y - 60.0f - 60.0f * (float)slot,
                360.0f, 52.0f};
            if(entry == NULL) continue;
            snprintf(id, sizeof(id), "editor.notification.toast.%llu",
                (unsigned long long)entry->id);
            if(rohr_ui_button(id, &entry->summary_text, bounds,
                    &notification_style).clicked)
                editor_notification_select(panel, entry->id);
        }
    }
    rohr_ui_modal_controls_end();
}

void editor_notification_panel_log_draw(EditorNotificationPanel *panel,
        UIRect bounds) {
    UIRect list_bounds;
    if(panel == NULL || !panel->log_open) return;
    rohr_ui_modal_controls_begin();
    rohr_ui_surface(bounds, (Color){37, 42, 52, 255});
    rohr_ui_border(bounds, 2.0f, (Color){5, 6, 8, 255});
    rohr_ui_label(&panel->log_title,
        (UIRect){bounds.x + 20.0f, bounds.y + 14.0f, bounds.width - 40.0f, 32.0f});
    list_bounds = (UIRect){bounds.x + 20.0f, bounds.y + 54.0f,
        bounds.width - 40.0f, bounds.height - 112.0f};
    panel->log_scroll_offset = rohr_ui_scroll_region_begin(
        "editor.notification.log.scroll", list_bounds,
        38.0f * (float)panel->entry_count, panel->log_scroll_offset, 38.0f).offset;
    for(size_t row = 0; row < panel->entry_count; row += 1) {
        size_t logical = panel->entry_count - row - 1;
        size_t index = (panel->entry_start + logical) % EDITOR_NOTIFICATION_LOG_MAX;
        EditorNotificationEntry *entry = &panel->entries[index];
        char id[64];
        UIRect row_bounds = {list_bounds.x, list_bounds.y + 38.0f * (float)row,
            list_bounds.width, 32.0f};
        snprintf(id, sizeof(id), "editor.notification.log.entry.%llu",
            (unsigned long long)entry->id);
        if(rohr_ui_button(id, &entry->summary_text, row_bounds,
                &notification_style).clicked)
            editor_notification_select(panel, entry->id);
    }
    rohr_ui_scroll_region_end();
    if(rohr_ui_button("editor.notification.log.close", &panel->close_label,
            (UIRect){bounds.x + bounds.width - 124.0f,
                bounds.y + bounds.height - 48.0f, 100.0f, 32.0f}, NULL).clicked)
        panel->log_open = false;
    rohr_ui_modal_controls_end();
}

void editor_notification_panel_report_draw(EditorNotificationPanel *panel,
        UIRect bounds) {
    UIRect detail_bounds;
    if(panel == NULL || !panel->report_open) return;
    rohr_ui_modal_controls_begin();
    rohr_ui_surface(bounds, (Color){37, 42, 52, 255});
    rohr_ui_border(bounds, 2.0f, (Color){5, 6, 8, 255});
    rohr_ui_label(&panel->report_title,
        (UIRect){bounds.x + 20.0f, bounds.y + 14.0f, bounds.width - 40.0f, 32.0f});
    detail_bounds = (UIRect){bounds.x + 24.0f, bounds.y + 60.0f,
        bounds.width - 48.0f, bounds.height - 124.0f};
    rohr_ui_surface(detail_bounds, (Color){27, 31, 39, 255});
    rohr_ui_border(detail_bounds, 1.0f, (Color){10, 12, 16, 255});
    if(panel->detail_text.text != NULL)
        (void)TTF_SetTextWrapWidth(panel->detail_text.text,
            (int)(detail_bounds.width - 20.0f));
    if(rohr_ui_clip_begin(detail_bounds)) {
        (void)rohr_graphics_text_draw(&panel->detail_text,
            (Position){detail_bounds.x + 10.0f, detail_bounds.y + 10.0f});
        rohr_ui_clip_end();
    }
    if(rohr_ui_button("editor.notification.close", &panel->close_label,
            (UIRect){bounds.x + bounds.width - 124.0f,
                bounds.y + bounds.height - 48.0f, 100.0f, 32.0f}, NULL).clicked) {
        panel->report_open = false;
        editor_notification_toast_remove(panel, panel->selected_id);
    }
    rohr_ui_modal_controls_end();
}

void editor_notification_panel_destroy(EditorNotificationPanel *panel) {
    if(panel == NULL) return;
    for(size_t i = 0; i < EDITOR_NOTIFICATION_LOG_MAX; i += 1)
        rohr_graphics_text_destroy(&panel->entries[i].summary_text);
    rohr_graphics_text_destroy(&panel->detail_text);
    rohr_graphics_text_destroy(&panel->report_title);
    rohr_graphics_text_destroy(&panel->log_label);
    rohr_graphics_text_destroy(&panel->log_title);
    rohr_graphics_text_destroy(&panel->close_label);
    *panel = (EditorNotificationPanel){0};
}
