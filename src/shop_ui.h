#ifndef SHOP_UI_H
#define SHOP_UI_H

#include "shop.h"
#include "supplies_ui.h"

enum SHOP_PAGE : uint8_t { SHOP_LIST, SHOP_DETAILS, SHOP_CONFIRM };

enum SHOP_EVENT : uint8_t {
    SHOP_NONE,
    SHOP_EDGE,
    SHOP_MOVE,
    SHOP_OPEN,
    SHOP_BACK,
    SHOP_CLOSE,
    SHOP_PURCHASE
};

struct ShopUi {
    SHOP_PAGE page{};
    SUPPLY_ID item{};
    uint8_t scroll{};
    uint8_t lines{};
    uint8_t starts[SUPPLIES_WRAP_CAPACITY + 1];
    char const PROGMEM* feedback{};
};

SHOP_EVENT shop_press(ShopUi& ui, uint8_t pressed);
void shop_feedback(ShopUi& ui, SHOP_RESULT result);
void shop_layout(ShopUi& ui);
void render_shop(uint8_t revealed, bool closing, ShopUi const& ui);

#endif
