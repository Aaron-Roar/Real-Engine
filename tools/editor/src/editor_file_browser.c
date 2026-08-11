#include "editor_file_browser.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool editor_file_browser_path_join(char *output, size_t capacity,
    const char *directory, const char *name) {
    size_t length;
    if(output == NULL || capacity == 0 || directory == NULL || name == NULL) return false;
    length = strlen(directory);
    return snprintf(output, capacity, "%s%s%s", directory,
        length > 0 && directory[length - 1] != '/' && directory[length - 1] != '\\' ? "/" : "",
        name) < (int)capacity;
}

static bool editor_file_browser_directory_name_valid(const char *name) {
    if(name == NULL || name[0] == '\0' || strcmp(name, ".") == 0 ||
            strcmp(name, "..") == 0) return false;
    return strchr(name, '/') == NULL && strchr(name, '\\') == NULL;
}

static SDL_EnumerationResult SDLCALL editor_file_browser_entry_add(void *userdata,
    const char *dirname, const char *filename) {
    EditorFileBrowser *browser = userdata;
    EditorFileBrowserEntry *entry;
    SDL_PathInfo info;
    char path[EDITOR_FILE_BROWSER_PATH_MAX + EDITOR_FILE_BROWSER_NAME_MAX];
    size_t length;
    (void)dirname;
    if(browser == NULL || filename == NULL ||
            browser->entry_count >= EDITOR_FILE_BROWSER_ENTRY_MAX ||
            !editor_file_browser_path_join(path, sizeof(path), browser->directory, filename) ||
            !SDL_GetPathInfo(path, &info)) return SDL_ENUM_CONTINUE;
    length = strlen(filename);
    if((browser->mode == EDITOR_FILE_BROWSER_DIRECTORY ||
            browser->mode == EDITOR_FILE_BROWSER_CREATE_DIRECTORY) &&
            info.type != SDL_PATHTYPE_DIRECTORY) return SDL_ENUM_CONTINUE;
    if(browser->mode != EDITOR_FILE_BROWSER_DIRECTORY &&
            browser->mode != EDITOR_FILE_BROWSER_CREATE_DIRECTORY &&
            info.type != SDL_PATHTYPE_DIRECTORY && (length < 5 ||
            strcmp(filename + length - 5, ".json") != 0)) return SDL_ENUM_CONTINUE;
    if(length == 0 || length >= EDITOR_FILE_BROWSER_NAME_MAX) return SDL_ENUM_CONTINUE;
    entry = &browser->entries[browser->entry_count++];
    memcpy(entry->name, filename, length + 1);
    entry->directory = info.type == SDL_PATHTYPE_DIRECTORY;
    return SDL_ENUM_CONTINUE;
}

static SDL_EnumerationResult SDLCALL editor_file_browser_preview_entry_add(
    void *userdata, const char *dirname, const char *filename) {
    EditorFileBrowser *browser = userdata;
    EditorFileBrowserEntry *entry;
    SDL_PathInfo info;
    char path[EDITOR_FILE_BROWSER_PATH_MAX + EDITOR_FILE_BROWSER_NAME_MAX];
    size_t length;

    (void)dirname;
    if(browser == NULL || filename == NULL ||
            browser->preview_count >= EDITOR_FILE_BROWSER_ENTRY_MAX ||
            !editor_file_browser_path_join(path, sizeof(path),
                browser->selected_directory, filename) ||
            !SDL_GetPathInfo(path, &info)) return SDL_ENUM_CONTINUE;
    length = strlen(filename);
    if(length == 0 || length >= EDITOR_FILE_BROWSER_NAME_MAX) return SDL_ENUM_CONTINUE;
    entry = &browser->preview_entries[browser->preview_count++];
    memcpy(entry->name, filename, length + 1);
    entry->directory = info.type == SDL_PATHTYPE_DIRECTORY;
    return SDL_ENUM_CONTINUE;
}

static int editor_file_browser_entry_compare(const void *left, const void *right) {
    const EditorFileBrowserEntry *a = left;
    const EditorFileBrowserEntry *b = right;
    if(a->directory != b->directory) return a->directory ? -1 : 1;
    return strcmp(a->name, b->name);
}

static bool editor_file_browser_text_create(FontAsset *font, const char *text,
    TextAsset *asset) {
    TextAssetResult result;
    if(font == NULL || text == NULL || asset == NULL) return false;
    result = rohr_graphics_text_create(font, text, (Color){230, 234, 242, 255});
    if(rohr_error_check(result)) return false;
    *asset = result.result.value;
    return true;
}

static UIButtonStyle editor_file_browser_selected_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();

    style.idle = (Color){118, 96, 35, 255};
    style.hovered = (Color){145, 119, 45, 255};
    return style;
}

static bool editor_file_browser_refresh(EditorFileBrowser *browser) {
    if(browser == NULL) return false;
    for(size_t i = 0; i < browser->entry_count; i += 1) {
        rohr_graphics_text_destroy(&browser->entry_labels[i]);
    }
    browser->entry_count = 0;
    browser->scroll_offset = 0.0f;
    if(!SDL_EnumerateDirectory(browser->directory,
            editor_file_browser_entry_add, browser)) return false;
    qsort(browser->entries, browser->entry_count, sizeof(browser->entries[0]),
        editor_file_browser_entry_compare);
    for(size_t i = 0; i < browser->entry_count; i += 1) {
        if(editor_file_browser_text_create(browser->font, browser->entries[i].name,
                &browser->entry_labels[i])) continue;
        for(size_t j = 0; j < i; j += 1) {
            rohr_graphics_text_destroy(&browser->entry_labels[j]);
        }
        browser->entry_count = 0;
        return false;
    }
    (void)rohr_graphics_text_value_set(&browser->directory_label, browser->directory);
    return true;
}

static void editor_file_browser_preview_clear(EditorFileBrowser *browser) {
    if(browser == NULL) return;
    for(size_t i = 0; i < browser->preview_count; i += 1) {
        rohr_graphics_text_destroy(&browser->preview_labels[i]);
    }
    browser->preview_count = 0;
    browser->preview_scroll_offset = 0.0f;
    browser->selected_directory[0] = '\0';
    browser->preview_selected_path[0] = '\0';
    browser->preview_selected_directory = false;
    (void)rohr_graphics_text_value_set(&browser->selected_directory_label, "");
}

bool editor_file_browser_selection_clear(EditorFileBrowser *browser) {
    if(browser == NULL) return false;
    if(browser->preview_selected_path[0] != '\0') {
        browser->preview_selected_path[0] = '\0';
        browser->preview_selected_directory = false;
        return true;
    }
    if(browser->selected_directory[0] != '\0') {
        editor_file_browser_preview_clear(browser);
        return true;
    }
    return false;
}

static bool editor_file_browser_preview_refresh(EditorFileBrowser *browser,
    const char *directory) {
    size_t length;

    if(browser == NULL || directory == NULL) return false;
    length = strlen(directory);
    if(length == 0 || length >= sizeof(browser->selected_directory)) return false;
    editor_file_browser_preview_clear(browser);
    memcpy(browser->selected_directory, directory, length + 1);
    if(!SDL_EnumerateDirectory(browser->selected_directory,
            editor_file_browser_preview_entry_add, browser)) {
        editor_file_browser_preview_clear(browser);
        return false;
    }
    qsort(browser->preview_entries, browser->preview_count,
        sizeof(browser->preview_entries[0]), editor_file_browser_entry_compare);
    for(size_t i = 0; i < browser->preview_count; i += 1) {
        if(editor_file_browser_text_create(browser->font,
                browser->preview_entries[i].name, &browser->preview_labels[i])) continue;
        editor_file_browser_preview_clear(browser);
        return false;
    }
    (void)rohr_graphics_text_value_set(&browser->selected_directory_label,
        browser->selected_directory);
    return true;
}

static bool editor_file_browser_parent_path_get(const EditorFileBrowser *browser,
    char *path, size_t capacity) {
    size_t length;
    size_t minimum;
    if(browser == NULL || path == NULL || capacity == 0 ||
            strlen(browser->directory) >= capacity) return false;
    snprintf(path, capacity, "%s", browser->directory);
    length = strlen(path);
    minimum = length >= 2 && path[1] == ':' ? 2 : 1;
    while(length > minimum && (path[length - 1] == '/' ||
            path[length - 1] == '\\')) path[--length] = '\0';
    while(length > minimum && path[length - 1] != '/' &&
            path[length - 1] != '\\') path[--length] = '\0';
    while(length > minimum && (path[length - 1] == '/' ||
            path[length - 1] == '\\')) path[--length] = '\0';
    return true;
}

static void editor_file_browser_parent(EditorFileBrowser *browser) {
    char path[EDITOR_FILE_BROWSER_PATH_MAX];

    if(!editor_file_browser_parent_path_get(browser, path, sizeof(path))) return;
    snprintf(browser->directory, sizeof(browser->directory), "%s", path);
    editor_file_browser_preview_clear(browser);
    browser->refresh_pending = true;
}

void editor_file_browser_init(EditorFileBrowser *browser) {
    if(browser != NULL) *browser = (EditorFileBrowser){0};
}

void editor_file_browser_destroy(EditorFileBrowser *browser) {
    if(browser == NULL) return;
    for(size_t i = 0; i < browser->entry_count; i += 1) {
        rohr_graphics_text_destroy(&browser->entry_labels[i]);
    }
    for(size_t i = 0; i < browser->preview_count; i += 1) {
        rohr_graphics_text_destroy(&browser->preview_labels[i]);
    }
    rohr_graphics_text_destroy(&browser->directory_label);
    rohr_graphics_text_destroy(&browser->selected_directory_label);
    rohr_graphics_text_destroy(&browser->parent_label);
    *browser = (EditorFileBrowser){0};
}

bool editor_file_browser_open(EditorFileBrowser *browser, EditorFileBrowserMode mode,
    const char *directory, FontAsset *font) {
    size_t length;
    if(browser == NULL || directory == NULL || font == NULL) return false;
    length = strlen(directory);
    if(length == 0 || length >= sizeof(browser->directory)) return false;
    editor_file_browser_destroy(browser);
    *browser = (EditorFileBrowser){.active = true, .mode = mode};
    browser->font = font;
    if(!editor_file_browser_text_create(font, directory, &browser->directory_label) ||
            !editor_file_browser_text_create(font, "",
                &browser->selected_directory_label) ||
            !editor_file_browser_text_create(font, "..", &browser->parent_label)) {
        editor_file_browser_destroy(browser);
        return false;
    }
    memcpy(browser->directory, directory, length + 1);
    if(mode == EDITOR_FILE_BROWSER_SAVE) {
        snprintf(browser->filename, sizeof(browser->filename), "project.json");
    } else if(mode == EDITOR_FILE_BROWSER_CREATE_DIRECTORY) {
        snprintf(browser->filename, sizeof(browser->filename), "project-dir");
    }
    if(editor_file_browser_refresh(browser)) return true;
    browser->active = false;
    return false;
}

EditorFileBrowserResult editor_file_browser_draw(EditorFileBrowser *browser,
    TextAsset *field_display, const TextAsset *save_label,
    const TextAsset *open_label, const TextAsset *create_label,
    const TextAsset *cancel_label, float window_width, float window_height) {
    EditorFileBrowserResult result = {0};
    UIRect dialog;
    bool directory_mode;
    float left_width;
    float right_x;
    float list_height;
    if(browser == NULL || !browser->active || field_display == NULL) return result;
    if(browser->refresh_pending) {
        browser->refresh_pending = false;
        if(!editor_file_browser_refresh(browser)) {
            browser->active = false;
            return result;
        }
    }
    directory_mode = browser->mode == EDITOR_FILE_BROWSER_DIRECTORY ||
        browser->mode == EDITOR_FILE_BROWSER_CREATE_DIRECTORY;
    dialog = (UIRect){window_width * 0.5f - (directory_mode ? 460.0f : 310.0f),
        window_height * 0.5f - 270.0f, directory_mode ? 920.0f : 620.0f, 540.0f};
    left_width = directory_mode ? (dialog.width - 42.0f) * 0.5f : dialog.width - 28.0f;
    right_x = dialog.x + 28.0f + left_width;
    rohr_ui_surface((UIRect){0.0f, 34.0f, window_width, window_height - 34.0f},
        (Color){12, 14, 18, 255});
    rohr_ui_surface(dialog, (Color){42, 47, 58, 255});
    rohr_ui_border(dialog, 2.0f, (Color){8, 9, 12, 255});
    rohr_ui_label(&browser->directory_label, (UIRect){dialog.x + 14.0f, dialog.y + 12.0f,
        dialog.width - 28.0f, 30.0f});
    list_height = 390.0f;
    browser->scroll_offset = rohr_ui_scroll_region_begin("editor.file_browser.scroll",
        (UIRect){dialog.x + 14.0f, dialog.y + 50.0f, left_width, list_height},
        32.0f * (float)(browser->entry_count + 1), browser->scroll_offset, 38.0f).offset;
    {
        char parent_path[EDITOR_FILE_BROWSER_PATH_MAX];
        UIButtonStyle selected_style = editor_file_browser_selected_style_get();
        bool have_parent = editor_file_browser_parent_path_get(browser,
            parent_path, sizeof(parent_path));
        bool selected = have_parent &&
            strcmp(parent_path, browser->selected_directory) == 0;
        UIButtonResult parent = rohr_ui_button("editor.file_browser.parent",
            &browser->parent_label, (UIRect){dialog.x + 14.0f, dialog.y + 50.0f,
                left_width, 28.0f}, selected ? &selected_style : NULL);
        if(parent.double_clicked) {
            editor_file_browser_parent(browser);
        } else if(parent.clicked && directory_mode && have_parent) {
            (void)editor_file_browser_preview_refresh(browser, parent_path);
        }
    }
    for(size_t i = 0; !browser->refresh_pending && i < browser->entry_count; i += 1) {
        char id[64];
        char entry_path[EDITOR_FILE_BROWSER_PATH_MAX];
        UIButtonResult interaction;
        UIButtonStyle selected_style = editor_file_browser_selected_style_get();
        bool selected = browser->entries[i].directory &&
            editor_file_browser_path_join(entry_path, sizeof(entry_path),
                browser->directory, browser->entries[i].name) &&
            strcmp(entry_path, browser->selected_directory) == 0;
        snprintf(id, sizeof(id), "editor.file_browser.entry.%zu", i);
        interaction = rohr_ui_button(id, &browser->entry_labels[i],
            (UIRect){dialog.x + 14.0f, dialog.y + 82.0f + (float)i * 32.0f,
                left_width, 28.0f}, selected ? &selected_style : NULL);
        if(!interaction.clicked) continue;
        if(browser->entries[i].directory) {
            char path[EDITOR_FILE_BROWSER_PATH_MAX];
            if(editor_file_browser_path_join(path, sizeof(path), browser->directory,
                    browser->entries[i].name)) {
                if(directory_mode && !interaction.double_clicked) {
                    (void)editor_file_browser_preview_refresh(browser, path);
                } else {
                    editor_file_browser_preview_clear(browser);
                    snprintf(browser->directory, sizeof(browser->directory), "%s", path);
                    browser->refresh_pending = true;
                    break;
                }
            }
        } else {
            snprintf(browser->filename, sizeof(browser->filename), "%s",
                browser->entries[i].name);
        }
    }
    rohr_ui_scroll_region_end();
    if(directory_mode) {
        float tree_inset = 24.0f;
        float tree_spine_x = right_x + 10.0f;
        float selected_y = 0.0f;
        bool selected_y_found = false;
        char parent_path[EDITOR_FILE_BROWSER_PATH_MAX];
        Color tree_color = {235, 238, 244, 255};

        if(editor_file_browser_parent_path_get(browser, parent_path,
                sizeof(parent_path)) &&
                strcmp(parent_path, browser->selected_directory) == 0) {
            selected_y = dialog.y + 64.0f - browser->scroll_offset;
            selected_y_found = true;
        }
        for(size_t i = 0; !selected_y_found && i < browser->entry_count; i += 1) {
            char path[EDITOR_FILE_BROWSER_PATH_MAX];
            if(browser->entries[i].directory &&
                    editor_file_browser_path_join(path, sizeof(path), browser->directory,
                        browser->entries[i].name) &&
                    strcmp(path, browser->selected_directory) == 0) {
                selected_y = dialog.y + 96.0f + (float)i * 32.0f -
                    browser->scroll_offset;
                selected_y_found = true;
            }
        }
        if(selected_y_found) {
            bool clipped = rohr_ui_clip_begin((UIRect){dialog.x + 14.0f,
                dialog.y + 50.0f, dialog.width - 28.0f, list_height});
            float left_edge = dialog.x + 14.0f + left_width;
            rohr_ui_surface((UIRect){left_edge, selected_y - 1.0f,
                tree_spine_x - left_edge, 2.0f}, tree_color);
            if(browser->preview_count > 0) {
                float first_y = dialog.y + 96.0f - browser->preview_scroll_offset;
                float last_y = first_y +
                    (float)(browser->preview_count - 1) * 32.0f;
                float vertical_top = fminf(selected_y, first_y);
                float vertical_bottom = fmaxf(selected_y, last_y);
                rohr_ui_surface((UIRect){tree_spine_x - 1.0f, vertical_top,
                    2.0f, vertical_bottom - vertical_top + 1.0f}, tree_color);
                for(size_t i = 0; i < browser->preview_count; i += 1) {
                    float branch_y = dialog.y + 96.0f + (float)i * 32.0f -
                        browser->preview_scroll_offset;
                    rohr_ui_surface((UIRect){tree_spine_x, branch_y - 1.0f,
                        tree_inset - 10.0f, 2.0f}, tree_color);
                }
            }
            if(clipped) rohr_ui_clip_end();
        }
        rohr_ui_label(&browser->selected_directory_label,
            (UIRect){right_x + tree_inset, dialog.y + 50.0f,
                left_width - tree_inset, 28.0f});
        browser->preview_scroll_offset = rohr_ui_scroll_region_begin(
            "editor.file_browser.preview_scroll",
            (UIRect){right_x + tree_inset, dialog.y + 82.0f,
                left_width - tree_inset, list_height - 32.0f},
            32.0f * (float)browser->preview_count,
            browser->preview_scroll_offset, 38.0f).offset;
        for(size_t i = 0; i < browser->preview_count; i += 1) {
            char id[64];
            char path[EDITOR_FILE_BROWSER_PATH_MAX];
            UIButtonResult interaction;
            UIButtonStyle selected_style = editor_file_browser_selected_style_get();
            bool have_path = editor_file_browser_path_join(path, sizeof(path),
                browser->selected_directory, browser->preview_entries[i].name);
            bool selected = have_path &&
                strcmp(path, browser->preview_selected_path) == 0;

            snprintf(id, sizeof(id), "editor.file_browser.preview.%zu", i);
            interaction = rohr_ui_button(id, &browser->preview_labels[i],
                (UIRect){right_x + tree_inset,
                    dialog.y + 82.0f + (float)i * 32.0f,
                    left_width - tree_inset, 28.0f},
                selected ? &selected_style : NULL);
            if(!interaction.clicked || !have_path) continue;
            if(interaction.double_clicked && browser->preview_entries[i].directory) {
                editor_file_browser_preview_clear(browser);
                snprintf(browser->directory, sizeof(browser->directory), "%s", path);
                browser->refresh_pending = true;
                break;
            }
            snprintf(browser->preview_selected_path,
                sizeof(browser->preview_selected_path), "%s", path);
            browser->preview_selected_directory =
                browser->preview_entries[i].directory;
        }
        rohr_ui_scroll_region_end();
    }
    if(browser->mode == EDITOR_FILE_BROWSER_SAVE) {
        (void)rohr_ui_field("editor.file_browser.filename",
            (UIFieldBinding){.kind = UI_FIELD_STRING, .string = browser->filename,
                .string_capacity = sizeof(browser->filename)}, field_display,
            (UIRect){dialog.x + 14.0f, dialog.y + 450.0f,
                dialog.width - 28.0f, 30.0f}, NULL);
    } else if(browser->mode == EDITOR_FILE_BROWSER_CREATE_DIRECTORY) {
        (void)rohr_ui_field("editor.file_browser.directory_name",
            (UIFieldBinding){.kind = UI_FIELD_STRING, .string = browser->filename,
                .string_capacity = sizeof(browser->filename)}, field_display,
            (UIRect){dialog.x + 174.0f, dialog.y + 450.0f,
                dialog.width - 188.0f, 34.0f}, NULL);
    } else if(browser->mode == EDITOR_FILE_BROWSER_OPEN) {
        (void)rohr_graphics_text_value_set(field_display, browser->filename);
        rohr_ui_button_disabled((UIRect){dialog.x + 14.0f, dialog.y + 450.0f,
            dialog.width - 28.0f, 30.0f}, NULL);
        rohr_ui_label(field_display, (UIRect){dialog.x + 14.0f, dialog.y + 450.0f,
            dialog.width - 28.0f, 30.0f});
    }
    if(rohr_ui_button("editor.file_browser.submit",
            browser->mode == EDITOR_FILE_BROWSER_SAVE ? save_label :
                (browser->mode == EDITOR_FILE_BROWSER_CREATE_DIRECTORY ?
                    create_label : open_label),
            browser->mode == EDITOR_FILE_BROWSER_CREATE_DIRECTORY ?
                (UIRect){dialog.x + 14.0f, dialog.y + 450.0f, 150.0f, 34.0f} :
                (UIRect){dialog.x + dialog.width - 274.0f, dialog.y + 490.0f,
                    120.0f, 34.0f}, NULL).clicked &&
            ((browser->mode == EDITOR_FILE_BROWSER_DIRECTORY &&
                ((browser->preview_selected_path[0] != '\0' &&
                    browser->preview_selected_directory) ||
                 (browser->preview_selected_path[0] == '\0' &&
                    browser->selected_directory[0] != '\0')) &&
                snprintf(result.path, sizeof(result.path), "%s",
                    browser->preview_selected_directory ?
                        browser->preview_selected_path :
                        browser->selected_directory) <
                    (int)sizeof(result.path)) ||
             (browser->mode == EDITOR_FILE_BROWSER_CREATE_DIRECTORY &&
                editor_file_browser_directory_name_valid(browser->filename) &&
                editor_file_browser_path_join(result.path, sizeof(result.path),
                    browser->directory, browser->filename)) ||
             (browser->mode != EDITOR_FILE_BROWSER_DIRECTORY &&
                browser->mode != EDITOR_FILE_BROWSER_CREATE_DIRECTORY &&
                browser->filename[0] != '\0' &&
                editor_file_browser_path_join(result.path, sizeof(result.path),
                    browser->directory, browser->filename)))) {
        result.submitted = true;
        browser->active = false;
    }
    if(rohr_ui_button("editor.file_browser.cancel", cancel_label,
            (UIRect){dialog.x + dialog.width - 144.0f, dialog.y + 490.0f,
                120.0f, 34.0f}, NULL).clicked) {
        result.cancelled = true;
        browser->active = false;
    }
    return result;
}
