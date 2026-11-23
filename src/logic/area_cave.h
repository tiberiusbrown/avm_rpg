#ifndef LOGIC_AREA_CAVE_H
#define LOGIC_AREA_CAVE_H

#include "enemy_spawn.h"
#include "mechanisms.h"
#include "container_rewards.h"
#include "area_ids.h"
#include "tile_ids.h"
#include "npc_content.h"
#include "npc_policy.h"
#include "quests/missing_surveyor.h"

// Hollow Mine. The east winch releases the west inspection nook and its bridge plate.

inline constexpr BattleEncounter PROGMEM BATTLE_MINE_SCOUTS =
    battle_encounter(battlefields_for_area(AREA_CAVE), ENEMY_WOLF);
inline constexpr BattleEncounter PROGMEM BATTLE_MINE_BAT =
    battle_encounter(battlefields_for_area(AREA_CAVE), ENEMY_GIANT_BAT);

using CaveEnemies = TypeList<
    EnemySpawn<positions::MineScout, &BATTLE_MINE_SCOUTS, 17, ENEMY_CHASE, SPEED_FAST, 0,
               RESPAWN_REMEMBER>
>;
using MineWestEnemies = TypeList<
    EnemySpawn<positions::MineBatRoost, &BATTLE_MINE_BAT, 18>
>;

#ifndef RPG_NPC_POLICY_HEADER
inline bool surveyor_rescued() { return game.get_flag(FLAG_SURVEYOR_RESCUED); }

template <> struct NpcPolicy<NPC_SURVEYOR>
    : AtOneOf<positions::SurveyorTrapped, When<positions::SurveyorReturned, surveyor_rescued>> {};
#endif

template <> struct NpcContent<NPC_SURVEYOR> {
    static constexpr auto name = F("Surveyor");
    static constexpr NpcDefinition PROGMEM definition{NPC_STAND, SOUTH, 0, 32, 32, 0, 0, 5, 0, 10, true};

    static void talk() {
        constexpr auto id = NPC_SURVEYOR;
        if(game.get_flag(FLAG_SURVEYOR_RESCUED)) {
            switch(dialogue_options<"Mine routes", "The old relay">()) {
                case 0:
                    dialogue_show(id, F("The ridge hatch skips the rail gallery. I marked it on the new survey."));
                    break;
                case 1:
                    dialogue_show(id, F("The coil in the east winch room is stamped with the same mark as the well lantern."));
                    break;
            }
            return;
        }

        quest_start(QUEST_MISSING_SURVEYOR);
        game.set_flag(FLAG_SURVEYOR_FOUND);
        if(!game.get_flag(FLAG_CAVE_BRIDGE)) {
            dialogue_show(id, F("I can't get back across. The west inspection plate should release the bridge."));
            return;
        }
        if(dialogue_ask(id, F("I thought I'd have to spend another night here. Can you show me the way to the hatch?")) &&
           Quest<QUEST_MISSING_SURVEYOR>::rescue()) {
            dialogue_show(id, F("I see the ladder now. I'll meet you by the inn. Thank you."));
            npc_leave_scene(id);
        }
    }
};

namespace handler_impl {

template <> struct AreaMechanisms<AREA_CAVE> {
    static constexpr Mechanism TABLE[] = {
        {MECHANISM_LEVER, one_cell(positions::CaveLever), FLAG_CAVE_GATE, CAVE_TILE_SWITCH_RIGHT},
        {MECHANISM_GATE, one_cell(positions::CaveGate), FLAG_CAVE_GATE, CAVE_TILE_GATE_FRONT_OPEN},
        {MECHANISM_LEVER, one_cell(positions::CaveVaultLever), FLAG_CAVE_NOOK,
         CAVE_TILE_SWITCH_RIGHT},
        {MECHANISM_GATE, one_cell(positions::CaveNookGate), FLAG_CAVE_NOOK,
         CAVE_TILE_GATE_SIDE_OPEN},
        {MECHANISM_PLATE, one_cell(positions::CavePlate), FLAG_CAVE_BRIDGE,
         CAVE_TILE_PRESSURE_PLATE_DOWN},
        {MECHANISM_BRIDGE, positions::CaveBridgeRegion, FLAG_CAVE_BRIDGE,
         CAVE_TILE_BRIDGE_VERTICAL},
        {MECHANISM_CHEST, one_cell(positions::CaveChest), FLAG_CAVE_CHEST, CAVE_TILE_CHEST_OPEN},
    };
};

template <>
struct ChestReward<positions::CaveChest> : ContainerReward<25, {ITEM_TRAVELERS_BOOTS}, {SUPPLY_POTION, 1}, {SUPPLY_ETHER, 1}> {
    static constexpr auto MESSAGE = F("You find 25 gold, a Potion, an Ether, and Traveler's Boots.");
};

// The mouth and the ladder hole lead outdoors, each to its own doorway in the cave.
template <> struct AreaPassages<AREA_CAVE> {
    static constexpr Passage TABLE[] = {
        {positions::CaveMouth, positions::CaveExit, -1},
        {positions::CaveHole, positions::CaveLadder, 1},
    };
};

// A steam vent blows for VENT_BLOW_TICKS of every 256 ticks, on the game clock alone, so it keeps
// no state. While it blows it is solid, and the player waits for it to rest.
inline constexpr uint8_t VENT_BLOW_TICKS = 96;

constexpr bool vent_blows(uint16_t ticks) {
    return (uint8_t)ticks < VENT_BLOW_TICKS;
}

// The vent hisses as it starts to blow, which is the warning that it is about to bar the way. Every
// vent within hearing starts on the same tick, and the first hiss is the only one played.
template <TilePos POS> void steam_vent(Tiles<POS>) {
    if(vent_blows(game.ticks)) {
        set_tile<POS>(CAVE_TILE_VENT_ACTIVE);
        if((uint8_t)game.ticks == 0)
            sfx_ambient(SFX_STEAM_HISS);
    }
}

template <TilePos POS> void take_lantern_relay(Dialogue<POS>) {
    if(!player_just_interacted<POS>())
        return;
    if(game.get_flag(FLAG_LANTERN_RELAY_FOUND)) {
        dialogue_show(F("The lamp bracket still bears the workshop mark."));
        return;
    }
    game.set_flag(FLAG_LANTERN_RELAY_FOUND);
    dialogue_show(F("You free an old copper relay from the lamp bracket. Its mark matches the well lantern."));
}

template <> struct AreaMessages<AREA_CAVE> {
    static constexpr Message TABLE[] = {
        {positions::CaveNotice, F("Bridge crew: throw the east gate lever first. Open the west inspection nook from the winch room.")},
        {positions::MineWarning, F("Nell's chalk: wolf on the rails ahead. Watch the corners.")},
        {positions::MineLedger, F("Two loads came up. Same weight, same ore. Both entries carry the same unusual mark.")},
        {positions::WinchChalk, F("Chalk over the winch: release the west grille. Step on the plate behind it to run the bridge out.")},
        {positions::VentChalk, F("Wait for the hiss to die. Cross on the dark grate. Same rule on the way back.")},
        {positions::PoolEcho, F("Two tide lines on the same stone. The lower one is wet, but the pool has never reached it today.")},
        {positions::LadderChalk, F("Ladder to the west ridge. Leave a pair of dry boots for the next shift.")},
    };
};

} // namespace handler_impl

#endif
