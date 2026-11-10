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

// Scene exit is the only retirement: NPCs are never removed while their scene lasts.
enum NPC_RETIRE_REASON : uint8_t { NPC_RETIRED_RESET };

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

NpcDefinition const PROGMEM* npc_definition(NPC_ID id);
NpcRoute const PROGMEM* npc_route(uint8_t id);
// Default residence of an NPC, supplied by the logic layer.
TilePos npc_home(NPC_ID id);
// Dynamic NPCs follow their residence; static NPCs stay at their home.
bool npc_is_dynamic(NPC_ID id);
bool npc_destination_valid(TilePos destination);
void npc_residence_defaults();
// Sets where a dynamic NPC lives. An active NPC walks there; the change never moves it at once.
bool npc_set_destination(NPC_ID id, TilePos destination);
Npc* npc_find(NPC_ID id);
bool npc_visible(uint16_t x, uint16_t y);
bool npc_overlap(uint16_t x, uint16_t y, uint16_t other_x, uint16_t other_y);
void npc_reset();
void npc_begin_frame();
void npc_reconcile();
void npc_update();
void npc_interact();
void npc_player_begin(DIRECTION dir);
bool npc_player_clear(uint16_t x, uint16_t y, DIRECTION dir);
bool npc_move_to(NPC_ID id, uint16_t x, uint16_t y);

template <TilePos POS> bool npc_move_to(NPC_ID id) {
    return POS.area == game.area && npc_move_to(id, (uint16_t)POS.x * TILE, (uint16_t)POS.y * TILE);
}

bool npc_follow_route(NPC_ID id, uint8_t route);

#endif
