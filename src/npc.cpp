#include "npc.h"
#include "chunk.h"
#include "encounter.h"
#include "logic/enemy_catalog.h"
#include "logic/npc_logic.h"

#include <string.h>

#ifndef RPG_AVM
unsigned npc_site_checks;
#endif

Npc* npc_find(NPC_ID id) {
    if(id)
        for(auto& n : game.npcs.actors) {
            if(!n.id)
                break;
            if(n.id == id)
                return &n;
        }
    return nullptr;
}

void npc_recruit(NPC_ID id) {
    if(!id)
        return;
    auto& pool = game.npcs;
    if(Npc* actor = npc_find(id)) {
        npc_on_retire(*actor, NPC_RETIRED_PARTY);
        uint8_t slot = (uint8_t)(actor - pool.actors);
        while(slot + 1 < NPC_CAPACITY && pool.actors[slot + 1].id) {
            pool.actors[slot] = pool.actors[slot + 1];
            ++slot;
        }
        pool.actors[slot] = {};
    }
}

// draw_actors uses the 16x16 ACTOR_SPRITES at actor.x/y, without an offset.
// A corner sidestep can move along both axes, each by at most this allowance.
static constexpr uint8_t ADMISSION_CLEARANCE = PLAYER_PIXELS_PER_FRAME;

static bool sprite_intersects(uint16_t x, uint16_t y, Camera camera, uint8_t clearance) {
    // Map axes have at most 255 tiles (4080 pixels). Subtract signed coordinates
    // to avoid unsigned underflow; 16-bit deltas cover every map edge.
    int16_t dx = (int16_t)x - (int16_t)camera.x, dy = (int16_t)y - (int16_t)camera.y;
    return dx < 128 + clearance && dx + TILE > -(int16_t)clearance &&
           dy < 64 + clearance && dy + TILE > -(int16_t)clearance;
}

bool npc_visible(uint16_t x, uint16_t y) {
    return sprite_intersects(x, y, camera, 0);
}

static bool protected_view(uint16_t x, uint16_t y) {
    return sprite_intersects(x, y, game_camera(), ADMISSION_CLEARANCE);
}

bool npc_overlap(uint16_t x, uint16_t y, uint16_t ox, uint16_t oy) {
    // Both use the same 8x8 foot box at (4,8), so its offsets cancel here.
    return x < ox + FOOT_WIDTH && ox < x + FOOT_WIDTH && y < oy + FOOT_HEIGHT &&
           oy < y + FOOT_HEIGHT;
}


void npc_begin_frame() {
    for(auto& n : game.npcs.actors) {
        // Occupied slots come first, so the first empty slot ends the scan.
        // Activation clears the entire slot before reuse.
        if(!n.id)
            break;
        if(!(n.state & NPC_PUSHED)) {
            n.push = 0;
            n.state &= ~NPC_CAN_DETOUR;
        }
        n.state &= ~NPC_PUSHED;
        if((n.state & NPC_PASS_PLAYER) &&
           !npc_overlap(n.actor.x, n.actor.y, game.player.x, game.player.y))
            n.state &= ~NPC_PASS_PLAYER;
    }
}

static void saturate(uint8_t& value) {
    if(value != 255)
        ++value;
}

// Whether the feet at (x, y) are free of the player and the other NPCs. Terrain is not checked.
static bool occupancy_clear(Npc const* self, uint16_t x, uint16_t y) {
    if((!self || !(self->state & NPC_PASS_PLAYER)) &&
       npc_overlap(x, y, game.player.x, game.player.y))
        return false;
    for(auto const& other : game.npcs.actors) {
        if(!other.id)
            break;
        if(&other != self && npc_overlap(x, y, other.actor.x, other.actor.y))
            return false;
    }
    return true;
}

static bool position_clear(Npc const* self, uint16_t x, uint16_t y) {
    return terrain_clear(x, y) && occupancy_clear(self, x, y);
}

// Whether a one-pixel step of n to (x, y) is clear. Known-clear feet need only the tiles the step
// enters; otherwise the whole footprint is checked.
static bool step_clear(Npc const& n, uint16_t x, uint16_t y, DIRECTION dir) {
    bool terrain = n.policy_pending & 4 ? terrain_step_clear(x, y, dir) : terrain_clear(x, y);
    if(!terrain)
        return false;
    if(npc_is_enemy(n.id) && npc_overlap(x, y, game.player.x, game.player.y)) {
        for(auto const& other : game.npcs.actors) {
            if(!other.id)
                break;
            if(&other != &n && npc_overlap(x, y, other.actor.x, other.actor.y))
                return false;
        }
        encounter_contact(n.id);
        return false;
    }
    return occupancy_clear(&n, x, y);
}

static bool foot_cached_at(uint16_t x, uint16_t y) {
    // Normalize the top-left foot pixel; two unsigned range checks cover both
    // edges of each axis, including a footprint before the cache origin.
    uint16_t dx = (uint16_t)(x + FOOT_LEFT - (uint16_t)cache.origin_tx * TILE);
    uint16_t dy = (uint16_t)(y + FOOT_TOP - (uint16_t)cache.origin_ty * TILE);
    return dx <= CACHE_WIDTH * TILE - FOOT_WIDTH && dy <= CACHE_HEIGHT * TILE - FOOT_HEIGHT;
}

static bool foot_cached(Npc const& n) {
    return foot_cached_at(n.actor.x, n.actor.y);
}

// A scene casts its NPCs when it is entered. Afterwards a cast may happen only out of view, so an
// NPC is never seen appearing. Off-cache travel completes through safe unloading;
// cached simulation moves only one pixel per step.
// Casts id at its home. A home that is blocked, or in view after entry, waits until it is clear
// and out of view. The lifecycle functions stay out of line so their frames do not grow the frame
// of update_visible_world, which the stack budget counts on every path.
static bool candidate_clear(NPC_ID id, TilePos site) {
    if(site.area != game.area || party_contains(id) || npc_find(id))
        return false;
    uint16_t x = (uint16_t)site.x * TILE, y = (uint16_t)site.y * TILE;
    return foot_cached_at(x, y) && (game.npcs.entering || !protected_view(x, y)) &&
           !tile_portal(cached_tile(site)) && position_clear(nullptr, x, y);
}

// This activation bit lets the normal NPC loop skip pursuit without a flash read.
static constexpr uint8_t PURSUIT_ENABLED = 8;

static bool cast_npc(NPC_ID id, TilePos home, bool revalidate) {
    auto& pool = game.npcs;
    // The first cast uses admission's world state. Every successful cast invokes a hook:
    // subsequent casts must recheck policy, party, camera, terrain and all occupancy.
    Npc* slot = nullptr;
    for(auto& n : pool.actors)
        if(!n.id) {
            slot = &n;
            break;
        }
    if(!slot) {
        saturate(pool.activation_deferrals);
        return false;
    }
    if(revalidate && (npc_location(id) != home || !candidate_clear(id, home))) {
        saturate(pool.activation_deferrals);
        return false;
    }
    auto def = npc_definition(id);
    if(!def)
        return false;
    uint16_t x = (uint16_t)home.x * TILE, y = (uint16_t)home.y * TILE;
    *slot = {};
    slot->id = id;
    slot->assigned_site = home;
    slot->policy_pending = (npc_location_is_fixed(id) ? 2 : 0) | 4;
    slot->actor = {x, y, def->facing, 0};
    slot->behavior = def->behavior;
    if(slot->behavior == NPC_CHASE) {
        slot->behavior = NPC_WANDER;
        slot->policy_pending |= PURSUIT_ENABLED;
    }
    slot->route = def->route;
    npc_on_activate(*slot);
    if(slot->behavior == NPC_ROUTE)
        slot->state |= NPC_ROUTE_PENDING;
    return true;
}

// Walks an NPC that overlaps solid terrain one pixel toward the nearest clear spot along a
// cardinal ray. The distance to that spot drops by one per step, so the NPC walks out without a
// jump. Other NPCs and the player still block the step.
__attribute__((noinline)) static bool escape_step(Npc& n) {
    int16_t x = (int16_t)n.actor.x, y = (int16_t)n.actor.y;
    for(int16_t radius = 1; radius <= TILE; ++radius)
        for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
            int16_t dx = dir == EAST ? 1 : dir == WEST ? -1 : 0;
            int16_t dy = dir == SOUTH ? 1 : dir == NORTH ? -1 : 0;
            if(!terrain_clear((uint16_t)(x + dx * radius), (uint16_t)(y + dy * radius)))
                continue;
            int16_t nx = x + dx, ny = y + dy;
            if(!(n.state & NPC_PASS_PLAYER) &&
               npc_overlap((uint16_t)nx, (uint16_t)ny, game.player.x, game.player.y))
                return false;
            for(auto const& other : game.npcs.actors) {
                if(!other.id)
                    break;
                if(&other != &n &&
                   npc_overlap((uint16_t)nx, (uint16_t)ny, other.actor.x, other.actor.y))
                    return false;
            }
            n.policy_pending &= (uint8_t)~4;
            n.actor.x = (uint16_t)nx;
            n.actor.y = (uint16_t)ny;
            n.actor.facing = dir;
            return true;
        }
    return false;
}

namespace {
void retire_slot(uint8_t index, NPC_RETIRE_REASON reason = NPC_RETIRED_UNLOADED) {
    auto& pool = game.npcs;
    npc_on_retire(pool.actors[index], reason);
    while(index + 1 < NPC_CAPACITY && pool.actors[index + 1].id) {
        pool.actors[index] = pool.actors[index + 1];
        ++index;
    }
    pool.actors[index] = {};
}
} // namespace

void npc_retire_enemy(NPC_ID id) {
    if(npc_is_enemy(id))
        if(auto* actor = npc_find(id))
            retire_slot((uint8_t)(actor - game.npcs.actors), NPC_RETIRED_ENCOUNTER);
}

RPG_NOINLINE void npc_stage_begin() {
#ifndef RPG_AVM
    npc_site_checks = 0;
#endif
    candidate_count = 0;
    auto& pool = game.npcs;
    Npc* current = pool.actors;
    uint8_t i = 0;
    for(; i < NPC_CAPACITY && current->id;) {
        auto& n = *current;
        if(npc_is_enemy(n.id)) {
            bool shown = protected_view(n.actor.x, n.actor.y);
            if(!shown && (npc_location(n.id) != n.assigned_site ||
                          (!foot_cached(n) &&
                           !foot_cached_at((uint16_t)n.assigned_site.x * TILE,
                                           (uint16_t)n.assigned_site.y * TILE)))) {
                retire_slot(i);
                continue;
            }
            ++i;
            ++current;
            continue;
        }
        // A one-site essential policy can reuse its initialized snapshot.
        if(!(n.policy_pending & 2)) {
            TilePos site = npc_location(n.id);
            if(site != n.assigned_site) {
                n.assigned_site = site;
                n.policy_pending |= 1;
            }
        }
        // Scripted movement pins the actor until it completes, including off cache.
        if(n.behavior == NPC_TARGET || n.behavior == NPC_ROUTE) {
            ++i;
            ++current;
            continue;
        }
        if(n.policy_pending & 1) {
            n.policy_pending &= (uint8_t)~1;
            n.behavior = n.assigned_site.area == game.area ? NPC_TRAVEL : NPC_STAND;
            n.timer = n.stuck = n.push = n.detour = 0;
            n.state = NPC_STATE_NONE;
        }
        if(!foot_cached(n) && !protected_view(n.actor.x, n.actor.y)) {
            // A wanderer may leave the cache while its home is still local.
            // Keep its state and slot: npc_update freezes uncached feet. Retiring
            // here would immediately admit the same identity back at its home.
            // Travel must still complete and emit its event before unloading.
            if((n.behavior == NPC_STAND || n.behavior == NPC_WANDER) &&
               n.assigned_site.area == game.area &&
               foot_cached_at((uint16_t)n.assigned_site.x * TILE,
                              (uint16_t)n.assigned_site.y * TILE)) {
                ++i;
                ++current;
                continue;
            }
            if(n.behavior == NPC_TRAVEL) {
                n.behavior = NPC_STAND;
                n.actor.walking = 0;
                n.timer = n.stuck = n.push = n.detour = 0;
                n.state = NPC_STATE_NONE;
                npc_on_event(n, NPC_EVENT_TARGET_REACHED);
                // Movement hooks may start another script; keep its full state pinned.
                if(n.behavior == NPC_TARGET || n.behavior == NPC_ROUTE) {
                    ++i;
                    ++current;
                    continue;
                }
            }
            retire_slot(i);
            continue;
        }
        ++i;
        ++current;
    }
}

RPG_NOINLINE void npc_detail::offer_candidate(NPC_ID id, TilePos site) {
    auto def = npc_definition(id);
    if(!def)
        return;
    uint8_t priority = def->priority;
    uint8_t at = 0;
    while(at < candidate_count) {
        auto const& c = candidates[at];
        if(c.id == id)
            return;
        if(c.priority < priority || (c.priority == priority && c.id > id))
            break;
        ++at;
    }
    uint16_t x = (uint16_t)site.x * TILE, y = (uint16_t)site.y * TILE;
    if(at == NPC_CAPACITY || !foot_cached_at(x, y) ||
       (!game.npcs.entering && protected_view(x, y)) || tile_portal(cached_tile(site)) ||
       !position_clear(nullptr, x, y))
        return;
    if(candidate_count < NPC_CAPACITY)
        ++candidate_count;
    for(uint8_t i = candidate_count - 1; i > at; --i)
        candidates[i] = candidates[i - 1];
    candidates[at] = {id, site, priority};
}

RPG_NOINLINE void npc_stage_end() {
    bool revalidate = false;
    for(uint8_t i = 0; i < candidate_count; ++i)
        if(cast_npc(candidates[i].id, candidates[i].site, revalidate))
            revalidate = true;
    candidate_count = 0;
    game.npcs.entering = 0;
}

void npc_reset() {
    for(auto const& n : game.npcs.actors) {
        if(!n.id)
            break;
        npc_on_retire(n, NPC_RETIRED_RESET);
    }
    memset(&game.npcs, 0, sizeof(game.npcs));
    candidate_count = 0;
    game.npcs.push_direction = STILL;
    game.npcs.entering = 1;
    npc_logic_restore();
}

static uint16_t distance(uint16_t x, uint16_t y, PixelPos target) {
    return (x > target.x ? x - target.x : target.x - x) +
           (y > target.y ? y - target.y : target.y - y);
}

static PixelPos home_pixel(Npc const& n) {
    return {(uint16_t)n.assigned_site.x * TILE, (uint16_t)n.assigned_site.y * TILE};
}

static bool leashed(Npc const& n, uint16_t x, uint16_t y) {
    if(n.behavior == NPC_WANDER) {
        PixelPos home = home_pixel(n);
        auto def = npc_definition(n.id);
        int16_t dx = (int16_t)x - (int16_t)home.x;
        int16_t dy = (int16_t)y - (int16_t)home.y;
        return dx >= -(int16_t)def->leash_x && dx <= def->leash_x &&
               dy >= -(int16_t)def->leash_y && dy <= def->leash_y;
    }
    if(n.behavior == NPC_CHASE || n.behavior == NPC_RETURN)
        return distance(x, y, home_pixel(n)) <= enemy_pursuit(n.id).leash;
    return true;
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
       !step_clear(n, (uint16_t)x, (uint16_t)y, dir))
        return false;
    n.policy_pending |= 4;
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

// A detour packs into Npc::detour: sidestep pixels left (bits 0-3), detours begun on this leg
// (bits 4-5), and the side (bits 6-7). Sidesteps left above zero means a detour is under way.
static uint8_t detour_left(Npc const& n) {
    return n.detour & 15;
}

static uint8_t detour_used(Npc const& n) {
    return (n.detour >> 4) & 3;
}

static DIRECTION detour_side(Npc const& n) {
    return (DIRECTION)(n.detour >> 6);
}

static void detour_pack(Npc& n, uint8_t left, uint8_t used, DIRECTION side) {
    n.detour = (uint8_t)(left | used << 4 | side << 6);
}

static DIRECTION opposite(DIRECTION dir) {
    return (DIRECTION)((dir + 2) & 3);
}

// Starts a detour around the obstacle ahead. The first sidestep goes toward preferred when that
// is clear, and away from it otherwise.
__attribute__((noinline)) static bool detour_begin(Npc& n, DIRECTION ahead, DIRECTION preferred) {
    uint8_t used = detour_used(n);
    if(used >= NPC_DETOURS_PER_LEG)
        return false;
    DIRECTION side = preferred;
    if(!step(n, side)) {
        side = opposite(preferred);
        if(!step(n, side))
            return false;
    }
    n.actor.facing = ahead;
    detour_pack(n, NPC_DETOUR_MAX - 1, used + 1, side);
    return true;
}

// Continues a detour. The NPC rejoins its way ahead as soon as that is clear; until then it
// sidesteps, while its budget lasts.
__attribute__((noinline)) static bool detour_continue(Npc& n) {
    DIRECTION ahead = n.actor.facing, side = detour_side(n);
    uint8_t left = detour_left(n), used = detour_used(n);
    if(step(n, ahead)) {
        detour_pack(n, 0, used, side);
        return true;
    }
    if(left && step(n, side)) {
        n.actor.facing = ahead;
        detour_pack(n, left - 1, used, side);
        return true;
    }
    n.actor.facing = ahead;
    detour_pack(n, 0, used, side);
    return false;
}

// The way a steering NPC is heading: along its longer axis toward the target.
static DIRECTION toward_ahead(Npc const& n, PixelPos target) {
    int dx = (int)target.x - (int)n.actor.x, dy = (int)target.y - (int)n.actor.y;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if(ax >= ay)
        return dx < 0 ? WEST : EAST;
    return dy < 0 ? NORTH : SOUTH;
}

// The side to sidestep to: toward the target across the way ahead, positive when level with it.
static DIRECTION toward_side(Npc const& n, PixelPos target, DIRECTION ahead) {
    if(ahead == EAST || ahead == WEST)
        return target.y < n.actor.y ? NORTH : SOUTH;
    return target.x < n.actor.x ? WEST : EAST;
}

// Whether the pixel one step ahead lies inside the NPC's limits: its leash, and the terrain cache
// that its feet need. Past either one, the limit is not an obstacle, so an NPC waits or stops
// instead of detouring.
static bool ahead_within_limits(Npc const& n, DIRECTION dir) {
    int16_t x = (int16_t)n.actor.x, y = (int16_t)n.actor.y;
    if(dir == NORTH)
        --y;
    if(dir == EAST)
        ++x;
    if(dir == SOUTH)
        ++y;
    if(dir == WEST)
        --x;
    return x >= 0 && y >= 0 && leashed(n, (uint16_t)x, (uint16_t)y) &&
           foot_cached_at((uint16_t)x, (uint16_t)y);
}

// A steering NPC walks straight at its target. Blocked for a brief pause, it detours around the
// obstacle instead of standing until the target fails.
__attribute__((noinline)) static bool steer_or_detour(Npc& n, PixelPos target, uint8_t period) {
    if(detour_left(n)) {
        if(n.behavior != NPC_CHASE || n.actor.facing == toward_ahead(n, target))
            return detour_continue(n);
        n.detour = 0;
    }
    if(steer(n, target))
        return true;
    if(n.stuck + period < NPC_DETOUR_FRAMES)
        return false;
    DIRECTION ahead = toward_ahead(n, target);
    if(!ahead_within_limits(n, ahead))
        return false; // Waiting for the cache to cover the way ahead is not a detour.
    return detour_begin(n, ahead, toward_side(n, target, ahead));
}

// A wanderer keeps its heading. Blocked, it pauses briefly, then walks around the obstacle toward
// its home. It stops walking only when no detour is possible.
__attribute__((noinline)) static bool wander_walk(Npc& n, uint8_t period) {
    if(detour_left(n))
        return detour_continue(n);
    if(step(n, n.actor.facing)) {
        n.stuck = 0;
        return true;
    }
    if(!ahead_within_limits(n, n.actor.facing))
        return false;
    if(n.stuck + period < NPC_DETOUR_FRAMES) {
        n.stuck += period;
        return true;
    }
    TilePos home = n.assigned_site;
    PixelPos anchor = {(uint16_t)home.x * TILE, (uint16_t)home.y * TILE};
    DIRECTION ahead = n.actor.facing;
    if(!detour_begin(n, ahead, toward_side(n, anchor, ahead)))
        return false;
    n.stuck = 0;
    return true;
}

bool npc_move_to(NPC_ID id, uint16_t x, uint16_t y) {
    Npc* n = npc_find(id);
    if(!n || !game.dimensions.pixel_bounds(x, y))
        return false;
    n->policy_pending &= (uint8_t)~4;
    n->target = {x, y};
    n->behavior = NPC_TARGET;
    n->timer = n->stuck = 0;
    n->detour = 0;
    n->state &= ~(NPC_TARGET_FAILED | NPC_YIELDING | NPC_CAN_DETOUR | NPC_ROUTE_PENDING);
    n->push = 0;
    return true;
}

bool npc_follow_route(NPC_ID id, uint8_t route) {
    Npc* n = npc_find(id);
    auto path = npc_route(route);
    if(!n || !path || path->area != game.area)
        return false;
    n->policy_pending &= (uint8_t)~4;
    n->route = route;
    n->behavior = NPC_ROUTE;
    n->waypoint = n->timer = n->stuck = 0;
    n->detour = 0;
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

// Returning home must finish before detection resumes, including when the player
// waits just beyond the leash.
static RPG_NOINLINE void update_pursuit(Npc& n, uint8_t pause) {
    EnemyPursuit rules = enemy_pursuit(n.id);
    PixelPos home = home_pixel(n);
    PixelPos player{game.player.x, game.player.y};
    uint16_t range = distance(n.actor.x, n.actor.y, player);
    uint16_t from_home = distance(n.actor.x, n.actor.y, home);
    if(n.behavior == NPC_WANDER && range <= rules.detect &&
       distance(player.x, player.y, home) <= rules.leash) {
        n.behavior = NPC_CHASE;
        n.timer = n.stuck = n.detour = 0;
        n.state = NPC_STATE_NONE;
    }
    if(n.behavior == NPC_CHASE && (range > rules.disengage || from_home >= rules.leash)) {
        n.behavior = NPC_RETURN;
        n.stuck = n.detour = 0;
    }
    if(n.behavior == NPC_RETURN && !from_home) {
        n.behavior = NPC_WANDER;
        n.timer = pause;
    }
}

void npc_update() {
    // Called once after the complete effective cache, never from a chunk handler.
    for(auto& n : game.npcs.actors) {
        if(!n.id)
            break;
        // Check between actors as well: an event hook may edit terrain while
        // this update is in progress. Reloads also invalidate discarded edits.
        if(game.npcs.terrain_dirty) {
            for(auto& other : game.npcs.actors) {
                if(!other.id)
                    break;
                other.policy_pending &= (uint8_t)~4;
            }
            game.npcs.terrain_dirty = 0;
        }
        // A settled actor with known-clear feet has no movement or timer work this frame.
        if(n.behavior == NPC_STAND && (n.policy_pending & 4) &&
           !(n.policy_pending & PURSUIT_ENABLED) && !(n.state & NPC_YIELDING) &&
           !n.actor.walking && !n.timer)
            continue;
        // A clipped sprite may still be visible after its feet leave the cache.
        // Freeze that sliver until cached again or fully offscreen; never eject it
        // against the outside sentinel or simulate uncached terrain.
        if(!foot_cached(n)) {
            n.actor.walking = 0;
            continue;
        }
        auto def = npc_definition(n.id);
        // An NPC steps once every def->period frames. Its walk pose holds between steps.
        bool step_frame = !(game.ticks & (def->period - 1));
        if(step_frame)
            n.actor.walking = 0;
        // A tile change can leave an NPC overlapping solid terrain. It walks out one pixel at a
        // time, never in a jump. If it cannot move, its ordinary logic still runs, so a blocked
        // target can still fail.
        if(step_frame && !(n.policy_pending & 4)) {
            if(terrain_clear(n.actor.x, n.actor.y)) {
                n.policy_pending |= 4;
            } else if(escape_step(n)) {
                n.actor.walking = 1;
                continue;
            }
        }
        if((n.policy_pending & PURSUIT_ENABLED) && step_frame)
            update_pursuit(n, def->pause);
        if(n.state & NPC_YIELDING) {
            if(!step_frame)
                continue;
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
        if(n.timer) {
            --n.timer;
            if(n.behavior != NPC_WANDER || !(n.state & NPC_WALK_PHASE))
                continue;
        }
        if(!step_frame)
            continue;
        if(n.behavior == NPC_WANDER) {
            if(!n.timer) {
                n.state ^= NPC_WALK_PHASE;
                uint8_t random = (uint8_t)rng_next();
                n.actor.facing = (DIRECTION)(random & 3);
                // Jitter doubles from 0 to 7 ticks to 0 to 15 frames.
                n.timer = (n.state & NPC_WALK_PHASE ? def->walk : def->pause) + (random & 15);
                n.stuck = 0;
                n.detour = 0;
            }
            if((n.state & NPC_WALK_PHASE) && !wander_walk(n, def->period)) {
                n.state &= ~NPC_WALK_PHASE;
                n.timer = def->pause;
                n.stuck = 0;
            }
        } else if(n.behavior == NPC_TARGET || n.behavior == NPC_ROUTE || n.behavior == NPC_TRAVEL ||
                  n.behavior == NPC_CHASE || n.behavior == NPC_RETURN) {
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
            } else if(n.behavior == NPC_TRAVEL || n.behavior == NPC_RETURN) {
                target = home_pixel(n);
            } else if(n.behavior == NPC_CHASE) {
                target = {game.player.x, game.player.y};
            } else {
                target = n.target;
            }
            if(n.actor.x == target.x && n.actor.y == target.y) {
                n.stuck = 0;
                n.detour = 0;
                if(path)
                    waypoint_reached(n, path);
                else {
                    n.behavior = NPC_STAND;
                    npc_on_event(n, NPC_EVENT_TARGET_REACHED);
                }
            } else if(steer_or_detour(n, target, def->period)) {
                n.stuck = 0;
            } else if((n.stuck += def->period) >= NPC_STUCK_FRAMES) {
                n.stuck = 0;
                n.detour = 0;
                if(path)
                    n.timer = def->pause; // Bounded wait, retry this authored segment.
                else if(n.behavior == NPC_TARGET) {
                    n.behavior = NPC_STAND;
                    n.state |= NPC_TARGET_FAILED;
                    npc_on_event(n, NPC_EVENT_TARGET_FAILED);
                }
            }
        }
    }
}

void npc_player_begin(DIRECTION dir) {
    if(dir != game.npcs.push_direction || dir == STILL) {
        for(auto& n : game.npcs.actors) {
            if(!n.id)
                break;
            n.push = 0;
            n.state &= ~NPC_CAN_DETOUR;
        }
    }
    game.npcs.push_direction = dir;
}

static bool player_position_clear(uint16_t x, uint16_t y) {
    if(!terrain_clear(x, y))
        return false;
    for(auto const& n : game.npcs.actors) {
        if(!n.id)
            break;
        if(!(n.state & NPC_PASS_PLAYER) && npc_overlap(x, y, n.actor.x, n.actor.y))
            return false;
    }
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

__attribute__((noinline)) static bool player_can_detour(Npc const& n, DIRECTION dir) {
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

__attribute__((noinline)) static bool plan_yield(Npc& n, uint16_t px, uint16_t py, DIRECTION dir) {
    // Offset contacts can need almost two foot widths to separate. Keep the
    // complete 1..15 range available, including off-center 8x8 encounters.
    uint8_t max_distance = 2 * (dir == EAST || dir == WEST ? FOOT_HEIGHT : FOOT_WIDTH) - 1;
    static_assert(2 * FOOT_WIDTH - 1 <= 15 && 2 * FOOT_HEIGHT - 1 <= 15,
                  "yield distance fits the low nibble of stuck");
    // Validate a short side step without moving the actor in the collision call.
    for(uint8_t side = 1; side <= 3; side += 2) {
        DIRECTION escape = (DIRECTION)((dir + side) & 3);
        int16_t x = (int16_t)n.actor.x, y = (int16_t)n.actor.y;
        for(uint8_t distance = 1; distance <= max_distance; ++distance) {
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
                n.detour = 0;
                n.state |= NPC_YIELDING;
                return true;
            }
        }
    }
    return false;
}

bool npc_player_clear(uint16_t x, uint16_t y, DIRECTION dir, bool actual_step) {
    bool clear = true;
    for(auto& n : game.npcs.actors) {
        if(!n.id)
            break;
        if(!npc_overlap(x, y, n.actor.x, n.actor.y))
            continue;
        if(npc_is_enemy(n.id)) {
            if(actual_step)
                encounter_contact(n.id);
            clear = false;
            continue;
        }
        if(n.state & NPC_PASS_PLAYER)
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
    DIRECTION facing = game.player.facing;
    if(facing > WEST)
        return;
    bool horizontal = facing == EAST || facing == WEST;
    bool reverse = facing == NORTH || facing == WEST;
    Npc* best = nullptr;
    uint16_t nearest = 65535;
    for(auto& n : game.npcs.actors) {
        if(!n.id)
            break;
        int16_t dx = (int16_t)n.actor.x - (int16_t)game.player.x;
        int16_t dy = (int16_t)n.actor.y - (int16_t)game.player.y;
        int16_t forward = horizontal ? dx : dy;
        int16_t side = horizontal ? dy : dx;
        if(reverse)
            forward = (int16_t)-forward;
        if(forward < 1 || forward > 24 || side < -10 || side > 10)
            continue;
        if(!npc_definition(n.id)->talkable)
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
