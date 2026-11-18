#ifndef FLAGS_H
#define FLAGS_H

#include <stdint.h>

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
    FLAG_COUNT
};

template <uint16_t COUNT> struct FlagSet {
    static_assert(COUNT > 0, "a flag set needs at least one flag");
    static constexpr uint16_t BYTE_COUNT = COUNT / 8 + (COUNT % 8 != 0);
    uint8_t bytes[BYTE_COUNT];

    bool get_flag(uint16_t flag) const {
        return flag < COUNT && (bytes[flag / 8] & (uint8_t)(1u << (flag % 8))) != 0;
    }

    void set_flag(uint16_t flag) {
        if(flag < COUNT)
            bytes[flag / 8] |= (uint8_t)(1u << (flag % 8));
    }

    void clear_flag(uint16_t flag) {
        if(flag < COUNT)
            bytes[flag / 8] &= (uint8_t)~(1u << (flag % 8));
    }

    void toggle_flag(uint16_t flag) {
        if(flag < COUNT)
            bytes[flag / 8] ^= (uint8_t)(1u << (flag % 8));
    }
};

using WorldFlags = FlagSet<FLAG_COUNT>;
static_assert(sizeof(WorldFlags) == WorldFlags::BYTE_COUNT, "flags use exactly the required bytes");

#endif
