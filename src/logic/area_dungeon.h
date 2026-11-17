#ifndef LOGIC_AREA_DUNGEON_H
#define LOGIC_AREA_DUNGEON_H

#include "mechanisms.h"
#include "area_ids.h"
#include "tile_ids.h"

// The dungeon: the hall lever lowers the spikes, the cell's plate opens the east gate, the east
// wing's lever sinks the blocks, and the orb opens the vault.

namespace handler_impl {

template <> struct AreaMechanisms<AREA_DUNGEON> {
    static constexpr Mechanism TABLE[] = {
        {one_cell(positions::DungeonLever), FLAG_DUNGEON_SPIKES, DUNGEON_TILE_SWITCH_RIGHT},
        {positions::DungeonSpikesRegion, FLAG_DUNGEON_SPIKES, DUNGEON_TILE_SPIKES_DOWN},
        {one_cell(positions::DungeonCellDoor), FLAG_DUNGEON_CELL, DUNGEON_TILE_BARS_DOOR_OPEN},
        {one_cell(positions::DungeonPlate), FLAG_DUNGEON_GATE, DUNGEON_TILE_PRESSURE_PLATE_DOWN},
        {one_cell(positions::DungeonGate), FLAG_DUNGEON_GATE, DUNGEON_TILE_GATE_SIDE_OPEN},
        {one_cell(positions::DungeonBlockLever), FLAG_DUNGEON_BLOCKS, DUNGEON_TILE_SWITCH_RIGHT},
        {positions::DungeonBlocksRegion, FLAG_DUNGEON_BLOCKS, DUNGEON_TILE_BLOCK_LOWERED},
        {one_cell(positions::DungeonOrb), FLAG_DUNGEON_ORB, DUNGEON_TILE_ORB_LIT},
        {one_cell(positions::DungeonRuneWest), FLAG_DUNGEON_ORB, DUNGEON_TILE_RUNE_LIT},
        {one_cell(positions::DungeonRuneEast), FLAG_DUNGEON_ORB, DUNGEON_TILE_RUNE_LIT},
        {one_cell(positions::DungeonVaultDoor), FLAG_DUNGEON_ORB, DUNGEON_TILE_ARCHWAY},
        {one_cell(positions::DungeonChest), FLAG_DUNGEON_CHEST, DUNGEON_TILE_CHEST_OPEN},
    };
};

// The mine door outdoors leads to the doorway at the south end of the dungeon.
template <> struct AreaPassages<AREA_DUNGEON> {
    static constexpr Passage TABLE[] = {
        {positions::MineEntrance, positions::DungeonExit, -1},
    };
};

// The orb lights at a touch and stays lit. Its flag also lights the runes and opens the vault.
template <TilePos POS> void light_orb(Dialogue<POS>) {
    constexpr Mechanism entry = mechanism<POS>();
    if(!player_just_interacted<POS>())
        return;
    if(game.get_flag(entry.flag)) {
        dialogue_show(F("The orb glows steadily."));
        return;
    }
    game.set_flag(entry.flag);
    set_tile<POS>(entry.tile);
    dialogue_show(F("The orb wakes at your touch. Stone grinds behind the north wall."));
}

template <TilePos POS> void light_orb(Tiles<POS>) {
    apply_state<POS>();
}

template <> struct AreaMessages<AREA_DUNGEON> {
    static constexpr Message TABLE[] = {
        {positions::DungeonPlaque, F("Four seals guard the vault: spike, bar, stone and light.")},
    };
};

} // namespace handler_impl

#endif
