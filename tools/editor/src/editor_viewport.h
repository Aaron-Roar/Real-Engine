#ifndef ROHR_EDITOR_VIEWPORT_H
#define ROHR_EDITOR_VIEWPORT_H

#include "editor_project.h"

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
    EDITOR_VIEWPORT_VERTEX
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
    EDITOR_SELECTION_VERTEX
} EditorHierarchySelection;

typedef enum EditorOriginKind {
    EDITOR_ORIGIN_NONE,
    EDITOR_ORIGIN_RIGID_BODY,
    EDITOR_ORIGIN_SOFT_BODY
} EditorOriginKind;

typedef struct EditorViewportState {
    int dragged_vertex;
    bool dragged_body;
    bool rotated_body;
    bool dragged_anchor;
    bool dragged_soft_node;
    bool dragged_soft_body;
    bool rotated_soft_body;
    bool dragged_origin;
    bool camera_panning;
    bool camera_pan_with_primary;
    bool local_view;
    Vec2D drag_offset;
    Vec2D camera_offset;
    float camera_zoom;
    Position camera_pointer;
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
    EditorSoftAreaId soft_area_candidates[EDITOR_SOFT_AREA_MAX];
    size_t soft_area_candidate_count;
    EditorOriginKind selected_origin_kind;
    EditorRigidBodyId preview_rigid_body;
    EditorAnchorId preview_anchor;
    EditorSoftNodeId preview_soft_node;
} EditorViewportState;

void editor_viewport_state_init(EditorViewportState *state);
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
void editor_viewport_draw(
    const EditorProject *project,
    const EditorViewportState *state
);
bool editor_viewport_selection_nudge(EditorViewportState *state,
    EditorProject *project, Vec2D screen_delta);

#endif
