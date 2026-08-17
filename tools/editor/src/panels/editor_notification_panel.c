#include "editor_notification_panel.h"

#include <stdio.h>

static bool editor_notification_text_create(const FontAsset *font,
        const char *value, TextAsset *text) {
    TextAssetResult result = rohr_graphics_text_create(font, value,
        (Color){238, 241, 247, 255});
    if(rohr_error_check(result)) return false;
    *text = result.result.value;
    return true;
}

bool editor_notification_panel_create(EditorNotificationPanel *panel,
        const FontAsset *font) {
    if(panel == NULL || font == NULL) return false;
    *panel = (EditorNotificationPanel){0};
    return editor_notification_text_create(font, "", &panel->summary_text) &&
        editor_notification_text_create(font, "", &panel->detail_text) &&
        editor_notification_text_create(font, "Notification Report",
            &panel->report_title) &&
        editor_notification_text_create(font, "Close", &panel->close_label);
}

void editor_notification_panel_push(EditorNotificationPanel *panel,
        const char *summary, const char *detail) {
    if(panel == NULL || summary == NULL || detail == NULL) return;
    snprintf(panel->summary, sizeof(panel->summary), "%s", summary);
    snprintf(panel->detail, sizeof(panel->detail), "%s", detail);
    (void)rohr_graphics_text_value_set(&panel->summary_text, panel->summary);
    (void)rohr_graphics_text_value_set(&panel->detail_text, panel->detail);
    panel->active = true;
    panel->report_open = false;
}

void editor_notification_panel_toast_draw(EditorNotificationPanel *panel,
        float screen_height) {
    UIRect bounds = {14.0f, screen_height - 68.0f, 360.0f, 52.0f};
    if(panel == NULL || !panel->active || panel->report_open) return;
    rohr_ui_modal_controls_begin();
    rohr_ui_surface(bounds, (Color){91, 32, 38, 245});
    rohr_ui_border(bounds, 2.0f, (Color){22, 5, 8, 255});
    if(rohr_ui_button("editor.notification.toast", &panel->summary_text,
            bounds, &(UIButtonStyle){
                .idle = {91, 32, 38, 0},
                .hovered = {124, 43, 51, 180},
                .pressed = {70, 22, 27, 210},
                .disabled = {70, 70, 70, 180}
            }).clicked) panel->report_open = true;
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
        panel->active = false;
    }
    rohr_ui_modal_controls_end();
}

void editor_notification_panel_destroy(EditorNotificationPanel *panel) {
    if(panel == NULL) return;
    rohr_graphics_text_destroy(&panel->summary_text);
    rohr_graphics_text_destroy(&panel->detail_text);
    rohr_graphics_text_destroy(&panel->report_title);
    rohr_graphics_text_destroy(&panel->close_label);
    *panel = (EditorNotificationPanel){0};
}
