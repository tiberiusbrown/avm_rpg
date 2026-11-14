#include "rpg.h"
#include "chunk.h"
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

bool dialogue_show(char const PROGMEM* words) {
    ++shown;
    last_text = words;
    return true;
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
                            CHECK(n.actor.facing == (n.id == expected
                                                         ? (DIRECTION)((facing + 2) & 3)
                                                         : STILL));
                        }
                    }
                }
            }
        }
    }
    npc_talk(NPC_NONE);
}

int main() {
    interaction_geometry();
    // Without a pending request, the Dialogue phase shows nothing.
    npc_dialogue();
    CHECK(shown == 0);

    // Talking only records the request; nothing is shown until the Dialogue phase.
    npc_talk(NPC_VILLAGER);
    CHECK(shown == 0);
    npc_dialogue();
    CHECK(shown == 1 && strstr(last_text, "Nocturne Grove"));

    // The request is consumed by the phase that shows it.
    npc_dialogue();
    CHECK(shown == 1);

    npc_talk(NPC_WANDERER);
    npc_dialogue();
    CHECK(shown == 2 && strstr(last_text, "old oak"));

    // NPCs without a line stay silent, and the request still clears.
    npc_talk(NPC_NONE);
    npc_dialogue();
    CHECK(shown == 2);

    printf("npc dialogue checks passed: %u\n", checks);
    return 0;
}
