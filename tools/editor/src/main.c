#include "rohr.h"

#include <stdio.h>

#if defined(_WIN32)
#include <direct.h>
#define editor_chdir _chdir
#else
#include <unistd.h>
#define editor_chdir chdir
#endif

#define EDITOR_VIEWPORT_WIDTH (WINDOW_WIDTH * 0.8f)
#define EDITOR_TOOLS_WIDTH (WINDOW_WIDTH - EDITOR_VIEWPORT_WIDTH)

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
    TextAsset select_label = {0};
    TextAsset create_label = {0};
    TextAsset delete_label = {0};
    ViewportId viewport = 0;
    float first_slider = 0.35f;
    float second_slider = 0.70f;
    bool running = true;

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
            !editor_text_create(&font, "Select", &select_label) ||
            !editor_text_create(&font, "Create", &create_label) ||
            !editor_text_create(&font, "Delete", &delete_label)) goto fail;

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
        (void)rohr_ui_button("editor.select", &select_label,
            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 58.0f,
                EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
        (void)rohr_ui_button("editor.create", &create_label,
            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 106.0f,
                EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
        (void)rohr_ui_button("editor.delete", &delete_label,
            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 154.0f,
                EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
        {
            UISliderConfig slider = rohr_ui_slider_config_default_get();
            UISliderResult result;

            slider.center = (Position){EDITOR_VIEWPORT_WIDTH +
                EDITOR_TOOLS_WIDTH * 0.5f, 245.0f};
            slider.length = EDITOR_TOOLS_WIDTH - 36.0f;
            result = rohr_ui_slider("editor.slider.first", first_slider, &slider);
            first_slider = result.value;
            slider.center.y = 315.0f;
            result = rohr_ui_slider("editor.slider.second", second_slider, &slider);
            second_slider = result.value;
        }
        rohr_ui_frame_end();
        rohr_graphics_show();
    }

    rohr_graphics_text_destroy(&delete_label);
    rohr_graphics_text_destroy(&create_label);
    rohr_graphics_text_destroy(&select_label);
    rohr_graphics_text_destroy(&tools_title);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_graphics_text_destroy(&delete_label);
    rohr_graphics_text_destroy(&create_label);
    rohr_graphics_text_destroy(&select_label);
    rohr_graphics_text_destroy(&tools_title);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
