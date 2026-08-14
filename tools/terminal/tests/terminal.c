#define _POSIX_C_SOURCE 200809L
#include "rohr_terminal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static bool terminal_line_contains(const RohrTerminal *terminal, const char *text) {
    size_t text_length = strlen(text);
    for(size_t line_index = 0; line_index < rohr_terminal_line_count_get(terminal);
            line_index += 1) {
        RohrTerminalLineView line = rohr_terminal_line_get(terminal, line_index);
        if(line.cell_count < text_length) continue;
        for(size_t start = 0; start + text_length <= line.cell_count; start += 1) {
            bool equal = true;
            for(size_t i = 0; i < text_length; i += 1) {
                if(line.cells[start + i].codepoint != (unsigned char)text[i]) equal = false;
            }
            if(equal) return true;
        }
    }
    return false;
}

int main(void) {
    RohrTerminal *terminal = NULL;
    RohrTerminalConfig config = rohr_terminal_config_default_get();
    RohrTerminalResult result;
    const char command[] =
        "printf 'removed-by-clear\\n\\033[?2004h\\033[H\\033[2Jterminal-test\\n'; "
        "pwd; printf '\\033[?25l\\033[4G'; exit 7\n";
    struct timespec delay = {.tv_nsec = 10000000};

    config.shell = "/bin/sh";
    config.working_directory = "/tmp";
    config.scrollback_lines = 16;
    result = rohr_terminal_create(&terminal, &config);
    if(!result.success || terminal == NULL) return 1;
    result = rohr_terminal_input_write(terminal, command, sizeof(command) - 1);
    if(!result.success) {
        rohr_terminal_destroy(terminal);
        return 1;
    }
    for(size_t i = 0; i < 200 && rohr_terminal_running_check(terminal); i += 1) {
        result = rohr_terminal_update(terminal);
        if(!result.success) {
            rohr_terminal_destroy(terminal);
            return 1;
        }
        (void)nanosleep(&delay, NULL);
    }
    (void)rohr_terminal_update(terminal);
    RohrTerminalCursor cursor = rohr_terminal_cursor_get(terminal);
    if(rohr_terminal_running_check(terminal) || rohr_terminal_exit_code_get(terminal) != 7 ||
            !terminal_line_contains(terminal, "terminal-test") ||
            !terminal_line_contains(terminal, "/tmp") ||
            terminal_line_contains(terminal, "removed-by-clear") ||
            terminal_line_contains(terminal, "2004h") || cursor.visible) {
        fprintf(stderr, "running=%d exit=%d lines=%zu terminal_test=%d cwd=%d\n",
            rohr_terminal_running_check(terminal), rohr_terminal_exit_code_get(terminal),
            rohr_terminal_line_count_get(terminal),
            terminal_line_contains(terminal, "terminal-test"),
            terminal_line_contains(terminal, "/tmp"));
        for(size_t line_index = 0; line_index < rohr_terminal_line_count_get(terminal);
                line_index += 1) {
            RohrTerminalLineView line = rohr_terminal_line_get(terminal, line_index);
            for(size_t i = 0; i < line.cell_count; i += 1)
                if(line.cells[i].codepoint < 128) fputc((int)line.cells[i].codepoint, stderr);
            fputc('\n', stderr);
        }
        rohr_terminal_destroy(terminal);
        return 1;
    }
    rohr_terminal_destroy(terminal);
    return 0;
}
