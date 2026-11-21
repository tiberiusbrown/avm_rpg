#ifndef SHOP_H
#define SHOP_H

#include "supplies.h"

enum SHOP_RESULT : uint8_t {
    SHOP_SUCCESS,
    SHOP_NOT_ENOUGH_GOLD,
    SHOP_STACK_FULL,
    SHOP_INVALID_ITEM
};

uint8_t shop_price(SUPPLY_ID item);
SHOP_RESULT shop_can_buy(uint16_t gold, Supplies const& supplies, SUPPLY_ID item);
SHOP_RESULT shop_buy(uint16_t& gold, Supplies& supplies, SUPPLY_ID item);

#endif
