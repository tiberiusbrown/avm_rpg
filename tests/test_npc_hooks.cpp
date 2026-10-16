// Link the production mechanics against an independent character policy.
// The static rules library does not need to pull in the game's policy object.
#include "logic/npc_logic.h"
#include "chunk.h"
#include "map_positions.h"
#include "npc_routes.h"
#include "tile_ids.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, activated, retired, restored;
static unsigned events[5];
static NPC_RETIRE_REASON last_retirement;
static bool replace_start, chain_completion, activate_target;

#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    static constexpr NpcDefinition PROGMEM DEFINITION{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 3, 0};
    return id == NPC_GUIDE ? &DEFINITION : nullptr;
}

uint8_t npc_placement(NPC_ID) {
    return 0;
}

TilePos npc_home(NPC_ID, uint8_t) {
    return positions::GuideHome;
}

void npc_on_activate(Npc& npc) {
    ++activated;
    CHECK(npc.id == NPC_GUIDE && npc_find(npc.id) == &npc);
    if(activate_target)
        CHECK(npc_move_to(npc.id, 48, 24));
}

void npc_on_event(Npc& npc, NPC_EVENT event) {
    ++events[event];
    CHECK(npc_find(npc.id) == &npc);
    if(event == NPC_EVENT_ROUTE_STARTED) {
        CHECK(npc.behavior == NPC_ROUTE && !(npc.state & NPC_ROUTE_PENDING));
        if(replace_start) {
            replace_start = false;
            CHECK(npc_follow_route(npc.id, routes::CourierReverse));
        }
    } else {
        CHECK(npc.behavior == NPC_STAND);
        if(event == NPC_EVENT_ROUTE_COMPLETED && chain_completion) {
            CHECK(npc.route == routes::CourierWalk);
            CHECK(npc_move_to(npc.id, 184, 49));
        }
        if(event == NPC_EVENT_TARGET_FAILED)
            CHECK(npc.state & NPC_TARGET_FAILED);
    }
}

void npc_on_retire(Npc const& npc, NPC_RETIRE_REASON reason) {
    ++retired;
    last_retirement = reason;
    CHECK(npc.id == NPC_GUIDE && npc_find(npc.id) == &npc);
}

void npc_logic_restore() {
    ++restored;
    CHECK(!npc_find(NPC_GUIDE) && !game.npcs.request_count);
}

void npc_talk(NPC_ID) {}

bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

static Npc& route_fixture() {
    game_new();
    CHECK(npc_find(NPC_GUIDE));
    Npc& npc = *npc_find(NPC_GUIDE);
    npc.actor = {120, 32, EAST, 0};
    game.player = {136, 16, EAST, 0};
    refresh_tile_cache();
    CHECK(npc_follow_route(npc.id, routes::CourierWalk));
    return npc;
}

int main() {
    activate_target = true;
    game_new();
    CHECK(activated == 1 && restored == 1);
    CHECK(npc_find(NPC_GUIDE)->behavior == NPC_TARGET);
    for(uint8_t i = 0; i < 10; ++i)
        npc_update();
    CHECK(events[NPC_EVENT_TARGET_REACHED] == 1);
    activate_target = false;

    Npc& npc = route_fixture();
    chain_completion = true;
    unsigned starts = events[NPC_EVENT_ROUTE_STARTED];
    for(uint8_t i = 0; i < 140; ++i) {
        npc_update();
        if(events[NPC_EVENT_ROUTE_COMPLETED])
            break;
    }
    CHECK(events[NPC_EVENT_ROUTE_STARTED] == starts + 1);
    CHECK(events[NPC_EVENT_ROUTE_COMPLETED] == 1);
    CHECK(npc.behavior == NPC_TARGET && npc.actor.y == 48);
    npc_update();
    CHECK(npc.actor.y == 49 && npc.behavior == NPC_TARGET);
    npc_update();
    CHECK(npc.behavior == NPC_STAND && events[NPC_EVENT_TARGET_REACHED] == 2);
    chain_completion = false;

    route_fixture();
    starts = events[NPC_EVENT_ROUTE_STARTED];
    replace_start = true;
    npc_update();
    CHECK(npc.route == routes::CourierReverse && (npc.state & NPC_ROUTE_PENDING));
    CHECK(npc.waypoint == 0 && npc.timer == 0); // Replacement waits until next update.
    npc_update();
    CHECK(events[NPC_EVENT_ROUTE_STARTED] == starts + 2);
    for(uint8_t i = 0; i < 90; ++i)
        npc_update();
    CHECK(events[NPC_EVENT_ROUTE_COMPLETED] == 1); // Reverse endpoints are not completion.

    CHECK(npc_follow_route(npc.id, routes::CourierWalk));
    npc.waypoint = 255;
    starts = events[NPC_EVENT_ROUTE_STARTED];
    npc_update();
    npc_update();
    CHECK(events[NPC_EVENT_ROUTE_INVALID] == 1 && events[NPC_EVENT_ROUTE_STARTED] == starts);
    CHECK(npc.behavior == NPC_STAND && !(npc.state & NPC_ROUTE_PENDING));

    // Changing to a target cancels a route that has not started.
    CHECK(npc_follow_route(npc.id, routes::CourierWalk));
    CHECK(npc_move_to(npc.id, npc.actor.x + 1, npc.actor.y));
    memset(game.cache.tiles, TILE_WALL, sizeof(game.cache.tiles));
    for(uint8_t i = 0; i < NPC_STUCK_FRAMES + 2; ++i)
        npc_update();
    CHECK(events[NPC_EVENT_TARGET_FAILED] == 1 && events[NPC_EVENT_ROUTE_STARTED] == starts);
    npc_repair();
    CHECK(!npc_find(NPC_GUIDE) && last_retirement == NPC_RETIRED_TERRAIN);

    route_fixture();
    game.player = {224, 136, EAST, 0};
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_GUIDE) && last_retirement == NPC_RETIRED_OFFSCREEN);
    route_fixture();
    unsigned retirements = retired, restores = restored;
    npc_reset();
    CHECK(retired == retirements + 1 && last_retirement == NPC_RETIRED_RESET);
    CHECK(restored == restores + 1);
    npc_reset();
    CHECK(retired == retirements + 1 && restored == restores + 2);
    printf("PASS independent NPC lifecycle policy: %u checks\n", checks);
}
