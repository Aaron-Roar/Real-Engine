#define _GNU_SOURCE
#include "terminal_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

RohrTerminalResult rohr_terminal_platform_start(RohrTerminal *terminal,
        const RohrTerminalConfig *config) {
    struct winsize size = {
        .ws_row = config->rows,
        .ws_col = config->columns
    };
    const char *shell = config->shell;
    pid_t child;
    int master;

    if(shell == NULL || shell[0] == '\0') shell = getenv("SHELL");
    if(shell == NULL || shell[0] == '\0') shell = "/bin/sh";
    if(config->working_directory != NULL && config->working_directory[0] != '\0' &&
            access(config->working_directory, X_OK) != 0)
        return rohr_terminal_result_error(
            ROHR_TERMINAL_ERROR_WORKING_DIRECTORY_FAILED,
            "terminal working directory is not accessible: %s",
            config->working_directory);
    child = forkpty(&master, NULL, NULL, &size);
    if(child < 0) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_PTY_CREATE_FAILED, "PTY creation failed: %s", strerror(errno));
    if(child == 0) {
        const char *name = strrchr(shell, '/');
        if(config->working_directory != NULL && config->working_directory[0] != '\0' &&
                chdir(config->working_directory) != 0) _exit(126);
        if(config->terminal_type != NULL && config->terminal_type[0] != '\0')
            (void)setenv("TERM", config->terminal_type, 1);
        name = name == NULL ? shell : name + 1;
        execl(shell, name, "-i", (char *)NULL);
        _exit(127);
    }
    if(fcntl(master, F_SETFL, fcntl(master, F_GETFL) | O_NONBLOCK) < 0) {
        (void)kill(child, SIGTERM);
        (void)waitpid(child, NULL, 0);
        (void)close(master);
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_PTY_CREATE_FAILED,
            "could not make PTY non-blocking: %s", strerror(errno));
    }
    terminal->platform_handle = master;
    terminal->process_handle = child;
    terminal->running = true;
    return rohr_terminal_result_value();
}

RohrTerminalResult rohr_terminal_platform_read(RohrTerminal *terminal,
        char *buffer, size_t capacity, size_t *read_count) {
    ssize_t count;
    if(terminal == NULL || buffer == NULL || read_count == NULL)
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_INVALID_ARGUMENT,
            "terminal read received an invalid argument");
    *read_count = 0;
    count = read((int)terminal->platform_handle, buffer, capacity);
    if(count > 0) {
        *read_count = (size_t)count;
        return rohr_terminal_result_value();
    }
    if(count == 0 || errno == EAGAIN || errno == EWOULDBLOCK || errno == EIO)
        return rohr_terminal_result_value();
    return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_READ_FAILED,
        "PTY read failed: %s", strerror(errno));
}

RohrTerminalResult rohr_terminal_platform_write(RohrTerminal *terminal,
        const char *buffer, size_t length) {
    size_t written = 0;
    if(terminal == NULL || buffer == NULL)
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_INVALID_ARGUMENT,
            "terminal write received an invalid argument");
    while(written < length) {
        ssize_t count = write((int)terminal->platform_handle,
            buffer + written, length - written);
        if(count > 0) written += (size_t)count;
        else if(count < 0 && errno == EINTR) continue;
        else return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_WRITE_FAILED,
            "PTY write failed: %s", strerror(errno));
    }
    return rohr_terminal_result_value();
}

void rohr_terminal_platform_status_update(RohrTerminal *terminal) {
    int status;
    pid_t result;
    if(terminal == NULL || !terminal->running) return;
    result = waitpid((pid_t)terminal->process_handle, &status, WNOHANG);
    if(result <= 0) return;
    terminal->running = false;
    if(WIFEXITED(status)) terminal->exit_code = WEXITSTATUS(status);
    else if(WIFSIGNALED(status)) terminal->exit_code = 128 + WTERMSIG(status);
}

void rohr_terminal_platform_destroy(RohrTerminal *terminal) {
    if(terminal == NULL) return;
    if(terminal->running) {
        (void)kill((pid_t)terminal->process_handle, SIGHUP);
        (void)kill((pid_t)terminal->process_handle, SIGTERM);
        (void)waitpid((pid_t)terminal->process_handle, NULL, 0);
        terminal->running = false;
    }
    if(terminal->platform_handle >= 0) {
        (void)close((int)terminal->platform_handle);
        terminal->platform_handle = -1;
    }
}
