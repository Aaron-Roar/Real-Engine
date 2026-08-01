#include <stdio.h>
#include "rohr.h"
#include "example_runtime.h"

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message((engine_result).result.error))

int main(void) {
    if(!example_use_executable_directory()) return 1;
    MouseState mouse = {0};
    bool running = true;
    FontAsset font = {0};
    TextAsset title = {0};
    TextAsset play_label = {0};
    TextAsset settings_label = {0};
    TextAsset quit_label = {0};
    TextAsset description = {0};
    TextAsset slider_label = {0};
    TextAsset slider_value_label = {0};
    TextAsset slider_minus = {0};
    TextAsset slider_plus = {0};
    UIFontDefinition font_definition;
    UILabelDefinition title_definition;
    UILabelDefinition play_definition;
    UILabelDefinition description_definition;
    UIButtonDefinition settings_button;
    UISliderDefinition value_slider;
    float slider_value;

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
            "assets/user-interface/game.json"
        );
        if(rohr_error_check(state_result)) {
            PRINT_ENGINE_ERROR(state_result);
            goto fail;
        }
        UIButtonDefinitionResult settings_result =
            rohr_ui_button_by_name_get("settings_button");
        if(rohr_error_check(settings_result)) {
            PRINT_ENGINE_ERROR(settings_result);
            goto fail;
        }
        settings_button = settings_result.result.value;

        UIFontDefinitionResult font_result =
            rohr_ui_font_by_name_get("menu_font");
        if(rohr_error_check(font_result)) {
            PRINT_ENGINE_ERROR(font_result);
            goto fail;
        }
        font_definition = font_result.result.value;

        UILabelDefinitionResult title_result =
            rohr_ui_label_by_name_get("example_title");
        if(rohr_error_check(title_result)) {
            PRINT_ENGINE_ERROR(title_result);
            goto fail;
        }
        title_definition = title_result.result.value;

        UILabelDefinitionResult play_result =
            rohr_ui_label_by_name_get("play_label");
        if(rohr_error_check(play_result)) {
            PRINT_ENGINE_ERROR(play_result);
            goto fail;
        }
        play_definition = play_result.result.value;

        UILabelDefinitionResult description_result =
            rohr_ui_label_by_name_get("example_description");
        if(rohr_error_check(description_result)) {
            PRINT_ENGINE_ERROR(description_result);
            goto fail;
        }
        description_definition = description_result.result.value;

        UISliderDefinitionResult slider_result =
            rohr_ui_slider_by_name_get("angled_value_slider");
        if(rohr_error_check(slider_result)) {
            PRINT_ENGINE_ERROR(slider_result);
            goto fail;
        }
        value_slider = slider_result.result.value;
        slider_value = value_slider.initial_value;
    }
    {
        FontAssetResult font_result = rohr_graphics_load_font((FontDescriptor){
            .file = font_definition.file,
            .point_size = font_definition.point_size,
        });
        if(rohr_error_check(font_result)) {
            PRINT_ENGINE_ERROR(font_result);
            goto fail;
        }
        font = font_result.result.value;

        TextAssetResult title_result = rohr_graphics_create_text(
            &font,
            title_definition.text,
            title_definition.color
        );
        if(rohr_error_check(title_result)) {
            PRINT_ENGINE_ERROR(title_result);
            goto fail;
        }
        title = title_result.result.value;

        TextAssetResult play_result = rohr_graphics_create_text(
            &font,
            play_definition.text,
            play_definition.color
        );
        if(rohr_error_check(play_result)) {
            PRINT_ENGINE_ERROR(play_result);
            goto fail;
        }
        play_label = play_result.result.value;

        TextAssetResult settings_result = rohr_graphics_create_text(
            &font,
            settings_button.label,
            settings_button.text_color
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
            description_definition.text,
            description_definition.color
        );
        if(rohr_error_check(description_result)) {
            PRINT_ENGINE_ERROR(description_result);
            goto fail;
        }
        description = description_result.result.value;

        TextAssetResult slider_label_result = rohr_graphics_create_text(
            &font, value_slider.label, value_slider.text_color
        );
        if(rohr_error_check(slider_label_result)) {
            PRINT_ENGINE_ERROR(slider_label_result);
            goto fail;
        }
        slider_label = slider_label_result.result.value;
        TextAssetResult minus_result = rohr_graphics_create_text(
            &font, "-", value_slider.text_color
        );
        if(rohr_error_check(minus_result)) {
            PRINT_ENGINE_ERROR(minus_result);
            goto fail;
        }
        slider_minus = minus_result.result.value;
        TextAssetResult plus_result = rohr_graphics_create_text(
            &font, "+", value_slider.text_color
        );
        if(rohr_error_check(plus_result)) {
            PRINT_ENGINE_ERROR(plus_result);
            goto fail;
        }
        slider_plus = plus_result.result.value;
        char value_text[UI_LABEL_MAX];
        snprintf(value_text, sizeof(value_text), value_slider.value_format, slider_value);
        TextAssetResult value_result = rohr_graphics_create_text(
            &font, value_text, value_slider.text_color
        );
        if(rohr_error_check(value_result)) {
            PRINT_ENGINE_ERROR(value_result);
            goto fail;
        }
        slider_value_label = value_result.result.value;
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
            .pointer = rohr_graphics_mouse_screen_position_get(),
            .primary_button = mouse.button_states[MOUSE_BUTTON_LEFT],
        });

        rohr_ui_label(&title, title_definition.bounds);

        UIRect play_bounds = play_definition.bounds;
        UIButtonResult play = rohr_ui_button("main_menu.play", NULL, play_bounds, NULL);
        UIButtonResult settings = rohr_ui_button(
            settings_button.name,
            &settings_label,
            settings_button.bounds,
            &settings_button.style
        );
        UIButtonResult quit = rohr_ui_button("main_menu.quit", &quit_label, (UIRect){220.0f, 280.0f, 200.0f, 55.0f}, NULL);

        /* A label can also be drawn separately over an unlabeled button. */
        rohr_ui_label(&play_label, play_bounds);
        rohr_ui_label(
            &description,
            description_definition.bounds
        );
        UISliderResult slider = rohr_ui_slider_with_text(
            value_slider.name,
            slider_value,
            &value_slider.config,
            &(UISliderText){
                .label = &slider_label,
                .value = &slider_value_label,
                .minus = &slider_minus,
                .plus = &slider_plus,
            }
        );
        slider_value = slider.value;
        if(slider.changed) {
            char value_text[UI_LABEL_MAX];
            snprintf(value_text, sizeof(value_text), value_slider.value_format, slider_value);
            rohr_graphics_destroy_text(&slider_value_label);
            TextAssetResult value_result = rohr_graphics_create_text(
                &font, value_text, value_slider.text_color
            );
            if(rohr_error_check(value_result)) {
                PRINT_ENGINE_ERROR(value_result);
                running = false;
            } else {
                slider_value_label = value_result.result.value;
            }
        }

        if(play.clicked) printf("Play clicked\n");
        if(settings.clicked) printf("Settings clicked\n");
        if(quit.clicked) running = false;

        rohr_ui_end_frame();
        rohr_graphics_show();
    }

    rohr_graphics_destroy_text(&slider_plus);
    rohr_graphics_destroy_text(&slider_minus);
    rohr_graphics_destroy_text(&slider_value_label);
    rohr_graphics_destroy_text(&slider_label);
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
    rohr_graphics_destroy_text(&slider_plus);
    rohr_graphics_destroy_text(&slider_minus);
    rohr_graphics_destroy_text(&slider_value_label);
    rohr_graphics_destroy_text(&slider_label);
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
