/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "console.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_LOGS 100
#define CONSOLE_TOKEN_LENGTH 3

bool console_debug = false;
bool console_active = false;

typedef struct ConsoleLog {
    LogSourceType source;
    ConsoleLogString log;
} ConsoleLog;

static ConsoleLog logs[MAX_LOGS] = {0};
static int log_index = 0;
static int log_count = 0;

static char console_source_symbol(LogSourceType source) {
    switch(source) {
        case LOG_ENGINE:
            return '#';
        case LOG_APP:
            return '*';
        case LOG_ERROR:
            return '!';
        case LOG_CONSOLE:
            return '>';
        default:
            return '?';
    }
}

static FILE *console_stream(LogSourceType source) {
    if(source == LOG_ERROR) {
        return stderr;
    }
    return stdout;
}

static void console_store_log(LogSourceType source, ConsoleLogString input) {
    input.string[MAX_LOG_STR - 1] = '\0';

    logs[log_index].source = source;
    memcpy(
        logs[log_index].log.string,
        input.string,
        sizeof(logs[log_index].log.string)
    );

    log_index = (log_index + 1) % MAX_LOGS;
    if(log_count < MAX_LOGS) {
        log_count += 1;
    }
}

void console_logs_print(void) {
    int first_log = log_count == MAX_LOGS ? log_index : 0;

    for(int i = 0; i < log_count; i += 1) {
        int index = (first_log + i) % MAX_LOGS;
        fputs(logs[index].log.string, console_stream(logs[index].source));
    }
}

void console_init(void) {
    console_active = true;
}

void console_shutdown(void) {
    console_active = false;
}

bool console_read(ConsoleLogString *input) {
    if(input != NULL) {
        input->string[0] = '\0';
    }
    return false;
}

void console_vwrite(LogSourceType source, const char *fmt, va_list args) {
    ConsoleLogString str_buff = {0};

    str_buff.string[0] = '[';
    str_buff.string[1] = console_source_symbol(source);
    str_buff.string[2] = ']';

    vsnprintf(
        &str_buff.string[CONSOLE_TOKEN_LENGTH],
        sizeof(str_buff.string) - CONSOLE_TOKEN_LENGTH,
        fmt,
        args
    );

    console_store_log(source, str_buff);
    fputs(str_buff.string, console_stream(source));
    fflush(console_stream(source));
}

void console_write(LogSourceType source, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    console_vwrite(source, fmt, args);
    va_end(args);
}

void console_debug_vwrite(LogSourceType source, const char *fmt, va_list args) {
    if(console_debug) {
        console_vwrite(source, fmt, args);
    }
}

void console_debug_write(LogSourceType source, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    console_debug_vwrite(source, fmt, args);
    va_end(args);
}

bool console_active_get(void) {
    return console_active;
}

void console_debug_set(bool state) {
    console_debug = state;
}
