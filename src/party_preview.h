#ifndef PARTY_PREVIEW_H
#define PARTY_PREVIEW_H

#include "rpg.h"

// Sample content for the Party's preview pages. Nothing here is learned, owned or equipped, and no
// gameplay reads it. Replace these tables with real perk tracks, equipment and ownership when those
// systems exist. The Party screen only reads them through the functions below.

// Perk tracks come first for the movement classes, then for the attack classes, so a track's index
// is its movement class or MOVEMENT_CLASS_COUNT plus its attack class. Each track has ranks 0 to
// PREVIEW_RANK_COUNT - 1 in order.
inline constexpr uint8_t PREVIEW_TRACK_COUNT =
    (uint8_t)MOVEMENT_CLASS_COUNT + (uint8_t)ATTACK_CLASS_COUNT;
inline constexpr uint8_t PREVIEW_RANK_COUNT = 5;
// Equipment-granted effects, each naming the item that would grant it.
inline constexpr uint8_t PREVIEW_EQUIPMENT_COUNT = 6;
// Items per equipment slot, which the slots index in the order weapon, armor, relic.
inline constexpr uint8_t PREVIEW_SLOT_COUNT = 3;
inline constexpr uint8_t PREVIEW_ITEMS_PER_SLOT = 2;

char const PROGMEM* preview_rank_name(uint8_t track, uint8_t rank);
char const PROGMEM* preview_rank_text(uint8_t track, uint8_t rank);
char const PROGMEM* preview_equipment_name(uint8_t index);
// The item that would grant an equipment effect, named as the effect's source.
char const PROGMEM* preview_equipment_source(uint8_t index);
char const PROGMEM* preview_equipment_text(uint8_t index);
char const PROGMEM* preview_item_name(uint8_t slot, uint8_t index);
char const PROGMEM* preview_item_text(uint8_t slot, uint8_t index);

#endif
