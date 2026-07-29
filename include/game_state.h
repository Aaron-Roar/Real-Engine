#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stddef.h>
#include "error.h"

/** Current JSON game-state schema version. */
#define GAME_STATE_VERSION 1

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
EngineResult game_state_load_files(const char *const *paths, size_t path_count);

/** Load entities from one JSON state file. */
EngineResult game_state_load_file(const char *path);

/**
 * Save all named live entities to one JSON state file.
 *
 * Unnamed entities are omitted because stable cross-session references require
 * names. Runtime-only collision reports and loaded graphics assets are omitted.
 */
EngineResult game_state_save_file(const char *path);

#endif
