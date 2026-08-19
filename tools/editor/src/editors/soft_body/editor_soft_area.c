#include "editor_soft_area.h"

#include "editors/editor_mode_controls.h"

#include <math.h>
#include <stdio.h>

static EditorSoftBody *body_get(EditorObject *object, EditorSoftBodyId id) {
    if(object == NULL) return NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == id) return &object->soft_body_items[i];
    return NULL;
}

static EditorSoftArea *area_get(EditorSoftBody *body, EditorSoftAreaId id) {
    if(body == NULL) return NULL;
    for(size_t i = 0; i < body->area_count; i += 1)
        if(body->areas[i].id == id) return &body->areas[i];
    return NULL;
}

static EditorSoftBeam *area_beam_get(EditorSoftBody *body,
        const EditorSoftArea *area, size_t edge) {
    EditorSoftNodeId a, b;
    if(body == NULL || area == NULL || edge >= area->node_count) return NULL;
    a = area->nodes[edge]; b = area->nodes[(edge + 1) % area->node_count];
    for(size_t i = 0; i < body->beam_count; i += 1)
        if((body->beams[i].node_a == a && body->beams[i].node_b == b) ||
                (body->beams[i].node_a == b && body->beams[i].node_b == a))
            return &body->beams[i];
    return NULL;
}

static UIButtonStyle selected_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    style.idle = (Color){118, 96, 35, 255};
    style.hovered = (Color){145, 119, 45, 255};
    return style;
}

static bool local_swatch(const char *id, uint32_t *color, bool disabled,
        UIRect bounds, const EditorModeContext *context) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    Color displayed = disabled ? (Color){70, 72, 78, 255} :
        rohr_graphics_color_hex_create(*color);
    UIButtonResult result;
    style.idle = displayed; style.pressed = displayed; style.disabled = displayed;
    style.hovered = disabled ? displayed :
        (Color){displayed.red, displayed.green, displayed.blue, 220};
    if(disabled) {
        rohr_ui_button_disabled(bounds, &style);
        rohr_ui_border(bounds, 2.0f, (Color){18, 20, 24, 255});
        return false;
    }
    result = rohr_ui_button(id, NULL, bounds, &style);
    rohr_ui_border(bounds, 2.0f, (Color){8, 9, 12, 255});
    if(result.clicked && context->local_color_open != NULL)
        context->local_color_open(context->color_context, color);
    return result.clicked;
}

bool editor_soft_area_editor_create(EditorSoftAreaEditor *editor,
        FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorSoftAreaEditor){.font = font,
        .boundary_beam_color = UINT32_C(0xffffffff)};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Name", name_label); CREATE("Area Color", area_color_label);
    CREATE("Beam Color", beam_color_label); CREATE("Inherit", inherit_label);
#undef CREATE
    for(size_t i = 0; i < EDITOR_SOFT_AREA_MAX; i += 1) {
        char name[32]; snprintf(name, sizeof(name), "area_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->area_names[i])) goto fail;
    }
    for(size_t i = 0; i < EDITOR_SOFT_BEAM_MAX; i += 1) {
        char name[32]; snprintf(name, sizeof(name), "beam_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->beam_names[i])) goto fail;
    }
    return true;
fail:
    editor_soft_area_editor_destroy(editor);
    return false;
}

void editor_soft_area_editor_destroy(EditorSoftAreaEditor *editor) {
    if(editor == NULL) return;
    rohr_graphics_text_destroy(&editor->name_label);
    rohr_graphics_text_destroy(&editor->area_color_label);
    rohr_graphics_text_destroy(&editor->beam_color_label);
    rohr_graphics_text_destroy(&editor->inherit_label);
    for(size_t i = 0; i < EDITOR_SOFT_AREA_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->area_names[i]);
    for(size_t i = 0; i < EDITOR_SOFT_BEAM_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->beam_names[i]);
    *editor = (EditorSoftAreaEditor){0};
}

bool editor_soft_area_editor_draw(EditorSoftAreaEditor *editor,
        const EditorModeContext *context) {
    EditorObject *object;
    EditorSoftBody *body;
    EditorSoftArea *area;
    size_t area_index;
    float y = 42.0f;
    UIFieldResult name_result;
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    body = body_get(object, context->viewport->selected_soft_body);
    area = area_get(body, context->viewport->selected_soft_area);
    if(area == NULL) return false;
    area_index = (size_t)(area - body->areas);
    if(area_index >= EDITOR_SOFT_AREA_MAX) return false;
    for(size_t candidate_index = 0;
            candidate_index < context->viewport->soft_area_candidate_count;
            candidate_index += 1) {
        EditorSoftArea *candidate = area_get(body,
            context->viewport->soft_area_candidates[candidate_index]);
        size_t index;
        char id[64];
        UIButtonStyle style = selected_style_get();
        UIButtonResult result;
        if(candidate == NULL) continue;
        index = (size_t)(candidate - body->areas);
        if(index >= EDITOR_SOFT_AREA_MAX ||
                !editor_mode_named_text_sync(editor->font, candidate->name,
                    &editor->area_names[index], editor->area_cache[index],
                    EDITOR_OBJECT_NAME_MAX)) return false;
        snprintf(id, sizeof(id), "editor.soft_area.candidate.%u", candidate->id);
        result = rohr_ui_button(id, &editor->area_names[index],
            (UIRect){context->x + 10.0f, y,
                context->width - 20.0f, 28.0f},
            candidate->id == area->id ? &style : NULL);
        if(result.clicked || result.focus_changed) {
            context->viewport->selected_soft_area = candidate->id;
            context->viewport->selection = EDITOR_SELECTION_SOFT_AREA;
            editor->boundary_color_area = 0;
        }
        y += 32.0f;
    }
    y += 10.0f;
    if(!editor_mode_named_text_sync(editor->font, area->name,
            &editor->area_names[area_index], editor->area_cache[area_index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    if(!area->color_overridden) area->color = body->area_color;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 8.0f, y, 48.0f, 30.0f});
    name_result = editor_mode_name_field("editor.soft_area.name", area->name,
        sizeof(area->name), &editor->area_names[area_index],
        (UIRect){context->x + 58.0f, y,
            context->width - 68.0f, 30.0f});
    y += 40.0f;
    rohr_ui_label(&editor->area_color_label,
        (UIRect){context->x + 8.0f, y, 90.0f, 26.0f});
    {
        bool inherit = !area->color_overridden;
        float field_width = fmaxf(34.0f, context->width - 196.0f);
        if(editor_mode_checkbox_left("editor.soft_area.color_inherit",
                &editor->inherit_label,
                (UIRect){context->x + context->width - 92.0f,
                    y, 82.0f, 26.0f}, &inherit)) {
            area->color_overridden = !inherit;
            area->color = body->area_color;
        }
        (void)editor_mode_color_swatch("editor.soft_area.color", &area->color,
            inherit, (UIRect){context->x + 100.0f, y, field_width, 26.0f},
            context, EDITOR_ITEM_SOFT_AREA, object->id, body->id, area->id,
            EDITOR_PROPERTY_COLOR);
    }
    y += 36.0f;
    if(editor->boundary_color_area != area->id) {
        EditorSoftBeam *first = area_beam_get(body, area, 0);
        editor->boundary_beam_color = first != NULL && first->color_overridden ?
            first->color : body->beam_color;
        editor->boundary_color_area = area->id;
    }
    {
        bool inherit = true;
        float field_width = fmaxf(34.0f, context->width - 196.0f);
        for(size_t edge = 0; edge < area->node_count; edge += 1) {
            EditorSoftBeam *beam = area_beam_get(body, area, edge);
            if(beam != NULL && beam->color_overridden) inherit = false;
        }
        rohr_ui_label(&editor->beam_color_label,
            (UIRect){context->x + 8.0f, y, 90.0f, 26.0f});
        if(editor_mode_checkbox_left("editor.soft_area.beam_color_inherit",
                &editor->inherit_label,
                (UIRect){context->x + context->width - 92.0f,
                    y, 82.0f, 26.0f}, &inherit)) {
            for(size_t edge = 0; edge < area->node_count; edge += 1) {
                EditorSoftBeam *beam = area_beam_get(body, area, edge);
                if(beam == NULL) continue;
                beam->color_overridden = !inherit;
                beam->color = body->beam_color;
            }
            editor->boundary_beam_color = body->beam_color;
        }
        (void)local_swatch("editor.soft_area.boundary_beam_color",
            &editor->boundary_beam_color, inherit,
            (UIRect){context->x + 100.0f, y, field_width, 26.0f}, context);
        if(!inherit) for(size_t edge = 0; edge < area->node_count; edge += 1) {
            EditorSoftBeam *beam = area_beam_get(body, area, edge);
            if(beam == NULL) continue;
            beam->color = editor->boundary_beam_color;
            beam->color_overridden = true;
        }
    }
    y += 36.0f;
    for(size_t edge = 0; edge < area->node_count; edge += 1) {
        EditorSoftBeam *beam = area_beam_get(body, area, edge);
        size_t beam_index;
        char id[64];
        UIButtonStyle style = selected_style_get();
        UIButtonResult result;
        if(beam == NULL) continue;
        beam_index = (size_t)(beam - body->beams);
        if(beam_index >= EDITOR_SOFT_BEAM_MAX ||
                !editor_mode_named_text_sync(editor->font, beam->name,
                    &editor->beam_names[beam_index], editor->beam_cache[beam_index],
                    EDITOR_OBJECT_NAME_MAX)) return name_result.active;
        snprintf(id, sizeof(id), "editor.soft_area.beam.%u", beam->id);
        result = rohr_ui_button(id, &editor->beam_names[beam_index],
            (UIRect){context->x + 10.0f, y + (float)edge * 34.0f,
                context->width - 20.0f, 28.0f}, &style);
        if(result.clicked || result.focus_changed) {
            context->viewport->selection = EDITOR_SELECTION_SOFT_BEAM;
            context->viewport->selected_soft_beam = beam->id;
            context->viewport->mode = EDITOR_VIEWPORT_SOFT_BEAM;
        }
    }
    return name_result.active;
}
