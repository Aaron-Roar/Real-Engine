#ifndef ROHR_EDITOR_H
#define ROHR_EDITOR_H

#include "rohr.h"

/**
 * @file rohr_editor.h
 * @brief Public Real Engine editor API facade.
 */

/**
 * Initialize editor-owned state.
 *
 * The engine must be initialized before this function is called.
 *
 * @return EngineResult describing success or failure.
 */
EngineResult RE_init(void);

/**
 * Process one editor update.
 *
 * @return EngineResult describing success or failure.
 */
EngineResult RE_update(void);

#endif
