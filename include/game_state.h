#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stddef.h>
#include "error.h"
#include "ui.h"

/** Current JSON game-state schema version. */
#define GAME_STATE_VERSION 1

/** Maximum source documents retained for compact template saving. */
#define GAME_STATE_MAX_TEMPLATE_DOCUMENTS 64

/**
 * Load and merge entities from one or more JSON state files.
 *
 * All names from every file are registered before named relationships are
 * resolved, so references may cross file boundaries. Entities created by a
 * failed load are deleted before this function returns.
 *
 * @param paths Array of file paths.
 * @param path_count Number of entries in paths.
 * @return EngineResult describing success or failure.
 */
EngineResult game_state_files_load(const char *const *paths, size_t path_count);

/** Load entities from one JSON state file. */
EngineResult game_state_file_load(const char *path);

/** Find a UI button definition loaded from JSON by its authored name. */
UIButtonDefinitionResult ui_button_by_name_get(const char *name);

/** Find a UI font definition loaded from JSON by name. */
UIFontDefinitionResult ui_font_by_name_get(const char *name);

/** Find a standalone UI label definition loaded from JSON by name. */
UILabelDefinitionResult ui_label_by_name_get(const char *name);

/** Find a UI slider definition loaded from JSON by name. */
UISliderDefinitionResult ui_slider_by_name_get(const char *name);

/**
 * Save all named live entities to one JSON state file.
 *
 * Unnamed entities are omitted because stable cross-session references require
 * names. Runtime-derived interaction and broad-phase state are omitted.
 */
EngineResult game_state_file_save(const char *path);

/**
 * Save retained authored state definitions without expanding prototypes.
 *
 * This merges the immutable definitions from all successfully loaded state
 * documents. It preserves count, placement, and variation declarations but
 * does not capture runtime mutations.
 */
EngineResult game_state_template_file_save(const char *path);

#endif
