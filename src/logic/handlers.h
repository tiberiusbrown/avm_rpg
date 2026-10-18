#ifndef LOGIC_HANDLERS_H
#define LOGIC_HANDLERS_H

#include "handler_defaults.h"

namespace handler_impl {

template <TilePos POS> void update_villager(Gameplay<POS>) {
    request_essential_npc<NPC_VILLAGER, POS>();
}

template <TilePos POS> void update_wanderer(Gameplay<POS>) {
    request_essential_npc<NPC_WANDERER, POS>();
}

template <TilePos POS> void read_sign(Dialogue<POS>) {
    if(player_just_interacted<POS>())
        dialogue_show(F("Welcome to Nocturne Grove. Take a quiet walk among the trees."));
}

} // namespace handler_impl

#endif
