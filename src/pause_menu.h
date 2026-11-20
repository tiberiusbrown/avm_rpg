#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include "rpg.h"

inline constexpr uint8_t PAUSE_FIRST_BASELINE = 17;
inline constexpr uint8_t PAUSE_ROW_STEP = 7;
inline constexpr uint8_t PAUSE_ROWS = 7;
static_assert(PAUSE_FIRST_BASELINE - 6 > 8);
static_assert(PAUSE_FIRST_BASELINE + (PAUSE_ROWS - 1) * PAUSE_ROW_STEP < 62);

constexpr bool menu_item_visible(PAUSE_ITEM item, bool can_load, bool in_battle, bool outdoors) {
    switch(item) {
        case PAUSE_RESUME:
        case PAUSE_AUDIO:
            return true;
        case PAUSE_SAVE:
        case PAUSE_PARTY:
        case PAUSE_SUPPLIES:
        case PAUSE_JOURNAL:
            return !in_battle;
        case PAUSE_MAP:
            return !in_battle && outdoors;
        case PAUSE_PASS:
        case PAUSE_RESIGN:
            return in_battle;
        case PAUSE_LOAD:
            return !in_battle && can_load;
        default:
            return false;
    }
}

struct PauseMenuView {
    uint16_t visible;
    uint8_t count;
    uint8_t selected;
    uint8_t first;
};

constexpr PAUSE_ITEM pause_item(PauseMenuView const& view, uint8_t row) {
    for(uint8_t item = 0; item < PAUSE_ITEM_COUNT; ++item) {
        if(view.visible & (1u << item)) {
            if(!row)
                return (PAUSE_ITEM)item;
            --row;
        }
    }
    return PAUSE_RESUME;
}

constexpr void pause_reconcile(PauseMenuView& view) {
    if(view.selected < view.first)
        view.first = view.selected;
    else if(view.selected >= view.first + PAUSE_ROWS)
        view.first = view.selected - PAUSE_ROWS + 1;
}

constexpr PauseMenuView pause_open(PAUSE_ITEM initial, bool can_load, bool in_battle,
                                   bool outdoors) {
    PauseMenuView view{};
    for(uint8_t item = 0; item < PAUSE_ITEM_COUNT; ++item) {
        if(!menu_item_visible((PAUSE_ITEM)item, can_load, in_battle, outdoors))
            continue;
        view.visible |= 1u << item;
        if(item == initial)
            view.selected = view.count;
        ++view.count;
    }
    pause_reconcile(view);
    return view;
}

constexpr void pause_move(PauseMenuView& view, bool down) {
    if(down)
        view.selected = view.selected + 1 == view.count ? 0 : view.selected + 1;
    else
        view.selected = view.selected ? view.selected - 1 : view.count - 1;
    pause_reconcile(view);
}

#endif
