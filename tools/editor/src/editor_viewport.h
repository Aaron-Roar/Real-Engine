#ifndef ROHR_EDITOR_VIEWPORT_H
#define ROHR_EDITOR_VIEWPORT_H

#include "editor_project.h"

#define EDITOR_VIEWPORT_ROTATION_ARM_LENGTH 60.0f
#include "editor_auto_shape.h"

typedef enum EditorViewportMode {
    EDITOR_VIEWPORT_HIERARCHY,
    EDITOR_VIEWPORT_OBJECT,
    EDITOR_VIEWPORT_RIGID_BODY,
    EDITOR_VIEWPORT_PARTICLE,
    EDITOR_VIEWPORT_HITBOX,
    EDITOR_VIEWPORT_JOINT,
    EDITOR_VIEWPORT_ANCHOR,
    EDITOR_VIEWPORT_SOFT_BODY,
    EDITOR_VIEWPORT_SOFT_NODE,
    EDITOR_VIEWPORT_SOFT_BEAM,
    EDITOR_VIEWPORT_SOFT_AREA,
    EDITOR_VIEWPORT_ORIGIN,
    EDITOR_VIEWPORT_LINE,
    EDITOR_VIEWPORT_VERTEX,
    EDITOR_VIEWPORT_AUTO_SHAPE,
    EDITOR_VIEWPORT_SPRITE,
    EDITOR_VIEWPORT_ANIMATED_SPRITE,
    EDITOR_VIEWPORT_ANIMATION_FRAME
} EditorViewportMode;

typedef enum EditorHierarchySelection {
    EDITOR_SELECTION_NONE,
    EDITOR_SELECTION_OBJECT,
    EDITOR_SELECTION_RIGID_BODY,
    EDITOR_SELECTION_PARTICLE,
    EDITOR_SELECTION_HITBOX,
    EDITOR_SELECTION_JOINT,
    EDITOR_SELECTION_ANCHOR,
    EDITOR_SELECTION_SOFT_BODY,
    EDITOR_SELECTION_SOFT_NODE,
    EDITOR_SELECTION_SOFT_BEAM,
    EDITOR_SELECTION_SOFT_AREA,
    EDITOR_SELECTION_ORIGIN,
    EDITOR_SELECTION_LINE,
    EDITOR_SELECTION_VERTEX,
    EDITOR_SELECTION_SPRITE,
    EDITOR_SELECTION_ANIMATED_SPRITE,
    EDITOR_SELECTION_ANIMATION_FRAME
} EditorHierarchySelection;

typedef enum EditorOriginKind {
    EDITOR_ORIGIN_NONE,
    EDITOR_ORIGIN_RIGID_BODY,
    EDITOR_ORIGIN_SOFT_BODY
} EditorOriginKind;

typedef struct EditorSelectionRef {
    EditorHierarchySelection kind;
    EditorObjectId object;
    uint32_t parent;
    uint32_t container;
    uint32_t item;
} EditorSelectionRef;

typedef struct EditorViewportState {
    int dragged_vertex;
    bool dragged_body;
    bool rotated_body;
    bool dragged_anchor;
    bool dragged_soft_node;
    bool dragged_soft_body;
    bool dragged_sprite;
    bool dragged_animated_sprite;
    bool rotated_soft_body;
    bool dragged_origin;
    bool group_dragging;
    bool group_rotating;
    bool camera_panning;
    bool camera_pan_with_primary;
    bool marquee_active;
    bool selection_modifier;
    Vec2D drag_offset;
    Position camera_pointer;
    Position marquee_start;
    Position marquee_end;
    Position group_pointer;
    Position group_pivot;
    float group_pointer_angle;
    float rotation_pointer_offset;
    EditorViewportMode mode;
    EditorHierarchySelection selection;
    EditorHierarchySelection last_viewport_click_selection;
    EditorObjectId last_viewport_click_object;
    uint32_t last_viewport_click_index;
    Uint64 last_viewport_click_at;
    uint32_t selected_line;
    uint32_t selected_vertex;
    EditorRigidBodyId selected_rigid_body;
    EditorHitboxId selected_hitbox;
    EditorJointId selected_joint;
    EditorAnchorId selected_anchor;
    EditorSoftBodyId selected_soft_body;
    EditorSoftNodeId selected_soft_node;
    EditorSoftBeamId selected_soft_beam;
    EditorSoftAreaId selected_soft_area;
    EditorSpriteId selected_sprite;
    EditorAnimatedSpriteId selected_animated_sprite;
    EditorSpriteId selected_animation_frame;
    EditorSoftAreaId soft_area_candidates[EDITOR_SOFT_AREA_MAX];
    size_t soft_area_candidate_count;
    EditorOriginKind selected_origin_kind;
    EditorViewportMode auto_shape_parent_mode;
    uint32_t auto_shape_points[EDITOR_SOFT_NODE_MAX];
    size_t auto_shape_point_count;
    EditorRigidBodyId preview_rigid_body;
    EditorAnchorId preview_anchor;
    EditorSoftNodeId preview_soft_node;
    EditorSelectionRef *selected_items;
    size_t selected_item_count;
    size_t selected_item_capacity;
} EditorViewportState;

void editor_viewport_state_init(EditorViewportState *state);
void editor_viewport_state_destroy(EditorViewportState *state);
void editor_viewport_selection_clear(EditorViewportState *state);
bool editor_viewport_selection_ref_get(const EditorProject *project,
    const EditorViewportState *state, EditorSelectionRef *selection);
bool editor_viewport_selection_contains(const EditorViewportState *state,
    EditorSelectionRef selection);
bool editor_viewport_selection_homogeneous_check(const EditorViewportState *state);
bool editor_viewport_selection_set(EditorProject *project,
    EditorViewportState *state, EditorSelectionRef selection, bool additive);
void editor_viewport_marquee_begin(EditorViewportState *state, Position pointer);
void editor_viewport_marquee_update(EditorViewportState *state, Position pointer);
bool editor_viewport_marquee_finish(EditorViewportState *state,
    EditorProject *project, Position pointer);
void editor_viewport_hitbox_editor_enter(EditorViewportState *state);
void editor_viewport_object_editor_enter(EditorViewportState *state);
void editor_viewport_hitbox_editor_exit(EditorViewportState *state);
bool editor_viewport_hitbox_editor_active_get(const EditorViewportState *state);
void editor_viewport_line_editor_enter(EditorViewportState *state, uint32_t line);
void editor_viewport_vertex_editor_enter(EditorViewportState *state, uint32_t vertex);
void editor_viewport_back(EditorViewportState *state);
bool editor_viewport_update(
    EditorViewportState *state,
    EditorProject *project,
    Position pointer,
    MouseButtonState primary_button,
    MouseButtonState pan_button,
    bool pan_modifier,
    float wheel_y,
    bool pointer_consumed
);
bool editor_viewport_auto_shape_update(EditorViewportState *state,
    EditorProject *project, EditorAutoShapeConfig *config, Position pointer,
    MouseButtonState primary_button, MouseButtonState pan_button,
    bool pan_modifier, float wheel_y, bool pointer_consumed);
void editor_viewport_draw(
    const EditorProject *project,
    const EditorViewportState *state,
    bool grid_visible
);
void editor_viewport_asset_root_set(const char *path);
void editor_viewport_assets_destroy(void);
bool editor_viewport_selection_nudge(EditorViewportState *state,
    EditorProject *project, Vec2D screen_delta);

#endif
