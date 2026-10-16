#ifndef RPG_H
#define RPG_H

#include <stdint.h>

#ifdef RPG_AVM
#include <avm/pgmspace.h>
#else
#define PROGMEM
#define F(text) (text)
#endif

enum { TILE = 8, NPC_CAP = 12, SAVE_BYTES = 24, SAVE_VERSION = 1 };

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

enum { OUTDOOR = 1, INTERIOR = 2, FLAG_HELPED = 1 };

struct Actor {
    uint16_t x, y;
    uint8_t facing, walking;
};

struct Npc {
    Actor actor;
    uint8_t id, timer;
};

struct Game {
    Actor player;
    Npc npcs[NPC_CAP];
    uint16_t rng, flags, gold, ticks;
    uint8_t area, hp, max_hp, level, npc_count, portal_lock;
};

struct Camera {
    uint16_t x, y;
};

struct Area {
    uint8_t width, height;
    uint8_t const PROGMEM* tiles;
};

struct NpcDef {
    uint16_t x, y, min_x, min_y, max_x, max_y;
    uint8_t id, behavior;
};

Area const PROGMEM* world_area(uint8_t id);
uint8_t world_tile(uint8_t area, uint8_t x, uint8_t y);
bool terrain_clear(uint8_t area, uint16_t x, uint16_t y);
bool world_save_position_valid(uint8_t area, uint16_t x, uint16_t y);
bool actors_overlap(uint16_t x, uint16_t y, uint16_t bx, uint16_t by);
bool position_clear(Game const& g, uint16_t x, uint16_t y, int8_t ignore);
void world_enter(Game& g, uint8_t area, uint16_t x, uint16_t y, uint8_t facing);
bool portals_update(Game& g);
void npcs_update(Game& g);
uint16_t rng_next(Game& g);
void game_new(Game& g);
void game_update(Game& g, uint8_t held);
bool actor_move(Game& g, Actor* actor, uint8_t dir, uint8_t pixels, int8_t ignore);
Camera game_camera(Game const& g);
uint8_t interaction_target(Game const& g);
// Block until acknowledged (true) or dismissed with B (false).
bool dialogue_show(Game& g, char const PROGMEM* words);
// Wrap/page the question, then return A = yes or B = no.
bool dialogue_ask(Game& g, char const PROGMEM* question);
void dialogue_talk(Game& g, uint8_t id);
void save_encode(Game const& g, uint8_t out[SAVE_BYTES]);
bool save_decode(Game& g, uint8_t const in[SAVE_BYTES]);
uint16_t save_checksum(uint8_t const* in);
void rpg_render_world(Game const& g);
void render_title(bool can_continue, uint8_t selection);
void render_menu(Game const& g, uint8_t selection, char const PROGMEM* feedback);
void render_status(Game const& g);

void input_sync();
uint8_t input_held();
uint8_t input_pressed();
void input_wait_release(uint8_t mask = BTN_ALL);
uint8_t input_wait_press(uint8_t mask = BTN_ALL);
void input_wait_frame();

void title_screen(Game& g);
void explore(Game& g);
void menu_screen(Game& g);
void status_screen(Game const& g);
bool game_save(Game const& g);
bool game_load(Game& g);

extern uint8_t const PROGMEM TILE_SPRITES[];
extern uint8_t const PROGMEM ACTOR_SPRITES[];

#endif
