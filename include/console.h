#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdbool.h>

/** Maximum number of characters stored in one console log string. */
#define MAX_LOG_STR 100

/** Console input or log line storage. */
typedef struct ConsoleLogString {
    /** Null-terminated string buffer. */
    char string[MAX_LOG_STR];
} ConsoleLogString;

/** Global console active state. */
extern bool console_active;

/** Enable debug console writes. */
#define CONSOLE_DEBUG_ON true
/** Disable debug console writes. */
#define CONSOLE_DEBUG_OFF false

/** Global console debug state. */
extern bool console_debug;

/** Source category for console output. */
typedef enum LogSourceType {
    /** Engine subsystem output. */
    LOG_ENGINE,
    /** Application-level output. */
    LOG_APP,
    /** Error output. */
    LOG_ERROR,
    /** Console echo/output. */
    LOG_CONSOLE,
} LogSourceType;

/** Print stored logs. */
void console_print_logs(void);
/** Initialize console state. */
void console_init(void);
/** Shut down console state. */
void console_shutdown(void);
/** Read one console input line.
 *
 * The portable stdout console does not provide live input and always returns
 * false.
 */
bool console_read(ConsoleLogString *input);
/** Write a formatted message to the console. */
void console_write(LogSourceType source, const char *fmt, ...);
/** Check whether the console is active. */
bool console_is_active(void);
/** Write a formatted debug message when debug output is enabled. */
void console_debug_write(LogSourceType source, const char *fmt, ...);
/** Set console debug output state. */
void console_debug_set(bool state);
#endif
