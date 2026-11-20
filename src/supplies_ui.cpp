#include "supplies_ui.h"

char const PROGMEM* supply_name(SUPPLY_ID id) {
    static char const PROGMEM* const PROGMEM NAMES[] = {
        F("Potion"),
        F("Elixir"),
        F("Revive"),
        F("Ether"),
    };
    return NAMES[id];
}

char const PROGMEM* supply_description(SUPPLY_ID id) {
    static char const PROGMEM* const PROGMEM DESCRIPTIONS[] = {
        F("Restore up to 8 HP to one conscious party member."),
        F("Restore up to 4 HP to each conscious party member."),
        F("Restore an incapacitated party member to 5 HP, up to their maximum."),
        F("Restore up to 3 AP to one party member, even at 0 HP."),
    };
    return DESCRIPTIONS[id];
}

SUPPLIES_EVENT supplies_press(SuppliesUi& ui, Supplies const& supplies, Party const& party,
                              uint8_t pressed) {
    if(!pressed)
        return SUPPLIES_NONE;
    if(pressed & BTN_B) {
        ui.feedback = nullptr;
        if(ui.page == SUPPLIES_LIST)
            return SUPPLIES_CLOSE;
        ui.page = ui.page == SUPPLIES_DETAILS ? SUPPLIES_LIST : SUPPLIES_DETAILS;
        return SUPPLIES_BACK;
    }

    bool down = pressed & BTN_DOWN;
    if(pressed & (BTN_UP | BTN_DOWN)) {
        switch(ui.page) {
            case SUPPLIES_LIST:
                ui.supply = (SUPPLY_ID)((ui.supply + (down ? 1 : SUPPLY_COUNT - 1)) % SUPPLY_COUNT);
                ui.feedback = nullptr;
                return SUPPLIES_MOVE;
            case SUPPLIES_DETAILS:
                if(down && ui.scroll + SUPPLIES_DETAIL_LINES < ui.lines)
                    ++ui.scroll;
                else if(!down && ui.scroll)
                    --ui.scroll;
                else
                    return SUPPLIES_EDGE;
                return SUPPLIES_MOVE;
            case SUPPLIES_TARGET: {
                int16_t step = down ? 1 : -1;
                for(int16_t i = ui.target + step; i >= 0 && i < party.count; i += step) {
                    if(supply_can_use(ui.supply, party, (uint8_t)i)) {
                        ui.target = (uint8_t)i;
                        return SUPPLIES_MOVE;
                    }
                }
                return SUPPLIES_EDGE;
            }
            default:
                return SUPPLIES_NONE;
        }
    }
    if(!(pressed & BTN_A))
        return SUPPLIES_NONE;

    switch(ui.page) {
        case SUPPLIES_LIST:
            ui.page = SUPPLIES_DETAILS;
            ui.scroll = 0;
            ui.feedback = nullptr;
            return SUPPLIES_OPEN;
        case SUPPLIES_DETAILS:
            if(!supply_count(supplies, ui.supply)) {
                ui.feedback = F("None remaining");
            } else if(ui.supply == SUPPLY_ELIXIR) {
                if(supply_can_use(ui.supply, party, SUPPLY_NO_TARGET)) {
                    ui.page = SUPPLIES_CONFIRM;
                    ui.target = SUPPLY_NO_TARGET;
                    return SUPPLIES_OPEN;
                }
                ui.feedback = F("No healing needed");
            } else {
                for(uint8_t i = 0; i < party.count; ++i) {
                    if(supply_can_use(ui.supply, party, i)) {
                        ui.page = SUPPLIES_TARGET;
                        ui.target = i;
                        return SUPPLIES_OPEN;
                    }
                }
                ui.feedback =
                    ui.supply == SUPPLY_ETHER ? F("AP already full") : F("No valid target");
            }
            return SUPPLIES_EDGE;
        default:
            return SUPPLIES_APPLY;
    }
}
