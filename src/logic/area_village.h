#ifndef LOGIC_AREA_VILLAGE_H
#define LOGIC_AREA_VILLAGE_H

#include "enemy_spawn.h"
#include "npc_content.h"
#include "npc_policy.h"
#include "story.h"
#include "tile_ids.h"
#include "mechanisms.h"
#include "area_ids.h"

// The village, the forest clearing west of it, and the village houses.

// The giant rat in Eliza's bedroom is the first enemy of the catalog.
inline constexpr BattleEncounter PROGMEM BATTLE_GIANT_RAT =
    battle_encounter(BATTLEFIELDS_HOUSE, ENEMY_GIANT_RAT);
inline constexpr NPC_ID NPC_GIANT_RAT = (NPC_ID)NPC_ENEMY_FIRST;

#ifndef RPG_NPC_POLICY_HEADER
inline bool eliza_in_village() {
    return game.get_flag(FLAG_RAT_REPORTED);
}

template <>
struct NpcPolicy<NPC_ELIZA>
    : AtOneOf<positions::ElizaAtAlden, When<positions::ElizaGarden, eliza_in_village>> {};

template <> struct NpcPolicy<NPC_BRAM> : AtOneOf<positions::BramPost> {};
template <> struct NpcPolicy<NPC_ALDEN> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BOULDER_A> : AtOneOf<positions::BoulderA> {};

template <> struct NpcPolicy<NPC_BOULDER_B> : AtOneOf<positions::BoulderB> {};

template <> struct NpcPolicy<NPC_BOULDER_C> : AtOneOf<positions::BoulderC> {};

template <> struct NpcPolicy<NPC_BOULDER_D> : AtOneOf<positions::BoulderD> {};
#endif

// The player is a named speaker without a map actor or conversation. The definition's sprite row
// draws the traveler in battle.
template <> struct NpcContent<NPC_ALDEN> {
    static constexpr auto name = F("Alden");
    static constexpr NpcDefinition PROGMEM definition{
        NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 0, 0, 0, false};

    static void talk() {}
};

// Eliza paces a small patch of whichever place she is in.
template <> struct NpcContent<NPC_ELIZA> {
    static constexpr auto name = F("Eliza");
    static constexpr NpcDefinition PROGMEM definition{NPC_WANDER, SOUTH, 2, 64, 32,  24,
                                                      16,         4,     0, 8,  true};

    static void talk() {
        story_talk_eliza();
    }
};

// Bram holds the only way out of the village. He turns to whoever speaks to him.
template <> struct NpcContent<NPC_BRAM> {
    static constexpr auto name = F("Bram");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, WEST, 1, 0, 0,    0,
                                                      0,         4,    0, 7, true, NPC_BODY_FIXED};

    static void talk() {}
};

// A boulder rolls one tile away from the traveler who pushes it.
template <NPC_ID ID, uint8_t IMAGE> struct Boulder {
    static constexpr char const PROGMEM* name = nullptr;
    static constexpr NpcDefinition PROGMEM definition{
        NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 3, 0, IMAGE, true, NPC_BODY_STATIC};

    static void talk() {
        story_push_boulder(ID);
    }
};

template <> struct NpcContent<NPC_BOULDER_A> : Boulder<NPC_BOULDER_A, NPC_STATIC_BOULDER> {};

template <> struct NpcContent<NPC_BOULDER_B> : Boulder<NPC_BOULDER_B, NPC_STATIC_BOULDER_SPLIT> {};

template <> struct NpcContent<NPC_BOULDER_C> : Boulder<NPC_BOULDER_C, NPC_STATIC_BOULDER_SPLIT> {};

template <> struct NpcContent<NPC_BOULDER_D> : Boulder<NPC_BOULDER_D, NPC_STATIC_BOULDER> {};

// The rat appears once the traveler agrees to help. It holds still until the traveler has chosen
// how to fight, then it chases.
struct RatHooks : EnemyHooks {
    static bool available() {
        return game.get_flag(FLAG_RAT_ACCEPTED);
    }
};

inline constexpr EnemySettings RAT_SETTINGS{.facing = WEST,
                                            .wander_x = 0,
                                            .wander_y = 0,
                                            .detect = 10 * TILE,
                                            .disengage = 12 * TILE,
                                            .leash = 12 * TILE};

using VillageEnemies = TypeList<EnemySpawn<positions::RatNest, &BATTLE_GIANT_RAT, 16, ENEMY_WANDER,
                                           SPEED_NORMAL, 0, RESPAWN_NEVER, RatHooks, RAT_SETTINGS>>;

namespace handler_impl {

template <> struct AreaMessages<AREA_OUTDOOR> {
    static constexpr Message TABLE[] = {
        {positions::AldenSign, F("Alden's home.")},
        {positions::ElizaSign, F("Eliza's home.")},
        {positions::BramSign, F("Bram's home.")},
        {positions::ShopSign, F("Shop.")},
        {positions::InnSign, F("Inn.")},
        {positions::NorthHouseSign, F("A house.")},
        {positions::SouthHouseSign, F("A house.")},
        {positions::BarnSign, F("A storehouse.")},
        {positions::BramDoor, F("The door is locked.")},
        {positions::NorthHouseDoor, F("No one answers.")},
        {positions::SouthHouseDoor, F("No one answers.")},
        {positions::BarnDoor, F("No one answers.")},
    };
};

// A house pairs its outdoor door with the exit of its interior, and names the interior's floor.
// All three are map markers.
struct HouseLink {
    TilePos door, exit;
    TileRegion rooms;
};

inline constexpr HouseLink HOUSE_LINKS[] = {
    {positions::AldenDoor, positions::AldenExit, positions::AldenRoomsRegion},
    {positions::ElizaDoor, positions::ElizaExit, positions::ElizaRoomsRegion},
    {positions::ShopDoor, positions::ShopExit, positions::ShopRoomsRegion},
    {positions::InnDoor, positions::InnExit, positions::InnRoomsRegion},
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

} // namespace handler_impl

#endif
