#include "rpg.h"
#include "map_positions.h"
#include "npc.h"

#include <string.h>

Game game{};

uint16_t rng_next() {
    uint16_t x = game.rng;

    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);

    game.rng = x;
    return x;
}

void game_new() {
    memset(&game, 0, sizeof(game));

    game.rng = 0xbabe;
    game.hp = 20;
    game.max_hp = 20;
    game.level = 1;
    npc_residence_defaults();

    Area const PROGMEM* area = world_area(AREA_OUTDOOR);
    world_enter(AREA_OUTDOOR, area->spawn.x, area->spawn.y, NORTH);
}

static DIRECTION direction(uint8_t held) {
    bool vup = (held & BTN_UP) != 0, vdown = (held & BTN_DOWN) != 0;
    bool left = (held & BTN_LEFT) != 0, right = (held & BTN_RIGHT) != 0;

    if(vup != vdown)
        return vup ? NORTH : SOUTH;
    if(left != right)
        return right ? EAST : WEST;
    return STILL;
}

// The pixel one step from (x, y) in dir. False at the top or left edge.
static bool next_pixel(uint16_t x, uint16_t y, DIRECTION dir, uint16_t& nx, uint16_t& ny) {
    nx = x;
    ny = y;
    if(dir == NORTH) {
        if(!y)
            return false;
        --ny;
    }
    if(dir == SOUTH)
        ++ny;
    if(dir == WEST) {
        if(!x)
            return false;
        --nx;
    }
    if(dir == EAST)
        ++nx;
    return true;
}

// Moves one pixel in dir when that pixel is clear for the actor.
static bool step_actor(Actor* a, DIRECTION dir) {
    uint16_t x, y;
    if(!next_pixel(a->x, a->y, dir, x, y))
        return false;
    bool player = a == &game.player;
    bool clear = player ? player_terrain_clear(x, y) : terrain_clear(x, y);
    if(!clear || (player && !npc_player_clear(x, y, dir)))
        return false;
    a->x = x;
    a->y = y;
    return true;
}

// When terrain blocks the player's next pixel, a sidestep of up to CORNER_ASSIST_PIXELS
// toward a clear position moves the player one pixel sideways. Walking on from there
// takes it past a corner a few pixels deep. Deeper overlaps stay blocked, and NPCs never
// sidestep.

static bool sidestep_corner(Actor* a, DIRECTION dir) {
    uint16_t fx, fy;
    if(!next_pixel(a->x, a->y, dir, fx, fy) || player_terrain_clear(fx, fy))
        return false;
    bool horizontal = dir == EAST || dir == WEST;
    for(uint8_t shift = 1; shift <= CORNER_ASSIST_PIXELS; ++shift) {
        for(uint8_t side = 0; side < 2; ++side) {
            DIRECTION sidestep = horizontal ? (side ? SOUTH : NORTH) : (side ? EAST : WEST);
            uint16_t cx = fx, cy = fy;
            if(horizontal)
                cy = side ? (uint16_t)(fy + shift) : (uint16_t)(fy - shift);
            else
                cx = side ? (uint16_t)(fx + shift) : (uint16_t)(fx - shift);
            uint16_t sx, sy;
            if(!player_terrain_clear(cx, cy) || !next_pixel(a->x, a->y, sidestep, sx, sy) ||
               !player_terrain_clear(sx, sy) || !npc_player_clear(sx, sy, sidestep))
                continue;
            a->x = sx;
            a->y = sy;
            return true;
        }
    }
    return false;
}

bool actor_move(Actor* a, DIRECTION dir, uint8_t pixels) {
    bool moved = false;

    a->walking = 0;
    if(dir == STILL)
        return false;

    a->facing = dir;

    while(pixels--) {
        if(step_actor(a, dir)) {
            moved = true;
            continue;
        }
        if(a == &game.player && sidestep_corner(a, dir)) {
            moved = true;
            // The sidestep uses this pixel, and the forward pixel is taken if it is now clear.
            step_actor(a, dir);
            continue;
        }
        break;
    }

    a->walking = (uint8_t)moved;
    return moved;
}

Camera game_camera() {
    int16_t x = (int16_t)game.player.x - (128 - TILE) / 2,
            y = (int16_t)game.player.y - (64 - TILE) / 2;
    int16_t mx = (int16_t)game.dimensions.width_tiles * TILE - 128;
    int16_t my = (int16_t)game.dimensions.height_tiles * TILE - 64;
    Camera c;

    if(mx < 0)
        mx = 0;
    if(my < 0)
        my = 0;

    if(x < 0)
        x = 0;
    if(y < 0)
        y = 0;

    if(x > mx)
        x = mx;
    if(y > my)
        y = my;

    c.x = (uint16_t)x;
    c.y = (uint16_t)y;
    return c;
}

void game_update(uint8_t held, uint8_t pressed) {
    bool did_teleport = update_visible_world(pressed);
    // A runs ordinary chunk logic, including blocking dialogue, with movement paused.
    if(pressed & BTN_A)
        return;
    RPG_PHASE(6);
    npc_player_begin(direction(held));
    if(!did_teleport)
        actor_move(&game.player, direction(held), PLAYER_PIXELS_PER_FRAME);
    RPG_PHASE(0);
    ++game.ticks;
}
