#include "terminal_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

RohrTerminalResult rohr_terminal_result_value(void) {
    return (RohrTerminalResult){.success = true};
}

RohrTerminalResult rohr_terminal_result_error(RohrTerminalError error,
        const char *format, ...) {
    RohrTerminalResult result = {.error = error};
    va_list arguments;
    if(format == NULL) return result;
    va_start(arguments, format);
    (void)vsnprintf(result.message, sizeof(result.message), format, arguments);
    va_end(arguments);
    return result;
}

RohrTerminalConfig rohr_terminal_config_default_get(void) {
    return (RohrTerminalConfig){
        .terminal_type = "xterm-256color",
        .scrollback_lines = ROHR_TERMINAL_DEFAULT_SCROLLBACK_LINES,
        .columns = 120,
        .rows = 30
    };
}

RohrTerminalResult rohr_terminal_create(RohrTerminal **output,
        const RohrTerminalConfig *config) {
    RohrTerminalConfig value = config == NULL ?
        rohr_terminal_config_default_get() : *config;
    RohrTerminalEmulatorConfig emulator_config;
    RohrTerminal *terminal;
    RohrTerminalResult result;
    if(output == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_INVALID_ARGUMENT, "terminal output pointer is null");
    *output = NULL;
    if(value.scrollback_lines == 0) value.scrollback_lines =
        ROHR_TERMINAL_DEFAULT_SCROLLBACK_LINES;
    if(value.columns == 0) value.columns = 120;
    if(value.rows == 0) value.rows = 30;
    terminal = calloc(1, sizeof(*terminal));
    if(terminal == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_ALLOCATION_FAILED, "terminal allocation failed");
    terminal->platform_handle = -1;
    terminal->process_handle = -1;
    terminal->exit_code = -1;
    emulator_config = (RohrTerminalEmulatorConfig){
        .scrollback_lines = value.scrollback_lines,
        .columns = value.columns,
        .rows = value.rows
    };
    result = rohr_terminal_emulator_create(&terminal->emulator, &emulator_config);
    if(!result.success) {
        free(terminal);
        return result;
    }
    result = rohr_terminal_platform_start(terminal, &value);
    if(!result.success) {
        rohr_terminal_destroy(terminal);
        return result;
    }
    *output = terminal;
    return rohr_terminal_result_value();
}

RohrTerminalResult rohr_terminal_update(RohrTerminal *terminal) {
    char buffer[4096];
    if(terminal == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_INVALID_ARGUMENT, "terminal is null");
    for(;;) {
        size_t count = 0;
        RohrTerminalResult result = rohr_terminal_platform_read(
            terminal, buffer, sizeof(buffer), &count);
        if(!result.success) return result;
        if(count == 0) break;
        result = rohr_terminal_emulator_output_write(
            terminal->emulator, buffer, count);
        if(!result.success) return result;
    }
    rohr_terminal_platform_status_update(terminal);
    return rohr_terminal_result_value();
}

RohrTerminalResult rohr_terminal_input_write(RohrTerminal *terminal,
        const char *input, size_t length) {
    if(terminal == NULL || input == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_INVALID_ARGUMENT, "terminal input is null");
    return rohr_terminal_platform_write(terminal, input, length);
}

RohrTerminalResult rohr_terminal_interrupt(RohrTerminal *terminal) {
    const char interrupt = 3;
    return rohr_terminal_input_write(terminal, &interrupt, 1);
}

bool rohr_terminal_running_check(const RohrTerminal *terminal) {
    return terminal != NULL && terminal->running;
}

int rohr_terminal_exit_code_get(const RohrTerminal *terminal) {
    return terminal == NULL ? -1 : terminal->exit_code;
}

size_t rohr_terminal_line_count_get(const RohrTerminal *terminal) {
    return terminal == NULL ? 0 :
        rohr_terminal_emulator_line_count_get(terminal->emulator);
}

RohrTerminalLineView rohr_terminal_line_get(const RohrTerminal *terminal,
        size_t index) {
    return terminal == NULL ? (RohrTerminalLineView){0} :
        rohr_terminal_emulator_line_get(terminal->emulator, index);
}

RohrTerminalCursor rohr_terminal_cursor_get(const RohrTerminal *terminal) {
    return terminal == NULL ? (RohrTerminalCursor){0} :
        rohr_terminal_emulator_cursor_get(terminal->emulator);
}

const char *rohr_terminal_error_message_get(const RohrTerminalResult *result) {
    return result == NULL ? "terminal result is null" : result->message;
}

void rohr_terminal_destroy(RohrTerminal *terminal) {
    if(terminal == NULL) return;
    rohr_terminal_platform_destroy(terminal);
    rohr_terminal_emulator_destroy(terminal->emulator);
    free(terminal);
}
