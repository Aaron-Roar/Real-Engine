#include <stdio.h>
#include "rohr.h"

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message((engine_result).result.error))

static const char *ui_font_path =
    "examples/user-interface/JetBrainsMono-BoldItalic.ttf";

int main(void) {
    MouseState mouse = {0};
    bool running = true;
    FontAsset font = {0};
    TextAsset title = {0};
    TextAsset play_label = {0};
    TextAsset settings_label = {0};
    TextAsset quit_label = {0};
    TextAsset description = {0};
    UIButtonDefinition settings_button;

    {
        EngineResult init_result = rohr_engine_init();
        if(rohr_error_check(init_result)) {
            PRINT_ENGINE_ERROR(init_result);
            return 1;
        }
    }
    {
        EngineResult graphics_result = rohr_graphics_start();
        if(rohr_error_check(graphics_result)) {
            PRINT_ENGINE_ERROR(graphics_result);
            rohr_engine_shutdown();
            return 1;
        }
    }
    {
        EngineResult state_result = rohr_game_state_load_file(
            "examples/user-interface/user_interface.json"
        );
        if(rohr_error_check(state_result)) {
            PRINT_ENGINE_ERROR(state_result);
            goto fail;
        }
        UIButtonDefinitionResult settings_result =
            rohr_game_state_find_ui_button("settings_button");
        if(rohr_error_check(settings_result)) {
            PRINT_ENGINE_ERROR(settings_result);
            goto fail;
        }
        settings_button = settings_result.result.value;
    }
    {
        FontAssetResult font_result = rohr_graphics_load_font((FontDescriptor){
            .file = ui_font_path,
            .point_size = 24.0f,
        });
        if(rohr_error_check(font_result)) {
            PRINT_ENGINE_ERROR(font_result);
            goto fail;
        }
        font = font_result.result.value;

        TextAssetResult title_result = rohr_graphics_create_text(
            &font,
            "Rohr Engine UI Example",
            (Color){240, 244, 250, 255}
        );
        if(rohr_error_check(title_result)) {
            PRINT_ENGINE_ERROR(title_result);
            goto fail;
        }
        title = title_result.result.value;

        TextAssetResult play_result = rohr_graphics_create_text(
            &font,
            "Play",
            (Color){240, 244, 250, 255}
        );
        if(rohr_error_check(play_result)) {
            PRINT_ENGINE_ERROR(play_result);
            goto fail;
        }
        play_label = play_result.result.value;

        TextAssetResult settings_result = rohr_graphics_create_text(
            &font,
            settings_button.label,
            (Color){240, 244, 250, 255}
        );
        if(rohr_error_check(settings_result)) {
            PRINT_ENGINE_ERROR(settings_result);
            goto fail;
        }
        settings_label = settings_result.result.value;

        TextAssetResult quit_result = rohr_graphics_create_text(
            &font,
            "Quit",
            (Color){240, 244, 250, 255}
        );
        if(rohr_error_check(quit_result)) {
            PRINT_ENGINE_ERROR(quit_result);
            goto fail;
        }
        quit_label = quit_result.result.value;

        TextAssetResult description_result = rohr_graphics_create_text(
            &font,
            "Buttons support hover, press, click, and optional labels.",
            (Color){170, 180, 195, 255}
        );
        if(rohr_error_check(description_result)) {
            PRINT_ENGINE_ERROR(description_result);
            goto fail;
        }
        description = description_result.result.value;
    }

    while(running) {
        SDL_Event event = rohr_engine_poll_event();

        if(event.type == SDL_EVENT_QUIT) {
            break;
        }
        rohr_controller_update_mouse_states(&mouse);
        rohr_controller_add_mouse_event(
            &mouse,
            rohr_controller_capture_mouse_event(&event)
        );

        rohr_graphics_draw_background((Color){18, 22, 30, 255});
        rohr_ui_begin_frame((UIInput){
            .pointer = rohr_graphics_get_mouse_screen_position(),
            .primary_button = mouse.button_states[MOUSE_BUTTON_LEFT],
        });

        rohr_ui_label(&title, (UIRect){0.0f, 30.0f, WINDOW_WIDTH, 50.0f});

        UIRect play_bounds = {220.0f, 130.0f, 200.0f, 55.0f};
        UIButtonResult play = rohr_ui_button("main_menu.play", NULL, play_bounds, NULL);
        UIButtonResult settings = rohr_ui_button(
            settings_button.id,
            &settings_label,
            settings_button.bounds,
            &settings_button.style
        );
        UIButtonResult quit = rohr_ui_button("main_menu.quit", &quit_label, (UIRect){220.0f, 280.0f, 200.0f, 55.0f}, NULL);

        /* A label can also be drawn separately over an unlabeled button. */
        rohr_ui_label(&play_label, play_bounds);
        rohr_ui_label(
            &description,
            (UIRect){0.0f, 405.0f, WINDOW_WIDTH, 45.0f}
        );

        if(play.clicked) printf("Play clicked\n");
        if(settings.clicked) printf("Settings clicked\n");
        if(quit.clicked) running = false;

        rohr_ui_end_frame();
        rohr_graphics_show();
    }

    rohr_graphics_destroy_text(&description);
    rohr_graphics_destroy_text(&quit_label);
    rohr_graphics_destroy_text(&settings_label);
    rohr_graphics_destroy_text(&play_label);
    rohr_graphics_destroy_text(&title);
    rohr_graphics_destroy_font(&font);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_graphics_destroy_text(&description);
    rohr_graphics_destroy_text(&quit_label);
    rohr_graphics_destroy_text(&settings_label);
    rohr_graphics_destroy_text(&play_label);
    rohr_graphics_destroy_text(&title);
    rohr_graphics_destroy_font(&font);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
