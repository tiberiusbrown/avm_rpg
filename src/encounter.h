#ifndef ENCOUNTER_H
#define ENCOUNTER_H

#include "rpg.h"

inline constexpr uint16_t REMEMBER_DELAY_FRAMES = 15000;

struct EnemyPursuit {
    uint8_t detect, disengage, leash;
};

EnemyPursuit enemy_pursuit(NPC_ID id);
void encounter_contact(NPC_ID id);
void encounter_restore();
bool encounter_run(); // Returns false on Game Over.
bool remember_tick(); // Once per exploration frame, after modal work.
void remember_reset();
bool encounter_save_valid();

#endif
