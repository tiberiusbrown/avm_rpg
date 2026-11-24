#ifndef LOGIC_CONTAINER_REWARDS_H
#define LOGIC_CONTAINER_REWARDS_H

#include "equipment.h"

namespace handler_impl {

struct SupplyReward {
    SUPPLY_ID id;
    uint8_t amount;
};

struct ItemReward {
    ITEM_ID first = ITEM_NONE;
    ITEM_ID second = ITEM_NONE;
};

template <uint16_t GOLD, ItemReward ITEMS, SupplyReward... STACKS> struct ContainerReward {
    static_assert(ITEMS.first == ITEM_NONE || equipment_valid_item(ITEMS.first));
    static_assert(ITEMS.second == ITEM_NONE ||
                  (equipment_valid_item(ITEMS.second) && ITEMS.second != ITEMS.first));
    static bool claim() {
        Supplies next = game.supplies;
        if(!(supplies_add(next, STACKS.id, STACKS.amount) && ...)) {
            dialogue_show(F("You cannot carry all the supplies."));
            return false;
        }
        if(game.gold > UINT16_MAX - GOLD) {
            dialogue_show(F("You cannot carry all the gold."));
            return false;
        }
        if constexpr(ITEMS.first != ITEM_NONE || ITEMS.second != ITEM_NONE) {
            if(equipment_owned(ITEMS.first) || equipment_owned(ITEMS.second)) {
                dialogue_show(F("You already own equipment in this chest."));
                return false;
            }
        }

        game.supplies = next;
        game.gold += GOLD;
        if constexpr(ITEMS.first != ITEM_NONE)
            equipment_acquire(ITEMS.first);
        if constexpr(ITEMS.second != ITEM_NONE)
            equipment_acquire(ITEMS.second);
        return true;
    }
};

} // namespace handler_impl

#endif
