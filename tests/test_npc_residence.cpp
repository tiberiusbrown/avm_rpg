#include "rpg.h"
#include "npc.h"
#include "logic/npc_logic.h"
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

// The NPC logic library needs the dialogue overlay; these tests never open it.
bool dialogue_show(char const PROGMEM*) {
    return true;
}

static void place_player(uint8_t tx, uint8_t ty) {
    game.player.x = (uint16_t)tx * TILE;
    game.player.y = (uint16_t)ty * TILE;
    game.player.walking = 0;
}

static void test_defaults_and_view_admission() {
    game_new();
    CHECK(game.npcs.residence[NPC_VILLAGER].x == 5 && game.npcs.residence[NPC_VILLAGER].y == 5);
    CHECK(game.npcs.residence[NPC_WANDERER].x == 10 && game.npcs.residence[NPC_WANDERER].y == 6);
    CHECK(npc_find(NPC_VILLAGER) && npc_find(NPC_WANDERER));
    CHECK(game.npcs.dormant_count == 0);

    // The far east end leaves the villager out of view; it teleports to its residence.
    place_player(positions::VillagerEast.x - 1, positions::VillagerEast.y);
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_VILLAGER));
    CHECK(game.npcs.dormant_count == 1 && game.npcs.dormant[0] == NPC_VILLAGER);
}

static void test_static_npc_stays_in_its_chunk() {
    CHECK(npc_is_dynamic(NPC_VILLAGER));
    CHECK(!npc_is_dynamic(NPC_WANDERER));
    TilePos before = game.npcs.residence[NPC_WANDERER];
    CHECK(!npc_set_destination(NPC_WANDERER, positions::VillagerWest));
    CHECK(game.npcs.residence[NPC_WANDERER] == before);
}

static void test_destination_admission_and_travel() {
    // The player stands at (15,7), which sees the destination (16,7).
    CHECK(npc_set_destination(NPC_VILLAGER, positions::VillagerEast));
    update_visible_world(0, false);
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(villager);
    CHECK(villager->actor.x == 16 * TILE && villager->actor.y == 7 * TILE);

    // An active dynamic NPC walks toward a new destination; it does not teleport.
    CHECK(npc_set_destination(NPC_VILLAGER, positions::VillagerWest));
    CHECK(villager->behavior == NPC_TRAVEL);
}

static void test_save_round_trip_and_rejection() {
    uint8_t record[SAVE_BYTES];
    CHECK(npc_set_destination(NPC_VILLAGER, positions::VillagerEast));
    TilePos before = game.npcs.residence[NPC_VILLAGER];
    save_encode(record);

    game_new();
    CHECK(save_decode(record));
    CHECK(game.npcs.residence[NPC_VILLAGER] == before);

    // A residence outside the authored map is rejected, even with a valid checksum.
    size_t residence_at = SAVE_HEADER_BYTES + WorldFlags::BYTE_COUNT;
    record[residence_at] = 9; // No such area.
    uint16_t crc = save_checksum(record);
    record[SAVE_CRC_OFFSET] = (uint8_t)crc;
    record[SAVE_CRC_OFFSET + 1] = (uint8_t)(crc >> 8);
    CHECK(!save_decode(record));
}

int main() {
    game_new();
    test_defaults_and_view_admission();
    test_static_npc_stays_in_its_chunk();
    test_destination_admission_and_travel();
    test_save_round_trip_and_rejection();
    printf("npc residence checks passed: %u\n", checks);
    return 0;
}
