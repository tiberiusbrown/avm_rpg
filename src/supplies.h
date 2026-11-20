#ifndef SUPPLIES_H
#define SUPPLIES_H

#include <stdint.h>

enum SUPPLY_ID : uint8_t {
    SUPPLY_POTION,
    SUPPLY_ELIXIR,
    SUPPLY_REVIVE,
    SUPPLY_ETHER,
    SUPPLY_COUNT
};

inline constexpr uint8_t SUPPLY_LIMIT = 15;
inline constexpr uint8_t SUPPLY_NO_TARGET = 255;

struct Supplies {
    uint8_t quantity[SUPPLY_COUNT];
};

inline constexpr Supplies STARTING_SUPPLIES{{2, 1, 1, 1}};
static_assert(sizeof(Supplies) == 4);

struct Party;

constexpr bool supply_valid(SUPPLY_ID id) {
    return id < SUPPLY_COUNT;
}

uint8_t supply_count(Supplies const& supplies, SUPPLY_ID id);
bool supplies_can_add(Supplies const& supplies, SUPPLY_ID id, uint8_t amount);
bool supplies_add(Supplies& supplies, SUPPLY_ID id, uint8_t amount);
bool supply_can_use(SUPPLY_ID id, Party const& party, uint8_t target);
bool supply_use(Supplies& supplies, Party& party, SUPPLY_ID id, uint8_t target);
void party_rest(Party& party);

#endif
