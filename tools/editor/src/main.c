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

static bool editor_named_text_sync(FontAsset *font, const char *name,
    TextAsset *text, char *cache, size_t capacity) {
    if(font == NULL || name == NULL || text == NULL || cache == NULL || capacity == 0) {
        return false;
    }
    if(strcmp(cache, name) == 0) return true;
    rohr_graphics_text_destroy(text);
    if(!editor_text_create(font, name, text)) return false;
    snprintf(cache, capacity, "%s", name);
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

static EditorRigidBody *editor_selected_body_get(EditorObject *object,
    const EditorViewportState *state) {
    return object == NULL || state == NULL ? NULL :
        editor_project_rigid_body_get(object, state->selected_rigid_body);
}

static EditorHitbox *editor_selected_hitbox_get(EditorObject *object,
    const EditorViewportState *state) {
    EditorRigidBody *body = editor_selected_body_get(object, state);
    return body == NULL ? NULL : editor_project_hitbox_get(body, state->selected_hitbox);
}

static EditorRigidBodyId editor_body_id_next_get(const EditorObject *object,
    EditorRigidBodyId current) {
    if(object == NULL || object->rigid_body_count == 0) return 0;
    if(current == 0) return object->rigid_bodies[0].id;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id != current) continue;
        return i + 1 < object->rigid_body_count ? object->rigid_bodies[i + 1].id : 0;
    }
    return object->rigid_bodies[0].id;
}

static EditorJoint *editor_selected_joint_get(EditorObject *object,
    const EditorViewportState *state) {
    if(object == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        if(object->joint_items[i].id == state->selected_joint) {
            return &object->joint_items[i];
        }
    }
    return NULL;
}

static EditorSoftBody *editor_selected_soft_body_get(EditorObject *object,
    const EditorViewportState *state) {
    if(object == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        if(object->soft_body_items[i].id == state->selected_soft_body) {
            return &object->soft_body_items[i];
        }
    }
    return NULL;
}

static EditorSoftNode *editor_selected_soft_node_get(EditorSoftBody *body,
    const EditorViewportState *state) {
    if(body == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id == state->selected_soft_node) return &body->nodes[i];
    }
    return NULL;
}

static EditorSoftBeam *editor_selected_soft_beam_get(EditorSoftBody *body,
    const EditorViewportState *state) {
    if(body == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < body->beam_count; i += 1) {
        if(body->beams[i].id == state->selected_soft_beam) return &body->beams[i];
    }
    return NULL;
}

static EditorSoftNodeId editor_soft_node_id_next_get(const EditorSoftBody *body,
    EditorSoftNodeId current) {
    if(body == NULL || body->node_count == 0) return 0;
    if(current == 0) return body->nodes[0].id;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id != current) continue;
        return i + 1 < body->node_count ? body->nodes[i + 1].id : 0;
    }
    return body->nodes[0].id;
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
        EditorRigidBody *body = editor_selected_body_get(selected, viewport_state);
        if(body == NULL || !editor_project_hitbox_remove(
                body, viewport_state->selected_hitbox)) return false;
        viewport_state->mode = EDITOR_VIEWPORT_RIGID_BODY;
        viewport_state->selection = EDITOR_SELECTION_RIGID_BODY;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_RIGID_BODY) {
        if(!editor_project_rigid_body_remove(
                selected, viewport_state->selected_rigid_body)) return false;
        editor_viewport_object_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_JOINT) {
        if(!editor_project_joint_remove(selected, viewport_state->selected_joint)) return false;
        editor_viewport_object_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_BODY) {
        if(!editor_project_soft_body_remove(
                selected, viewport_state->selected_soft_body)) return false;
        editor_viewport_object_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_NODE) {
        EditorSoftBody *body = editor_selected_soft_body_get(selected, viewport_state);
        if(body == NULL || !editor_project_soft_node_remove(
                body, viewport_state->selected_soft_node)) return false;
        viewport_state->mode = EDITOR_VIEWPORT_SOFT_BODY;
        viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_BEAM) {
        EditorSoftBody *body = editor_selected_soft_body_get(selected, viewport_state);
        if(body == NULL || !editor_project_soft_beam_remove(
                body, viewport_state->selected_soft_beam)) return false;
        viewport_state->mode = EDITOR_VIEWPORT_SOFT_BODY;
        viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_VERTEX) {
        EditorHitbox *hitbox = editor_selected_hitbox_get(selected, viewport_state);
        if(!editor_project_hitbox_vertex_remove(
                hitbox, viewport_state->selected_vertex)) return false;
        editor_viewport_hitbox_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_LINE) {
        EditorHitbox *hitbox = editor_selected_hitbox_get(selected, viewport_state);
        if(!editor_project_hitbox_line_remove(
                hitbox, viewport_state->selected_line)) return false;
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
    if(viewport_state->selection == EDITOR_SELECTION_HITBOX &&
            editor_selected_hitbox_get(selected, viewport_state) != NULL) {
        editor_viewport_hitbox_editor_enter(viewport_state);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_RIGID_BODY &&
            editor_selected_body_get(selected, viewport_state) != NULL) {
        viewport_state->mode = EDITOR_VIEWPORT_RIGID_BODY;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_JOINT) {
        viewport_state->mode = EDITOR_VIEWPORT_JOINT;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_BODY) {
        viewport_state->mode = EDITOR_VIEWPORT_SOFT_BODY;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_NODE) {
        viewport_state->mode = EDITOR_VIEWPORT_SOFT_NODE;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_BEAM) {
        viewport_state->mode = EDITOR_VIEWPORT_SOFT_BEAM;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_VERTEX &&
            editor_selected_hitbox_get(selected, viewport_state) != NULL &&
            viewport_state->selected_vertex <
                editor_selected_hitbox_get(selected, viewport_state)->vertex_count) {
        editor_viewport_vertex_editor_enter(
            viewport_state, viewport_state->selected_vertex);
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_LINE &&
            editor_selected_hitbox_get(selected, viewport_state) != NULL &&
            viewport_state->selected_line <
                editor_selected_hitbox_get(selected, viewport_state)->vertex_count) {
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
    } else if(viewport_state->mode == EDITOR_VIEWPORT_RIGID_BODY) {
        viewport_state->selection = EDITOR_SELECTION_RIGID_BODY;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_HITBOX) {
        viewport_state->selection = EDITOR_SELECTION_HITBOX;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_VERTEX) {
        viewport_state->selection = EDITOR_SELECTION_VERTEX;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_LINE) {
        viewport_state->selection = EDITOR_SELECTION_LINE;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_JOINT) {
        viewport_state->selection = EDITOR_SELECTION_JOINT;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_SOFT_BODY) {
        viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_SOFT_NODE) {
        viewport_state->selection = EDITOR_SELECTION_SOFT_NODE;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_SOFT_BEAM) {
        viewport_state->selection = EDITOR_SELECTION_SOFT_BEAM;
    } else {
        return false;
    }
    return editor_selected_delete(project, viewport_state);
}

static void editor_current_selection_clear(
    EditorProject *project,
    EditorViewportState *viewport_state
) {
    if(project == NULL || viewport_state == NULL) return;
    if(viewport_state->mode == EDITOR_VIEWPORT_HIERARCHY) {
        editor_project_selection_clear(project);
        viewport_state->selection = EDITOR_SELECTION_NONE;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_OBJECT) {
        viewport_state->selection = EDITOR_SELECTION_OBJECT;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_RIGID_BODY) {
        viewport_state->selection = EDITOR_SELECTION_RIGID_BODY;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_HITBOX) {
        viewport_state->selection = EDITOR_SELECTION_HITBOX;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_VERTEX) {
        viewport_state->selection = EDITOR_SELECTION_VERTEX;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_LINE) {
        viewport_state->selection = EDITOR_SELECTION_LINE;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_JOINT) {
        viewport_state->selection = EDITOR_SELECTION_JOINT;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_SOFT_BODY) {
        viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_SOFT_NODE) {
        viewport_state->selection = EDITOR_SELECTION_SOFT_NODE;
    } else if(viewport_state->mode == EDITOR_VIEWPORT_SOFT_BEAM) {
        viewport_state->selection = EDITOR_SELECTION_SOFT_BEAM;
    }
}

int main(void) {
    KeyboardState keyboard = {0};
    MouseState mouse = {0};
    FontAsset font = {0};
    TextAsset hitbox_editor_label = {0};
    TextAsset add_object_label = {0};
    TextAsset add_hitbox_label = {0};
    TextAsset hitbox_label = {0};
    TextAsset add_rigid_body_label = {0};
    TextAsset add_joint_label = {0};
    TextAsset add_soft_body_label = {0};
    TextAsset add_node_label = {0};
    TextAsset add_beam_label = {0};
    TextAsset visible_label = {0};
    TextAsset hidden_label = {0};
    TextAsset revolute_label = {0};
    TextAsset weld_label = {0};
    TextAsset spring_label = {0};
    TextAsset body_a_label = {0};
    TextAsset body_b_label = {0};
    TextAsset node_a_label = {0};
    TextAsset node_b_label = {0};
    TextAsset mass_label = {0};
    TextAsset stiffness_label = {0};
    TextAsset joint_body_a_display = {0};
    TextAsset joint_body_b_display = {0};
    TextAsset beam_node_a_display = {0};
    TextAsset beam_node_b_display = {0};
    TextAsset rigid_body_labels[EDITOR_RIGID_BODY_MAX] = {0};
    TextAsset body_hitbox_labels[EDITOR_BODY_HITBOX_MAX] = {0};
    TextAsset joint_labels[EDITOR_JOINT_MAX] = {0};
    TextAsset soft_body_labels[EDITOR_SOFT_BODY_MAX] = {0};
    TextAsset soft_node_labels[EDITOR_SOFT_NODE_MAX] = {0};
    TextAsset soft_beam_labels[EDITOR_SOFT_BEAM_MAX] = {0};
    char rigid_body_cache[EDITOR_RIGID_BODY_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char body_hitbox_cache[EDITOR_BODY_HITBOX_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char joint_cache[EDITOR_JOINT_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char soft_body_cache[EDITOR_SOFT_BODY_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char soft_node_cache[EDITOR_SOFT_NODE_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char soft_beam_cache[EDITOR_SOFT_BEAM_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
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
    TextAsset delete_rigid_body_label = {0};
    TextAsset delete_joint_label = {0};
    TextAsset delete_soft_body_label = {0};
    TextAsset delete_node_label = {0};
    TextAsset delete_beam_label = {0};
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
    static EditorProject project;
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
            !editor_text_create(&font, "Add Rigid Body", &add_rigid_body_label) ||
            !editor_text_create(&font, "Add Joint", &add_joint_label) ||
            !editor_text_create(&font, "Add Soft Body", &add_soft_body_label) ||
            !editor_text_create(&font, "Add Node", &add_node_label) ||
            !editor_text_create(&font, "Add Beam", &add_beam_label) ||
            !editor_text_create(&font, "Visible", &visible_label) ||
            !editor_text_create(&font, "Hidden", &hidden_label) ||
            !editor_text_create(&font, "Revolute", &revolute_label) ||
            !editor_text_create(&font, "Weld", &weld_label) ||
            !editor_text_create(&font, "Spring", &spring_label) ||
            !editor_text_create(&font, "Body A", &body_a_label) ||
            !editor_text_create(&font, "Body B", &body_b_label) ||
            !editor_text_create(&font, "Node A", &node_a_label) ||
            !editor_text_create(&font, "Node B", &node_b_label) ||
            !editor_text_create(&font, "Mass", &mass_label) ||
            !editor_text_create(&font, "Stiffness", &stiffness_label) ||
            !editor_text_create(&font, "", &joint_body_a_display) ||
            !editor_text_create(&font, "", &joint_body_b_display) ||
            !editor_text_create(&font, "", &beam_node_a_display) ||
            !editor_text_create(&font, "", &beam_node_b_display) ||
            !editor_text_create(&font, "Vertices", &vertices_label) ||
            !editor_text_create(&font, "Lines", &lines_label) ||
            !editor_text_create(&font, "Lock Position", &lock_label) ||
            !editor_text_create(&font, "Unlock Position", &unlock_label) ||
            !editor_text_create(&font, "Add Vertex", &add_vertex_label) ||
            !editor_text_create(&font, "Delete Hitbox", &delete_hitbox_label) ||
            !editor_text_create(&font, "Delete Vertex", &delete_vertex_label) ||
            !editor_text_create(&font, "Delete Line", &delete_line_label) ||
            !editor_text_create(&font, "Delete Object", &delete_object_label) ||
            !editor_text_create(&font, "Delete Rigid Body", &delete_rigid_body_label) ||
            !editor_text_create(&font, "Delete Joint", &delete_joint_label) ||
            !editor_text_create(&font, "Delete Soft Body", &delete_soft_body_label) ||
            !editor_text_create(&font, "Delete Node", &delete_node_label) ||
            !editor_text_create(&font, "Delete Beam", &delete_beam_label) ||
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
        if(viewport_state.mode == EDITOR_VIEWPORT_RIGID_BODY) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorRigidBody *body = editor_selected_body_get(selected, &viewport_state);
            if(body != NULL) {
                size_t body_index = (size_t)(body - selected->rigid_bodies);
                if(!editor_named_text_sync(&font, body->name,
                        &rigid_body_labels[body_index], rigid_body_cache[body_index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&rigid_body_labels[body_index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 42.0f,
                        EDITOR_TOOLS_WIDTH - 16.0f, 30.0f});
                if(rohr_ui_button("editor.rigid_body.visibility",
                        body->visible ? &visible_label : &hidden_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 80.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    body->visible = !body->visible;
                }
                if(rohr_ui_button("editor.rigid_body.add_hitbox", &add_hitbox_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 118.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    EditorHitbox *added = editor_project_hitbox_add(&project, body);
                    if(added != NULL) {
                        viewport_state.selection = EDITOR_SELECTION_HITBOX;
                        viewport_state.selected_hitbox = added->id;
                    }
                }
                for(size_t i = 0; i < body->hitbox_count; i += 1) {
                    EditorHitbox *box = &body->hitboxes[i];
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    UIButtonResult result;
                    char id[64];
                    if(!editor_named_text_sync(&font, box->name, &body_hitbox_labels[i],
                            body_hitbox_cache[i], EDITOR_OBJECT_NAME_MAX)) goto fail;
                    snprintf(id, sizeof(id), "editor.hitbox.%u", box->id);
                    result = rohr_ui_button(id, &body_hitbox_labels[i],
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f,
                            164.0f + (float)i * 30.0f,
                            EDITOR_TOOLS_WIDTH - 26.0f, 26.0f},
                        viewport_state.selection == EDITOR_SELECTION_HITBOX &&
                            viewport_state.selected_hitbox == box->id ?
                            &selected_style : NULL);
                    if(result.clicked) {
                        viewport_state.selection = EDITOR_SELECTION_HITBOX;
                        viewport_state.selected_hitbox = box->id;
                        if(result.double_clicked) (void)editor_selected_open(
                            &project, &viewport_state);
                    }
                }
                {
                    UIButtonStyle style = editor_delete_button_style_get();
                    if(rohr_ui_button("editor.rigid_body.delete", &delete_rigid_body_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 660.0f,
                                EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, &style).clicked) {
                        (void)editor_open_item_delete(&project, &viewport_state);
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_HITBOX) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorHitbox *hitbox = editor_selected_hitbox_get(selected, &viewport_state);

            rohr_ui_label(&hitbox_editor_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 48.0f,
                    EDITOR_TOOLS_WIDTH - 16.0f, 24.0f});
            if(hitbox != NULL && rohr_ui_button("editor.hitbox.visibility",
                    hitbox->visible ? &visible_label : &hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 76.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL).clicked) {
                hitbox->visible = !hitbox->visible;
            }
            rohr_ui_label(&vertices_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 110.0f,
                    EDITOR_TOOLS_WIDTH - 20.0f, 24.0f});
            if(hitbox != NULL) {
                for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
                    char id[64];
                    float y = 138.0f + (float)i * 27.0f;
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    const UIButtonStyle *style = viewport_state.selection ==
                        EDITOR_SELECTION_VERTEX && viewport_state.selected_vertex == i ?
                        &selected_style : NULL;
                    UIButtonResult result;
                    snprintf(id, sizeof(id), "editor.vertex.%u", hitbox->vertices[i].id);
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
                    float base = 146.0f + (float)hitbox->vertex_count * 27.0f;
                    rohr_ui_label(&lines_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        base, EDITOR_TOOLS_WIDTH - 20.0f, 24.0f});
                    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
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
                            (float)hitbox->vertex_count * 27.0f;

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
            EditorHitbox *hitbox = editor_selected_hitbox_get(selected, &viewport_state);
            if(hitbox != NULL && viewport_state.selected_vertex < hitbox->vertex_count) {
                EditorVertex *vertex = &hitbox->vertices[viewport_state.selected_vertex];
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
                    if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN) {
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
            EditorHitbox *hitbox = editor_selected_hitbox_get(selected, &viewport_state);
            if(hitbox != NULL && viewport_state.selected_line < hitbox->vertex_count) {
                uint32_t line = viewport_state.selected_line;
                EditorVertex *a = &hitbox->vertices[line];
                EditorVertex *b = &hitbox->vertices[(line + 1) % hitbox->vertex_count];
                bool constrained = a->position_locked && b->position_locked;
                bool vertex_inserted = false;
                float length = editor_project_hitbox_line_length_get(hitbox, line);
                UISliderConfig slider = rohr_ui_slider_config_default_get();
                rohr_ui_label(&line_labels[line], (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    48.0f, EDITOR_TOOLS_WIDTH - 16.0f, 24.0f});
                if(rohr_ui_button("editor.line.add_vertex", &add_vertex_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 82.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, NULL).clicked &&
                        editor_project_hitbox_vertex_insert(&project, hitbox, line)) {
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
                                hitbox, line, length);
                        }
                        slider.center = (Position){EDITOR_VIEWPORT_WIDTH +
                            EDITOR_TOOLS_WIDTH * 0.5f, 202.0f};
                        slider.length = EDITOR_TOOLS_WIDTH - 36.0f;
                        slider.min_value = 5.0f;
                        slider.max_value = EDITOR_VIEWPORT_WIDTH;
                        (void)editor_project_hitbox_line_length_set(hitbox, line,
                            rohr_ui_slider("editor.line.length", length, &slider).value);
                    }
                    {
                        UIButtonStyle delete_style = editor_delete_button_style_get();
                        UIRect delete_bounds = {EDITOR_VIEWPORT_WIDTH + 10.0f, 238.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f};
                        if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN) {
                            rohr_ui_button_disabled(delete_bounds, &delete_style);
                            rohr_ui_label(&delete_line_label, delete_bounds);
                        } else if(rohr_ui_button("editor.line.delete", &delete_line_label,
                                delete_bounds, &delete_style).clicked) {
                            (void)editor_open_item_delete(&project, &viewport_state);
                        }
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_JOINT) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorJoint *joint = editor_selected_joint_get(selected, &viewport_state);
            if(joint != NULL) {
                size_t index = (size_t)(joint - selected->joint_items);
                const TextAsset *kind_label = joint->kind == EDITOR_JOINT_WELD ?
                    &weld_label : (joint->kind == EDITOR_JOINT_SPRING ?
                        &spring_label : &revolute_label);
                EditorRigidBody *body_a = editor_project_rigid_body_get(selected, joint->body_a);
                EditorRigidBody *body_b = editor_project_rigid_body_get(selected, joint->body_b);
                UIButtonStyle delete_style = editor_delete_button_style_get();
                if(!editor_named_text_sync(&font, joint->name, &joint_labels[index],
                        joint_cache[index], EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&joint_labels[index], (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    42.0f, EDITOR_TOOLS_WIDTH - 16.0f, 30.0f});
                if(rohr_ui_button("editor.joint.visibility",
                        joint->visible ? &visible_label : &hidden_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 80.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    joint->visible = !joint->visible;
                }
                if(rohr_ui_button("editor.joint.kind", kind_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 118.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    joint->kind = (EditorJointKind)((joint->kind + 1) % 3);
                }
                (void)rohr_graphics_text_value_set(&joint_body_a_display,
                    body_a == NULL ? "None" : body_a->name);
                (void)rohr_graphics_text_value_set(&joint_body_b_display,
                    body_b == NULL ? "None" : body_b->name);
                rohr_ui_label(&body_a_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    158.0f, 55.0f, 28.0f});
                rohr_ui_label(&body_b_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    194.0f, 55.0f, 28.0f});
                if(rohr_ui_button("editor.joint.body_a", &joint_body_a_display,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 63.0f, 158.0f,
                            EDITOR_TOOLS_WIDTH - 73.0f, 28.0f}, NULL).clicked) {
                    joint->body_a = editor_body_id_next_get(selected, joint->body_a);
                }
                if(rohr_ui_button("editor.joint.body_b", &joint_body_b_display,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 63.0f, 194.0f,
                            EDITOR_TOOLS_WIDTH - 73.0f, 28.0f}, NULL).clicked) {
                    joint->body_b = editor_body_id_next_get(selected, joint->body_b);
                }
                if(rohr_ui_button("editor.joint.delete", &delete_joint_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 660.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f},
                        &delete_style).clicked) {
                    (void)editor_open_item_delete(&project, &viewport_state);
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_BODY) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorSoftBody *body = editor_selected_soft_body_get(selected, &viewport_state);
            if(body != NULL) {
                size_t body_index = (size_t)(body - selected->soft_body_items);
                UIButtonStyle delete_style = editor_delete_button_style_get();
                if(!editor_named_text_sync(&font, body->name,
                        &soft_body_labels[body_index], soft_body_cache[body_index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&soft_body_labels[body_index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 42.0f,
                        EDITOR_TOOLS_WIDTH - 16.0f, 30.0f});
                if(rohr_ui_button("editor.soft_body.visibility",
                        body->visible ? &visible_label : &hidden_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 80.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    body->visible = !body->visible;
                }
                if(rohr_ui_button("editor.soft_body.add_node", &add_node_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 118.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    EditorSoftNode *node = editor_project_soft_node_add(&project, body,
                        (Position){(float)body->node_count * 24.0f, 0.0f});
                    if(node != NULL) {
                        viewport_state.selection = EDITOR_SELECTION_SOFT_NODE;
                        viewport_state.selected_soft_node = node->id;
                    }
                }
                if(rohr_ui_button("editor.soft_body.add_beam", &add_beam_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 154.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked &&
                        body->node_count >= 2) {
                    EditorSoftBeam *beam = editor_project_soft_beam_add(&project, body,
                        body->nodes[body->node_count - 2].id,
                        body->nodes[body->node_count - 1].id);
                    if(beam != NULL) {
                        viewport_state.selection = EDITOR_SELECTION_SOFT_BEAM;
                        viewport_state.selected_soft_beam = beam->id;
                    }
                }
                {
                    float y = 198.0f;
                    for(size_t i = 0; i < body->node_count; i += 1, y += 28.0f) {
                        EditorSoftNode *node = &body->nodes[i];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        char id[64];
                        if(!editor_named_text_sync(&font, node->name, &soft_node_labels[i],
                                soft_node_cache[i], EDITOR_OBJECT_NAME_MAX)) goto fail;
                        snprintf(id, sizeof(id), "editor.soft_node.%u", node->id);
                        {
                            UIButtonResult result = rohr_ui_button(id, &soft_node_labels[i],
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, y,
                                    EDITOR_TOOLS_WIDTH - 26.0f, 24.0f},
                                viewport_state.selection == EDITOR_SELECTION_SOFT_NODE &&
                                    viewport_state.selected_soft_node == node->id ?
                                    &selected_style : NULL);
                            if(result.clicked) {
                                viewport_state.selection = EDITOR_SELECTION_SOFT_NODE;
                                viewport_state.selected_soft_node = node->id;
                                if(result.double_clicked) (void)editor_selected_open(
                                    &project, &viewport_state);
                            }
                        }
                    }
                    for(size_t i = 0; i < body->beam_count; i += 1, y += 28.0f) {
                        EditorSoftBeam *beam = &body->beams[i];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        char id[64];
                        if(!editor_named_text_sync(&font, beam->name, &soft_beam_labels[i],
                                soft_beam_cache[i], EDITOR_OBJECT_NAME_MAX)) goto fail;
                        snprintf(id, sizeof(id), "editor.soft_beam.%u", beam->id);
                        {
                            UIButtonResult result = rohr_ui_button(id, &soft_beam_labels[i],
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, y,
                                    EDITOR_TOOLS_WIDTH - 26.0f, 24.0f},
                                viewport_state.selection == EDITOR_SELECTION_SOFT_BEAM &&
                                    viewport_state.selected_soft_beam == beam->id ?
                                    &selected_style : NULL);
                            if(result.clicked) {
                                viewport_state.selection = EDITOR_SELECTION_SOFT_BEAM;
                                viewport_state.selected_soft_beam = beam->id;
                                if(result.double_clicked) (void)editor_selected_open(
                                    &project, &viewport_state);
                            }
                        }
                    }
                }
                if(rohr_ui_button("editor.soft_body.delete", &delete_soft_body_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 660.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f},
                        &delete_style).clicked) {
                    (void)editor_open_item_delete(&project, &viewport_state);
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_NODE) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorSoftBody *body = editor_selected_soft_body_get(selected, &viewport_state);
            EditorSoftNode *node = editor_selected_soft_node_get(body, &viewport_state);
            if(node != NULL) {
                size_t index = (size_t)(node - body->nodes);
                UIButtonStyle delete_style = editor_delete_button_style_get();
                UIFieldResult x_result;
                UIFieldResult y_result;
                UIFieldResult mass_result;
                if(!editor_named_text_sync(&font, node->name, &soft_node_labels[index],
                        soft_node_cache[index], EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&soft_node_labels[index], (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    42.0f, EDITOR_TOOLS_WIDTH - 16.0f, 30.0f});
                if(rohr_ui_button("editor.soft_node.visibility",
                        node->visible ? &visible_label : &hidden_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 80.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    node->visible = !node->visible;
                }
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 122.0f, 50.0f, 26.0f});
                x_result = rohr_ui_field("editor.soft_node.x",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &node->position.x},
                    &x_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f, 122.0f,
                        EDITOR_TOOLS_WIDTH - 70.0f, 26.0f}, NULL);
                rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 158.0f, 50.0f, 26.0f});
                y_result = rohr_ui_field("editor.soft_node.y",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &node->position.y},
                    &y_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f, 158.0f,
                        EDITOR_TOOLS_WIDTH - 70.0f, 26.0f}, NULL);
                rohr_ui_label(&mass_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 194.0f, 68.0f, 26.0f});
                mass_result = rohr_ui_field("editor.soft_node.mass",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &node->node_mass},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 78.0f, 194.0f,
                        EDITOR_TOOLS_WIDTH - 88.0f, 26.0f}, NULL);
                field_editing = x_result.active || y_result.active || mass_result.active;
                if(rohr_ui_button("editor.soft_node.delete", &delete_node_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 660.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, &delete_style).clicked) {
                    (void)editor_open_item_delete(&project, &viewport_state);
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_BEAM) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorSoftBody *body = editor_selected_soft_body_get(selected, &viewport_state);
            EditorSoftBeam *beam = editor_selected_soft_beam_get(body, &viewport_state);
            if(beam != NULL) {
                size_t index = (size_t)(beam - body->beams);
                UIButtonStyle delete_style = editor_delete_button_style_get();
                UIFieldResult stiffness_result;
                EditorSoftNode *node_a = NULL;
                EditorSoftNode *node_b = NULL;
                for(size_t i = 0; i < body->node_count; i += 1) {
                    if(body->nodes[i].id == beam->node_a) node_a = &body->nodes[i];
                    if(body->nodes[i].id == beam->node_b) node_b = &body->nodes[i];
                }
                if(!editor_named_text_sync(&font, beam->name, &soft_beam_labels[index],
                        soft_beam_cache[index], EDITOR_OBJECT_NAME_MAX) ||
                        !rohr_graphics_text_value_set(&beam_node_a_display,
                            node_a == NULL ? "None" : node_a->name) ||
                        !rohr_graphics_text_value_set(&beam_node_b_display,
                            node_b == NULL ? "None" : node_b->name)) goto fail;
                rohr_ui_label(&soft_beam_labels[index], (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    42.0f, EDITOR_TOOLS_WIDTH - 16.0f, 30.0f});
                if(rohr_ui_button("editor.soft_beam.visibility",
                        beam->visible ? &visible_label : &hidden_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 80.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    beam->visible = !beam->visible;
                }
                rohr_ui_label(&node_a_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 122.0f, 70.0f, 28.0f});
                if(rohr_ui_button("editor.soft_beam.node_a", &beam_node_a_display,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 80.0f, 122.0f,
                            EDITOR_TOOLS_WIDTH - 90.0f, 28.0f}, NULL).clicked) {
                    beam->node_a = editor_soft_node_id_next_get(body, beam->node_a);
                }
                rohr_ui_label(&node_b_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 158.0f, 70.0f, 28.0f});
                if(rohr_ui_button("editor.soft_beam.node_b", &beam_node_b_display,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 80.0f, 158.0f,
                            EDITOR_TOOLS_WIDTH - 90.0f, 28.0f}, NULL).clicked) {
                    beam->node_b = editor_soft_node_id_next_get(body, beam->node_b);
                }
                rohr_ui_label(&stiffness_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    196.0f, 90.0f, 26.0f});
                stiffness_result = rohr_ui_field("editor.soft_beam.stiffness",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &beam->stiffness},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 196.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 26.0f}, NULL);
                field_editing = stiffness_result.active;
                if(rohr_ui_button("editor.soft_beam.delete", &delete_beam_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 660.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, &delete_style).clicked) {
                    (void)editor_open_item_delete(&project, &viewport_state);
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
                if(rohr_ui_button("editor.object.visibility",
                        selected->visible ? &visible_label : &hidden_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 92.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL).clicked) {
                    selected->visible = !selected->visible;
                }
                if(rohr_ui_button("editor.add_rigid_body", &add_rigid_body_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 128.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    EditorRigidBody *body = editor_project_rigid_body_add(&project, selected);
                    if(body != NULL) {
                        viewport_state.selection = EDITOR_SELECTION_RIGID_BODY;
                        viewport_state.selected_rigid_body = body->id;
                    }
                }
                if(rohr_ui_button("editor.add_joint", &add_joint_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 166.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    EditorJoint *joint = editor_project_joint_add(
                        &project, selected, EDITOR_JOINT_REVOLUTE);
                    if(joint != NULL) {
                        viewport_state.selection = EDITOR_SELECTION_JOINT;
                        viewport_state.selected_joint = joint->id;
                    }
                }
                if(rohr_ui_button("editor.add_soft_body", &add_soft_body_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 204.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    EditorSoftBody *body = editor_project_soft_body_add(&project, selected);
                    if(body != NULL) {
                        viewport_state.selection = EDITOR_SELECTION_SOFT_BODY;
                        viewport_state.selected_soft_body = body->id;
                    }
                }
                {
                    float y = 250.0f;
                    for(size_t i = 0; i < selected->rigid_body_count; i += 1, y += 30.0f) {
                        EditorRigidBody *body = &selected->rigid_bodies[i];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        UIButtonResult result;
                        char id[64];
                        if(!editor_named_text_sync(&font, body->name,
                                &rigid_body_labels[i], rigid_body_cache[i],
                                EDITOR_OBJECT_NAME_MAX)) goto fail;
                        snprintf(id, sizeof(id), "editor.rigid_body.%u", body->id);
                        result = rohr_ui_button(id, &rigid_body_labels[i],
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, y,
                                EDITOR_TOOLS_WIDTH - 26.0f, 26.0f},
                            viewport_state.selection == EDITOR_SELECTION_RIGID_BODY &&
                                viewport_state.selected_rigid_body == body->id ?
                                &selected_style : NULL);
                        if(result.clicked) {
                            viewport_state.selection = EDITOR_SELECTION_RIGID_BODY;
                            viewport_state.selected_rigid_body = body->id;
                            if(result.double_clicked) (void)editor_selected_open(
                                &project, &viewport_state);
                        }
                    }
                    for(size_t i = 0; i < selected->joint_count; i += 1, y += 30.0f) {
                        EditorJoint *joint = &selected->joint_items[i];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        UIButtonResult result;
                        char id[64];
                        if(!editor_named_text_sync(&font, joint->name, &joint_labels[i],
                                joint_cache[i], EDITOR_OBJECT_NAME_MAX)) goto fail;
                        snprintf(id, sizeof(id), "editor.joint.%u", joint->id);
                        result = rohr_ui_button(id, &joint_labels[i],
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, y,
                                EDITOR_TOOLS_WIDTH - 26.0f, 26.0f},
                            viewport_state.selection == EDITOR_SELECTION_JOINT &&
                                viewport_state.selected_joint == joint->id ?
                                &selected_style : NULL);
                        if(result.clicked) {
                            viewport_state.selection = EDITOR_SELECTION_JOINT;
                            viewport_state.selected_joint = joint->id;
                            if(result.double_clicked) (void)editor_selected_open(
                                &project, &viewport_state);
                        }
                    }
                    for(size_t i = 0; i < selected->soft_body_count; i += 1, y += 30.0f) {
                        EditorSoftBody *body = &selected->soft_body_items[i];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        UIButtonResult result;
                        char id[64];
                        if(!editor_named_text_sync(&font, body->name, &soft_body_labels[i],
                                soft_body_cache[i], EDITOR_OBJECT_NAME_MAX)) goto fail;
                        snprintf(id, sizeof(id), "editor.soft_body.%u", body->id);
                        result = rohr_ui_button(id, &soft_body_labels[i],
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, y,
                                EDITOR_TOOLS_WIDTH - 26.0f, 26.0f},
                            viewport_state.selection == EDITOR_SELECTION_SOFT_BODY &&
                                viewport_state.selected_soft_body == body->id ?
                                &selected_style : NULL);
                        if(result.clicked) {
                            viewport_state.selection = EDITOR_SELECTION_SOFT_BODY;
                            viewport_state.selected_soft_body = body->id;
                            if(result.double_clicked) (void)editor_selected_open(
                                &project, &viewport_state);
                        }
                    }
                }
                {
                    UIButtonStyle delete_style = editor_delete_button_style_get();
                    if(rohr_ui_button("editor.object.delete", &delete_object_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 660.0f,
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
        {
            bool ui_consumed = rohr_ui_pointer_consumed_get();
            bool viewport_consumed = editor_viewport_update(
                &viewport_state,
                &project,
                rohr_graphics_mouse_screen_position_get(),
                mouse.button_states[MOUSE_BUTTON_LEFT],
                ui_consumed);

            if(mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED &&
                    !ui_consumed && !viewport_consumed && !panel_resizing) {
                editor_current_selection_clear(&project, &viewport_state);
            }
        }
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

    rohr_graphics_text_destroy(&beam_node_b_display);
    rohr_graphics_text_destroy(&beam_node_a_display);
    rohr_graphics_text_destroy(&joint_body_b_display);
    rohr_graphics_text_destroy(&joint_body_a_display);
    rohr_graphics_text_destroy(&stiffness_label);
    rohr_graphics_text_destroy(&mass_label);
    rohr_graphics_text_destroy(&node_b_label);
    rohr_graphics_text_destroy(&node_a_label);
    rohr_graphics_text_destroy(&body_b_label);
    rohr_graphics_text_destroy(&body_a_label);
    rohr_graphics_text_destroy(&spring_label);
    rohr_graphics_text_destroy(&weld_label);
    rohr_graphics_text_destroy(&revolute_label);
    rohr_graphics_text_destroy(&hidden_label);
    rohr_graphics_text_destroy(&visible_label);
    rohr_graphics_text_destroy(&add_beam_label);
    rohr_graphics_text_destroy(&add_node_label);
    rohr_graphics_text_destroy(&add_soft_body_label);
    rohr_graphics_text_destroy(&add_joint_label);
    rohr_graphics_text_destroy(&add_rigid_body_label);
    rohr_graphics_text_destroy(&delete_beam_label);
    rohr_graphics_text_destroy(&delete_node_label);
    rohr_graphics_text_destroy(&delete_soft_body_label);
    rohr_graphics_text_destroy(&delete_joint_label);
    rohr_graphics_text_destroy(&delete_rigid_body_label);
    for(size_t i = 0; i < EDITOR_RIGID_BODY_MAX; i += 1) {
        rohr_graphics_text_destroy(&rigid_body_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_BODY_HITBOX_MAX; i += 1) {
        rohr_graphics_text_destroy(&body_hitbox_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_JOINT_MAX; i += 1) {
        rohr_graphics_text_destroy(&joint_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_BODY_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_body_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_NODE_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_node_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_BEAM_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_beam_labels[i]);
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
    rohr_graphics_text_destroy(&beam_node_b_display);
    rohr_graphics_text_destroy(&beam_node_a_display);
    rohr_graphics_text_destroy(&joint_body_b_display);
    rohr_graphics_text_destroy(&joint_body_a_display);
    rohr_graphics_text_destroy(&stiffness_label);
    rohr_graphics_text_destroy(&mass_label);
    rohr_graphics_text_destroy(&node_b_label);
    rohr_graphics_text_destroy(&node_a_label);
    rohr_graphics_text_destroy(&body_b_label);
    rohr_graphics_text_destroy(&body_a_label);
    rohr_graphics_text_destroy(&spring_label);
    rohr_graphics_text_destroy(&weld_label);
    rohr_graphics_text_destroy(&revolute_label);
    rohr_graphics_text_destroy(&hidden_label);
    rohr_graphics_text_destroy(&visible_label);
    rohr_graphics_text_destroy(&add_beam_label);
    rohr_graphics_text_destroy(&add_node_label);
    rohr_graphics_text_destroy(&add_soft_body_label);
    rohr_graphics_text_destroy(&add_joint_label);
    rohr_graphics_text_destroy(&add_rigid_body_label);
    rohr_graphics_text_destroy(&delete_beam_label);
    rohr_graphics_text_destroy(&delete_node_label);
    rohr_graphics_text_destroy(&delete_soft_body_label);
    rohr_graphics_text_destroy(&delete_joint_label);
    rohr_graphics_text_destroy(&delete_rigid_body_label);
    for(size_t i = 0; i < EDITOR_RIGID_BODY_MAX; i += 1) {
        rohr_graphics_text_destroy(&rigid_body_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_BODY_HITBOX_MAX; i += 1) {
        rohr_graphics_text_destroy(&body_hitbox_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_JOINT_MAX; i += 1) {
        rohr_graphics_text_destroy(&joint_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_BODY_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_body_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_NODE_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_node_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_BEAM_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_beam_labels[i]);
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
    return 1;
}
