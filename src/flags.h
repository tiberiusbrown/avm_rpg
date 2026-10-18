#ifndef FLAGS_H
#define FLAGS_H

#include <stdint.h>

// Stable bit indices, not masks. Append new flags before FLAG_COUNT.
// No quests in this sample; keep one reserved bit for the generic save/flag API.
enum FLAG : uint16_t { FLAG_RESERVED = 0, FLAG_COUNT };

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
