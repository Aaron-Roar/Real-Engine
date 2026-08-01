#include "rohr_editor.h"

#include <stdio.h>

int main(void) {
    EngineResult engine_result = rohr_engine_init();
    if(rohr_error_check(engine_result)) {
        fprintf(stderr, "%s\n", rohr_error_default_message(engine_result.result.error));
        return 1;
    }

    EngineResult editor_result = RE_init();
    if(rohr_error_check(editor_result)) {
        fprintf(stderr, "%s\n", rohr_error_default_message(editor_result.result.error));
        rohr_engine_shutdown();
        return 1;
    }

    rohr_engine_shutdown();
    return 0;
}
