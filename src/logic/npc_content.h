#pragma once

#include "npc.h"

#include <utility>

template <NPC_ID ID> struct NpcContent;

template <typename> struct NpcContentCatalog;

template <std::size_t... I> struct NpcContentCatalog<std::index_sequence<I...>> {
    static constexpr char const PROGMEM* const PROGMEM names[] = {
        nullptr, NpcContent<(NPC_ID)(I + 1)>::name...
    };
    static constexpr NpcDefinition const PROGMEM* const PROGMEM definitions[] = {
        nullptr, &NpcContent<(NPC_ID)(I + 1)>::definition...
    };

    static void talk(NPC_ID id) {
        (void)((id == I + 1 && (NpcContent<(NPC_ID)(I + 1)>::talk(), true)) || ...);
    }
};
