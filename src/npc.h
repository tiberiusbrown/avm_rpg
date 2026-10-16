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

enum NPC_RETIRE_REASON : uint8_t { NPC_RETIRED_OFFSCREEN, NPC_RETIRED_TERRAIN, NPC_RETIRED_RESET };

struct NpcDefinition {
    NPC_BEHAVIOR behavior;
    DIRECTION facing;
    uint8_t period, pause, walk, leash_x, leash_y, priority, route;
};

static_assert(sizeof(NpcDefinition) == 9, "compact program-memory NPC definition");

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

NpcDefinition const PROGMEM* npc_definition(NPC_ID id);
NpcRoute const PROGMEM* npc_route(uint8_t id);
TilePos npc_home(NPC_ID id, uint8_t placement);
uint8_t npc_placement(NPC_ID id);
Npc* npc_find(NPC_ID id);
bool npc_visible(uint16_t x, uint16_t y);
bool npc_overlap(uint16_t x, uint16_t y, uint16_t other_x, uint16_t other_y);
void npc_reset();
void npc_begin_frame();
void npc_request(NPC_ID id, TilePos anchor);

template <NPC_ID ID, TilePos POS> void request_essential_npc() {
    npc_request(ID, POS);
}

void npc_reconcile();
void npc_update();
void npc_repair();
void npc_interact();
void npc_player_begin(DIRECTION dir);
bool npc_player_clear(uint16_t x, uint16_t y, DIRECTION dir);
bool npc_move_to(NPC_ID id, uint16_t x, uint16_t y);

template <TilePos POS> bool npc_move_to(NPC_ID id) {
    return POS.area == game.area && npc_move_to(id, (uint16_t)POS.x * 8, (uint16_t)POS.y * 8);
}

bool npc_follow_route(NPC_ID id, uint8_t route);

#endif
