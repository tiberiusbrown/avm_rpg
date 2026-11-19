#include "rpg.h"
#include "npc.h"
#include "quest.h"
#include <string.h>

bool save_valid() {
    if(game.options.music > OPTION_VOLUME_MAX || game.options.sfx > OPTION_VOLUME_MAX)
        return false;
    if(game.save_version != SAVE_VERSION || game.player.facing > WEST || !game.level || !game.rng)
        return false;
    if(!game.party.count || game.party.count > PARTY_CAPACITY ||
       game.party.members[0].id != NPC_NONE)
        return false;
    for(uint8_t i = 0; i < game.party.count; ++i) {
        PartyMember const& member = game.party.members[i];
        if(member.hp > party_max_hp(member) || member.ap > party_max_ap(member) ||
           member.id >= NPC_ID_LIMIT || member.movement_class >= MOVEMENT_CLASS_COUNT ||
           member.attack_class >= ATTACK_CLASS_COUNT)
            return false;
        for(uint8_t j = 0; j < i; ++j)
            if(game.party.members[j].id == member.id)
                return false;
    }
    if(FLAG_COUNT % 8 && (game.flags.bytes[WorldFlags::BYTE_COUNT - 1] >> (FLAG_COUNT % 8)))
        return false;
    // Malformed quest IDs and impossible quest-specific states are rejected, not repaired.
    if(!quest_saved_state_valid())
        return false;
    Area const PROGMEM* area = world_area(game.area);
    if(!area)
        return false;
    MapDimensions dimensions{area->width, area->height};
    return dimensions.pixel_bounds(game.player.x, game.player.y);
}

bool save_restore() {
    if(!save_valid())
        return false;

    // The entire pointer-free Game is saved in place. Rebuild its derived world/NPC state before
    // resuming, including overlap repair in the caller's first cache refresh.
    memset(&game.npcs, 0, sizeof(game.npcs));
    game.ticks = 0;
    game.can_interact = false;
    // A loaded save resumes with no pending evaluation. A stale tracked quest is dropped.
    quest_restore();
    return world_place(game.area, game.player.x, game.player.y, game.player.facing);
}
