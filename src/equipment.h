#ifndef EQUIPMENT_H
#define EQUIPMENT_H

#include "rpg.h"

struct EquipmentItem {
    EQUIPMENT_SLOT slot;
    char const PROGMEM* name;
    char const PROGMEM* modifier;
    char const PROGMEM* description;
};

EquipmentItem const PROGMEM* equipment_item(ITEM_ID item);
bool equipment_owned(ITEM_ID item);
bool equipment_acquire(ITEM_ID item);

// PARTY_CAPACITY means the item has no wearer.
uint8_t equipment_wearer(ITEM_ID item);
bool equipment_equip(uint8_t member, ITEM_ID item);
bool equipment_unequip(uint8_t member, EQUIPMENT_SLOT slot);
bool equipment_saved_state_valid();

#endif
