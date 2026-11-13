#ifndef RPG_H
#define RPG_H

#include <stdint.h>
#include "world_types.h"
#include "area_ids.h"
#include "npc_ids.h"

#ifdef RPG_AVM
#include <avm/pgmspace.h>
#else
#define PROGMEM
#define F(text) (text)
#define memcpy_P(dst, src, size) memcpy((dst), (src), (size))
#endif

#ifdef _MSC_VER
#define RPG_NOINLINE __declspec(noinline)
#else
#define RPG_NOINLINE __attribute__((noinline))
#endif

// Device QA guests (RPG_DEVICE_QA) record the simulation phase and frame markers that the device
// tests read. Shipping guests compile both out.
#if defined(RPG_AVM) && defined(RPG_DEVICE_QA)
extern volatile uint8_t world_phase;
#define RPG_PHASE(value) (world_phase = (value))
#define RPG_QA_MARK(call) call
extern "C" void rpg_frame_begin();
extern "C" void rpg_frame_end();
extern "C" void rpg_render_begin();
extern "C" void rpg_render_end();
#else
#define RPG_PHASE(value) ((void)0)
#define RPG_QA_MARK(call) ((void)0)
#endif

enum {
    SAVE_VERSION = 5,
    SAVE_HEADER_BYTES = 20,
    SAVE_NPC_BYTES = 3 * (NPC_ID_LIMIT - 1),
    SAVE_BYTES = SAVE_HEADER_BYTES + WorldFlags::BYTE_COUNT + SAVE_NPC_BYTES + 2,
    SAVE_CRC_OFFSET = SAVE_BYTES - 2
};

static_assert(SAVE_BYTES <= 255, "save length fits the record header");

enum {
    BTN_B = 4,
    BTN_A = 8,
    BTN_DOWN = 16,
    BTN_LEFT = 32,
    BTN_RIGHT = 64,
    BTN_UP = 128,
    BTN_ALL = 252
};

enum DIRECTION : uint8_t { NORTH, EAST, SOUTH, WEST, STILL = 4 };

// DIALOGUE_TYPING means the overlay is open and its letters are still animating.
enum DIALOGUE_PROMPT : uint8_t {
    DIALOGUE_CLOSED,
    DIALOGUE_MORE,
    DIALOGUE_DONE,
    DIALOGUE_CHOICE,
    DIALOGUE_TYPING
};

struct Actor {
    uint16_t x, y;
    DIRECTION facing;
    uint8_t walking;
};

#include "npc_types.h"

// Each walking pose lasts this many simulation ticks, which is 160 ms at 50 Hz.
inline constexpr uint16_t WALK_POSE_TICKS = 8;

// Character rows in actors16.png; the generated sprite sheet asserts this count.
inline constexpr uint8_t ACTOR_ROW_COUNT = 32;

constexpr uint16_t actor_frame(Actor const& actor, uint8_t sprite_row, uint16_t ticks) {
    uint8_t phase = (ticks / WALK_POSE_TICKS) & 3;
    uint8_t step = actor.walking && (phase & 1) ? (phase == 1 ? 1 : 2) : 0;
    return (uint16_t)sprite_row * 12 + actor.facing * 3 + step;
}

struct Camera {
    uint16_t x, y;
};

struct Game {
    Actor player;
    uint16_t rng, gold, ticks;
    WorldFlags flags;
    uint8_t area, hp, max_hp, level;
    TileCache cache;
    Camera camera;
    MapDimensions dimensions; // Active area dimensions, cached on entry or teleport.
    TilePos previous_player_tile;
    bool can_interact;
    NpcPool npcs;

    bool get_flag(FLAG flag) const {
        return flags.get_flag(flag);
    }

    void set_flag(FLAG flag) {
        flags.set_flag(flag);
    }

    void clear_flag(FLAG flag) {
        flags.clear_flag(flag);
    }

    void toggle_flag(FLAG flag) {
        flags.toggle_flag(flag);
    }

    bool player_is_on_tile(TilePos tile) const;

    template <TilePos POS> bool player_is_on_tile() const {
        return POS.area == area && dimensions.pixel_bounds(player.x, player.y) &&
               ((player.x + FOOT_CENTER_X) >> TILE_SHIFT) == POS.x &&
               ((player.y + FOOT_CENTER_Y) >> TILE_SHIFT) == POS.y;
    }

    bool player_is_in_region(TileRegion region) const;
};

extern Game game;

struct Area {
    uint8_t width, height;
    uint8_t chunk_columns, chunk_rows;
    uint8_t const PROGMEM* tiles;
    PixelPos spawn;
};

static_assert(sizeof(Area::width) == 1 && sizeof(Area::height) == 1 &&
                  sizeof(Area::chunk_columns) == 1 && sizeof(Area::chunk_rows) == 1,
              "8-bit logical and chunk-grid dimensions");
static_assert(sizeof(Actor::x) == 2 && sizeof(Actor::y) == 2 && sizeof(Camera::x) == 2 &&
                  sizeof(Camera::y) == 2,
              "16-bit actor and camera pixels");

Area const PROGMEM* world_area(uint8_t id);
void reload_tile_window(TileCache& cache, uint8_t const PROGMEM* tiles, MapDimensions dimensions,
                        Camera camera);
// Any collision at all makes a tile solid for NPCs, navigation and cached reads.
bool tile_solid(uint8_t tile);
// Sixteen triangles: four per 8x8 quadrant (top-left, top-right, bottom-left,
// bottom-right), each nibble ordered north, east, south, west. Player movement only.
uint16_t tile_collision(uint8_t tile);
bool collision_pixel_solid(uint16_t mask, uint8_t x, uint8_t y);
#ifndef RPG_AVM
// Host tests substitute masks for tiles that the shipped tileset does not use.
void tile_collision_override(uint8_t tile, uint16_t mask);
void tile_collision_reset();
#endif
bool tile_portal(uint8_t tile);
// Wrong-area and out-of-window reads return the solid TILE_OUTSIDE sentinel.
uint8_t cached_tile(TilePos p);
bool terrain_clear(uint16_t x, uint16_t y);
// Player foot box against collision masks. Full tiles block the whole box.
bool player_terrain_clear(uint16_t x, uint16_t y);
// Bit i is set when pixel i of the foot box's leading edge is blocked: the right or left
// column for east and west steps, and the bottom or top row for south and north steps.
uint8_t player_leading_edge(uint16_t x, uint16_t y, DIRECTION dir);
void reload_tile_cache();
bool update_visible_world(uint8_t pressed = 0, bool simulate = true);
// Places the player in an area and resets its NPCs. The visible world is stale until
// update_visible_world runs. Returns false, placing nothing, for an unknown area.
bool world_place(uint8_t area, uint16_t x, uint16_t y, DIRECTION facing);
// world_place followed by update_visible_world.
void world_enter(uint8_t area, uint16_t x, uint16_t y, DIRECTION facing);
uint16_t rng_next();
// Starts a new game at the outdoor spawn. The visible world is stale until
// update_visible_world runs.
void game_start();
// game_start followed by update_visible_world.
void game_new();
void game_update(uint8_t held, uint8_t pressed = 0);
bool actor_move(Actor* actor, DIRECTION dir, uint8_t pixels);
// Player walking speed, in pixels per frame.
inline constexpr uint8_t PLAYER_PIXELS_PER_FRAME = 1;
// A blocked step is passed by sidestepping this many pixels or fewer.
inline constexpr uint8_t CORNER_ASSIST_PIXELS = 6;
Camera game_camera();
// Block until acknowledged (true) or dismissed with B (false).
bool dialogue_show(char const PROGMEM* words);
// Wrap/page the question, then return A = yes or B = no.
bool dialogue_ask(char const PROGMEM* question);
void save_encode(uint8_t out[SAVE_BYTES]);
// Restores a valid record into game without placing the player in its world. A caller that resumes
// play must then call world_place(game.area, game.player.x, game.player.y, game.player.facing) and
// update_visible_world.
bool save_decode(uint8_t const* in, uint16_t length = SAVE_BYTES);
uint16_t save_checksum(uint8_t const* in, uint16_t length = SAVE_CRC_OFFSET);
void rpg_render_world();
void render_title(bool can_continue, uint8_t selection);
void render_menu(uint8_t selection, char const PROGMEM* feedback, uint8_t audio_rate);
void render_status();

void input_sync();
uint8_t input_held();
uint8_t input_pressed();
void input_wait_release(uint8_t mask = BTN_ALL);
uint8_t input_wait_press(uint8_t mask = BTN_ALL);
void input_wait_frame();

void title_screen();
void explore();
void menu_screen();
void status_screen();
bool game_save();
bool game_load();
char const PROGMEM* music_next();

extern uint8_t const PROGMEM TILE_SPRITES[];
// The sprite sheet of a tileset. Each area draws with the tileset from area_tileset().
uint8_t const PROGMEM* tileset_sprites(uint8_t tileset);
extern uint8_t const PROGMEM ACTOR_SPRITES[];
// The battle cursor: 22x22 plus-mask frames, one per dash phase. The generated sheet asserts the
// count.
inline constexpr uint8_t CURSOR_FRAME_COUNT = 4;
extern uint8_t const PROGMEM CURSOR_SPRITES[];
// The battle destination markers in battle_targets16.png. Frames 0 to 3 are the movement diamond,
// and frames 4 to 7 are the attack brackets. A marker shows its base plus the cursor's phase, so
// every marker pulses with the cursor. The generated sheet asserts the layout.
inline constexpr uint8_t BATTLE_TARGET_FRAMES = 4;
inline constexpr uint8_t BATTLE_TARGET_MOVE = 0;
inline constexpr uint8_t BATTLE_TARGET_ATTACK = 4;
static_assert(BATTLE_TARGET_FRAMES == CURSOR_FRAME_COUNT, "markers pulse with the cursor's phases");
constexpr uint8_t battle_target_frame(uint8_t base, uint8_t phase) {
    return (uint8_t)(base + phase % BATTLE_TARGET_FRAMES);
}
extern uint8_t const PROGMEM BATTLE_TARGET_SPRITES[];
// The archer's arrow in flight, battle_arrows16.png: one 16x16 plus-mask frame per DIRECTION, so a
// direction is its own frame index. The generated sheet asserts the count.
inline constexpr uint8_t BATTLE_ARROW_FRAMES = 4;
extern uint8_t const PROGMEM BATTLE_ARROW_SPRITES[];

#endif
