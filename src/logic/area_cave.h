#ifndef LOGIC_AREA_CAVE_H
#define LOGIC_AREA_CAVE_H

#include "mechanisms.h"
#include "area_ids.h"
#include "tile_ids.h"

// The cave: a lever opens the vault gate, the vault's lever opens the nook, and the nook's plate
// runs the bridge out over the chasm. Steam vents blow across the way, and a message explains it.

namespace handler_impl {

template <> struct AreaMechanisms<AREA_CAVE> {
    static constexpr Mechanism TABLE[] = {
        {one_cell(positions::CaveLever), FLAG_CAVE_GATE, CAVE_TILE_SWITCH_RIGHT},
        {one_cell(positions::CaveGate), FLAG_CAVE_GATE, CAVE_TILE_GATE_FRONT_OPEN},
        {one_cell(positions::CaveVaultLever), FLAG_CAVE_NOOK, CAVE_TILE_SWITCH_RIGHT},
        {one_cell(positions::CaveNookGate), FLAG_CAVE_NOOK, CAVE_TILE_GATE_SIDE_OPEN},
        {one_cell(positions::CavePlate), FLAG_CAVE_BRIDGE, CAVE_TILE_PRESSURE_PLATE_DOWN},
        {positions::CaveBridgeRegion, FLAG_CAVE_BRIDGE, CAVE_TILE_BRIDGE_VERTICAL},
        {one_cell(positions::CaveChest), FLAG_CAVE_CHEST, CAVE_TILE_CHEST_OPEN},
    };
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

template <TilePos POS> void steam_vent(Tiles<POS>) {
    if(vent_blows(game.ticks))
        set_tile<POS>(CAVE_TILE_VENT_ACTIVE);
}

template <> struct AreaMessages<AREA_CAVE> {
    static constexpr Message TABLE[] = {
        {positions::CaveNotice,
         F("Levers work the gates. The old plate in the west nook runs the bridge.")},
    };
};

} // namespace handler_impl

#endif
