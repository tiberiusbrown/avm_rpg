// Engine tests run without the game's story. These stand-ins keep it silent.
#include "logic/story.h"

void story_update() {}

bool story_class_choice_due() {
    return false;
}

void story_choose_classes(MOVEMENT_CLASS, ATTACK_CLASS) {}

uint16_t story_locked_menu_items() {
    return 0;
}

void story_talk_eliza() {}

void story_push_boulder(NPC_ID) {}
