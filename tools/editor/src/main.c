#include "rohr.h"
#include "editor_project.h"
#include "editor_viewport.h"
#include "editor_layout.h"

#include <stdio.h>

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

int main(void) {
    KeyboardState keyboard = {0};
    MouseState mouse = {0};
    FontAsset font = {0};
    TextAsset tools_title = {0};
    TextAsset add_entity_label = {0};
    TextAsset add_hitbox_label = {0};
    TextAsset vertices_label = {0};
    ViewportId viewport = 0;
    EditorProject project;
    EditorViewportState viewport_state;
    float vertex_count = 4.0f;
    bool running = true;

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
            !editor_text_create(&font, "Add Entity", &add_entity_label) ||
            !editor_text_create(&font, "Add Hitbox", &add_hitbox_label) ||
            !editor_text_create(&font, "Vertices", &vertices_label)) goto fail;

    while(running) {
        SDL_Event event;

        rohr_controller_key_states_update(&keyboard);
        rohr_controller_mouse_states_update(&mouse);
        while((event = rohr_engine_event_poll()).type != 0) {
            rohr_controller_key_event_add(
                &keyboard,
                rohr_controller_keyboard_event_capture(&event));
            rohr_controller_mouse_event_add(
                &mouse,
                rohr_controller_mouse_event_capture(&event));
            if(event.type == SDL_EVENT_QUIT) running = false;
        }
        if(rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) running = false;
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
        {
            UIButtonResult add_entity = rohr_ui_button(
                "editor.add_entity", &add_entity_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 58.0f,
                    EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
            EditorObject *selected = editor_project_selected_get(&project);
            UIButtonResult add_hitbox;

            if(add_entity.clicked) {
                selected = editor_project_object_add(
                    &project,
                    (Position){EDITOR_VIEWPORT_WIDTH * 0.5f,
                        WINDOW_HEIGHT * 0.5f});
                vertex_count = 4.0f;
            }
            if(selected != NULL && !selected->has_hitbox) {
                add_hitbox = rohr_ui_button(
                    "editor.add_hitbox", &add_hitbox_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 106.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
                if(add_hitbox.clicked) {
                    editor_project_hitbox_add(selected, (uint32_t)vertex_count);
                }
            } else {
                rohr_ui_button_disabled(
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 106.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
                rohr_ui_label(&add_hitbox_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 106.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 38.0f});
            }
        }
        rohr_ui_label(&vertices_label,
            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 168.0f,
                EDITOR_TOOLS_WIDTH - 20.0f, 24.0f});
        {
            UISliderConfig slider = rohr_ui_slider_config_default_get();
            UISliderResult result;
            EditorObject *selected = editor_project_selected_get(&project);

            slider.center = (Position){EDITOR_VIEWPORT_WIDTH +
                EDITOR_TOOLS_WIDTH * 0.5f, 215.0f};
            slider.length = EDITOR_TOOLS_WIDTH - 36.0f;
            slider.min_value = EDITOR_HITBOX_VERTEX_MIN;
            slider.max_value = EDITOR_HITBOX_VERTEX_MAX;
            slider.step = 1.0f;
            result = rohr_ui_slider("editor.hitbox.vertices", vertex_count, &slider);
            vertex_count = result.value;
            if(result.changed && selected != NULL && selected->has_hitbox) {
                editor_project_hitbox_vertex_count_set(
                    selected, (uint32_t)vertex_count);
            }
        }
        editor_viewport_update(
            &viewport_state,
            &project,
            rohr_graphics_mouse_screen_position_get(),
            mouse.button_states[MOUSE_BUTTON_LEFT],
            rohr_ui_pointer_consumed_get());
        editor_viewport_draw(&project);
        rohr_ui_frame_end();
        rohr_graphics_show();
    }

    rohr_graphics_text_destroy(&vertices_label);
    rohr_graphics_text_destroy(&add_hitbox_label);
    rohr_graphics_text_destroy(&add_entity_label);
    rohr_graphics_text_destroy(&tools_title);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_graphics_text_destroy(&vertices_label);
    rohr_graphics_text_destroy(&add_hitbox_label);
    rohr_graphics_text_destroy(&add_entity_label);
    rohr_graphics_text_destroy(&tools_title);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
