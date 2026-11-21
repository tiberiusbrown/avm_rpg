#include "shop_ui.h"

SHOP_EVENT shop_press(ShopUi& ui, uint8_t pressed) {
    if(pressed & BTN_B) {
        ui.feedback = nullptr;
        if(ui.page == SHOP_LIST)
            return SHOP_CLOSE;
        ui.page = ui.page == SHOP_CONFIRM ? SHOP_DETAILS : SHOP_LIST;
        return SHOP_BACK;
    }

    if(pressed & (BTN_UP | BTN_DOWN)) {
        bool down = pressed & BTN_DOWN;
        if(ui.page == SHOP_LIST) {
            ui.item = (SUPPLY_ID)((ui.item + (down ? 1 : SUPPLY_COUNT - 1)) % SUPPLY_COUNT);
            ui.feedback = nullptr;
            return SHOP_MOVE;
        }
        if(ui.page == SHOP_CONFIRM)
            return SHOP_NONE;
        if(down && ui.scroll + SUPPLIES_DETAIL_LINES < ui.lines)
            ++ui.scroll;
        else if(!down && ui.scroll)
            --ui.scroll;
        else
            return SHOP_EDGE;
        return SHOP_MOVE;
    }

    if(!(pressed & BTN_A))
        return SHOP_NONE;
    if(ui.page == SHOP_CONFIRM)
        return SHOP_PURCHASE;
    ui.feedback = nullptr;
    if(ui.page == SHOP_LIST) {
        ui.page = SHOP_DETAILS;
        ui.scroll = 0;
    } else {
        ui.page = SHOP_CONFIRM;
    }
    return SHOP_OPEN;
}

void shop_feedback(ShopUi& ui, SHOP_RESULT result) {
    switch(result) {
        case SHOP_SUCCESS:
            ui.page = SHOP_LIST;
            ui.feedback = F("Purchased!");
            break;
        case SHOP_NOT_ENOUGH_GOLD:
            ui.feedback = F("Not enough gold");
            break;
        case SHOP_STACK_FULL:
            ui.feedback = F("Already carrying 15");
            break;
        case SHOP_INVALID_ITEM:
            ui.feedback = F("Cannot purchase");
            break;
    }
}
