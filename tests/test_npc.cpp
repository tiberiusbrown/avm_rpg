#include "rpg.h"
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

bool dialogue_ask(char const PROGMEM*) {
    ++npc_test_choices;
    return true;
}

int main() {
    run_npc_scenarios();
    printf("PASS essential NPCs: %u checks\n", checks);
}
