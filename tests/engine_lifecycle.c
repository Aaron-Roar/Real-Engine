#include "rohr.h"

#include <stdio.h>

int main(void) {
    EngineResult result = rohr_engine_init();
    if(rohr_error_check(result)) {
        fprintf(stderr, "%s\n", rohr_error_default_message(result.result.error));
        return 1;
    }

    rohr_engine_shutdown();
    return 0;
}
