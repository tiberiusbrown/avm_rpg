#include "rpg.h"
#include "chunk.h"
#include "story_stub.h"
#include "map_positions.h"
#include "logic/npc_logic.h"
#include "quest.h"
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

// Questions take the scripted answer. Silas's offer is accepted unless a test changes it.
static bool answer_yes = true;
static unsigned questions;

bool dialogue_ask(char const PROGMEM* question) {
    ++questions;
    return question && answer_yes;
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

int main() {
    CHECK(npc_name(NPC_NONE) == nullptr);
    CHECK(npc_name((NPC_ID)NPC_ID_LIMIT) == nullptr);
    for(uint8_t id = 1; id < NPC_ID_LIMIT; ++id)
        CHECK(npc_name((NPC_ID)id) && *npc_name((NPC_ID)id));
    CHECK(!strcmp(npc_name(NPC_ROWAN), "Rowan"));
    interaction_geometry();
    // Without a pending request, the Dialogue phase shows nothing.
    npc_dialogue();
    CHECK(shown == 0);

    // Talking only records the request; nothing is shown until the Dialogue phase.
    npc_talk(NPC_VILLAGER);
    CHECK(shown == 0);
    npc_dialogue();
    CHECK(shown == 1 && strstr(last_text, "Nocturne Grove"));
    CHECK(last_speaker == NPC_VILLAGER);

    // The request is consumed by the phase that shows it.
    npc_dialogue();
    CHECK(shown == 1);

    npc_talk(NPC_WANDERER);
    npc_dialogue();
    CHECK(shown == 2 && strstr(last_text, "old oak"));
    CHECK(last_speaker == NPC_WANDERER);
    // The first conversation with Silas asks him to find Iona, and an answer of yes starts it.
    CHECK(questions == 1 && quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE);

    // NPCs without a line stay silent, and the request still clears.
    npc_talk(NPC_NONE);
    npc_dialogue();
    CHECK(shown == 2);
    // Declining leaves the quest unstarted, and Silas asks again on the next conversation.
    game_new();
    answer_yes = false;
    npc_talk(NPC_WANDERER);
    npc_dialogue();
    CHECK(questions == 2 && quest_status(QUEST_FIND_IONA) == QUEST_NOT_STARTED);
    answer_yes = true;

    story_stub_reset(BATTLE_VICTORY);
    game_new();
    CHECK(story_stub_joins() == 0);
    CHECK(game.party.count == 1 && game.party.members[0].id == NPC_NONE);
    CHECK(game.party.members[0].hp == 20 && party_max_hp(game.party.members[0]) == 20);
    CHECK(game.party.members[0].ap == 4 && party_max_ap(game.party.members[0]) == 4);
    CHECK(game.party.members[0].movement_class == MOVEMENT_STEP);
    CHECK(game.party.members[0].attack_class == ATTACK_STRIKE);
    Npc* rowan = npc_find(NPC_ROWAN);
    CHECK(rowan && rowan->actor.x > game.player.x && rowan->actor.y > game.player.y);
    CHECK(rowan->actor.x - game.player.x <= 2 * TILE);
    CHECK(rowan->actor.y - game.player.y <= TILE);
    // Approach the NPC from the west and use the actual interaction path.
    world_enter(AREA_OUTDOOR, rowan->actor.x - TILE, rowan->actor.y, EAST);
    CHECK(npc_find(NPC_ROWAN));
    game_update(0, BTN_A);
    CHECK(game.party.count == 2 && party_contains(NPC_ROWAN));
    CHECK(last_speaker == NPC_ROWAN);
    CHECK(story_stub_joins() == 1 && story_stub_last_join() == NPC_ROWAN);
    CHECK(!npc_find(NPC_ROWAN));
    CHECK(npc_find(NPC_VILLAGER) && npc_find(NPC_WANDERER));
    // Compact removal must leave no holes before another occupied slot.
    bool empty = false;
    for(auto const& actor : game.npcs.actors) {
        if(!actor.id)
            empty = true;
        else
            CHECK(!empty);
    }
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_ROWAN));
    CHECK(game.party.members[1].hp == 18 && party_max_hp(game.party.members[1]) == 18);
    CHECK(game.party.members[1].ap == 4 && party_max_ap(game.party.members[1]) == 4);
    CHECK(game.party.members[1].movement_class == MOVEMENT_STEP);
    CHECK(game.party.members[1].attack_class == ATTACK_SHOOT);
    CHECK(save_valid());
    Game recruited = game;
    npc_talk(NPC_ROWAN);
    npc_dialogue();
    CHECK(game.party.count == 2);
    game.party.members[1].hp = 7;
    CHECK(save_restore());
    update_visible_world(0, false);
    CHECK(game.party.count == 2 && game.party.members[1].hp == 7);
    CHECK(!npc_find(NPC_ROWAN));
    npc_talk(NPC_ROWAN);
    npc_dialogue();
    CHECK(game.party.count == 2 && game.party.members[1].hp == 7);
    CHECK(!npc_find(NPC_ROWAN));

    world_enter(AREA_INTERIORS, positions::ElderExit.x * TILE,
                (positions::ElderExit.y - 1) * TILE, SOUTH);
    world_enter(AREA_OUTDOOR, positions::PlayerStart.x * TILE, positions::PlayerStart.y * TILE, NORTH);
    CHECK(!npc_find(NPC_ROWAN));
    CHECK(npc_find(NPC_VILLAGER) && npc_find(NPC_WANDERER));

    CHECK(story_stub_joins() == 1);

    // Capacity, identity and statistic failures leave the entire party untouched.
    CHECK(party_add({20, 4, MOVEMENT_LEAP, ATTACK_STRIKE, NPC_VILLAGER}));
    CHECK(party_add({0, 1, MOVEMENT_STEP, ATTACK_SHOOT, NPC_WANDERER}));
    CHECK(game.party.count == PARTY_CAPACITY && save_valid());
    CHECK(story_stub_joins() == 3 && story_stub_last_join() == NPC_WANDERER);
    Party full = game.party;
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ROWAN}));
    CHECK(!memcmp(&full, &game.party, sizeof(Party)));
    game = recruited;
    Party before = game.party;
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ROWAN}));
    CHECK(!party_add({21, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_VILLAGER}));
    CHECK(!party_add({20, 5, MOVEMENT_STEP, ATTACK_SHOOT, NPC_VILLAGER}));
    CHECK(!party_add({1, 1, MOVEMENT_CLASS_COUNT, ATTACK_SHOOT, NPC_VILLAGER}));
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_CLASS_COUNT, NPC_VILLAGER}));
    CHECK(!party_add({1, 1, MOVEMENT_STEP, ATTACK_SHOOT, (NPC_ID)NPC_ID_LIMIT}));
    CHECK(!memcmp(&before, &game.party, sizeof(Party)));
    CHECK(story_stub_joins() == 3);
    game_new();
    CHECK(game.party.count == 1 && !party_contains(NPC_ROWAN));
    CHECK(npc_find(NPC_ROWAN));

    printf("npc dialogue checks passed: %u\n", checks);
    return 0;
}
