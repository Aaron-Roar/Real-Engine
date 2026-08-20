/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_TERMINAL_INTERNAL_H
#define ROHR_TERMINAL_INTERNAL_H

#include "rohr_terminal.h"

typedef struct RohrTerminalLine {
    RohrTerminalCell *cells;
    size_t count;
    size_t capacity;
} RohrTerminalLine;

typedef enum RohrTerminalEscapeState {
    ROHR_TERMINAL_ESCAPE_NONE,
    ROHR_TERMINAL_ESCAPE_STARTED,
    ROHR_TERMINAL_ESCAPE_CSI,
    ROHR_TERMINAL_ESCAPE_OSC,
    ROHR_TERMINAL_ESCAPE_OSC_END
} RohrTerminalEscapeState;

struct RohrTerminal {
    intptr_t platform_handle;
    intptr_t process_handle;
    bool running;
    int exit_code;
    RohrTerminalEmulator *emulator;
};

struct RohrTerminalEmulator {
    RohrTerminalLine *lines;
    size_t line_capacity;
    size_t line_count;
    size_t line_first;
    size_t cursor_line;
    size_t cursor_column;
    uint16_t rows;
    uint16_t columns;
    bool cursor_visible;
    uint32_t foreground;
    uint32_t background;
    uint16_t attributes;
    RohrTerminalEscapeState escape_state;
    char escape_parameters[64];
    size_t escape_parameter_count;
    uint32_t utf8_codepoint;
    uint8_t utf8_remaining;
};

RohrTerminalResult rohr_terminal_result_value(void);
RohrTerminalResult rohr_terminal_result_error(RohrTerminalError error,
    const char *format, ...);
RohrTerminalResult rohr_terminal_platform_start(RohrTerminal *terminal,
    const RohrTerminalConfig *config);
RohrTerminalResult rohr_terminal_platform_read(RohrTerminal *terminal,
    char *buffer, size_t capacity, size_t *read_count);
RohrTerminalResult rohr_terminal_platform_write(RohrTerminal *terminal,
    const char *buffer, size_t length);
void rohr_terminal_platform_status_update(RohrTerminal *terminal);
void rohr_terminal_platform_destroy(RohrTerminal *terminal);

#endif
