#include "rpg.h"

#include <string.h>

uint16_t rng_next(Game& g) {
    uint16_t x = g.rng;

    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);

    g.rng = x;
    return x;
}

void game_new(Game& g) {
    memset(&g, 0, sizeof(g));

    g.rng = 0xbabe;
    g.hp = 20;
    g.max_hp = 20;
    g.level = 1;

    world_enter(g, OUTDOOR, 32, 40, EAST);
}

static uint8_t direction(uint8_t held) {
    bool vup = (held & BTN_UP) != 0, vdown = (held & BTN_DOWN) != 0;
    bool left = (held & BTN_LEFT) != 0, right = (held & BTN_RIGHT) != 0;

    if(vup != vdown)
        return vup ? NORTH : SOUTH;
    if(left != right)
        return right ? EAST : WEST;
    return STILL;
}

bool actor_move(Game& g, Actor* a, uint8_t dir, uint8_t pixels, int8_t ignore) {
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

        if(!position_clear(g, x, y, ignore))
            break;

        a->x = x;
        a->y = y;
        moved = true;
    }

    a->walking = (uint8_t)moved;
    return moved;
}

Camera game_camera(Game const& g) {
    Area const PROGMEM* a = world_area(g.area);
    int16_t x = (int16_t)g.player.x - 60, y = (int16_t)g.player.y - 28;
    int16_t mx = (int16_t)a->width * 8 - 128, my = (int16_t)a->height * 8 - 64;
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

uint8_t interaction_target(Game const& g) {
    uint8_t i;
    int16_t px = (int16_t)g.player.x, py = (int16_t)g.player.y;

    for(i = 0; i < g.npc_count; ++i) {
        int16_t dx = (int16_t)g.npcs[i].actor.x - px;
        int16_t dy = (int16_t)g.npcs[i].actor.y - py;
        bool found = false;

        switch(g.player.facing) {
            case NORTH:
                found = dy < 0 && dy >= -14 && dx >= -6 && dx <= 6;
                break;
            case SOUTH:
                found = dy > 0 && dy <= 14 && dx >= -6 && dx <= 6;
                break;
            case EAST:
                found = dx > 0 && dx <= 14 && dy >= -6 && dy <= 6;
                break;
            case WEST:
                found = dx < 0 && dx >= -14 && dy >= -6 && dy <= 6;
                break;
        }

        if(found)
            return g.npcs[i].id;
    }
    return 0;
}

void game_update(Game& g, uint8_t held) {
    actor_move(g, &g.player, direction(held), 2, -1);
    if(!portals_update(g))
        npcs_update(g);
    ++g.ticks;
}
