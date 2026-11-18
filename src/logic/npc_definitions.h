#ifndef NPC_DEFINITIONS_H
#define NPC_DEFINITIONS_H

#include "npc.h"

namespace npc_definitions {
inline constexpr NpcDefinition PROGMEM VILLAGER{NPC_STAND, SOUTH, 2, 32, 32, 0, 0, 3, 0, 9, true};
inline constexpr NpcDefinition PROGMEM WANDERER{NPC_WANDER, WEST, 2, 48, 48, 16, 12, 2, 0, 2, true};
inline constexpr NpcDefinition PROGMEM ROWAN{NPC_STAND, WEST, 0, 32, 32, 0, 0, 3, 0, 3, true};
} // namespace npc_definitions

#endif
