/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_ERROR_NOTIFICATIONS_H
#define ROHR_EDITOR_ERROR_NOTIFICATIONS_H

#include "editor_error.h"
#include "panels/editor_notification_panel.h"

void editor_error_notification_failure(EditorNotificationPanel *system,
    const char *operation, EditorResult result);

#endif
