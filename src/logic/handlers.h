#ifndef LOGIC_HANDLERS_H
#define LOGIC_HANDLERS_H

#include "handler_defaults.h"
#include "tile_ids.h"

// Tags associate these overloads with generated dependent calls through ADL.
namespace handler_impl {

template <TilePos const& POS> void update_north_gate(Tiles<POS>) {
    if(game.get_flag(FLAG_GATE_OPEN))
        set_tile<POS>(TILE_PATH);
}

template <TilePos const& POS> void update_north_gate(Dialogue<POS>) {
    if(player_just_interacted<POS>() && dialogue_ask(F("Toggle the north gate?")))
        game.toggle_flag(FLAG_GATE_OPEN);
}

template <TilePos const& POS> void update_bridge(Tiles<POS>) {
    if(game.get_flag(FLAG_BRIDGE_REPAIRED))
        set_tile<POS>(TILE_PATH);
}

template <TilePos const& POS> void update_bridge(Dialogue<POS>) {
    if(player_just_interacted<POS>() && dialogue_ask(F("Toggle the eastern bridge?")))
        game.toggle_flag(FLAG_BRIDGE_REPAIRED);
}

template <TilePos const& POS> void update_platform(Tiles<POS>) {
    if((game.ticks & 31) < 16)
        set_tile<POS>(TILE_PATH);
}

template <TilePos const& POS> void update_waystone(Gameplay<POS>) {
    if(!game.get_flag(FLAG_WAYSTONE) && game.player_is_on_tile<POS>()) {
        game.set_flag(FLAG_WAYSTONE);
        ++game.gold;
    }
}

template <TilePos const& POS> void update_house_door(Gameplay<POS>) {
    if(player_is_walking_onto<POS>())
        teleport_player<positions::InteriorArrival>(NORTH);
}

template <TilePos const& POS> void update_interior_exit(Gameplay<POS>) {
    if(player_is_walking_onto<POS>())
        teleport_player<positions::OutdoorArrival>(SOUTH);
}

template <TilePos const& POS> void update_lantern(Dialogue<POS>) {
    if(!player_just_interacted<POS>())
        return;

    if(game.get_flag(FLAG_HELPED)) {
        dialogue_show(F("The lantern is lit. Thanks for helping! I remember you."));
        return;
    }

    if(!dialogue_show(F("Welcome to our small test house. The path is outside. "
                        "We keep a lantern here for travelers returning after dark. "
                        "A little light helps everyone find their way home.")))
        return;

    if(dialogue_ask(F("Will you light the travelers' lantern? Your answer stays."))) {
        game.set_flag(FLAG_HELPED);
        game.gold += 5;
        dialogue_show(F("The lantern is lit. Thanks for helping! I remember you."));
    } else {
        dialogue_show(F("Perhaps another day. The door stays open."));
    }
}

} // namespace handler_impl

#endif
