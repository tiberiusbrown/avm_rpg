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

// How one pixel step ended. Only a terrain block lets the player sidestep.
enum STEP_RESULT : uint8_t { STEP_MOVED, STEP_TERRAIN, STEP_NPC, STEP_EDGE };

// Moves one pixel in dir when that pixel is clear for the actor.
static STEP_RESULT step_actor(Actor* a, DIRECTION dir) {
    uint16_t x, y;
    if(!next_pixel(a->x, a->y, dir, x, y))
        return STEP_EDGE;
    bool player = a == &game.player;
    if(!(player ? player_terrain_clear(x, y) : terrain_clear(x, y)))
        return STEP_TERRAIN;
    if(player && !npc_player_clear(x, y, dir))
        return STEP_NPC;
    a->x = x;
    a->y = y;
    return STEP_MOVED;
}

static_assert(CORNER_ASSIST_PIXELS < FOOT_WIDTH, "sidestep shifts stay within the edge");

// A sidestep of `shift` pixels toward side 0 (north or west) keeps the edge pixels below the
// shift, which therefore must be clear. Side 1 (south or east) keeps the pixels above it. The
// exact foot-box test still decides each candidate.
static bool sidestep_admissible(uint8_t edge, uint8_t shift, uint8_t side) {
    if(side == 0)
        return (edge & ((1u << (FOOT_WIDTH - shift)) - 1)) == 0;
    return (edge >> shift) == 0;
}

// A blocked player step sidesteps by up to CORNER_ASSIST_PIXELS toward a clear position, as a
// one-pixel move in that direction. Walking on from there takes it past a corner a few pixels
// deep. Deeper overlaps stay blocked, and NPCs never sidestep.
__attribute__((noinline)) static bool sidestep_corner(Actor* a, DIRECTION dir) {
    uint16_t fx, fy;
    if(!next_pixel(a->x, a->y, dir, fx, fy))
        return false;
    uint8_t edge = player_leading_edge(fx, fy, dir);
    // Every edge pixel is blocked, so no sidestep up to the assist distance clears it.
    if(edge == 0xFF)
        return false;
    bool horizontal = dir == EAST || dir == WEST;
    // A strafe pixel depends only on its side, so each side is tested at most once.
    uint8_t strafe[2] = {0, 0}; // 0 untested, 1 clear, 2 blocked
    for(uint8_t shift = 1; shift <= CORNER_ASSIST_PIXELS; ++shift) {
        for(uint8_t side = 0; side < 2; ++side) {
            if(!sidestep_admissible(edge, shift, side))
                continue;
            int16_t offset = side ? shift : -shift;
            uint16_t cx = horizontal ? fx : (uint16_t)(fx + offset);
            uint16_t cy = horizontal ? (uint16_t)(fy + offset) : fy;
            if(!player_terrain_clear(cx, cy))
                continue;
            DIRECTION sidestep = horizontal ? (side ? SOUTH : NORTH) : (side ? EAST : WEST);
            if(!strafe[side]) {
                uint16_t sx, sy;
                bool clear = next_pixel(a->x, a->y, sidestep, sx, sy) &&
                             player_terrain_clear(sx, sy) && npc_player_clear(sx, sy, sidestep);
                strafe[side] = clear ? 1 : 2;
            }
            if(strafe[side] == 2)
                continue;
            uint16_t sx, sy;
            next_pixel(a->x, a->y, sidestep, sx, sy);
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
        STEP_RESULT step = step_actor(a, dir);
        if(step == STEP_MOVED) {
            moved = true;
            continue;
        }
        if(step == STEP_TERRAIN && a == &game.player && sidestep_corner(a, dir)) {
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
