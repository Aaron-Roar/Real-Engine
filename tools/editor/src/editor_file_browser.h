#ifndef ROHR_EDITOR_FILE_BROWSER_H
#define ROHR_EDITOR_FILE_BROWSER_H

#include "rohr.h"

#define EDITOR_FILE_BROWSER_PATH_MAX 1024
#define EDITOR_FILE_BROWSER_NAME_MAX 256
#define EDITOR_FILE_BROWSER_ENTRY_MAX 256

typedef enum EditorFileBrowserMode {
    EDITOR_FILE_BROWSER_OPEN,
    EDITOR_FILE_BROWSER_SAVE
} EditorFileBrowserMode;

typedef struct EditorFileBrowserEntry {
    char name[EDITOR_FILE_BROWSER_NAME_MAX];
    bool directory;
} EditorFileBrowserEntry;

typedef struct EditorFileBrowser {
    bool active;
    EditorFileBrowserMode mode;
    char directory[EDITOR_FILE_BROWSER_PATH_MAX];
    char filename[EDITOR_FILE_BROWSER_NAME_MAX];
    EditorFileBrowserEntry entries[EDITOR_FILE_BROWSER_ENTRY_MAX];
    TextAsset entry_labels[EDITOR_FILE_BROWSER_ENTRY_MAX];
    TextAsset directory_label;
    TextAsset parent_label;
    FontAsset *font;
    size_t entry_count;
    float scroll_offset;
    bool refresh_pending;
} EditorFileBrowser;

typedef struct EditorFileBrowserResult {
    bool submitted;
    bool cancelled;
    char path[EDITOR_FILE_BROWSER_PATH_MAX + EDITOR_FILE_BROWSER_NAME_MAX];
} EditorFileBrowserResult;

void editor_file_browser_init(EditorFileBrowser *browser);
void editor_file_browser_destroy(EditorFileBrowser *browser);
bool editor_file_browser_open(EditorFileBrowser *browser, EditorFileBrowserMode mode,
    const char *directory, FontAsset *font);
EditorFileBrowserResult editor_file_browser_draw(EditorFileBrowser *browser,
    TextAsset *field_display, const TextAsset *save_label,
    const TextAsset *open_label, const TextAsset *cancel_label, float window_width,
    float window_height);

#endif
