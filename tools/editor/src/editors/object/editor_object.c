#include "editor_object.h"

#include "editor_navigation.h"
#include "editors/editor_mode_controls.h"

#include <stdio.h>

static UIButtonStyle selected_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    style.idle = (Color){118, 96, 35, 255};
    style.hovered = (Color){145, 119, 45, 255};
    return style;
}

bool editor_object_editor_create(EditorObjectEditor *editor, FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorObjectEditor){.font = font};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Object Name", object_name_label); CREATE("Add Rigid Body", add_rigid_body_label);
    CREATE("Add Joint", add_joint_label); CREATE("Add Soft Body", add_soft_body_label);
    CREATE("Add Sprite", add_sprite_label); CREATE("Add Animation", add_animation_label);
    CREATE("[X]", visible_label); CREATE("[ ]", hidden_label);
    CREATE("Delete Object", delete_label);
#undef CREATE
    return true;
fail:
    editor_object_editor_destroy(editor);
    return false;
}

void editor_object_editor_destroy(EditorObjectEditor *editor) {
    if(editor == NULL) return;
#define DESTROY(member) rohr_graphics_text_destroy(&editor->member)
    DESTROY(object_name_label); DESTROY(add_rigid_body_label); DESTROY(add_joint_label);
    DESTROY(add_soft_body_label); DESTROY(add_sprite_label); DESTROY(add_animation_label);
    DESTROY(visible_label); DESTROY(hidden_label); DESTROY(delete_label);
#undef DESTROY
#define DESTROY_ARRAY(array, count) \
    for(size_t i = 0; i < (count); i += 1) rohr_graphics_text_destroy(&(array)[i])
    DESTROY_ARRAY(editor->object_names, EDITOR_OBJECT_MAX);
    DESTROY_ARRAY(editor->rigid_body_names, EDITOR_RIGID_BODY_MAX);
    DESTROY_ARRAY(editor->joint_names, EDITOR_JOINT_MAX);
    DESTROY_ARRAY(editor->soft_body_names, EDITOR_SOFT_BODY_MAX);
    DESTROY_ARRAY(editor->sprite_names, 64);
    DESTROY_ARRAY(editor->animation_names, 32);
#undef DESTROY_ARRAY
    *editor = (EditorObjectEditor){0};
}

static void visibility_toggle(EditorProject *project, EditorVisibilityKind kind,
        EditorObjectId object, uint32_t item, bool visible) {
    EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
        .data.visibility = {kind, object, 0, item, !visible}};
    (void)editor_command_execute(project, &command);
}

static bool item_info_get(EditorObjectEditor *editor, EditorObject *object,
        EditorHierarchyItem item, const char **name, TextAsset **label,
        char **cache, bool *visible, EditorHierarchySelection *selection,
        EditorVisibilityKind *visibility) {
    if(item.kind == EDITOR_HIERARCHY_RIGID_BODY) {
        for(size_t i = 0; i < object->rigid_body_count; i += 1)
            if(object->rigid_bodies[i].id == item.id) {
                *name = object->rigid_bodies[i].name; *visible = object->rigid_bodies[i].visible;
                *label = &editor->rigid_body_names[i]; *cache = editor->rigid_body_cache[i];
            }
        *selection = EDITOR_SELECTION_RIGID_BODY; *visibility = EDITOR_VISIBILITY_RIGID_BODY;
    } else if(item.kind == EDITOR_HIERARCHY_JOINT) {
        for(size_t i = 0; i < object->joint_count; i += 1)
            if(object->joint_items[i].id == item.id) {
                *name = object->joint_items[i].name; *visible = object->joint_items[i].visible;
                *label = &editor->joint_names[i]; *cache = editor->joint_cache[i];
            }
        *selection = EDITOR_SELECTION_JOINT; *visibility = EDITOR_VISIBILITY_JOINT;
    } else if(item.kind == EDITOR_HIERARCHY_SOFT_BODY) {
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == item.id) {
                *name = object->soft_body_items[i].name; *visible = object->soft_body_items[i].visible;
                *label = &editor->soft_body_names[i]; *cache = editor->soft_body_cache[i];
            }
        *selection = EDITOR_SELECTION_SOFT_BODY; *visibility = EDITOR_VISIBILITY_SOFT_BODY;
    } else if(item.kind == EDITOR_HIERARCHY_SPRITE) {
        for(size_t i = 0; i < object->sprite_count && i < 64; i += 1)
            if(object->sprites[i].id == item.id) {
                *name = object->sprites[i].name; *visible = object->sprites[i].visible;
                *label = &editor->sprite_names[i]; *cache = editor->sprite_cache[i];
            }
        *selection = EDITOR_SELECTION_SPRITE; *visibility = EDITOR_VISIBILITY_OBJECT;
    } else {
        for(size_t i = 0; i < object->animated_sprite_count && i < 32; i += 1)
            if(object->animated_sprite_items[i].id == item.id) {
                *name = object->animated_sprite_items[i].name;
                *visible = object->animated_sprite_items[i].visible;
                *label = &editor->animation_names[i]; *cache = editor->animation_cache[i];
            }
        *selection = EDITOR_SELECTION_ANIMATED_SPRITE;
        *visibility = EDITOR_VISIBILITY_OBJECT;
    }
    return *name != NULL && *label != NULL && *cache != NULL;
}

static void item_visibility_toggle(EditorObjectEditor *editor,
        EditorProject *project, EditorObject *object, EditorHierarchyItem item,
        EditorHierarchySelection selection, EditorVisibilityKind visibility,
        bool visible, const char *id, UIRect bounds) {
    if(!rohr_ui_button(id, visible ? &editor->visible_label :
            &editor->hidden_label, bounds, NULL).clicked) return;
    if(selection == EDITOR_SELECTION_SPRITE) {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_VISIBILITY_SET,
            .data.sprite_visibility_set = {object->id, item.id, !visible}};
        (void)editor_command_execute(project, &command);
    } else if(selection == EDITOR_SELECTION_ANIMATED_SPRITE) {
        EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET,
            .data.animated_sprite_boolean_set = {object->id, item.id, !visible}};
        (void)editor_command_execute(project, &command);
    } else visibility_toggle(project, visibility, object->id, item.id, visible);
}

bool editor_object_editor_draw(EditorObjectEditor *editor,
        const EditorModeContext *context, EditorSpriteBrowserOpenFunction browser_open,
        void *browser_context, bool additive_selection) {
    EditorObject *object;
    size_t object_index;
    char name[EDITOR_OBJECT_NAME_MAX];
    UIFieldResult name_result;
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    if(object == NULL) return false;
    object_index = (size_t)(object - context->project->objects);
    if(object_index >= EDITOR_OBJECT_MAX) return false;
    if(!editor_mode_named_text_sync(editor->font, object->name,
            &editor->object_names[object_index], editor->object_cache[object_index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    if(rohr_ui_button("editor.object.visibility", object->visible ?
            &editor->visible_label : &editor->hidden_label,
            (UIRect){context->x + 8.0f, 56.0f, 26.0f, 26.0f}, NULL).clicked)
        visibility_toggle(context->project, EDITOR_VISIBILITY_OBJECT,
            object->id, 0, object->visible);
    rohr_ui_label(&editor->object_name_label,
        (UIRect){context->x + 40.0f, 52.0f, 90.0f, 34.0f});
    snprintf(name, sizeof(name), "%s", object->name);
    name_result = rohr_ui_field("editor.object.name",
        (UIFieldBinding){.kind = UI_FIELD_STRING, .string = name,
            .string_capacity = sizeof(name)}, &editor->object_names[object_index],
        (UIRect){context->x + 130.0f, 52.0f,
            context->width - 140.0f, 34.0f}, NULL);
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
            .data.item_rename = {.kind = EDITOR_ITEM_OBJECT, .object = object->id}};
        snprintf(command.data.item_rename.name,
            sizeof(command.data.item_rename.name), "%s", name);
        (void)editor_command_execute(context->project, &command);
    }
#define ADD_BUTTON(button_id, button_label, button_y, item_kind, item_option, \
        selection_value, member) \
    if(rohr_ui_button((button_id), &(button_label), \
            (UIRect){context->x + 10.0f, (button_y), \
            context->width - 20.0f, 32.0f}, NULL).clicked) { \
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD, \
            .data.item_add = {.kind = (item_kind), .object = object->id, \
                .option = (item_option)}}; \
        EditorCommandResult result = editor_command_execute(context->project, &command); \
        if(result.kind == ERROR_RESULT_VALUE) { \
            context->viewport->selection = (selection_value); \
            context->viewport->member = result.result.object; \
        } \
    }
    ADD_BUTTON("editor.add_rigid_body", editor->add_rigid_body_label, 128.0f,
        EDITOR_ITEM_RIGID_BODY, 0, EDITOR_SELECTION_RIGID_BODY, selected_rigid_body);
    ADD_BUTTON("editor.add_joint", editor->add_joint_label, 166.0f,
        EDITOR_ITEM_JOINT, EDITOR_JOINT_SPRING, EDITOR_SELECTION_JOINT, selected_joint);
    ADD_BUTTON("editor.add_soft_body", editor->add_soft_body_label, 204.0f,
        EDITOR_ITEM_SOFT_BODY, 0, EDITOR_SELECTION_SOFT_BODY, selected_soft_body);
#undef ADD_BUTTON
    if(rohr_ui_button("editor.add_sprite", &editor->add_sprite_label,
            (UIRect){context->x + 10.0f, 242.0f,
                context->width - 20.0f, 32.0f}, NULL).clicked && browser_open != NULL)
        browser_open(browser_context, object->id);
    if(rohr_ui_button("editor.add_animated_sprite", &editor->add_animation_label,
            (UIRect){context->x + 10.0f, 280.0f,
                context->width - 20.0f, 32.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_ADD,
            .data.animated_sprite_add = {.object = object->id}};
        snprintf(command.data.animated_sprite_add.name,
            sizeof(command.data.animated_sprite_add.name), "animated_sprite_%u",
            context->project->next_animated_sprite_id);
        EditorCommandResult result = editor_command_execute(context->project, &command);
        if(result.kind == ERROR_RESULT_VALUE) {
            context->viewport->selection = EDITOR_SELECTION_ANIMATED_SPRITE;
            context->viewport->selected_animated_sprite = result.result.object;
        }
    }
    editor_project_object_hierarchy_sync(object);
    for(size_t i = 0; i < object->hierarchy_count; i += 1) {
        EditorHierarchyItem item = object->hierarchy[i];
        EditorHierarchySelection selection;
        EditorVisibilityKind visibility;
        TextAsset *label = NULL;
        char *cache = NULL;
        const char *item_name = NULL;
        bool visible = false;
        char id[64], visibility_id[72];
        float y = 326.0f + (float)i * 30.0f;
        if(!item_info_get(editor, object, item, &item_name, &label, &cache,
                &visible, &selection, &visibility)) continue;
        if(!editor_mode_named_text_sync(editor->font, item_name, label, cache,
                EDITOR_OBJECT_NAME_MAX)) return false;
        snprintf(id, sizeof(id), "editor.object.item.%u.%u", selection, item.id);
        snprintf(visibility_id, sizeof(visibility_id),
            "editor.object.item.%u.%u.visibility", selection, item.id);
        item_visibility_toggle(editor, context->project, object, item, selection,
            visibility, visible, visibility_id,
            (UIRect){context->x + 10.0f, y, 26.0f, 26.0f});
        UIRect bounds = {context->x + 42.0f, y, context->width - 50.0f, 26.0f};
        EditorSelectionRef ref = {selection, object->id, 0, 0, item.id};
        UIButtonStyle style = selected_style_get();
        UIButtonResult result = rohr_ui_button(id, label, bounds,
            editor_viewport_selection_contains(context->viewport, ref) ? &style : NULL);
        if(context->hierarchy_row != NULL)
            context->hierarchy_row(context->hierarchy_context, context->viewport,
                ref, bounds, result, i + 1 == object->hierarchy_count);
        if(result.clicked) {
            bool retained = result.double_clicked &&
                context->viewport->selected_item_count > 1 &&
                editor_viewport_selection_contains(context->viewport, ref);
            if(!retained) (void)editor_viewport_selection_set(context->project,
                context->viewport, ref, additive_selection);
            if(result.double_clicked && !retained)
                (void)editor_navigation_selected_open(context->project,
                    context->viewport);
        } else if(result.focus_changed)
            (void)editor_viewport_selection_set(context->project,
                context->viewport, ref, false);
    }
    if(context->delete_open_item != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        if(rohr_ui_button("editor.object.delete", &editor->delete_label,
                (UIRect){context->x + 10.0f,
                    context->delete_y_get != NULL ?
                        context->delete_y_get(context->delete_context) : 650.0f,
                    context->width - 20.0f, 34.0f}, &style).clicked)
            (void)context->delete_open_item(context->delete_context);
    }
    return name_result.active;
}
