#ifndef LOGIC_HANDLERS_H
#define LOGIC_HANDLERS_H

#include "handler_defaults.h"

namespace handler_impl {

template <TilePos POS> void read_sign(Dialogue<POS>) {
    if(player_just_interacted<POS>())
        dialogue_show(F("Welcome to Nocturne Grove. Take a quiet walk among the trees."));
}

} // namespace handler_impl

#endif
