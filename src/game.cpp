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
    g.mode = EXPLORE;

    world_enter(g, OUTDOOR, 32, 40, EAST);
}

void game_boot(Game& g, bool valid) {
    game_new(g);

    g.mode = TITLE;
    g.save_valid = (uint8_t)valid;
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

COMMAND game_tick(Game& g, uint8_t held) {
    uint8_t pressed = (uint8_t)(held & (uint8_t)~g.previous_buttons);
    uint8_t dir = direction(held);

    g.previous_buttons = held;
    if(g.feedback)
        --g.feedback;

    switch(g.mode) {

        case TITLE:
            if(pressed & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT))
                g.selection ^= 1;

            if(pressed & BTN_A) {
                if(g.selection && g.save_valid)
                    return LOAD_COMMAND;
                if(!g.selection) {
                    game_new(g);
                    g.previous_buttons = held;
                }
            }
            break;

        case EXPLORE:
            if(pressed & BTN_B) {
                g.mode = MENU;
                g.selection = 0;
                g.feedback = 0;
                break;
            }

            if(pressed & BTN_A) {
                uint8_t id = interaction_target(g);

                if(id) {
                    dialogue_talk(g, id);
                    break;
                }
                g.feedback = 20;
            }

            actor_move(g, &g.player, dir, 2, -1);
            if(!portals_update(g))
                npcs_update(g);
            ++g.ticks;
            break;

        case MENU:
            if(pressed & BTN_B) {
                g.mode = EXPLORE;
                g.feedback = 0;
            } else if(pressed & (BTN_DOWN | BTN_RIGHT))
                g.selection = (uint8_t)((g.selection + 1) % 3);
            else if(pressed & (BTN_UP | BTN_LEFT))
                g.selection = (uint8_t)((g.selection + 2) % 3);
            else if(pressed & BTN_A) {
                if(g.selection == 0)
                    g.mode = STATUS;
                if(g.selection == 1)
                    return SAVE_COMMAND;
                if(g.selection == 2) {
                    g.mode = EXPLORE;
                    g.feedback = 0;
                }
            }
            break;

        case STATUS:
            if(pressed & (BTN_A | BTN_B))
                g.mode = MENU;
            break;

        case TACTICAL_RESERVED:
            break;
    }

    return NO_COMMAND;
}
