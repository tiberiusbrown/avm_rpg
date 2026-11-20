#ifndef SUPPLIES_UI_H
#define SUPPLIES_UI_H

#include "rpg.h"

enum SUPPLIES_PAGE : uint8_t { SUPPLIES_LIST, SUPPLIES_DETAILS, SUPPLIES_TARGET, SUPPLIES_CONFIRM };

enum SUPPLIES_EVENT : uint8_t {
    SUPPLIES_NONE,
    SUPPLIES_EDGE,
    SUPPLIES_MOVE,
    SUPPLIES_OPEN,
    SUPPLIES_BACK,
    SUPPLIES_CLOSE,
    SUPPLIES_APPLY
};

inline constexpr uint8_t SUPPLIES_WIDTH = 128;
inline constexpr uint8_t SUPPLIES_DETAIL_LINES = 3;
inline constexpr uint8_t SUPPLIES_WRAP_CAPACITY = 8;

struct SuppliesUi {
    SUPPLIES_PAGE page{};
    SUPPLY_ID supply{};
    uint8_t target{};
    uint8_t scroll{};
    uint8_t lines{};
    uint8_t starts[SUPPLIES_WRAP_CAPACITY + 1];
    char const PROGMEM* feedback{};
};

SUPPLIES_EVENT supplies_press(SuppliesUi& ui, Supplies const& supplies, Party const& party,
                              uint8_t pressed);
char const PROGMEM* supply_name(SUPPLY_ID id);
char const PROGMEM* supply_description(SUPPLY_ID id);
void supplies_layout(SuppliesUi& ui);
void render_supplies(uint8_t revealed, bool closing, SuppliesUi const& ui);

#endif
