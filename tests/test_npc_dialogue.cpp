#include "rpg.h"
#include "chunk.h"
#include "story_stub.h"
#include "cast.h"
#include "logic/npc_logic.h"
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

static unsigned shown;
static char const* last_text;
static NPC_ID last_speaker = NPC_NONE;

bool dialogue_show(char const PROGMEM* words) {
    ++shown;
    last_text = words;
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    last_speaker = id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

static void interaction_geometry() {
    constexpr int8_t OFFSETS[] = {-25, -24, -11, -10, -1, 0, 1, 10, 11, 24, 25};
    constexpr int8_t FACING_X[] = {0, 1, 0, -1, 0}, FACING_Y[] = {-1, 0, 1, 0, 0};
    game_start();
    uint16_t px = game.player.x, py = game.player.y;
    for(uint8_t facing = NORTH; facing <= STILL; ++facing) {
        game.player.facing = (DIRECTION)facing;
        for(int8_t dx : OFFSETS) {
            for(int8_t dy : OFFSETS) {
                for(uint8_t swap = 0; swap < 2; ++swap) {
                    for(uint8_t tie = 0; tie < 2; ++tie) {
                        game.npcs.actors[0] = {};
                        game.npcs.actors[1] = {};
                        game.npcs.actors[2] = {};
                        NPC_ID expected = NPC_NONE;
                        uint16_t nearest = 65535;
                        for(uint8_t slot = 0; slot < 2; ++slot) {
                            Npc& n = game.npcs.actors[slot];
                            n.id = (slot ^ swap) ? NPC_WANDERER : NPC_VILLAGER;
                            int16_t x = slot && !tie ? -dx : dx;
                            int16_t y = slot && !tie ? -dy : dy;
                            n.actor = {(uint16_t)(px + x), (uint16_t)(py + y), STILL, 0};
                            int16_t forward = x * FACING_X[facing] + y * FACING_Y[facing];
                            int16_t side = x * FACING_Y[facing] - y * FACING_X[facing];
                            if(forward < 1 || forward > 24 || side < -10 || side > 10)
                                continue;
                            uint16_t distance = (uint16_t)(forward * TILE + abs(side));
                            if(distance < nearest || (distance == nearest && n.id < expected)) {
                                expected = n.id;
                                nearest = distance;
                            }
                        }
                        game.can_interact = false;
                        frame_pressed = BTN_A;
                        npc_interact();
                        CHECK(game.can_interact == (expected != NPC_NONE));
                        CHECK(frame_pressed == (expected ? 0 : BTN_A));
                        for(uint8_t slot = 0; slot < 2; ++slot) {
                            Npc const& n = game.npcs.actors[slot];
                            CHECK(n.actor.facing ==
                                  (n.id == expected ? (DIRECTION)((facing + 2) & 3) : STILL));
                        }
                    }
                }
            }
        }
    }
    npc_talk(NPC_NONE);
}

// The traveler is a named speaker whose policy places no actor. Boulders are objects and unnamed.
static void names() {
    CHECK(npc_name(NPC_NONE) == nullptr);
    CHECK(npc_name((NPC_ID)NPC_ID_LIMIT) == nullptr);
    CHECK(!strcmp(npc_name(NPC_ALDEN), "Alden"));
    CHECK(npc_location(NPC_ALDEN).area == 0 && !npc_definition(NPC_ALDEN)->talkable);
    for(uint8_t id = 1; id < NPC_ID_LIMIT; ++id) {
        bool boulder = id >= NPC_BOULDER_A && id <= NPC_BOULDER_D;
        CHECK(boulder ? !npc_name((NPC_ID)id) : npc_name((NPC_ID)id) && *npc_name((NPC_ID)id));
    }
    CHECK(!strcmp(npc_name(NPC_ARCHER), "Archer"));
}

// A talk request waits for the Dialogue phase, which shows it once under the speaker's name.
static void requests() {
    game_new();
    npc_dialogue();
    CHECK(shown == 0);
    npc_talk(NPC_WANDERER);
    CHECK(shown == 0);
    npc_dialogue();
    CHECK(shown == 1 && last_speaker == NPC_WANDERER && !strcmp(last_text, "Hello."));
    npc_dialogue();
    CHECK(shown == 1);
    npc_talk(NPC_NONE);
    npc_dialogue();
    CHECK(shown == 1);

    // Interaction takes the actual path: the player faces the villager from the west.
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(villager);
    world_enter(AREA_OUTDOOR, villager->actor.x - TILE, villager->actor.y, EAST);
    CHECK(npc_find(NPC_VILLAGER));
    game_update(0, BTN_A);
    CHECK(shown == 2 && last_speaker == NPC_VILLAGER);
    CHECK(npc_find(NPC_VILLAGER)->actor.facing == WEST);
}

static void party_limits() {
    story_stub_reset(BATTLE_VICTORY);
    game_new();
    CHECK(story_stub_joins() == 0);
    CHECK(game.party.count == 1 && game.party.members[0].id == NPC_ALDEN);
    CHECK(game.party.members[0].hp == 12 && party_max_hp(game.party.members[0]) == 12);
    CHECK(game.party.members[0].ap == 4 && party_max_ap(game.party.members[0]) == 4);
    CHECK(game.party.members[0].movement_class == MOVEMENT_STEP);
    CHECK(game.party.members[0].attack_class == ATTACK_STRIKE);

    // A recruit leaves the map, and the compacted actors leave no hole.
    CHECK(npc_find(NPC_ARCHER));
    CHECK(party_add(party_new_member(NPC_ARCHER, MOVEMENT_STEP, ATTACK_SHOOT)));
    CHECK(story_stub_joins() == 1 && story_stub_last_join() == NPC_ARCHER);
    CHECK(!npc_find(NPC_ARCHER) && npc_find(NPC_VILLAGER));
    bool empty = false;
    for(auto const& actor : game.npcs.actors) {
        if(!actor.id)
            empty = true;
        else
            CHECK(!empty);
    }
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_ARCHER));
    CHECK(save_valid());
    Game recruited = game;
    game.party.members[1].hp = 7;
    CHECK(save_restore());
    update_visible_world(0, false);
    CHECK(game.party.count == 2 && game.party.members[1].hp == 7);
    CHECK(!npc_find(NPC_ARCHER));

    world_enter(AREA_INTERIORS, sites::INN.x * TILE, (sites::INN.y + 1) * TILE, NORTH);
    world_enter(AREA_OUTDOOR, sites::PLAYER_START.x * TILE, sites::PLAYER_START.y * TILE, NORTH);
    CHECK(!npc_find(NPC_ARCHER));
    CHECK(npc_find(NPC_VILLAGER) && npc_location(NPC_WANDERER) == sites::LANE);
    CHECK(story_stub_joins() == 1);

    // Capacity, identity and statistic failures leave the entire party untouched.
    CHECK(party_add({12, 4, MOVEMENT_LEAP, ATTACK_STRIKE, NPC_VILLAGER, 1, 0}));
    CHECK(party_add({0, 1, MOVEMENT_STEP, ATTACK_SHOOT, NPC_WANDERER, 1, 0}));
    CHECK(game.party.count == PARTY_CAPACITY && save_valid());
    CHECK(story_stub_joins() == 3 && story_stub_last_join() == NPC_WANDERER);
    Party full = game.party;
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_SHOOT, NPC_SMITH, 1, 0}));
    CHECK(!memcmp(&full, &game.party, sizeof(Party)));
    game = recruited;
    Party before = game.party;
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ARCHER, 1, 0}));
    CHECK(!party_add({13, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_VILLAGER, 1, 0}));
    CHECK(!party_add({12, 5, MOVEMENT_STEP, ATTACK_SHOOT, NPC_VILLAGER, 1, 0}));
    CHECK(!party_add({1, 1, MOVEMENT_CLASS_COUNT, ATTACK_SHOOT, NPC_VILLAGER, 1, 0}));
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_CLASS_COUNT, NPC_VILLAGER, 1, 0}));
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_SHOOT, (NPC_ID)NPC_ID_LIMIT, 1, 0}));
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_SHOOT, NPC_NONE, 1, 0}));
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ALDEN, 1, 0}));
    CHECK(!memcmp(&before, &game.party, sizeof(Party)));
    CHECK(story_stub_joins() == 3);
    game_new();
    CHECK(game.party.count == 1 && !party_contains(NPC_ARCHER));
    CHECK(npc_find(NPC_ARCHER));
}

int main() {
    names();
    interaction_geometry();
    requests();
    party_limits();
    printf("npc dialogue checks passed: %u\n", checks);
    return 0;
}
