#ifndef ROHR_TERMINAL_H
#define ROHR_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ROHR_TERMINAL_DEFAULT_SCROLLBACK_LINES 5000

typedef struct RohrTerminal RohrTerminal;

typedef enum RohrTerminalError {
    ROHR_TERMINAL_ERROR_NONE = 0,
    ROHR_TERMINAL_ERROR_INVALID_ARGUMENT,
    ROHR_TERMINAL_ERROR_UNSUPPORTED_PLATFORM,
    ROHR_TERMINAL_ERROR_ALLOCATION_FAILED,
    ROHR_TERMINAL_ERROR_PTY_CREATE_FAILED,
    ROHR_TERMINAL_ERROR_PROCESS_CREATE_FAILED,
    ROHR_TERMINAL_ERROR_WORKING_DIRECTORY_FAILED,
    ROHR_TERMINAL_ERROR_READ_FAILED,
    ROHR_TERMINAL_ERROR_WRITE_FAILED
} RohrTerminalError;

typedef struct RohrTerminalResult {
    bool success;
    RohrTerminalError error;
    char message[256];
} RohrTerminalResult;

typedef struct RohrTerminalConfig {
    const char *shell;
    const char *working_directory;
    const char *terminal_type;
    size_t scrollback_lines;
    uint16_t columns;
    uint16_t rows;
} RohrTerminalConfig;

typedef struct RohrTerminalCell {
    uint32_t codepoint;
    uint32_t foreground;
    uint32_t background;
    uint16_t attributes;
} RohrTerminalCell;

typedef struct RohrTerminalLineView {
    const RohrTerminalCell *cells;
    size_t cell_count;
} RohrTerminalLineView;

typedef struct RohrTerminalCursor {
    size_t line_index;
    size_t column;
    bool visible;
} RohrTerminalCursor;

RohrTerminalConfig rohr_terminal_config_default_get(void);
RohrTerminalResult rohr_terminal_create(RohrTerminal **terminal,
    const RohrTerminalConfig *config);
RohrTerminalResult rohr_terminal_update(RohrTerminal *terminal);
RohrTerminalResult rohr_terminal_input_write(RohrTerminal *terminal,
    const char *input, size_t length);
RohrTerminalResult rohr_terminal_interrupt(RohrTerminal *terminal);
bool rohr_terminal_running_check(const RohrTerminal *terminal);
int rohr_terminal_exit_code_get(const RohrTerminal *terminal);
size_t rohr_terminal_line_count_get(const RohrTerminal *terminal);
RohrTerminalLineView rohr_terminal_line_get(const RohrTerminal *terminal,
    size_t index);
RohrTerminalCursor rohr_terminal_cursor_get(const RohrTerminal *terminal);
const char *rohr_terminal_error_message_get(const RohrTerminalResult *result);
void rohr_terminal_destroy(RohrTerminal *terminal);

#endif
