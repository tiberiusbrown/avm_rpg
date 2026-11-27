#include "rpg.h"
#include "chunk.h"
#include "cast.h"
#include "chunk.h"
#include "world_assets.h"
#include "cast.h"
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

NpcRoute const PROGMEM* npc_route(uint8_t id) {
    static constexpr RoutePoint POINTS[] = {
        {sites::WELL.x, sites::WELL.y}, {sites::WELL.x, static_cast<uint8_t>(sites::WELL.y + 1)}};
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

void npc_dialogue() {}

bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

int main() {
    game_new();
    CHECK(activated == 1 && restored == 1);
    Npc& n = *npc_find(NPC_VILLAGER);
    CHECK(npc_follow_route(n.id, 1));
    for(unsigned i = 0; i < 24; ++i)
        npc_update();
    CHECK(events[NPC_EVENT_ROUTE_STARTED] == 1 && events[NPC_EVENT_ROUTE_COMPLETED] == 1);
    CHECK(n.actor.y == (sites::WELL.y + 1) * TILE && n.behavior == NPC_STAND);
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
    memset(cache.tiles, TILE_TREE, sizeof(cache.tiles));
    note_tile_edit();
    for(unsigned i = 0; i < NPC_STUCK_FRAMES; ++i)
        npc_update();
    CHECK(events[NPC_EVENT_TARGET_FAILED] == 1);
    // Every tile is solid now, so the villager has no clear spot to walk out to. It keeps its
    // slot and holds still.
    Npc* stuck = npc_find(NPC_VILLAGER);
    uint16_t sx = stuck->actor.x, sy = stuck->actor.y;
    npc_update();
    CHECK(npc_find(NPC_VILLAGER) == stuck && stuck->actor.x == sx && stuck->actor.y == sy);
    world_enter(AREA_OUTDOOR, sites::PLAYER_START.x * TILE, sites::PLAYER_START.y * TILE, NORTH);
    CHECK(npc_find(NPC_VILLAGER));
    CHECK(npc_move_to(NPC_VILLAGER, sites::WELL.x * TILE, sites::WELL.y * TILE + 1));
    npc_update();
    npc_update();
    CHECK(events[NPC_EVENT_TARGET_REACHED] == 1);
    world_enter(AREA_OUTDOOR, sites::ORCHARD.x * TILE, sites::ORCHARD.y * TILE, SOUTH);
    // Scene exit retires the NPCs that were present, and only them. The villager's home is
    // outside the cache from here, so it is not cast until the cache covers it.
    CHECK(!npc_find(NPC_VILLAGER) && last_retirement == NPC_RETIRED_RESET);
    unsigned present = (npc_find(NPC_VILLAGER) ? 1u : 0u) + (npc_find(NPC_WANDERER) ? 1u : 0u);
    unsigned count = retired;
    npc_reset();
    CHECK(retired == count + present);
    // Absence and other-area assignments retain visible actors until safe unloading.
    game_new();
    CHECK(npc_find(NPC_VILLAGER));
    game.set_flag(TEST_FLAG_B);
    update_visible_world(0, false);
    CHECK(npc_find(NPC_VILLAGER));
    game.player.x = sites::TEST_FIELD.x * TILE;
    game.player.y = sites::TEST_FIELD.y * TILE;
    unsigned before_unload = retired;
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_VILLAGER) && retired == before_unload + 1);
    CHECK(last_retirement == NPC_RETIRED_UNLOADED);
    update_visible_world(0, false);
    CHECK(retired == before_unload + 1);

    game_new();
    game.set_flag(TEST_FLAG_A);
    update_visible_world(0, false);
    CHECK(npc_find(NPC_VILLAGER) && npc_location(NPC_VILLAGER).area == AREA_INTERIORS);
    game.player.x = sites::TEST_FIELD.x * TILE;
    game.player.y = sites::TEST_FIELD.y * TILE;
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_VILLAGER));
    TilePos cave_site = npc_location(NPC_VILLAGER);
    world_enter(cave_site.area, (cave_site.x - 1) * TILE, cave_site.y * TILE, EAST);
    CHECK(npc_find(NPC_VILLAGER));
    CHECK(npc_find(NPC_VILLAGER)->assigned_site == cave_site);

    game_new();
    CHECK(npc_follow_route(NPC_VILLAGER, 2));
    game.set_flag(TEST_FLAG_A);
    game.player.x = sites::TEST_FIELD.x * TILE;
    game.player.y = sites::TEST_FIELD.y * TILE;
    update_visible_world(0, false);
    auto pinned = npc_find(NPC_VILLAGER);
    CHECK(pinned && pinned->behavior == NPC_ROUTE && pinned->route == 2);
    CHECK(pinned->assigned_site.area == AREA_INTERIORS && pinned->policy_pending);
    CHECK(npc_move_to(NPC_VILLAGER, pinned->actor.x + 1, pinned->actor.y));
    update_visible_world(0, false);
    CHECK(npc_find(NPC_VILLAGER)->behavior == NPC_TARGET);

    printf("PASS independent NPC lifecycle policy: %u checks\n", checks);
}
