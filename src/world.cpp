#include "rpg.h"
#include "world_assets.h"

Area const PROGMEM* world_area(uint8_t id) {
    return id == INTERIOR ? &ROOM_AREA : &FIELD_AREA;
}

uint8_t world_tile(uint8_t area, uint8_t x, uint8_t y) {
    Area const PROGMEM* a = world_area(area);

    if(x >= a->width || y >= a->height)
        return 1;
    return a->tiles[(uint16_t)y * a->width + x];
}

bool terrain_clear(uint8_t area, uint16_t x, uint16_t y) {
    Area const PROGMEM* a = world_area(area);
    uint8_t tx, ty, x1, x2, y1, y2;

    if(x > (uint16_t)a->width * 8 - 8 || y > (uint16_t)a->height * 8 - 8)
        return false;

    x1 = (uint8_t)((x + 1) >> 3);
    x2 = (uint8_t)((x + 6) >> 3);
    y1 = (uint8_t)((y + 3) >> 3);
    y2 = (uint8_t)((y + 7) >> 3);

    for(ty = y1; ty <= y2; ++ty)
        for(tx = x1; tx <= x2; ++tx) {
            uint8_t tile = world_tile(area, tx, ty);
            if(tile == 1 || tile == 2)
                return false;
        }
    return true;
}

bool actors_overlap(uint16_t x, uint16_t y, uint16_t bx, uint16_t by) {
    return x + 1 < bx + 7 && bx + 1 < x + 7 && y + 3 < by + 8 && by + 3 < y + 8;
}

bool position_clear(Game const& g, uint16_t x, uint16_t y, int8_t ignore) {
    uint8_t i;

    if(!terrain_clear(g.area, x, y))
        return false;
    if(ignore >= 0 && actors_overlap(x, y, g.player.x, g.player.y))
        return false;

    for(i = 0; i < g.npc_count; ++i) {
        if((int8_t)i != ignore && actors_overlap(x, y, g.npcs[i].actor.x, g.npcs[i].actor.y))
            return false;
    }
    return true;
}

static NpcDef const PROGMEM* npc_def(uint8_t id) {
    return &NPC_DEFS[id - 1];
}

bool world_save_position_valid(uint8_t area, uint16_t x, uint16_t y) {
    if(!terrain_clear(area, x, y))
        return false;

    if(area == INTERIOR) {
        NpcDef const PROGMEM* d = &NPC_DEFS[0];
        if(actors_overlap(x, y, d->x, d->y))
            return false;
    }
    return true;
}

void world_enter(Game& g, uint8_t area, uint16_t x, uint16_t y, uint8_t facing) {
    uint8_t i, start = area == INTERIOR ? 0 : 1, count = area == INTERIOR ? 1 : 3;
#ifdef RPG_STRESS
    if(area == OUTDOOR)
        count = 12;
#endif

    g.area = area;
    g.player.x = x;
    g.player.y = y;
    g.player.facing = facing;
    g.player.walking = 0;
    g.npc_count = 0;
    g.portal_lock = 1;

    for(i = start; i < (uint8_t)(start + count); ++i) {
        NpcDef const PROGMEM* d = &NPC_DEFS[i];
        uint16_t nx = d->x, ny = d->y;

        /* Reconstruct safely even if a save occupies an ambient spawn. */
        if(actors_overlap(nx, ny, x, y) || !position_clear(g, nx, ny, -1)) {
            bool found = false;
            for(ny = d->min_y; ny <= d->max_y && !found; ny += 8) {
                for(nx = d->min_x; nx <= d->max_x; nx += 8)
                    if(!actors_overlap(nx, ny, x, y) && position_clear(g, nx, ny, -1)) {
                        found = true;
                        break;
                    }
                if(found)
                    break;
            }
            if(!found)
                continue;
        }

        g.npcs[g.npc_count].actor.x = nx;
        g.npcs[g.npc_count].actor.y = ny;
        g.npcs[g.npc_count].actor.facing = SOUTH;
        g.npcs[g.npc_count].actor.walking = 0;
        g.npcs[g.npc_count].id = d->id;
        g.npcs[g.npc_count].timer = (uint8_t)(8 + i * 3);
        ++g.npc_count;
    }
}

bool portals_update(Game& g) {
    uint8_t tx = (uint8_t)((g.player.x + 4) >> 3), ty = (uint8_t)((g.player.y + 6) >> 3);
    bool on = world_tile(g.area, tx, ty) == 3;

    if(g.portal_lock) {
        if(!on)
            g.portal_lock = 0;
        return false;
    }

    if(!on)
        return false;

    if(g.area == OUTDOOR)
        world_enter(g, INTERIOR, 16, 56, NORTH);
    else
        world_enter(g, OUTDOOR, 96, 48, SOUTH);
    return true;
}

void npcs_update(Game& g) {
    uint8_t i;

    for(i = 0; i < g.npc_count; ++i) {
        Npc* n = &g.npcs[i];
        NpcDef const PROGMEM* d = npc_def(n->id);

        if(!d->behavior)
            continue;

        if(n->timer)
            --n->timer;
        else {
            uint16_t r = rng_next(g);
            n->actor.facing = (uint8_t)(r & 3);
            n->actor.walking = (uint8_t)((r & 12) != 0);
            n->timer = (uint8_t)(12 + (r >> 8) % 24);
        }

        if(n->actor.walking) {
            uint16_t x = n->actor.x, y = n->actor.y;
            uint8_t f = n->actor.facing;

            if(f == NORTH)
                --y;
            if(f == SOUTH)
                ++y;
            if(f == WEST)
                --x;
            if(f == EAST)
                ++x;

            if(x < d->min_x || x > d->max_x || y < d->min_y || y > d->max_y ||
               !actor_move(g, &n->actor, f, 1, (int8_t)i)) {
                n->actor.walking = 0;
                n->timer = 6;
            }
        }
    }
}
