#include "terminal_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROHR_TERMINAL_COLOR_DEFAULT_FOREGROUND UINT32_C(0xe6ebf4ff)
#define ROHR_TERMINAL_COLOR_DEFAULT_BACKGROUND UINT32_C(0x000000ff)

static const uint32_t terminal_colors[8] = {
    UINT32_C(0x000000ff), UINT32_C(0xcd3131ff), UINT32_C(0x0dbc79ff),
    UINT32_C(0xe5e510ff), UINT32_C(0x2472c8ff), UINT32_C(0xbc3fbcff),
    UINT32_C(0x11a8cdff), UINT32_C(0xe5e5e5ff)
};

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

static RohrTerminalLine *rohr_terminal_current_line_get(RohrTerminal *terminal) {
    size_t index = (terminal->line_first + terminal->line_count - 1) %
        terminal->line_capacity;
    return &terminal->lines[index];
}

static bool rohr_terminal_line_start(RohrTerminal *terminal) {
    size_t index;
    RohrTerminalLine *line;
    if(terminal->line_count == terminal->line_capacity) {
        index = terminal->line_first;
        terminal->line_first = (terminal->line_first + 1) % terminal->line_capacity;
    } else {
        index = (terminal->line_first + terminal->line_count) % terminal->line_capacity;
        terminal->line_count += 1;
    }
    line = &terminal->lines[index];
    line->count = 0;
    return true;
}

static bool rohr_terminal_cell_add(RohrTerminal *terminal, uint32_t codepoint) {
    RohrTerminalLine *line = rohr_terminal_current_line_get(terminal);
    if(line->count == line->capacity) {
        size_t capacity = line->capacity == 0 ? 64 : line->capacity * 2;
        RohrTerminalCell *cells = realloc(line->cells, capacity * sizeof(*cells));
        if(cells == NULL) return false;
        line->cells = cells;
        line->capacity = capacity;
    }
    line->cells[line->count++] = (RohrTerminalCell){
        .codepoint = codepoint,
        .foreground = terminal->foreground,
        .background = terminal->background,
        .attributes = terminal->attributes
    };
    return true;
}

static void rohr_terminal_sgr_apply(RohrTerminal *terminal) {
    char parameters[sizeof(terminal->escape_parameters)];
    char *cursor;
    snprintf(parameters, sizeof(parameters), "%s", terminal->escape_parameters);
    cursor = parameters;
    if(cursor[0] == '\0') cursor = "0";
    while(cursor != NULL) {
        char *next = strchr(cursor, ';');
        long value;
        if(next != NULL) *next++ = '\0';
        value = strtol(cursor, NULL, 10);
        if(value == 0) {
            terminal->foreground = ROHR_TERMINAL_COLOR_DEFAULT_FOREGROUND;
            terminal->background = ROHR_TERMINAL_COLOR_DEFAULT_BACKGROUND;
            terminal->attributes = 0;
        } else if(value == 1) terminal->attributes |= UINT16_C(1);
        else if(value == 4) terminal->attributes |= UINT16_C(2);
        else if(value == 7) terminal->attributes |= UINT16_C(4);
        else if(value >= 30 && value <= 37)
            terminal->foreground = terminal_colors[value - 30];
        else if(value >= 40 && value <= 47)
            terminal->background = terminal_colors[value - 40];
        else if(value == 39)
            terminal->foreground = ROHR_TERMINAL_COLOR_DEFAULT_FOREGROUND;
        else if(value == 49)
            terminal->background = ROHR_TERMINAL_COLOR_DEFAULT_BACKGROUND;
        cursor = next;
    }
}

static bool rohr_terminal_codepoint_add(RohrTerminal *terminal, uint32_t codepoint) {
    if(codepoint == '\n') return rohr_terminal_line_start(terminal);
    if(codepoint == '\r') return true;
    if(codepoint == '\b') {
        RohrTerminalLine *line = rohr_terminal_current_line_get(terminal);
        if(line->count > 0) line->count -= 1;
        return true;
    }
    if(codepoint < 32 && codepoint != '\t') return true;
    return rohr_terminal_cell_add(terminal, codepoint);
}

static bool rohr_terminal_screen_clear(RohrTerminal *terminal) {
    for(size_t i = 0; i < terminal->line_capacity; i += 1)
        terminal->lines[i].count = 0;
    terminal->line_first = 0;
    terminal->line_count = 0;
    return rohr_terminal_line_start(terminal);
}

static long rohr_terminal_csi_first_parameter_get(const RohrTerminal *terminal) {
    const char *parameter = terminal->escape_parameters;
    while(*parameter != '\0' && (*parameter < '0' || *parameter > '9')) parameter += 1;
    return *parameter == '\0' ? 0 : strtol(parameter, NULL, 10);
}

static bool rohr_terminal_byte_add(RohrTerminal *terminal, uint8_t byte) {
    if(terminal->escape_state == ROHR_TERMINAL_ESCAPE_STARTED) {
        if(byte == '[') terminal->escape_state = ROHR_TERMINAL_ESCAPE_CSI;
        else if(byte == ']') terminal->escape_state = ROHR_TERMINAL_ESCAPE_OSC;
        else terminal->escape_state = ROHR_TERMINAL_ESCAPE_NONE;
        return true;
    }
    if(terminal->escape_state == ROHR_TERMINAL_ESCAPE_OSC) {
        if(byte == 0x07) terminal->escape_state = ROHR_TERMINAL_ESCAPE_NONE;
        else if(byte == 0x1b) terminal->escape_state = ROHR_TERMINAL_ESCAPE_OSC_END;
        return true;
    }
    if(terminal->escape_state == ROHR_TERMINAL_ESCAPE_OSC_END) {
        terminal->escape_state = byte == '\\' ? ROHR_TERMINAL_ESCAPE_NONE :
            ROHR_TERMINAL_ESCAPE_OSC;
        return true;
    }
    if(terminal->escape_state == ROHR_TERMINAL_ESCAPE_CSI) {
        if(byte >= 0x20 && byte <= 0x3f) {
            if(terminal->escape_parameter_count + 1 <
                    sizeof(terminal->escape_parameters)) {
                terminal->escape_parameters[terminal->escape_parameter_count++] = (char)byte;
                terminal->escape_parameters[terminal->escape_parameter_count] = '\0';
            }
            return true;
        }
        if(byte == 'm') rohr_terminal_sgr_apply(terminal);
        else if(byte == 'J' &&
                rohr_terminal_csi_first_parameter_get(terminal) >= 2) {
            if(!rohr_terminal_screen_clear(terminal)) return false;
        }
        terminal->escape_state = ROHR_TERMINAL_ESCAPE_NONE;
        terminal->escape_parameter_count = 0;
        terminal->escape_parameters[0] = '\0';
        return true;
    }
    if(byte == 0x1b) {
        terminal->escape_state = ROHR_TERMINAL_ESCAPE_STARTED;
        return true;
    }
    if(terminal->utf8_remaining == 0) {
        if(byte < 0x80) return rohr_terminal_codepoint_add(terminal, byte);
        if((byte & 0xe0) == 0xc0) {
            terminal->utf8_codepoint = byte & 0x1f;
            terminal->utf8_remaining = 1;
        } else if((byte & 0xf0) == 0xe0) {
            terminal->utf8_codepoint = byte & 0x0f;
            terminal->utf8_remaining = 2;
        } else if((byte & 0xf8) == 0xf0) {
            terminal->utf8_codepoint = byte & 0x07;
            terminal->utf8_remaining = 3;
        } else return rohr_terminal_codepoint_add(terminal, UINT32_C(0xfffd));
        return true;
    }
    if((byte & 0xc0) != 0x80) {
        terminal->utf8_remaining = 0;
        return rohr_terminal_codepoint_add(terminal, UINT32_C(0xfffd));
    }
    terminal->utf8_codepoint = (terminal->utf8_codepoint << 6) | (byte & 0x3f);
    terminal->utf8_remaining -= 1;
    return terminal->utf8_remaining != 0 ||
        rohr_terminal_codepoint_add(terminal, terminal->utf8_codepoint);
}

RohrTerminalResult rohr_terminal_create(RohrTerminal **output,
        const RohrTerminalConfig *config) {
    RohrTerminalConfig defaults;
    RohrTerminal *terminal;
    RohrTerminalResult result;
    if(output == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_INVALID_ARGUMENT, "terminal output pointer is null");
    *output = NULL;
    defaults = config == NULL ? rohr_terminal_config_default_get() : *config;
    if(defaults.scrollback_lines == 0) defaults.scrollback_lines =
        ROHR_TERMINAL_DEFAULT_SCROLLBACK_LINES;
    if(defaults.columns == 0) defaults.columns = 120;
    if(defaults.rows == 0) defaults.rows = 30;
    terminal = calloc(1, sizeof(*terminal));
    if(terminal == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_ALLOCATION_FAILED, "terminal allocation failed");
    terminal->lines = calloc(defaults.scrollback_lines, sizeof(*terminal->lines));
    if(terminal->lines == NULL) {
        free(terminal);
        return rohr_terminal_result_error(
            ROHR_TERMINAL_ERROR_ALLOCATION_FAILED, "terminal scrollback allocation failed");
    }
    terminal->platform_handle = -1;
    terminal->process_handle = -1;
    terminal->line_capacity = defaults.scrollback_lines;
    terminal->foreground = ROHR_TERMINAL_COLOR_DEFAULT_FOREGROUND;
    terminal->background = ROHR_TERMINAL_COLOR_DEFAULT_BACKGROUND;
    terminal->exit_code = -1;
    (void)rohr_terminal_line_start(terminal);
    result = rohr_terminal_platform_start(terminal, &defaults);
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
        for(size_t i = 0; i < count; i += 1) {
            if(!rohr_terminal_byte_add(terminal, (uint8_t)buffer[i]))
                return rohr_terminal_result_error(
                    ROHR_TERMINAL_ERROR_ALLOCATION_FAILED,
                    "terminal output line allocation failed");
        }
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
    return terminal == NULL ? 0 : terminal->line_count;
}

RohrTerminalLineView rohr_terminal_line_get(const RohrTerminal *terminal,
        size_t index) {
    const RohrTerminalLine *line;
    if(terminal == NULL || index >= terminal->line_count) return (RohrTerminalLineView){0};
    line = &terminal->lines[(terminal->line_first + index) % terminal->line_capacity];
    return (RohrTerminalLineView){.cells = line->cells, .cell_count = line->count};
}

const char *rohr_terminal_error_message_get(const RohrTerminalResult *result) {
    return result == NULL ? "terminal result is null" : result->message;
}

void rohr_terminal_destroy(RohrTerminal *terminal) {
    if(terminal == NULL) return;
    rohr_terminal_platform_destroy(terminal);
    for(size_t i = 0; i < terminal->line_capacity; i += 1)
        free(terminal->lines[i].cells);
    free(terminal->lines);
    free(terminal);
}
