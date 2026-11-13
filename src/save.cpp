#include "rpg.h"
#include "npc.h"

bool save_valid() {
    if(game.save_version != SAVE_VERSION || game.player.facing > WEST || !game.max_hp ||
       game.hp > game.max_hp || !game.level || !game.rng)
        return false;
    if(FLAG_COUNT % 8 && (game.flags.bytes[WorldFlags::BYTE_COUNT - 1] >> (FLAG_COUNT % 8)))
        return false;
    for(uint8_t id = 1; id < NPC_ID_LIMIT; ++id)
        if(!npc_destination_valid(game.npcs.residence[id]))
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
    game.ticks = 0;
    game.can_interact = false;
    return world_place(game.area, game.player.x, game.player.y, game.player.facing);
}
