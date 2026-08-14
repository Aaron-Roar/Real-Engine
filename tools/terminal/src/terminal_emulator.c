#include "terminal_internal.h"

#include <stdlib.h>
#include <string.h>

#define ROHR_TERMINAL_COLOR_DEFAULT_FOREGROUND UINT32_C(0xe6ebf4ff)
#define ROHR_TERMINAL_COLOR_DEFAULT_BACKGROUND UINT32_C(0x000000ff)

static const uint32_t terminal_colors[8] = {
    UINT32_C(0x000000ff), UINT32_C(0xcd3131ff), UINT32_C(0x0dbc79ff),
    UINT32_C(0xe5e510ff), UINT32_C(0x2472c8ff), UINT32_C(0xbc3fbcff),
    UINT32_C(0x11a8cdff), UINT32_C(0xe5e5e5ff)
};

static RohrTerminalLine *emulator_line_get(RohrTerminalEmulator *emulator,
        size_t line) {
    return &emulator->lines[(emulator->line_first + line) % emulator->line_capacity];
}

static RohrTerminalLine *emulator_current_line_get(RohrTerminalEmulator *emulator) {
    return emulator_line_get(emulator, emulator->cursor_line);
}

static bool emulator_line_start(RohrTerminalEmulator *emulator) {
    size_t index;
    if(emulator->line_count == emulator->line_capacity) {
        index = emulator->line_first;
        emulator->line_first = (emulator->line_first + 1) % emulator->line_capacity;
    } else {
        index = (emulator->line_first + emulator->line_count) % emulator->line_capacity;
        emulator->line_count += 1;
    }
    emulator->lines[index].count = 0;
    emulator->cursor_line = emulator->line_count - 1;
    emulator->cursor_column = 0;
    return true;
}

static bool emulator_line_capacity_set(RohrTerminalLine *line, size_t required) {
    size_t capacity;
    RohrTerminalCell *cells;
    if(required <= line->capacity) return true;
    capacity = line->capacity == 0 ? 64 : line->capacity;
    while(capacity < required) capacity *= 2;
    cells = realloc(line->cells, capacity * sizeof(*cells));
    if(cells == NULL) return false;
    line->cells = cells;
    line->capacity = capacity;
    return true;
}

static RohrTerminalCell emulator_blank_cell_get(
        const RohrTerminalEmulator *emulator) {
    return (RohrTerminalCell){
        .codepoint = ' ',
        .foreground = emulator->foreground,
        .background = emulator->background,
        .attributes = emulator->attributes
    };
}

static bool emulator_cell_write(RohrTerminalEmulator *emulator,
        uint32_t codepoint) {
    RohrTerminalLine *line = emulator_current_line_get(emulator);
    size_t required = emulator->cursor_column + 1;
    if(!emulator_line_capacity_set(line, required)) return false;
    while(line->count < emulator->cursor_column)
        line->cells[line->count++] = emulator_blank_cell_get(emulator);
    line->cells[emulator->cursor_column] = (RohrTerminalCell){
        .codepoint = codepoint,
        .foreground = emulator->foreground,
        .background = emulator->background,
        .attributes = emulator->attributes
    };
    emulator->cursor_column += 1;
    if(emulator->cursor_column > line->count) line->count = emulator->cursor_column;
    if(emulator->columns > 0 && emulator->cursor_column >= emulator->columns)
        return emulator_line_start(emulator);
    return true;
}

static bool emulator_screen_clear(RohrTerminalEmulator *emulator) {
    for(size_t i = 0; i < emulator->line_capacity; i += 1)
        emulator->lines[i].count = 0;
    emulator->line_first = 0;
    emulator->line_count = 0;
    return emulator_line_start(emulator);
}

static long emulator_parameter_get(const RohrTerminalEmulator *emulator,
        size_t requested, long fallback) {
    const char *cursor = emulator->escape_parameters;
    size_t index = 0;
    while(*cursor == '?' || *cursor == '>' || *cursor == '!') cursor += 1;
    while(index < requested && *cursor != '\0') {
        cursor = strchr(cursor, ';');
        if(cursor == NULL) return fallback;
        cursor += 1;
        index += 1;
    }
    if(*cursor < '0' || *cursor > '9') return fallback;
    return strtol(cursor, NULL, 10);
}

static void emulator_sgr_apply(RohrTerminalEmulator *emulator) {
    char parameters[sizeof(emulator->escape_parameters)];
    char *cursor;
    memcpy(parameters, emulator->escape_parameters, sizeof(parameters));
    cursor = parameters;
    if(cursor[0] == '\0') cursor = "0";
    while(cursor != NULL) {
        char *next = strchr(cursor, ';');
        long value;
        if(next != NULL) *next++ = '\0';
        value = strtol(cursor, NULL, 10);
        if(value == 0) {
            emulator->foreground = ROHR_TERMINAL_COLOR_DEFAULT_FOREGROUND;
            emulator->background = ROHR_TERMINAL_COLOR_DEFAULT_BACKGROUND;
            emulator->attributes = 0;
        } else if(value == 1) emulator->attributes |= UINT16_C(1);
        else if(value == 4) emulator->attributes |= UINT16_C(2);
        else if(value == 7) emulator->attributes |= UINT16_C(4);
        else if(value >= 30 && value <= 37)
            emulator->foreground = terminal_colors[value - 30];
        else if(value >= 40 && value <= 47)
            emulator->background = terminal_colors[value - 40];
        else if(value == 39)
            emulator->foreground = ROHR_TERMINAL_COLOR_DEFAULT_FOREGROUND;
        else if(value == 49)
            emulator->background = ROHR_TERMINAL_COLOR_DEFAULT_BACKGROUND;
        cursor = next;
    }
}

static void emulator_cursor_move(RohrTerminalEmulator *emulator, uint8_t command) {
    size_t amount = (size_t)emulator_parameter_get(emulator, 0, 1);
    if(amount == 0) amount = 1;
    if(command == 'A') emulator->cursor_line = amount > emulator->cursor_line ?
        0 : emulator->cursor_line - amount;
    else if(command == 'B') {
        emulator->cursor_line += amount;
        if(emulator->cursor_line >= emulator->line_count)
            emulator->cursor_line = emulator->line_count - 1;
    } else if(command == 'C') emulator->cursor_column += amount;
    else if(command == 'D') emulator->cursor_column = amount > emulator->cursor_column ?
        0 : emulator->cursor_column - amount;
    else if(command == 'G') emulator->cursor_column = amount - 1;
    else if(command == 'H' || command == 'f') {
        size_t visible_first = emulator->line_count > emulator->rows ?
            emulator->line_count - emulator->rows : 0;
        size_t row = (size_t)emulator_parameter_get(emulator, 0, 1);
        size_t column = (size_t)emulator_parameter_get(emulator, 1, 1);
        emulator->cursor_line = visible_first + (row > 0 ? row - 1 : 0);
        if(emulator->cursor_line >= emulator->line_count)
            emulator->cursor_line = emulator->line_count - 1;
        emulator->cursor_column = column > 0 ? column - 1 : 0;
    }
}

static bool emulator_erase_line(RohrTerminalEmulator *emulator, long mode) {
    RohrTerminalLine *line = emulator_current_line_get(emulator);
    if(mode == 0) {
        if(emulator->cursor_column < line->count) line->count = emulator->cursor_column;
        return true;
    }
    if(mode == 2) {
        line->count = 0;
        return true;
    }
    if(mode == 1) {
        size_t end = emulator->cursor_column < line->count ?
            emulator->cursor_column : line->count;
        for(size_t i = 0; i < end; i += 1) line->cells[i] = emulator_blank_cell_get(emulator);
    }
    return true;
}

static bool emulator_characters_edit(RohrTerminalEmulator *emulator,
        uint8_t command) {
    RohrTerminalLine *line = emulator_current_line_get(emulator);
    size_t amount = (size_t)emulator_parameter_get(emulator, 0, 1);
    if(amount == 0) amount = 1;
    if(emulator->cursor_column >= line->count && command != '@') return true;
    if(command == 'P') {
        size_t available = line->count - emulator->cursor_column;
        if(amount > available) amount = available;
        memmove(&line->cells[emulator->cursor_column],
            &line->cells[emulator->cursor_column + amount],
            (available - amount) * sizeof(*line->cells));
        line->count -= amount;
    } else if(command == 'X') {
        size_t end = emulator->cursor_column + amount;
        if(end > line->count) end = line->count;
        for(size_t i = emulator->cursor_column; i < end; i += 1)
            line->cells[i] = emulator_blank_cell_get(emulator);
    } else if(command == '@') {
        size_t at = emulator->cursor_column > line->count ? line->count :
            emulator->cursor_column;
        if(!emulator_line_capacity_set(line, line->count + amount)) return false;
        memmove(&line->cells[at + amount], &line->cells[at],
            (line->count - at) * sizeof(*line->cells));
        for(size_t i = 0; i < amount; i += 1)
            line->cells[at + i] = emulator_blank_cell_get(emulator);
        line->count += amount;
    }
    return true;
}

static bool emulator_csi_apply(RohrTerminalEmulator *emulator, uint8_t command) {
    long mode = emulator_parameter_get(emulator, 0, 0);
    if(command == 'm') emulator_sgr_apply(emulator);
    else if(command == 'J' && mode >= 2) return emulator_screen_clear(emulator);
    else if(command == 'K') return emulator_erase_line(emulator, mode);
    else if(command == 'P' || command == 'X' || command == '@')
        return emulator_characters_edit(emulator, command);
    else if(command == 'A' || command == 'B' || command == 'C' ||
            command == 'D' || command == 'G' || command == 'H' || command == 'f')
        emulator_cursor_move(emulator, command);
    else if((command == 'h' || command == 'l') &&
            emulator->escape_parameters[0] == '?' && mode == 25)
        emulator->cursor_visible = command == 'h';
    return true;
}

static bool emulator_codepoint_add(RohrTerminalEmulator *emulator,
        uint32_t codepoint) {
    if(codepoint == '\n') return emulator_line_start(emulator);
    if(codepoint == '\r') {
        emulator->cursor_column = 0;
        return true;
    }
    if(codepoint == '\b') {
        if(emulator->cursor_column > 0) emulator->cursor_column -= 1;
        return true;
    }
    if(codepoint == '\t') {
        size_t next = (emulator->cursor_column + 8) & ~(size_t)7;
        while(emulator->cursor_column < next)
            if(!emulator_cell_write(emulator, ' ')) return false;
        return true;
    }
    if(codepoint < 32) return true;
    return emulator_cell_write(emulator, codepoint);
}

static bool emulator_byte_add(RohrTerminalEmulator *emulator, uint8_t byte) {
    if(emulator->escape_state == ROHR_TERMINAL_ESCAPE_STARTED) {
        if(byte == '[') emulator->escape_state = ROHR_TERMINAL_ESCAPE_CSI;
        else if(byte == ']') emulator->escape_state = ROHR_TERMINAL_ESCAPE_OSC;
        else emulator->escape_state = ROHR_TERMINAL_ESCAPE_NONE;
        return true;
    }
    if(emulator->escape_state == ROHR_TERMINAL_ESCAPE_OSC) {
        if(byte == 0x07) emulator->escape_state = ROHR_TERMINAL_ESCAPE_NONE;
        else if(byte == 0x1b) emulator->escape_state = ROHR_TERMINAL_ESCAPE_OSC_END;
        return true;
    }
    if(emulator->escape_state == ROHR_TERMINAL_ESCAPE_OSC_END) {
        emulator->escape_state = byte == '\\' ? ROHR_TERMINAL_ESCAPE_NONE :
            ROHR_TERMINAL_ESCAPE_OSC;
        return true;
    }
    if(emulator->escape_state == ROHR_TERMINAL_ESCAPE_CSI) {
        if(byte >= 0x20 && byte <= 0x3f) {
            if(emulator->escape_parameter_count + 1 < sizeof(emulator->escape_parameters)) {
                emulator->escape_parameters[emulator->escape_parameter_count++] = (char)byte;
                emulator->escape_parameters[emulator->escape_parameter_count] = '\0';
            }
            return true;
        }
        if(!emulator_csi_apply(emulator, byte)) return false;
        emulator->escape_state = ROHR_TERMINAL_ESCAPE_NONE;
        emulator->escape_parameter_count = 0;
        emulator->escape_parameters[0] = '\0';
        return true;
    }
    if(byte == 0x1b) {
        emulator->escape_state = ROHR_TERMINAL_ESCAPE_STARTED;
        return true;
    }
    if(emulator->utf8_remaining == 0) {
        if(byte < 0x80) return emulator_codepoint_add(emulator, byte);
        if((byte & 0xe0) == 0xc0) {
            emulator->utf8_codepoint = byte & 0x1f;
            emulator->utf8_remaining = 1;
        } else if((byte & 0xf0) == 0xe0) {
            emulator->utf8_codepoint = byte & 0x0f;
            emulator->utf8_remaining = 2;
        } else if((byte & 0xf8) == 0xf0) {
            emulator->utf8_codepoint = byte & 0x07;
            emulator->utf8_remaining = 3;
        } else return emulator_codepoint_add(emulator, UINT32_C(0xfffd));
        return true;
    }
    if((byte & 0xc0) != 0x80) {
        emulator->utf8_remaining = 0;
        return emulator_codepoint_add(emulator, UINT32_C(0xfffd));
    }
    emulator->utf8_codepoint = (emulator->utf8_codepoint << 6) | (byte & 0x3f);
    emulator->utf8_remaining -= 1;
    return emulator->utf8_remaining != 0 ||
        emulator_codepoint_add(emulator, emulator->utf8_codepoint);
}

RohrTerminalEmulatorConfig rohr_terminal_emulator_config_default_get(void) {
    return (RohrTerminalEmulatorConfig){
        .scrollback_lines = ROHR_TERMINAL_DEFAULT_SCROLLBACK_LINES,
        .columns = 120,
        .rows = 30
    };
}

RohrTerminalResult rohr_terminal_emulator_create(RohrTerminalEmulator **output,
        const RohrTerminalEmulatorConfig *config) {
    RohrTerminalEmulatorConfig value = config == NULL ?
        rohr_terminal_emulator_config_default_get() : *config;
    RohrTerminalEmulator *emulator;
    if(output == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_INVALID_ARGUMENT, "terminal emulator output is null");
    *output = NULL;
    if(value.scrollback_lines == 0) value.scrollback_lines =
        ROHR_TERMINAL_DEFAULT_SCROLLBACK_LINES;
    if(value.columns == 0) value.columns = 120;
    if(value.rows == 0) value.rows = 30;
    emulator = calloc(1, sizeof(*emulator));
    if(emulator == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_ALLOCATION_FAILED, "terminal emulator allocation failed");
    emulator->lines = calloc(value.scrollback_lines, sizeof(*emulator->lines));
    if(emulator->lines == NULL) {
        free(emulator);
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_ALLOCATION_FAILED,
            "terminal emulator scrollback allocation failed");
    }
    emulator->line_capacity = value.scrollback_lines;
    emulator->columns = value.columns;
    emulator->rows = value.rows;
    emulator->foreground = ROHR_TERMINAL_COLOR_DEFAULT_FOREGROUND;
    emulator->background = ROHR_TERMINAL_COLOR_DEFAULT_BACKGROUND;
    emulator->cursor_visible = true;
    (void)emulator_line_start(emulator);
    *output = emulator;
    return rohr_terminal_result_value();
}

RohrTerminalResult rohr_terminal_emulator_output_write(
        RohrTerminalEmulator *emulator, const char *output, size_t length) {
    if(emulator == NULL || output == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_INVALID_ARGUMENT, "terminal emulator output is null");
    for(size_t i = 0; i < length; i += 1) {
        if(!emulator_byte_add(emulator, (uint8_t)output[i]))
            return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_ALLOCATION_FAILED,
                "terminal emulator cell allocation failed");
    }
    return rohr_terminal_result_value();
}

size_t rohr_terminal_emulator_line_count_get(const RohrTerminalEmulator *emulator) {
    return emulator == NULL ? 0 : emulator->line_count;
}

RohrTerminalLineView rohr_terminal_emulator_line_get(
        const RohrTerminalEmulator *emulator, size_t index) {
    const RohrTerminalLine *line;
    if(emulator == NULL || index >= emulator->line_count)
        return (RohrTerminalLineView){0};
    line = &emulator->lines[(emulator->line_first + index) % emulator->line_capacity];
    return (RohrTerminalLineView){.cells = line->cells, .cell_count = line->count};
}

RohrTerminalCursor rohr_terminal_emulator_cursor_get(
        const RohrTerminalEmulator *emulator) {
    if(emulator == NULL) return (RohrTerminalCursor){0};
    return (RohrTerminalCursor){
        .line_index = emulator->cursor_line,
        .column = emulator->cursor_column,
        .visible = emulator->cursor_visible
    };
}

void rohr_terminal_emulator_destroy(RohrTerminalEmulator *emulator) {
    if(emulator == NULL) return;
    for(size_t i = 0; i < emulator->line_capacity; i += 1)
        free(emulator->lines[i].cells);
    free(emulator->lines);
    free(emulator);
}
