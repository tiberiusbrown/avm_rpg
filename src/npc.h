#ifndef NPC_H
#define NPC_H

#include "rpg.h"

enum NPC_EVENT : uint8_t {
    NPC_EVENT_ROUTE_STARTED,
    NPC_EVENT_ROUTE_COMPLETED,
    NPC_EVENT_ROUTE_INVALID,
    NPC_EVENT_TARGET_REACHED,
    NPC_EVENT_TARGET_FAILED
};

// Recruitment removes a map actor immediately; scene exit retires the remaining actors.
enum NPC_RETIRE_REASON : uint8_t { NPC_RETIRED_RESET, NPC_RETIRED_PARTY, NPC_RETIRED_UNLOADED, NPC_RETIRED_ENCOUNTER };

// How an NPC stands in the world.
enum NPC_BODY : uint8_t {
    NPC_BODY_CHARACTER, // An actors16 sprite with an 8x8 foot box. It steps aside for the player.
    NPC_BODY_FIXED,     // A character that holds its ground: it never yields or lets anyone pass.
    NPC_BODY_STATIC     // An object from npc_static.png that blocks with its collision mask.
};

struct NpcDefinition {
    NPC_BEHAVIOR behavior;
    DIRECTION facing;
    // Frames per movement step; pause and walk are frame counts.
    uint8_t period, pause, walk, leash_x, leash_y, priority, route;
    // The actors16 row of a character, or the npc_static frame of a static object.
    uint8_t sprite_row = 0;
    // Whether A while facing the NPC reaches its content: a conversation, or what an object does.
    bool talkable = true;
    NPC_BODY body = NPC_BODY_CHARACTER;
};

static_assert(sizeof(NpcDefinition) == 12, "compact program-memory NPC definition");

// Bits of Npc::policy_pending.
inline constexpr uint8_t NPC_FLAG_PENDING = 1;    // The policy assigned a new site.
inline constexpr uint8_t NPC_FLAG_FIXED_SITE = 2; // The policy has one site, so it never moves.
inline constexpr uint8_t NPC_FLAG_CLEAR = 4;      // The position is known to be clear of terrain.
inline constexpr uint8_t NPC_FLAG_PURSUIT = 8;    // An enemy that chases the player.
inline constexpr uint8_t NPC_FLAG_STATIC = 16;    // NPC_BODY_STATIC, cached from the definition.
inline constexpr uint8_t NPC_FLAG_IMMOVABLE = 32; // NPC_BODY_FIXED or NPC_BODY_STATIC.

struct RoutePoint {
    uint8_t x, y;
};

static_assert(sizeof(RoutePoint) == 2, "byte tile-coordinate route points");

struct NpcRoute {
    RoutePoint const PROGMEM* points;
    uint8_t count;
    NPC_ROUTE_MODE mode;
    uint8_t pause, area;
};

// Display name in program memory; unknown IDs have no name.
char const PROGMEM* npc_name(NPC_ID id);
// The collision mask of one npc_static frame, in the layout of TILE_COLLISION.
uint16_t npc_static_collision(uint8_t frame);
NpcDefinition const PROGMEM* npc_definition(NPC_ID id);
NpcRoute const PROGMEM* npc_route(uint8_t id);
#ifndef RPG_AVM
extern unsigned npc_site_checks;
#endif
TilePos npc_location(NPC_ID id);
// Derived policy metadata, queried once when activating a slot.
bool npc_location_is_fixed(NPC_ID id);
void npc_stage_begin();
void npc_stage_end();
Npc* npc_find(NPC_ID id);

namespace npc_detail {
// Admission's slow path; the public front door has already checked these identities.
void offer_candidate(NPC_ID id, TilePos site);
} // namespace npc_detail

// Keep cheap rejection at the admission boundary, where site IDs are constants.
// Active residents need no slow-path call, definition lookup or collision work.
inline void npc_offer_candidate(NPC_ID id, TilePos site) {
    if(site.area == game.area && !npc_find(id) && !party_contains(id))
        npc_detail::offer_candidate(id, site);
}

// Called after adding a party member. Retires its map actor, preserving the occupied prefix.
void npc_recruit(NPC_ID id);
void npc_retire_enemy(NPC_ID id);
// Finish a scripted departure once the NPC policy assigns it outside this area.
void npc_leave_scene(NPC_ID id);
bool npc_visible(uint16_t x, uint16_t y);
bool npc_overlap(uint16_t x, uint16_t y, uint16_t other_x, uint16_t other_y);
void npc_reset();
// A save holds the pool as it stands. Validation accepts one that the update code can run, and
// resuming takes up its actors where they were, on a terrain window that is about to be rebuilt.
bool npc_saved_state_valid(MapDimensions dimensions);
void npc_resume();
void npc_begin_frame();
void npc_update();
void npc_interact();
void npc_player_begin(DIRECTION dir);
bool npc_player_clear(uint16_t x, uint16_t y, DIRECTION dir, bool actual_step = true);
bool npc_move_to(NPC_ID id, uint16_t x, uint16_t y);

template <TilePos POS> bool npc_move_to(NPC_ID id) {
    return POS.area == game.area && npc_move_to(id, (uint16_t)POS.x * TILE, (uint16_t)POS.y * TILE);
}

bool npc_follow_route(NPC_ID id, uint8_t route);
// Makes an active enemy chase the player, with the pursuit rules of its spawn.
bool npc_start_pursuit(NPC_ID id);
// Slides a resting static NPC one tile in dir, if the destination lies in bounds and in the
// chunk of the object's site, and is free of terrain, the player and other NPCs. The object
// reserves the destination while it slides. It keeps its place while the terrain cache reaches
// that chunk; once cast again, it is back at its policy site.
bool npc_push(NPC_ID id, DIRECTION dir, TileRegion bounds);
// Whether the player's foot box at (x, y) is clear of every static NPC. An object that the player
// already overlaps does not block, so the player can always walk out of it.
bool npc_object_clear(uint16_t x, uint16_t y);
// Bit i is set when pixel i of the foot box's leading edge lies in a static NPC, as
// player_leading_edge reports for terrain.
uint8_t npc_object_edge(uint16_t x, uint16_t y, DIRECTION dir);

#endif
