#include "editor_particle.h"

#include "editors/editor_mode_controls.h"

#include <math.h>

bool editor_particle_editor_create(EditorParticleEditor *editor,
        FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorParticleEditor){0};
    if(!editor_mode_text_create(font, "Particle", &editor->title) ||
            !editor_mode_text_create(font, "Radius", &editor->radius_label) ||
            !editor_mode_text_create(font, "Origin X", &editor->origin_x_label) ||
            !editor_mode_text_create(font, "Origin Y", &editor->origin_y_label) ||
            !editor_mode_text_create(font, "Auto Fit", &editor->auto_fit_label) ||
            !editor_mode_text_create(font, "Ring Color",
                &editor->ring_color_label) ||
            !editor_mode_text_create(font, "Fill Color",
                &editor->fill_color_label) ||
            !editor_mode_text_create(font, "", &editor->radius_field) ||
            !editor_mode_text_create(font, "", &editor->origin_x_field) ||
            !editor_mode_text_create(font, "", &editor->origin_y_field)) {
        editor_particle_editor_destroy(editor);
        return false;
    }
    return true;
}

void editor_particle_editor_destroy(EditorParticleEditor *editor) {
    if(editor == NULL) return;
    rohr_graphics_text_destroy(&editor->title);
    rohr_graphics_text_destroy(&editor->radius_label);
    rohr_graphics_text_destroy(&editor->origin_x_label);
    rohr_graphics_text_destroy(&editor->origin_y_label);
    rohr_graphics_text_destroy(&editor->auto_fit_label);
    rohr_graphics_text_destroy(&editor->ring_color_label);
    rohr_graphics_text_destroy(&editor->fill_color_label);
    rohr_graphics_text_destroy(&editor->radius_field);
    rohr_graphics_text_destroy(&editor->origin_x_field);
    rohr_graphics_text_destroy(&editor->origin_y_field);
    *editor = (EditorParticleEditor){0};
}

bool editor_particle_editor_draw(EditorParticleEditor *editor,
        const EditorModeContext *context) {
    EditorObject *object;
    EditorRigidBody *body;
    UIFieldResult radius = {0};
    UIFieldResult origin_x = {0};
    UIFieldResult origin_y = {0};
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    body = object == NULL ? NULL : editor_project_rigid_body_get(object,
        context->viewport->selected_rigid_body);
    if(body == NULL || !body->particle) return false;
    rohr_ui_label(&editor->title,
        (UIRect){context->x + 10.0f, 42.0f, context->width - 20.0f, 30.0f});
    rohr_ui_label(&editor->radius_label,
        (UIRect){context->x + 8.0f, 84.0f, 52.0f, 26.0f});
    if(body->particle_auto_fit) {
        editor_mode_numeric_disabled_draw(&editor->radius_field,
            body->particle_radius, (UIRect){context->x + 62.0f, 84.0f,
                fmaxf(30.0f, context->width - 166.0f), 26.0f});
    } else {
        radius = rohr_ui_field("editor.particle.radius",
            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                .number = &body->particle_radius}, &editor->radius_field,
            (UIRect){context->x + 62.0f, 84.0f,
                fmaxf(30.0f, context->width - 166.0f), 26.0f}, NULL);
    }
    if(radius.changed) body->particle_radius = fmaxf(0.0f, body->particle_radius);
    if(editor_mode_checkbox_left("editor.particle.auto_fit",
            &editor->auto_fit_label,
            (UIRect){context->x + context->width - 100.0f,
                84.0f, 90.0f, 26.0f}, &body->particle_auto_fit) &&
            body->particle_auto_fit)
        body->particle_radius = editor_project_particle_auto_radius_get(body);
    rohr_ui_label(&editor->origin_x_label,
        (UIRect){context->x + 8.0f, 120.0f, 72.0f, 26.0f});
    origin_x = rohr_ui_field("editor.particle.origin_x",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
            .number = &body->particle_origin.x}, &editor->origin_x_field,
        (UIRect){context->x + 82.0f, 120.0f,
            context->width - 92.0f, 26.0f}, NULL);
    rohr_ui_label(&editor->origin_y_label,
        (UIRect){context->x + 8.0f, 156.0f, 72.0f, 26.0f});
    origin_y = rohr_ui_field("editor.particle.origin_y",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
            .number = &body->particle_origin.y}, &editor->origin_y_field,
        (UIRect){context->x + 82.0f, 156.0f,
            context->width - 92.0f, 26.0f}, NULL);
    rohr_ui_label(&editor->ring_color_label,
        (UIRect){context->x + 8.0f, 192.0f, 104.0f, 26.0f});
    (void)editor_mode_color_swatch("editor.particle.ring_color",
        &body->particle_ring_color, false,
        (UIRect){context->x + 114.0f, 192.0f,
            context->width - 124.0f, 26.0f}, context,
        EDITOR_ITEM_RIGID_BODY, object->id, 0, body->id,
        EDITOR_PROPERTY_PARTICLE_RING_COLOR);
    rohr_ui_label(&editor->fill_color_label,
        (UIRect){context->x + 8.0f, 228.0f, 104.0f, 26.0f});
    (void)editor_mode_color_swatch("editor.particle.fill_color",
        &body->particle_fill_color, false,
        (UIRect){context->x + 114.0f, 228.0f,
            context->width - 124.0f, 26.0f}, context,
        EDITOR_ITEM_RIGID_BODY, object->id, 0, body->id,
        EDITOR_PROPERTY_PARTICLE_FILL_COLOR);
    return radius.active || origin_x.active || origin_y.active;
}
