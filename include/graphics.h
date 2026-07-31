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

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define MAX_TEXTURES 50
#define MAX_ANIMATIONS_FRAMES 20
#define MAX_ANIMATION_SETS 10

#define RECORDING_WIDTH  WINDOW_WIDTH
#define RECORDING_HEIGHT WINDOW_HEIGHT

/** 2D scale factor for textures and sprites. */
typedef struct {
    /** Horizontal scale. */
    float x;
    /** Vertical scale. */
    float y;
} Scale;

/** World-space camera transform used by the renderer. */
typedef struct {
    /** World position shown at the center of the viewport. */
    Position position;
    /** Counterclockwise world orientation in radians. */
    Orientation orientation;
} Camera;

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

/** RGBA color value. */
typedef struct Color {
  /** Red channel. */
  uint8_t red;
  /** Green channel. */
  uint8_t green;
  /** Blue channel. */
  uint8_t blue;
  /** Alpha channel. */
  uint8_t alpha;
} Color;

/**
 * Create a color from a 32-bit RGBA hex value.
 */
Color graphics_creat_color_hex(uint32_t hex_color_code);

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
bool graphics_poll_events(SDL_Event *event);

/** Clear the render target with a background color. */
void graphics_draw_background(Color color);

/** Draw a filled rectangle in logical screen coordinates. */
bool graphics_draw_screen_rect(float x, float y, float width, float height, Color color);

/** Draw a centered, rotated rectangle in logical screen coordinates. */
bool graphics_draw_screen_quad(
    Position center,
    float width,
    float height,
    float angle,
    Color color
);

/** Present the current frame. */
void graphics_show(void);

/** Draw one entity hitbox. */
void graphics_draw_hit_box(Entity entity, Fill fill_type);

/** Draw one entity hitbox with a caller supplied color. */
void graphics_draw_hit_box_colored(Entity entity, Fill fill_type, Color color);

/** Draw every live entity hitbox. */
void graphics_draw_hit_boxes(void);

/** Load a texture from a descriptor. */
TextureAssetResult graphics_load_texture(TextureDescriptor text_desc);

/** Load a font. The caller must destroy successful assets. */
FontAssetResult graphics_load_font(FontDescriptor descriptor);

/** Close a loaded font after all text assets using it are destroyed. */
void graphics_destroy_font(FontAsset *font);

/** Create reusable text. An empty string produces an empty text asset. */
TextAssetResult graphics_create_text(const FontAsset *font, const char *value, Color color);

/** Destroy reusable text. */
void graphics_destroy_text(TextAsset *text);

/** Draw reusable text with its top-left corner in logical screen space. */
bool graphics_draw_text(const TextAsset *text, Position position);

/** Load an animation from texture descriptors. */
AnimationAssetResult graphics_load_animation(AnimationDescriptor anim_desc);

/** Create sprite runtime state from an animation asset. */
AnimatedSprite graphics_create_animated_sprite(AnimationAsset asset_ptr, Scale scale);

/** Attach an animated sprite to an entity. */
EngineResult graphics_add_animated_sprite(Entity entity, AnimatedSprite sprite);

/** Draw all live animated sprites. */
void graphics_draw_animated_sprites(void);

/** Update frame state for all live animated sprites. */
void graphics_update_sprite_frames(Tick current_tick, Time current_time);

/** Scale an entity's animated sprite textures. */
void graphics_scale_textures(Entity entity, Scale scale);

/** Replace the active camera transform. */
void graphics_set_camera(Camera camera);

/** Return the active camera transform. */
Camera graphics_get_camera(void);

/** Translate the active camera in world space. */
void graphics_move_camera(Vec2D translation);

/** Rotate the active camera counterclockwise by radians. */
void graphics_rotate_camera(Orientation radians);

/**
 * Attach the camera to an entity transform.
 *
 * The position offset is expressed in the entity's local space. The
 * orientation offset is added to the entity's orientation.
 */
EngineResult graphics_attach_camera(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset
);

/** Attach a camera with independent position and orientation following. */
EngineResult graphics_attach_camera_with_options(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset,
    bool follow_position,
    bool follow_orientation
);

/** Detach the camera while preserving its resolved world transform. */
void graphics_detach_camera(void);

/** Report whether the camera is attached to a live entity transform. */
bool graphics_camera_is_attached(void);

/** Copy the active attachment description to the caller. */
bool graphics_get_camera_attachment(CameraAttachment *attachment);

/** Convert world coordinates to logical screen coordinates. */
Position graphics_world_to_screen(Position pos);

/** Convert logical screen coordinates to world coordinates. */
Position graphics_screen_to_world(Position screen);

/** Convert SDL window coordinates to logical screen coordinates. */
Position graphics_window_to_screen(Position window);

/** Get the mouse position in logical screen coordinates. */
Position graphics_get_mouse_screen_position(void);

/** Draw the editor/debug grid. */
void graphics_draw_grid(void);

/** Start recording frames to an output file through ffmpeg. */
bool graphics_recording_start(
    const char *output_path,
    int fps
);

/** Draw all particle entities. */
void graphics_draw_particles(void);

/** Draw local origin axes for all live hitbox entities. */
void graphics_draw_local_origins(void);
#endif
