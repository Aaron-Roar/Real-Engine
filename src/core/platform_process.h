#ifndef PLATFORM_PROCESS_H
#define PLATFORM_PROCESS_H

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <stdbool.h>
#include <stdio.h>

bool platform_process_command_check(const char *command);
FILE *platform_process_open_write(const char *command);
int platform_process_close(FILE *process);

#endif
