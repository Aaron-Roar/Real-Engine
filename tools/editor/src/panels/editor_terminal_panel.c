#include "editor_terminal_panel.h"

#include <stdio.h>
#include <string.h>

#define EDITOR_TERMINAL_DEFAULT_HEIGHT 220.0f
#define EDITOR_TERMINAL_DIVIDER_HEIGHT 5.0f
#define EDITOR_TERMINAL_LINE_HEIGHT 17.0f

static bool editor_terminal_text_create(FontAsset *font, TextAsset *text) {
    TextAssetResult result = rohr_graphics_text_create(
        font, "", (Color){230, 234, 242, 255});
    if(rohr_error_check(result)) return false;
    *text = result.result.value;
    return true;
}

static size_t editor_terminal_codepoint_write(char *output, size_t capacity,
        size_t used, uint32_t codepoint) {
    if(codepoint <= 0x7f && used + 1 < capacity) output[used++] = (char)codepoint;
    else if(codepoint <= 0x7ff && used + 2 < capacity) {
        output[used++] = (char)(0xc0 | (codepoint >> 6));
        output[used++] = (char)(0x80 | (codepoint & 0x3f));
    } else if(codepoint <= 0xffff && used + 3 < capacity) {
        output[used++] = (char)(0xe0 | (codepoint >> 12));
        output[used++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        output[used++] = (char)(0x80 | (codepoint & 0x3f));
    } else if(used + 4 < capacity) {
        output[used++] = (char)(0xf0 | (codepoint >> 18));
        output[used++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        output[used++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        output[used++] = (char)(0x80 | (codepoint & 0x3f));
    }
    return used;
}

bool editor_terminal_panel_create(EditorTerminalPanel *panel, FontAsset *font) {
    int glyph_advance = 0;
    if(panel == NULL || font == NULL) return false;
    memset(panel, 0, sizeof(*panel));
    panel->height = EDITOR_TERMINAL_DEFAULT_HEIGHT;
    if(font->font == NULL || !TTF_GetGlyphMetrics(font->font, 'M', NULL, NULL,
            NULL, NULL, &glyph_advance) || glyph_advance <= 0) return false;
    panel->cell_width = (float)glyph_advance;
    for(size_t i = 0; i < EDITOR_TERMINAL_VISIBLE_LINE_MAX; i += 1) {
        if(!editor_terminal_text_create(font, &panel->lines[i])) {
            editor_terminal_panel_destroy(panel);
            return false;
        }
    }
    return true;
}

bool editor_terminal_panel_project_open(EditorTerminalPanel *panel,
        const char *project_directory) {
    RohrTerminalConfig config = rohr_terminal_config_default_get();
    RohrTerminalResult result;
    if(panel == NULL || project_directory == NULL) return false;
    editor_terminal_panel_project_close(panel);
    config.working_directory = project_directory;
    result = rohr_terminal_create(&panel->terminal, &config);
    if(!result.success) {
        fprintf(stderr, "Could not start project terminal: %s\n",
            rohr_terminal_error_message_get(&result));
        return false;
    }
    panel->scroll_offset = 0;
    return true;
}

void editor_terminal_panel_project_close(EditorTerminalPanel *panel) {
    if(panel == NULL) return;
    rohr_terminal_destroy(panel->terminal);
    panel->terminal = NULL;
    panel->focused = false;
}

void editor_terminal_panel_visible_toggle(EditorTerminalPanel *panel) {
    if(panel == NULL) return;
    panel->visible = !panel->visible;
    if(!panel->visible) panel->focused = false;
}

bool editor_terminal_panel_focused_check(const EditorTerminalPanel *panel) {
    return panel != NULL && panel->visible && panel->focused;
}

static void editor_terminal_input_write(EditorTerminalPanel *panel,
        const char *text, size_t length) {
    RohrTerminalResult result;
    if(panel->terminal == NULL) return;
    result = rohr_terminal_input_write(panel->terminal, text, length);
    if(!result.success) fprintf(stderr, "Terminal input failed: %s\n",
        rohr_terminal_error_message_get(&result));
}

bool editor_terminal_panel_event_add(EditorTerminalPanel *panel,
        const SDL_Event *event, float viewport_width, float viewport_bottom) {
    Position pointer;
    bool inside;
    if(panel == NULL || event == NULL || !panel->visible) return false;
    pointer = rohr_graphics_mouse_screen_position_get();
    inside = pointer.x >= 0.0f && pointer.x < viewport_width &&
        pointer.y >= viewport_bottom;
    if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event->button.button == SDL_BUTTON_LEFT) {
        panel->focused = inside;
        if(panel->focused && SDL_GetKeyboardFocus() != NULL)
            (void)SDL_StartTextInput(SDL_GetKeyboardFocus());
        else if(SDL_GetKeyboardFocus() != NULL)
            (void)SDL_StopTextInput(SDL_GetKeyboardFocus());
    }
    if(event->type == SDL_EVENT_MOUSE_WHEEL && inside) {
        if(event->wheel.y > 0.0f) panel->scroll_offset += 3;
        else if(event->wheel.y < 0.0f && panel->scroll_offset > 0)
            panel->scroll_offset = panel->scroll_offset > 3 ?
                panel->scroll_offset - 3 : 0;
        return true;
    }
    if(!panel->focused || panel->terminal == NULL) return false;
    if(event->type == SDL_EVENT_TEXT_INPUT) {
        editor_terminal_input_write(panel, event->text.text,
            strlen(event->text.text));
        panel->scroll_offset = 0;
        return true;
    }
    if(event->type == SDL_EVENT_KEY_DOWN) {
        const char *sequence = NULL;
        bool control = (event->key.mod & SDL_KMOD_CTRL) != 0;
        if(control && event->key.key == SDLK_C) {
            (void)rohr_terminal_interrupt(panel->terminal);
            return true;
        }
        switch(event->key.key) {
            case SDLK_RETURN: case SDLK_KP_ENTER: sequence = "\r"; break;
            case SDLK_BACKSPACE: sequence = "\x7f"; break;
            case SDLK_TAB: sequence = "\t"; break;
            case SDLK_ESCAPE: sequence = "\x1b"; break;
            case SDLK_UP: sequence = "\x1b[A"; break;
            case SDLK_DOWN: sequence = "\x1b[B"; break;
            case SDLK_RIGHT: sequence = "\x1b[C"; break;
            case SDLK_LEFT: sequence = "\x1b[D"; break;
            case SDLK_HOME: sequence = "\x1b[H"; break;
            case SDLK_END: sequence = "\x1b[F"; break;
            case SDLK_DELETE: sequence = "\x1b[3~"; break;
            default: break;
        }
        if(sequence != NULL) editor_terminal_input_write(panel, sequence,
            strlen(sequence));
        return true;
    }
    return true;
}

void editor_terminal_panel_update(EditorTerminalPanel *panel) {
    RohrTerminalResult result;
    if(panel == NULL || panel->terminal == NULL) return;
    result = rohr_terminal_update(panel->terminal);
    if(!result.success) fprintf(stderr, "Terminal update failed: %s\n",
        rohr_terminal_error_message_get(&result));
}

void editor_terminal_panel_operation_write(EditorTerminalPanel *panel,
        const char *command) {
    RohrTerminalResult result;
    if(panel == NULL || panel->terminal == NULL || command == NULL) return;
    result = rohr_terminal_output_write(panel->terminal, "\r\n$ ", 4);
    if(result.success)
        result = rohr_terminal_output_write(panel->terminal, command, strlen(command));
    if(result.success)
        result = rohr_terminal_output_write(panel->terminal, "\r\n", 2);
    if(!result.success) fprintf(stderr, "Terminal operation output failed: %s\n",
        rohr_terminal_error_message_get(&result));
    panel->scroll_offset = 0;
}

float editor_terminal_panel_viewport_bottom_get(const EditorTerminalPanel *panel) {
    if(panel == NULL || !panel->visible) return WINDOW_HEIGHT;
    return WINDOW_HEIGHT - panel->height;
}

void editor_terminal_panel_draw(EditorTerminalPanel *panel, float viewport_width,
        float viewport_bottom) {
    size_t line_count;
    size_t visible_count;
    size_t first;
    if(panel == NULL || !panel->visible) return;
    (void)rohr_graphics_screen_rect_draw(0.0f, viewport_bottom, viewport_width,
        WINDOW_HEIGHT - viewport_bottom, (Color){12, 14, 18, 255});
    (void)rohr_graphics_screen_rect_draw(0.0f, viewport_bottom, viewport_width,
        EDITOR_TERMINAL_DIVIDER_HEIGHT, (Color){75, 84, 100, 255});
    if(panel->terminal == NULL) return;
    line_count = rohr_terminal_line_count_get(panel->terminal);
    visible_count = (size_t)((WINDOW_HEIGHT - viewport_bottom - 10.0f) /
        EDITOR_TERMINAL_LINE_HEIGHT);
    if(visible_count > EDITOR_TERMINAL_VISIBLE_LINE_MAX)
        visible_count = EDITOR_TERMINAL_VISIBLE_LINE_MAX;
    if(panel->scroll_offset > line_count) panel->scroll_offset = line_count;
    first = line_count > visible_count + panel->scroll_offset ?
        line_count - visible_count - panel->scroll_offset : 0;
    visible_count = line_count - first < visible_count ? line_count - first : visible_count;
    for(size_t i = 0; i < visible_count; i += 1) {
        RohrTerminalLineView line = rohr_terminal_line_get(panel->terminal, first + i);
        char buffer[2048];
        size_t used = 0;
        for(size_t cell = 0; cell < line.cell_count; cell += 1)
            used = editor_terminal_codepoint_write(buffer, sizeof(buffer), used,
                line.cells[cell].codepoint);
        buffer[used] = '\0';
        (void)rohr_graphics_text_value_set(&panel->lines[i], buffer);
        (void)rohr_graphics_text_draw(&panel->lines[i],
            (Position){8.0f, viewport_bottom + 7.0f + i * EDITOR_TERMINAL_LINE_HEIGHT});
    }
    {
        RohrTerminalCursor cursor = rohr_terminal_cursor_get(panel->terminal);
        if(cursor.visible && cursor.line_index >= first &&
                cursor.line_index < first + visible_count) {
            float x = 8.0f + (float)cursor.column * panel->cell_width;
            float y = viewport_bottom + 7.0f +
                (float)(cursor.line_index - first) * EDITOR_TERMINAL_LINE_HEIGHT;
            (void)rohr_graphics_screen_rect_draw(x, y, 2.0f,
                EDITOR_TERMINAL_LINE_HEIGHT - 2.0f, (Color){230, 234, 242, 255});
        }
    }
}

void editor_terminal_panel_destroy(EditorTerminalPanel *panel) {
    if(panel == NULL) return;
    editor_terminal_panel_project_close(panel);
    for(size_t i = 0; i < EDITOR_TERMINAL_VISIBLE_LINE_MAX; i += 1)
        rohr_graphics_text_destroy(&panel->lines[i]);
    memset(panel, 0, sizeof(*panel));
}
