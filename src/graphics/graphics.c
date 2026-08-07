#include "graphics.h"
#include "core/engine_internal.h"
#include "console.h"
#include "engine.h"
#include "systems.h"
#include "physics.h"
#include "core/platform_process.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static SDL_Renderer *sdl_renderer = NULL;
static SDL_Window *sdl_window = NULL;
static TTF_TextEngine *ttf_text_engine = NULL;
static bool ttf_initialized = false;
static bool graphics_aabb_tree_debug_enabled = false;
static Camera camera = {0};

#define GRAPHICS_CLIP_STACK_MAX 16
typedef struct GraphicsClipState {
    SDL_Rect rectangle;
    bool active;
} GraphicsClipState;
static GraphicsClipState graphics_clip_state = {0};
static GraphicsClipState graphics_clip_stack[GRAPHICS_CLIP_STACK_MAX] = {0};
static size_t graphics_clip_stack_count = 0;

typedef struct ActiveCameraAttachment {
    CameraAttachment value;
    bool attached;
} ActiveCameraAttachment;

static ActiveCameraAttachment camera_attachment = {0};
static Camera cameras[MAX_CAMERAS] = {0};
static ActiveCameraAttachment camera_attachments[MAX_CAMERAS] = {0};
typedef struct CameraRuntime {
    CameraRenderCallback callback;
    void *context;
    bool enabled;
    bool pause_with_engine;
} CameraRuntime;
static CameraRuntime camera_runtime[MAX_CAMERAS] = {0};
typedef struct CameraMotion {
    Position start;
    Position target;
    Tick start_tick;
    Tick duration_ticks;
    bool active;
} CameraMotion;
static CameraMotion camera_motion[MAX_CAMERAS] = {0};
typedef struct CameraZoomMotion {
    float start;
    float target;
    Tick start_tick;
    Tick duration_ticks;
    bool active;
} CameraZoomMotion;
static CameraZoomMotion camera_zoom_motion[MAX_CAMERAS] = {0};
static uint32_t camera_generations[MAX_CAMERAS] = {0};
static bool cameras_used[MAX_CAMERAS] = {0};
static CameraId active_camera = CAMERA_INVALID;
static bool graphics_resolve_camera_attachment(void);

typedef struct GraphicsScreen {
    CameraId camera;
    int width;
    int height;
    SDL_Texture *texture;
} GraphicsScreen;

typedef uint32_t ScreenId;
#define SCREEN_INVALID 0
typedef struct ScreenConfig {
    CameraId camera;
    int width;
    int height;
} ScreenConfig;
ERROR_DECLARE_RESULT_TYPE(ScreenIdResult, ScreenId);

typedef struct GraphicsViewport {
    ViewportRectangle rectangle;
    ScreenFit fit;
    CameraId camera;
    bool enabled;
} GraphicsViewport;

static GraphicsScreen screens[MAX_SCREENS] = {0};
static GraphicsViewport viewports[MAX_VIEWPORTS] = {0};
static uint32_t screen_generations[MAX_SCREENS] = {0};
static uint32_t viewport_generations[MAX_VIEWPORTS] = {0};
static bool screens_used[MAX_SCREENS] = {0};
static bool viewports_used[MAX_VIEWPORTS] = {0};
static ScreenId drawing_screen = SCREEN_INVALID;
static CameraId camera_before_screen = CAMERA_INVALID;
static EngineResult graphics_screen_destroy(ScreenId id);

static uint32_t graphics_resource_id(uint32_t generation, size_t slot) {
    return (generation << 8) | (uint32_t)(slot + 1);
}

static bool graphics_screen_slot(ScreenId id, size_t *slot) {
    size_t value;
    if(id == SCREEN_INVALID || slot == NULL) return false;
    value = (size_t)((id & 0xffu) - 1u);
    if(value >= MAX_SCREENS || !screens_used[value]
            || graphics_resource_id(screen_generations[value], value) != id) return false;
    *slot = value;
    return true;
}

static bool graphics_screen_for_camera(CameraId camera_id, size_t *slot) {
    size_t value;
    if(slot == NULL) return false;
    for(value = 0; value < MAX_SCREENS; value += 1) {
        if(screens_used[value] && screens[value].camera == camera_id) {
            *slot = value;
            return true;
        }
    }
    return false;
}

static bool graphics_camera_viewport_size(CameraId camera_id, int *width, int *height) {
    size_t slot;
    bool found = false;
    if(width == NULL || height == NULL) return false;
    for(slot = 0; slot < MAX_VIEWPORTS; slot += 1) {
        if(!viewports_used[slot]) continue;
        if(viewports[slot].camera == camera_id || (!found && camera_id == CAMERA_INVALID)) {
            *width = (int)viewports[slot].rectangle.width;
            *height = (int)viewports[slot].rectangle.height;
            found = true;
            if(viewports[slot].camera == camera_id) return true;
        }
    }
    return found;
}

static bool graphics_viewport_slot(ViewportId id, size_t *slot) {
    size_t value;
    if(id == VIEWPORT_INVALID || slot == NULL) return false;
    value = (size_t)((id & 0xffu) - 1u);
    if(value >= MAX_VIEWPORTS || !viewports_used[value]
            || graphics_resource_id(viewport_generations[value], value) != id) return false;
    *slot = value;
    return true;
}

static CameraId graphics_camera_id(size_t slot) {
    return (CameraId)((camera_generations[slot] << 8) | (uint32_t)(slot + 1));
}

static bool graphics_camera_slot(CameraId id, size_t *slot) {
    size_t value;
    if(id == CAMERA_INVALID || slot == NULL) return false;
    value = (size_t)((id & 0xffu) - 1u);
    if(value >= MAX_CAMERAS || !cameras_used[value] || graphics_camera_id(value) != id) return false;
    *slot = value;
    return true;
}

CameraConfig graphics_camera_config_default_get(void) {
    return (CameraConfig){
        .dimensions = {WINDOW_WIDTH, WINDOW_HEIGHT},
        .zoom = 1.0f,
    };
}

static Camera graphics_camera_from_config_get(CameraConfig config) {
    CameraConfig defaults = graphics_camera_config_default_get();
    if(config.dimensions.x <= 0.0f) config.dimensions.x = defaults.dimensions.x;
    if(config.dimensions.y <= 0.0f) config.dimensions.y = defaults.dimensions.y;
    if(config.zoom <= 0.0f) config.zoom = defaults.zoom;
    return (Camera){
        .position = config.position,
        .orientation = config.orientation,
        .dimensions = config.dimensions,
        .zoom = config.zoom,
    };
}

MEMORY_DEFINE_OBJECT_POOL(AnimatedSpritePool, AnimatedSprite)

AnimatedSpritePool animated_sprites_pool = {0};
const Color hit_box_color = {255,0,0,255};
const Color particle_color = {0,0,255,255};

#include <stdint.h>

typedef struct ScreenRecorder {
    FILE *ffmpeg_pipe;

    bool recording;
    int fps;
    int width;
    int height;

    char output_path[512];
} ScreenRecorder;

static ScreenRecorder screen_recorder = {0};

static void graphics_recording_disable(const char *reason) {
    if(reason != NULL) {
        console_write(LOG_ENGINE, "%s\n", reason);
    }

    if(screen_recorder.ffmpeg_pipe != NULL) {
        (void)platform_process_close(screen_recorder.ffmpeg_pipe);
    }

    screen_recorder = (ScreenRecorder){0};
}

EngineResult graphics_tables_init(void) {
    CameraConfig default_config = graphics_camera_config_default_get();
    memset(cameras, 0, sizeof(cameras));
    memset(camera_attachments, 0, sizeof(camera_attachments));
    memset(camera_runtime, 0, sizeof(camera_runtime));
    memset(camera_motion, 0, sizeof(camera_motion));
    memset(camera_zoom_motion, 0, sizeof(camera_zoom_motion));
    memset(camera_generations, 0, sizeof(camera_generations));
    memset(cameras_used, 0, sizeof(cameras_used));
    cameras_used[0] = true;
    camera_generations[0] = 1;
    active_camera = graphics_camera_id(0);
    camera = graphics_camera_from_config_get(default_config);
    cameras[0] = camera;
    camera_runtime[0].enabled = true;
    camera_attachment = (ActiveCameraAttachment){0};
    memset(screens, 0, sizeof(screens));
    memset(viewports, 0, sizeof(viewports));
    memset(screen_generations, 0, sizeof(screen_generations));
    memset(viewport_generations, 0, sizeof(viewport_generations));
    memset(screens_used, 0, sizeof(screens_used));
    memset(viewports_used, 0, sizeof(viewports_used));
    drawing_screen = SCREEN_INVALID;
    camera_before_screen = CAMERA_INVALID;
    graphics_aabb_tree_debug_enabled = false;
    if(AnimatedSpritePool_init(&animated_sprites_pool, 0).kind == ERROR_RESULT_ERROR) {
        graphics_tables_destroy();
        return error_result_error(ERROR_ENGINE_GRAPHICS_TABLES_INIT_FAILED);
    }
    return error_result_value(true);
}

EngineResult graphics_tables_ensure_capacity(size_t capacity) {
    size_t new_capacity;

    if(capacity > MAX_ENTITIES) {
        return error_result_error(ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    if(capacity <= animated_sprites_pool.capacity) {
        return error_result_value(true);
    }
    new_capacity = animated_sprites_pool.capacity == 0 ? 16 : animated_sprites_pool.capacity;
    while(new_capacity < capacity) {
        new_capacity *= 2;
    }
    if(new_capacity > MAX_ENTITIES) {
        new_capacity = MAX_ENTITIES;
    }
    if(AnimatedSpritePool_expand(
        &animated_sprites_pool,
        new_capacity - animated_sprites_pool.capacity
    ).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED);
    }
    return error_result_value(true);
}

void graphics_tables_destroy(void) {
    (void)AnimatedSpritePool_destroy(&animated_sprites_pool);
}

bool graphics_recording_start(const char *output_path, int fps) {
    if(screen_recorder.recording) {
        return false;
    }

    if(output_path == NULL || fps <= 0) {
        return false;
    }

    if(!platform_process_command_check("ffmpeg")) {
        console_write(
            LOG_ENGINE,
            "FFmpeg was not found; recording disabled\n"
        );
        return false;
    }

    screen_recorder = (ScreenRecorder){
        .ffmpeg_pipe = NULL,
        .recording = true,
        .fps = fps,
        .width = 0,
        .height = 0
    };

    snprintf(
        screen_recorder.output_path,
        sizeof(screen_recorder.output_path),
        "%s",
        output_path
    );

    return true;
}
static bool graphics_recording_ffmpeg_open(int width, int height) {
    char command[1024];

    snprintf(
        command,
        sizeof(command),

        "ffmpeg -y "
        "-loglevel warning "
        "-f rawvideo "
        "-pixel_format rgba "
        "-video_size %dx%d "
        "-framerate %d "
        "-i pipe:0 "
        "-an "
        "-c:v libx264 "
        "-preset ultrafast "
        "-crf 30 "
        "-pix_fmt yuv420p "
        "\"%s\"",

        width,
        height,
        screen_recorder.fps,
        screen_recorder.output_path
    );

    screen_recorder.ffmpeg_pipe =
        platform_process_open_write(command);

    if(screen_recorder.ffmpeg_pipe == NULL) {
        graphics_recording_disable("Failed to start FFmpeg recording; recording disabled");
        return false;
    }

    screen_recorder.width = width;
    screen_recorder.height = height;

    console_write(
        LOG_ENGINE,
        "Started recording %dx%d at %d FPS\n",
        width,
        height,
        screen_recorder.fps
    );

    return true;
}

static bool graphics_record_frame(void)
{
    if(!screen_recorder.recording) {
        return true;
    }

    SDL_FRect presentation_frect;

    if(!SDL_GetRenderLogicalPresentationRect(
        sdl_renderer,
        &presentation_frect
    )) {
        graphics_recording_disable("Failed to get presentation rect; recording disabled");
        return true;
    }

    SDL_Rect presentation_rect = {
        .x = (int)presentation_frect.x,
        .y = (int)presentation_frect.y,
        .w = (int)presentation_frect.w,
        .h = (int)presentation_frect.h
    };

    if(presentation_rect.w <= 0 ||
       presentation_rect.h <= 0) {
        return true;
    }

    SDL_Surface *captured =
        SDL_RenderReadPixels(
            sdl_renderer,
            &presentation_rect
        );

    if(captured == NULL) {
        graphics_recording_disable("Failed to capture frame; recording disabled");
        return true;
    }

    SDL_Surface *scaled =
        SDL_ScaleSurface(
            captured,
            RECORDING_WIDTH,
            RECORDING_HEIGHT,
            SDL_SCALEMODE_LINEAR
        );

    SDL_DestroySurface(captured);

    if(scaled == NULL) {
        graphics_recording_disable("Failed to scale recorded frame; recording disabled");
        return true;
    }

    SDL_Surface *rgba_surface =
        SDL_ConvertSurface(
            scaled,
            SDL_PIXELFORMAT_RGBA32
        );

    SDL_DestroySurface(scaled);

    if(rgba_surface == NULL) {
        graphics_recording_disable("Failed to convert recorded frame; recording disabled");
        return true;
    }

    if(screen_recorder.ffmpeg_pipe == NULL) {
        if(!graphics_recording_ffmpeg_open(
            RECORDING_WIDTH,
            RECORDING_HEIGHT
        )) {
            SDL_DestroySurface(rgba_surface);
            return true;
        }
    }

    size_t bytes_per_row =
        (size_t)RECORDING_WIDTH * 4;

    uint8_t *pixels =
        (uint8_t *)rgba_surface->pixels;

    for(int y = 0; y < RECORDING_HEIGHT; y++) {
        uint8_t *row =
            pixels + y * rgba_surface->pitch;

        if(fwrite(
            row,
            1,
            bytes_per_row,
            screen_recorder.ffmpeg_pipe
        ) != bytes_per_row) {
            SDL_DestroySurface(rgba_surface);
            graphics_recording_disable("Failed writing recorded frame; recording disabled");
            return true;
        }
    }

    SDL_DestroySurface(rgba_surface);
    return true;
}
void graphics_recording_stop(void) {
    if(!screen_recorder.recording) {
        return;
    }

    if(screen_recorder.ffmpeg_pipe != NULL) {
        int result =
            platform_process_close(screen_recorder.ffmpeg_pipe);

        if(result != 0) {
            console_write(
                LOG_ENGINE,
                "FFmpeg exited with status: %d\n",
                result
            );
        }
    }

    screen_recorder = (ScreenRecorder){0};

    console_write(
        LOG_ENGINE,
        "Recording stopped\n"
    );
}

void graphics_aabb_tree_debug_set(bool enabled) {
    graphics_aabb_tree_debug_enabled = enabled;
}

bool graphics_aabb_tree_debug_check(void) {
    return graphics_aabb_tree_debug_enabled;
}

void graphics_aabb_tree_draw(void) {
    if(!graphics_aabb_tree_debug_enabled || sdl_renderer == NULL) return;
    for(size_t index = 0; index < physics_broadphase_tree.count; index += 1) {
        const AABBTreeNode *node = &physics_broadphase_tree.nodes[index];
        bool leaf = aabb_tree_node_leaf_check(node);
        Position top_left = graphics_world_to_screen_get(
            (Position){node->bounds.min_x, node->bounds.max_y});
        Position top_right = graphics_world_to_screen_get(
            (Position){node->bounds.max_x, node->bounds.max_y});
        Position bottom_right = graphics_world_to_screen_get(
            (Position){node->bounds.max_x, node->bounds.min_y});
        Position bottom_left = graphics_world_to_screen_get(
            (Position){node->bounds.min_x, node->bounds.min_y});
        SDL_FPoint points[5] = {
            {top_left.x, top_left.y},
            {top_right.x, top_right.y},
            {bottom_right.x, bottom_right.y},
            {bottom_left.x, bottom_left.y},
            {top_left.x, top_left.y}
        };

        (void)SDL_SetRenderDrawColor(
            sdl_renderer,
            leaf ? 80 : 255,
            leaf ? 220 : 170,
            leaf ? 120 : 40,
            255
        );
        (void)SDL_RenderLines(sdl_renderer, points, 5);
    }
}

void graphics_textures_scale(Entity entity, Scale scale) {
    EntityIndex index;

    if(!entity_index_get(entity, &index) || !entity_components_check(entity, ANIMATED_SPRITE)) {
        return;
    }
    for(int i = 0; i < MAX_TEXTURES; i += 1) {
        animated_sprites[index].animation.texture_list.textures[i].size.x *= scale.x;
        animated_sprites[index].animation.texture_list.textures[i].size.y *= scale.y;
    }
}

Color graphics_color_hex_create(uint32_t hex_color_code) {
  return (Color) {
    .red = (hex_color_code >> 16) & 0xFF,
    .green = (hex_color_code >> 8)  & 0xFF,
    .blue = hex_color_code         & 0xFF,
    .alpha = 255
  };
}

Color graphics_color_rgba_create(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
  return (Color){
      .red = red,
      .blue = blue,
      .green = green,
      .alpha = alpha
  };
}

static void graphics_camera_store_active(void) {
    size_t slot;
    if(graphics_camera_slot(active_camera, &slot)) {
        cameras[slot] = camera;
        camera_attachments[slot] = camera_attachment;
    }
}

CameraIdResult graphics_camera_create(CameraConfig config) {
    size_t slot;
    for(slot = 0; slot < MAX_CAMERAS; slot += 1) {
        if(!cameras_used[slot]) {
            camera_generations[slot] += 1;
            if(camera_generations[slot] == 0) camera_generations[slot] = 1;
            cameras_used[slot] = true;
            cameras[slot] = graphics_camera_from_config_get(config);
            camera_attachments[slot] = (ActiveCameraAttachment){0};
            camera_runtime[slot] = (CameraRuntime){.enabled = true};
            camera_motion[slot] = (CameraMotion){0};
            camera_zoom_motion[slot] = (CameraZoomMotion){0};
            return ERROR_RESULT_MAKE_VALUE(CameraIdResult, graphics_camera_id(slot));
        }
    }
    return ERROR_RESULT_MAKE_ERROR(CameraIdResult, ERROR_MEMORY_POOL_FULL);
}

EngineResult graphics_camera_active_set(CameraId id) {
    size_t slot;
    if(!graphics_camera_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    graphics_camera_store_active();
    active_camera = id;
    camera = cameras[slot];
    camera_attachment = camera_attachments[slot];
    if(sdl_renderer != NULL) {
        (void)SDL_SetRenderViewport(sdl_renderer, NULL);
        (void)SDL_SetRenderClipRect(sdl_renderer, NULL);
    }
    return error_result_value(true);
}

CameraId graphics_camera_active_get(void) {
    return active_camera;
}

CameraResult graphics_camera_get(CameraId id) {
    size_t slot;
    if(!graphics_camera_slot(id, &slot)) {
        return ERROR_RESULT_MAKE_ERROR(CameraResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(id == active_camera) {
        (void)graphics_resolve_camera_attachment();
        graphics_camera_store_active();
    }
    return ERROR_RESULT_MAKE_VALUE(CameraResult, cameras[slot]);
}

EngineResult graphics_camera_set(CameraId id, Camera value) {
    size_t slot;
    if(!graphics_camera_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    value = graphics_camera_from_config_get((CameraConfig){
        .position = value.position,
        .orientation = value.orientation,
        .dimensions = value.dimensions,
        .zoom = value.zoom,
    });
    cameras[slot] = value;
    camera_attachments[slot] = (ActiveCameraAttachment){0};
    camera_motion[slot] = (CameraMotion){0};
    camera_zoom_motion[slot] = (CameraZoomMotion){0};
    if(id == active_camera) {
        camera = value;
        camera_attachment = (ActiveCameraAttachment){0};
        return graphics_camera_active_set(id);
    }
    return error_result_value(true);
}

EngineResult graphics_camera_destroy(CameraId id) {
    size_t slot;
    size_t screen_slot;
    size_t viewport_slot;
    if(!graphics_camera_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(id == active_camera) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(graphics_screen_for_camera(id, &screen_slot)) {
        EngineResult result = graphics_screen_destroy(
            graphics_resource_id(screen_generations[screen_slot], screen_slot)
        );
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }
    for(viewport_slot = 0; viewport_slot < MAX_VIEWPORTS; viewport_slot += 1) {
        if(viewports_used[viewport_slot] && viewports[viewport_slot].camera == id) {
            viewports[viewport_slot].camera = CAMERA_INVALID;
        }
    }
    cameras_used[slot] = false;
    cameras[slot] = (Camera){0};
    camera_attachments[slot] = (ActiveCameraAttachment){0};
    camera_runtime[slot] = (CameraRuntime){0};
    camera_motion[slot] = (CameraMotion){0};
    camera_zoom_motion[slot] = (CameraZoomMotion){0};
    return error_result_value(true);
}

static bool graphics_resolve_camera_attachment(void) {
    EntityIndex index;
    Vec2D world_offset = camera_attachment.value.position_offset;

    if(!camera_attachment.attached) {
        return false;
    }
    if(!entity_index_get(camera_attachment.value.entity, &index)
            || (camera_attachment.value.follow_position
                && (index >= positions_pool.capacity
                    || !positions_pool.used[index]))
            || (camera_attachment.value.follow_orientation
                && (index >= orientations_pool.capacity
                    || !orientations_pool.used[index]))) {
        camera_attachment = (ActiveCameraAttachment){0};
        return false;
    }

    if(camera_attachment.value.follow_orientation) {
        world_offset = math_vector_rotate(
            camera_attachment.value.position_offset,
            orientations[index]
        );
    }
    camera.position = camera_attachment.value.follow_position
            ? (Position){
                .x = positions[index].x + world_offset.x,
                .y = positions[index].y + world_offset.y
            }
            : camera_attachment.value.position_offset;
    camera.orientation = camera_attachment.value.follow_orientation
            ? orientations[index]
                + camera_attachment.value.orientation_offset
            : camera_attachment.value.orientation_offset;
    graphics_camera_store_active();
    return true;
}

void graphics_camera_move(Vec2D translation) {
    (void)graphics_camera_position_move(active_camera, translation, 0.0);
}

void graphics_camera_rotate(Orientation radians) {
    if(graphics_resolve_camera_attachment()) {
        camera_attachment.value.orientation_offset += radians;
    }
    camera.orientation += radians;
    graphics_camera_store_active();
}

EngineResult graphics_camera_attach(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset
) {
    return graphics_camera_with_options_attach(
        entity,
        position_offset,
        orientation_offset,
        true,
        true
    );
}

EngineResult graphics_camera_with_options_attach(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset,
    bool follow_position,
    bool follow_orientation
) {
    EntityIndex index;

    if(!entity_index_get(entity, &index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if((follow_position
            && (index >= positions_pool.capacity
                || !positions_pool.used[index]))
            || (follow_orientation
                && (index >= orientations_pool.capacity
                    || !orientations_pool.used[index]))) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }

    camera_attachment = (ActiveCameraAttachment){
        .value = {
            .entity = entity,
            .position_offset = position_offset,
            .orientation_offset = orientation_offset,
            .follow_position = follow_position,
            .follow_orientation = follow_orientation
        },
        .attached = true
    };
    (void)graphics_resolve_camera_attachment();
    graphics_camera_store_active();
    return error_result_value(true);
}

void graphics_camera_detach(void) {
    (void)graphics_resolve_camera_attachment();
    camera_attachment = (ActiveCameraAttachment){0};
    graphics_camera_store_active();
}

bool graphics_camera_attached_get(void) {
    return graphics_resolve_camera_attachment();
}

bool graphics_camera_attachment_get(CameraAttachment *attachment) {
    if(attachment == NULL || !graphics_resolve_camera_attachment()) {
        return false;
    }
    *attachment = camera_attachment.value;
    return true;
}

EngineResult graphics_camera_attachment_set(
        CameraId id,
        Entity entity,
        Vec2D position_offset,
        Orientation orientation_offset,
        bool follow_position,
        bool follow_orientation
        ) {
    CameraId previous = active_camera;
    size_t slot;
    if(!graphics_camera_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    camera_motion[slot] = (CameraMotion){0};
    EngineResult result = graphics_camera_active_set(id);
    if(error_check(result)) return result;
    result = graphics_camera_with_options_attach(
        entity,
        position_offset,
        orientation_offset,
        follow_position,
        follow_orientation
    );
    graphics_camera_store_active();
    if(previous != id) (void)graphics_camera_active_set(previous);
    return result;
}

EngineResult graphics_camera_attachment_remove(CameraId id) {
    CameraId previous = active_camera;
    size_t slot;
    if(!graphics_camera_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    camera_motion[slot] = (CameraMotion){0};
    EngineResult result = graphics_camera_active_set(id);
    if(error_check(result)) return result;
    graphics_camera_detach();
    graphics_camera_store_active();
    if(previous != id) (void)graphics_camera_active_set(previous);
    return error_result_value(true);
}

static ScreenConfig graphics_screen_config_default_get(void) {
    return (ScreenConfig){
        .camera = active_camera,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
    };
}

static ScreenIdResult graphics_screen_create(ScreenConfig config) {
    size_t slot;
    if(sdl_renderer == NULL) {
        return ERROR_RESULT_MAKE_ERROR(ScreenIdResult, ERROR_ENGINE_GRAPHICS_INIT_FAILED);
    }
    if(!graphics_camera_slot(config.camera, &slot)) {
        return ERROR_RESULT_MAKE_ERROR(ScreenIdResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(config.width <= 0) config.width = WINDOW_WIDTH;
    if(config.height <= 0) config.height = WINDOW_HEIGHT;
    for(slot = 0; slot < MAX_SCREENS; slot += 1) {
        SDL_Texture *texture;
        if(screens_used[slot]) continue;
        texture = SDL_CreateTexture(
            sdl_renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,
            config.width,
            config.height
        );
        if(texture == NULL) {
            return ERROR_RESULT_MAKE_ERROR(ScreenIdResult, ERROR_ENGINE_GRAPHICS_INIT_FAILED);
        }
        (void)SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        screen_generations[slot] += 1;
        if(screen_generations[slot] == 0) screen_generations[slot] = 1;
        screens_used[slot] = true;
        screens[slot] = (GraphicsScreen){
            .camera = config.camera,
            .width = config.width,
            .height = config.height,
            .texture = texture,
        };
        return ERROR_RESULT_MAKE_VALUE(
            ScreenIdResult,
            graphics_resource_id(screen_generations[slot], slot)
        );
    }
    return ERROR_RESULT_MAKE_ERROR(ScreenIdResult, ERROR_MEMORY_POOL_FULL);
}

static EngineResult graphics_screen_destroy(ScreenId id) {
    size_t slot;
    if(!graphics_screen_slot(id, &slot) || id == drawing_screen) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    SDL_DestroyTexture(screens[slot].texture);
    screens[slot] = (GraphicsScreen){0};
    screens_used[slot] = false;
    return error_result_value(true);
}

static EngineResult graphics_screen_begin(ScreenId id) {
    size_t slot;
    if(drawing_screen != SCREEN_INVALID || !graphics_screen_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    camera_before_screen = active_camera;
    {
        EngineResult result = graphics_camera_active_set(screens[slot].camera);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }
    if(!SDL_SetRenderTarget(sdl_renderer, screens[slot].texture)) {
        (void)graphics_camera_active_set(camera_before_screen);
        camera_before_screen = CAMERA_INVALID;
        return error_result_error(ERROR_ENGINE_GRAPHICS_INIT_FAILED);
    }
    (void)SDL_SetRenderViewport(sdl_renderer, NULL);
    (void)SDL_SetRenderClipRect(sdl_renderer, NULL);
    drawing_screen = id;
    return error_result_value(true);
}

static EngineResult graphics_screen_end(void) {
    if(drawing_screen == SCREEN_INVALID) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    (void)SDL_SetRenderTarget(sdl_renderer, NULL);
    drawing_screen = SCREEN_INVALID;
    if(camera_before_screen != CAMERA_INVALID) {
        (void)graphics_camera_active_set(camera_before_screen);
    }
    camera_before_screen = CAMERA_INVALID;
    (void)SDL_SetRenderViewport(sdl_renderer, NULL);
    (void)SDL_SetRenderClipRect(sdl_renderer, NULL);
    return error_result_value(true);
}

static EngineResult graphics_camera_begin(CameraId camera_id) {
    size_t screen_slot;
    int width = WINDOW_WIDTH;
    int height = WINDOW_HEIGHT;
    if(!graphics_camera_slot(camera_id, &screen_slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(!graphics_camera_viewport_size(camera_id, &width, &height)) {
        (void)graphics_camera_viewport_size(CAMERA_INVALID, &width, &height);
    }
    if(graphics_screen_for_camera(camera_id, &screen_slot)
            && (screens[screen_slot].width != width || screens[screen_slot].height != height)
            && graphics_camera_viewport_size(camera_id, &width, &height)) {
        EngineResult result = graphics_screen_destroy(
            graphics_resource_id(screen_generations[screen_slot], screen_slot)
        );
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }
    if(!graphics_screen_for_camera(camera_id, &screen_slot)) {
        ScreenConfig config = graphics_screen_config_default_get();
        ScreenIdResult result;
        config.camera = camera_id;
        config.width = width;
        config.height = height;
        result = graphics_screen_create(config);
        if(result.kind == ERROR_RESULT_ERROR) {
            return error_result_error(result.result.error);
        }
        return graphics_screen_begin(result.result.value);
    }
    return graphics_screen_begin(
        graphics_resource_id(screen_generations[screen_slot], screen_slot)
    );
}

static EngineResult graphics_camera_end(void) {
    return graphics_screen_end();
}

EngineResult graphics_camera_render_callback_set(
    CameraId camera_id,
    CameraRenderCallback callback,
    void *context
) {
    size_t slot;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    camera_runtime[slot].callback = callback;
    camera_runtime[slot].context = context;
    return error_result_value(true);
}

EngineResult graphics_camera_enable_set(CameraId camera_id) {
    size_t slot;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    camera_runtime[slot].enabled = true;
    return error_result_value(true);
}

EngineResult graphics_camera_disable_set(CameraId camera_id) {
    size_t slot;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    camera_runtime[slot].enabled = false;
    return error_result_value(true);
}

EngineResult graphics_camera_pause_with_engine_set(CameraId camera_id) {
    size_t slot;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    camera_runtime[slot].pause_with_engine = true;
    return error_result_value(true);
}

EngineResult graphics_camera_render_when_paused_set(CameraId camera_id) {
    size_t slot;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    camera_runtime[slot].pause_with_engine = false;
    return error_result_value(true);
}

static EngineResult graphics_camera_resolve_and_detach(CameraId camera_id, Camera *value) {
    CameraResult camera_result;
    EngineResult result;
    if(value == NULL) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    result = graphics_camera_attachment_remove(camera_id);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    camera_result = graphics_camera_get(camera_id);
    if(camera_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(camera_result.result.error);
    }
    *value = camera_result.result.value;
    return error_result_value(true);
}

static EngineResult graphics_camera_start_motion(
    CameraId camera_id,
    Position target,
    Time duration
) {
    Camera value;
    size_t slot;
    EngineResult result;
    Tick duration_ticks;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    result = graphics_camera_resolve_and_detach(camera_id, &value);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(duration <= 0.0) {
        value.position = target;
        return graphics_camera_set(camera_id, value);
    }
    duration_ticks = (Tick)ceil(duration / engine_time_per_tick_get());
    if(duration_ticks == 0) duration_ticks = 1;
    camera_motion[slot] = (CameraMotion){
        .start = value.position,
        .target = target,
        .start_tick = engine_tick_get(),
        .duration_ticks = duration_ticks,
        .active = true,
    };
    return error_result_value(true);
}

EngineResult graphics_camera_position_move(
    CameraId camera_id,
    Vec2D translation,
    Time duration
) {
    Camera value;
    EngineResult result = graphics_camera_resolve_and_detach(camera_id, &value);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    return graphics_camera_start_motion(
        camera_id,
        (Position){
            value.position.x + translation.x,
            value.position.y + translation.y,
        },
        duration
    );
}

EngineResult graphics_camera_position_set(CameraId camera_id, Position position, Time duration) {
    return graphics_camera_start_motion(
        camera_id,
        position,
        duration
    );
}

EngineResult graphics_camera_position_from_entity_set(CameraId camera_id, Entity entity, Time duration) {
    EntityIndex index;
    if(!entity_index_get(entity, &index) || index >= positions_pool.capacity
            || !positions_pool.used[index]) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    return graphics_camera_start_motion(
        camera_id,
        positions[index],
        duration
    );
}

EngineResult graphics_camera_entity_attachment_set(CameraId camera_id, Entity entity) {
    CameraResult result = graphics_camera_get(camera_id);
    if(result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(result.result.error);
    }
    return graphics_camera_attachment_set(
        camera_id,
        entity,
        (Vec2D){0.0f, 0.0f},
        result.result.value.orientation,
        true,
        false
    );
}

EngineResult graphics_camera_moving_get(CameraId camera_id) {
    size_t slot;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    return error_result_value(camera_motion[slot].active);
}

EngineResult graphics_camera_zoom_set(
    CameraId camera_id,
    float zoom,
    Time duration
) {
    size_t slot;
    Tick duration_ticks;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(zoom <= 0.0f) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    if(duration <= 0.0) {
        cameras[slot].zoom = zoom;
        camera_zoom_motion[slot] = (CameraZoomMotion){0};
        if(camera_id == active_camera) camera.zoom = zoom;
        return error_result_value(true);
    }
    duration_ticks = (Tick)ceil(duration / engine_time_per_tick_get());
    if(duration_ticks == 0) duration_ticks = 1;
    camera_zoom_motion[slot] = (CameraZoomMotion){
        .start = cameras[slot].zoom,
        .target = zoom,
        .start_tick = engine_tick_get(),
        .duration_ticks = duration_ticks,
        .active = true,
    };
    return error_result_value(true);
}

CameraZoomResult graphics_camera_zoom_get(CameraId camera_id) {
    size_t slot;
    if(!graphics_camera_slot(camera_id, &slot)) {
        return ERROR_RESULT_MAKE_ERROR(CameraZoomResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(CameraZoomResult, cameras[slot].zoom);
}

ViewportConfig graphics_viewport_config_default_get(void) {
    return (ViewportConfig){
        .rectangle = {0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT},
        .fit = SCREEN_FIT_CONTAIN,
    };
}

ViewportIdResult graphics_viewport_create(ViewportConfig config) {
    size_t slot;
    ViewportConfig defaults = graphics_viewport_config_default_get();
    if(config.rectangle.width <= 0.0f) config.rectangle.width = defaults.rectangle.width;
    if(config.rectangle.height <= 0.0f) config.rectangle.height = defaults.rectangle.height;
    if(config.fit < SCREEN_FIT_NONE || config.fit > SCREEN_FIT_COVER) {
        config.fit = defaults.fit;
    }
    for(slot = 0; slot < MAX_VIEWPORTS; slot += 1) {
        if(viewports_used[slot]) continue;
        viewport_generations[slot] += 1;
        if(viewport_generations[slot] == 0) viewport_generations[slot] = 1;
        viewports_used[slot] = true;
        viewports[slot] = (GraphicsViewport){
            .rectangle = config.rectangle,
            .fit = config.fit,
            .camera = CAMERA_INVALID,
            .enabled = false,
        };
        return ERROR_RESULT_MAKE_VALUE(
            ViewportIdResult,
            graphics_resource_id(viewport_generations[slot], slot)
        );
    }
    return ERROR_RESULT_MAKE_ERROR(ViewportIdResult, ERROR_MEMORY_POOL_FULL);
}

EngineResult graphics_viewport_destroy(ViewportId id) {
    size_t slot;
    if(!graphics_viewport_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    viewports[slot] = (GraphicsViewport){0};
    viewports_used[slot] = false;
    return error_result_value(true);
}

EngineResult graphics_viewport_camera_set(ViewportId id, CameraId camera_id) {
    size_t slot;
    size_t camera_slot;
    if(!graphics_viewport_slot(id, &slot) || !graphics_camera_slot(camera_id, &camera_slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    viewports[slot].camera = camera_id;
    return error_result_value(true);
}

EngineResult graphics_viewport_camera_clear(ViewportId id) {
    size_t slot;
    if(!graphics_viewport_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    viewports[slot].camera = CAMERA_INVALID;
    return error_result_value(true);
}

EngineResult graphics_viewport_enable_set(ViewportId id) {
    size_t slot;
    if(!graphics_viewport_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    viewports[slot].enabled = true;
    return error_result_value(true);
}

EngineResult graphics_viewport_disable_set(ViewportId id) {
    size_t slot;
    if(!graphics_viewport_slot(id, &slot)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    viewports[slot].enabled = false;
    return error_result_value(true);
}

Position graphics_world_to_screen_get(Position world) {
    float output_x = 0.0f;
    float output_y = 0.0f;
    float output_width = WINDOW_WIDTH;
    float output_height = WINDOW_HEIGHT;
    size_t screen_slot;
    (void)graphics_resolve_camera_attachment();
    if(graphics_screen_slot(drawing_screen, &screen_slot)) {
        output_x = 0.0f;
        output_y = 0.0f;
        output_width = (float)screens[screen_slot].width;
        output_height = (float)screens[screen_slot].height;
    }
    Vec2D relative = {
        .x = world.x - camera.position.x,
        .y = world.y - camera.position.y
    };
    Vec2D camera_space = math_vector_rotate(relative, -camera.orientation);
    float scale_x = camera.zoom * output_width / camera.dimensions.x;
    float scale_y = camera.zoom * output_height / camera.dimensions.y;

    return (Position){
        .x = output_x + output_width * 0.5f + camera_space.x * scale_x,
        .y = output_y + output_height * 0.5f - camera_space.y * scale_y
    };
}

Position graphics_screen_to_world_get(Position screen) {
    float output_x = 0.0f;
    float output_y = 0.0f;
    float output_width = WINDOW_WIDTH;
    float output_height = WINDOW_HEIGHT;
    size_t screen_slot;
    (void)graphics_resolve_camera_attachment();
    if(graphics_screen_slot(drawing_screen, &screen_slot)) {
        output_x = 0.0f;
        output_y = 0.0f;
        output_width = (float)screens[screen_slot].width;
        output_height = (float)screens[screen_slot].height;
    }
    Vec2D camera_space = {
        .x = (screen.x - output_x - output_width * 0.5f)
            * camera.dimensions.x / (camera.zoom * output_width),
        .y = (output_y + output_height * 0.5f - screen.y)
            * camera.dimensions.y / (camera.zoom * output_height)
    };
    Vec2D relative = math_vector_rotate(camera_space, camera.orientation);

    return (Position){
        .x = relative.x + camera.position.x,
        .y = relative.y + camera.position.y
    };
}

Position graphics_window_to_screen_get(Position window) {
    float screen_x;
    float screen_y;

    if(sdl_renderer == NULL || !SDL_RenderCoordinatesFromWindow(
            sdl_renderer,
            window.x,
            window.y,
            &screen_x,
            &screen_y)) {
        return (Position){0};
    }
    return (Position){
        .x = screen_x,
        .y = screen_y,
    };
}

Position graphics_mouse_screen_position_get(void) {
    float window_x;
    float window_y;

    if(sdl_renderer == NULL) {
        return (Position){0};
    }
    SDL_GetMouseState(&window_x, &window_y);
    return graphics_window_to_screen_get((Position){window_x, window_y});
}

EngineResult graphics_start(void) {
    console_write(LOG_ENGINE, "---Initializing Graphics---\n");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return error_result_error(ERROR_ENGINE_GRAPHICS_INIT_FAILED);
    }
    if(!TTF_Init()) {
        return error_result_error(ERROR_ENGINE_GRAPHICS_INIT_FAILED);
    }
    ttf_initialized = true;

    console_write(LOG_ENGINE, "Starting game window and renderer\n");
    console_write(LOG_ENGINE, "Window width: %d\n", WINDOW_WIDTH);
    console_write(LOG_ENGINE, "Window height: %d\n", WINDOW_HEIGHT);
    if (!SDL_CreateWindowAndRenderer(
            "Game Test",
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            SDL_WINDOW_RESIZABLE,
            &sdl_window,
            &sdl_renderer
        )) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        TTF_Quit();
        ttf_initialized = false;
        return error_result_error(ERROR_ENGINE_GRAPHICS_INIT_FAILED);
    }

    console_write(LOG_ENGINE, "Configuring renderer\n");
    SDL_SetRenderLogicalPresentation(
        sdl_renderer,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_LOGICAL_PRESENTATION_LETTERBOX
    );
    ttf_text_engine = TTF_CreateRendererTextEngine(sdl_renderer);
    if(ttf_text_engine == NULL) {
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(sdl_window);
        sdl_renderer = NULL;
        sdl_window = NULL;
        TTF_Quit();
        ttf_initialized = false;
        return error_result_error(ERROR_ENGINE_GRAPHICS_INIT_FAILED);
    }
    (void)graphics_camera_active_set(active_camera);

    console_write(LOG_ENGINE, "Graphics initialization complete\n");
    console_write(LOG_ENGINE, "---Initializing Graphics---\n");
    return error_result_value(true);
}

void graphics_renderer_end(void) {
    size_t screen_slot;
    for(screen_slot = 0; screen_slot < MAX_SCREENS; screen_slot += 1) {
        if(screens_used[screen_slot]) {
            SDL_DestroyTexture(screens[screen_slot].texture);
            screens[screen_slot] = (GraphicsScreen){0};
            screens_used[screen_slot] = false;
        }
    }
    memset(viewports, 0, sizeof(viewports));
    memset(viewports_used, 0, sizeof(viewports_used));
    drawing_screen = SCREEN_INVALID;
    if(ttf_text_engine != NULL) {
        TTF_DestroyRendererTextEngine(ttf_text_engine);
        ttf_text_engine = NULL;
    }
    if(ttf_initialized) {
        TTF_Quit();
        ttf_initialized = false;
    }
    SDL_DestroyRenderer(sdl_renderer);
    sdl_renderer = NULL;
    console_write(LOG_ENGINE, "Renderer terminated\n");
}

void graphics_window_end(void) {
    SDL_DestroyWindow(sdl_window);
    sdl_window = NULL;
    console_write(LOG_ENGINE, "Window terminated\n");
}

void graphics_end(void) {
    console_write(LOG_ENGINE, "---Graphics Termination---\n");
    graphics_recording_stop();
    graphics_renderer_end();
    graphics_window_end();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    console_write(LOG_ENGINE, "SDL3 terminated\n");
    console_write(LOG_ENGINE, "Graphics termination complete\n");
    console_write(LOG_ENGINE, "---Graphics Termination---\n");

}

bool graphics_events_poll(SDL_Event *event) {
    while (SDL_PollEvent(event)) {
        if (event->type == SDL_EVENT_QUIT) {
            return false;
        }
    }
    return true;
}

void graphics_background_draw(Color color) {
    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(sdl_renderer, color.red, color.green, color.blue, color.alpha);
    SDL_RenderClear(sdl_renderer);  /* start with a blank canvas. */
}

bool graphics_screen_rect_draw(float x, float y, float width, float height, Color color) {
    SDL_FRect rect = {
        .x = x,
        .y = y,
        .w = width,
        .h = height,
    };

    if(sdl_renderer == NULL || width <= 0.0f || height <= 0.0f) {
        return false;
    }
    if(!SDL_SetRenderDrawColor(sdl_renderer, color.red, color.green, color.blue, color.alpha)) {
        return false;
    }
    return SDL_RenderFillRect(sdl_renderer, &rect);
}

bool graphics_screen_clip_set(float x, float y, float width, float height) {
    SDL_Rect clip;
    int left;
    int top;
    int right;
    int bottom;

    if(sdl_renderer == NULL || width <= 0.0f || height <= 0.0f) return false;
    left = (int)ceilf(x);
    top = (int)ceilf(y);
    right = (int)floorf(x + width);
    bottom = (int)floorf(y + height);
    if(right <= left || bottom <= top) return false;
    clip = (SDL_Rect){left, top, right - left, bottom - top};
    if(!SDL_SetRenderClipRect(sdl_renderer, &clip)) return false;
    graphics_clip_state = (GraphicsClipState){.rectangle = clip, .active = true};
    graphics_clip_stack_count = 0;
    return true;
}

void graphics_screen_clip_clear(void) {
    if(sdl_renderer != NULL) (void)SDL_SetRenderClipRect(sdl_renderer, NULL);
    graphics_clip_state = (GraphicsClipState){0};
    graphics_clip_stack_count = 0;
}

bool graphics_screen_clip_push(float x, float y, float width, float height) {
    SDL_Rect clip;
    int left;
    int top;
    int right;
    int bottom;

    if(sdl_renderer == NULL || width <= 0.0f || height <= 0.0f ||
            graphics_clip_stack_count >= GRAPHICS_CLIP_STACK_MAX) return false;
    left = (int)ceilf(x);
    top = (int)ceilf(y);
    right = (int)floorf(x + width);
    bottom = (int)floorf(y + height);
    if(graphics_clip_state.active) {
        left = left > graphics_clip_state.rectangle.x ?
            left : graphics_clip_state.rectangle.x;
        top = top > graphics_clip_state.rectangle.y ?
            top : graphics_clip_state.rectangle.y;
        right = right < graphics_clip_state.rectangle.x +
            graphics_clip_state.rectangle.w ? right :
            graphics_clip_state.rectangle.x + graphics_clip_state.rectangle.w;
        bottom = bottom < graphics_clip_state.rectangle.y +
            graphics_clip_state.rectangle.h ? bottom :
            graphics_clip_state.rectangle.y + graphics_clip_state.rectangle.h;
    }
    graphics_clip_stack[graphics_clip_stack_count++] = graphics_clip_state;
    clip = (SDL_Rect){left, top,
        right > left ? right - left : 0,
        bottom > top ? bottom - top : 0};
    if(!SDL_SetRenderClipRect(sdl_renderer, &clip)) {
        graphics_clip_stack_count -= 1;
        return false;
    }
    graphics_clip_state = (GraphicsClipState){.rectangle = clip, .active = true};
    return true;
}

void graphics_screen_clip_pop(void) {
    if(sdl_renderer == NULL || graphics_clip_stack_count == 0) return;
    graphics_clip_state = graphics_clip_stack[--graphics_clip_stack_count];
    (void)SDL_SetRenderClipRect(sdl_renderer,
        graphics_clip_state.active ? &graphics_clip_state.rectangle : NULL);
}

bool graphics_screen_quad_draw(
    Position center,
    float width,
    float height,
    float angle,
    Color color
) {
    SDL_Vertex vertices[4] = {0};
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    float half_width = width * 0.5f;
    float half_height = height * 0.5f;
    Vec2D axis = {cosf(angle), -sinf(angle)};
    Vec2D perpendicular = {sinf(angle), cosf(angle)};
    const float signs[4][2] = {
        {-1.0f, -1.0f},
        { 1.0f, -1.0f},
        { 1.0f,  1.0f},
        {-1.0f,  1.0f},
    };
    int i;

    if(sdl_renderer == NULL || width <= 0.0f || height <= 0.0f
            || !isfinite(angle)) {
        return false;
    }
    for(i = 0; i < 4; i += 1) {
        vertices[i].position.x = center.x
            + axis.x * half_width * signs[i][0]
            + perpendicular.x * half_height * signs[i][1];
        vertices[i].position.y = center.y
            + axis.y * half_width * signs[i][0]
            + perpendicular.y * half_height * signs[i][1];
        vertices[i].color = (SDL_FColor){
            color.red / 255.0f,
            color.green / 255.0f,
            color.blue / 255.0f,
            color.alpha / 255.0f,
        };
    }
    return SDL_RenderGeometry(sdl_renderer, NULL, vertices, 4, indices, 6);
}

void graphics_rect_draw(Shape rect, Position pos) {
    SDL_FRect sdl_rect;
    (void)rect;
    /* draw a filled rectangle in the middle of the canvas. */
    SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 255, SDL_ALPHA_OPAQUE);  /* blue, full alpha */
    Position screen_loc = graphics_world_to_screen_get(pos);
    sdl_rect.x = screen_loc.x;
    sdl_rect.y = screen_loc.y;
    sdl_rect.w = 20;
    sdl_rect.h = 20;
    SDL_RenderFillRect(sdl_renderer, &sdl_rect);
}

static void graphics_empty_viewport_draw(ViewportRectangle rectangle) {
    SDL_FRect background = {
        rectangle.x,
        rectangle.y,
        rectangle.width,
        rectangle.height,
    };
    float offset;
    SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    (void)SDL_RenderFillRect(sdl_renderer, &background);
    SDL_SetRenderDrawColor(sdl_renderer, 45, 45, 45, SDL_ALPHA_OPAQUE);
    for(offset = -rectangle.height; offset < rectangle.width; offset += 24.0f) {
        float start_x = rectangle.x + (offset < 0.0f ? 0.0f : offset);
        float start_y = rectangle.y + rectangle.height + (offset < 0.0f ? offset : 0.0f);
        float end_x = rectangle.x + (offset + rectangle.height > rectangle.width
            ? rectangle.width
            : offset + rectangle.height);
        float end_y = rectangle.y + rectangle.height
            - (end_x - rectangle.x - offset);
        (void)SDL_RenderLine(sdl_renderer, start_x, start_y, end_x, end_y);
    }
}

static void graphics_camera_motions_update(void) {
    Tick current_tick = engine_tick_get();
    size_t slot;
    for(slot = 0; slot < MAX_CAMERAS; slot += 1) {
        if(!cameras_used[slot]) continue;
        if(camera_motion[slot].active) {
            CameraMotion *motion = &camera_motion[slot];
            Position target = motion->target;
            Tick elapsed = current_tick - motion->start_tick;
            float progress = elapsed >= motion->duration_ticks
                ? 1.0f
                : (float)elapsed / (float)motion->duration_ticks;
            cameras[slot].position = (Position){
                .x = motion->start.x + (target.x - motion->start.x) * progress,
                .y = motion->start.y + (target.y - motion->start.y) * progress,
            };
            if(graphics_camera_id(slot) == active_camera) {
                camera.position = cameras[slot].position;
            }
            if(progress >= 1.0f) *motion = (CameraMotion){0};
        }
        if(camera_zoom_motion[slot].active) {
            CameraZoomMotion *motion = &camera_zoom_motion[slot];
            Tick elapsed = current_tick - motion->start_tick;
            float progress = elapsed >= motion->duration_ticks
                ? 1.0f
                : (float)elapsed / (float)motion->duration_ticks;
            cameras[slot].zoom = motion->start
                + (motion->target - motion->start) * progress;
            if(graphics_camera_id(slot) == active_camera) {
                camera.zoom = cameras[slot].zoom;
            }
            if(progress >= 1.0f) *motion = (CameraZoomMotion){0};
        }
    }
}

static void graphics_render_viewport_cameras(void) {
    bool rendered[MAX_CAMERAS] = {0};
    size_t viewport_slot;
    for(viewport_slot = 0; viewport_slot < MAX_VIEWPORTS; viewport_slot += 1) {
        CameraId camera_id;
        CameraRuntime *runtime;
        size_t camera_slot;
        if(!viewports_used[viewport_slot] || !viewports[viewport_slot].enabled) continue;
        camera_id = viewports[viewport_slot].camera;
        if(!graphics_camera_slot(camera_id, &camera_slot) || rendered[camera_slot]) continue;
        rendered[camera_slot] = true;
        runtime = &camera_runtime[camera_slot];
        if(!runtime->enabled || runtime->callback == NULL
                || (runtime->pause_with_engine && engine_paused_get())) continue;
        if(graphics_camera_begin(camera_id).kind == ERROR_RESULT_ERROR) continue;
        runtime->callback(camera_id, runtime->context);
        (void)graphics_camera_end();
    }
}

static void graphics_viewports_draw(void) {
    size_t viewport_slot;
    bool has_enabled_viewport = false;
    (void)SDL_SetRenderTarget(sdl_renderer, NULL);
    (void)SDL_SetRenderViewport(sdl_renderer, NULL);
    (void)SDL_SetRenderClipRect(sdl_renderer, NULL);
    for(viewport_slot = 0; viewport_slot < MAX_VIEWPORTS; viewport_slot += 1) {
        if(viewports_used[viewport_slot] && viewports[viewport_slot].enabled) {
            has_enabled_viewport = true;
            break;
        }
    }
    if(!has_enabled_viewport) return;
    SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    (void)SDL_RenderClear(sdl_renderer);
    for(viewport_slot = 0; viewport_slot < MAX_VIEWPORTS; viewport_slot += 1) {
        GraphicsViewport *viewport;
        GraphicsScreen *screen;
        SDL_Rect clip;
        SDL_FRect destination;
        size_t screen_slot;
        float scale_x;
        float scale_y;
        float scale;
        if(!viewports_used[viewport_slot] || !viewports[viewport_slot].enabled) continue;
        viewport = &viewports[viewport_slot];
        clip = (SDL_Rect){
            (int)viewport->rectangle.x,
            (int)viewport->rectangle.y,
            (int)viewport->rectangle.width,
            (int)viewport->rectangle.height,
        };
        (void)SDL_SetRenderClipRect(sdl_renderer, &clip);
        if(!graphics_screen_for_camera(viewport->camera, &screen_slot)) {
            graphics_empty_viewport_draw(viewport->rectangle);
            continue;
        }
        SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        {
            SDL_FRect background = {
                viewport->rectangle.x,
                viewport->rectangle.y,
                viewport->rectangle.width,
                viewport->rectangle.height,
            };
            (void)SDL_RenderFillRect(sdl_renderer, &background);
        }
        screen = &screens[screen_slot];
        destination = (SDL_FRect){
            viewport->rectangle.x,
            viewport->rectangle.y,
            viewport->rectangle.width,
            viewport->rectangle.height,
        };
        scale_x = viewport->rectangle.width / (float)screen->width;
        scale_y = viewport->rectangle.height / (float)screen->height;
        if(viewport->fit == SCREEN_FIT_NONE) {
            destination.w = (float)screen->width;
            destination.h = (float)screen->height;
        } else if(viewport->fit != SCREEN_FIT_STRETCH) {
            scale = viewport->fit == SCREEN_FIT_COVER
                ? fmaxf(scale_x, scale_y)
                : fminf(scale_x, scale_y);
            destination.w = (float)screen->width * scale;
            destination.h = (float)screen->height * scale;
        }
        destination.x += (viewport->rectangle.width - destination.w) * 0.5f;
        destination.y += (viewport->rectangle.height - destination.h) * 0.5f;
        (void)SDL_RenderTexture(sdl_renderer, screen->texture, NULL, &destination);
    }
    (void)SDL_SetRenderClipRect(sdl_renderer, NULL);
}

void graphics_show(void) {
    graphics_camera_motions_update();
    graphics_render_viewport_cameras();
    graphics_viewports_draw();
      if(screen_recorder.recording) {
        if(!graphics_record_frame()) {
            graphics_recording_stop();
        }
    }
    SDL_RenderPresent(sdl_renderer);
}

bool graphics_shape_outline_draw(Shape shape, Color color) {
    if (shape.amount_of_vertices < 2) {
        return false;
    }

    SDL_FPoint points[MAX_VERTICIES + 1];

    for (int i = 0; i < shape.amount_of_vertices; i++) {
        Position screen_loc = graphics_world_to_screen_get(shape.vertices[i]);
        points[i].x = screen_loc.x;
        points[i].y = screen_loc.y;
    }

    points[shape.amount_of_vertices] = points[0];

    SDL_SetRenderDrawColor(sdl_renderer, color.red, color.green, color.blue, color.alpha);
    return SDL_RenderLines(sdl_renderer, points, shape.amount_of_vertices + 1);
}

bool graphics_shape_filled_draw(Shape shape, Color color)
{
    if (shape.amount_of_vertices < 3) {
        return false;
    }

    SDL_Vertex vertices[MAX_VERTICIES];

    for (int i = 0; i < shape.amount_of_vertices; i++) {
        Position screen_loc = graphics_world_to_screen_get(shape.vertices[i]);
        vertices[i].position.x = screen_loc.x;
        vertices[i].position.y = screen_loc.y;

        vertices[i].color.r = color.red / 255.0f;
        vertices[i].color.g = color.green / 255.0f;
        vertices[i].color.b = color.blue / 255.0f;
        vertices[i].color.a = color.alpha / 255.0f;

        vertices[i].tex_coord.x = 0.0f;
        vertices[i].tex_coord.y = 0.0f;
    }

    int indices[(MAX_VERTICIES - 2) * 3];
    int index_count = 0;

    for (int i = 1; i < shape.amount_of_vertices - 1; i++) {
        indices[index_count++] = 0;
        indices[index_count++] = i;
        indices[index_count++] = i + 1;
    }

    return SDL_RenderGeometry(
        sdl_renderer,
        NULL,
        vertices,
        shape.amount_of_vertices,
        indices,
        index_count
    );
}

void graphics_hit_box_draw(Entity entity, Fill fill_type) {
    ShapeResult shape_result = physics_global_hit_box_get(entity);
    if(shape_result.kind == ERROR_RESULT_ERROR) {
        return;
    }
    Shape shape = shape_result.result.value;
    if(fill_type == GRAPHICS_FILLED) {
        graphics_shape_filled_draw(shape, hit_box_color);
    }
    else {
        graphics_shape_outline_draw(shape, hit_box_color);
    }
}

void graphics_hit_box_colored_draw(Entity entity, Fill fill_type, Color color) {
    ShapeResult shape_result = physics_global_hit_box_get(entity);
    if(shape_result.kind == ERROR_RESULT_ERROR) {
        return;
    }
    Shape shape = shape_result.result.value;
    if(fill_type == GRAPHICS_FILLED) {
        graphics_shape_filled_draw(shape, color);
    }
    else {
        graphics_shape_outline_draw(shape, color);
    }
}

void graphics_hit_boxes_draw(void) {
  for(int i = 0; i < MAX_ENTITIES; i += 1) {
    if(entity_index_alive_check(i)) {
        if( entity_index_components_check(i, HIT_BOX)) {
            EntityResult entity_result = entity_from_index_get(i);
            if(entity_result.kind == ERROR_RESULT_VALUE) {
                graphics_hit_box_draw(entity_result.result.value, GRAPHICS_OUTLINE);
            }
        }
    }
  }
}

void graphics_particle_draw(Entity entity, Fill fill_type) {
    EntityIndex index;

    if(!entity_index_get(entity, &index)) {
        return;
    }
    ShapeResult shape_result = physics_global_hit_box_get(entity);
    if(shape_result.kind == ERROR_RESULT_ERROR) {
        return;
    }
    Shape shape = shape_result.result.value;
    float radius = math_circle_radius(shape,math_polygon_centroid(shape));
    Shape circle = math_circle_create(radius, 10);
    Shape world_circle = physics_shape_world_translate(circle, positions[index], 0);
    if(fill_type == GRAPHICS_FILLED) {
        graphics_shape_filled_draw(world_circle, particle_color);
    }
    else {
        graphics_shape_outline_draw(world_circle, particle_color);
    }
}
void graphics_particles_draw(void) {
  for(int i = 0; i < MAX_ENTITIES; i += 1) {
    if(entity_index_alive_check(i)) {
        if( entity_index_components_check(i, HIT_BOX)) {
          if( entity_index_components_check(i, PARTICLE)) {
              EntityResult entity_result = entity_from_index_get(i);
              if(entity_result.kind == ERROR_RESULT_VALUE) {
                  graphics_particle_draw(entity_result.result.value, GRAPHICS_OUTLINE);
              }
          }
        }
    }
  }
}
TextureAssetResult graphics_texture_load(TextureDescriptor text_desc) {
        SDL_Surface *surface = NULL;
        char *png_path = NULL;
        TextureAsset asset = {0};
        asset.size = (Scale){
            .x = text_desc.size.x,
            .y = text_desc.size.y,
        };

        SDL_asprintf(&png_path, "%s", text_desc.file);
        surface = SDL_LoadPNG(png_path);
        SDL_free(png_path);
        if(surface == NULL) {
            return ERROR_RESULT_MAKE_ERROR(TextureAssetResult, ERROR_ENGINE_TEXTURE_LOAD_FAILED);
        }

        asset.texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
        SDL_DestroySurface(surface);  /* done with this, the texture has a copy of the pixels now. */
        if(asset.texture == NULL) {
            return ERROR_RESULT_MAKE_ERROR(TextureAssetResult, ERROR_ENGINE_TEXTURE_LOAD_FAILED);
        }

        return ERROR_RESULT_MAKE_VALUE(TextureAssetResult, asset);
}

FontAssetResult graphics_font_load(FontDescriptor descriptor) {
    FontAsset asset = {0};

    if(descriptor.file == NULL || descriptor.point_size <= 0.0f || !ttf_initialized) {
        return ERROR_RESULT_MAKE_ERROR(FontAssetResult, ERROR_ENGINE_FONT_LOAD_FAILED);
    }
    asset.font = TTF_OpenFont(descriptor.file, descriptor.point_size);
    if(asset.font == NULL) {
        return ERROR_RESULT_MAKE_ERROR(FontAssetResult, ERROR_ENGINE_FONT_LOAD_FAILED);
    }
    return ERROR_RESULT_MAKE_VALUE(FontAssetResult, asset);
}

void graphics_font_destroy(FontAsset *font) {
    if(font == NULL || font->font == NULL) {
        return;
    }
    TTF_CloseFont(font->font);
    *font = (FontAsset){0};
}

TextAssetResult graphics_text_create(const FontAsset *font, const char *value, Color color) {
    TextAsset asset = {0};
    int width;
    int height;

    if(font == NULL || font->font == NULL || value == NULL || ttf_text_engine == NULL) {
        return ERROR_RESULT_MAKE_ERROR(TextAssetResult, ERROR_ENGINE_TEXT_CREATE_FAILED);
    }
    asset.text = TTF_CreateText(ttf_text_engine, font->font, value, 0);
    if(asset.text == NULL || !TTF_SetTextColor(
            asset.text,
            color.red,
            color.green,
            color.blue,
            color.alpha) || !TTF_GetTextSize(asset.text, &width, &height)) {
        if(asset.text != NULL) {
            TTF_DestroyText(asset.text);
        }
        return ERROR_RESULT_MAKE_ERROR(TextAssetResult, ERROR_ENGINE_TEXT_CREATE_FAILED);
    }
    asset.size = (Scale){
        .x = (float)width,
        .y = (float)height,
    };
    return ERROR_RESULT_MAKE_VALUE(TextAssetResult, asset);
}

bool graphics_text_value_set(TextAsset *text, const char *value) {
    int width;
    int height;

    if(text == NULL || text->text == NULL || value == NULL ||
            !TTF_SetTextString(text->text, value, 0) ||
            !TTF_GetTextSize(text->text, &width, &height)) return false;
    text->size = (Scale){.x = (float)width, .y = (float)height};
    return true;
}

void graphics_text_destroy(TextAsset *text) {
    if(text == NULL) {
        return;
    }
    if(text->text != NULL) {
        TTF_DestroyText(text->text);
    }
    *text = (TextAsset){0};
}

bool graphics_text_draw(const TextAsset *text, Position position) {
    if(text == NULL || text->text == NULL) {
        return false;
    }
    return TTF_DrawRendererText(text->text, position.x, position.y);
}

AnimationAssetResult graphics_animation_load(AnimationDescriptor anim_desc) {
    AnimationAsset asset = {0};
    asset.texture_list.amount = anim_desc.amount_of_descriptors;
    asset.ticks_per_frame = anim_desc.ticks_per_frame;
    asset.time_per_frame = anim_desc.time_per_frame;

    for(int i = 0; i < anim_desc.amount_of_descriptors; i += 1) {
        TextureAssetResult texture_result = graphics_texture_load(anim_desc.texture_descriptors[i]);
        if(texture_result.kind == ERROR_RESULT_ERROR) {
            return ERROR_RESULT_MAKE_ERROR(AnimationAssetResult, texture_result.result.error);
        }
        asset.texture_list.textures[i] = texture_result.result.value;
    }

    return ERROR_RESULT_MAKE_VALUE(AnimationAssetResult, asset);
}

AnimatedSprite graphics_animated_sprite_create(AnimationAsset asset_ptr, Scale scale) {
    AnimatedSprite sprite = {0};
    sprite.animation = asset_ptr;
    sprite.animation_frame = 0;
    sprite.direction = DIRECTION_RIGHT;
    sprite.scale = scale;
    sprite.last_update_tick = 0;
    sprite.last_update_time = 0;

    return sprite;
}

void graphics_sprite_frame_update(AnimatedSprite *sprite, Tick current_tick, Time current_time) {
    bool frame_need_update_tick = (sprite->animation.ticks_per_frame <= (current_tick - sprite->last_update_tick)) && (sprite->animation.ticks_per_frame != 0);
    bool frame_need_update_time = (sprite->animation.time_per_frame <= (current_time - sprite->last_update_time) && (sprite->animation.time_per_frame != 0));

    if(frame_need_update_tick || frame_need_update_time) {
        sprite->animation_frame = (sprite->animation_frame + 1)%sprite->animation.texture_list.amount;
        sprite->last_update_tick = current_tick;
        sprite->last_update_time = current_time;
    }
}

void graphics_texture_draw(TextureAsset texture_asset, Position pos, Orientation ort) {
    SDL_FRect dst_rect = {0};
    float output_width = WINDOW_WIDTH;
    float output_height = WINDOW_HEIGHT;
    size_t screen_slot;
    (void)graphics_resolve_camera_attachment();
    if(graphics_screen_slot(drawing_screen, &screen_slot)) {
        output_width = (float)screens[screen_slot].width;
        output_height = (float)screens[screen_slot].height;
    }
    Position screen_loc = graphics_world_to_screen_get(pos);
    dst_rect.w = texture_asset.size.x * camera.zoom
        * output_width / camera.dimensions.x;
    dst_rect.h = texture_asset.size.y * camera.zoom
        * output_height / camera.dimensions.y;
    dst_rect.x = screen_loc.x - dst_rect.w * 0.5f;//(float) texture_width;
    dst_rect.y = screen_loc.y - dst_rect.h * 0.5f;//(float) texture_height;

    SDL_FPoint center = {
        .x = dst_rect.w * 0.5f,
        .y = dst_rect.h * 0.5f
    };
    double degrees = -(double)(ort - camera.orientation) * 180.0 / (double)PI_F;
    SDL_RenderTextureRotated(
        sdl_renderer,
    texture_asset.texture,
    NULL,
    &dst_rect,
    degrees,
    &center,
    SDL_FLIP_NONE
    );
}

void graphics_sprite_draw(AnimatedSprite sprite, Position pos, Orientation ort) {
    TextureAsset asset = {0};
    asset = sprite.animation.texture_list.textures[sprite.animation_frame];
    asset.size.x = asset.size.x * sprite.scale.x;
    asset.size.y = asset.size.y * sprite.scale.y;

    graphics_texture_draw(asset, pos, ort);
}

EngineResult graphics_animated_sprite_add(Entity entity, AnimatedSprite sprite) {
    EntityIndex index;
    EngineResult result;

    if(!entity_index_get(entity, &index) || !entity_index_alive_check(index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    (void)AnimatedSpritePool_store_at(&animated_sprites_pool, index, sprite);
    result = entity_components_add(entity, ANIMATED_SPRITE);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    return error_result_value(true);
}

void graphics_animated_sprites_draw(void) {
    RohrComponentMask filter = ANIMATED_SPRITE;
    for(int i = 0; i < MAX_ENTITIES; i += 1) {
        if(entity_index_alive_check(i) && entity_index_components_check(i, filter)) {
            graphics_sprite_draw(animated_sprites[i], positions[i], orientations[i]);
        }
    }
}

void graphics_sprite_frames_update(Tick current_tick, Time current_time) {
    RohrComponentMask filter = ANIMATED_SPRITE;
    for(int i = 0; i < MAX_ENTITIES; i += 1) {
        if(entity_index_alive_check(i) && entity_index_components_check(i, filter)) {
            graphics_sprite_frame_update(&animated_sprites[i], current_tick, current_time);
        }
    }
}

void graphics_local_origin_draw(Entity entity) {
    EntityIndex index;

    if(!entity_index_get(entity, &index)) {
        return;
    }
    if (!entity_components_check(entity, HIT_BOX)) {
        return;
    }

    ShapeResult shape_result = physics_global_hit_box_get(entity);
    if(shape_result.kind == ERROR_RESULT_ERROR) {
        return;
    }
    Shape global_shape = shape_result.result.value;

    if (global_shape.amount_of_vertices <= 0) {
        return;
    }

    Position origin = positions[index];
    Orientation angle = orientations[index];

    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);

    Vec2D local_x_axis = {
        .x = cos_angle,
        .y = sin_angle
    };

    Vec2D local_y_axis = {
        .x = -sin_angle,
        .y = cos_angle
    };

    float max_x_projection = 0.0f;
    float max_y_projection = 0.0f;

    for (int i = 0; i < global_shape.amount_of_vertices; i++) {
        Vec2D relative_vertex = {
            .x = global_shape.vertices[i].x - origin.x,
            .y = global_shape.vertices[i].y - origin.y
        };

        float x_projection =
            relative_vertex.x * local_x_axis.x +
            relative_vertex.y * local_x_axis.y;

        float y_projection =
            relative_vertex.x * local_y_axis.x +
            relative_vertex.y * local_y_axis.y;

        if (x_projection > max_x_projection) {
            max_x_projection = x_projection;
        }

        if (y_projection > max_y_projection) {
            max_y_projection = y_projection;
        }
    }

    Position x_positive = {
        .x = origin.x + local_x_axis.x * max_x_projection,
        .y = origin.y + local_x_axis.y * max_x_projection
    };

    Position y_positive = {
        .x = origin.x + local_y_axis.x * max_y_projection,
        .y = origin.y + local_y_axis.y * max_y_projection
    };

    Position screen_origin =
        graphics_world_to_screen_get(origin);

    Position screen_x_positive =
        graphics_world_to_screen_get(x_positive);

    Position screen_y_positive =
        graphics_world_to_screen_get(y_positive);

    /* Positive local X axis */
    SDL_SetRenderDrawColor(
        sdl_renderer,
        255, 255, 0, 255
    );

    SDL_RenderLine(
        sdl_renderer,
        screen_origin.x,
        screen_origin.y,
        screen_x_positive.x,
        screen_x_positive.y
    );

    /* Positive local Y axis */
    SDL_SetRenderDrawColor(
        sdl_renderer,
        0, 255, 255, 255
    );

    SDL_RenderLine(
        sdl_renderer,
        screen_origin.x,
        screen_origin.y,
        screen_y_positive.x,
        screen_y_positive.y
    );
}

void graphics_local_origins_draw(void) {
    for(int i = 0; i < MAX_ENTITIES; i += 1) {
        if(!entity_index_alive_check(i)) {
            continue;
        }
        if (!entity_index_components_check(i, HIT_BOX)) {
            continue;
        }
        EntityResult entity_result = entity_from_index_get(i);
        if(entity_result.kind == ERROR_RESULT_VALUE) {
            graphics_local_origin_draw(entity_result.result.value);
        }
    }
}

static bool graphics_joint_world_anchors_get(Joint joint, Position *anchor_a, Position *anchor_b) {
    EntityIndex a_index;
    EntityIndex b_index;

    if(anchor_a == NULL || anchor_b == NULL) return false;
    if(joint.anchor_a != JOINT_ANCHOR_INVALID && joint.anchor_b != JOINT_ANCHOR_INVALID) {
        JointAnchorPositionResult first = physics_joint_anchor_world_position_get(joint.anchor_a);
        JointAnchorPositionResult second = physics_joint_anchor_world_position_get(joint.anchor_b);
        if(first.kind == ERROR_RESULT_ERROR || second.kind == ERROR_RESULT_ERROR) return false;
        *anchor_a = first.result.value;
        *anchor_b = second.result.value;
        return true;
    }
    if(!entity_index_get(joint.a, &a_index) || !entity_index_alive_check(a_index) ||
            !entity_index_get(joint.b, &b_index) || !entity_index_alive_check(b_index)) return false;
    {
        Vec2D offset_a = math_vector_rotate(joint.local_anchor_a, orientations[a_index]);
        Vec2D offset_b = math_vector_rotate(joint.local_anchor_b, orientations[b_index]);
        *anchor_a = (Position){positions[a_index].x + offset_a.x, positions[a_index].y + offset_a.y};
        *anchor_b = (Position){positions[b_index].x + offset_b.x, positions[b_index].y + offset_b.y};
    }
    return true;
}

static bool graphics_joint_pin_symbol_draw(Position center) {
    SDL_FPoint ring[17];
    SDL_FRect dot = {.x = center.x - 3.0f, .y = center.y - 3.0f, .w = 6.0f, .h = 6.0f};

    for(uint32_t i = 0; i < 16; i += 1) {
        float angle = (2.0f * PI_F * (float)i) / 16.0f;
        ring[i] = (SDL_FPoint){center.x + cosf(angle) * 9.0f, center.y + sinf(angle) * 9.0f};
    }
    ring[16] = ring[0];
    return SDL_RenderFillRect(sdl_renderer, &dot) && SDL_RenderLines(sdl_renderer, ring, 17);
}

static bool graphics_joint_weld_symbol_draw(Position center) {
    const float radius = 9.0f;
    SDL_FPoint box[] = {
        {center.x - radius, center.y - radius},
        {center.x + radius, center.y - radius},
        {center.x + radius, center.y + radius},
        {center.x - radius, center.y + radius},
        {center.x - radius, center.y - radius}
    };
    SDL_FPoint diagonal_a[] = {
        {center.x - radius, center.y - radius},
        {center.x + radius, center.y + radius}
    };
    SDL_FPoint diagonal_b[] = {
        {center.x + radius, center.y - radius},
        {center.x - radius, center.y + radius}
    };

    return SDL_RenderLines(sdl_renderer, box, 5) &&
        SDL_RenderLines(sdl_renderer, diagonal_a, 2) &&
        SDL_RenderLines(sdl_renderer, diagonal_b, 2);
}

static bool graphics_joint_spring_symbol_draw(Position start, Position end) {
    SDL_FPoint points[10];
    Vec2D delta = {.x = end.x - start.x, .y = end.y - start.y};
    float length = math_vector_magnitude(delta);
    Vec2D perpendicular;

    if(length <= 0.001f) return graphics_joint_pin_symbol_draw(start);
    perpendicular = (Vec2D){.x = -delta.y / length, .y = delta.x / length};
    for(uint32_t i = 0; i < 10; i += 1) {
        float t = (float)i / 9.0f;
        float offset = i == 0 || i == 9 ? 0.0f : (i % 2 == 0 ? 7.0f : -7.0f);
        points[i] = (SDL_FPoint){
            .x = start.x + delta.x * t + perpendicular.x * offset,
            .y = start.y + delta.y * t + perpendicular.y * offset
        };
    }
    return SDL_RenderLines(sdl_renderer, points, 10);
}

bool graphics_joint_draw(Entity joint_entity, Color color) {
    EntityIndex index;
    Joint joint;
    Position world_a;
    Position world_b;
    Position screen_a;
    Position screen_b;
    Position center;

    if(sdl_renderer == NULL || !entity_index_get(joint_entity, &index) ||
            !entity_index_alive_check(index) || !entity_index_components_check(index, JOINT) ||
            index >= joints_pool.capacity || !joints_pool.used[index]) return false;
    joint = joints[index];
    if(!graphics_joint_world_anchors_get(joint, &world_a, &world_b)) return false;
    screen_a = graphics_world_to_screen_get(world_a);
    screen_b = graphics_world_to_screen_get(world_b);
    center = (Position){(screen_a.x + screen_b.x) * 0.5f, (screen_a.y + screen_b.y) * 0.5f};
    if(!SDL_SetRenderDrawColor(sdl_renderer, color.red, color.green, color.blue, color.alpha)) return false;
    switch(joint.type) {
        case JOINT_SPRING:
            return graphics_joint_spring_symbol_draw(screen_a, screen_b);
        case JOINT_PIN:
            return graphics_joint_pin_symbol_draw(center);
        case JOINT_WELD:
            return graphics_joint_weld_symbol_draw(center);
        default:
            return false;
    }
}

void graphics_joints_draw(Color color) {
    for(EntityIndex index = 0; index < joints_pool.capacity; index += 1) {
        EntityResult joint;

        if(!joints_pool.used[index] || !entity_index_alive_check(index) ||
                !entity_index_components_check(index, JOINT)) continue;
        joint = entity_from_index_get(index);
        if(joint.kind == ERROR_RESULT_VALUE) (void)graphics_joint_draw(joint.result.value, color);
    }
}

bool graphics_soft_body_draw(Entity soft_body_entity, Color surface_color,
        Color beam_color, Color node_color) {
    SoftBodyResult body_result = physics_soft_body_get(soft_body_entity);
    SoftBody body;

    if(sdl_renderer == NULL || body_result.kind == ERROR_RESULT_ERROR) return false;
    body = body_result.result.value;
    for(uint32_t i = 0; i < body.triangle_count; i += 1) {
        SoftBodyTriangleResult triangle = physics_soft_body_triangle_get(body.triangles[i]);
        EntityIndex indices[3];
        Shape shape = {.amount_of_vertices = 3};
        if(triangle.kind == ERROR_RESULT_ERROR ||
                !entity_index_get(triangle.result.value.node_a, &indices[0]) ||
                !entity_index_get(triangle.result.value.node_b, &indices[1]) ||
                !entity_index_get(triangle.result.value.node_c, &indices[2])) continue;
        for(uint32_t vertex = 0; vertex < 3; vertex += 1) shape.vertices[vertex] = positions[indices[vertex]];
        (void)graphics_shape_filled_draw(shape, surface_color);
    }
    if(!SDL_SetRenderDrawColor(sdl_renderer, beam_color.red, beam_color.green,
            beam_color.blue, beam_color.alpha)) return false;
    for(uint32_t i = 0; i < body.beam_count; i += 1) {
        SoftBodyBeamResult beam = physics_soft_body_beam_get(body.beams[i]);
        EntityIndex a;
        EntityIndex b;
        Position screen_a;
        Position screen_b;
        if(beam.kind == ERROR_RESULT_ERROR || !entity_index_get(beam.result.value.node_a, &a) ||
                !entity_index_get(beam.result.value.node_b, &b)) continue;
        screen_a = graphics_world_to_screen_get(positions[a]);
        screen_b = graphics_world_to_screen_get(positions[b]);
        (void)SDL_RenderLine(sdl_renderer, screen_a.x, screen_a.y, screen_b.x, screen_b.y);
    }
    for(uint32_t i = 0; i < body.node_count; i += 1) {
        SoftBodyNodeResult node = physics_soft_body_node_get(body.nodes[i]);
        EntityIndex index;
        Shape shape;
        if(node.kind == ERROR_RESULT_ERROR || !entity_index_get(body.nodes[i], &index)) continue;
        shape = physics_shape_world_translate(math_circle_create(node.result.value.radius, 12),
            positions[index], 0.0f);
        (void)graphics_shape_filled_draw(shape, node_color);
    }
    return true;
}
