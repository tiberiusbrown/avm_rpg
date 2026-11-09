#include "rpg.h"
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

int main() {
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
