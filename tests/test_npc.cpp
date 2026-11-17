#include "rpg.h"
#include "battle.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)
#include "npc_scenarios.h"

bool dialogue_show(char const PROGMEM*) {
    ++npc_test_dialogues;
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM*) {
    ++npc_test_choices;
    return true;
}

// A party member is drawn in battle with the sprite row of the NPC it was recruited as. The
// traveler (NPC_NONE) has no definition and uses the player's row.
static void party_sprites_follow_npcs() {
    Party saved = game.party;
    for(uint8_t id = 0; id < NPC_ID_LIMIT; ++id) {
        game.party.members[0] = {255, 255, MOVEMENT_FIGHTER, ATTACK_FIGHTER, (NPC_ID)id};
        BattleUnit unit{0, 5, battle_party_id(0), BATTLE_PARTY};
        NpcDefinition const PROGMEM* definition = npc_definition((NPC_ID)id);
        CHECK(battle_sprite_row(unit) == (definition ? definition->sprite_row : 0));
    }
    game.party = saved;
}

int main() {
    run_npc_scenarios();
    party_sprites_follow_npcs();
    printf("PASS essential NPCs: %u checks\n", checks);
}
