#include "npc.h"

#include "enemy_catalog.h"
#include "handlers.h"
#include "npc_content.h"

using EssentialContent = NpcContentCatalog<std::make_index_sequence<NPC_ID_LIMIT - 1>>;

char const PROGMEM* npc_name(NPC_ID id) {
    return id < NPC_ID_LIMIT ? EssentialContent::names[id] : nullptr;
}

NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    if(id < NPC_ID_LIMIT)
        return EssentialContent::definitions[id];
    return npc_is_enemy(id) ? AuthoredEnemies::definitions[id - NPC_ENEMY_FIRST] : nullptr;
}
