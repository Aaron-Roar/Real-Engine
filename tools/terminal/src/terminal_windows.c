#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0a00
#endif
#include <windows.h>

#include "terminal_internal.h"

#include <stdlib.h>

typedef struct RohrTerminalWindowsState {
    HPCON pseudo_console;
    HANDLE input;
    HANDLE output;
    HANDLE process;
    HANDLE thread;
} RohrTerminalWindowsState;

static RohrTerminalResult rohr_terminal_windows_error(
        RohrTerminalError error, const char *operation) {
    return rohr_terminal_result_error(error, "%s failed with Windows error %lu",
        operation, (unsigned long)GetLastError());
}

static wchar_t *rohr_terminal_windows_utf8_get(const char *text) {
    int count;
    wchar_t *wide;
    if(text == NULL || text[0] == '\0') return NULL;
    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if(count <= 0) return NULL;
    wide = malloc((size_t)count * sizeof(*wide));
    if(wide == NULL) return NULL;
    if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text, -1, wide, count) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static void rohr_terminal_windows_state_destroy(RohrTerminalWindowsState *state,
        bool terminate) {
    if(state == NULL) return;
    if(terminate && state->process != NULL &&
            WaitForSingleObject(state->process, 0) == WAIT_TIMEOUT)
        (void)TerminateProcess(state->process, 1);
    if(state->thread != NULL) CloseHandle(state->thread);
    if(state->process != NULL) CloseHandle(state->process);
    if(state->pseudo_console != NULL) ClosePseudoConsole(state->pseudo_console);
    if(state->input != NULL) CloseHandle(state->input);
    if(state->output != NULL) CloseHandle(state->output);
    free(state);
}

RohrTerminalResult rohr_terminal_platform_start(RohrTerminal *terminal,
        const RohrTerminalConfig *config) {
    RohrTerminalWindowsState *state = NULL;
    HANDLE input_read = NULL;
    HANDLE output_write = NULL;
    SIZE_T attribute_size = 0;
    STARTUPINFOEXW startup = {0};
    PROCESS_INFORMATION process = {0};
    wchar_t *shell = NULL;
    wchar_t *directory = NULL;
    const char *shell_text;
    HRESULT status;
    bool created = false;

    if(terminal == NULL || config == NULL)
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_INVALID_ARGUMENT,
            "terminal start received an invalid argument");
    shell_text = config->shell;
    if(shell_text == NULL || shell_text[0] == '\0') shell_text = getenv("COMSPEC");
    if(shell_text == NULL || shell_text[0] == '\0') shell_text = "cmd.exe";
    shell = rohr_terminal_windows_utf8_get(shell_text);
    if(shell == NULL) return rohr_terminal_result_error(
        ROHR_TERMINAL_ERROR_PROCESS_CREATE_FAILED,
        "terminal shell path is invalid or could not be allocated");
    if(config->working_directory != NULL && config->working_directory[0] != '\0') {
        directory = rohr_terminal_windows_utf8_get(config->working_directory);
        if(directory == NULL) {
            free(shell);
            return rohr_terminal_result_error(
                ROHR_TERMINAL_ERROR_WORKING_DIRECTORY_FAILED,
                "terminal working directory is invalid or could not be allocated");
        }
    }
    state = calloc(1, sizeof(*state));
    if(state == NULL) {
        free(directory);
        free(shell);
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_ALLOCATION_FAILED,
            "terminal Windows state allocation failed");
    }
    if(!CreatePipe(&input_read, &state->input, NULL, 0) ||
            !CreatePipe(&state->output, &output_write, NULL, 0)) goto done;
    status = CreatePseudoConsole((COORD){(SHORT)config->columns, (SHORT)config->rows},
        input_read, output_write, 0, &state->pseudo_console);
    if(FAILED(status)) {
        SetLastError((DWORD)status);
        goto done;
    }
    CloseHandle(input_read);
    input_read = NULL;
    CloseHandle(output_write);
    output_write = NULL;
    startup.StartupInfo.cb = sizeof(startup);
    (void)InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_size);
    startup.lpAttributeList = malloc(attribute_size);
    if(startup.lpAttributeList == NULL || !InitializeProcThreadAttributeList(
            startup.lpAttributeList, 1, 0, &attribute_size) ||
            !UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, state->pseudo_console,
                sizeof(state->pseudo_console), NULL, NULL)) goto done;
    if(!CreateProcessW(NULL, shell, NULL, NULL, false,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            NULL, directory, &startup.StartupInfo, &process)) goto done;
    state->process = process.hProcess;
    state->thread = process.hThread;
    terminal->platform_handle = (intptr_t)state;
    terminal->process_handle = (intptr_t)state->process;
    terminal->running = true;
    created = true;
done:
    if(startup.lpAttributeList != NULL) {
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        free(startup.lpAttributeList);
    }
    if(input_read != NULL) CloseHandle(input_read);
    if(output_write != NULL) CloseHandle(output_write);
    free(directory);
    free(shell);
    if(!created) {
        RohrTerminalResult result = rohr_terminal_windows_error(
            ROHR_TERMINAL_ERROR_PROCESS_CREATE_FAILED, "ConPTY creation");
        rohr_terminal_windows_state_destroy(state, true);
        return result;
    }
    return rohr_terminal_result_value();
}

RohrTerminalResult rohr_terminal_platform_read(RohrTerminal *terminal,
        char *buffer, size_t capacity, size_t *read_count) {
    RohrTerminalWindowsState *state;
    DWORD available = 0;
    DWORD count = 0;
    if(terminal == NULL || buffer == NULL || read_count == NULL)
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_INVALID_ARGUMENT,
            "terminal read received an invalid argument");
    *read_count = 0;
    state = (RohrTerminalWindowsState *)terminal->platform_handle;
    if(state == NULL || state->output == NULL)
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_READ_FAILED,
            "terminal output pipe is unavailable");
    if(!PeekNamedPipe(state->output, NULL, 0, NULL, &available, NULL)) {
        if(GetLastError() == ERROR_BROKEN_PIPE) return rohr_terminal_result_value();
        return rohr_terminal_windows_error(ROHR_TERMINAL_ERROR_READ_FAILED,
            "ConPTY output query");
    }
    if(available == 0) return rohr_terminal_result_value();
    if(available > capacity) available = (DWORD)capacity;
    if(!ReadFile(state->output, buffer, available, &count, NULL)) {
        if(GetLastError() == ERROR_BROKEN_PIPE) return rohr_terminal_result_value();
        return rohr_terminal_windows_error(ROHR_TERMINAL_ERROR_READ_FAILED,
            "ConPTY output read");
    }
    *read_count = (size_t)count;
    return rohr_terminal_result_value();
}

RohrTerminalResult rohr_terminal_platform_write(RohrTerminal *terminal,
        const char *buffer, size_t length) {
    RohrTerminalWindowsState *state;
    size_t written = 0;
    if(terminal == NULL || buffer == NULL)
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_INVALID_ARGUMENT,
            "terminal write received an invalid argument");
    state = (RohrTerminalWindowsState *)terminal->platform_handle;
    if(state == NULL || state->input == NULL)
        return rohr_terminal_result_error(ROHR_TERMINAL_ERROR_WRITE_FAILED,
            "terminal input pipe is unavailable");
    while(written < length) {
        DWORD chunk = length - written > MAXDWORD ? MAXDWORD : (DWORD)(length - written);
        DWORD count = 0;
        if(!WriteFile(state->input, buffer + written, chunk, &count, NULL))
            return rohr_terminal_windows_error(ROHR_TERMINAL_ERROR_WRITE_FAILED,
                "ConPTY input write");
        written += (size_t)count;
    }
    return rohr_terminal_result_value();
}

void rohr_terminal_platform_status_update(RohrTerminal *terminal) {
    RohrTerminalWindowsState *state;
    DWORD exit_code;
    if(terminal == NULL || !terminal->running) return;
    state = (RohrTerminalWindowsState *)terminal->platform_handle;
    if(state == NULL || WaitForSingleObject(state->process, 0) != WAIT_OBJECT_0) return;
    terminal->running = false;
    if(GetExitCodeProcess(state->process, &exit_code)) terminal->exit_code = (int)exit_code;
}

void rohr_terminal_platform_destroy(RohrTerminal *terminal) {
    RohrTerminalWindowsState *state;
    if(terminal == NULL) return;
    state = (RohrTerminalWindowsState *)terminal->platform_handle;
    rohr_terminal_windows_state_destroy(state, terminal->running);
    terminal->platform_handle = -1;
    terminal->process_handle = -1;
    terminal->running = false;
}
