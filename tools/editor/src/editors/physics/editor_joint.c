#include "editor_joint.h"

#include "editor_navigation.h"
#include "editors/editor_mode_controls.h"

#include <stdio.h>

static EditorJoint *joint_get(EditorObject *object, EditorJointId id) {
    if(object == NULL) return NULL;
    for(size_t i = 0; i < object->joint_count; i += 1)
        if(object->joint_items[i].id == id) return &object->joint_items[i];
    return NULL;
}

static void property_float_set(EditorProject *project, EditorObjectId object,
        EditorJointId joint, EditorPropertyKind property, float value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {EDITOR_ITEM_JOINT, object, 0, joint, 0,
            property, EDITOR_PROPERTY_VALUE_FLOAT, {.number = value}}};
    (void)editor_command_execute(project, &command);
}

static void property_uint_set(EditorProject *project, EditorObjectId object,
        EditorJointId joint, EditorPropertyKind property, uint32_t value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {EDITOR_ITEM_JOINT, object, 0, joint, 0,
            property, EDITOR_PROPERTY_VALUE_UINT, {.integer = value}}};
    (void)editor_command_execute(project, &command);
}

static void relationship_set(EditorProject *project, EditorObjectId object,
        EditorJointId joint, uint32_t endpoint, EditorAnchorId anchor) {
    EditorCommand command = {.type = EDITOR_COMMAND_RELATIONSHIP_SET,
        .data.relationship_set = {EDITOR_RELATIONSHIP_JOINT_ANCHOR,
            object, 0, joint, endpoint, anchor}};
    (void)editor_command_execute(project, &command);
}

static void anchor_preview_set(EditorViewportState *viewport,
        EditorObject *object, EditorAnchorId id) {
    EditorAnchor *anchor;
    if(viewport == NULL || object == NULL || id == 0) return;
    anchor = editor_project_anchor_get(object, id);
    if(anchor == NULL) return;
    viewport->preview_anchor = anchor->id;
    viewport->preview_rigid_body = anchor->rigid_body;
}

static UIButtonStyle selected_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    style.idle = (Color){118, 96, 35, 255};
    style.hovered = (Color){145, 119, 45, 255};
    return style;
}

bool editor_joint_editor_create(EditorJointEditor *editor, FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorJointEditor){.font = font};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Name", name_label); CREATE("Visual Size", visual_size_label);
    CREATE("Revolute", revolute_label); CREATE("Weld", weld_label);
    CREATE("Spring", spring_label); CREATE("Anchor A", anchor_a_label);
    CREATE("Anchor B", anchor_b_label); CREATE("None", none_label);
    CREATE("Add Anchor", add_anchor_label); CREATE("Damping", damping_label);
    CREATE("Rest Length", rest_length_label); CREATE("Stiffness", stiffness_label);
    CREATE("[X]", visible_label); CREATE("[ ]", hidden_label);
    CREATE("Delete Joint", delete_label); CREATE("", damping_field);
    CREATE("", rest_length_field); CREATE("", stiffness_field);
#undef CREATE
    for(size_t i = 0; i < EDITOR_JOINT_MAX; i += 1) {
        char name[32]; snprintf(name, sizeof(name), "joint_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->joint_names[i])) goto fail;
    }
    for(size_t i = 0; i < EDITOR_ANCHOR_MAX; i += 1) {
        char name[32]; snprintf(name, sizeof(name), "anchor_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->anchor_names[i])) goto fail;
    }
    return true;
fail:
    editor_joint_editor_destroy(editor);
    return false;
}

void editor_joint_editor_destroy(EditorJointEditor *editor) {
    if(editor == NULL) return;
#define DESTROY(member) rohr_graphics_text_destroy(&editor->member)
    DESTROY(name_label); DESTROY(visual_size_label); DESTROY(revolute_label);
    DESTROY(weld_label); DESTROY(spring_label); DESTROY(anchor_a_label);
    DESTROY(anchor_b_label); DESTROY(none_label); DESTROY(add_anchor_label);
    DESTROY(damping_label); DESTROY(rest_length_label); DESTROY(stiffness_label);
    DESTROY(visible_label); DESTROY(hidden_label); DESTROY(delete_label);
    DESTROY(damping_field); DESTROY(rest_length_field); DESTROY(stiffness_field);
#undef DESTROY
    for(size_t i = 0; i < EDITOR_JOINT_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->joint_names[i]);
    for(size_t i = 0; i < EDITOR_ANCHOR_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->anchor_names[i]);
    *editor = (EditorJointEditor){0};
}

static bool nonnegative_field(EditorJointEditor *editor,
        const EditorModeContext *context, EditorObjectId object,
        EditorJointId joint, const char *id, const TextAsset *label,
        TextAsset *field, float y, float label_width,
        EditorPropertyKind property, float source) {
    float value = source;
    UIFieldResult result;
    rohr_ui_label(label,
        (UIRect){context->x + 8.0f, y, label_width, 26.0f});
    result = rohr_ui_field(id,
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &value},
        field,
        (UIRect){context->x + label_width + 10.0f, y,
            context->width - label_width - 20.0f, 26.0f}, NULL);
    if(result.changed) property_float_set(context->project, object, joint,
        property, value < 0.0f ? 0.0f : value);
    return result.active;
}

bool editor_joint_editor_draw(EditorJointEditor *editor,
        const EditorModeContext *context) {
    EditorObject *object;
    EditorJoint *joint;
    size_t joint_index;
    char name[EDITOR_OBJECT_NAME_MAX];
    UIFieldResult name_result;
    bool field_active;
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    joint = joint_get(object, context->viewport->selected_joint);
    if(joint == NULL) return false;
    joint_index = (size_t)(joint - object->joint_items);
    if(joint_index >= EDITOR_JOINT_MAX) return false;
    snprintf(name, sizeof(name), "%s", joint->name);
    if(!editor_mode_named_text_sync(editor->font, joint->name,
            &editor->joint_names[joint_index], editor->joint_cache[joint_index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 40.0f, 40.0f, 48.0f, 30.0f});
    name_result = editor_mode_name_field("editor.joint.name", name,
        sizeof(name), &editor->joint_names[joint_index],
        (UIRect){context->x + 88.0f, 40.0f,
            context->width - 96.0f, 30.0f});
    field_active = name_result.active;
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
            .data.item_rename = {.kind = EDITOR_ITEM_JOINT,
                .object = object->id, .item = joint->id}};
        snprintf(command.data.item_rename.name,
            sizeof(command.data.item_rename.name), "%s", name);
        (void)editor_command_execute(context->project, &command);
    }
    rohr_ui_label(&editor->visual_size_label,
        (UIRect){context->x + 40.0f, 76.0f, context->width - 48.0f, 24.0f});
    if(rohr_ui_button("editor.joint.visibility", joint->visible ?
            &editor->visible_label : &editor->hidden_label,
            (UIRect){context->x + 10.0f, 94.0f, 24.0f, 24.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
            .data.visibility = {EDITOR_VISIBILITY_JOINT, object->id, 0,
                joint->id, !joint->visible}};
        (void)editor_command_execute(context->project, &command);
    }
    {
        UISliderConfig slider = rohr_ui_slider_config_default_get();
        slider.center = (Position){context->x +
            (context->width + 40.0f) * 0.5f, 106.0f};
        slider.length = context->width - 60.0f;
        slider.min_value = 0.25f; slider.max_value = 3.0f;
        UISliderResult result = rohr_ui_slider("editor.joint.visual_size",
            joint->visual_size, &slider);
        if(result.changed) property_float_set(context->project, object->id,
            joint->id, EDITOR_PROPERTY_VISUAL_SIZE, result.value);
    }
    {
        const TextAsset *options[] = {&editor->revolute_label,
            &editor->weld_label, &editor->spring_label};
        UIDropdownResult result = rohr_ui_dropdown("editor.joint.kind", options,
            3, (size_t)joint->kind, (UIRect){context->x + 10.0f, 118.0f,
                context->width - 20.0f, 30.0f}, NULL);
        if(result.changed) property_uint_set(context->project, object->id,
            joint->id, EDITOR_PROPERTY_JOINT_KIND,
            (uint32_t)result.selected_index);
    }
    {
        const TextAsset *options[EDITOR_ANCHOR_MAX + 1];
        size_t selected_a = 0, selected_b = 0;
        options[0] = &editor->none_label;
        for(size_t i = 0; i < object->anchor_count && i < EDITOR_ANCHOR_MAX; i += 1) {
            if(!editor_mode_named_text_sync(editor->font, object->anchors[i].name,
                    &editor->anchor_names[i], editor->anchor_cache[i],
                    EDITOR_OBJECT_NAME_MAX)) return field_active;
            options[i + 1] = &editor->anchor_names[i];
            if(object->anchors[i].id == joint->anchor_a) selected_a = i + 1;
            if(object->anchors[i].id == joint->anchor_b) selected_b = i + 1;
        }
        rohr_ui_label(&editor->anchor_a_label,
            (UIRect){context->x + 8.0f, 158.0f, 55.0f, 28.0f});
        rohr_ui_label(&editor->anchor_b_label,
            (UIRect){context->x + 8.0f, 194.0f, 55.0f, 28.0f});
        UIDropdownResult a = rohr_ui_dropdown("editor.joint.anchor_a", options,
            object->anchor_count + 1, selected_a,
            (UIRect){context->x + 63.0f, 158.0f,
                context->width - 73.0f, 28.0f}, NULL);
        UIDropdownResult b = rohr_ui_dropdown("editor.joint.anchor_b", options,
            object->anchor_count + 1, selected_b,
            (UIRect){context->x + 63.0f, 194.0f,
                context->width - 73.0f, 28.0f}, NULL);
        if(a.button_hovered || a.hovered_index >= 0)
            anchor_preview_set(context->viewport, object, a.hovered_index > 0 ?
                object->anchors[a.hovered_index - 1].id : joint->anchor_a);
        if(b.button_hovered || b.hovered_index >= 0)
            anchor_preview_set(context->viewport, object, b.hovered_index > 0 ?
                object->anchors[b.hovered_index - 1].id : joint->anchor_b);
        if(a.changed) relationship_set(context->project, object->id, joint->id, 0,
            a.selected_index == 0 ? 0 : object->anchors[a.selected_index - 1].id);
        if(b.changed) relationship_set(context->project, object->id, joint->id, 1,
            b.selected_index == 0 ? 0 : object->anchors[b.selected_index - 1].id);
    }
    if(rohr_ui_button("editor.joint.add_anchor", &editor->add_anchor_label,
            (UIRect){context->x + 10.0f, 232.0f,
                context->width - 20.0f, 30.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
            .data.item_add = {.kind = EDITOR_ITEM_ANCHOR, .object = object->id,
                .parent = object->rigid_body_count > 0 ?
                    object->rigid_bodies[0].id : 0}};
        EditorCommandResult added = editor_command_execute(context->project, &command);
        if(added.kind == ERROR_RESULT_VALUE)
            context->viewport->selected_anchor = added.result.object;
    }
    {
        size_t start = 0;
        for(size_t i = 0; i < object->anchor_count; i += 1)
            if(object->anchors[i].id == context->viewport->selected_anchor && i >= 6)
                start = i - 5;
        for(size_t i = start; i < object->anchor_count && i < start + 6 &&
                i < EDITOR_ANCHOR_MAX; i += 1) {
            EditorAnchor *anchor = &object->anchors[i];
            UIButtonStyle style = selected_style_get();
            char id[64], visibility_id[72];
            float y = 270.0f + (float)(i - start) * 27.0f;
            if(!editor_mode_named_text_sync(editor->font, anchor->name,
                    &editor->anchor_names[i], editor->anchor_cache[i],
                    EDITOR_OBJECT_NAME_MAX)) return field_active;
            snprintf(id, sizeof(id), "editor.anchor.%u", anchor->id);
            snprintf(visibility_id, sizeof(visibility_id),
                "editor.anchor.%u.visibility", anchor->id);
            if(rohr_ui_button(visibility_id, anchor->visible ? &editor->visible_label :
                    &editor->hidden_label,
                    (UIRect){context->x + 10.0f, y, 23.0f, 23.0f}, NULL).clicked) {
                EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
                    .data.visibility = {EDITOR_VISIBILITY_ANCHOR, object->id,
                        0, anchor->id, !anchor->visible}};
                (void)editor_command_execute(context->project, &command);
            }
            UIButtonResult result = rohr_ui_button(id, &editor->anchor_names[i],
                (UIRect){context->x + 40.0f, y, context->width - 48.0f, 23.0f},
                (context->viewport->selected_anchor == anchor->id ||
                    editor_viewport_selection_contains(context->viewport,
                        (EditorSelectionRef){EDITOR_SELECTION_ANCHOR,
                            object->id, 0, 0, anchor->id})) ? &style : NULL);
            if(context->hierarchy_row != NULL)
                context->hierarchy_row(context->hierarchy_context, context->viewport,
                    (EditorSelectionRef){EDITOR_SELECTION_ANCHOR,
                        object->id, 0, 0, anchor->id},
                    (UIRect){context->x + 40.0f, y,
                        context->width - 48.0f, 23.0f}, result,
                    i + 1 == object->anchor_count);
            if(result.clicked || result.focus_changed) {
                context->viewport->selection = EDITOR_SELECTION_ANCHOR;
                context->viewport->selected_anchor = anchor->id;
                if(result.double_clicked)
                    (void)editor_navigation_selected_open(context->project,
                        context->viewport);
            }
        }
    }
    if(joint->kind == EDITOR_JOINT_REVOLUTE)
        field_active = nonnegative_field(editor, context, object->id, joint->id,
            "editor.joint.revolute.damping", &editor->damping_label,
            &editor->damping_field, 442.0f, 76.0f, EDITOR_PROPERTY_DAMPING,
            joint->damping) || field_active;
    else if(joint->kind == EDITOR_JOINT_SPRING) {
        field_active = nonnegative_field(editor, context, object->id, joint->id,
            "editor.joint.spring.rest_length", &editor->rest_length_label,
            &editor->rest_length_field, 442.0f, 96.0f,
            EDITOR_PROPERTY_REST_LENGTH,
            joint->rest_length) || field_active;
        field_active = nonnegative_field(editor, context, object->id, joint->id,
            "editor.joint.spring.stiffness", &editor->stiffness_label,
            &editor->stiffness_field, 474.0f, 90.0f,
            EDITOR_PROPERTY_STIFFNESS,
            joint->stiffness) || field_active;
        field_active = nonnegative_field(editor, context, object->id, joint->id,
            "editor.joint.spring.damping", &editor->damping_label,
            &editor->damping_field, 506.0f, 76.0f, EDITOR_PROPERTY_DAMPING,
            joint->damping) || field_active;
    }
    if(context->delete_y_get != NULL && context->delete_open_item != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        if(rohr_ui_button("editor.joint.delete", &editor->delete_label,
                (UIRect){context->x + 10.0f,
                    context->delete_y_get(context->delete_context),
                    context->width - 20.0f, 34.0f}, &style).clicked)
            (void)context->delete_open_item(context->delete_context);
    }
    return field_active;
}
