#ifndef LOGIC_AREA_CAVE_H
#define LOGIC_AREA_CAVE_H

#include "enemy_spawn.h"
#include "mechanisms.h"
#include "container_rewards.h"
#include "area_ids.h"
#include "tile_ids.h"
#include "quest.h"

// Hollow Mine. The east winch releases the west inspection nook and its bridge plate.

inline constexpr BattleEncounter PROGMEM BATTLE_MINE_SCOUTS =
    battle_encounter(battlefields_for_area(AREA_CAVE), ENEMY_WOLF);

struct MineWolfHooks : EnemyHooks {
    static void victory() {
        game.set_flag(FLAG_MINE_WOLF_DRIVEN);
    }
};

using CaveEnemies = TypeList<
    EnemySpawn<positions::MineScout, &BATTLE_MINE_SCOUTS, 17, ENEMY_CHASE, SPEED_FAST, 0,
               RESPAWN_REMEMBER, MineWolfHooks>
>;

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

template <TilePos POS> void read_bridge_notice(Dialogue<POS>) {
    if(player_just_interacted<POS>() &&
       dialogue_show(F("Bridge crew: first throw the east gate lever. Open the west inspection nook from the winch room.")))
        quest_start(QUEST_SPAN_BELOW);
}

template <TilePos POS> void read_wolf_warning(Dialogue<POS>) {
    if(player_just_interacted<POS>() &&
       dialogue_show(F("Nell's chalk: Wolf on the rails ahead. Its tracks go past the old tally board.")))
        quest_start(QUEST_TRACKS_IN_DUST);
}

template <TilePos POS> void read_mine_ledger(Dialogue<POS>) {
    if(!player_just_interacted<POS>())
        return;
    if(!game.get_flag(FLAG_MINE_WOLF_DRIVEN)) {
        dialogue_show(F("Fresh claw marks score the dust. Something is still guarding this chamber."));
        return;
    }
    if(dialogue_show(F("Two loads came up. Same weight, same ore, same cut in every stone. Both marks are identical.")) &&
       !game.get_flag(FLAG_MINE_LEDGER)) {
        game.set_flag(FLAG_MINE_LEDGER);
        quest_award_experience(3);
    }
}

template <> struct AreaMessages<AREA_CAVE> {
    static constexpr Message TABLE[] = {
        {positions::WinchChalk, F("Chalk over the winch: release the west grille. Step on the plate behind it to run the bridge out.")},
        {positions::VentChalk, F("Wait for the hiss to die. Cross on the dark grate. Same rule on the way back.")},
        {positions::PoolEcho, F("Two tide lines on the same stone. The lower one is wet, but the pool has never reached it today.")},
        {positions::LadderChalk, F("Ladder to the west ridge. Leave a pair of dry boots for the next shift.")},
    };
};

} // namespace handler_impl

#endif
