/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_FILE_BROWSER_H
#define ROHR_EDITOR_FILE_BROWSER_H

#include "rohr.h"

#define EDITOR_FILE_BROWSER_PATH_MAX 1024
#define EDITOR_FILE_BROWSER_NAME_MAX 256
#define EDITOR_FILE_BROWSER_ENTRY_MAX 256

typedef enum EditorFileBrowserMode {
    EDITOR_FILE_BROWSER_OPEN,
    EDITOR_FILE_BROWSER_OPEN_PNG,
    EDITOR_FILE_BROWSER_OPEN_PNG_MULTI,
    EDITOR_FILE_BROWSER_SAVE,
    EDITOR_FILE_BROWSER_DIRECTORY,
    EDITOR_FILE_BROWSER_CREATE_DIRECTORY
} EditorFileBrowserMode;

typedef struct EditorFileBrowserEntry {
    char name[EDITOR_FILE_BROWSER_NAME_MAX];
    bool directory;
} EditorFileBrowserEntry;

typedef struct EditorFileBrowser {
    bool active;
    EditorFileBrowserMode mode;
    char directory[EDITOR_FILE_BROWSER_PATH_MAX];
    char selected_directory[EDITOR_FILE_BROWSER_PATH_MAX];
    char preview_selected_path[EDITOR_FILE_BROWSER_PATH_MAX];
    bool preview_selected_directory;
    char filename[EDITOR_FILE_BROWSER_NAME_MAX];
    EditorFileBrowserEntry entries[EDITOR_FILE_BROWSER_ENTRY_MAX];
    TextAsset entry_labels[EDITOR_FILE_BROWSER_ENTRY_MAX];
    EditorFileBrowserEntry preview_entries[EDITOR_FILE_BROWSER_ENTRY_MAX];
    TextAsset preview_labels[EDITOR_FILE_BROWSER_ENTRY_MAX];
    TextAsset directory_label;
    TextAsset selected_directory_label;
    TextAsset parent_label;
    FontAsset *font;
    size_t entry_count;
    size_t preview_count;
    float scroll_offset;
    float preview_scroll_offset;
    bool refresh_pending;
    bool entry_selected[EDITOR_FILE_BROWSER_ENTRY_MAX];
    size_t selection_anchor;
    bool selection_anchor_valid;
} EditorFileBrowser;

typedef struct EditorFileBrowserResult {
    bool submitted;
    bool cancelled;
    size_t selected_count;
    char path[EDITOR_FILE_BROWSER_PATH_MAX + EDITOR_FILE_BROWSER_NAME_MAX];
} EditorFileBrowserResult;

void editor_file_browser_init(EditorFileBrowser *browser);
void editor_file_browser_destroy(EditorFileBrowser *browser);
bool editor_file_browser_open(EditorFileBrowser *browser, EditorFileBrowserMode mode,
    const char *directory, FontAsset *font);
bool editor_file_browser_selection_clear(EditorFileBrowser *browser);
bool editor_file_browser_directory_path_get(const EditorFileBrowser *browser,
    char *path, size_t capacity);
bool editor_file_browser_selected_path_get(const EditorFileBrowser *browser,
    size_t selected_index, char *path, size_t capacity);
EditorFileBrowserResult editor_file_browser_draw(EditorFileBrowser *browser,
    TextAsset *field_display, const TextAsset *save_label,
    const TextAsset *open_label, const TextAsset *create_label,
    const TextAsset *cancel_label, float window_width, float window_height);

#endif
