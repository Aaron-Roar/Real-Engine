#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "entity_components.h"
#include "engine.h"
#include "physics.h"
#include "math2d.h"
#include "math.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define MAX_TEXTURES 50
#define MAX_ANIMATIONS_FRAMES 20
#define MAX_ANIMATION_SETS 10
#define MAX_CAMERAS 16
#define MAX_SCREENS 16
#define MAX_VIEWPORTS 16

#define RECORDING_WIDTH  WINDOW_WIDTH
#define RECORDING_HEIGHT WINDOW_HEIGHT

/** 2D scale factor for textures and sprites. */
typedef struct {
    /** Horizontal scale. */
    float x;
    /** Vertical scale. */
    float y;
} Scale;

/** Stable handle for an engine-owned camera. */
typedef uint32_t CameraId;
typedef void (*CameraRenderCallback)(CameraId camera, void *context);

/** Invalid camera handle. */
#define CAMERA_INVALID 0

/** Logical window rectangle occupied by a viewport. */
typedef struct ViewportRectangle {
    float x;
    float y;
    float width;
    float height;
} ViewportRectangle;

/** World-space camera transform used by the renderer. */
typedef struct {
    /** World position shown at the center of the viewport. */
    Position position;
    /** Counterclockwise world orientation in radians. */
    Orientation orientation;
    /** Logical world-view dimensions before magnification. */
    Vec2D dimensions;
    /** Magnification where values above one zoom in. */
    float zoom;
} Camera;

/** Defaults used when creating an engine-owned camera. */
typedef struct CameraConfig {
    Position position;
    Orientation orientation;
    Vec2D dimensions;
    float zoom;
} CameraConfig;

ERROR_DECLARE_RESULT_TYPE(CameraIdResult, CameraId);
ERROR_DECLARE_RESULT_TYPE(CameraResult, Camera);
ERROR_DECLARE_RESULT_TYPE(CameraZoomResult, float);

/** Non-owning description of an entity-following camera attachment. */
typedef struct {
    /** Entity transform followed by the camera. */
    Entity entity;
    /**
     * Local offset when following orientation, otherwise a world-space offset
     * or fixed position.
     */
    Vec2D position_offset;
    /** Relative or fixed orientation in radians. */
    Orientation orientation_offset;
    /** Whether the camera inherits the entity position. */
    bool follow_position;
    /** Whether the camera inherits the entity orientation. */
    bool follow_orientation;
} CameraAttachment;

/** Result type for functions that return a CameraAttachment. */
ERROR_DECLARE_RESULT_TYPE(CameraAttachmentResult, CameraAttachment);

typedef uint32_t ViewportId;

#define VIEWPORT_INVALID 0

typedef enum ScreenFit {
    SCREEN_FIT_NONE,
    SCREEN_FIT_STRETCH,
    SCREEN_FIT_CONTAIN,
    SCREEN_FIT_COVER,
} ScreenFit;

typedef struct ViewportConfig {
    /** Window-space destination and clipping rectangle. */
    ViewportRectangle rectangle;
    /** How assigned screen content is fitted into the rectangle. */
    ScreenFit fit;
} ViewportConfig;

ERROR_DECLARE_RESULT_TYPE(ViewportIdResult, ViewportId);

/** Descriptor for loading a texture from disk. */
typedef struct {
  /** Path to the texture file. */
  const char *file;
  /** Source texture size metadata. */
  Scale size;
} TextureDescriptor;

/** Descriptor for loading an animation from texture descriptors. */
typedef struct {
  /** Texture descriptors for each animation frame. */
  TextureDescriptor texture_descriptors[MAX_ANIMATIONS_FRAMES];
  /** Number of valid descriptors. */
  uint8_t amount_of_descriptors;
  /** Frame duration measured in engine ticks. */
  Tick ticks_per_frame;
  /** Frame duration measured in engine time. */
  Time time_per_frame;
} AnimationDescriptor;

/** SDL texture handle owned by the graphics module. */
typedef SDL_Texture* Texture;

/** Loaded texture and its size metadata. */
typedef struct {
    /** SDL texture handle. */
    Texture texture;
    /** Texture size metadata. */
    Scale size;
} TextureAsset;

/** Result type for functions that return a TextureAsset. */
ERROR_DECLARE_RESULT_TYPE(TextureAssetResult, TextureAsset);

/** Descriptor for loading a scalable font from disk. */
typedef struct FontDescriptor {
    /** Path to a font file supported by SDL3_ttf. */
    const char *file;
    /** Font point size. */
    float point_size;
} FontDescriptor;

/**
 * Loaded font owned by the caller. Destroy all text created from this font
 * before destroying the font, and destroy the font before graphics shutdown.
 */
typedef struct FontAsset {
    TTF_Font *font;
} FontAsset;

/** Result type for functions that return a FontAsset. */
ERROR_DECLARE_RESULT_TYPE(FontAssetResult, FontAsset);

/**
 * Reusable rendered text owned by the caller. Destroy it before its font and
 * before graphics shutdown.
 */
typedef struct TextAsset {
    TTF_Text *text;
    /** Logical screen-space dimensions of the rendered text. */
    Scale size;
} TextAsset;

/** Result type for functions that return a TextAsset. */
ERROR_DECLARE_RESULT_TYPE(TextAssetResult, TextAsset);

/** Fixed list of loaded textures for an animation. */
typedef struct {
    /** Loaded texture assets. */
    TextureAsset textures[MAX_TEXTURES];
    /** Number of valid textures. */
    int amount;
} TextureList;

/** Loaded animation asset. */
typedef struct {
    /** Textures used as animation frames. */
    TextureList texture_list;
    /** Frame duration measured in engine ticks. */
    Tick ticks_per_frame;
    /** Frame duration measured in engine time. */
    Time time_per_frame;
} AnimationAsset;

/** Result type for functions that return an AnimationAsset. */
ERROR_DECLARE_RESULT_TYPE(AnimationAssetResult, AnimationAsset);

/** Horizontal facing direction for sprite drawing. */
typedef enum {DIRECTION_LEFT, DIRECTION_RIGHT} Direction;


/** Runtime animated sprite state. */
typedef struct {
    /** Animation asset used by this sprite. */
    AnimationAsset animation;
    /** Current animation frame index. */
    int animation_frame;
    /** Tick when the frame last advanced. */
    Tick last_update_tick;
    /** Time when the frame last advanced. */
    Time last_update_time;
    /** Current draw direction. */
    Direction direction;
    /** Draw scale. */
    Scale scale;
} AnimatedSprite;

/** Fixed set of animated sprites. */
typedef struct {
    /** Sprite entries. */
    AnimatedSprite sprite_set[MAX_ANIMATION_SETS];
    /** Number of valid sprite entries. */
    uint8_t amount_of_sets;
} AnimatedSpriteSet;

/** Pool storing animated sprites by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(AnimatedSpritePool, AnimatedSprite);

/** Pool backing the animated_sprites table. */
extern AnimatedSpritePool animated_sprites_pool;

/** Animated sprite table indexed by EntityIndex. */
#define animated_sprites animated_sprites_pool.objects

/** Shape fill mode for debug drawing. */
typedef enum Fill {
    /** Draw only the outline. */
    GRAPHICS_OUTLINE,
    /** Draw filled geometry. */
    GRAPHICS_FILLED
} Fill;

/**
 * Create a color from a 32-bit RRGGBBAA hex value.
 */
Color graphics_color_hex_create(uint32_t hex_color_code);

/**
 * Create the SDL window and renderer.
 */
EngineResult graphics_start(void);

/**
 * Destroy graphics resources and stop SDL video.
 */
void graphics_end(void);

/**
 * Poll graphics events and report whether the window should remain open.
 */
bool graphics_events_poll(SDL_Event *event);

/** Clear the render target with a background color. */
void graphics_background_draw(Color color);

/** Draw a filled rectangle in logical screen coordinates. */
bool graphics_screen_rect_draw(float x, float y, float width, float height, Color color);
bool graphics_screen_clip_set(float x, float y, float width, float height);
void graphics_screen_clip_clear(void);
bool graphics_screen_clip_push(float x, float y, float width, float height);
void graphics_screen_clip_pop(void);

/** Draw a centered, rotated rectangle in logical screen coordinates. */
bool graphics_screen_quad_draw(
    Position center,
    float width,
    float height,
    float angle,
    Color color
);

/** Present the current frame. */
void graphics_show(void);
EngineResult graphics_vsync_set(bool enabled);
EngineResult graphics_frame_limit_set(int frames_per_second);

/** Draw one entity hitbox. */
void graphics_hit_box_draw(Entity entity, Fill fill_type);

/** Draw one entity hitbox with a caller supplied color. */
void graphics_hit_box_colored_draw(Entity entity, Fill fill_type, Color color);

/** Draw every live entity hitbox. */
void graphics_hit_boxes_draw(void);

/** Load a texture from a descriptor. */
TextureAssetResult graphics_texture_load(TextureDescriptor text_desc);

/** Load a font. The caller must destroy successful assets. */
FontAssetResult graphics_font_load(FontDescriptor descriptor);

/** Close a loaded font after all text assets using it are destroyed. */
void graphics_font_destroy(FontAsset *font);

/** Create reusable text. An empty string produces an empty text asset. */
TextAssetResult graphics_text_create(const FontAsset *font, const char *value, Color color);
bool graphics_text_value_set(TextAsset *text, const char *value);

/** Destroy reusable text. */
void graphics_text_destroy(TextAsset *text);

/** Draw reusable text with its top-left corner in logical screen space. */
bool graphics_text_draw(const TextAsset *text, Position position);

/** Load an animation from texture descriptors. */
AnimationAssetResult graphics_animation_load(AnimationDescriptor anim_desc);

/** Create sprite runtime state from an animation asset. */
AnimatedSprite graphics_animated_sprite_create(AnimationAsset asset_ptr, Scale scale);

/** Attach an animated sprite to an entity. */
EngineResult graphics_animated_sprite_add(Entity entity, AnimatedSprite sprite);

/** Draw all live animated sprites. */
void graphics_animated_sprites_draw(void);

/** Update frame state for all live animated sprites. */
void graphics_sprite_frames_update(Tick current_tick, Time current_time);

/** Scale an entity's animated sprite textures. */
void graphics_textures_scale(Entity entity, Scale scale);

/** Translate the active camera in world space. */
void graphics_camera_move(Vec2D translation);

/** Rotate the active camera counterclockwise by radians. */
void graphics_camera_rotate(Orientation radians);

/**
 * Attach the camera to an entity transform.
 *
 * The position offset is expressed in the entity's local space. The
 * orientation offset is added to the entity's orientation.
 */
EngineResult graphics_camera_attach(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset
);

/** Attach a camera with independent position and orientation following. */
EngineResult graphics_camera_with_options_attach(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset,
    bool follow_position,
    bool follow_orientation
);

/** Detach the camera while preserving its resolved world transform. */
void graphics_camera_detach(void);

/** Report whether the camera is attached to a live entity transform. */
bool graphics_camera_attached_get(void);

/** Copy the active attachment description to the caller. */
bool graphics_camera_attachment_get(CameraAttachment *attachment);

CameraConfig graphics_camera_config_default_get(void);
CameraIdResult graphics_camera_create(CameraConfig config);
EngineResult graphics_camera_destroy(CameraId camera);
EngineResult graphics_camera_active_set(CameraId camera);
CameraId graphics_camera_active_get(void);
CameraResult graphics_camera_get(CameraId camera);
EngineResult graphics_camera_set(CameraId camera, Camera value);
EngineResult graphics_camera_attachment_set(
    CameraId camera,
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset,
    bool follow_position,
    bool follow_orientation
);
EngineResult graphics_camera_attachment_remove(CameraId camera);

/** Register the drawing callback invoked automatically when this camera is needed. */
EngineResult graphics_camera_render_callback_set(
    CameraId camera,
    CameraRenderCallback callback,
    void *context
);
EngineResult graphics_camera_enable_set(CameraId camera);
EngineResult graphics_camera_disable_set(CameraId camera);
/** Skip camera rendering while the engine is paused. */
EngineResult graphics_camera_pause_with_engine_set(CameraId camera);
/** Continue camera rendering while paused; this is the default. */
EngineResult graphics_camera_render_when_paused_set(CameraId camera);
/** Move relatively over engine-tick time; non-positive duration is immediate. */
EngineResult graphics_camera_position_move(CameraId camera, Vec2D translation, Time duration);
/** Move to a world position over engine-tick time. */
EngineResult graphics_camera_position_set(CameraId camera, Position position, Time duration);
/** Move toward an entity's current position without attaching. */
EngineResult graphics_camera_position_from_entity_set(CameraId camera, Entity entity, Time duration);
/** Immediately attach to and follow an entity's position. */
EngineResult graphics_camera_entity_attachment_set(CameraId camera, Entity entity);
/** Return success with true while a timed movement is active. */
EngineResult graphics_camera_moving_get(CameraId camera);
/** Scale over engine-tick time; non-positive duration is immediate. */
EngineResult graphics_camera_zoom_set(CameraId camera, float zoom, Time duration);
CameraZoomResult graphics_camera_zoom_get(CameraId camera);

/** Return a disabled, full-window viewport using contain fitting. */
ViewportConfig graphics_viewport_config_default_get(void);
ViewportIdResult graphics_viewport_create(ViewportConfig config);
EngineResult graphics_viewport_destroy(ViewportId viewport);
/** Assign a camera without transferring ownership. */
EngineResult graphics_viewport_camera_set(ViewportId viewport, CameraId camera);
EngineResult graphics_viewport_camera_clear(ViewportId viewport);
EngineResult graphics_viewport_enable_set(ViewportId viewport);
EngineResult graphics_viewport_disable_set(ViewportId viewport);

/** Convert world coordinates to logical screen coordinates. */
Position graphics_world_to_screen_get(Position pos);

/** Convert logical screen coordinates to world coordinates. */
Position graphics_screen_to_world_get(Position screen);

/** Convert SDL window coordinates to logical screen coordinates. */
Position graphics_window_to_screen_get(Position window);
Scale graphics_render_output_size_get(void);
bool graphics_logical_size_set(int width, int height);

/** Get the mouse position in logical screen coordinates. */
Position graphics_mouse_screen_position_get(void);

/** Enable or disable AABB-tree debug drawing. */
void graphics_aabb_tree_debug_set(bool enabled);
/** Return whether AABB-tree debug drawing is enabled. */
bool graphics_aabb_tree_debug_check(void);
/** Draw the current physics AABB-tree bounds when enabled. */
void graphics_aabb_tree_draw(void);

/** Start recording frames to an output file through ffmpeg. */
bool graphics_recording_start(
    const char *output_path,
    int fps
);

/** Draw all particle entities. */
void graphics_particles_draw(void);

/** Draw local origin axes for all live hitbox entities. */
void graphics_local_origins_draw(void);
/** Draw one joint using its engineering-style debug symbol. */
bool graphics_joint_draw(Entity joint, Color color);
/** Draw every live joint using engineering-style debug symbols. */
void graphics_joints_draw(Color color);
/** Draw a soft body's deforming surfaces, beams, and collision nodes. */
bool graphics_soft_body_draw(Entity soft_body, Color surface, Color beam, Color node);
EngineResult graphics_soft_body_node_color_set(Entity soft_body, Entity node, Color color);
EngineResult graphics_soft_body_beam_color_set(
    Entity soft_body, Entity node_a, Entity node_b, Color color);
EngineResult graphics_soft_body_area_color_set(
    Entity soft_body, Entity node_a, Entity node_b, Entity node_c, Color color);
#endif
