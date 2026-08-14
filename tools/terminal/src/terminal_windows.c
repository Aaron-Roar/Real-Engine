#include "terminal_internal.h"

RohrTerminalResult rohr_terminal_platform_start(RohrTerminal *terminal,
        const RohrTerminalConfig *config) {
    (void)terminal;
    (void)config;
    return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_UNSUPPORTED_PLATFORM,
        "Windows ConPTY support is not implemented yet");
}

RohrTerminalResult rohr_terminal_platform_read(RohrTerminal *terminal,
        char *buffer, size_t capacity, size_t *read_count) {
    (void)terminal;
    (void)buffer;
    (void)capacity;
    if(read_count != NULL) *read_count = 0;
    return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_UNSUPPORTED_PLATFORM,
        "Windows ConPTY support is not implemented yet");
}

RohrTerminalResult rohr_terminal_platform_write(RohrTerminal *terminal,
        const char *buffer, size_t length) {
    (void)terminal;
    (void)buffer;
    (void)length;
    return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_UNSUPPORTED_PLATFORM,
        "Windows ConPTY support is not implemented yet");
}

void rohr_terminal_platform_status_update(RohrTerminal *terminal) {
    (void)terminal;
}

void rohr_terminal_platform_destroy(RohrTerminal *terminal) {
    (void)terminal;
}
