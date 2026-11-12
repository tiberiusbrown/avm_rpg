#ifndef LOGIC_HANDLERS_H
#define LOGIC_HANDLERS_H

#include "handler_defaults.h"
#include "battle.h"

namespace handler_impl {

// The guardians fight once the welcome closes. Each interaction starts a fresh battle, and the
// outcome decides the follow-up line.
template <TilePos POS> void read_sign(Dialogue<POS>) {
    if(player_just_interacted<POS>()) {
        dialogue_show(F("Welcome to Nocturne Grove. Take a quiet walk among the trees."));
        if(battle_show(&BATTLE_GROVE) == BATTLE_VICTORY)
            dialogue_show(F("The guardians bow and withdraw into the trees."));
        else
            dialogue_show(F("The guardians drive you back. Rest, then try again."));
    }
}

// A house pairs its outdoor door with the exit of its interior. Both are map markers.
struct HouseLink {
    TilePos door, exit;
};

inline constexpr HouseLink HOUSE_LINKS[] = {
    {positions::ElderDoor, positions::ElderExit},     {positions::CottageDoor, positions::CottageExit},
    {positions::WeaverDoor, positions::WeaverExit},   {positions::ShopDoor, positions::ShopExit},
    {positions::BakeryDoor, positions::BakeryExit},   {positions::FarmDoor, positions::FarmExit},
    {positions::InnDoor, positions::InnExit},         {positions::SmithyDoor, positions::SmithyExit},
    {positions::HutDoor, positions::HutExit},         {positions::FisherDoor, positions::FisherExit},
};

// Declared and never defined. A door or exit that is missing from HOUSE_LINKS reaches this
// call during constant evaluation, which fails to compile.
void house_link_missing();

// The floor cell just inside the exit that belongs to this door.
consteval TilePos house_inside(TilePos door) {
    for(auto const& link : HOUSE_LINKS)
        if(link.door == door)
            return {link.exit.area, link.exit.x, (uint8_t)(link.exit.y - 1)};
    house_link_missing();
    return door;
}

// The ground cell in front of the door that belongs to this exit.
consteval TilePos house_front(TilePos exit) {
    for(auto const& link : HOUSE_LINKS)
        if(link.exit == exit)
            return {link.door.area, link.door.x, (uint8_t)(link.door.y + 1)};
    house_link_missing();
    return exit;
}

// Facing a door and pressing A enters the house.
template <TilePos POS> void enter_house(Gameplay<POS>) {
    if(player_just_interacted<POS>())
        teleport_player<house_inside(POS)>(NORTH);
}

// Walking onto the exit leaves the house. No button is needed.
template <TilePos POS> void leave_house(Gameplay<POS>) {
    if(player_is_walking_onto<POS>())
        teleport_player<house_front(POS)>(SOUTH);
}

} // namespace handler_impl

#endif
