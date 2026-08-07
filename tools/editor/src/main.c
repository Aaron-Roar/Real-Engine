#include "rohr.h"
#include "editor_project.h"
#include "editor_viewport.h"
#include "editor_layout.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define editor_chdir _chdir
#else
#include <unistd.h>
#define editor_chdir chdir
#endif

#define EDITOR_VIEWPORT_MIN_WIDTH 100.0f
#define EDITOR_TOOLS_MIN_WIDTH 40.0f
#define EDITOR_DIVIDER_GRAB_WIDTH 6.0f

float editor_viewport_width = WINDOW_WIDTH * 0.8f;
float editor_window_width = WINDOW_WIDTH;

static void editor_window_layout_sync(void) {
    Scale output = rohr_graphics_render_output_size_get();
    float logical_width;

    if(output.x <= 0.0f || output.y <= 0.0f) return;
    logical_width = roundf(WINDOW_HEIGHT * output.x / output.y);
    if(logical_width < EDITOR_VIEWPORT_MIN_WIDTH + EDITOR_TOOLS_MIN_WIDTH) {
        logical_width = EDITOR_VIEWPORT_MIN_WIDTH + EDITOR_TOOLS_MIN_WIDTH;
    }
    if(logical_width == editor_window_width) return;
    if(!rohr_graphics_logical_size_set((int)logical_width, WINDOW_HEIGHT)) return;
    editor_window_width = logical_width;
    editor_viewport_width = fminf(editor_viewport_width,
        editor_window_width - EDITOR_TOOLS_MIN_WIDTH);
}

static bool editor_result_ok(EngineResult result) {
    if(!rohr_error_check(result)) return true;
    fprintf(stderr, "%s\n", rohr_error_default_message_get(result.result.error));
    return false;
}

static bool editor_use_executable_directory(void) {
    const char *base_path = SDL_GetBasePath();

    return base_path != NULL && editor_chdir(base_path) == 0;
}

static bool editor_text_create(
    FontAsset *font,
    const char *value,
    TextAsset *text
) {
    TextAssetResult result;

    if(font == NULL || value == NULL || text == NULL) return false;
    result = rohr_graphics_text_create(font, value, (Color){230, 234, 242, 255});
    if(rohr_error_check(result)) {
        fprintf(stderr, "%s\n",
            rohr_error_default_message_get(result.result.error));
        return false;
    }
    *text = result.result.value;
    return true;
}

static void editor_numeric_field_disabled_draw(
    TextAsset *display,
    float value,
    UIRect bounds
) {
    char text[32];

    if(display == NULL) return;
    snprintf(text, sizeof(text), "%.1f", value);
    (void)rohr_graphics_text_value_set(display, text);
    rohr_ui_button_disabled(bounds, NULL);
    rohr_ui_label(display, bounds);
}

static UIButtonStyle editor_delete_button_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();

    style.idle = (Color){145, 42, 48, 255};
    style.hovered = (Color){181, 53, 60, 255};
    style.pressed = (Color){112, 31, 37, 255};
    style.disabled = (Color){75, 35, 38, 210};
    return style;
}

static UIButtonStyle editor_selected_button_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();

    style.idle = (Color){118, 96, 35, 255};
    style.hovered = (Color){145, 119, 45, 255};
    return style;
}

static bool editor_selected_delete(
    EditorProject *project,
    EditorViewportState *viewport_state
) {
    EditorObject *selected;

    if(project == NULL || viewport_state == NULL) return false;
    selected = editor_project_selected_get(project);
    if(selected == NULL) return false;
    if(viewport_state->selection == EDITOR_SELECTION_OBJECT) {
        if(!editor_project_object_remove(project, selected->id)) return false;
        editor_viewport_hitbox_editor_exit(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_HITBOX) {
        if(!editor_project_hitbox_remove(selected)) return false;
        editor_viewport_object_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_VERTEX) {
        if(!editor_project_hitbox_vertex_remove(
                selected, viewport_state->selected_vertex)) return false;
        editor_viewport_hitbox_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_LINE) {
        if(!editor_project_hitbox_line_remove(
                selected, viewport_state->selected_line)) return false;
        editor_viewport_hitbox_editor_enter(viewport_state);
        return true;
    }
    return false;
}

static bool editor_selected_open(
    EditorProject *project,
    EditorViewportState *viewport_state
) {
    EditorObject *selected;

    if(project == NULL || viewport_state == NULL) return false;
    selected = editor_project_selected_get(project);
    if(selected == NULL) return false;
    if(viewport_state->selection == EDITOR_SELECTION_OBJECT) {
        editor_viewport_object_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_HITBOX && selected->has_hitbox) {
        editor_viewport_hitbox_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_VERTEX &&
            viewport_state->selected_vertex < selected->hitbox.vertex_count) {
        editor_viewport_vertex_editor_enter(
            viewport_state, viewport_state->selected_vertex);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_LINE &&
            viewport_state->selected_line < selected->hitbox.vertex_count) {
        editor_viewport_line_editor_enter(viewport_state, viewport_state->selected_line);
        return true;
    }
    return false;
}

static bool editor_open_item_delete(
    EditorProject *project,
    EditorViewportState *viewport_state
) {
    if(viewport_state == NULL) return false;
    if(viewport_state->mode == EDITOR_VIEWPORT_OBJECT) {
        viewport_state->selection = EDITOR_SELECTION_OBJECT;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_HITBOX) {
        viewport_state->selection = EDITOR_SELECTION_HITBOX;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_VERTEX) {
        viewport_state->selection = EDITOR_SELECTION_VERTEX;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_LINE) {
        viewport_state->selection = EDITOR_SELECTION_LINE;
    } else {
        return false;
    }
    return editor_selected_delete(project, viewport_state);
}

int main(void) {
    KeyboardState keyboard = {0};
    MouseState mouse = {0};
    FontAsset font = {0};
    TextAsset hitbox_editor_label = {0};
    TextAsset add_object_label = {0};
    TextAsset add_hitbox_label = {0};
    TextAsset hitbox_label = {0};
    TextAsset vertices_label = {0};
    TextAsset lines_label = {0};
    TextAsset vertex_labels[EDITOR_HITBOX_VERTEX_MAX] = {0};
    TextAsset line_labels[EDITOR_HITBOX_VERTEX_MAX] = {0};
    TextAsset lock_label = {0};
    TextAsset unlock_label = {0};
    TextAsset add_vertex_label = {0};
    TextAsset delete_hitbox_label = {0};
    TextAsset delete_vertex_label = {0};
    TextAsset delete_line_label = {0};
    TextAsset delete_object_label = {0};
    TextAsset constrained_label = {0};
    TextAsset x_label = {0};
    TextAsset y_label = {0};
    TextAsset length_label = {0};
    TextAsset object_name_label = {0};
    TextAsset x_field = {0};
    TextAsset y_field = {0};
    TextAsset length_field = {0};
    TextAsset object_name_labels[EDITOR_OBJECT_MAX] = {0};
    char object_name_cache[EDITOR_OBJECT_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    ViewportId viewport = 0;
    EditorProject project;
    EditorViewportState viewport_state;
    bool running = true;
    bool field_editing = false;
    bool panel_resizing = false;

    editor_project_init(&project);
    editor_viewport_state_init(&viewport_state);

    if(!editor_use_executable_directory() ||
            !editor_result_ok(rohr_engine_init()) ||
            !editor_result_ok(rohr_graphics_start())) goto fail;
    editor_window_layout_sync();
    {
        ViewportConfig config = rohr_viewport_config_default_get();
        ViewportIdResult result;

        config.rectangle = (ViewportRectangle){
            0.0f, 0.0f, EDITOR_VIEWPORT_WIDTH, WINDOW_HEIGHT
        };
        result = rohr_viewport_create(config);
        if(rohr_error_check(result)) goto fail;
        viewport = result.result.value;
        if(!editor_result_ok(rohr_viewport_camera_clear(viewport)) ||
                !editor_result_ok(rohr_viewport_disable_set(viewport))) goto fail;
    }
    {
        FontAssetResult result = rohr_graphics_font_load((FontDescriptor){
            .file = "assets/JetBrainsMono-BoldItalic.ttf",
            .point_size = 12.0f
        });

        if(rohr_error_check(result)) goto fail;
        font = result.result.value;
    }
    if(!editor_text_create(&font, "Hitbox Editor", &hitbox_editor_label) ||
            !editor_text_create(&font, "Add Object", &add_object_label) ||
            !editor_text_create(&font, "Add Hitbox", &add_hitbox_label) ||
            !editor_text_create(&font, "Hitbox", &hitbox_label) ||
            !editor_text_create(&font, "Vertices", &vertices_label) ||
            !editor_text_create(&font, "Lines", &lines_label) ||
            !editor_text_create(&font, "Lock Position", &lock_label) ||
            !editor_text_create(&font, "Unlock Position", &unlock_label) ||
            !editor_text_create(&font, "Add Vertex", &add_vertex_label) ||
            !editor_text_create(&font, "Delete Hitbox", &delete_hitbox_label) ||
            !editor_text_create(&font, "Delete Vertex", &delete_vertex_label) ||
            !editor_text_create(&font, "Delete Line", &delete_line_label) ||
            !editor_text_create(&font, "Delete Object", &delete_object_label) ||
            !editor_text_create(&font, "Line distance fully constrained", &constrained_label) ||
            !editor_text_create(&font, "X", &x_label) ||
            !editor_text_create(&font, "Y", &y_label) ||
            !editor_text_create(&font, "Length", &length_label) ||
            !editor_text_create(&font, "Object Name", &object_name_label) ||
            !editor_text_create(&font, "", &x_field) ||
            !editor_text_create(&font, "", &y_field) ||
            !editor_text_create(&font, "", &length_field)) goto fail;
    for(uint32_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        char name[32];
        snprintf(name, sizeof(name), "vertex_%u", i + 1);
        if(!editor_text_create(&font, name, &vertex_labels[i])) goto fail;
        snprintf(name, sizeof(name), "line_%u", i + 1);
        if(!editor_text_create(&font, name, &line_labels[i])) goto fail;
    }

    while(running) {
        SDL_Event event;
        rohr_controller_key_states_update(&keyboard);
        rohr_controller_mouse_states_update(&mouse);
        while((event = rohr_engine_event_poll()).type != 0) {
            rohr_ui_field_event_add(&event);
            rohr_controller_key_event_add(
                &keyboard,
                rohr_controller_keyboard_event_capture(&event));
            rohr_controller_mouse_event_add(
                &mouse,
                rohr_controller_mouse_event_capture(&event));
            if(event.type == SDL_EVENT_QUIT) running = false;
        }
        if(!field_editing &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            if(editor_viewport_hitbox_editor_active_get(&viewport_state)) {
                editor_viewport_back(&viewport_state);
            } else {
                running = false;
            }
        }
        if(!field_editing && viewport_state.selection != EDITOR_SELECTION_NONE &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_DELETE)) {
            (void)editor_selected_delete(&project, &viewport_state);
        }
        if(!field_editing && viewport_state.selection != EDITOR_SELECTION_NONE &&
                (rohr_controller_key_pressed_get(&keyboard, SDLK_RETURN) ||
                    rohr_controller_key_pressed_get(&keyboard, SDLK_KP_ENTER))) {
            (void)editor_selected_open(&project, &viewport_state);
        }
        if(!running) break;
        editor_window_layout_sync();

        {
            Position pointer = rohr_graphics_mouse_screen_position_get();
            MouseButtonState primary = mouse.button_states[MOUSE_BUTTON_LEFT];

            if(!panel_resizing && primary == MOUSE_BUTTON_STATE_PRESSED &&
                    fabsf(pointer.x - EDITOR_VIEWPORT_WIDTH) <=
                        EDITOR_DIVIDER_GRAB_WIDTH) {
                panel_resizing = true;
            }
            if(panel_resizing && (primary == MOUSE_BUTTON_STATE_PRESSED ||
                    primary == MOUSE_BUTTON_STATE_DOWN)) {
                EDITOR_VIEWPORT_WIDTH = fmaxf(EDITOR_VIEWPORT_MIN_WIDTH,
                    fminf(pointer.x, editor_window_width - EDITOR_TOOLS_MIN_WIDTH));
            }
            if(primary == MOUSE_BUTTON_STATE_RELEASED ||
                    primary == MOUSE_BUTTON_STATE_UP) {
                panel_resizing = false;
            }
        }

        rohr_graphics_background_draw((Color){18, 21, 27, 255});
        (void)rohr_graphics_screen_rect_draw(
            0.0f, 0.0f, EDITOR_VIEWPORT_WIDTH, WINDOW_HEIGHT,
            (Color){25, 29, 37, 255});
        (void)rohr_graphics_screen_rect_draw(
            EDITOR_VIEWPORT_WIDTH, 0.0f, EDITOR_TOOLS_WIDTH, WINDOW_HEIGHT,
            (Color){38, 43, 53, 255});

        (void)rohr_graphics_screen_clip_set(
            EDITOR_VIEWPORT_WIDTH, 0.0f, EDITOR_TOOLS_WIDTH, WINDOW_HEIGHT);
        rohr_ui_frame_begin((UIInput){
            .pointer = rohr_graphics_mouse_screen_position_get(),
            .primary_button = mouse.button_states[MOUSE_BUTTON_LEFT]
        });
        field_editing = false;
        if(viewport_state.mode == EDITOR_VIEWPORT_HITBOX) {
            EditorObject *selected = editor_project_selected_get(&project);

            rohr_ui_label(&hitbox_editor_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 48.0f,
                    EDITOR_TOOLS_WIDTH - 16.0f, 24.0f});
            rohr_ui_label(&vertices_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 82.0f,
                    EDITOR_TOOLS_WIDTH - 20.0f, 24.0f});
            if(selected != NULL && selected->has_hitbox) {
                for(uint32_t i = 0; i < selected->hitbox.vertex_count; i += 1) {
                    char id[64];
                    float y = 110.0f + (float)i * 27.0f;
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    const UIButtonStyle *style = viewport_state.selection ==
                        EDITOR_SELECTION_VERTEX && viewport_state.selected_vertex == i ?
                        &selected_style : NULL;
                    UIButtonResult result;
                    snprintf(id, sizeof(id), "editor.vertex.%u", selected->hitbox.vertices[i].id);
                    result = rohr_ui_button(id, &vertex_labels[i],
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, y,
                            EDITOR_TOOLS_WIDTH - 26.0f, 23.0f}, style);
                    if(result.clicked) {
                        viewport_state.selection = EDITOR_SELECTION_VERTEX;
                        viewport_state.selected_vertex = i;
                        if(result.double_clicked) {
                            (void)editor_selected_open(&project, &viewport_state);
                        }
                    }
                }
                {
                    float base = 118.0f + (float)selected->hitbox.vertex_count * 27.0f;
                    rohr_ui_label(&lines_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        base, EDITOR_TOOLS_WIDTH - 20.0f, 24.0f});
                    for(uint32_t i = 0; i < selected->hitbox.vertex_count; i += 1) {
                        char id[64];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        const UIButtonStyle *style = viewport_state.selection ==
                            EDITOR_SELECTION_LINE && viewport_state.selected_line == i ?
                            &selected_style : NULL;
                        UIButtonResult result;
                        snprintf(id, sizeof(id), "editor.line.%u", i);
                        result = rohr_ui_button(id, &line_labels[i],
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f,
                                base + 28.0f + (float)i * 27.0f,
                                EDITOR_TOOLS_WIDTH - 26.0f, 23.0f}, style);
                        if(result.clicked) {
                            viewport_state.selection = EDITOR_SELECTION_LINE;
                            viewport_state.selected_line = i;
                            if(result.double_clicked) {
                                (void)editor_selected_open(&project, &viewport_state);
                            }
                        }
                    }
                    {
                        UIButtonStyle delete_style = editor_delete_button_style_get();
                        float delete_y = base + 40.0f +
                            (float)selected->hitbox.vertex_count * 27.0f;

                        if(rohr_ui_button("editor.hitbox.delete", &delete_hitbox_label,
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, delete_y,
                                    EDITOR_TOOLS_WIDTH - 26.0f, 30.0f},
                                &delete_style).clicked) {
                            (void)editor_open_item_delete(&project, &viewport_state);
                        }
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_VERTEX) {
            EditorObject *selected = editor_project_selected_get(&project);
            if(selected != NULL && viewport_state.selected_vertex < selected->hitbox.vertex_count) {
                EditorVertex *vertex = &selected->hitbox.vertices[viewport_state.selected_vertex];
                UISliderConfig slider = rohr_ui_slider_config_default_get();
                rohr_ui_label(&vertex_labels[viewport_state.selected_vertex],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    48.0f, EDITOR_TOOLS_WIDTH - 16.0f, 24.0f});
                if(rohr_ui_button("editor.vertex.lock",
                        vertex->position_locked ? &unlock_label : &lock_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 82.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, NULL).clicked) {
                    vertex->position_locked = !vertex->position_locked;
                }
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 122.0f, 20.0f, 22.0f});
                rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 192.0f, 20.0f, 22.0f});
                slider.length = EDITOR_TOOLS_WIDTH - 42.0f;
                slider.min_value = -EDITOR_VIEWPORT_WIDTH * 0.5f;
                slider.max_value = EDITOR_VIEWPORT_WIDTH * 0.5f;
                slider.center = (Position){EDITOR_VIEWPORT_WIDTH + 72.0f, 157.0f};
                if(vertex->position_locked) {
                    editor_numeric_field_disabled_draw(&x_field, vertex->position.x,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 122.0f,
                            EDITOR_TOOLS_WIDTH - 38.0f, 24.0f});
                    editor_numeric_field_disabled_draw(&y_field, vertex->position.y,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 192.0f,
                            EDITOR_TOOLS_WIDTH - 38.0f, 24.0f});
                } else {
                    UIFieldResult x_result = rohr_ui_field("editor.vertex.x.field",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &vertex->position.x}, &x_field,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 122.0f,
                            EDITOR_TOOLS_WIDTH - 38.0f, 24.0f}, NULL);
                    UIFieldResult y_result = rohr_ui_field("editor.vertex.y.field",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &vertex->position.y}, &y_field,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 192.0f,
                            EDITOR_TOOLS_WIDTH - 38.0f, 24.0f}, NULL);
                    field_editing = x_result.active || y_result.active;
                    vertex->position.x = rohr_ui_slider("editor.vertex.x", vertex->position.x, &slider).value;
                    slider.center.y = 227.0f;
                    vertex->position.y = rohr_ui_slider("editor.vertex.y", vertex->position.y, &slider).value;
                }
                {
                    UIButtonStyle delete_style = editor_delete_button_style_get();
                    UIRect delete_bounds = {EDITOR_VIEWPORT_WIDTH + 10.0f, 260.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 30.0f};
                    if(selected->hitbox.vertex_count <= EDITOR_HITBOX_VERTEX_MIN) {
                        rohr_ui_button_disabled(delete_bounds, &delete_style);
                        rohr_ui_label(&delete_vertex_label, delete_bounds);
                    } else if(rohr_ui_button("editor.vertex.delete", &delete_vertex_label,
                            delete_bounds, &delete_style).clicked) {
                        (void)editor_open_item_delete(&project, &viewport_state);
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_LINE) {
            EditorObject *selected = editor_project_selected_get(&project);
            if(selected != NULL && viewport_state.selected_line < selected->hitbox.vertex_count) {
                uint32_t line = viewport_state.selected_line;
                EditorVertex *a = &selected->hitbox.vertices[line];
                EditorVertex *b = &selected->hitbox.vertices[(line + 1) % selected->hitbox.vertex_count];
                bool constrained = a->position_locked && b->position_locked;
                bool vertex_inserted = false;
                float length = editor_project_hitbox_line_length_get(selected, line);
                UISliderConfig slider = rohr_ui_slider_config_default_get();
                rohr_ui_label(&line_labels[line], (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    48.0f, EDITOR_TOOLS_WIDTH - 16.0f, 24.0f});
                if(rohr_ui_button("editor.line.add_vertex", &add_vertex_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 82.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, NULL).clicked &&
                        editor_project_hitbox_vertex_insert(&project, selected, line)) {
                    editor_viewport_hitbox_editor_enter(&viewport_state);
                    vertex_inserted = true;
                }
                if(!vertex_inserted) {
                    rohr_ui_label(&length_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        150.0f, 52.0f, 26.0f});
                    if(constrained) {
                        editor_numeric_field_disabled_draw(&length_field, length,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f,
                                150.0f, EDITOR_TOOLS_WIDTH - 70.0f, 26.0f});
                        rohr_ui_label(&constrained_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 5.0f,
                                190.0f, EDITOR_TOOLS_WIDTH - 10.0f, 38.0f});
                    } else {
                        UIFieldResult length_result = rohr_ui_field(
                            "editor.line.length.field",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &length},
                            &length_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f, 150.0f,
                                EDITOR_TOOLS_WIDTH - 70.0f, 26.0f}, NULL);
                        field_editing = length_result.active;
                        if(length_result.changed) {
                            (void)editor_project_hitbox_line_length_set(
                                selected, line, length);
                        }
                        slider.center = (Position){EDITOR_VIEWPORT_WIDTH +
                            EDITOR_TOOLS_WIDTH * 0.5f, 202.0f};
                        slider.length = EDITOR_TOOLS_WIDTH - 36.0f;
                        slider.min_value = 5.0f;
                        slider.max_value = EDITOR_VIEWPORT_WIDTH;
                        (void)editor_project_hitbox_line_length_set(selected, line,
                            rohr_ui_slider("editor.line.length", length, &slider).value);
                    }
                    {
                        UIButtonStyle delete_style = editor_delete_button_style_get();
                        UIRect delete_bounds = {EDITOR_VIEWPORT_WIDTH + 10.0f, 238.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f};
                        if(selected->hitbox.vertex_count <= EDITOR_HITBOX_VERTEX_MIN) {
                            rohr_ui_button_disabled(delete_bounds, &delete_style);
                            rohr_ui_label(&delete_line_label, delete_bounds);
                        } else if(rohr_ui_button("editor.line.delete", &delete_line_label,
                                delete_bounds, &delete_style).clicked) {
                            (void)editor_open_item_delete(&project, &viewport_state);
                        }
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_OBJECT) {
            EditorObject *selected = editor_project_selected_get(&project);
            size_t selected_index = 0;
            if(selected != NULL) {
                selected_index = (size_t)(selected - project.objects);
                if(strcmp(object_name_cache[selected_index], selected->name) != 0) {
                    rohr_graphics_text_destroy(&object_name_labels[selected_index]);
                    if(!editor_text_create(&font, selected->name,
                            &object_name_labels[selected_index])) goto fail;
                    snprintf(object_name_cache[selected_index], EDITOR_OBJECT_NAME_MAX,
                        "%s", selected->name);
                }
                rohr_ui_label(&object_name_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 52.0f, 90.0f, 34.0f});
                {
                    UIFieldResult name_result = rohr_ui_field("editor.object.name",
                    (UIFieldBinding){.kind = UI_FIELD_STRING,
                        .string = selected->name,
                        .string_capacity = sizeof(selected->name)},
                    &object_name_labels[selected_index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 98.0f, 52.0f,
                        EDITOR_TOOLS_WIDTH - 108.0f, 34.0f}, NULL);
                    field_editing = name_result.active;
                    if(name_result.changed) {
                        snprintf(object_name_cache[selected_index],
                            EDITOR_OBJECT_NAME_MAX, "%s", selected->name);
                    }
                }
                if(!selected->has_hitbox) {
                    if(rohr_ui_button("editor.add_hitbox", &add_hitbox_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 98.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 36.0f}, NULL).clicked) {
                        editor_project_hitbox_add(&project, selected);
                        viewport_state.selection = EDITOR_SELECTION_HITBOX;
                    }
                } else {
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    const UIButtonStyle *style = viewport_state.selection ==
                        EDITOR_SELECTION_HITBOX ? &selected_style : NULL;
                    UIButtonResult result = rohr_ui_button("editor.object.hitbox",
                        &hitbox_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, 100.0f,
                            EDITOR_TOOLS_WIDTH - 26.0f, 30.0f}, style);
                    if(result.clicked) {
                        viewport_state.selection = EDITOR_SELECTION_HITBOX;
                        if(result.double_clicked) {
                            (void)editor_selected_open(&project, &viewport_state);
                        }
                    }
                }
                {
                    UIButtonStyle delete_style = editor_delete_button_style_get();
                    if(rohr_ui_button("editor.object.delete", &delete_object_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 146.0f,
                                EDITOR_TOOLS_WIDTH - 20.0f, 34.0f},
                            &delete_style).clicked) {
                        (void)editor_open_item_delete(&project, &viewport_state);
                    }
                }
            }
        } else {
            UIButtonResult add_object = rohr_ui_button(
                "editor.add_object", &add_object_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 10.0f,
                    EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
            if(add_object.clicked) {
                EditorObject *added = editor_project_object_add(
                    &project,
                    (Position){EDITOR_VIEWPORT_WIDTH * 0.5f,
                        WINDOW_HEIGHT * 0.5f});
                if(added != NULL) viewport_state.selection = EDITOR_SELECTION_OBJECT;
            }
            (void)rohr_graphics_screen_rect_draw(
                EDITOR_VIEWPORT_WIDTH + 10.0f, 58.0f,
                EDITOR_TOOLS_WIDTH - 20.0f, 1.0f,
                (Color){75, 84, 100, 255});
            for(size_t i = 0; i < project.object_count; i += 1) {
                EditorObject *object = &project.objects[i];
                float y = 70.0f + (float)i * 34.0f;
                char object_button_id[64];
                UIButtonResult object_result;

                if(strcmp(object_name_cache[i], object->name) != 0) {
                    rohr_graphics_text_destroy(&object_name_labels[i]);
                    if(!editor_text_create(&font, object->name,
                            &object_name_labels[i])) goto fail;
                    snprintf(object_name_cache[i], EDITOR_OBJECT_NAME_MAX,
                        "%s", object->name);
                }

                snprintf(object_button_id, sizeof(object_button_id),
                    "editor.object.%u", object->id);
                {
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    const UIButtonStyle *style = viewport_state.selection ==
                            EDITOR_SELECTION_OBJECT && project.selected == object->id ?
                        &selected_style : NULL;
                object_result = rohr_ui_button(
                    object_button_id, &object_name_labels[i],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, y,
                        EDITOR_TOOLS_WIDTH - 16.0f, 28.0f}, style);
                }
                if(object_result.clicked) {
                    (void)editor_project_object_select(&project, object->id);
                    viewport_state.selection = EDITOR_SELECTION_OBJECT;
                    if(object_result.double_clicked) {
                        (void)editor_selected_open(&project, &viewport_state);
                    }
                }
            }
        }
        editor_viewport_update(
            &viewport_state,
            &project,
            rohr_graphics_mouse_screen_position_get(),
            mouse.button_states[MOUSE_BUTTON_LEFT],
            rohr_ui_pointer_consumed_get());
        rohr_ui_frame_end();
        rohr_graphics_screen_clip_clear();
        (void)rohr_graphics_screen_clip_set(
            0.0f, 0.0f, EDITOR_VIEWPORT_WIDTH, WINDOW_HEIGHT);
        editor_viewport_draw(&project, &viewport_state);
        rohr_graphics_screen_clip_clear();
        (void)rohr_graphics_screen_rect_draw(
            EDITOR_VIEWPORT_WIDTH - 1.0f, 0.0f, 3.0f, WINDOW_HEIGHT,
            (Color){75, 84, 100, 255});
        rohr_graphics_show();
    }

    rohr_graphics_text_destroy(&vertices_label);
    rohr_graphics_text_destroy(&length_field);
    rohr_graphics_text_destroy(&y_field);
    rohr_graphics_text_destroy(&x_field);
    rohr_graphics_text_destroy(&length_label);
    rohr_graphics_text_destroy(&object_name_label);
    rohr_graphics_text_destroy(&y_label);
    rohr_graphics_text_destroy(&x_label);
    rohr_graphics_text_destroy(&constrained_label);
    rohr_graphics_text_destroy(&delete_object_label);
    rohr_graphics_text_destroy(&delete_line_label);
    rohr_graphics_text_destroy(&delete_vertex_label);
    rohr_graphics_text_destroy(&delete_hitbox_label);
    rohr_graphics_text_destroy(&add_vertex_label);
    rohr_graphics_text_destroy(&unlock_label);
    rohr_graphics_text_destroy(&lock_label);
    for(uint32_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        rohr_graphics_text_destroy(&line_labels[i]);
        rohr_graphics_text_destroy(&vertex_labels[i]);
    }
    rohr_graphics_text_destroy(&lines_label);
    rohr_graphics_text_destroy(&hitbox_label);
    for(size_t i = 0; i < EDITOR_OBJECT_MAX; i += 1) {
        rohr_graphics_text_destroy(&object_name_labels[i]);
    }
    rohr_graphics_text_destroy(&add_hitbox_label);
    rohr_graphics_text_destroy(&add_object_label);
    rohr_graphics_text_destroy(&hitbox_editor_label);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_graphics_text_destroy(&vertices_label);
    rohr_graphics_text_destroy(&length_field);
    rohr_graphics_text_destroy(&y_field);
    rohr_graphics_text_destroy(&x_field);
    rohr_graphics_text_destroy(&length_label);
    rohr_graphics_text_destroy(&object_name_label);
    rohr_graphics_text_destroy(&y_label);
    rohr_graphics_text_destroy(&x_label);
    rohr_graphics_text_destroy(&constrained_label);
    rohr_graphics_text_destroy(&delete_object_label);
    rohr_graphics_text_destroy(&delete_line_label);
    rohr_graphics_text_destroy(&delete_vertex_label);
    rohr_graphics_text_destroy(&delete_hitbox_label);
    rohr_graphics_text_destroy(&add_vertex_label);
    rohr_graphics_text_destroy(&unlock_label);
    rohr_graphics_text_destroy(&lock_label);
    for(uint32_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        rohr_graphics_text_destroy(&line_labels[i]);
        rohr_graphics_text_destroy(&vertex_labels[i]);
    }
    rohr_graphics_text_destroy(&lines_label);
    rohr_graphics_text_destroy(&hitbox_label);
    for(size_t i = 0; i < EDITOR_OBJECT_MAX; i += 1) {
        rohr_graphics_text_destroy(&object_name_labels[i]);
    }
    rohr_graphics_text_destroy(&add_hitbox_label);
    rohr_graphics_text_destroy(&add_object_label);
    rohr_graphics_text_destroy(&hitbox_editor_label);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
