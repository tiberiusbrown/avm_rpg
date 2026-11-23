#ifndef LOGIC_AREA_VILLAGE_H
#define LOGIC_AREA_VILLAGE_H

#include "enemy_spawn.h"
#include "npc_content.h"
#include "npc_policy.h"
#include "quests/lantern_keeper.h"
#include "tile_ids.h"
#include "mechanisms.h"
#include "area_ids.h"

// Millcross and its houses.

inline constexpr BattleEncounter PROGMEM BATTLE_GROVE_WOLVES =
    battle_encounter(ENEMY_WOLF);
inline constexpr BattleEncounter PROGMEM BATTLE_ORCHARD_SPIDERS =
    battle_encounter(ENEMY_GIANT_SPIDER, ENEMY_GIANT_SPIDER);
inline constexpr BattleEncounter PROGMEM BATTLE_FARM_RATS =
    battle_encounter(battlefields(BATTLEFIELD_MEADOW), ENEMY_GIANT_RAT, ENEMY_GIANT_RAT);
inline constexpr BattleEncounter PROGMEM BATTLE_EAST_BEETLE =
    battle_encounter(battlefields(BATTLEFIELD_GROVE), ENEMY_GIANT_BEETLE);
inline constexpr BattleEncounter PROGMEM BATTLE_RIVER_SNAKE =
    battle_encounter(battlefields(BATTLEFIELD_RIVERBANK), ENEMY_SNAKE);

inline void lantern_inn_rest() {
    if(dialogue_ask(F("Rest at the Lantern Inn?"))) {
        party_rest(game.party);
        dialogue_show(F("The party feels rested."));
    }
}

#ifndef RPG_NPC_POLICY_HEADER
template <> struct NpcPolicy<NPC_TESSA> : AtOneOf<positions::TessaWell> {};
template <> struct NpcPolicy<NPC_ORRIN> : AtOneOf<positions::OrrinLane> {};
template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<positions::RowanHome> {};
template <> struct NpcPolicy<NPC_MARA> : AtOneOf<positions::MaraInn> {};

inline bool bram_meeting_at_lantern() {
    return game.get_flag(FLAG_BRAM_LANTERN_MEETING);
}

template <>
struct NpcPolicy<NPC_BRAM>
    : AtOneOf<positions::BramYard, When<positions::BramLantern, bram_meeting_at_lantern>> {};
template <> struct NpcPolicy<NPC_NELL> : AtOneOf<positions::NellLookout> {};
template <> struct NpcPolicy<NPC_ADA> : AtOneOf<positions::AdaShop> {};
#endif

template <> struct NpcContent<NPC_TESSA> {
    static constexpr auto name = F("Tessa");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 2, 32, 32, 0, 0, 4, 0, 9, true};

    static void talk() {
        constexpr auto id = NPC_TESSA;
        switch(dialogue_options<"Orchard", "The lantern">()) {
            case 0:
                dialogue_show(id, F("I water the grafts by hand now. Bram meant well closing the sluice, but he could have told me first."));
                break;
            case 1:
                if(!game.get_flag(FLAG_LANTERN_RESTORED))
                    dialogue_show(id, F("I used to sort my apples by the light from the well. Have you had a look at that lamp?"));
                else
                    dialogue_show(id, game.get_flag(FLAG_LANTERN_MODERN)
                                          ? F("That's a clean, steady light. Bram does good work when you give him time.")
                                          : F("My grandfather used to count that pulse while he worked. I'd forgotten the sound of it."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_ORRIN> {
    static constexpr auto name = F("Orrin");
    static constexpr NpcDefinition PROGMEM definition{NPC_WANDER, WEST, 2, 48, 48, 16, 12, 3, 0, 2, true};

    static void talk() {
        constexpr auto id = NPC_ORRIN;
        switch(dialogue_options<"Old workshop", "Mine workings", "The sluice">()) {
            case 0:
                if(game.get_flag(FLAG_LANTERN_RESTORED))
                    dialogue_show(id, game.get_flag(FLAG_LANTERN_MODERN)
                                          ? F("A regulator we can make here. Good. We won't have to ask the mine for another.")
                                          : F("Watch the pulse in the well. My mother said there was once a second lamp on the other end of the line."));
                else
                    dialogue_show(id, F("Bram kept a record from the old workshop. Try the table inside his smithy."));
                break;
            case 1:
                dialogue_show(id, game.get_flag(FLAG_SURVEYOR_RESCUED)
                                      ? F("I owe the surveyor tea. They'll want to hear how that hatch used to rattle.")
                                      : F("The east winch room feeds the west inspection nook. That's where they kept the bridge release."));
                break;
            case 2:
                dialogue_show(id, F("I watched the foam climb three stones uphill. Bram did right to shut the race."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_ROWAN> {
    static constexpr auto name = F("Rowan");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, WEST, 0, 32, 32, 0, 0, 4, 0, 3, true};

    static void talk() {
        constexpr auto id = NPC_ROWAN;
        if(!dialogue_show(id, F("I'm Rowan. Need a bow on the road, or just directions?")))
            return;

        switch(dialogue_options<"Join my party", "Millcross", "Hollow Mine", "The old span">()) {
            case 0:
                party_add(party_new_member(id, MOVEMENT_STEP, ATTACK_SHOOT));
                break;
            case 1:
                dialogue_show(id, game.get_flag(FLAG_SURVEYOR_RESCUED)
                                      ? F("The surveyor's taking the sun by the inn. Good to see them above ground.")
                                      : F("Mara keeps a bed ready. Stop at the inn if the road bites back."));
                break;
            case 2:
                dialogue_show(id, game.get_flag(FLAG_LANTERN_RESTORED)
                                      ? F("You can see the well lamp from the north road now. In the mine, watch for wolf tracks by the rails.")
                                      : F("Nell saw wolf tracks by the rails. I'll take the back line if you take point."));
                break;
            case 3:
                dialogue_show(id, game.get_flag(FLAG_CAVE_BRIDGE)
                                      ? F("The old ladder is useful again. It comes out west of the mine trail.")
                                      : F("The bridge fell and the mine closed. Look for the miners' chalk if you go in."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_MARA> {
    static constexpr auto name = F("Mara");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 0, 32, 32, 0, 0, 4, 0, 11, true};

    static void talk() {
        constexpr auto id = NPC_MARA;
        switch(dialogue_options<"Get some rest", "The lantern", "Survey notes">()) {
            case 0:
                lantern_inn_rest();
                break;
            case 1:
                if(game.get_flag(FLAG_LANTERN_RESTORED))
                    dialogue_show(id, game.get_flag(FLAG_LANTERN_MODERN)
                                          ? F("Bram's new light stays steady in the wind. I can see the square from my door again.")
                                          : F("The old light is back. I'd forgotten how it flickers right before the well rings."));
                else if(game.get_flag(FLAG_BRAM_LANTERN_MEETING))
                    dialogue_show(id, F("Bram will be at the well lantern."));
                else if(game.get_flag(FLAG_LANTERN_STARTED))
                    dialogue_show(id, F("Bram works east of the square. He'll know what's wrong with the well lantern."));
                else if(dialogue_ask(id, F("The well lantern went dark. Will you ask Bram to have a look?")))
                    quest_start(QUEST_LANTERN_KEEPER);
                break;
            case 2:
                dialogue_show(id, game.get_flag(FLAG_SURVEYOR_RESCUED)
                                      ? F("The surveyor's outside. I sent them out with a hot mug.")
                                      : F("The surveyor left their notes on the table to the east. Nell was the last to see them."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_BRAM> {
    static constexpr auto name = F("Bram");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 1, 32, 32,  0,
                                                      0,         4,     0, 7,  true};

    static void talk() {
        constexpr auto id = NPC_BRAM;
        switch(dialogue_options<"The lantern", "Old workshop", "Your work">()) {
            case 0:
                if(game.get_flag(FLAG_LANTERN_RESTORED))
                    dialogue_show(id, game.get_flag(FLAG_LANTERN_MODERN)
                                          ? F("I can make another regulator from that pattern. I hope we won't need one.")
                                          : F("The old relay draws its beat from somewhere. I only wired it back in."));
                else if(game.get_flag(FLAG_BRAM_LANTERN_MEETING))
                    dialogue_show(id, F("Have a look at the lantern and we'll choose a repair."));
                else if(Quest<QUEST_LANTERN_KEEPER>::ready()) {
                    if(dialogue_show(id, F("Meet me at the well lantern. We can fit a new part or "
                                           "use the old relay if you have it.")))
                        game.set_flag(FLAG_BRAM_LANTERN_MEETING);
                } else if(dialogue_show(
                              id, F("I opened the well lantern this morning. The regulator is "
                                    "cracked. The old workshop record is inside my smithy.")))
                    game.set_flag(FLAG_LANTERN_DIAGNOSED);
                break;
            case 1:
                dialogue_show(id, F("The record is on my table. The workshop built lamps for both the mine and the square."));
                break;
            case 2:
                dialogue_show(id, F("Hinges, nails, pruning knives. Got something that needs an edge? Set it by the door."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_NELL> {
    static constexpr auto name = F("Nell");
    static constexpr NpcDefinition PROGMEM definition{NPC_WANDER, SOUTH, 2, 40, 44, 10, 8, 3, 0, 12, true};

    static void talk() {
        constexpr auto id = NPC_NELL;
        switch(dialogue_options<"The surveyor", "Mine trail", "The old relay">()) {
            case 0:
                if(game.get_flag(FLAG_SURVEYOR_RESCUED))
                    dialogue_show(id, F("They're back! I want to see the sketch of the landing by the ladder."));
                else if(game.get_flag(FLAG_SURVEY_STARTED))
                    dialogue_show(id, F("They were headed for the far side of Hollow Mine. Their notes are on the inn table."));
                else if(dialogue_ask(id, F("The surveyor went to Hollow Mine and missed supper. Will you look for them?")))
                    quest_start(QUEST_MISSING_SURVEYOR);
                break;
            case 1:
                dialogue_show(id, game.get_flag(FLAG_CAVE_BRIDGE)
                                      ? F("I heard the bridge slam into place. The hatch is on the west ridge if you want a shorter walk.")
                                      : F("Look for the survey mark on the north road. In the mine, the east winch opens the way to the west plate."));
                break;
            case 2:
                dialogue_show(id, F("There's a lamp with a coil in the east winch room. I saw it just past the gate."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_ADA> {
    static constexpr auto name = F("Ada");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 0, 32, 32, 0, 0, 4, 0, 14, true};

    static void talk() {
        constexpr auto id = NPC_ADA;
        switch(dialogue_options<"Browse supplies", "Trade road">()) {
            case 0:
                shop_requested = true;
                break;
            case 1:
                if(game.get_flag(FLAG_SURVEYOR_RESCUED))
                    dialogue_show(id, F("The surveyor bought a fresh notebook. They say the west hatch cuts the climb in half."));
                else if(game.get_flag(FLAG_LANTERN_RESTORED))
                    dialogue_show(id, F("People wait under the well lantern again. Good for the evening trade."));
                else
                    dialogue_show(id, F("Miners used to buy lamp oil by the barrel. These days it's Bram buying files and Nell asking for string."));
                break;
        }
    }
};

using VillageEnemies = TypeList<
    EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 17>
>;
using OrchardEnemies = TypeList<
    EnemySpawn<positions::OrchardSpiders, &BATTLE_ORCHARD_SPIDERS, 19>
>;
using MillcrossWildlife = TypeList<
    EnemySpawn<positions::FarmRats, &BATTLE_FARM_RATS, 16>,
    EnemySpawn<positions::EastBeetle, &BATTLE_EAST_BEETLE, 20>,
    EnemySpawn<positions::RiverSnake, &BATTLE_RIVER_SNAKE, 21>
>;

namespace handler_impl {

template <> struct AreaMessages<AREA_OUTDOOR> {
    static constexpr Message TABLE[] = {
        {positions::WellSign, F("Millcross. Well lantern south of this sign. Inn west, provisions east, Hollow Mine north.")},
        {positions::OrchardSign, F("Orchard lane. Latch the pens behind you. Mind the grafts.")},
        {positions::SmithySign, F("Bram's smithy. Set repairs by the door.")},
        {positions::CaveSign, F("Hollow Mine. Wolf sign on the rails. Save before entering. Watch for steam.")},
        {positions::LadderSign, F("Maintenance hatch. The bridge linkage opens it from inside the mine.")},
        {positions::SluiceStone, F("The stone is worn around one scratched line: Even water knows the road home.")},
    };
};

template <> struct AreaMessages<AREA_INTERIORS> {
    static constexpr Message TABLE[] = {
        {positions::InnRegister, F("Most guests sign under a trade. Someone wrote: Here to listen to the well.")},
    };
};

template <TilePos POS> void village_lantern(Tiles<POS>) {
    if(game.get_flag(FLAG_LANTERN_RESTORED))
        set_tile<POS>(game.get_flag(FLAG_LANTERN_MODERN) ? TILE_VILLAGE_LANTERN_MODERN
                                                       : TILE_VILLAGE_LANTERN_ANCIENT);
}

template <TilePos POS> void village_lantern(Dialogue<POS>) {
    if(!player_just_interacted<POS>())
        return;
    if(game.get_flag(FLAG_LANTERN_RESTORED)) {
        dialogue_show(game.get_flag(FLAG_LANTERN_MODERN) ? F("The new regulator holds the light steady.")
                                                       : F("The old relay pulses in time with a ring from the well."));
        return;
    }

    quest_start(QUEST_LANTERN_KEEPER);
    using Lantern = Quest<QUEST_LANTERN_KEEPER>;
    if(!Lantern::ready()) {
        dialogue_show(F("The lantern is dark. A smith's mark is stamped on its base."));
        return;
    }

    bool has_relay = game.get_flag(FLAG_LANTERN_RELAY_FOUND);
    if(!dialogue_show(NPC_BRAM, has_relay ? F("I can fit a new regulator or hook up that old relay. Which shall it be?")
                                         : F("I can make a new regulator now. We can also wait if you want to look for the old relay.")))
        return;
    uint8_t choice = has_relay
                         ? dialogue_options<"Fit new regulator", "Restore old relay", "Leave it for now">()
                         : dialogue_options<"Fit new regulator", "Leave it for now">();
    if((choice == 0 || (has_relay && choice == 1)) && Lantern::repair(choice == 0))
        dialogue_show(NPC_BRAM, choice == 0 ? F("There. A good steady light, made right here.")
                                           : F("There it goes. Hear the well ring when the light pulses?"));
}

template <TilePos POS> void read_workshop_record(Dialogue<POS>) {
    if(player_just_interacted<POS>()) {
        game.set_flag(FLAG_LANTERN_RECORD_FOUND);
        dialogue_show(F("Workshop record: the square lamp can take a new regulator. Its original relay is stored in the east winch room of Hollow Mine."));
    }
}

template <TilePos POS> void read_survey_notes(Dialogue<POS>) {
    if(player_just_interacted<POS>()) {
        quest_start(QUEST_MISSING_SURVEYOR);
        game.set_flag(FLAG_SURVEY_NOTES_READ);
        dialogue_show(F("Survey notes: follow the rails past the east gate. Crossing control in west inspection nook. Check ladder on far landing."));
    }
}

template <TilePos POS> void read_survey_trail(Dialogue<POS>) {
    if(player_just_interacted<POS>()) {
        game.set_flag(FLAG_SURVEY_TRAIL_FOUND);
        dialogue_show(F("Survey mark: north to the rail entrance. The west ridge hatch is linked to the bridge."));
    }
}

template <TilePos POS> void maintenance_hatch(Tiles<POS>) {
    if(game.get_flag(FLAG_CAVE_BRIDGE))
        set_tile<POS>(TILE_HOLE_LADDER);
}

template <TilePos POS> void maintenance_hatch(Gameplay<POS>) {
    if(game.get_flag(FLAG_CAVE_BRIDGE))
        take_passage(Gameplay<POS>{});
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
    if(player_just_interacted<POS>())
        lantern_inn_rest();
}

} // namespace handler_impl

#endif
