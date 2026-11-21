#include "shop.h"

#include "rpg.h"

uint8_t shop_price(SUPPLY_ID item) {
    static constexpr uint8_t PROGMEM PRICES[] = {12, 22, 30, 18};
    static_assert(sizeof(PRICES) == SUPPLY_COUNT);
    return supply_valid(item) ? PRICES[item] : 0;
}

static SHOP_RESULT validate_purchase(uint16_t gold, Supplies const& supplies, SUPPLY_ID item,
                                     uint8_t price) {
    if(!price)
        return SHOP_INVALID_ITEM;
    if(gold < price)
        return SHOP_NOT_ENOUGH_GOLD;
    if(!supplies_can_add(supplies, item, 1))
        return SHOP_STACK_FULL;
    return SHOP_SUCCESS;
}

SHOP_RESULT shop_can_buy(uint16_t gold, Supplies const& supplies, SUPPLY_ID item) {
    return validate_purchase(gold, supplies, item, shop_price(item));
}

SHOP_RESULT shop_buy(uint16_t& gold, Supplies& supplies, SUPPLY_ID item) {
    uint8_t price = shop_price(item);
    SHOP_RESULT result = validate_purchase(gold, supplies, item, price);
    if(result != SHOP_SUCCESS)
        return result;
    ++supplies.quantity[item];
    gold -= price;
    return SHOP_SUCCESS;
}
