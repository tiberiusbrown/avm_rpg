#ifndef LOGIC_HANDLERS_H
#define LOGIC_HANDLERS_H

#include "handler_defaults.h"
#include "battle.h"
#include "tile_ids.h"

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

// A passage joins an entrance outdoors to a doorway of an interior map. Both are map markers, and
// the player takes either end by walking onto it. `inside_step` tells where the player arrives
// inside: -1 for an exit in a south wall, where the floor is the cell above it, and 1 for a doorway
// in the wall row, where the floor is the cell below it. Outdoors the player arrives on the cell
// below the entrance.
struct Passage {
    TilePos outside, inside;
    int8_t inside_step;
};

inline constexpr Passage PASSAGES[] = {
    {positions::CaveMouth, positions::CaveExit, -1},
    {positions::CaveHole, positions::CaveLadder, 1},
    {positions::MineEntrance, positions::DungeonExit, -1},
};

// Declared and never defined. An entrance or doorway that is missing from PASSAGES reaches this
// call during constant evaluation, which fails to compile.
void passage_missing();

// Where the player arrives after walking onto this end of a passage.
consteval TilePos passage_arrival(TilePos end) {
    for(auto const& passage : PASSAGES) {
        if(passage.outside == end)
            return {passage.inside.area, passage.inside.x,
                    (uint8_t)(passage.inside.y + passage.inside_step)};
        if(passage.inside == end)
            return {passage.outside.area, passage.outside.x, (uint8_t)(passage.outside.y + 1)};
    }
    passage_missing();
    return end;
}

// The player arrives walking away from the end that they come out of.
consteval DIRECTION passage_facing(TilePos end) {
    for(auto const& passage : PASSAGES)
        if(passage.outside == end && passage.inside_step < 0)
            return NORTH;
    return SOUTH;
}

template <TilePos POS> void take_passage(Gameplay<POS>) {
    if(player_is_walking_onto<POS>())
        teleport_player<passage_arrival(POS)>(passage_facing(POS));
}

// A mechanism is every cell that one world flag changes. The map shows each cell as it is while
// the flag is clear, and the cell shows `tile` while the flag is set. The cache is rebuilt from the
// map when it reloads, so the tile pass applies the set state on every frame.
struct Mechanism {
    TileRegion cells;
    FLAG flag;
    uint8_t tile;
};

constexpr TileRegion one_cell(TilePos pos) {
    return {pos, 1, 1};
}

inline constexpr Mechanism MECHANISMS[] = {
    // The cave: a lever opens the vault gate, the vault's lever opens the nook, and the nook's
    // plate runs the bridge out over the chasm.
    {one_cell(positions::CaveLever), FLAG_CAVE_GATE, CAVE_TILE_SWITCH_RIGHT},
    {one_cell(positions::CaveGate), FLAG_CAVE_GATE, CAVE_TILE_GATE_FRONT_OPEN},
    {one_cell(positions::CaveVaultLever), FLAG_CAVE_NOOK, CAVE_TILE_SWITCH_RIGHT},
    {one_cell(positions::CaveNookGate), FLAG_CAVE_NOOK, CAVE_TILE_GATE_SIDE_OPEN},
    {one_cell(positions::CavePlate), FLAG_CAVE_BRIDGE, CAVE_TILE_PRESSURE_PLATE_DOWN},
    {positions::CaveBridgeRegion, FLAG_CAVE_BRIDGE, CAVE_TILE_BRIDGE_VERTICAL},
    {one_cell(positions::CaveChest), FLAG_CAVE_CHEST, CAVE_TILE_CHEST_OPEN},
    // The dungeon: the hall lever lowers the spikes, the cell's plate opens the east gate, the
    // east wing's lever sinks the blocks, and the orb opens the vault.
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

// Declared and never defined. A cell that is missing from MECHANISMS fails to compile.
void mechanism_missing();

consteval Mechanism mechanism(TilePos cell) {
    for(auto const& entry : MECHANISMS)
        if(entry.cells.contains(cell))
            return entry;
    mechanism_missing();
    return MECHANISMS[0];
}

// Shows the cell's set state. Every handler of a mechanism cell runs this in the tile pass.
template <TilePos POS> void apply_state() {
    constexpr Mechanism entry = mechanism(POS);
    if(game.get_flag(entry.flag))
        set_tile<POS>(entry.tile);
}

// A gate, a bridge, spikes, blocks or any other cell that only follows its flag.
template <TilePos POS> void show_state(Tiles<POS>) {
    apply_state<POS>();
}

// Facing a lever and pressing A throws it, and pressing A again throws it back.
template <TilePos POS> void use_lever(Gameplay<POS>) {
    if(player_just_interacted<POS>())
        game.toggle_flag(mechanism(POS).flag);
}

template <TilePos POS> void use_lever(Tiles<POS>) {
    apply_state<POS>();
}

// A pressure plate goes down under the player and stays down.
template <TilePos POS> void press_plate(Gameplay<POS>) {
    if(game.player_is_on_tile<POS>())
        game.set_flag(mechanism(POS).flag);
}

template <TilePos POS> void press_plate(Tiles<POS>) {
    apply_state<POS>();
}

// Facing a closed door and pressing A opens it for good. The open door is walked through.
template <TilePos POS> void open_door(Gameplay<POS>) {
    constexpr FLAG flag = mechanism(POS).flag;
    if(!game.get_flag(flag) && player_just_interacted<POS>())
        game.set_flag(flag);
}

template <TilePos POS> void open_door(Tiles<POS>) {
    apply_state<POS>();
}

inline constexpr uint8_t CHEST_GOLD = 25;

// A chest gives its gold once. The open lid is drawn before the words appear.
template <TilePos POS> void open_chest(Dialogue<POS>) {
    constexpr Mechanism entry = mechanism(POS);
    if(!player_just_interacted<POS>())
        return;
    if(game.get_flag(entry.flag)) {
        dialogue_show(F("The chest is empty."));
        return;
    }
    game.set_flag(entry.flag);
    game.gold += CHEST_GOLD;
    set_tile<POS>(entry.tile);
    dialogue_show(F("You find 25 gold coins."));
}

template <TilePos POS> void open_chest(Tiles<POS>) {
    apply_state<POS>();
}

// The orb lights at a touch and stays lit. Its flag also lights the runes and opens the vault.
template <TilePos POS> void light_orb(Dialogue<POS>) {
    constexpr Mechanism entry = mechanism(POS);
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

// Signs and plaques. Each notice has its own words.
template <TilePos POS> void read_notice(Dialogue<POS>) {
    static_assert(POS == positions::CaveSign || POS == positions::CaveNotice ||
                      POS == positions::DungeonPlaque,
                  "give this notice its words in read_notice");
    if(!player_just_interacted<POS>())
        return;
    if constexpr(POS == positions::CaveSign)
        dialogue_show(F("Hollow Mine. Closed since the bridge fell. Mind the steam."));
    else if constexpr(POS == positions::CaveNotice)
        dialogue_show(F("Levers work the gates. The old plate in the west nook runs the bridge."));
    else
        dialogue_show(F("Four seals guard the vault: spike, bar, stone and light."));
}

} // namespace handler_impl

#endif
