#include "rohr.h"
#include "editor_project.h"
#include "editor_viewport.h"
#include "editor_layout.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define editor_chdir _chdir
#else
#include <unistd.h>
#define editor_chdir chdir
#endif

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

static bool editor_object_name_key_apply(EditorObject *object, SDL_Keycode key) {
    size_t length;

    if(object == NULL) return false;
    length = strlen(object->name);
    if(key == SDLK_BACKSPACE) {
        if(length > 0) object->name[length - 1] = '\0';
        return true;
    }
    if((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9') ||
            key == '_' || key == '-' || key == ' ') {
        if(length + 1 < sizeof(object->name)) {
            object->name[length] = (char)key;
            object->name[length + 1] = '\0';
        }
        return true;
    }
    return false;
}

int main(void) {
    KeyboardState keyboard = {0};
    MouseState mouse = {0};
    FontAsset font = {0};
    TextAsset tools_title = {0};
    TextAsset hierarchy_label = {0};
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
    TextAsset constrained_label = {0};
    TextAsset x_label = {0};
    TextAsset y_label = {0};
    TextAsset length_label = {0};
    TextAsset object_name_labels[EDITOR_OBJECT_MAX] = {0};
    char object_name_cache[EDITOR_OBJECT_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    ViewportId viewport = 0;
    EditorProject project;
    EditorViewportState viewport_state;
    bool running = true;
    bool name_editing = false;

    editor_project_init(&project);
    editor_viewport_state_init(&viewport_state);

    if(!editor_use_executable_directory() ||
            !editor_result_ok(rohr_engine_init()) ||
            !editor_result_ok(rohr_graphics_start())) goto fail;
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
    if(!editor_text_create(&font, "Tools", &tools_title) ||
            !editor_text_create(&font, "Hierarchy", &hierarchy_label) ||
            !editor_text_create(&font, "Hitbox Editor", &hitbox_editor_label) ||
            !editor_text_create(&font, "Add Object", &add_object_label) ||
            !editor_text_create(&font, "Add Hitbox", &add_hitbox_label) ||
            !editor_text_create(&font, "Hitbox", &hitbox_label) ||
            !editor_text_create(&font, "Vertices", &vertices_label) ||
            !editor_text_create(&font, "Lines", &lines_label) ||
            !editor_text_create(&font, "Lock Position", &lock_label) ||
            !editor_text_create(&font, "Unlock Position", &unlock_label) ||
            !editor_text_create(&font, "Add Vertex", &add_vertex_label) ||
            !editor_text_create(&font, "Line distance fully constrained", &constrained_label) ||
            !editor_text_create(&font, "X", &x_label) ||
            !editor_text_create(&font, "Y", &y_label) ||
            !editor_text_create(&font, "Length", &length_label)) goto fail;
    for(uint32_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        char name[32];
        snprintf(name, sizeof(name), "vertex_%u", i + 1);
        if(!editor_text_create(&font, name, &vertex_labels[i])) goto fail;
        snprintf(name, sizeof(name), "line_%u", i + 1);
        if(!editor_text_create(&font, name, &line_labels[i])) goto fail;
    }

    while(running) {
        SDL_Event event;
        bool escape_name_edit_consumed = false;

        rohr_controller_key_states_update(&keyboard);
        rohr_controller_mouse_states_update(&mouse);
        while((event = rohr_engine_event_poll()).type != 0) {
            if(name_editing && event.type == SDL_EVENT_KEY_DOWN) {
                EditorObject *selected = editor_project_selected_get(&project);
                if(event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                    name_editing = false;
                } else if(event.key.key == SDLK_ESCAPE) {
                    name_editing = false;
                    escape_name_edit_consumed = true;
                } else {
                    (void)editor_object_name_key_apply(selected, event.key.key);
                }
            }
            rohr_controller_key_event_add(
                &keyboard,
                rohr_controller_keyboard_event_capture(&event));
            rohr_controller_mouse_event_add(
                &mouse,
                rohr_controller_mouse_event_capture(&event));
            if(event.type == SDL_EVENT_QUIT) running = false;
        }
        if(!escape_name_edit_consumed &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            if(editor_viewport_hitbox_editor_active_get(&viewport_state)) {
                editor_viewport_back(&viewport_state);
            } else {
                running = false;
            }
        }
        if(!running) break;

        rohr_graphics_background_draw((Color){18, 21, 27, 255});
        (void)rohr_graphics_screen_rect_draw(
            0.0f, 0.0f, EDITOR_VIEWPORT_WIDTH, WINDOW_HEIGHT,
            (Color){25, 29, 37, 255});
        (void)rohr_graphics_screen_rect_draw(
            EDITOR_VIEWPORT_WIDTH, 0.0f, EDITOR_TOOLS_WIDTH, WINDOW_HEIGHT,
            (Color){38, 43, 53, 255});
        (void)rohr_graphics_screen_rect_draw(
            EDITOR_VIEWPORT_WIDTH, 0.0f, 1.0f, WINDOW_HEIGHT,
            (Color){75, 84, 100, 255});

        rohr_ui_frame_begin((UIInput){
            .pointer = rohr_graphics_mouse_screen_position_get(),
            .primary_button = mouse.button_states[MOUSE_BUTTON_LEFT]
        });
        rohr_ui_label(&tools_title,
            (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 14.0f,
                EDITOR_TOOLS_WIDTH - 16.0f, 24.0f});
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
                    snprintf(id, sizeof(id), "editor.vertex.%u", selected->hitbox.vertices[i].id);
                    if(rohr_ui_button(id, &vertex_labels[i],
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, y,
                                EDITOR_TOOLS_WIDTH - 26.0f, 23.0f}, NULL).clicked) {
                        editor_viewport_vertex_editor_enter(&viewport_state, i);
                    }
                }
                {
                    float base = 118.0f + (float)selected->hitbox.vertex_count * 27.0f;
                    rohr_ui_label(&lines_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        base, EDITOR_TOOLS_WIDTH - 20.0f, 24.0f});
                    for(uint32_t i = 0; i < selected->hitbox.vertex_count; i += 1) {
                        char id[64];
                        snprintf(id, sizeof(id), "editor.line.%u", i);
                        if(rohr_ui_button(id, &line_labels[i],
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f,
                                    base + 28.0f + (float)i * 27.0f,
                                    EDITOR_TOOLS_WIDTH - 26.0f, 23.0f}, NULL).clicked) {
                            editor_viewport_line_editor_enter(&viewport_state, i);
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
                    rohr_ui_button_disabled((UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 145.0f, 88.0f, 24.0f}, NULL);
                    rohr_ui_button_disabled((UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 215.0f, 88.0f, 24.0f}, NULL);
                } else {
                    vertex->position.x = rohr_ui_slider("editor.vertex.x", vertex->position.x, &slider).value;
                    slider.center.y = 227.0f;
                    vertex->position.y = rohr_ui_slider("editor.vertex.y", vertex->position.y, &slider).value;
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_LINE) {
            EditorObject *selected = editor_project_selected_get(&project);
            if(selected != NULL && viewport_state.selected_line < selected->hitbox.vertex_count) {
                uint32_t line = viewport_state.selected_line;
                EditorVertex *a = &selected->hitbox.vertices[line];
                EditorVertex *b = &selected->hitbox.vertices[(line + 1) % selected->hitbox.vertex_count];
                bool constrained = a->position_locked && b->position_locked;
                float length = editor_project_hitbox_line_length_get(selected, line);
                UISliderConfig slider = rohr_ui_slider_config_default_get();
                rohr_ui_label(&line_labels[line], (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    48.0f, EDITOR_TOOLS_WIDTH - 16.0f, 24.0f});
                if(rohr_ui_button("editor.line.add_vertex", &add_vertex_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 82.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, NULL).clicked &&
                        editor_project_hitbox_vertex_insert(&project, selected, line)) {
                    editor_viewport_hitbox_editor_enter(&viewport_state);
                }
                rohr_ui_label(&length_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    126.0f, EDITOR_TOOLS_WIDTH - 16.0f, 22.0f});
                if(constrained) {
                    rohr_ui_button_disabled((UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        154.0f, EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL);
                    rohr_ui_label(&constrained_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 5.0f,
                        190.0f, EDITOR_TOOLS_WIDTH - 10.0f, 38.0f});
                } else {
                    slider.center = (Position){EDITOR_VIEWPORT_WIDTH +
                        EDITOR_TOOLS_WIDTH * 0.5f, 170.0f};
                    slider.length = EDITOR_TOOLS_WIDTH - 36.0f;
                    slider.min_value = 5.0f;
                    slider.max_value = EDITOR_VIEWPORT_WIDTH;
                    (void)editor_project_hitbox_line_length_set(selected, line,
                        rohr_ui_slider("editor.line.length", length, &slider).value);
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
                if(rohr_ui_button("editor.object.name",
                        &object_name_labels[selected_index],
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 52.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, NULL).clicked) {
                    name_editing = true;
                }
                if(!selected->has_hitbox) {
                    if(rohr_ui_button("editor.add_hitbox", &add_hitbox_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 98.0f,
                                EDITOR_TOOLS_WIDTH - 20.0f, 36.0f}, NULL).clicked) {
                        editor_project_hitbox_add(&project, selected);
                        editor_viewport_hitbox_editor_enter(&viewport_state);
                    }
                } else if(rohr_ui_button("editor.object.hitbox", &hitbox_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, 100.0f,
                            EDITOR_TOOLS_WIDTH - 26.0f, 30.0f}, NULL).clicked) {
                    editor_viewport_hitbox_editor_enter(&viewport_state);
                }
            }
        } else {
            UIButtonResult add_object = rohr_ui_button(
                "editor.add_object", &add_object_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 58.0f,
                    EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
            if(add_object.clicked) {
                EditorObject *added = editor_project_object_add(
                    &project,
                    (Position){EDITOR_VIEWPORT_WIDTH * 0.5f,
                        WINDOW_HEIGHT * 0.5f});
                if(added != NULL) editor_viewport_object_editor_enter(&viewport_state);
            }
            rohr_ui_label(&hierarchy_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 108.0f,
                    EDITOR_TOOLS_WIDTH - 16.0f, 24.0f});
            for(size_t i = 0; i < project.object_count; i += 1) {
                EditorObject *object = &project.objects[i];
                float y = 142.0f + (float)i * 34.0f;
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
                object_result = rohr_ui_button(
                    object_button_id, &object_name_labels[i],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, y,
                        EDITOR_TOOLS_WIDTH - 16.0f, 28.0f}, NULL);
                if(object_result.clicked) {
                    (void)editor_project_object_select(&project, object->id);
                    editor_viewport_object_editor_enter(&viewport_state);
                }
            }
        }
        editor_viewport_update(
            &viewport_state,
            &project,
            rohr_graphics_mouse_screen_position_get(),
            mouse.button_states[MOUSE_BUTTON_LEFT],
            rohr_ui_pointer_consumed_get());
        editor_viewport_draw(&project, &viewport_state);
        rohr_ui_frame_end();
        rohr_graphics_show();
    }

    rohr_graphics_text_destroy(&vertices_label);
    rohr_graphics_text_destroy(&length_label);
    rohr_graphics_text_destroy(&y_label);
    rohr_graphics_text_destroy(&x_label);
    rohr_graphics_text_destroy(&constrained_label);
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
    rohr_graphics_text_destroy(&hierarchy_label);
    rohr_graphics_text_destroy(&tools_title);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_graphics_text_destroy(&vertices_label);
    rohr_graphics_text_destroy(&length_label);
    rohr_graphics_text_destroy(&y_label);
    rohr_graphics_text_destroy(&x_label);
    rohr_graphics_text_destroy(&constrained_label);
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
    rohr_graphics_text_destroy(&hierarchy_label);
    rohr_graphics_text_destroy(&tools_title);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
