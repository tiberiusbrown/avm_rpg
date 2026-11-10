#include "rpg.h"
#include "npc.h"
#include "tile_ids.h"
#include "map_positions.h"
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

// Two dynamic NPCs with their own logic, so casting is exercised without the content NPCs.
// This file replaces the logic library entirely.
NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    static constexpr NpcDefinition PROGMEM ONE{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 3, 0};
    static constexpr NpcDefinition PROGMEM TWO{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 2, 0};
    return id == NPC_VILLAGER ? &ONE : id == NPC_WANDERER ? &TWO : nullptr;
}

TilePos npc_home(NPC_ID id) {
    if(id == NPC_VILLAGER)
        return positions::VillagerEast; // Outside the start cache.
    if(id == NPC_WANDERER)
        return positions::WandererHome;
    return {};
}

bool npc_is_dynamic(NPC_ID) {
    return true;
}

void npc_on_activate(Npc&) {}
void npc_on_event(Npc&, NPC_EVENT) {}
void npc_on_retire(Npc const&, NPC_RETIRE_REASON) {}
void npc_logic_restore() {}
void npc_talk(NPC_ID) {}
void npc_dialogue() {}

// The sign handler in the chunk dispatch references the overlay; these tests never open it.
bool dialogue_show(char const PROGMEM*) {
    return true;
}

static void place_player(uint8_t tx, uint8_t ty) {
    game.player.x = (uint16_t)tx * TILE;
    game.player.y = (uint16_t)ty * TILE;
    game.player.walking = 0;
}

// Entry casts a resident at its home when the home is cached. The villager's home lies outside
// the start cache, so it is not cast until the cache covers it.
static void test_entry_casts_residents() {
    game_new();
    Npc* wanderer = npc_find(NPC_WANDERER);
    CHECK(wanderer);
    CHECK(wanderer->actor.x == positions::WandererHome.x * TILE &&
          wanderer->actor.y == positions::WandererHome.y * TILE);
    CHECK(!npc_find(NPC_VILLAGER));
}

// After entry, a spot in view is never cast into, and a home beyond the cache stays empty. A cached
// spot out of view also casts once it is clear. The camera follows the player, and the map's border
// keeps it from clamping at a corner, so no interior spot is cached yet off screen. That case is
// left to the corner-free scenarios.
static void test_casts_wait_for_spots_out_of_view() {
    game_new();
    game.npcs.residence[NPC_VILLAGER] = positions::VillagerHome; // Beside the player, in view.
    game.npcs.residence[NPC_WANDERER] = positions::VillagerEast; // Beyond the cache.
    place_player(positions::VillagerHome.x - 1, positions::VillagerHome.y);
    update_visible_world(0, false); // Refreshes the cache for this view before the edits below.
    npc_reset();
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    npc_reconcile(); // Entry finds no clear spot.
    CHECK(!npc_find(NPC_VILLAGER) && !npc_find(NPC_WANDERER));
    memset(game.cache.tiles, TILE_GRASS_06, sizeof(game.cache.tiles));
    npc_reconcile(); // The villager's home is in view, so it waits. The wanderer's is not cached.
    CHECK(!npc_find(NPC_VILLAGER) && !npc_find(NPC_WANDERER));
}

int main() {
    test_entry_casts_residents();
    test_casts_wait_for_spots_out_of_view();
    printf("npc index checks passed: %u\n", checks);
    return 0;
}
