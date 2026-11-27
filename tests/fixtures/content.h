#pragma once

#include "cast.h"
#include "logic/npc_content.h"

// Fixture characters: their definitions cover standing and wandering villagers at several speeds.
template <NPC_ID ID> struct FixtureTalk {
    static void talk() {
        dialogue_show(ID, F("Hello."));
    }
};

template <> struct NpcContent<NPC_VILLAGER> : FixtureTalk<NPC_VILLAGER> {
    static constexpr auto name = F("Villager");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 2, 32, 32,  0,
                                                      0,         4,     0, 9,  true};
};

template <> struct NpcContent<NPC_WANDERER> : FixtureTalk<NPC_WANDERER> {
    static constexpr auto name = F("Wanderer");
    static constexpr NpcDefinition PROGMEM definition{NPC_WANDER, WEST, 2, 48, 48,  16,
                                                      12,         3,    0, 2,  true};
};

template <> struct NpcContent<NPC_ARCHER> : FixtureTalk<NPC_ARCHER> {
    static constexpr auto name = F("Archer");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, WEST, 0, 32, 32,  0,
                                                      0,         4,    0, 3,  true};
};

template <> struct NpcContent<NPC_INNKEEPER> : FixtureTalk<NPC_INNKEEPER> {
    static constexpr auto name = F("Innkeeper");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 0, 32, 32,  0,
                                                      0,         4,     0, 11, true};
};

template <> struct NpcContent<NPC_SMITH> : FixtureTalk<NPC_SMITH> {
    static constexpr auto name = F("Smith");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 1, 32, 32,  0,
                                                      0,         4,     0, 7,  true};
};

template <> struct NpcContent<NPC_WATCHER> : FixtureTalk<NPC_WATCHER> {
    static constexpr auto name = F("Watcher");
    static constexpr NpcDefinition PROGMEM definition{NPC_WANDER, SOUTH, 2, 40, 44,  10,
                                                      8,          3,     0, 12, true};
};

template <> struct NpcContent<NPC_MERCHANT> : FixtureTalk<NPC_MERCHANT> {
    static constexpr auto name = F("Merchant");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 0, 32, 32,  0,
                                                      0,         4,     0, 14, true};
};

template <> struct NpcContent<NPC_SURVEY> : FixtureTalk<NPC_SURVEY> {
    static constexpr auto name = F("Surveyor");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 0, 32, 32,  0,
                                                      0,         5,     0, 10, true};
};
