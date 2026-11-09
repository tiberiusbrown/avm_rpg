#include "rpg.h"
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

// Two dynamic NPCs with their own logic, so the residence index can be exercised
// without the content NPCs. This file replaces the logic library entirely.
NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    static constexpr NpcDefinition PROGMEM ONE{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 3, 0};
    static constexpr NpcDefinition PROGMEM TWO{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 2, 0};
    return id == NPC_VILLAGER ? &ONE : id == NPC_WANDERER ? &TWO : nullptr;
}

TilePos npc_home(NPC_ID id) {
    if(id == NPC_VILLAGER)
        return TilePos{AREA_OUTDOOR, 2, 1};
    if(id == NPC_WANDERER)
        return TilePos{AREA_OUTDOOR, 9, 5};
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

static uint16_t index_key(TilePos p) {
    return (uint16_t)((p.area << 11) | ((p.y / CHUNK_HEIGHT) << 5) | (p.x / CHUNK_WIDTH));
}

static void check_index_sorted() {
    for(uint8_t i = 1; i < game.npcs.dormant_count; ++i)
        CHECK(index_key(game.npcs.residence[game.npcs.dormant[i - 1]]) <=
              index_key(game.npcs.residence[game.npcs.dormant[i]]));
}

static void place_player(uint8_t tx, uint8_t ty) {
    game.player.x = (uint16_t)tx * TILE;
    game.player.y = (uint16_t)ty * TILE;
    game.player.walking = 0;
}

static void test_order_follows_chunk_position() {
    // The player far to the east leaves both residences out of view.
    game_new();
    place_player(15, 7);
    update_visible_world(0, false);
    CHECK(game.npcs.dormant_count == 2);
    check_index_sorted();
    CHECK(game.npcs.dormant[0] == NPC_VILLAGER); // Row 0, column 0 sorts first.

    npc_set_destination(NPC_VILLAGER, TilePos{AREA_OUTDOOR, 9, 9}); // Row 2: after row 1.
    check_index_sorted();
    CHECK(game.npcs.dormant[0] == NPC_WANDERER && game.npcs.dormant[1] == NPC_VILLAGER);

    npc_set_destination(NPC_WANDERER, TilePos{AREA_OUTDOOR, 3, 0}); // Row 0, column 0.
    check_index_sorted();
    CHECK(game.npcs.dormant[0] == NPC_WANDERER);
}

static void test_range_admission_filters_by_visibility() {
    // The villager's residence is in the view; the wanderer's chunk is also inside the
    // view's chunk range, but its tile is not visible, so it stays dormant.
    npc_set_destination(NPC_VILLAGER, TilePos{AREA_OUTDOOR, 3, 2});
    npc_set_destination(NPC_WANDERER, TilePos{AREA_OUTDOOR, 9, 5});
    place_player(2, 2);
    update_visible_world(0, false);
    CHECK(npc_find(NPC_VILLAGER));
    CHECK(!npc_find(NPC_WANDERER));
    CHECK(game.npcs.dormant_count == 1 && game.npcs.dormant[0] == NPC_WANDERER);

    // Moving the view over the wanderer admits it at its residence.
    place_player(10, 4);
    update_visible_world(0, false);
    Npc* wanderer = npc_find(NPC_WANDERER);
    CHECK(wanderer && wanderer->actor.x == 9 * TILE && wanderer->actor.y == 5 * TILE);
    // The villager's residence is now out of view, so it teleported into the index.
    CHECK(game.npcs.dormant_count == 1 && game.npcs.dormant[0] == NPC_VILLAGER);

    // Leaving view teleports it back to the index, at its residence.
    place_player(2, 2);
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_WANDERER));
    CHECK(game.npcs.dormant_count == 1 && game.npcs.dormant[0] == NPC_WANDERER);
    check_index_sorted();
}

int main() {
    test_order_follows_chunk_position();
    test_range_admission_filters_by_visibility();
    printf("npc index checks passed: %u\n", checks);
    return 0;
}
