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

struct NpcDefinition {
    NPC_BEHAVIOR behavior;
    DIRECTION facing;
    // Frames per movement step; pause and walk are frame counts.
    uint8_t period, pause, walk, leash_x, leash_y, priority, route;
    uint8_t sprite_row = 0;
    bool talkable = true;
};

static_assert(sizeof(NpcDefinition) == 11, "compact program-memory NPC definition");

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
    if(site.area == game.area && !party_contains(id) && !npc_find(id))
        npc_detail::offer_candidate(id, site);
}

// Called after adding a party member. Retires its map actor, preserving the occupied prefix.
void npc_recruit(NPC_ID id);
void npc_retire_enemy(NPC_ID id);
bool npc_visible(uint16_t x, uint16_t y);
bool npc_overlap(uint16_t x, uint16_t y, uint16_t other_x, uint16_t other_y);
void npc_reset();
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

#endif
