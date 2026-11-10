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
    CHECK(game.npcs.residence[NPC_VILLAGER] == positions::VillagerHome);
    CHECK(game.npcs.residence[NPC_WANDERER] == positions::WandererHome);
    CHECK(npc_find(NPC_VILLAGER) && npc_find(NPC_WANDERER));

    // Leaving view does not teleport an active NPC. It keeps its slot and position.
    Npc* villager = npc_find(NPC_VILLAGER);
    uint16_t x = villager->actor.x, y = villager->actor.y;
    place_player(positions::VillagerEast.x - 1, positions::VillagerEast.y);
    update_visible_world(0, false);
    CHECK(npc_find(NPC_VILLAGER) == villager);
    CHECK(villager->actor.x == x && villager->actor.y == y);
}

static void test_static_npc_stays_in_its_chunk() {
    CHECK(npc_is_dynamic(NPC_VILLAGER));
    CHECK(!npc_is_dynamic(NPC_WANDERER));
    TilePos before = game.npcs.residence[NPC_WANDERER];
    CHECK(!npc_set_destination(NPC_WANDERER, positions::VillagerWest));
    CHECK(game.npcs.residence[NPC_WANDERER] == before);
}

static void test_destination_admission_and_travel() {
    // Entering the area casts the villager at its new residence, which the player can see.
    npc_reset();
    // The player stands beside the destination, which is then in view.
    place_player(positions::VillagerEast.x - 1, positions::VillagerEast.y);
    CHECK(npc_set_destination(NPC_VILLAGER, positions::VillagerEast));
    update_visible_world(0, false);
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(villager);
    CHECK(villager->actor.x == positions::VillagerEast.x * TILE &&
          villager->actor.y == positions::VillagerEast.y * TILE);

    // An active dynamic NPC walks toward a new destination; it does not teleport.
    uint16_t x = villager->actor.x, y = villager->actor.y;
    CHECK(npc_set_destination(NPC_VILLAGER, positions::VillagerWest));
    CHECK(villager->behavior == NPC_TRAVEL);
    CHECK(villager->actor.x == x && villager->actor.y == y);
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

static unsigned moved_pixels(uint16_t x, uint16_t y, Npc const& n) {
    int dx = (int)n.actor.x - (int)x, dy = (int)n.actor.y - (int)y;
    return (unsigned)(abs(dx) + abs(dy));
}

// A walking NPC that leaves view keeps its slot and walks on. It never reappears at home.
static void test_leaving_view_is_continuous() {
    game_new();
    // Five tiles east of the wanderer's home. Its leash reaches one tile west of home.
    place_player(positions::WandererHome.x + 5, positions::WandererHome.y + 1);
    update_visible_world(0, false);
    CHECK(npc_move_to(NPC_WANDERER, positions::WandererHome.x * TILE - TILE,
                      positions::WandererHome.y * TILE));
    Npc* wanderer = npc_find(NPC_WANDERER);
    CHECK(wanderer);
    bool left_view = false;
    for(unsigned frame = 0; frame < 120; ++frame) {
        uint16_t x = wanderer->actor.x, y = wanderer->actor.y;
        game_update(0);
        CHECK(npc_find(NPC_WANDERER) == wanderer);
        CHECK(moved_pixels(x, y, *wanderer) <= 1);
        left_view |= !npc_visible(wanderer->actor.x, wanderer->actor.y);
    }
    CHECK(left_view);
}

static uint16_t home_x() {
    return (uint16_t)(positions::VillagerHome.x * TILE);
}

static uint16_t home_y() {
    return (uint16_t)(positions::VillagerHome.y * TILE);
}

// An NPC outside the terrain cache is not simulated, and it resumes from the same place.
static void test_npc_outside_cache_is_frozen() {
    game_new();
    Npc* villager = npc_find(NPC_VILLAGER);
    place_player(positions::VillagerEast.x, positions::VillagerEast.y); // Far east: the home is outside the cache.
    update_visible_world(0, false);
    CHECK(npc_move_to(NPC_VILLAGER, home_x(), home_y() + TILE));
    for(unsigned frame = 0; frame < 20; ++frame)
        game_update(0);
    CHECK(villager->actor.x == home_x() && villager->actor.y == home_y());
    place_player(positions::PlayerStart.x, positions::PlayerStart.y); // Back at the start, where the home is cached again.
    for(unsigned frame = 0; frame < 200 && villager->behavior != NPC_STAND; ++frame) {
        uint16_t x = villager->actor.x, y = villager->actor.y;
        game_update(0);
        CHECK(npc_find(NPC_VILLAGER) == villager);
        CHECK(moved_pixels(x, y, *villager) <= 1);
    }
    CHECK(villager->actor.x == home_x() && villager->actor.y == home_y() + TILE);
}

// NPCs step on every other frame, and their walk pose holds between steps.
static void test_npc_steps_every_other_frame() {
    game_new();
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(npc_move_to(NPC_VILLAGER, home_x(), home_y() + TILE)); // Sixteen pixels south of home.
    unsigned frames = 0;
    bool moved_last = false;
    for(; frames < 200 && villager->actor.y != home_y() + TILE; ++frames) {
        uint16_t x = villager->actor.x, y = villager->actor.y;
        game_update(0);
        bool moved = villager->actor.x != x || villager->actor.y != y;
        CHECK(moved_pixels(x, y, *villager) <= 1 && !(moved && moved_last));
        CHECK(villager->actor.walking); // The pose holds between steps.
        moved_last = moved;
    }
    CHECK(villager->actor.y == home_y() + TILE);
    CHECK(frames >= 30 && frames <= 34); // About two frames per pixel.
}

int main() {
    game_new();
    test_defaults_and_view_admission();
    test_static_npc_stays_in_its_chunk();
    test_destination_admission_and_travel();
    test_save_round_trip_and_rejection();
    test_leaving_view_is_continuous();
    test_npc_outside_cache_is_frozen();
    test_npc_steps_every_other_frame();
    printf("npc residence checks passed: %u\n", checks);
    return 0;
}
