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

    Area const PROGMEM* area = world_area(AREA_OUTDOOR);
    world_enter(AREA_OUTDOOR, area->spawn.x, area->spawn.y, EAST);
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

bool actor_move(Actor* a, DIRECTION dir, uint8_t pixels) {
    bool moved = false;

    a->walking = 0;
    if(dir == STILL)
        return false;

    a->facing = dir;

    while(pixels--) {
        uint16_t x = a->x, y = a->y;

        if(dir == NORTH) {
            if(!y)
                break;
            --y;
        }
        if(dir == SOUTH)
            ++y;
        if(dir == WEST) {
            if(!x)
                break;
            --x;
        }
        if(dir == EAST)
            ++x;

        if(!terrain_clear(x, y) || (a == &game.player && !npc_player_clear(x, y, dir)))
            break;

        a->x = x;
        a->y = y;
        moved = true;
    }

    a->walking = (uint8_t)moved;
    return moved;
}

Camera game_camera() {
    int16_t x = (int16_t)game.player.x - 60, y = (int16_t)game.player.y - 28;
    int16_t mx = (int16_t)game.dimensions.width_tiles * 8 - 128;
    int16_t my = (int16_t)game.dimensions.height_tiles * 8 - 64;
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
        actor_move(&game.player, direction(held), 2);
    RPG_PHASE(0);
    ++game.ticks;
}
