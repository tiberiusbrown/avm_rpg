#ifndef FLAGS_H
#define FLAGS_H

#include "equipment_ids.h"

// Saved capacity stays fixed as the area catalogs grow.
inline constexpr uint8_t ENEMY_DEFEAT_CAPACITY = 16;

// Stable bit indices, not masks. Append new flags before FLAG_COUNT.
enum FLAG : uint16_t {
    FLAG_RESERVED = 0,
    // The opening in the forest clearing and the way home.
    FLAG_INTRO_SHOWN,      // The traveler spoke on waking.
    FLAG_HOME_STARTED,     // Go Home is under way.
    FLAG_BOULDERS_NOTICED, // The traveler saw that boulders block the way out.
    FLAG_LEFT_CLEARING,    // The traveler passed the boulders.
    FLAG_HOME_REACHED,     // The traveler stepped inside their home: Go Home is complete.
    // Eliza's Giant Rat.
    FLAG_ELIZA_GREETED, // Eliza asked for help on finding the traveler.
    FLAG_RAT_ACCEPTED,  // The traveler agreed to get rid of the rat.
    FLAG_RAT_SIGHTED,   // The traveler saw the rat in Eliza's bedroom.
    FLAG_CLASSES_CHOSEN,
    FLAG_RAT_REPORTED, // Eliza heard that the rat is gone: the quest is complete.
    // Pause menu items that the story unlocks.
    FLAG_MENU_JOURNAL,
    FLAG_MENU_PARTY,
    FLAG_MENU_SUPPLIES,
    // Expanding either saved range may require a save version bump.
    FLAG_ITEM_FIRST,
    FLAG_ITEM_END = FLAG_ITEM_FIRST + ITEM_DEFINED_COUNT,
    FLAG_ENEMY_FIRST = FLAG_ITEM_END,
    FLAG_COUNT = FLAG_ENEMY_FIRST + ENEMY_DEFEAT_CAPACITY
};

static_assert(FLAG_ITEM_END - FLAG_ITEM_FIRST == ITEM_DEFINED_COUNT);

// The giant rat is entry 0 of the enemy catalog; area_village.h checks it.
inline constexpr FLAG FLAG_RAT_DEFEATED = FLAG_ENEMY_FIRST;

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
