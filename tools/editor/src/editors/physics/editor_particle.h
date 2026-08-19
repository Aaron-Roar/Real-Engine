#ifndef ROHR_EDITOR_PARTICLE_H
#define ROHR_EDITOR_PARTICLE_H

#include "editors/editor_mode_context.h"

typedef struct EditorParticleEditor {
    TextAsset title;
    TextAsset radius_label;
    TextAsset auto_fit_label;
    TextAsset ring_color_label;
    TextAsset fill_color_label;
    TextAsset radius_field;
} EditorParticleEditor;

bool editor_particle_editor_create(EditorParticleEditor *editor,
    FontAsset *font);
void editor_particle_editor_destroy(EditorParticleEditor *editor);
bool editor_particle_editor_draw(EditorParticleEditor *editor,
    const EditorModeContext *context);

#endif
