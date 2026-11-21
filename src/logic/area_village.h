#ifndef LOGIC_AREA_VILLAGE_H
#define LOGIC_AREA_VILLAGE_H

#include "mechanisms.h"
#include "area_ids.h"

// The village: the Grove's signs and the houses with their interiors.

namespace handler_impl {

// The sign outside the mine is a message of the outdoor area, so it is listed here with the
// village's own markers.
template <> struct AreaMessages<AREA_OUTDOOR> {
    static constexpr Message TABLE[] = {
        {positions::CaveSign, F("Hollow Mine. Closed since the bridge fell. Mind the steam.")},
    };
};

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
    if(player_just_interacted<POS>()) {
        // The door is heard while the world fades out.
        sfx_play(SFX_DOOR_OPEN);
        teleport_player<house_inside(POS)>(NORTH);
    }
}

// Walking onto the exit leaves the house. No button is needed.
template <TilePos POS> void leave_house(Gameplay<POS>) {
    if(player_is_walking_onto<POS>()) {
        sfx_play(SFX_DOOR_CLOSE);
        teleport_player<house_front(POS)>(SOUTH);
    }
}

template <TilePos POS> void open_shop(Dialogue<POS>) {
    if(player_just_interacted<POS>())
        shop_requested = true;
}

template <TilePos POS> void rest_at_inn(Dialogue<POS>) {
    if(player_just_interacted<POS>() && dialogue_ask(F("Rest at the inn?"))) {
        party_rest(game.party);
        dialogue_show(F("The party feels rested."));
    }
}

} // namespace handler_impl

#endif
