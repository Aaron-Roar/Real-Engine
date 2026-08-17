#include "editor_notification_panel.h"
#include "editor_notification_layout.h"

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

static void editor_notification_select(EditorNotificationPanel *panel,
        uint64_t id) {
    EditorNotificationRecord *entry = editor_notification_store_id_get(
        &panel->store, id);
    if(entry == NULL) return;
    panel->selected_id = id;
    (void)rohr_graphics_text_value_set(&panel->report_title, entry->summary);
    (void)rohr_graphics_text_value_set(&panel->detail_text, entry->detail);
    panel->report_scroll_offset = 0.0f;
    panel->log_open = false;
    panel->report_open = true;
}

static void editor_notification_toast_remove(EditorNotificationPanel *panel,
        uint64_t id) {
    editor_notification_store_toast_remove(&panel->store, id);
}

bool editor_notification_panel_create(EditorNotificationPanel *panel,
        const FontAsset *font, const FontAsset *toast_font) {
    if(panel == NULL || font == NULL || toast_font == NULL) return false;
    *panel = (EditorNotificationPanel){0};
    panel->font = font;
    panel->toast_font = toast_font;
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
    TextAsset summary_text = {0};
    TextAsset toast_text = {0};
    size_t index;
    bool replaced;
    if(panel == NULL || panel->font == NULL || panel->toast_font == NULL ||
            summary == NULL || detail == NULL)
        return;
    if(!editor_notification_text_create(panel->font, summary, &summary_text)) return;
    if(!editor_notification_text_create(panel->toast_font, summary, &toast_text)) {
        rohr_graphics_text_destroy(&summary_text);
        return;
    }
    if(!editor_notification_store_push(&panel->store, summary, detail,
            SDL_GetTicks(), &index, &replaced)) {
        rohr_graphics_text_destroy(&summary_text);
        rohr_graphics_text_destroy(&toast_text);
        return;
    }
    if(replaced) {
        rohr_graphics_text_destroy(&panel->summary_texts[index]);
        rohr_graphics_text_destroy(&panel->toast_texts[index]);
    }
    panel->summary_texts[index] = summary_text;
    panel->toast_texts[index] = toast_text;
}

void editor_notification_panel_toast_draw(EditorNotificationPanel *panel,
        float screen_height) {
    UIRect log_bounds = {EDITOR_NOTIFICATION_LEFT,
        screen_height - EDITOR_NOTIFICATION_BOTTOM -
            EDITOR_NOTIFICATION_LOG_HEIGHT,
        EDITOR_NOTIFICATION_WIDTH, EDITOR_NOTIFICATION_LOG_HEIGHT};
    if(panel == NULL) return;
    editor_notification_store_toasts_expire(&panel->store, SDL_GetTicks(),
        EDITOR_NOTIFICATION_TOAST_LIFETIME_MS);
    rohr_ui_modal_controls_begin();
    if(rohr_ui_button("editor.notification.log", &panel->log_label,
            log_bounds, NULL).clicked) {
        panel->report_open = false;
        panel->log_open = true;
    }
    if(!panel->report_open && !panel->log_open) {
        for(size_t slot = 0; slot < panel->store.toast_count; slot += 1) {
            size_t toast_index = panel->store.toast_count - slot - 1;
            EditorNotificationRecord *entry = editor_notification_store_id_get(
                &panel->store, panel->store.toast_ids[toast_index]);
            size_t index;
            char id[64];
            UIRect bounds = {log_bounds.x,
                log_bounds.y - EDITOR_NOTIFICATION_TOAST_GAP -
                    EDITOR_NOTIFICATION_TOAST_HEIGHT -
                    (EDITOR_NOTIFICATION_TOAST_HEIGHT +
                        EDITOR_NOTIFICATION_TOAST_GAP) * (float)slot,
                log_bounds.width, EDITOR_NOTIFICATION_TOAST_HEIGHT};
            if(entry == NULL) continue;
            index = (size_t)(entry - panel->store.entries);
            snprintf(id, sizeof(id), "editor.notification.toast.%llu",
                (unsigned long long)entry->id);
            if(rohr_ui_button(id, &panel->toast_texts[index], bounds,
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
        EDITOR_NOTIFICATION_LOG_ROW_HEIGHT * (float)panel->store.entry_count,
        panel->log_scroll_offset, EDITOR_NOTIFICATION_SCROLL_STEP).offset;
    for(size_t row = 0; row < panel->store.entry_count; row += 1) {
        EditorNotificationRecord *entry = editor_notification_store_newest_get(
            &panel->store, row);
        size_t index = (size_t)(entry - panel->store.entries);
        char id[64];
        UIRect row_bounds = {list_bounds.x,
            list_bounds.y + EDITOR_NOTIFICATION_LOG_ROW_HEIGHT * (float)row,
            list_bounds.width, EDITOR_NOTIFICATION_LOG_ROW_BUTTON_HEIGHT};
        snprintf(id, sizeof(id), "editor.notification.log.entry.%llu",
            (unsigned long long)entry->id);
        if(rohr_ui_button(id, &panel->summary_texts[index], row_bounds,
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
    float content_height = 0.0f;
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
    if(panel->detail_text.text != NULL) {
        int text_width;
        int text_height;
        (void)TTF_SetTextWrapWidth(panel->detail_text.text,
            (int)(detail_bounds.width - 20.0f));
        if(TTF_GetTextSize(panel->detail_text.text, &text_width, &text_height))
            content_height = (float)text_height + 20.0f;
    }
    panel->report_scroll_offset = rohr_ui_scroll_region_begin(
        "editor.notification.report.scroll", detail_bounds, content_height,
        panel->report_scroll_offset, EDITOR_NOTIFICATION_SCROLL_STEP).offset;
    if(panel->detail_text.text != NULL)
        (void)rohr_graphics_text_draw(&panel->detail_text,
            (Position){detail_bounds.x + 10.0f,
                detail_bounds.y + 10.0f - panel->report_scroll_offset});
    rohr_ui_scroll_region_end();
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
    for(size_t i = 0; i < EDITOR_NOTIFICATION_LOG_MAX; i += 1) {
        rohr_graphics_text_destroy(&panel->summary_texts[i]);
        rohr_graphics_text_destroy(&panel->toast_texts[i]);
    }
    rohr_graphics_text_destroy(&panel->detail_text);
    rohr_graphics_text_destroy(&panel->report_title);
    rohr_graphics_text_destroy(&panel->log_label);
    rohr_graphics_text_destroy(&panel->log_title);
    rohr_graphics_text_destroy(&panel->close_label);
    *panel = (EditorNotificationPanel){0};
}
