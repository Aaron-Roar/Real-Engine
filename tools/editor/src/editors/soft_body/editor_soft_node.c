/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_soft_node.h"

#include "editors/editor_mode_controls.h"

#include <math.h>
#include <stdio.h>

static EditorSoftBody *body_get(EditorObject *object, EditorSoftBodyId id) {
    if(object == NULL) return NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == id) return &object->soft_body_items[i];
    return NULL;
}

static EditorSoftNode *node_get(EditorSoftBody *body, EditorSoftNodeId id) {
    if(body == NULL) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1)
        if(body->nodes[i].id == id) return &body->nodes[i];
    return NULL;
}

static void float_set(EditorProject *project, EditorObjectId object,
        EditorSoftBodyId body, EditorSoftNodeId node,
        EditorPropertyKind property, float value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {EDITOR_ITEM_SOFT_NODE, object, body, node, 0,
            property, EDITOR_PROPERTY_VALUE_FLOAT, {.number = value}}};
    (void)editor_command_execute(project, &command);
}

static void bool_set(EditorProject *project, EditorObjectId object,
        EditorSoftBodyId body, EditorSoftNodeId node,
        EditorPropertyKind property, bool value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {EDITOR_ITEM_SOFT_NODE, object, body, node, 0,
            property, EDITOR_PROPERTY_VALUE_BOOL, {.boolean = value}}};
    (void)editor_command_execute(project, &command);
}

static bool checkbox(const char *id, const TextAsset *label,
        UIRect bounds, bool *checked) {
    UIButtonResult result = rohr_ui_interaction(id, bounds);
    UIRect box = {bounds.x + 4.0f, bounds.y + 4.0f,
        bounds.height - 8.0f, bounds.height - 8.0f};
    Color background = result.pressed ? (Color){58, 65, 78, 255} :
        result.hovered || result.focused ? (Color){67, 75, 90, 255} :
        (Color){48, 54, 66, 255};
    if(result.clicked) *checked = !*checked;
    rohr_ui_surface(bounds, background);
    rohr_ui_surface(box, (Color){22, 25, 31, 255});
    rohr_ui_border(box, 2.0f, (Color){8, 9, 12, 255});
    if(*checked) rohr_ui_surface((UIRect){box.x + 5.0f, box.y + 5.0f,
        box.width - 10.0f, box.height - 10.0f}, (Color){225, 230, 240, 255});
    rohr_ui_label(label, (UIRect){box.x + box.width + 8.0f, bounds.y,
        bounds.width - box.width - 12.0f, bounds.height});
    return result.clicked;
}

bool editor_soft_node_editor_create(EditorSoftNodeEditor *editor,
        FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorSoftNodeEditor){.font = font};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Name", name_label); CREATE("X", x_label); CREATE("Y", y_label);
    CREATE("Mass", mass_label); CREATE("Radius", radius_label);
    CREATE("Friction", friction_label); CREATE("Restitution", restitution_label);
    CREATE("Gravity", gravity_label); CREATE("Collision", collision_label);
    CREATE("Collision Category", collision_category_label);
    CREATE("Collide With", collide_with_label); CREATE("Node Color", color_label);
    CREATE("Inherit", inherit_label); CREATE("[X]", visible_label);
    CREATE("[ ]", hidden_label); CREATE("Delete Node", delete_label);
    CREATE("", x_field); CREATE("", y_field); CREATE("", mass_field);
    CREATE("", radius_field); CREATE("", friction_field);
    CREATE("", restitution_field);
#undef CREATE
    for(size_t i = 0; i < EDITOR_SOFT_NODE_MAX; i += 1) {
        char name[32]; snprintf(name, sizeof(name), "node_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->node_names[i])) goto fail;
    }
    return true;
fail:
    editor_soft_node_editor_destroy(editor);
    return false;
}

void editor_soft_node_editor_destroy(EditorSoftNodeEditor *editor) {
    if(editor == NULL) return;
#define DESTROY(member) rohr_graphics_text_destroy(&editor->member)
    DESTROY(name_label); DESTROY(x_label); DESTROY(y_label); DESTROY(mass_label);
    DESTROY(radius_label); DESTROY(friction_label); DESTROY(restitution_label);
    DESTROY(gravity_label); DESTROY(collision_label);
    DESTROY(collision_category_label); DESTROY(collide_with_label);
    DESTROY(color_label); DESTROY(inherit_label); DESTROY(visible_label);
    DESTROY(hidden_label); DESTROY(delete_label); DESTROY(x_field);
    DESTROY(y_field); DESTROY(mass_field); DESTROY(radius_field);
    DESTROY(friction_field); DESTROY(restitution_field);
#undef DESTROY
    for(size_t i = 0; i < EDITOR_SOFT_NODE_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->node_names[i]);
    *editor = (EditorSoftNodeEditor){0};
}

bool editor_soft_node_editor_draw(EditorSoftNodeEditor *editor,
        const EditorModeContext *context,
        EditorSoftNodeCollisionMenuFunction collision_menu,
        void *collision_context) {
    EditorObject *object;
    EditorSoftBody *body;
    EditorSoftNode *node;
    size_t node_index;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    float mass_value, radius_value, friction_value, restitution_value;
    UIFieldResult name_result, x_result, y_result, mass_result;
    UIFieldResult radius_result, friction_result, restitution_result;
    bool field_active;
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    body = body_get(object, context->viewport->selected_soft_body);
    node = node_get(body, context->viewport->selected_soft_node);
    if(node == NULL) return false;
    node_index = (size_t)(node - body->nodes);
    if(node_index >= EDITOR_SOFT_NODE_MAX) return false;
    snprintf(name, sizeof(name), "%s", node->name);
    if(!editor_mode_named_text_sync(editor->font, node->name,
            &editor->node_names[node_index], editor->node_cache[node_index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 40.0f, 40.0f, 48.0f, 30.0f});
    name_result = editor_mode_name_field("editor.soft_node.name", name,
        sizeof(name), &editor->node_names[node_index],
        (UIRect){context->x + 88.0f, 40.0f,
            context->width - 96.0f, 30.0f});
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
            .data.item_rename = {.kind = EDITOR_ITEM_SOFT_NODE,
                .object = object->id, .parent = body->id, .item = node->id}};
        snprintf(command.data.item_rename.name,
            sizeof(command.data.item_rename.name), "%s", name);
        (void)editor_command_execute(context->project, &command);
    }
    if(rohr_ui_button("editor.soft_node.visibility", node->visible ?
            &editor->visible_label : &editor->hidden_label,
            (UIRect){context->x + 8.0f, 44.0f, 26.0f, 26.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
            .data.visibility = {EDITOR_VISIBILITY_SOFT_NODE, object->id,
                body->id, node->id, !node->visible}};
        (void)editor_command_execute(context->project, &command);
    }
    position = node->position;
    rohr_ui_label(&editor->x_label,
        (UIRect){context->x + 8.0f, 122.0f, 50.0f, 26.0f});
    x_result = rohr_ui_field("editor.soft_node.x",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.x},
        &editor->x_field, (UIRect){context->x + 60.0f, 122.0f,
            context->width - 70.0f, 26.0f}, NULL);
    rohr_ui_label(&editor->y_label,
        (UIRect){context->x + 8.0f, 158.0f, 50.0f, 26.0f});
    y_result = rohr_ui_field("editor.soft_node.y",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.y},
        &editor->y_field, (UIRect){context->x + 60.0f, 158.0f,
            context->width - 70.0f, 26.0f}, NULL);
    if(x_result.changed || y_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SOFT_NODE_POSITION,
            .data.soft_node_position = {object->id, body->id, node->id, position}};
        (void)editor_command_execute(context->project, &command);
    }
    mass_value = node->node_mass;
    rohr_ui_label(&editor->mass_label,
        (UIRect){context->x + 8.0f, 194.0f, 68.0f, 26.0f});
    mass_result = rohr_ui_field("editor.soft_node.mass",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &mass_value},
        &editor->mass_field, (UIRect){context->x + 78.0f, 194.0f,
            context->width - 88.0f, 26.0f}, NULL);
    if(mass_result.changed) float_set(context->project, object->id, body->id,
        node->id, EDITOR_PROPERTY_MASS, fmaxf(0.0f, mass_value));
    radius_value = node->radius;
    rohr_ui_label(&editor->radius_label,
        (UIRect){context->x + 8.0f, 230.0f, 68.0f, 26.0f});
    radius_result = rohr_ui_field("editor.soft_node.radius",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &radius_value},
        &editor->radius_field, (UIRect){context->x + 78.0f, 230.0f,
            context->width - 88.0f, 26.0f}, NULL);
    if(radius_result.changed)
        node->radius = radius_value <= 0.0f ? 0.1f : radius_value;
    friction_value = node->friction;
    rohr_ui_label(&editor->friction_label,
        (UIRect){context->x + 8.0f, 266.0f, 68.0f, 26.0f});
    friction_result = rohr_ui_field("editor.soft_node.friction",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &friction_value},
        &editor->friction_field, (UIRect){context->x + 78.0f, 266.0f,
            context->width - 88.0f, 26.0f}, NULL);
    if(friction_result.changed) float_set(context->project, object->id, body->id,
        node->id, EDITOR_PROPERTY_FRICTION, fmaxf(0.0f, friction_value));
    restitution_value = node->restitution;
    rohr_ui_label(&editor->restitution_label,
        (UIRect){context->x + 8.0f, 302.0f, 96.0f, 26.0f});
    restitution_result = rohr_ui_field("editor.soft_node.restitution",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &restitution_value},
        &editor->restitution_field, (UIRect){context->x + 106.0f, 302.0f,
            context->width - 116.0f, 26.0f}, NULL);
    if(restitution_result.changed) float_set(context->project, object->id, body->id,
        node->id, EDITOR_PROPERTY_RESTITUTION,
        fminf(1.0f, fmaxf(0.0f, restitution_value)));
    {
        bool gravity = node->gravity_enabled;
        if(checkbox("editor.soft_node.gravity", &editor->gravity_label,
                (UIRect){context->x + 10.0f, 338.0f,
                    context->width - 20.0f, 28.0f}, &gravity))
            bool_set(context->project, object->id, body->id, node->id,
                EDITOR_PROPERTY_GRAVITY, gravity);
    }
    field_active = name_result.active || x_result.active || y_result.active ||
        mass_result.active || radius_result.active || friction_result.active ||
        restitution_result.active;
    {
        float row_x = context->x + 10.0f, row_width = context->width - 20.0f;
        float bottom = 406.0f;
        bool collision = node->collision_enabled;
        if(checkbox("editor.soft_node.collision", &editor->collision_label,
                (UIRect){row_x, 374.0f, row_width, 28.0f}, &collision)) {
            bool_set(context->project, object->id, body->id, node->id,
                EDITOR_PROPERTY_COLLISION, collision);
            if(!collision) editor->collision_category_open =
                editor->collide_with_open = false;
        }
        if(node->collision_enabled) {
            if(rohr_ui_button("editor.soft_node.collision_category",
                    &editor->collision_category_label,
                    (UIRect){row_x, bottom, row_width, 28.0f}, NULL).clicked) {
                editor->collision_category_open = !editor->collision_category_open;
                editor->collide_with_open = false;
            }
            rohr_ui_border((UIRect){row_x, bottom, row_width, 28.0f},
                2.0f, (Color){0, 0, 0, 255});
            bottom += 32.0f;
            if(editor->collision_category_open && collision_menu != NULL) {
                size_t rows = 0;
                if(!collision_menu(collision_context,
                        "editor.soft_node.collision_category.mask", context->project,
                        &node->collision_category, object->id, body->id, node->id,
                        EDITOR_COLLISION_FILTER_CATEGORY, row_x, bottom, row_width,
                        &field_active, &rows)) return field_active;
                bottom += (float)rows * 30.0f;
            }
            if(rohr_ui_button("editor.soft_node.collide_with",
                    &editor->collide_with_label,
                    (UIRect){row_x, bottom, row_width, 28.0f}, NULL).clicked) {
                editor->collide_with_open = !editor->collide_with_open;
                editor->collision_category_open = false;
            }
            rohr_ui_border((UIRect){row_x, bottom, row_width, 28.0f},
                2.0f, (Color){0, 0, 0, 255});
            bottom += 32.0f;
            if(editor->collide_with_open && collision_menu != NULL) {
                size_t rows = 0;
                if(!collision_menu(collision_context,
                        "editor.soft_node.collide_with.mask", context->project,
                        &node->collision_with, object->id, body->id, node->id,
                        EDITOR_COLLISION_FILTER_COLLIDE_WITH, row_x, bottom, row_width,
                        &field_active, &rows)) return field_active;
                bottom += (float)rows * 30.0f;
            }
            if((editor->collision_category_open || editor->collide_with_open) &&
                    context->primary_button == MOUSE_BUTTON_STATE_PRESSED) {
                Position pointer = rohr_graphics_mouse_screen_position_get();
                if(pointer.x < row_x || pointer.x > row_x + row_width ||
                        pointer.y < 374.0f || pointer.y > bottom)
                    editor->collision_category_open = editor->collide_with_open = false;
            }
        }
    }
    {
        bool inherit = !node->color_overridden;
        float field_width = fmaxf(34.0f, context->width - 196.0f);
        if(inherit) node->color = body->node_color;
        rohr_ui_label(&editor->color_label,
            (UIRect){context->x + 8.0f, 620.0f, 90.0f, 26.0f});
        if(editor_mode_checkbox_left("editor.soft_node.color_inherit",
                &editor->inherit_label,
                (UIRect){context->x + context->width - 92.0f,
                    620.0f, 82.0f, 26.0f}, &inherit)) {
            node->color_overridden = !inherit;
            node->color = body->node_color;
        }
        (void)editor_mode_color_swatch("editor.soft_node.color", &node->color,
            inherit, (UIRect){context->x + 100.0f, 620.0f,
                field_width, 26.0f}, context, EDITOR_ITEM_SOFT_NODE,
            object->id, body->id, node->id, EDITOR_PROPERTY_COLOR);
    }
    if(context->delete_y_get != NULL && context->delete_open_item != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        if(rohr_ui_button("editor.soft_node.delete", &editor->delete_label,
                (UIRect){context->x + 10.0f,
                    context->delete_y_get(context->delete_context),
                    context->width - 20.0f, 34.0f}, &style).clicked)
            (void)context->delete_open_item(context->delete_context);
    }
    return field_active;
}
