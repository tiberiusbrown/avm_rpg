#ifndef BATTLE_AI_H
#define BATTLE_AI_H

#include "battle.h"

// Greedy enemy choice for the side to act. Kills beat attacks, attacks beat moves, and a move
// scores higher the closer it leaves its unit to the nearest living party member. Ties go to the
// lowest unit index, then to the direction order north, east, south, west. The choice is only
// meaningful while battle_can_act is true, and it is always a legal action.
BattleAction battle_enemy_action(BattleState const& state);

#endif
