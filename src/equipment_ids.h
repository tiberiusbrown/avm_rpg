#ifndef EQUIPMENT_IDS_H
#define EQUIPMENT_IDS_H

#include <stdint.h>

// Append new items before ITEM_DEFINED_COUNT.
enum ITEM_ID : uint8_t {
    ITEM_FIRSTSTRIKE_SWORD,
    ITEM_HUNTERS_BLADE,
    ITEM_NEEDLE_SPEAR,
    ITEM_CRACKED_SABER,
    ITEM_BRASS_KNUCKLE,
    ITEM_STONEBREAKER,
    ITEM_WOLF_FANG,
    ITEM_THORN_PIKE,
    ITEM_PATHFINDERS_KNIFE,
    ITEM_WOODCUT_AXE,
    ITEM_MUD_SLING,
    ITEM_BARN_FORK,
    ITEM_GUARD_VEST,
    ITEM_LEATHER_JERKIN,
    ITEM_OAK_SHIELD,
    ITEM_REED_ARMOR,
    ITEM_RIVER_CLOAK,
    ITEM_ASH_CLOAK,
    ITEM_SCOUT_SANDALS,
    ITEM_SOFT_BANDAGES,
    ITEM_PADDED_SHAWL,
    ITEM_LEAD_BOOTS,
    ITEM_TURNSHOE,
    ITEM_HOPPER_VEST,
    ITEM_MOON_SHARD,
    ITEM_SURVIVORS_RIBBON,
    ITEM_FERRY_BEAD,
    ITEM_THORN_RING,
    ITEM_GRAVE_TOKEN,
    ITEM_SLUICE_TALLY,
    ITEM_SHARED_CUP,
    ITEM_LANTERN_WICK,
    ITEM_TESSAS_ROSETTE,
    ITEM_SPLIT_BUTTON,
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

#endif
