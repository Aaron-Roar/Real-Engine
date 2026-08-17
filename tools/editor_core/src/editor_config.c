#include "editor_config.h"

#include "lauxlib.h"
#include "lua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#define editor_config_access(path) (_access((path), 0) == 0)
#define EDITOR_CONFIG_PATH_SEPARATOR ';'
#else
#include <unistd.h>
#define editor_config_access(path) (access((path), X_OK) == 0)
#define EDITOR_CONFIG_PATH_SEPARATOR ':'
#endif

#ifndef ROHR_DEVELOPMENT_SHARE_DIR
#define ROHR_DEVELOPMENT_SHARE_DIR ""
#endif

static EditorResult editor_config_error(const char *path, const char *message) {
    return editor_result_error(EDITOR_ERROR_SCHEMA_INVALID,
        "Could not load editor Lua config %s: %s", path,
        message == NULL ? "unknown Lua error" : message);
}

static EditorResult editor_config_command_read(lua_State *lua, int table,
        const char *field, EditorConfigCommand *command, const char *path) {
    size_t count;
    lua_getfield(lua, table, field);
    if(lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return editor_result_value(true);
    }
    if(!lua_istable(lua, -1)) {
        lua_pop(lua, 1);
        return editor_config_error(path, "command must be an array of strings");
    }
    count = lua_rawlen(lua, -1);
    if(count == 0 || count > EDITOR_CONFIG_ARGUMENT_MAX) {
        lua_pop(lua, 1);
        return editor_config_error(path, "command argument count is invalid");
    }
    memset(command, 0, sizeof(*command));
    for(size_t i = 0; i < count; i += 1) {
        size_t length;
        const char *value;
        lua_rawgeti(lua, -1, (lua_Integer)i + 1);
        if(lua_type(lua, -1) != LUA_TSTRING) {
            lua_pop(lua, 2);
            return editor_config_error(path,
                "command arguments must be strings");
        }
        value = lua_tolstring(lua, -1, &length);
        if(length >= sizeof(command->arguments[i])) {
            lua_pop(lua, 2);
            return editor_config_error(path,
                "command arguments must be non-oversized strings");
        }
        memcpy(command->arguments[i], value, length + 1);
        lua_pop(lua, 1);
    }
    if(command->arguments[0][0] == '\0') {
        lua_pop(lua, 1);
        return editor_config_error(path,
            "command executable must not be empty");
    }
    command->count = count;
    command->set = true;
    lua_pop(lua, 1);
    return editor_result_value(true);
}

static EditorResult editor_config_command_table_read(lua_State *lua, int root,
        const char *name, EditorConfigCommand *configure,
        EditorConfigCommand *compile, const char *path) {
    EditorResult result;
    lua_getfield(lua, root, name);
    if(lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return editor_result_value(true);
    }
    if(!lua_istable(lua, -1)) {
        lua_pop(lua, 1);
        return editor_config_error(path, "configuration section must be a table");
    }
    result = editor_config_command_read(lua, lua_gettop(lua), "configure",
        configure, path);
    if(!editor_result_check(result)) result = editor_config_command_read(lua,
        lua_gettop(lua), "compile", compile, path);
    lua_pop(lua, 1);
    return result;
}

void editor_config_init(EditorConfig *config) {
    if(config != NULL) memset(config, 0, sizeof(*config));
}

EditorResult editor_config_file_merge(EditorConfig *config, const char *path,
        bool required) {
    lua_State *lua;
    EditorResult result = editor_result_value(true);
    FILE *file;
    if(config == NULL || path == NULL) return editor_result_error(
        EDITOR_ERROR_INVALID_ARGUMENT, "Editor config received an invalid argument");
    file = fopen(path, "rb");
    if(file == NULL) return required ? editor_result_error(EDITOR_ERROR_FILE_IO,
        "Could not open editor Lua config: %s", path) : result;
    fclose(file);
    lua = luaL_newstate();
    if(lua == NULL) return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Could not allocate the Lua configuration state");
    if(luaL_loadfile(lua, path) != LUA_OK || lua_pcall(lua, 0, 1, 0) != LUA_OK) {
        result = editor_config_error(path, lua_tostring(lua, -1));
    } else if(!lua_istable(lua, -1)) {
        result = editor_config_error(path, "config must return a table");
    } else {
        int root = lua_gettop(lua);
        lua_getfield(lua, root, "editor");
        if(lua_istable(lua, -1)) {
            lua_getfield(lua, -1, "font");
            if(lua_isstring(lua, -1)) {
                const char *font = lua_tostring(lua, -1);
                if(strlen(font) >= sizeof(config->font)) result =
                    editor_config_error(path, "editor font path is too long");
                else {
                    snprintf(config->font, sizeof(config->font), "%s", font);
                    config->font_set = true;
                }
            } else if(!lua_isnil(lua, -1)) result = editor_config_error(path,
                "editor.font must be a string or nil");
            lua_pop(lua, 1);
        } else if(!lua_isnil(lua, -1)) result = editor_config_error(path,
            "editor must be a table");
        lua_pop(lua, 1);
        if(!editor_result_check(result)) result = editor_config_command_table_read(
            lua, root, "project", &config->project_configure,
            &config->project_compile, path);
        if(!editor_result_check(result)) result = editor_config_command_table_read(
            lua, root, "cli", &config->cli_configure, &config->cli_compile, path);
        if(!editor_result_check(result)) result = editor_config_command_table_read(
            lua, root, "gui", &config->gui_configure, &config->gui_compile, path);
    }
    lua_close(lua);
    return result;
}

const EditorConfigCommand *editor_config_command_get(const EditorConfig *config,
        EditorConfigFrontend frontend, EditorConfigOperation operation) {
    const EditorConfigCommand *frontend_command;
    const EditorConfigCommand *project_command;
    if(config == NULL) return NULL;
    project_command = operation == EDITOR_CONFIG_OPERATION_CONFIGURE ?
        &config->project_configure : &config->project_compile;
    if(frontend == EDITOR_CONFIG_FRONTEND_CLI)
        frontend_command = operation == EDITOR_CONFIG_OPERATION_CONFIGURE ?
            &config->cli_configure : &config->cli_compile;
    else frontend_command = operation == EDITOR_CONFIG_OPERATION_CONFIGURE ?
        &config->gui_configure : &config->gui_compile;
    return frontend_command->set ? frontend_command :
        (project_command->set ? project_command : NULL);
}

static EditorResult editor_config_argument_expand(char *output, size_t capacity,
        const char *input, const char *project, const char *build, const char *sdk) {
    const struct { const char *key; const char *value; } replacements[] = {
        {"{project}", project}, {"{build}", build}, {"{sdk}", sdk}};
    size_t used = 0;
    while(*input != '\0') {
        bool replaced = false;
        for(size_t i = 0; i < sizeof(replacements) / sizeof(replacements[0]); i += 1) {
            size_t key_length = strlen(replacements[i].key);
            size_t value_length;
            if(strncmp(input, replacements[i].key, key_length) != 0) continue;
            if(replacements[i].value == NULL || replacements[i].value[0] == '\0')
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "Command uses unavailable placeholder: %s", replacements[i].key);
            value_length = strlen(replacements[i].value);
            if(used + value_length >= capacity) return editor_result_error(
                EDITOR_ERROR_CAPACITY, "Expanded editor command argument is too long");
            memcpy(output + used, replacements[i].value, value_length);
            used += value_length;
            input += key_length;
            replaced = true;
            break;
        }
        if(replaced) continue;
        if(used + 1 >= capacity) return editor_result_error(EDITOR_ERROR_CAPACITY,
            "Expanded editor command argument is too long");
        output[used++] = *input++;
    }
    output[used] = '\0';
    return editor_result_value(true);
}

EditorResult editor_config_command_expand(const EditorConfigCommand *command,
        const char *project, const char *build, const char *sdk,
        char arguments[EDITOR_CONFIG_ARGUMENT_MAX][EDITOR_CONFIG_ARGUMENT_LENGTH_MAX],
        const char *output[EDITOR_CONFIG_ARGUMENT_MAX + 1]) {
    EditorResult result;
    if(command == NULL || !command->set || arguments == NULL || output == NULL)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Could not expand an unset editor command");
    for(size_t i = 0; i < command->count; i += 1) {
        result = editor_config_argument_expand(arguments[i], sizeof(arguments[i]),
            command->arguments[i], project, build, sdk);
        if(editor_result_check(result)) return result;
        output[i] = arguments[i];
    }
    output[command->count] = NULL;
    return editor_result_value(true);
}

EditorResult editor_config_sdk_path_get(char *output, size_t capacity,
        const char *name, bool required) {
    const char *base = SDL_GetBasePath();
    char root[EDITOR_WORKSPACE_PATH_MAX * 2];
    size_t length;
    SDL_PathInfo info;
    int count;
    if(output == NULL || capacity == 0 || name == NULL) return editor_result_error(
        EDITOR_ERROR_INVALID_ARGUMENT, "SDK config path received an invalid argument");
    if(base != NULL && strlen(base) < sizeof(root)) {
        snprintf(root, sizeof(root), "%s", base);
        length = strlen(root);
        while(length > 0 && (root[length - 1] == '/' || root[length - 1] == '\\'))
            root[--length] = '\0';
        while(length > 0 && root[length - 1] != '/' && root[length - 1] != '\\')
            length -= 1;
        if(length > 0) root[length - 1] = '\0';
        count = snprintf(output, capacity, "%s/share/rohr/%s", root, name);
        if(count >= 0 && (size_t)count < capacity && SDL_GetPathInfo(output, &info) &&
                info.type == SDL_PATHTYPE_FILE) return editor_result_value(true);
    }
    count = snprintf(output, capacity, "%s/%s", ROHR_DEVELOPMENT_SHARE_DIR, name);
    if(ROHR_DEVELOPMENT_SHARE_DIR[0] != '\0' && count >= 0 &&
            (size_t)count < capacity && SDL_GetPathInfo(output, &info) &&
            info.type == SDL_PATHTYPE_FILE) return editor_result_value(true);
    output[0] = '\0';
    return required ? editor_result_error(EDITOR_ERROR_FILE_IO,
        "Could not locate SDK editor config: %s", name) : editor_result_value(true);
}

EditorResult editor_config_command_expression_parse_detailed(
        const char *expression, EditorConfigCommand *command,
        char *lua_error, size_t lua_error_capacity) {
    lua_State *lua;
    char source[UI_FIELD_EDIT_MAX + 16];
    size_t count;
    if(lua_error != NULL && lua_error_capacity > 0) lua_error[0] = '\0';
    if(expression == NULL || command == NULL || expression[0] == '\0')
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Build override must be a non-empty Lua array");
    if(snprintf(source, sizeof(source), "return %s", expression) >=
            (int)sizeof(source)) return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Build override Lua expression is too long");
    lua = luaL_newstate();
    if(lua == NULL) return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Could not allocate the Lua configuration state");
    if(luaL_loadbuffer(lua, source, strlen(source), "build override") != LUA_OK ||
            lua_pcall(lua, 0, 1, 0) != LUA_OK) {
        const char *message = lua_tostring(lua, -1);
        EditorResult result;
        if(lua_error != NULL && lua_error_capacity > 0)
            snprintf(lua_error, lua_error_capacity, "%s",
                message == NULL ? "Unknown Lua error" : message);
        result = editor_result_error(EDITOR_ERROR_SCHEMA_INVALID,
            "Build override contains invalid Lua syntax or evaluation");
        lua_close(lua);
        return result;
    }
    if(!lua_istable(lua, -1)) {
        lua_close(lua);
        return editor_config_error("build override", "value must be an array");
    }
    count = lua_rawlen(lua, -1);
    if(count == 0 || count > EDITOR_CONFIG_ARGUMENT_MAX) {
        lua_close(lua);
        return editor_config_error("build override",
            "command argument count is invalid");
    }
    memset(command, 0, sizeof(*command));
    for(size_t i = 0; i < count; i += 1) {
        size_t length;
        const char *value;
        lua_rawgeti(lua, -1, (lua_Integer)i + 1);
        if(lua_type(lua, -1) != LUA_TSTRING) {
            lua_close(lua);
            return editor_config_error("build override",
                "command arguments must be strings");
        }
        value = lua_tolstring(lua, -1, &length);
        if(length >= sizeof(command->arguments[i])) {
            lua_close(lua);
            return editor_config_error("build override",
                "command arguments must be non-oversized strings");
        }
        memcpy(command->arguments[i], value, length + 1);
        lua_pop(lua, 1);
    }
    if(command->arguments[0][0] == '\0') {
        lua_close(lua);
        return editor_config_error("build override",
            "command executable must not be empty");
    }
    lua_pushnil(lua);
    size_t key_count = 0;
    while(lua_next(lua, -2) != 0) {
        lua_Integer index;
        bool integer = lua_isinteger(lua, -2);
        index = integer ? lua_tointeger(lua, -2) : 0;
        lua_pop(lua, 1);
        if(!integer || index < 1 || (size_t)index > count) {
            lua_close(lua);
            return editor_config_error("build override",
                "value must contain only sequential array entries");
        }
        key_count += 1;
    }
    if(key_count != count) {
        lua_close(lua);
        return editor_config_error("build override",
            "value must contain only sequential array entries");
    }
    for(size_t i = 0; i < count; i += 1) {
        const char *cursor = command->arguments[i];
        while(*cursor != '\0') {
            if(*cursor == '{') {
                const char *known[] = {"{project}", "{build}", "{sdk}"};
                bool matched = false;
                for(size_t j = 0; j < sizeof(known) / sizeof(known[0]); j += 1) {
                    size_t length = strlen(known[j]);
                    if(strncmp(cursor, known[j], length) == 0) {
                        cursor += length;
                        matched = true;
                        break;
                    }
                }
                if(!matched) {
                    lua_close(lua);
                    return editor_config_error("build override",
                        "unknown command placeholder");
                }
                continue;
            }
            if(*cursor == '}') {
                lua_close(lua);
                return editor_config_error("build override",
                    "unknown command placeholder");
            }
            cursor += 1;
        }
    }
    command->count = count;
    command->set = true;
    lua_close(lua);
    return editor_result_value(true);
}

EditorResult editor_config_command_expression_parse(const char *expression,
        EditorConfigCommand *command) {
    return editor_config_command_expression_parse_detailed(expression, command,
        NULL, 0);
}

EditorResult editor_config_command_expression_write(const EditorConfigCommand *command,
        char *output, size_t capacity) {
    size_t used = 0;
    if(command == NULL || !command->set || output == NULL || capacity < 4)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Could not format an unset build command");
#define EDITOR_CONFIG_WRITE(character) do { \
    if(used + 1 >= capacity) return editor_result_error(EDITOR_ERROR_CAPACITY, \
        "Formatted build command is too long"); \
    output[used++] = (character); \
} while(0)
    EDITOR_CONFIG_WRITE('{');
    for(size_t i = 0; i < command->count; i += 1) {
        if(i > 0) {
            EDITOR_CONFIG_WRITE(',');
            EDITOR_CONFIG_WRITE(' ');
        }
        EDITOR_CONFIG_WRITE('"');
        for(size_t j = 0; command->arguments[i][j] != '\0'; j += 1) {
            char character = command->arguments[i][j];
            if(character == '\\' || character == '"') EDITOR_CONFIG_WRITE('\\');
            if(character == '\n') {
                EDITOR_CONFIG_WRITE('\\');
                character = 'n';
            }
            EDITOR_CONFIG_WRITE(character);
        }
        EDITOR_CONFIG_WRITE('"');
    }
    EDITOR_CONFIG_WRITE('}');
    output[used] = '\0';
#undef EDITOR_CONFIG_WRITE
    return editor_result_value(true);
}

static bool editor_config_executable_file_check(const char *path) {
    SDL_PathInfo info;
    return path != NULL && editor_config_access(path) &&
        SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

static bool editor_config_executable_candidate_check(const char *path) {
    if(editor_config_executable_file_check(path)) return true;
#if defined(_WIN32)
    const char *extensions = getenv("PATHEXT");
    char candidate[EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
    const char *last_separator = strrchr(path, '/');
    const char *windows_separator = strrchr(path, '\\');
    const char *name = last_separator == NULL ? path : last_separator + 1;
    if(windows_separator != NULL && windows_separator + 1 > name)
        name = windows_separator + 1;
    if(strrchr(name, '.') != NULL) return false;
    if(extensions == NULL || extensions[0] == '\0')
        extensions = ".COM;.EXE;.BAT;.CMD";
    while(true) {
        const char *end = strchr(extensions, ';');
        size_t length = end == NULL ? strlen(extensions) :
            (size_t)(end - extensions);
        int count = snprintf(candidate, sizeof(candidate), "%s%.*s", path,
            (int)length, extensions);
        if(count >= 0 && (size_t)count < sizeof(candidate) &&
                editor_config_executable_file_check(candidate)) return true;
        if(end == NULL) break;
        extensions = end + 1;
    }
#endif
    return false;
}

EditorResult editor_config_command_executable_check(
        const EditorConfigCommand *command, const char *project_directory) {
    const char *executable;
    const char *environment;
    bool explicit_path;
    if(command == NULL || !command->set || command->count == 0 ||
            command->arguments[0][0] == '\0') return editor_result_error(
        EDITOR_ERROR_INVALID_ARGUMENT, "Build command has no executable");
    executable = command->arguments[0];
    explicit_path = strchr(executable, '/') != NULL ||
        strchr(executable, '\\') != NULL;
    if(explicit_path) {
        char path[EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
        bool absolute = executable[0] == '/' || executable[0] == '\\' ||
            (strlen(executable) > 2 && executable[1] == ':');
        int count = absolute ? snprintf(path, sizeof(path), "%s", executable) :
            snprintf(path, sizeof(path), "%s/%s",
                project_directory == NULL ? "." : project_directory, executable);
        if(count >= 0 && (size_t)count < sizeof(path) &&
                editor_config_executable_candidate_check(path))
            return editor_result_value(true);
        return editor_result_error(EDITOR_ERROR_NOT_FOUND,
            "Build executable is unavailable: %s", executable);
    }
    environment = getenv("PATH");
    if(environment != NULL) {
        const char *start = environment;
        while(true) {
            const char *end = strchr(start, EDITOR_CONFIG_PATH_SEPARATOR);
            size_t length = end == NULL ? strlen(start) : (size_t)(end - start);
            char candidate[EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
            int count;
            if(length == 0) count = snprintf(candidate, sizeof(candidate),
                "%s", executable);
            else count = snprintf(candidate, sizeof(candidate), "%.*s/%s",
                (int)length, start, executable);
            if(count >= 0 && (size_t)count < sizeof(candidate) &&
                    editor_config_executable_candidate_check(candidate))
                return editor_result_value(true);
            if(end == NULL) break;
            start = end + 1;
        }
    }
    return editor_result_error(EDITOR_ERROR_NOT_FOUND,
        "Build executable was not found in PATH: %s", executable);
}

static bool editor_config_lua_command_write(FILE *file, const char *name,
        const EditorConfigCommand *command) {
    char expression[UI_FIELD_EDIT_MAX];
    if(command == NULL || !command->set)
        return fprintf(file, "        %s = nil,\n", name) > 0;
    if(editor_result_check(editor_config_command_expression_write(command,
            expression, sizeof(expression)))) return false;
    return fprintf(file, "        %s = %s,\n", name, expression) > 0;
}

EditorResult editor_config_gui_override_save(const char *project_directory,
        const EditorConfigCommand *configure, const EditorConfigCommand *compile) {
    char directory[EDITOR_WORKSPACE_PATH_MAX * 2];
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char temporary[EDITOR_WORKSPACE_PATH_MAX * 2];
    FILE *file;
    int count;
    if(project_directory == NULL || project_directory[0] == '\0')
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Cannot save GUI overrides without an open project");
    count = snprintf(directory, sizeof(directory), "%s/.rohr", project_directory);
    if(count < 0 || (size_t)count >= sizeof(directory) ||
            !SDL_CreateDirectory(directory)) return editor_result_error(
        EDITOR_ERROR_FILE_IO, "Could not create project config directory: %s",
        directory);
    if(snprintf(path, sizeof(path), "%s/gui-overrides.lua", directory) >=
            (int)sizeof(path) || snprintf(temporary, sizeof(temporary),
                "%s/gui-overrides.lua.tmp", directory) >= (int)sizeof(temporary))
        return editor_result_error(EDITOR_ERROR_CAPACITY,
            "Project GUI override path is too long");
    file = fopen(temporary, "wb");
    if(file == NULL) return editor_result_error(EDITOR_ERROR_FILE_IO,
        "Could not write GUI overrides: %s", temporary);
    bool written = fputs("-- Generated by Rohr Editor GUI.\nreturn {\n    gui = {\n",
        file) >= 0 && editor_config_lua_command_write(file, "configure", configure) &&
        editor_config_lua_command_write(file, "compile", compile) &&
        fputs("    },\n}\n", file) >= 0;
    bool closed = fclose(file) == 0;
    if(!written || !closed || !SDL_RenamePath(temporary, path)) {
        (void)SDL_RemovePath(temporary);
        return editor_result_error(EDITOR_ERROR_FILE_IO,
            "Could not atomically save GUI overrides: %s", path);
    }
    return editor_result_value(true);
}
