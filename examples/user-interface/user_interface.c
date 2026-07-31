#include <stdio.h>
#include "rohr.h"

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message((engine_result).result.error))

int main(void) {
    MouseState mouse = {0};
    bool running = true;

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

        UIButtonResult play = rohr_ui_button("main_menu.play", (UIRect){220.0f, 130.0f, 200.0f, 55.0f}, NULL);
        UIButtonResult settings = rohr_ui_button("main_menu.settings", (UIRect){220.0f, 205.0f, 200.0f, 55.0f}, NULL);
        UIButtonResult quit = rohr_ui_button("main_menu.quit", (UIRect){220.0f, 280.0f, 200.0f, 55.0f}, NULL);

        if(play.clicked) printf("Play clicked\n");
        if(settings.clicked) printf("Settings clicked\n");
        if(quit.clicked) running = false;

        rohr_ui_end_frame();
        rohr_graphics_show();
    }

    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;
}
