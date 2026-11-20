#ifndef EQUIPMENT_IDS_H
#define EQUIPMENT_IDS_H

#include <stdint.h>

// Saved IDs are permanent. Append new items before ITEM_DEFINED_COUNT.
enum ITEM_ID : uint8_t {
    ITEM_IRON_FANG,
    ITEM_TRAVELERS_BOOTS,
    ITEM_DAWN_PENDANT,
    ITEM_VITAL_PENDANT,
    ITEM_DEFINED_COUNT,
    ITEM_NONE = 0xff,
};

enum EQUIPMENT_SLOT : uint8_t { SLOT_WEAPON, SLOT_ARMOR, SLOT_RELIC, SLOT_COUNT };

constexpr bool equipment_valid_item(ITEM_ID item) {
    return item < ITEM_DEFINED_COUNT;
}

constexpr uint8_t equipment_encode(ITEM_ID item) {
    return equipment_valid_item(item) ? (uint8_t)(item + 1) : 0;
}

constexpr ITEM_ID equipment_decode(uint8_t stored) {
    return stored && stored <= ITEM_DEFINED_COUNT ? (ITEM_ID)(stored - 1) : ITEM_NONE;
}

inline constexpr uint8_t IRON_FANG_DAMAGE = 1;
inline constexpr uint8_t TRAVELERS_BOOTS_HP = 2;
inline constexpr uint8_t DAWN_PENDANT_AP = 1;
inline constexpr uint8_t VITAL_PENDANT_HP = 3;

#endif
