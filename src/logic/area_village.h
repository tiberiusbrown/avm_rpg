#ifndef LOGIC_AREA_VILLAGE_H
#define LOGIC_AREA_VILLAGE_H

#include "enemy_spawn.h"
#include "npc_content.h"
#include "npc_policy.h"
#include "quest.h"
#include "mechanisms.h"
#include "area_ids.h"

// The village: the Grove's signs and the houses with their interiors.

inline constexpr BattleEncounter PROGMEM BATTLE_GROVE_WOLVES =
    battle_encounter(ENEMY_WOLF);

#ifndef RPG_NPC_POLICY_HEADER
template <>
struct NpcPolicy<NPC_VILLAGER>
    : AtOneOf<positions::VillagerHome,
              When<positions::VillagerEast,
                   [] {
                       return game.get_flag(FLAG_VILLAGER_MOVED) &&
                              game.get_flag(FLAG_VILLAGER_EAST);
                   }>,
              When<positions::VillagerWest, [] {
                  return game.get_flag(FLAG_VILLAGER_MOVED) && !game.get_flag(FLAG_VILLAGER_EAST);
              }>> {};

template <> struct NpcPolicy<NPC_WANDERER> : AtOneOf<positions::WandererHome> {};

template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<positions::RowanHome> {};
#endif

template <> struct NpcContent<NPC_VILLAGER> {
    static constexpr auto name = F("Iona");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 2, 32, 32, 0, 0, 3, 0, 9, true};

    static void talk() {
        NPC_ID id = NPC_VILLAGER;
        if(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE)
            dialogue_show(id, F("Silas is worried about me? Tell him I'm fine. I just needed some time away from the village."));
        else
            dialogue_show(id,
                F("Evening, traveler. The lanterns stay lit all night in Nocturne Grove. I hope you decide to stay here. It's a quiet place, and the trees are kind to those who respect them."));
        // Talking sends the villager to the other end of the clearing, named by map markers.
        if(game.get_flag(FLAG_VILLAGER_MOVED))
            game.toggle_flag(FLAG_VILLAGER_EAST);
        else
            game.set_flag(FLAG_VILLAGER_EAST);
        game.set_flag(FLAG_VILLAGER_MOVED);
        quest_emit(NpcTalked<NPC_VILLAGER>{});
    }
};

template <> struct NpcContent<NPC_WANDERER> {
    static constexpr auto name = F("Silas");
    static constexpr NpcDefinition PROGMEM definition{NPC_WANDER, WEST, 2, 48, 48, 16, 12, 2, 0, 2, true};

    static void talk() {
        NPC_ID id = NPC_WANDERER;
        switch(quest_status(QUEST_FIND_IONA)) {
            case QUEST_NOT_STARTED:
                dialogue_show(id, F("I walk this path every night. Mind the roots near the old oak."));
                if(dialogue_ask(id, F("Have you seen Iona? She's been wandering around the grove. Could you check on her?")))
                    quest_start(QUEST_FIND_IONA);
                else
                    dialogue_show(id, F("Another time, then. I'll still be on this path."));
                break;
            case QUEST_ACTIVE:
                dialogue_show(id, F("Let me know when you've found Iona."));
                break;
            case QUEST_COMPLETED:
                dialogue_show(id, F("Good to hear Iona's safe. Thank you."));
                break;
            default:
                dialogue_show(id, F("I walk this path every night. Mind the roots near the old oak."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_ROWAN> {
    static constexpr auto name = F("Rowan");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, WEST, 0, 32, 32, 0, 0, 3, 0, 3, true};

    static void talk() {
        if(!dialogue_show(NPC_ROWAN, F("I'm Rowan. Need a bow on the road, or just directions?")))
            return;

        switch(dialogue_options<"Join my party", "Nocturne Grove", "Hollow Mine",
                                "The fallen bridge">()) {
            case 0:
                party_add(party_new_member(NPC_ROWAN, MOVEMENT_STEP, ATTACK_SHOOT));
                break;
            case 1:
                dialogue_show(NPC_ROWAN, F("You'll know the village by its lanterns. They stay lit all night."));
                break;
            case 2:
                dialogue_show(NPC_ROWAN, F("The notice by Hollow Mine says to mind the steam. I'd take it seriously."));
                break;
            case 3:
                dialogue_show(NPC_ROWAN, F("The bridge fell and the mine closed. Check the sign before heading in."));
                break;
        }
    }
};

using VillageEnemies = TypeList<
    EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 17>
>;

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
