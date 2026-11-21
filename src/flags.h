#ifndef FLAGS_H
#define FLAGS_H

#include "equipment_ids.h"

// Saved capacity stays fixed as the area catalogs grow.
inline constexpr uint8_t ENEMY_DEFEAT_CAPACITY = 16;

// Stable bit indices, not masks. Append new flags before FLAG_COUNT.
// The cave and dungeon flags record a mechanism's state: set means that its lever is thrown, its
// plate is down, its barrier is open or its chest is empty. The maps show the clear state.
enum FLAG : uint16_t {
    FLAG_RESERVED = 0,
    FLAG_CAVE_GATE,
    FLAG_CAVE_NOOK,
    FLAG_CAVE_BRIDGE,
    FLAG_CAVE_CHEST,
    FLAG_DUNGEON_SPIKES,
    FLAG_DUNGEON_CELL,
    FLAG_DUNGEON_GATE,
    FLAG_DUNGEON_BLOCKS,
    FLAG_DUNGEON_ORB,
    FLAG_DUNGEON_CHEST,
    FLAG_VILLAGER_MOVED,
    FLAG_VILLAGER_EAST,
    // Find Iona's history. Its status is derived from these two bits, and completion is never set
    // without the start bit.
    FLAG_QUEST_FIND_IONA_STARTED,
    FLAG_QUEST_FIND_IONA_DONE,
    // Expanding either saved range may require a save version bump.
    FLAG_ITEM_FIRST,
    FLAG_ITEM_END = FLAG_ITEM_FIRST + ITEM_DEFINED_COUNT,
    FLAG_ENEMY_FIRST = FLAG_ITEM_END,
    FLAG_COUNT = FLAG_ENEMY_FIRST + ENEMY_DEFEAT_CAPACITY
};

static_assert(FLAG_ITEM_END - FLAG_ITEM_FIRST == ITEM_DEFINED_COUNT);

constexpr FLAG enemy_defeated_flag_at(unsigned index) {
    return index < ENEMY_DEFEAT_CAPACITY ? (FLAG)(FLAG_ENEMY_FIRST + index) : FLAG_COUNT;
}

constexpr FLAG equipment_flag(uint8_t item) {
    return item < ITEM_DEFINED_COUNT ? (FLAG)(FLAG_ITEM_FIRST + item) : FLAG_COUNT;
}

template <uint16_t COUNT> struct FlagSet {
    static_assert(COUNT > 0, "a flag set needs at least one flag");
    static constexpr uint16_t BYTE_COUNT = COUNT / 8 + (COUNT % 8 != 0);
    uint8_t bytes[BYTE_COUNT];

    bool get_flag(uint16_t flag) const {
        return flag < COUNT && (bytes[flag / 8] & (uint8_t)(1u << (flag % 8))) != 0;
    }

    // Each mutator returns true only when it changed a valid flag, so callers can notify on change.
    bool set_flag(uint16_t flag) {
        if(flag >= COUNT || get_flag(flag))
            return false;
        bytes[flag / 8] |= (uint8_t)(1u << (flag % 8));
        return true;
    }

    bool clear_flag(uint16_t flag) {
        if(flag >= COUNT || !get_flag(flag))
            return false;
        bytes[flag / 8] &= (uint8_t)~(1u << (flag % 8));
        return true;
    }

    bool toggle_flag(uint16_t flag) {
        if(flag >= COUNT)
            return false;
        bytes[flag / 8] ^= (uint8_t)(1u << (flag % 8));
        return true;
    }
};

using WorldFlags = FlagSet<FLAG_COUNT>;
static_assert(sizeof(WorldFlags) == WorldFlags::BYTE_COUNT, "flags use exactly the required bytes");

#endif
