#ifndef ENCOUNTER_H
#define ENCOUNTER_H

#include "rpg.h"

inline constexpr uint16_t REMEMBER_DELAY_FRAMES = 15000;

constexpr FLAG enemy_defeated_flag(NPC_ID id) {
    return npc_is_enemy(id) ? (FLAG)(FLAG_ENEMY_FIRST + (uint8_t)id - NPC_ENEMY_FIRST) : FLAG_COUNT;
}

struct EnemyPursuit {
    uint8_t detect, disengage, leash;
};

EnemyPursuit enemy_pursuit(NPC_ID id);
void encounter_contact(NPC_ID id);
void encounter_restore();
bool encounter_run(); // Returns false on Game Over.
bool remember_tick(); // Once per exploration frame, after modal work.
void remember_reset();
bool remember_save_valid();

#endif
