#ifndef RPG_H
#define RPG_H

#include <stdint.h>
#include "world_types.h"
#include "area_ids.h"

#ifdef RPG_AVM
#include <avm/pgmspace.h>
#else
#define PROGMEM
#define F(text) (text)
#endif

#ifdef _MSC_VER
#define RPG_NOINLINE __declspec(noinline)
#else
#define RPG_NOINLINE __attribute__((noinline))
#endif

#ifdef RPG_AVM
extern volatile uint8_t world_phase;
#define RPG_PHASE(value) (world_phase = (value))
#else
#define RPG_PHASE(value) ((void)0)
#endif

enum {
    TILE = 8,
    SAVE_VERSION = 3,
    SAVE_HEADER_BYTES = 20,
    SAVE_BYTES = SAVE_HEADER_BYTES + WorldFlags::BYTE_COUNT + 2 > 24
                     ? SAVE_HEADER_BYTES + WorldFlags::BYTE_COUNT + 2
                     : 24,
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

enum DIALOGUE_PROMPT : uint8_t { DIALOGUE_CLOSED, DIALOGUE_MORE, DIALOGUE_DONE, DIALOGUE_CHOICE };

struct Actor {
    uint16_t x, y;
    uint8_t facing, walking;
};

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

    template <TilePos const& POS> bool player_is_on_tile() const {
        return POS.area == area && dimensions.pixel_bounds(player.x, player.y) &&
               ((player.x + 4) >> 3) == POS.x && ((player.y + 6) >> 3) == POS.y;
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
bool tile_solid(uint8_t tile);
bool tile_portal(uint8_t tile);
// Wrong-area and out-of-window reads return the solid TILE_OUTSIDE sentinel.
uint8_t cached_tile(TilePos p);
bool terrain_clear(uint16_t x, uint16_t y);
void reload_tile_cache();
bool update_visible_world(uint8_t pressed = 0);
void world_enter(uint8_t area, uint16_t x, uint16_t y, uint8_t facing);
uint16_t rng_next();
void game_new();
void game_update(uint8_t held, uint8_t pressed = 0);
bool actor_move(Actor* actor, uint8_t dir, uint8_t pixels);
Camera game_camera();
// Block until acknowledged (true) or dismissed with B (false).
bool dialogue_show(char const PROGMEM* words);
// Wrap/page the question, then return A = yes or B = no.
bool dialogue_ask(char const PROGMEM* question);
void save_encode(uint8_t out[SAVE_BYTES]);
bool save_decode(uint8_t const* in, uint16_t length = SAVE_BYTES);
uint16_t save_checksum(uint8_t const* in, uint16_t length = SAVE_CRC_OFFSET);
void rpg_render_world();
void render_title(bool can_continue, uint8_t selection);
void render_menu(uint8_t selection, char const PROGMEM* feedback);
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

extern uint8_t const PROGMEM TILE_SPRITES[];
extern uint8_t const PROGMEM ACTOR_SPRITES[];

#endif
