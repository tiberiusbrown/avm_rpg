#ifndef LOGIC_NPCS_COURIER_H
#define LOGIC_NPCS_COURIER_H

#include "common.h"
#include "npc_routes.h"

namespace npc_logic {

struct Courier : Defaults {
    static inline constexpr NpcDefinition PROGMEM DEFINITION{
        NPC_ROUTE, EAST, 1, 8, 8, 0, 0, 3, routes::CourierWalk};

    static NpcDefinition const PROGMEM* definition() {
        return &DEFINITION;
    }

    static uint8_t placement() {
        return game.get_flag(FLAG_COURIER_DISPATCHED) || game.get_flag(FLAG_COURIER_ARRIVED);
    }

    static TilePos home(uint8_t placement) {
        return placement ? positions::CourierFinish : positions::CourierStart;
    }

    static void activate(Npc& npc) {
        if(npc.placement)
            npc.behavior = NPC_STAND;
    }

    static void arrive() {
        game.set_flag(FLAG_COURIER_ARRIVED);
    }

    static void event(Npc& npc, NPC_EVENT event) {
        if(npc.route != routes::CourierWalk)
            return;
        if(event == NPC_EVENT_ROUTE_STARTED)
            game.set_flag(FLAG_COURIER_DISPATCHED);
        else if(event == NPC_EVENT_ROUTE_COMPLETED) {
            arrive();
            npc.placement = 1;
        }
    }

    static void retire(Npc const& npc, NPC_RETIRE_REASON reason) {
        if(npc.behavior != NPC_ROUTE || npc.route != routes::CourierWalk ||
           !game.get_flag(FLAG_COURIER_DISPATCHED) || game.get_flag(FLAG_COURIER_ARRIVED))
            return;
        if(reason == NPC_RETIRED_TERRAIN) {
            // An unsafe position interrupts delivery; retry from its source.
            game.clear_flag(FLAG_COURIER_DISPATCHED);
        } else {
            arrive();
        }
    }

    static void restore() {
        // Saved travel has no live actor to notify. Resolve once during reconstruction.
        if(game.get_flag(FLAG_COURIER_DISPATCHED))
            arrive();
    }

    static void talk() {
        dialogue_show(game.get_flag(FLAG_COURIER_ARRIVED)
                          ? F("Pip: The delivery is complete.")
                          : F("Pip: I am following the delivery route east."));
    }
};

} // namespace npc_logic

#endif
