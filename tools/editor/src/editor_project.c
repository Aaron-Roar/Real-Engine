#include "editor_project.h"

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool editor_name_c_keyword(const char *name) {
    static const char *keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch", "typedef",
        "union", "unsigned", "void", "volatile", "while", "_alignas",
        "_alignof", "_atomic", "_bool", "_complex", "_generic", "_imaginary",
        "_noreturn", "_static_assert", "_thread_local"
    };
    for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i += 1) {
        if(strcmp(name, keywords[i]) == 0) return true;
    }
    return false;
}

static bool editor_name_word_start(const char *input, size_t index) {
    unsigned char current = (unsigned char)input[index];
    unsigned char previous = index == 0 ? 0 : (unsigned char)input[index - 1];
    unsigned char next = (unsigned char)input[index + 1];
    if(!isalnum(current)) return false;
    if(index == 0 || !isalnum(previous)) return true;
    if(isupper(current) && (islower(previous) || isdigit(previous))) return true;
    return isupper(current) && isupper(previous) && islower(next);
}

void editor_project_object_name_format(char *output, size_t capacity,
    const char *input) {
    char formatted[EDITOR_OBJECT_NAME_MAX] = {0};
    size_t written = 0;
    bool word = false;
    if(output == NULL || capacity == 0) return;
    if(input != NULL) {
        for(size_t i = 0; input[i] != '\0' && written + 1 < sizeof(formatted); i += 1) {
            unsigned char character = (unsigned char)input[i];
            bool start = editor_name_word_start(input, i);
            if(!isalnum(character)) {
                word = false;
                continue;
            }
            if(start || !word) {
                formatted[written++] = (char)toupper(character);
                word = true;
            } else {
                formatted[written++] = (char)tolower(character);
            }
        }
    }
    if(written == 0) snprintf(formatted, sizeof(formatted), "Object");
    if(isdigit((unsigned char)formatted[0])) {
        char prefixed[EDITOR_OBJECT_NAME_MAX];
        snprintf(prefixed, sizeof(prefixed), "Object%s", formatted);
        snprintf(formatted, sizeof(formatted), "%s", prefixed);
    }
    snprintf(output, capacity, "%s", formatted);
}

void editor_project_property_name_format(char *output, size_t capacity,
    const char *input) {
    char formatted[EDITOR_OBJECT_NAME_MAX] = {0};
    size_t written = 0;
    bool have_word = false;
    if(output == NULL || capacity == 0) return;
    if(input != NULL) {
        for(size_t i = 0; input[i] != '\0' && written + 1 < sizeof(formatted); i += 1) {
            unsigned char character = (unsigned char)input[i];
            bool start = editor_name_word_start(input, i);
            if(!isalnum(character)) continue;
            if(start && have_word && written + 2 < sizeof(formatted)) {
                formatted[written++] = '_';
            }
            formatted[written++] = (char)tolower(character);
            have_word = true;
        }
    }
    if(written == 0) snprintf(formatted, sizeof(formatted), "item");
    if(isdigit((unsigned char)formatted[0]) || editor_name_c_keyword(formatted)) {
        char prefixed[EDITOR_OBJECT_NAME_MAX];
        snprintf(prefixed, sizeof(prefixed), "item_%s", formatted);
        snprintf(formatted, sizeof(formatted), "%s", prefixed);
    }
    snprintf(output, capacity, "%s", formatted);
}

static uint32_t editor_vertex_count_clamp(uint32_t vertex_count) {
    if(vertex_count < EDITOR_HITBOX_VERTEX_MIN) {
        return EDITOR_HITBOX_VERTEX_MIN;
    }
    if(vertex_count > EDITOR_HITBOX_VERTEX_MAX) {
        return EDITOR_HITBOX_VERTEX_MAX;
    }
    return vertex_count;
}

static void editor_hitbox_regular_set(EditorProject *project, EditorHitbox *hitbox,
    uint32_t vertex_count) {
    const float radius = 70.0f;

    if(project == NULL || hitbox == NULL) return;
    vertex_count = editor_vertex_count_clamp(vertex_count);
    hitbox->visible = true;
    hitbox->vertex_count = vertex_count;
    for(uint32_t i = 0; i < vertex_count; i += 1) {
        float angle = -1.57079632679f + 6.28318530718f *
            (float)i / (float)vertex_count;
        hitbox->vertices[i] = (EditorVertex){
            .id = project->next_vertex_id++,
            .position = {
                cosf(angle) * radius,
                sinf(angle) * radius
            }
        };
        snprintf(hitbox->vertices[i].name, sizeof(hitbox->vertices[i].name),
            "vertex_%u", i + 1);
        snprintf(hitbox->line_names[i], sizeof(hitbox->line_names[i]),
            "line_%u", i + 1);
    }
}

void editor_project_init(EditorProject *project) {
    if(project == NULL) return;
    *project = (EditorProject){
        .viewport_camera_zoom = 1.0f,
        .collision_masks = {{.name = "default"}},
        .collision_mask_count = 1,
        .next_id = 1,
        .next_vertex_id = 1,
        .next_rigid_body_id = 1,
        .next_hitbox_id = 1,
        .next_joint_id = 1,
        .next_anchor_id = 1,
        .next_soft_body_id = 1,
        .next_soft_node_id = 1,
        .next_soft_beam_id = 1,
        .next_soft_area_id = 1
    };
}

bool editor_project_collision_mask_add(EditorProject *project, const char *name,
    size_t *index) {
    char formatted[EDITOR_OBJECT_NAME_MAX];

    if(project == NULL || name == NULL || name[0] == '\0' ||
            project->collision_mask_count >= EDITOR_COLLISION_MASK_MAX) return false;
    editor_project_property_name_format(formatted, sizeof(formatted), name);
    if(formatted[0] == '\0') return false;
    for(size_t i = 0; i < project->collision_mask_count; i += 1) {
        if(strcmp(project->collision_masks[i].name, formatted) != 0) continue;
        if(index != NULL) *index = i;
        return false;
    }
    if(index != NULL) *index = project->collision_mask_count;
    snprintf(project->collision_masks[project->collision_mask_count].name,
        sizeof(project->collision_masks[project->collision_mask_count].name),
        "%s", formatted);
    project->collision_mask_count += 1;
    return true;
}

EditorObject *editor_project_object_add(EditorProject *project, Position position) {
    EditorObject *object;

    if(project == NULL || project->object_count >= EDITOR_OBJECT_MAX) return NULL;
    object = &project->objects[project->object_count++];
    *object = (EditorObject){
        .id = project->next_id++,
        .position = position,
        .visible = true
    };
    snprintf(object->name, sizeof(object->name), "Object%u", object->id);
    project->selected = object->id;
    return object;
}

bool editor_project_object_remove(EditorProject *project, EditorObjectId id) {
    size_t index;

    if(project == NULL || id == EDITOR_OBJECT_INVALID) return false;
    for(index = 0; index < project->object_count; index += 1) {
        if(project->objects[index].id == id) break;
    }
    if(index == project->object_count) return false;
    for(size_t i = index + 1; i < project->object_count; i += 1) {
        project->objects[i - 1] = project->objects[i];
    }
    project->object_count -= 1;
    project->objects[project->object_count] = (EditorObject){0};
    if(project->selected == id) project->selected = EDITOR_OBJECT_INVALID;
    return true;
}

EditorObject *editor_project_selected_get(EditorProject *project) {
    if(project == NULL || project->selected == EDITOR_OBJECT_INVALID) return NULL;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id == project->selected) return &project->objects[i];
    }
    return NULL;
}

bool editor_project_object_select(EditorProject *project, EditorObjectId id) {
    if(project == NULL) return false;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id != id) continue;
        project->selected = id;
        return true;
    }
    return false;
}

void editor_project_selection_clear(EditorProject *project) {
    if(project == NULL) return;
    project->selected = EDITOR_OBJECT_INVALID;
}

EditorRigidBody editor_project_rigid_body_default_get(void) {
    return (EditorRigidBody){
        .mass_value = 1.0f,
        .friction = 0.5f,
        .restitution = 0.0f,
        .gravity_enabled = false,
        .collision_enabled = true,
        .particle_auto_fit = true,
        .particle_ring_color = UINT32_C(0x4a925fff),
        .particle_fill_color = UINT32_C(0x4a925f40),
        .border_color = UINT32_C(0xe6ebf4ff),
        .surface_color = UINT32_C(0x66758c80),
        .collision_category = UINT64_C(1),
        .collision_with = UINT64_C(1),
        .particle_auto_fit = true,
        .particle_radius = 30.0f,
        .particle_ring_color = UINT32_C(0x4a90e2ff),
        .particle_fill_color = UINT32_C(0x4a90e240),
        .border_color = UINT32_C(0xffffffff),
        .surface_color = UINT32_C(0x808080ff),
        .visible = true
    };
}

static bool editor_project_hierarchy_item_exists(const EditorObject *object,
        EditorHierarchyItem item) {
    if(object == NULL || item.id == 0) return false;
    if(item.kind == EDITOR_HIERARCHY_RIGID_BODY) {
        for(size_t i = 0; i < object->rigid_body_count; i += 1)
            if(object->rigid_bodies[i].id == item.id) return true;
    } else if(item.kind == EDITOR_HIERARCHY_JOINT) {
        for(size_t i = 0; i < object->joint_count; i += 1)
            if(object->joint_items[i].id == item.id) return true;
    } else if(item.kind == EDITOR_HIERARCHY_SOFT_BODY) {
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == item.id) return true;
    }
    return false;
}

static void editor_project_hierarchy_item_add(EditorObject *object,
        EditorHierarchyItemKind kind, uint32_t id) {
    if(object == NULL || id == 0 ||
            object->hierarchy_count >= EDITOR_OBJECT_HIERARCHY_MAX) return;
    object->hierarchy[object->hierarchy_count++] = (EditorHierarchyItem){kind, id};
}

void editor_project_object_hierarchy_sync(EditorObject *object) {
    size_t output = 0;
    if(object == NULL) return;
    for(size_t i = 0; i < object->hierarchy_count; i += 1) {
        bool duplicate = false;
        if(!editor_project_hierarchy_item_exists(object, object->hierarchy[i])) continue;
        for(size_t j = 0; j < output; j += 1)
            if(object->hierarchy[j].kind == object->hierarchy[i].kind &&
                    object->hierarchy[j].id == object->hierarchy[i].id)
                duplicate = true;
        if(duplicate) continue;
        object->hierarchy[output++] = object->hierarchy[i];
    }
    object->hierarchy_count = output;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        EditorHierarchyItem item = {EDITOR_HIERARCHY_RIGID_BODY,
            object->rigid_bodies[i].id};
        bool found = false;
        for(size_t j = 0; j < output; j += 1)
            if(object->hierarchy[j].kind == item.kind &&
                    object->hierarchy[j].id == item.id) found = true;
        if(!found) editor_project_hierarchy_item_add(object, item.kind, item.id);
    }
    for(size_t i = 0; i < object->joint_count; i += 1) {
        EditorHierarchyItem item = {EDITOR_HIERARCHY_JOINT, object->joint_items[i].id};
        bool found = false;
        for(size_t j = 0; j < object->hierarchy_count; j += 1)
            if(object->hierarchy[j].kind == item.kind &&
                    object->hierarchy[j].id == item.id) found = true;
        if(!found) editor_project_hierarchy_item_add(object, item.kind, item.id);
    }
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        EditorHierarchyItem item = {EDITOR_HIERARCHY_SOFT_BODY,
            object->soft_body_items[i].id};
        bool found = false;
        for(size_t j = 0; j < object->hierarchy_count; j += 1)
            if(object->hierarchy[j].kind == item.kind &&
                    object->hierarchy[j].id == item.id) found = true;
        if(!found) editor_project_hierarchy_item_add(object, item.kind, item.id);
    }
}

size_t editor_project_object_hierarchy_index_get(const EditorObject *object,
        EditorHierarchyItemKind kind, uint32_t id) {
    if(object == NULL) return SIZE_MAX;
    for(size_t i = 0; i < object->hierarchy_count; i += 1)
        if(object->hierarchy[i].kind == kind && object->hierarchy[i].id == id) return i;
    return SIZE_MAX;
}

Position editor_project_particle_center_get(const EditorRigidBody *body) {
    const EditorHitbox *hitbox;
    float twice_area = 0.0f;
    Position weighted = {0};

    if(body == NULL || body->hitbox_count == 0) return (Position){0};
    hitbox = &body->hitboxes[0];
    if(hitbox->vertex_count == 0) return (Position){0};
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        Position a = hitbox->vertices[i].position;
        Position b = hitbox->vertices[(i + 1) % hitbox->vertex_count].position;
        float cross = a.x * b.y - b.x * a.y;
        twice_area += cross;
        weighted.x += (a.x + b.x) * cross;
        weighted.y += (a.y + b.y) * cross;
    }
    if(fabsf(twice_area) > 0.0001f) {
        return (Position){weighted.x / (3.0f * twice_area),
            weighted.y / (3.0f * twice_area)};
    }
    weighted = (Position){0};
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        weighted.x += hitbox->vertices[i].position.x;
        weighted.y += hitbox->vertices[i].position.y;
    }
    return (Position){weighted.x / (float)hitbox->vertex_count,
        weighted.y / (float)hitbox->vertex_count};
}

float editor_project_particle_auto_radius_get(const EditorRigidBody *body) {
    Position center = editor_project_particle_center_get(body);
    float radius = 0.0f;

    if(body == NULL || body->hitbox_count == 0) return radius;
    for(uint32_t i = 0; i < body->hitboxes[0].vertex_count; i += 1) {
        Position point = body->hitboxes[0].vertices[i].position;
        radius = fmaxf(radius, hypotf(point.x - center.x, point.y - center.y));
    }
    return radius;
}

void editor_project_particle_auto_fit_update(EditorProject *project) {
    if(project == NULL) return;
    for(size_t object_index = 0; object_index < project->object_count; object_index += 1) {
        EditorObject *object = &project->objects[object_index];
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            EditorRigidBody *body = &object->rigid_bodies[body_index];
            if(body->particle && body->particle_auto_fit)
                body->particle_radius = editor_project_particle_auto_radius_get(body);
        }
    }
}

EditorJoint editor_project_joint_default_get(EditorJointKind kind) {
    return (EditorJoint){
        .kind = kind,
        .rest_length = 0.0f,
        .stiffness = 100.0f,
        .damping = kind == EDITOR_JOINT_SPRING ? 10.0f : 0.0f,
        .visual_size = 1.0f,
        .visible = true
    };
}

EditorRigidBody *editor_project_rigid_body_add(EditorProject *project,
    EditorObject *object) {
    EditorRigidBody *body;

    if(project == NULL || object == NULL ||
            object->rigid_body_count >= EDITOR_RIGID_BODY_MAX) return NULL;
    body = &object->rigid_bodies[object->rigid_body_count++];
    *body = editor_project_rigid_body_default_get();
    body->id = project->next_rigid_body_id++;
    snprintf(body->name, sizeof(body->name), "rigid_body_%u", body->id);
    if(editor_project_hitbox_add(project, body) == NULL) {
        object->rigid_body_count -= 1;
        object->rigid_bodies[object->rigid_body_count] = (EditorRigidBody){0};
        return NULL;
    }
    editor_project_hierarchy_item_add(object, EDITOR_HIERARCHY_RIGID_BODY, body->id);
    return body;
}

EditorRigidBody *editor_project_rigid_body_get(EditorObject *object,
    EditorRigidBodyId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id == id) return &object->rigid_bodies[i];
    }
    return NULL;
}

bool editor_project_rigid_body_remove(EditorObject *object, EditorRigidBodyId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id != id) continue;
        for(size_t j = 0; j < object->anchor_count; j += 1) {
            EditorAnchor *anchor = &object->anchors[j];
            if(anchor->rigid_body != id) continue;
            if(anchor->position_follows_body) {
                (void)editor_project_anchor_position_lock_set(object, anchor, false);
            }
            if(anchor->rotation_follows_body) {
                (void)editor_project_anchor_rotation_lock_set(object, anchor, false);
            }
            anchor->rigid_body = 0;
        }
        for(size_t j = i + 1; j < object->rigid_body_count; j += 1) {
            object->rigid_bodies[j - 1] = object->rigid_bodies[j];
        }
        object->rigid_body_count -= 1;
        object->rigid_bodies[object->rigid_body_count] = (EditorRigidBody){0};
        editor_project_object_hierarchy_sync(object);
        return true;
    }
    return false;
}

bool editor_project_rigid_body_origin_set(EditorObject *object, EditorRigidBody *body,
    Position position) {
    Vec2D world_delta;
    Vec2D local_delta;
    float cosine;
    float sine;

    if(object == NULL || body == NULL) return false;
    world_delta = (Vec2D){position.x - body->position.x,
        position.y - body->position.y};
    cosine = cosf(-body->rotation);
    sine = sinf(-body->rotation);
    local_delta = (Vec2D){world_delta.x * cosine - world_delta.y * sine,
        world_delta.x * sine + world_delta.y * cosine};
    body->position = position;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        for(uint32_t vertex = 0; vertex < body->hitboxes[i].vertex_count; vertex += 1) {
            body->hitboxes[i].vertices[vertex].position.x -= local_delta.x;
            body->hitboxes[i].vertices[vertex].position.y -= local_delta.y;
        }
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        EditorAnchor *anchor = &object->anchors[i];
        if(anchor->rigid_body != body->id || !anchor->position_follows_body) continue;
        anchor->position.x -= local_delta.x;
        anchor->position.y -= local_delta.y;
    }
    return true;
}

EditorAnchor *editor_project_anchor_get(EditorObject *object, EditorAnchorId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        if(object->anchors[i].id == id) return &object->anchors[i];
    }
    return NULL;
}

EditorAnchor *editor_project_anchor_add(EditorProject *project, EditorObject *object,
    Position position, EditorRigidBodyId rigid_body) {
    EditorAnchor *anchor;

    if(project == NULL || object == NULL || object->anchor_count >= EDITOR_ANCHOR_MAX ||
            (rigid_body != 0 && editor_project_rigid_body_get(object, rigid_body) == NULL)) {
        return NULL;
    }
    anchor = &object->anchors[object->anchor_count++];
    *anchor = (EditorAnchor){.id = project->next_anchor_id++, .position = position,
        .rigid_body = rigid_body, .position_follows_body = rigid_body != 0,
        .rotation_follows_body = rigid_body != 0, .visible = true};
    snprintf(anchor->name, sizeof(anchor->name), "anchor_%u", anchor->id);
    return anchor;
}

static Position editor_position_rotate(Position position, float rotation) {
    float cosine = cosf(rotation);
    float sine = sinf(rotation);
    return (Position){position.x * cosine - position.y * sine,
        position.x * sine + position.y * cosine};
}

bool editor_project_anchor_position_lock_set(EditorObject *object, EditorAnchor *anchor,
    bool locked) {
    EditorRigidBody *body;
    Position position;

    if(object == NULL || anchor == NULL) return false;
    if(anchor->position_follows_body == locked) return true;
    body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body == NULL) return false;
    if(locked) {
        position = (Position){anchor->position.x - body->position.x,
            anchor->position.y - body->position.y};
        anchor->position = editor_position_rotate(position, -body->rotation);
    } else {
        position = editor_position_rotate(anchor->position, body->rotation);
        anchor->position = (Position){body->position.x + position.x,
            body->position.y + position.y};
    }
    anchor->position_follows_body = locked;
    return true;
}

bool editor_project_anchor_rotation_lock_set(EditorObject *object, EditorAnchor *anchor,
    bool locked) {
    EditorRigidBody *body;

    if(object == NULL || anchor == NULL) return false;
    if(anchor->rotation_follows_body == locked) return true;
    body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body == NULL) return false;
    anchor->rotation += locked ? -body->rotation : body->rotation;
    anchor->rotation_follows_body = locked;
    return true;
}

bool editor_project_anchor_rigid_body_set(EditorObject *object, EditorAnchor *anchor,
    EditorRigidBodyId rigid_body) {
    bool position_locked;
    bool rotation_locked;

    if(object == NULL || anchor == NULL || (rigid_body != 0 &&
            editor_project_rigid_body_get(object, rigid_body) == NULL)) return false;
    if(anchor->rigid_body == rigid_body) return true;
    position_locked = anchor->position_follows_body;
    rotation_locked = anchor->rotation_follows_body;
    if(position_locked && !editor_project_anchor_position_lock_set(object, anchor, false)) {
        return false;
    }
    if(rotation_locked && !editor_project_anchor_rotation_lock_set(object, anchor, false)) {
        return false;
    }
    anchor->rigid_body = rigid_body;
    if(rigid_body != 0) {
        if(position_locked) (void)editor_project_anchor_position_lock_set(object, anchor, true);
        if(rotation_locked) (void)editor_project_anchor_rotation_lock_set(object, anchor, true);
    }
    return true;
}

bool editor_project_anchor_remove(EditorObject *object, EditorAnchorId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        EditorJoint *joint = &object->joint_items[i];
        if(joint->anchor_a == id) joint->anchor_a = 0;
        if(joint->anchor_b == id) joint->anchor_b = 0;
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        if(object->anchors[i].id != id) continue;
        for(size_t j = i + 1; j < object->anchor_count; j += 1) {
            object->anchors[j - 1] = object->anchors[j];
        }
        object->anchor_count -= 1;
        object->anchors[object->anchor_count] = (EditorAnchor){0};
        return true;
    }
    return false;
}

EditorHitbox *editor_project_hitbox_add(EditorProject *project, EditorRigidBody *body) {
    EditorHitbox *hitbox;

    if(project == NULL || body == NULL || body->hitbox_count >= EDITOR_BODY_HITBOX_MAX) {
        return NULL;
    }
    hitbox = &body->hitboxes[body->hitbox_count++];
    *hitbox = (EditorHitbox){.id = project->next_hitbox_id++, .visible = true};
    snprintf(hitbox->name, sizeof(hitbox->name), "hitbox_%u", hitbox->id);
    editor_hitbox_regular_set(project, hitbox, EDITOR_HITBOX_VERTEX_MIN);
    return hitbox;
}

EditorHitbox *editor_project_hitbox_get(EditorRigidBody *body, EditorHitboxId id) {
    if(body == NULL || id == 0) return NULL;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        if(body->hitboxes[i].id == id) return &body->hitboxes[i];
    }
    return NULL;
}

bool editor_project_hitbox_remove(EditorRigidBody *body, EditorHitboxId id) {
    if(body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        if(body->hitboxes[i].id != id) continue;
        for(size_t j = i + 1; j < body->hitbox_count; j += 1) {
            body->hitboxes[j - 1] = body->hitboxes[j];
        }
        body->hitbox_count -= 1;
        body->hitboxes[body->hitbox_count] = (EditorHitbox){0};
        return true;
    }
    return false;
}

bool editor_project_hitbox_vertex_remove(EditorHitbox *hitbox, uint32_t vertex_index) {
    if(hitbox == NULL) return false;
    if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN ||
            vertex_index >= hitbox->vertex_count) return false;
    for(uint32_t i = vertex_index + 1; i < hitbox->vertex_count; i += 1) {
        hitbox->vertices[i - 1] = hitbox->vertices[i];
        snprintf(hitbox->line_names[i - 1], sizeof(hitbox->line_names[i - 1]),
            "%s", hitbox->line_names[i]);
    }
    hitbox->vertex_count -= 1;
    hitbox->vertices[hitbox->vertex_count] = (EditorVertex){0};
    hitbox->line_names[hitbox->vertex_count][0] = '\0';
    return true;
}

bool editor_project_hitbox_line_remove(EditorHitbox *hitbox, uint32_t line_index) {
    if(hitbox == NULL || line_index >= hitbox->vertex_count) return false;
    return editor_project_hitbox_vertex_remove(hitbox,
        (line_index + 1) % hitbox->vertex_count);
}

bool editor_project_hitbox_vertex_insert(EditorProject *project, EditorHitbox *hitbox,
    uint32_t line_index) {
    uint32_t second;
    EditorVertex inserted;

    if(project == NULL || hitbox == NULL) return false;
    if(hitbox->vertex_count >= EDITOR_HITBOX_VERTEX_MAX ||
            line_index >= hitbox->vertex_count) return false;
    second = (line_index + 1) % hitbox->vertex_count;
    inserted = (EditorVertex){
        .id = project->next_vertex_id++,
        .position = {
            (hitbox->vertices[line_index].position.x +
                hitbox->vertices[second].position.x) * 0.5f,
            (hitbox->vertices[line_index].position.y +
                hitbox->vertices[second].position.y) * 0.5f
        }
    };
    snprintf(inserted.name, sizeof(inserted.name), "vertex_%u",
        hitbox->vertex_count + 1);
    if(second == 0) {
        hitbox->vertices[hitbox->vertex_count] = inserted;
        snprintf(hitbox->line_names[hitbox->vertex_count],
            sizeof(hitbox->line_names[hitbox->vertex_count]), "line_%u",
            hitbox->vertex_count + 1);
    } else {
        for(uint32_t i = hitbox->vertex_count; i > second; i -= 1) {
            hitbox->vertices[i] = hitbox->vertices[i - 1];
            snprintf(hitbox->line_names[i], sizeof(hitbox->line_names[i]),
                "%s", hitbox->line_names[i - 1]);
        }
        hitbox->vertices[second] = inserted;
        snprintf(hitbox->line_names[second], sizeof(hitbox->line_names[second]),
            "line_%u", hitbox->vertex_count + 1);
    }
    hitbox->vertex_count += 1;
    return true;
}

float editor_project_hitbox_line_length_get(const EditorHitbox *hitbox,
    uint32_t line_index) {
    uint32_t second;
    Vec2D delta;

    if(hitbox == NULL || line_index >= hitbox->vertex_count) return 0.0f;
    second = (line_index + 1) % hitbox->vertex_count;
    delta = (Vec2D){
        hitbox->vertices[second].position.x - hitbox->vertices[line_index].position.x,
        hitbox->vertices[second].position.y - hitbox->vertices[line_index].position.y
    };
    return sqrtf(delta.x * delta.x + delta.y * delta.y);
}

bool editor_project_hitbox_line_length_set(EditorHitbox *hitbox,
    uint32_t line_index, float length) {
    EditorVertex *first;
    EditorVertex *second;
    Vec2D direction;
    float current;

    if(hitbox == NULL || length <= 0.001f ||
            line_index >= hitbox->vertex_count) return false;
    first = &hitbox->vertices[line_index];
    second = &hitbox->vertices[(line_index + 1) % hitbox->vertex_count];
    if(first->position_locked && second->position_locked) return false;
    direction = (Vec2D){second->position.x - first->position.x,
        second->position.y - first->position.y};
    current = sqrtf(direction.x * direction.x + direction.y * direction.y);
    if(current <= 0.001f) return false;
    direction.x /= current;
    direction.y /= current;
    if(first->position_locked) {
        second->position.x = first->position.x + direction.x * length;
        second->position.y = first->position.y + direction.y * length;
    } else if(second->position_locked) {
        first->position.x = second->position.x - direction.x * length;
        first->position.y = second->position.y - direction.y * length;
    } else {
        Position midpoint = {(first->position.x + second->position.x) * 0.5f,
            (first->position.y + second->position.y) * 0.5f};
        first->position = (Position){midpoint.x - direction.x * length * 0.5f,
            midpoint.y - direction.y * length * 0.5f};
        second->position = (Position){midpoint.x + direction.x * length * 0.5f,
            midpoint.y + direction.y * length * 0.5f};
    }
    return true;
}

static Position editor_anchor_world_position_get(EditorObject *object,
    EditorAnchor *anchor) {
    EditorRigidBody *body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body != NULL && anchor->position_follows_body) {
        Position offset = editor_position_rotate(anchor->position, body->rotation);
        return (Position){body->position.x + offset.x, body->position.y + offset.y};
    }
    return anchor->position;
}

static bool editor_project_joint_constraint_from_endpoint_apply(
    EditorObject *object,
    EditorJoint *joint,
    uint32_t driver_endpoint
) {
    EditorAnchor *driver;
    EditorAnchor *driven;
    EditorRigidBody *driver_body;
    EditorRigidBody *driven_body;
    Position driver_world;
    Position driven_world;

    if(object == NULL || joint == NULL || driver_endpoint > 1 ||
            joint->kind == EDITOR_JOINT_SPRING) return false;
    driver = editor_project_anchor_get(object,
        driver_endpoint == 0 ? joint->anchor_a : joint->anchor_b);
    driven = editor_project_anchor_get(object,
        driver_endpoint == 0 ? joint->anchor_b : joint->anchor_a);
    if(driver == NULL || driven == NULL) return false;
    driver_body = editor_project_rigid_body_get(object, driver->rigid_body);
    driven_body = editor_project_rigid_body_get(object, driven->rigid_body);
    if(driven_body == driver_body && driven_body != NULL) return true;

    if(joint->kind == EDITOR_JOINT_WELD && driven_body != NULL) {
        if(driver_body != NULL) {
            driven_body->rotation = driver_endpoint == 0 ?
                driver_body->rotation + joint->rest_angle :
                driver_body->rotation - joint->rest_angle;
        }
    }
    driver_world = editor_anchor_world_position_get(object, driver);
    driven_world = editor_anchor_world_position_get(object, driven);
    if(driven_body != NULL && driven->position_follows_body) {
        driven_body->position.x += driver_world.x - driven_world.x;
        driven_body->position.y += driver_world.y - driven_world.y;
    } else {
        driven->position = driver_world;
    }
    return true;
}

bool editor_project_joint_constraints_apply(EditorObject *object, EditorJoint *joint) {
    if(object == NULL || joint == NULL) return false;
    if(joint->kind == EDITOR_JOINT_SPRING) return true;
    if(editor_project_anchor_get(object, joint->anchor_a) == NULL ||
            editor_project_anchor_get(object, joint->anchor_b) == NULL) return true;
    return editor_project_joint_constraint_from_endpoint_apply(object, joint, 0);
}

bool editor_project_joint_kind_set(EditorObject *object, EditorJoint *joint,
    EditorJointKind kind) {
    if(object == NULL || joint == NULL || kind < EDITOR_JOINT_REVOLUTE ||
            kind > EDITOR_JOINT_SPRING) return false;
    joint->kind = kind;
    if(kind == EDITOR_JOINT_WELD && joint->anchor_a != 0 && joint->anchor_b != 0) {
        EditorAnchor *a = editor_project_anchor_get(object, joint->anchor_a);
        EditorAnchor *b = editor_project_anchor_get(object, joint->anchor_b);
        EditorRigidBody *body_a = a == NULL ? NULL :
            editor_project_rigid_body_get(object, a->rigid_body);
        EditorRigidBody *body_b = b == NULL ? NULL :
            editor_project_rigid_body_get(object, b->rigid_body);

        if(body_a != NULL && body_b != NULL) {
            joint->rest_angle = body_b->rotation - body_a->rotation;
        }
    }
    return editor_project_joint_constraints_apply(object, joint);
}

void editor_project_anchor_constraints_apply(EditorObject *object, EditorAnchorId anchor) {
    if(object == NULL || anchor == 0) return;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        EditorJoint *joint = &object->joint_items[i];
        if(joint->anchor_a == anchor || joint->anchor_b == anchor) {
            if(joint->kind != EDITOR_JOINT_SPRING && joint->anchor_a != 0 &&
                    joint->anchor_b != 0) {
                (void)editor_project_joint_constraint_from_endpoint_apply(
                    object, joint, joint->anchor_b == anchor ? 1u : 0u);
            }
        }
    }
}

void editor_project_rigid_body_constraints_apply(EditorObject *object,
    EditorRigidBodyId rigid_body) {
    bool resolved[EDITOR_RIGID_BODY_MAX] = {false};
    EditorRigidBodyId queue[EDITOR_RIGID_BODY_MAX];
    size_t queue_begin = 0;
    size_t queue_end = 0;
    EditorRigidBody *body;

    if(object == NULL || rigid_body == 0) return;
    body = editor_project_rigid_body_get(object, rigid_body);
    if(body == NULL) return;
    resolved[(size_t)(body - object->rigid_bodies)] = true;
    queue[queue_end++] = rigid_body;
    while(queue_begin < queue_end) {
        EditorRigidBodyId driver_id = queue[queue_begin++];

        for(size_t i = 0; i < object->joint_count; i += 1) {
            EditorJoint *joint = &object->joint_items[i];
            EditorAnchor *a;
            EditorAnchor *b;
            EditorRigidBody *driven_body;
            uint32_t driver_endpoint;
            size_t driven_index;

            if(joint->kind == EDITOR_JOINT_SPRING) continue;
            a = editor_project_anchor_get(object, joint->anchor_a);
            b = editor_project_anchor_get(object, joint->anchor_b);
            if(a == NULL || b == NULL) continue;
            if(a->rigid_body == driver_id) driver_endpoint = 0;
            else if(b->rigid_body == driver_id) driver_endpoint = 1;
            else continue;
            driven_body = editor_project_rigid_body_get(object,
                driver_endpoint == 0 ? b->rigid_body : a->rigid_body);
            if(driven_body == NULL) {
                (void)editor_project_joint_constraint_from_endpoint_apply(
                    object, joint, driver_endpoint == 0 ? 1 : 0);
                continue;
            }
            driven_index = (size_t)(driven_body - object->rigid_bodies);
            if(resolved[driven_index]) continue;
            (void)editor_project_joint_constraint_from_endpoint_apply(
                object, joint, driver_endpoint);
            resolved[driven_index] = true;
            queue[queue_end++] = driven_body->id;
        }
    }
}

EditorJoint *editor_project_joint_add(EditorProject *project, EditorObject *object,
    EditorJointKind kind) {
    EditorJoint *joint;

    if(project == NULL || object == NULL || object->joint_count >= EDITOR_JOINT_MAX) {
        return NULL;
    }
    joint = &object->joint_items[object->joint_count++];
    *joint = editor_project_joint_default_get(kind);
    joint->id = project->next_joint_id++;
    snprintf(joint->name, sizeof(joint->name), "joint_%u", joint->id);
    editor_project_hierarchy_item_add(object, EDITOR_HIERARCHY_JOINT, joint->id);
    return joint;
}

bool editor_project_joint_remove(EditorObject *object, EditorJointId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        if(object->joint_items[i].id != id) continue;
        for(size_t j = i + 1; j < object->joint_count; j += 1) {
            object->joint_items[j - 1] = object->joint_items[j];
        }
        object->joint_count -= 1;
        object->joint_items[object->joint_count] = (EditorJoint){0};
        editor_project_object_hierarchy_sync(object);
        return true;
    }
    return false;
}

bool editor_project_joint_anchor_set(EditorObject *object, EditorJoint *joint,
    uint32_t endpoint, EditorAnchorId anchor) {
    if(object == NULL || joint == NULL || endpoint > 1 ||
            (anchor != 0 && editor_project_anchor_get(object, anchor) == NULL)) return false;
    if(endpoint == 0) joint->anchor_a = anchor;
    else joint->anchor_b = anchor;
    if(joint->kind == EDITOR_JOINT_SPRING && joint->anchor_a != 0 &&
            joint->anchor_b != 0) {
        EditorAnchor *a = editor_project_anchor_get(object, joint->anchor_a);
        EditorAnchor *b = editor_project_anchor_get(object, joint->anchor_b);
        Position world_a = editor_anchor_world_position_get(object, a);
        Position world_b = editor_anchor_world_position_get(object, b);
        joint->rest_length = hypotf(world_b.x - world_a.x, world_b.y - world_a.y);
    } else if(joint->kind == EDITOR_JOINT_WELD && joint->anchor_a != 0 &&
            joint->anchor_b != 0) {
        EditorAnchor *a = editor_project_anchor_get(object, joint->anchor_a);
        EditorAnchor *b = editor_project_anchor_get(object, joint->anchor_b);
        EditorRigidBody *body_a = editor_project_rigid_body_get(object, a->rigid_body);
        EditorRigidBody *body_b = editor_project_rigid_body_get(object, b->rigid_body);

        if(body_a != NULL && body_b != NULL) {
            joint->rest_angle = body_b->rotation - body_a->rotation;
        }
    }
    return editor_project_joint_constraints_apply(object, joint);
}

EditorSoftBody *editor_project_soft_body_add(EditorProject *project, EditorObject *object) {
    EditorSoftBody *body;

    if(project == NULL || object == NULL ||
            object->soft_body_count >= EDITOR_SOFT_BODY_MAX) return NULL;
    body = &object->soft_body_items[object->soft_body_count++];
    *body = (EditorSoftBody){
        .id = project->next_soft_body_id++,
        .node_color = UINT32_C(0xffaa46ff),
        .beam_color = UINT32_C(0xebf0f5ff),
        .area_color = UINT32_C(0x505a78ff),
        .visible = true
    };
    snprintf(body->name, sizeof(body->name), "soft_body_%u", body->id);
    editor_project_hierarchy_item_add(object, EDITOR_HIERARCHY_SOFT_BODY, body->id);
    return body;
}

bool editor_project_soft_body_remove(EditorObject *object, EditorSoftBodyId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        if(object->soft_body_items[i].id != id) continue;
        for(size_t j = i + 1; j < object->soft_body_count; j += 1) {
            object->soft_body_items[j - 1] = object->soft_body_items[j];
        }
        object->soft_body_count -= 1;
        object->soft_body_items[object->soft_body_count] = (EditorSoftBody){0};
        editor_project_object_hierarchy_sync(object);
        return true;
    }
    return false;
}

bool editor_project_soft_body_origin_set(EditorSoftBody *body, Position position) {
    Vec2D world_delta;
    Vec2D local_delta;
    float cosine;
    float sine;

    if(body == NULL) return false;
    world_delta = (Vec2D){position.x - body->position.x,
        position.y - body->position.y};
    cosine = cosf(-body->rotation);
    sine = sinf(-body->rotation);
    local_delta = (Vec2D){world_delta.x * cosine - world_delta.y * sine,
        world_delta.x * sine + world_delta.y * cosine};
    body->position = position;
    for(size_t i = 0; i < body->node_count; i += 1) {
        body->nodes[i].position.x -= local_delta.x;
        body->nodes[i].position.y -= local_delta.y;
    }
    return true;
}

EditorSoftNode *editor_project_soft_node_add(EditorProject *project, EditorSoftBody *body,
    Position position) {
    EditorSoftNode *node;

    if(project == NULL || body == NULL || body->node_count >= EDITOR_SOFT_NODE_MAX) {
        return NULL;
    }
    node = &body->nodes[body->node_count++];
    *node = (EditorSoftNode){
        .id = project->next_soft_node_id++,
        .position = position,
        .node_mass = 1.0f,
        .radius = 4.0f,
        .friction = 0.0f,
        .restitution = 0.25f,
        .gravity_enabled = false,
        .collision_enabled = true,
        .collision_category = UINT64_C(1),
        .collision_with = UINT64_C(1),
        .color = UINT32_C(0xffaa46ff),
        .visible = true
    };
    snprintf(node->name, sizeof(node->name), "node_%u", node->id);
    return node;
}

bool editor_project_soft_node_remove(EditorProject *project, EditorSoftBody *body,
        EditorSoftNodeId id) {
    if(project == NULL || body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id != id) continue;
        for(size_t j = 0; j < body->beam_count; j += 1) {
            EditorSoftBeam *beam = &body->beams[j];
            if(beam->node_a == id) beam->node_a = 0;
            if(beam->node_b == id) beam->node_b = 0;
        }
        for(size_t j = i + 1; j < body->node_count; j += 1) {
            body->nodes[j - 1] = body->nodes[j];
        }
        body->node_count -= 1;
        body->nodes[body->node_count] = (EditorSoftNode){0};
        editor_project_soft_areas_sync(project, body);
        return true;
    }
    return false;
}

EditorSoftBeam *editor_project_soft_beam_add(EditorProject *project, EditorSoftBody *body,
    EditorSoftNodeId node_a, EditorSoftNodeId node_b) {
    EditorSoftBeam *beam;
    bool found_a = node_a == 0;
    bool found_b = node_b == 0;

    if(project == NULL || body == NULL || (node_a != 0 && node_a == node_b) ||
            body->beam_count >= EDITOR_SOFT_BEAM_MAX) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id == node_a) found_a = true;
        if(body->nodes[i].id == node_b) found_b = true;
    }
    if(!found_a || !found_b) return NULL;
    beam = &body->beams[body->beam_count++];
    *beam = (EditorSoftBeam){
        .id = project->next_soft_beam_id++,
        .node_a = node_a,
        .node_b = node_b,
        .stiffness = 1.0f,
        .damping = 0.0f,
        .color = UINT32_C(0xebf0f5ff),
        .visible = true
    };
    snprintf(beam->name, sizeof(beam->name), "beam_%u", beam->id);
    editor_project_soft_areas_sync(project, body);
    return beam;
}

bool editor_project_soft_beam_remove(EditorProject *project, EditorSoftBody *body,
        EditorSoftBeamId id) {
    if(project == NULL || body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->beam_count; i += 1) {
        if(body->beams[i].id != id) continue;
        for(size_t j = i + 1; j < body->beam_count; j += 1) {
            body->beams[j - 1] = body->beams[j];
        }
        body->beam_count -= 1;
        body->beams[body->beam_count] = (EditorSoftBeam){0};
        editor_project_soft_areas_sync(project, body);
        return true;
    }
    return false;
}

static const EditorSoftNode *editor_soft_node_by_id_get(const EditorSoftBody *body,
        EditorSoftNodeId id) {
    if(body == NULL || id == 0) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1)
        if(body->nodes[i].id == id) return &body->nodes[i];
    return NULL;
}

static bool editor_soft_area_boundary_equal(const EditorSoftArea *area,
        const EditorSoftNodeId *nodes, size_t count) {
    if(area == NULL || nodes == NULL || area->node_count != count) return false;
    for(size_t start = 0; start < count; start += 1) {
        if(area->nodes[start] != nodes[0]) continue;
        for(size_t direction = 0; direction < 2; direction += 1) {
            bool equal = true;
            for(size_t i = 0; i < count; i += 1) {
                size_t index = direction == 0 ? (start + i) % count :
                    (start + count - i) % count;
                if(area->nodes[index] != nodes[i]) equal = false;
            }
            if(equal) return true;
        }
    }
    return false;
}

static float editor_soft_area_signed_twice_get(const EditorSoftBody *body,
        const EditorSoftNodeId *nodes, size_t count) {
    float area = 0.0f;
    for(size_t i = 0; i < count; i += 1) {
        const EditorSoftNode *a = editor_soft_node_by_id_get(body, nodes[i]);
        const EditorSoftNode *b = editor_soft_node_by_id_get(body, nodes[(i + 1) % count]);
        if(a == NULL || b == NULL) return 0.0f;
        area += a->position.x * b->position.y - b->position.x * a->position.y;
    }
    return area;
}

void editor_project_soft_areas_sync(EditorProject *project, EditorSoftBody *body) {
    EditorSoftArea previous[EDITOR_SOFT_AREA_MAX];
    bool visited[EDITOR_SOFT_BEAM_MAX][2] = {{false}};
    size_t previous_count;

    if(project == NULL || body == NULL) return;
    previous_count = body->area_count;
    memcpy(previous, body->areas, sizeof(previous));
    body->area_count = 0;
    for(size_t start_beam = 0; start_beam < body->beam_count; start_beam += 1) {
        for(size_t start_direction = 0; start_direction < 2; start_direction += 1) {
            EditorSoftNodeId nodes[EDITOR_SOFT_AREA_NODE_MAX];
            size_t node_count = 0;
            size_t beam = start_beam;
            size_t direction = start_direction;
            EditorSoftNodeId start = start_direction == 0 ?
                body->beams[start_beam].node_a : body->beams[start_beam].node_b;
            bool closed = false;
            if(visited[start_beam][start_direction] || start == 0) continue;
            nodes[node_count++] = start;
            for(size_t step = 0; step < body->beam_count * 2; step += 1) {
                EditorSoftNodeId from = direction == 0 ?
                    body->beams[beam].node_a : body->beams[beam].node_b;
                EditorSoftNodeId to = direction == 0 ?
                    body->beams[beam].node_b : body->beams[beam].node_a;
                const EditorSoftNode *from_node = editor_soft_node_by_id_get(body, from);
                const EditorSoftNode *to_node = editor_soft_node_by_id_get(body, to);
                float incoming_angle;
                float best_turn = INFINITY;
                size_t next_beam = SIZE_MAX;
                size_t next_direction = 0;
                visited[beam][direction] = true;
                if(from_node == NULL || to_node == NULL || to == 0) break;
                if(to == start && node_count >= 3) {
                    closed = true;
                    break;
                }
                if(node_count >= EDITOR_SOFT_AREA_NODE_MAX) break;
                nodes[node_count++] = to;
                incoming_angle = atan2f(from_node->position.y - to_node->position.y,
                    from_node->position.x - to_node->position.x);
                for(size_t candidate = 0; candidate < body->beam_count; candidate += 1) {
                    EditorSoftNodeId other = 0;
                    size_t candidate_direction = 0;
                    const EditorSoftNode *other_node;
                    float angle;
                    float turn;
                    if(candidate == beam) continue;
                    if(body->beams[candidate].node_a == to) {
                        other = body->beams[candidate].node_b;
                        candidate_direction = 0;
                    } else if(body->beams[candidate].node_b == to) {
                        other = body->beams[candidate].node_a;
                        candidate_direction = 1;
                    } else continue;
                    other_node = editor_soft_node_by_id_get(body, other);
                    if(other_node == NULL) continue;
                    angle = atan2f(other_node->position.y - to_node->position.y,
                        other_node->position.x - to_node->position.x);
                    turn = incoming_angle - angle;
                    while(turn <= 0.0f) turn += 2.0f * PI_F;
                    if(turn < best_turn) {
                        best_turn = turn;
                        next_beam = candidate;
                        next_direction = candidate_direction;
                    }
                }
                if(next_beam == SIZE_MAX) break;
                beam = next_beam;
                direction = next_direction;
            }
            if(closed && node_count >= 3 &&
                    editor_soft_area_signed_twice_get(body, nodes, node_count) > 0.0001f &&
                    body->area_count < EDITOR_SOFT_AREA_MAX) {
                EditorSoftArea area = {0};
                for(size_t i = 0; i < previous_count; i += 1) {
                    if(editor_soft_area_boundary_equal(&previous[i], nodes, node_count)) {
                        area = previous[i];
                        break;
                    }
                }
                if(area.id == 0) {
                    area.id = project->next_soft_area_id++;
                    area.color = body->area_color;
                    area.visible = true;
                    snprintf(area.name, sizeof(area.name), "area_%u", area.id);
                }
                memcpy(area.nodes, nodes, node_count * sizeof(nodes[0]));
                area.node_count = node_count;
                body->areas[body->area_count++] = area;
            }
        }
    }
    for(size_t i = body->area_count; i < EDITOR_SOFT_AREA_MAX; i += 1) {
        body->areas[i] = (EditorSoftArea){0};
    }
}

static float editor_soft_triangle_cross(Position a, Position b, Position c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool editor_soft_point_in_triangle(Position point, Position a, Position b,
        Position c) {
    float ab = editor_soft_triangle_cross(a, b, point);
    float bc = editor_soft_triangle_cross(b, c, point);
    float ca = editor_soft_triangle_cross(c, a, point);
    return ab >= -0.0001f && bc >= -0.0001f && ca >= -0.0001f;
}

size_t editor_project_soft_area_triangulate(const EditorSoftBody *body,
        const EditorSoftArea *area, uint32_t triangles[][3], size_t capacity) {
    uint32_t remaining[EDITOR_SOFT_AREA_NODE_MAX];
    size_t count;
    size_t triangle_count = 0;
    if(body == NULL || area == NULL || triangles == NULL || area->node_count < 3) return 0;
    count = area->node_count;
    for(size_t i = 0; i < count; i += 1) remaining[i] = (uint32_t)i;
    while(count > 3 && triangle_count < capacity) {
        bool clipped = false;
        for(size_t i = 0; i < count; i += 1) {
            uint32_t previous = remaining[(i + count - 1) % count];
            uint32_t current = remaining[i];
            uint32_t next = remaining[(i + 1) % count];
            const EditorSoftNode *a = editor_soft_node_by_id_get(body, area->nodes[previous]);
            const EditorSoftNode *b = editor_soft_node_by_id_get(body, area->nodes[current]);
            const EditorSoftNode *c = editor_soft_node_by_id_get(body, area->nodes[next]);
            bool contains = false;
            if(a == NULL || b == NULL || c == NULL ||
                    editor_soft_triangle_cross(a->position, b->position, c->position) <=
                        0.0001f) continue;
            for(size_t j = 0; j < count; j += 1) {
                uint32_t candidate = remaining[j];
                const EditorSoftNode *node;
                if(candidate == previous || candidate == current || candidate == next) continue;
                node = editor_soft_node_by_id_get(body, area->nodes[candidate]);
                if(node != NULL && editor_soft_point_in_triangle(node->position,
                        a->position, b->position, c->position)) contains = true;
            }
            if(contains) continue;
            triangles[triangle_count][0] = previous;
            triangles[triangle_count][1] = current;
            triangles[triangle_count][2] = next;
            triangle_count += 1;
            for(size_t j = i + 1; j < count; j += 1) remaining[j - 1] = remaining[j];
            count -= 1;
            clipped = true;
            break;
        }
        if(!clipped) return 0;
    }
    if(count == 3 && triangle_count < capacity) {
        triangles[triangle_count][0] = remaining[0];
        triangles[triangle_count][1] = remaining[1];
        triangles[triangle_count][2] = remaining[2];
        triangle_count += 1;
    }
    return triangle_count;
}
