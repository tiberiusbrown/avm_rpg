#ifndef LOGIC_STORY_H
#define LOGIC_STORY_H

#include "rpg.h"

// The story script: what the characters say and the objects do when the traveler reaches them.
// The per-frame hooks are declared in rpg.h, and story.cpp defines everything.

// Eliza's conversation, from asking for help to hearing that the rat is gone.
void story_talk_eliza();
// Rolls a boulder one tile away from the traveler, if the gate has room for it.
void story_push_boulder(NPC_ID id);

#endif
