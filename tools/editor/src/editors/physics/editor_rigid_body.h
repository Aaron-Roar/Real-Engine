#ifndef EDITOR_RIGID_BODY_H
#define EDITOR_RIGID_BODY_H

#include "editors/editor_mode_context.h"

typedef bool (*EditorRigidBodyCollisionMenuFunction)(void *context,
    const char *id_prefix, EditorProject *project, uint64_t *active_masks,
    EditorObjectId object, EditorRigidBodyId body,
    EditorCollisionFilterKind filter, float x, float y, float width,
    bool *field_active, size_t *row_count);

typedef struct EditorRigidBodyEditor {
    FontAsset *font;
    TextAsset name_label, x_label, y_label, rotation_label;
    TextAsset mass_label, friction_label, restitution_label;
    TextAsset border_color_label, surface_color_label;
    TextAsset gravity_label, dynamic_label, static_label;
    TextAsset rotation_unlocked_label, rotation_locked_label;
    TextAsset collision_label, particle_label;
    TextAsset collision_category_label, collide_with_label;
    TextAsset origin_label, add_hitbox_label, delete_label;
    TextAsset visible_label, hidden_label;
    TextAsset x_field, y_field, rotation_field;
    TextAsset mass_field, friction_field, restitution_field;
    TextAsset body_names[EDITOR_RIGID_BODY_MAX];
    TextAsset hitbox_names[EDITOR_BODY_HITBOX_MAX];
    char body_cache[EDITOR_RIGID_BODY_MAX][EDITOR_OBJECT_NAME_MAX];
    char hitbox_cache[EDITOR_BODY_HITBOX_MAX][EDITOR_OBJECT_NAME_MAX];
    bool collision_category_open;
    bool collide_with_open;
} EditorRigidBodyEditor;

bool editor_rigid_body_editor_create(EditorRigidBodyEditor *editor,
    FontAsset *font);
void editor_rigid_body_editor_destroy(EditorRigidBodyEditor *editor);
bool editor_rigid_body_editor_draw(EditorRigidBodyEditor *editor,
    const EditorModeContext *context,
    EditorRigidBodyCollisionMenuFunction collision_menu,
    void *collision_menu_context);

#endif
