#include "npc.h"
#include "chunk.h"
#include "logic/npc_logic.h"

#include <string.h>

Npc* npc_find(NPC_ID id) {
    if(id)
        for(auto& n : game.npcs.actors)
            if(n.id == id)
                return &n;
    return nullptr;
}

bool npc_visible(uint16_t x, uint16_t y) {
    return x < game.camera.x + 128 && x + TILE > game.camera.x && y < game.camera.y + 64 &&
           y + TILE > game.camera.y;
}

bool npc_overlap(uint16_t x, uint16_t y, uint16_t ox, uint16_t oy) {
    // Both use the player's 12x10 foot box at (2,6).
    return x < ox + FOOT_WIDTH && ox < x + FOOT_WIDTH && y < oy + FOOT_HEIGHT &&
           oy < y + FOOT_HEIGHT;
}

void npc_reset() {
    for(auto const& n : game.npcs.actors)
        if(n.id)
            npc_on_retire(n, NPC_RETIRED_RESET);
    memset(&game.npcs, 0, sizeof(game.npcs));
    game.npcs.push_direction = STILL;
    npc_logic_restore();
}

void npc_begin_frame() {
    game.npcs.request_count = 0;
    for(auto& n : game.npcs.actors) {
        if(!(n.state & NPC_PUSHED)) {
            n.push = 0;
            n.state &= ~NPC_CAN_DETOUR;
        }
        n.state &= ~NPC_PUSHED;
        if(!npc_overlap(n.actor.x, n.actor.y, game.player.x, game.player.y))
            n.state &= ~NPC_PASS_PLAYER;
    }
}

static void saturate(uint8_t& value) {
    if(value != 255)
        ++value;
}

static bool higher_priority(NPC_ID a, NPC_ID b) {
    uint8_t pa = npc_definition(a)->priority, pb = npc_definition(b)->priority;
    return pa > pb || (pa == pb && a < b);
}

void npc_request(NPC_ID id, TilePos anchor) {
    // Visible incumbents already own their slot and retain behavior/position.
    // Their canonical reassignment is resolved when they leave the view.
    if(npc_find(id))
        return;
    if(!npc_definition(id))
        return;
    TilePos home = npc_home(id, npc_placement(id));
    if(home.area != game.area || home.area != anchor.area || home.x != anchor.x ||
       home.y != anchor.y)
        return;
    Camera camera = game_camera();
    uint16_t x = (uint16_t)home.x * TILE, y = (uint16_t)home.y * TILE;
    if(x >= camera.x + 128 || x + TILE <= camera.x || y >= camera.y + 64 || y + TILE <= camera.y)
        return;
    auto& pool = game.npcs;
    for(uint8_t i = 0; i < pool.request_count; ++i)
        if(pool.requests[i] == id)
            return;
    uint8_t index = 0;
    while(index < pool.request_count && !higher_priority(id, pool.requests[index]))
        ++index;
    if(pool.request_count == NPC_CAPACITY) {
        saturate(pool.capacity_faults);
        if(index == NPC_CAPACITY)
            return;
    } else {
        ++pool.request_count;
    }
    for(uint8_t i = pool.request_count - 1; i > index; --i)
        pool.requests[i] = pool.requests[i - 1];
    pool.requests[index] = id;
}

static bool position_clear(Npc const* self, uint16_t x, uint16_t y) {
    if(!terrain_clear(x, y))
        return false;
    if((!self || !(self->state & NPC_PASS_PLAYER)) &&
       npc_overlap(x, y, game.player.x, game.player.y))
        return false;
    for(auto const& other : game.npcs.actors)
        if(other.id && &other != self && npc_overlap(x, y, other.actor.x, other.actor.y))
            return false;
    return true;
}

static bool foot_cached(Npc const& n) {
    uint16_t left = (uint16_t)game.cache.origin_tx * TILE;
    uint16_t top = (uint16_t)game.cache.origin_ty * TILE;
    return n.actor.x + FOOT_LEFT >= left && n.actor.y + FOOT_TOP >= top &&
           n.actor.x + FOOT_RIGHT < left + CACHE_WIDTH * TILE &&
           n.actor.y + FOOT_BOTTOM < top + CACHE_HEIGHT * TILE;
}

static bool spawn_clear(Npc const* self, uint16_t x, uint16_t y) {
    return npc_visible(x, y) && position_clear(self, x, y) &&
           !tile_portal(cached_tile({game.area, (uint8_t)((x + FOOT_CENTER_X) >> TILE_SHIFT),
                                     (uint8_t)((y + FOOT_CENTER_Y) >> TILE_SHIFT)}));
}

static void retire(Npc& n, NPC_RETIRE_REASON reason) {
    npc_on_retire(n, reason);
    n.id = NPC_NONE;
}

void npc_reconcile() {
    // Existing visible actors have retention priority, even after their assignment changes.
    for(auto& n : game.npcs.actors) {
        if(n.id && !npc_visible(n.actor.x, n.actor.y))
            retire(n, NPC_RETIRED_OFFSCREEN);
    }
    for(uint8_t i = 0; i < game.npcs.request_count; ++i) {
        NPC_ID id = game.npcs.requests[i];
        if(npc_find(id))
            continue;
        uint8_t placement = npc_placement(id);
        TilePos home = npc_home(id, placement);
        uint16_t x = (uint16_t)home.x * TILE, y = (uint16_t)home.y * TILE;
        if(home.area != game.area || !spawn_clear(nullptr, x, y)) {
            saturate(game.npcs.activation_deferrals);
            continue;
        }
        Npc* slot = nullptr;
        for(auto& n : game.npcs.actors)
            if(!n.id) {
                slot = &n;
                break;
            }
        if(!slot) {
            saturate(game.npcs.capacity_faults);
            continue;
        }
        *slot = {};
        auto def = npc_definition(id);
        slot->id = id;
        slot->placement = placement;
        slot->actor = {x, y, def->facing, 0};
        slot->behavior = def->behavior;
        slot->route = def->route;
        npc_on_activate(*slot);
        if(slot->behavior == NPC_ROUTE)
            slot->state |= NPC_ROUTE_PENDING;
    }
}

static bool leashed(Npc const& n, uint16_t x, uint16_t y) {
    if(n.behavior != NPC_WANDER)
        return true;
    TilePos home = npc_home(n.id, n.placement);
    auto def = npc_definition(n.id);
    int16_t dx = (int16_t)x - (int16_t)((uint16_t)home.x * TILE);
    int16_t dy = (int16_t)y - (int16_t)((uint16_t)home.y * TILE);
    return dx >= -(int16_t)def->leash_x && dx <= def->leash_x && dy >= -(int16_t)def->leash_y &&
           dy <= def->leash_y;
}

static bool step(Npc& n, DIRECTION dir) {
    int16_t x = (int16_t)n.actor.x, y = (int16_t)n.actor.y;
    n.actor.facing = dir;
    if(dir == NORTH)
        --y;
    if(dir == EAST)
        ++x;
    if(dir == SOUTH)
        ++y;
    if(dir == WEST)
        --x;
    if(x < 0 || y < 0 || !leashed(n, (uint16_t)x, (uint16_t)y) ||
       !position_clear(&n, (uint16_t)x, (uint16_t)y))
        return false;
    n.actor.x = (uint16_t)x;
    n.actor.y = (uint16_t)y;
    n.actor.walking = 1;
    n.state &= ~NPC_CAN_DETOUR;
    return true;
}

static bool steer(Npc& n, PixelPos target) {
    // Two attempts maximum; alternate the preferred axis after obstruction.
    DIRECTION dx = target.x > n.actor.x ? EAST : WEST;
    DIRECTION dy = target.y > n.actor.y ? SOUTH : NORTH;
    if(n.stuck & 1) {
        if(target.y != n.actor.y && step(n, dy))
            return true;
        return target.x != n.actor.x && step(n, dx);
    }
    if(target.x != n.actor.x && step(n, dx))
        return true;
    return target.y != n.actor.y && step(n, dy);
}

bool npc_move_to(NPC_ID id, uint16_t x, uint16_t y) {
    Npc* n = npc_find(id);
    if(!n || !game.dimensions.pixel_bounds(x, y))
        return false;
    n->target = {x, y};
    n->behavior = NPC_TARGET;
    n->timer = n->stuck = 0;
    n->state &= ~(NPC_TARGET_FAILED | NPC_YIELDING | NPC_CAN_DETOUR | NPC_ROUTE_PENDING);
    n->push = 0;
    return true;
}

bool npc_follow_route(NPC_ID id, uint8_t route) {
    Npc* n = npc_find(id);
    auto path = npc_route(route);
    if(!n || !path || path->area != game.area)
        return false;
    n->route = route;
    n->behavior = NPC_ROUTE;
    n->waypoint = n->timer = n->stuck = 0;
    n->state &= ~(NPC_BACKWARD | NPC_TARGET_FAILED | NPC_YIELDING | NPC_CAN_DETOUR);
    n->state |= NPC_ROUTE_PENDING;
    n->push = 0;
    return true;
}

static void waypoint_reached(Npc& n, NpcRoute const PROGMEM* path) {
    n.timer = path->pause;
    if(n.state & NPC_BACKWARD) {
        if(n.waypoint)
            --n.waypoint;
        else {
            n.state &= ~NPC_BACKWARD;
            n.waypoint = 1;
        }
    } else if(n.waypoint + 1 < path->count) {
        ++n.waypoint;
    } else if(path->mode == ROUTE_LOOP) {
        n.waypoint = 0;
    } else if(path->mode == ROUTE_REVERSE) {
        n.state |= NPC_BACKWARD;
        --n.waypoint;
    } else {
        n.behavior = NPC_STAND;
        npc_on_event(n, NPC_EVENT_ROUTE_COMPLETED);
    }
}

void npc_update() {
    // Called once after the complete effective cache, never from a chunk handler.
    for(auto& n : game.npcs.actors) {
        if(!n.id)
            continue;
        n.actor.walking = 0;
        // A clipped sprite may still be visible after its feet leave the cache.
        // Freeze that sliver until cached again or fully offscreen; never eject it
        // against the outside sentinel or simulate uncached terrain.
        if(!foot_cached(n))
            continue;
        if(n.state & NPC_YIELDING) {
            // A compact interruption: high nibble is direction, low is distance.
            // Preserve the ordinary behavior, route/target and pause timer.
            uint8_t remaining = n.stuck & 15;
            if(remaining && step(n, (DIRECTION)(n.stuck >> 4))) {
                --n.stuck;
                if(remaining != 1)
                    continue;
            } else {
                // A dynamic obstruction must not turn a planned yield into a trap.
                n.state |= NPC_PASS_PLAYER;
            }
            n.state &= ~NPC_YIELDING;
            n.stuck = 0;
            continue;
        }
        auto def = npc_definition(n.id);
        if(n.timer) {
            --n.timer;
            if(n.behavior != NPC_WANDER || !(n.state & NPC_WALK_PHASE))
                continue;
        }
        if(game.ticks & (def->period - 1))
            continue;
        if(n.behavior == NPC_WANDER) {
            if(!n.timer) {
                n.state ^= NPC_WALK_PHASE;
                uint8_t random = (uint8_t)rng_next();
                n.actor.facing = (DIRECTION)(random & 3);
                n.timer = (n.state & NPC_WALK_PHASE ? def->walk : def->pause) + (random & 7);
            }
            if((n.state & NPC_WALK_PHASE) && !step(n, n.actor.facing)) {
                n.state &= ~NPC_WALK_PHASE;
                n.timer = def->pause;
            }
        } else if(n.behavior == NPC_TARGET || n.behavior == NPC_ROUTE) {
            PixelPos target;
            auto path = n.behavior == NPC_ROUTE ? npc_route(n.route) : nullptr;
            if(n.behavior == NPC_ROUTE) {
                if(!path || path->area != game.area || n.waypoint >= path->count) {
                    n.behavior = NPC_STAND;
                    n.state &= ~NPC_ROUTE_PENDING;
                    npc_on_event(n, NPC_EVENT_ROUTE_INVALID);
                    continue;
                }
                if(n.state & NPC_ROUTE_PENDING) {
                    n.state &= ~NPC_ROUTE_PENDING;
                    npc_on_event(n, NPC_EVENT_ROUTE_STARTED);
                    // A hook may replace the behavior/route. Simulate it next frame.
                    if(n.behavior != NPC_ROUTE || npc_route(n.route) != path ||
                       (n.state & NPC_ROUTE_PENDING))
                        continue;
                }
                target = {(uint16_t)((uint16_t)path->points[n.waypoint].x * TILE),
                          (uint16_t)((uint16_t)path->points[n.waypoint].y * TILE)};
            } else {
                target = n.target;
            }
            if(n.actor.x == target.x && n.actor.y == target.y) {
                n.stuck = 0;
                if(path)
                    waypoint_reached(n, path);
                else {
                    n.behavior = NPC_STAND;
                    npc_on_event(n, NPC_EVENT_TARGET_REACHED);
                }
            } else if(steer(n, target)) {
                n.stuck = 0;
            } else if(++n.stuck >= NPC_STUCK_FRAMES) {
                n.stuck = 0;
                if(path)
                    n.timer = def->pause; // Bounded wait, retry this authored segment.
                else {
                    n.behavior = NPC_STAND;
                    n.state |= NPC_TARGET_FAILED;
                    npc_on_event(n, NPC_EVENT_TARGET_FAILED);
                }
            }
        }
    }
}

void npc_repair() {
    for(auto& n : game.npcs.actors) {
        if(!n.id || !foot_cached(n) || terrain_clear(n.actor.x, n.actor.y))
            continue;
        bool repaired = false;
        for(uint8_t radius = 1; radius <= TILE && !repaired; ++radius) {
            for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
                int16_t x = (int16_t)n.actor.x, y = (int16_t)n.actor.y;
                if(dir == NORTH)
                    y -= radius;
                if(dir == EAST)
                    x += radius;
                if(dir == SOUTH)
                    y += radius;
                if(dir == WEST)
                    x -= radius;
                if(x >= 0 && y >= 0 && leashed(n, (uint16_t)x, (uint16_t)y) &&
                   spawn_clear(&n, (uint16_t)x, (uint16_t)y)) {
                    n.actor.x = (uint16_t)x;
                    n.actor.y = (uint16_t)y;
                    n.actor.walking = 0;
                    repaired = true;
                    break;
                }
            }
        }
        if(!repaired) {
            saturate(game.npcs.activation_deferrals);
            retire(n, NPC_RETIRED_TERRAIN); // No safe cache position: defer reconstruction.
        }
    }
}

void npc_player_begin(DIRECTION dir) {
    if(dir != game.npcs.push_direction || dir == STILL) {
        for(auto& n : game.npcs.actors) {
            n.push = 0;
            n.state &= ~NPC_CAN_DETOUR;
        }
    }
    game.npcs.push_direction = dir;
}

static bool player_position_clear(uint16_t x, uint16_t y) {
    if(!terrain_clear(x, y))
        return false;
    for(auto const& n : game.npcs.actors)
        if(n.id && !(n.state & NPC_PASS_PLAYER) && npc_overlap(x, y, n.actor.x, n.actor.y))
            return false;
    return true;
}

static bool player_segment(int16_t& x, int16_t& y, DIRECTION dir, uint8_t pixels) {
    while(pixels--) {
        if(dir == NORTH)
            --y;
        if(dir == EAST)
            ++x;
        if(dir == SOUTH)
            ++y;
        if(dir == WEST)
            --x;
        if(x < 0 || y < 0 || !player_position_clear((uint16_t)x, (uint16_t)y))
            return false;
    }
    return true;
}

static bool player_can_detour(Npc const& n, DIRECTION dir) {
    // Two fixed three-segment lanes around this foot box; no path search/grid.
    bool horizontal = dir == EAST || dir == WEST;
    int16_t forward = (int16_t)(horizontal ? n.actor.x : n.actor.y);
    forward += (dir == EAST || dir == SOUTH ? 1 : -1) * (horizontal ? FOOT_WIDTH : FOOT_HEIGHT);
    int16_t advance = forward - (int16_t)(horizontal ? game.player.x : game.player.y);
    if(advance < 0)
        advance = -advance;
    for(uint8_t side = 1; side <= 3; side += 2) {
        DIRECTION escape = (DIRECTION)((dir + side) & 3);
        int16_t offset = (int16_t)(horizontal ? n.actor.y : n.actor.x);
        offset +=
            (escape == EAST || escape == SOUTH ? 1 : -1) * (horizontal ? FOOT_HEIGHT : FOOT_WIDTH);
        offset -= (int16_t)(horizontal ? game.player.y : game.player.x);
        if(offset < 0)
            offset = -offset;
        int16_t x = (int16_t)game.player.x, y = (int16_t)game.player.y;
        if(player_segment(x, y, escape, (uint8_t)offset) &&
           player_segment(x, y, dir, (uint8_t)advance) &&
           player_segment(x, y, (DIRECTION)((escape + 2) & 3), (uint8_t)offset))
            return true;
    }
    return false;
}

static bool plan_yield(Npc& n, uint16_t px, uint16_t py, DIRECTION dir) {
    // Validate a short side step without moving the actor in the collision call.
    for(uint8_t side = 1; side <= 3; side += 2) {
        DIRECTION escape = (DIRECTION)((dir + side) & 3);
        int16_t x = (int16_t)n.actor.x, y = (int16_t)n.actor.y;
        for(uint8_t distance = 1; distance < TILE; ++distance) {
            if(escape == NORTH)
                --y;
            if(escape == EAST)
                ++x;
            if(escape == SOUTH)
                ++y;
            if(escape == WEST)
                --x;
            if(x < 0 || y < 0 || !leashed(n, (uint16_t)x, (uint16_t)y) ||
               !position_clear(&n, (uint16_t)x, (uint16_t)y))
                break;
            if(!npc_overlap(px, py, (uint16_t)x, (uint16_t)y)) {
                n.stuck = (uint8_t)((escape << 4) | distance);
                n.state |= NPC_YIELDING;
                return true;
            }
        }
    }
    return false;
}

bool npc_player_clear(uint16_t x, uint16_t y, DIRECTION dir) {
    bool clear = true;
    for(auto& n : game.npcs.actors) {
        if(!n.id || (n.state & NPC_PASS_PLAYER) || !npc_overlap(x, y, n.actor.x, n.actor.y))
            continue;
        if(!(n.state & NPC_PUSHED)) {
            n.state |= NPC_PUSHED;
            if(!(n.state & NPC_YIELDING)) {
                ++n.push;
                if(n.push >= NPC_PUSH_FRAMES &&
                   (!(n.state & NPC_CAN_DETOUR) || !(n.push & (NPC_PUSH_FRAMES - 1)))) {
                    n.push = NPC_PUSH_FRAMES;
                    if(player_can_detour(n, dir)) {
                        n.state |= NPC_CAN_DETOUR;
                    } else {
                        n.state &= ~NPC_CAN_DETOUR;
                        if(!plan_yield(n, x, y, dir)) {
                            n.state |= NPC_PASS_PLAYER;
                            continue;
                        }
                    }
                }
            }
        }
        clear = false;
    }
    return clear;
}

void npc_interact() {
    Npc* best = nullptr;
    uint16_t nearest = 65535;
    for(auto& n : game.npcs.actors) {
        if(!n.id || !npc_definition(n.id)->talkable)
            continue;
        int16_t dx = (int16_t)n.actor.x - (int16_t)game.player.x;
        int16_t dy = (int16_t)n.actor.y - (int16_t)game.player.y;
        int16_t forward = 0, side = 0;
        switch(game.player.facing) {
            case NORTH:
                forward = -dy;
                side = dx;
                break;
            case EAST:
                forward = dx;
                side = dy;
                break;
            case SOUTH:
                forward = dy;
                side = dx;
                break;
            case WEST:
                forward = -dx;
                side = dy;
                break;
            case STILL:
                continue;
        }
        if(forward < 1 || forward > 24 || side < -10 || side > 10)
            continue;
        uint16_t distance = (uint16_t)forward * TILE + (uint16_t)(side < 0 ? -side : side);
        if(distance < nearest || (distance == nearest && best && n.id < best->id)) {
            best = &n;
            nearest = distance;
        }
    }
    if(!best)
        return;
    game.can_interact = true;
    if(frame_pressed & BTN_A) {
        frame_pressed &= (uint8_t)~BTN_A;
        best->actor.facing = (DIRECTION)((game.player.facing + 2) & 3);
        npc_talk(best->id);
    }
}
