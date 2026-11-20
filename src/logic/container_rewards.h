#ifndef LOGIC_CONTAINER_REWARDS_H
#define LOGIC_CONTAINER_REWARDS_H

#include "rpg.h"

namespace handler_impl {

struct SupplyReward {
    SUPPLY_ID id;
    uint8_t amount;
};

// Stage the entire reward before changing the party's supplies or gold.
template <uint16_t GOLD, SupplyReward... STACKS> struct GoldSupplyReward {
    static bool claim() {
        Supplies next = game.supplies;
        if(!(supplies_add(next, STACKS.id, STACKS.amount) && ...)) {
            dialogue_show(F("You cannot carry all the supplies."));
            return false;
        }
        game.supplies = next;
        game.gold += GOLD;
        return true;
    }
};

} // namespace handler_impl

#endif
