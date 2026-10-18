#include "rpg.h"
#include "chunk.h"
#include "world_assets.h"
#include "map_positions.h"
#include "npc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

#include "logic/npc_logic.h"
static unsigned activated, retired, restored, events[5];
static NPC_RETIRE_REASON last_retirement;
static bool replace_route;

NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    static constexpr NpcDefinition DEFINITION{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 3, 0};
    return id == NPC_VILLAGER ? &DEFINITION : nullptr;
}

uint8_t npc_placement(NPC_ID) {
    return 0;
}

TilePos npc_home(NPC_ID, uint8_t) {
    return positions::VillagerHome;
}

NpcRoute const PROGMEM* npc_route(uint8_t id) {
    static constexpr RoutePoint POINTS[] = {{5, 5}, {5, 6}};
    static constexpr NpcRoute ONCE{POINTS, 2, ROUTE_ONCE, 2, AREA_OUTDOOR};
    static constexpr NpcRoute REVERSE{POINTS, 2, ROUTE_REVERSE, 2, AREA_OUTDOOR};
    return id == 1 ? &ONCE : id == 2 ? &REVERSE : nullptr;
}

void npc_on_activate(Npc& n) {
    ++activated;
    CHECK(npc_find(n.id) == &n);
}

void npc_on_event(Npc& n, NPC_EVENT event) {
    ++events[event];
    if(event == NPC_EVENT_ROUTE_STARTED && replace_route) {
        replace_route = false;
        CHECK(npc_follow_route(n.id, 2));
    }
}

void npc_on_retire(Npc const&, NPC_RETIRE_REASON reason) {
    ++retired;
    last_retirement = reason;
}

void npc_logic_restore() {
    ++restored;
}

void npc_talk(NPC_ID) {}

bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

int main() {
    game_new();
    CHECK(activated == 1 && restored == 1);
    Npc& n = *npc_find(NPC_VILLAGER);
    CHECK(npc_follow_route(n.id, 1));
    for(unsigned i = 0; i < 24; ++i)
        npc_update();
    CHECK(events[NPC_EVENT_ROUTE_STARTED] == 1 && events[NPC_EVENT_ROUTE_COMPLETED] == 1);
    CHECK(n.actor.y == 96 && n.behavior == NPC_STAND);
    replace_route = true;
    CHECK(npc_follow_route(n.id, 1));
    npc_update();
    CHECK(n.route == 2 && (n.state & NPC_ROUTE_PENDING));
    npc_update();
    CHECK(events[NPC_EVENT_ROUTE_STARTED] == 3);
    for(unsigned i = 0; i < 100; ++i)
        npc_update();
    CHECK(n.behavior == NPC_ROUTE && events[NPC_EVENT_ROUTE_COMPLETED] == 1);
    n.waypoint = 255;
    npc_update();
    CHECK(events[NPC_EVENT_ROUTE_INVALID] == 1 && n.behavior == NPC_STAND);
    CHECK(npc_move_to(n.id, n.actor.x + 1, n.actor.y));
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    for(unsigned i = 0; i < NPC_STUCK_FRAMES; ++i)
        npc_update();
    CHECK(events[NPC_EVENT_TARGET_FAILED] == 1);
    npc_repair();
    CHECK(!npc_find(NPC_VILLAGER) && last_retirement == NPC_RETIRED_TERRAIN);
    world_enter(AREA_OUTDOOR, 112, 80, NORTH);
    CHECK(npc_find(NPC_VILLAGER));
    CHECK(npc_move_to(NPC_VILLAGER, 80, 81));
    npc_update();
    npc_update();
    CHECK(events[NPC_EVENT_TARGET_REACHED] == 1);
    world_enter(AREA_OUTDOOR, 15 * TILE, 9 * TILE, SOUTH);
    CHECK(!npc_find(NPC_VILLAGER) && last_retirement == NPC_RETIRED_RESET);
    unsigned count = retired;
    npc_reset();
    CHECK(retired == count);
    printf("PASS independent NPC lifecycle policy: %u checks\n", checks);
}
