/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_PARTICLE_H
#define ROHR_EDITOR_PARTICLE_H

#include "editors/editor_mode_context.h"

typedef struct EditorParticleEditor {
    TextAsset title;
    TextAsset radius_label;
    TextAsset origin_x_label;
    TextAsset origin_y_label;
    TextAsset auto_fit_label;
    TextAsset ring_color_label;
    TextAsset fill_color_label;
    TextAsset radius_field;
    TextAsset origin_x_field;
    TextAsset origin_y_field;
} EditorParticleEditor;

bool editor_particle_editor_create(EditorParticleEditor *editor,
    FontAsset *font);
void editor_particle_editor_destroy(EditorParticleEditor *editor);
bool editor_particle_editor_draw(EditorParticleEditor *editor,
    const EditorModeContext *context);

#endif
