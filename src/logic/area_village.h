#ifndef LOGIC_AREA_VILLAGE_H
#define LOGIC_AREA_VILLAGE_H

#include "enemy_spawn.h"
#include "npc_content.h"
#include "npc_policy.h"
#include "quests/dry_roots.h"
#include "mechanisms.h"
#include "area_ids.h"

// Millcross and its houses.

inline constexpr BattleEncounter PROGMEM BATTLE_GROVE_WOLVES =
    battle_encounter(ENEMY_WOLF);

inline void lantern_inn_rest() {
    if(dialogue_ask(F("Rest at the Lantern Inn?"))) {
        party_rest(game.party);
        dialogue_show(F("The party feels rested."));
    }
}

#ifndef RPG_NPC_POLICY_HEADER
template <> struct NpcPolicy<NPC_TESSA>
    : AtOneOf<positions::TessaWell,
              When<positions::TessaOrchard, [] { return game.get_flag(FLAG_ORCHARD_SETTLED); }>> {};

template <> struct NpcPolicy<NPC_ORRIN> : AtOneOf<positions::OrrinLane> {};
template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<positions::RowanHome> {};
template <> struct NpcPolicy<NPC_MARA> : AtOneOf<positions::MaraInn> {};
template <> struct NpcPolicy<NPC_BRAM> : AtOneOf<positions::BramYard> {};
template <> struct NpcPolicy<NPC_NELL> : AtOneOf<positions::NellLookout> {};
template <> struct NpcPolicy<NPC_ADA> : AtOneOf<positions::AdaShop> {};
#endif

template <> struct NpcContent<NPC_TESSA> {
    static constexpr auto name = F("Tessa");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 2, 32, 32, 0, 0, 4, 0, 9, true};

    static void talk() {
        constexpr auto id = NPC_TESSA;
        switch(quest_status(QUEST_DRY_ROOTS)) {
            case QUEST_NOT_STARTED:
                if(dialogue_ask(id, F("Bram shut my sluice and my trees are drying out. Will you ask him and Orrin what happened?")))
                    quest_start(QUEST_DRY_ROOTS);
                return;
            case QUEST_COMPLETED:
                if(game.get_flag(FLAG_ORCHARD_PUBLIC))
                    dialogue_show(id, F("Bram told the whole square about the water. No more whispers. Come see the new grafts."));
                else
                    dialogue_show(id, F("Bram came by after supper. We agreed to watch the sluice together. The trees can wait a little."));
                return;
            default:
                break;
        }

        if(!Quest<QUEST_DRY_ROOTS>::ready()) {
            dialogue_show(id, F("Ask Bram in his yard east of here. Orrin walks by the north lane. He's seen every trick that sluice has."));
            return;
        }
        if(!dialogue_show(id, F("Water ran uphill? If Orrin saw it too, I believe you. Do we talk at the well or give Bram a quiet word?")))
            return;

        uint8_t choice = dialogue_options<"Tell the square", "Speak in private">();
        if(choice > 1)
            return;
        if(choice == 0)
            game.set_flag(FLAG_ORCHARD_PUBLIC);
        quest_emit(NpcTalked<id>{});
        dialogue_show(id, choice == 0 ? F("I'll ask him to tell the square. I'll be out in the orchard after.")
                                      : F("I'll talk to him after supper. For now, my trees need me."));
    }
};

template <> struct NpcContent<NPC_ORRIN> {
    static constexpr auto name = F("Orrin");
    static constexpr NpcDefinition PROGMEM definition{NPC_WANDER, WEST, 2, 48, 48, 16, 12, 3, 0, 2, true};

    static void talk() {
        constexpr auto id = NPC_ORRIN;
        switch(dialogue_options<"The sluice", "Old work", "Bram">()) {
            case 0:
                if(dialogue_show(id, F("Water climbed the race. I watched the foam pass three stones going uphill. Bram did right to shut it.")))
                    game.set_flag(FLAG_ORRIN_ACCOUNT);
                break;
            case 1:
                dialogue_show(id, F("My mother said old things remember their work. A gate wants to open. A wolf wants its den back."));
                break;
            case 2:
                dialogue_show(id, game.get_flag(FLAG_ORCHARD_SETTLED)
                                      ? F("Bram and Tessa can share a bench again. That's a start.")
                                      : F("He can fix any hinge in Millcross. Ask him a straight question and he goes all stiff."));
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
                dialogue_show(id, F("Mara keeps a bed ready. Save before heading north and stop at the inn if the road bites back."));
                break;
            case 2:
                dialogue_show(id, F("Nell saw wolf tracks by the rails. I'll take the back line if you take point."));
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
        switch(dialogue_options<"Get some rest", "Around town", "The lantern">()) {
            case 0:
                lantern_inn_rest();
                break;
            case 1:
                if(game.get_flag(FLAG_ORCHARD_SETTLED))
                    dialogue_show(id, F("Tessa has gone back to her trees. She left an apple on Bram's bench."));
                else
                    dialogue_show(id, F("Tessa's been at the well all morning. Bram walked right by her and counted his nails."));
                break;
            case 2:
                dialogue_show(id, F("I leave a lamp in the window for Nell. She brings home stones that keep shining after dark."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_BRAM> {
    static constexpr auto name = F("Bram");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 0, 32, 32, 0, 0, 4, 0, 7, true};

    static void talk() {
        constexpr auto id = NPC_BRAM;
        switch(dialogue_options<"The sluice", "Your work">()) {
            case 0:
                if(game.get_flag(FLAG_ORCHARD_SETTLED)) {
                    dialogue_show(id, game.get_flag(FLAG_ORCHARD_PUBLIC)
                                          ? F("I owed the square an explanation. Tessa stood beside me while I gave it.")
                                          : F("Tessa heard me out. We're watching the race together before I touch that gate again."));
                } else if(dialogue_show(id, F("I closed it. The stream ran backward and the gear shook in my hands. Ask Orrin. He was there."))) {
                    game.set_flag(FLAG_BRAM_ACCOUNT);
                }
                break;
            case 1:
                dialogue_show(id, F("Hinges, nails, pruning knives. Ada knows what a good edge costs. Here for a repair? Set it by the door."));
                break;
        }
    }
};

template <> struct NpcContent<NPC_NELL> {
    static constexpr auto name = F("Nell");
    static constexpr NpcDefinition PROGMEM definition{NPC_WANDER, SOUTH, 2, 40, 44, 10, 8, 3, 0, 12, true};

    static void talk() {
        constexpr auto id = NPC_NELL;
        switch(dialogue_options<"Wolf tracks", "Mine bridge", "Your finds">()) {
            case 0:
                dialogue_show(id, game.get_flag(FLAG_MINE_LEDGER)
                                      ? F("Two loads with one mark? I've seen that mark on a stone above the eastern trail.")
                                      : F("One wolf, fresh tracks, right on the mine rails. I left a warning at the camp. Bring Rowan if you go."));
                break;
            case 1:
                dialogue_show(id, game.get_flag(FLAG_CAVE_BRIDGE)
                                      ? F("I heard the bridge slam into place from the ridge. It still knows how to do that.")
                                      : F("The miners drew arrows in chalk. There's a plate in a nook west of the rails."));
                break;
            case 2:
                dialogue_show(id, F("Mara calls them pocket gravel. See this one? It only shines with the lamp out."));
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
                dialogue_show(id, F("Miners used to buy lamp oil by the barrel. These days it's Bram buying files and Nell asking for string."));
                break;
        }
    }
};

using VillageEnemies = TypeList<
    EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 17>
>;

namespace handler_impl {

template <> struct AreaMessages<AREA_OUTDOOR> {
    static constexpr Message TABLE[] = {
        {positions::WellSign, F("Millcross. Lantern Inn west, provisions east, Hollow Mine north.")},
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
