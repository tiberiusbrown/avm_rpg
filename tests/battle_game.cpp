#include "rpg.h"

// The battle tests link the game rules, whose equipment and quest calls reach the world and the
// story. These tests never show dialogue.
bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_show(NPC_ID, char const PROGMEM*) {
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return false;
}

bool dialogue_ask(NPC_ID, char const PROGMEM*) {
    return false;
}
