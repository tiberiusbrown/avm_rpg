#ifndef LOGIC_MECHANISMS_H
#define LOGIC_MECHANISMS_H

#include "handler_defaults.h"
#include "area_ids.h"
#include "sfx.h"
#include "tile_ids.h"

// The machinery that the environments share: flag-driven cells (levers, plates, doors, chests and
// everything that only follows a flag), messages that only show words, and passages between areas.
// An environment header supplies the tables for its own cells and ends, and this header supplies
// the handlers that read them.

namespace handler_impl {

// What a mechanism cell is. The kind decides how the player works the cell, whether a second
// operation undoes the first, and what each change sounds like, so a new lever or plate is one
// table row and the kinds are the one place that says what levers and plates do. A cell that the
// player does not work only follows its flag.
enum MECHANISM_KIND : uint8_t {
    // Cells that the player works.
    MECHANISM_LEVER, // Facing it and pressing A throws it, and pressing A again throws it back.
    MECHANISM_PLATE, // Standing on it presses it down for good.
    MECHANISM_DOOR,  // Facing it and pressing A opens it for good. The open door is walked through.
    MECHANISM_ORB,   // Touching it wakes it for good, with words.
    MECHANISM_CHEST, // Opening it pays out once.
    // Cells that only follow their flag.
    MECHANISM_GATE,   // A gate, a portcullis or an archway.
    MECHANISM_BLOCKS, // Blocks that sink into the floor.
    MECHANISM_SPIKES, // Spikes that sink into the floor.
    MECHANISM_BRIDGE, // A bridge over a chasm.
    MECHANISM_RUNE    // A rune that only lights up: its flag's other cells make the noise.
};

enum MECHANISM_INPUT : uint8_t {
    INPUT_NONE, // The cell only follows its flag.
    INPUT_FACE, // The player faces the cell and presses A.
    INPUT_STEP  // The player stands on the cell.
};

struct MechanismTraits {
    MECHANISM_INPUT input;
    bool toggles; // A second operation switches the flag off again, and otherwise the flag latches.
    SFX on, off;  // Heard when the cell's flag turns on or off; SFX_NONE is silence.
};

constexpr MechanismTraits mechanism_traits(MECHANISM_KIND kind) {
    switch(kind) {
        case MECHANISM_LEVER:
            return {INPUT_FACE, true, SFX_LEVER_ON, SFX_LEVER_OFF};
        case MECHANISM_PLATE:
            return {INPUT_STEP, false, SFX_PLATE_DOWN, SFX_NONE};
        case MECHANISM_DOOR:
            return {INPUT_FACE, false, SFX_DOOR_UNLOCK, SFX_NONE};
        case MECHANISM_ORB:
            return {INPUT_FACE, false, SFX_ORB_LIGHT, SFX_NONE};
        case MECHANISM_CHEST:
            return {INPUT_FACE, false, SFX_CHEST_OPEN, SFX_NONE};
        case MECHANISM_GATE:
        case MECHANISM_BLOCKS:
            return {INPUT_NONE, false, SFX_GRIND_OPEN, SFX_GRIND_CLOSE};
        case MECHANISM_SPIKES:
            return {INPUT_NONE, false, SFX_SCRAPE_OPEN, SFX_SCRAPE_CLOSE};
        case MECHANISM_BRIDGE:
            return {INPUT_NONE, false, SFX_BRIDGE_EXTEND, SFX_BRIDGE_RETRACT};
        default:
            return {INPUT_NONE, false, SFX_NONE, SFX_NONE};
    }
}

// A mechanism is every cell that one world flag changes. The map shows each cell as it is while
// the flag is clear, and the cell shows `tile` while the flag is set. The cache is rebuilt from the
// map when it reloads, so the tile pass applies the set state on every frame.
struct Mechanism {
    MECHANISM_KIND kind;
    TileRegion cells;
    FLAG flag;
    uint8_t tile;
};

constexpr TileRegion one_cell(TilePos pos) {
    return {pos, 1, 1};
}

// The mechanisms of one area. The header of the environment that owns the area specializes this
// with `static constexpr Mechanism TABLE[]`. Every cell has its row in the table of its own area.
template <uint8_t AREA> struct AreaMechanisms;

// Declared and never defined. A cell that is missing from its area's table fails to compile.
void mechanism_missing();

template <TilePos CELL> consteval Mechanism mechanism() {
    for(auto const& entry : AreaMechanisms<CELL.area>::TABLE)
        if(entry.cells.contains(CELL))
            return entry;
    mechanism_missing();
    return AreaMechanisms<CELL.area>::TABLE[0];
}

// The sound of what the cell's flag moves, for the flag turning on or off: the first row of the
// same flag that the player does not work and that makes a noise. A lever is heard to click and
// then the gate that it opens is heard to grind.
template <TilePos CELL> constexpr SFX moved_sound(bool on) {
    constexpr Mechanism self = mechanism<CELL>();
    for(auto const& entry : AreaMechanisms<CELL.area>::TABLE) {
        MechanismTraits traits = mechanism_traits(entry.kind);
        SFX sound = on ? traits.on : traits.off;
        if(entry.flag == self.flag && traits.input == INPUT_NONE && sound != SFX_NONE)
            return sound;
    }
    return SFX_NONE;
}

// Switches the cell's flag and plays the change: the cell's own sound, then the sound of what its
// flag moves. Every operation of a mechanism goes through here.
template <TilePos POS> void switch_mechanism(bool on) {
    constexpr Mechanism entry = mechanism<POS>();
    constexpr MechanismTraits traits = mechanism_traits(entry.kind);
    constexpr SFX moved_on = moved_sound<POS>(true), moved_off = moved_sound<POS>(false);
    if(on)
        game.set_flag(entry.flag);
    else
        game.clear_flag(entry.flag);
    sfx_play(on ? traits.on : traits.off, on ? moved_on : moved_off);
}

// Shows the cell's set state. Every handler of a mechanism cell runs this in the tile pass.
template <TilePos POS> void apply_state() {
    constexpr Mechanism entry = mechanism<POS>();
    if(game.get_flag(entry.flag))
        set_tile<POS>(entry.tile);
}

// Whether the player works the cell this frame. Facing it consumes the press of A.
template <TilePos POS, MECHANISM_INPUT INPUT> bool operated() {
    static_assert(INPUT != INPUT_NONE, "a cell that follows its flag is never operated");
    if constexpr(INPUT == INPUT_FACE)
        return player_just_interacted<POS>();
    else
        return game.player_is_on_tile<POS>();
}

// Works a cell of the given kind. The handler's kind must be the kind of the cell's table row, so a
// handler on the wrong row fails to compile. A latched cell takes no more presses.
template <TilePos POS, MECHANISM_KIND KIND> void work_mechanism() {
    constexpr Mechanism entry = mechanism<POS>();
    constexpr MechanismTraits traits = mechanism_traits(KIND);
    static_assert(entry.kind == KIND, "the handler does not match the kind of the cell's row");
    bool on = game.get_flag(entry.flag);
    if(on && !traits.toggles)
        return;
    if(operated<POS, traits.input>())
        switch_mechanism<POS>(!on);
}

// A gate, a bridge, spikes, blocks or any other cell that only follows its flag.
template <TilePos POS> void show_state(Tiles<POS>) {
    static_assert(mechanism_traits(mechanism<POS>().kind).input == INPUT_NONE,
                  "show_state belongs on a cell that the player does not work");
    apply_state<POS>();
}

// Facing a lever and pressing A throws it, and pressing A again throws it back.
template <TilePos POS> void use_lever(Gameplay<POS>) {
    work_mechanism<POS, MECHANISM_LEVER>();
}

template <TilePos POS> void use_lever(Tiles<POS>) {
    apply_state<POS>();
}

// A pressure plate goes down under the player and stays down.
template <TilePos POS> void press_plate(Gameplay<POS>) {
    work_mechanism<POS, MECHANISM_PLATE>();
}

template <TilePos POS> void press_plate(Tiles<POS>) {
    apply_state<POS>();
}

// Facing a closed door and pressing A opens it for good. The open door is walked through.
template <TilePos POS> void open_door(Gameplay<POS>) {
    work_mechanism<POS, MECHANISM_DOOR>();
}

template <TilePos POS> void open_door(Tiles<POS>) {
    apply_state<POS>();
}

inline constexpr uint8_t CHEST_GOLD = 25;

// Check the whole reward before opening the chest and drawing its lid.
template <TilePos POS> void open_chest(Dialogue<POS>) {
    constexpr Mechanism entry = mechanism<POS>();
    static_assert(entry.kind == MECHANISM_CHEST, "the handler does not match the kind of the row");
    if(!player_just_interacted<POS>())
        return;
    if(game.get_flag(entry.flag)) {
        dialogue_show(F("The chest is empty."));
        return;
    }
    static_assert(entry.flag == FLAG_CAVE_CHEST || entry.flag == FLAG_DUNGEON_CHEST);
    constexpr bool cave = entry.flag == FLAG_CAVE_CHEST;
    constexpr SUPPLY_ID first = cave ? SUPPLY_POTION : SUPPLY_ELIXIR;
    constexpr SUPPLY_ID second = cave ? SUPPLY_ETHER : SUPPLY_REVIVE;
    if(!supplies_can_add(game.supplies, first, 1) || !supplies_can_add(game.supplies, second, 1)) {
        dialogue_show(F("You cannot carry all the supplies."));
        return;
    }
    supplies_add(game.supplies, first, 1);
    supplies_add(game.supplies, second, 1);
    switch_mechanism<POS>(true);
    game.gold += CHEST_GOLD;
    set_tile<POS>(entry.tile);
    dialogue_show(cave ? F("You find 25 gold, a Potion and an Ether.")
                       : F("You find 25 gold, an Elixir and a Revive."));
}

template <TilePos POS> void open_chest(Tiles<POS>) {
    apply_state<POS>();
}

// A message is a cell that only shows its words when the player faces it and presses A: a sign or
// a plaque. Anything that does more than show words has a handler of its own.
struct Message {
    TilePos cell;
    char const PROGMEM* words;
};

// The messages of one area. The header of the environment that owns the area specializes this with
// `static constexpr Message TABLE[]`. Every message has its row in the table of its own area.
template <uint8_t AREA> struct AreaMessages;

// Declared and never defined. A message that is missing from its area's table fails to compile.
void message_missing();

template <TilePos CELL> consteval Message message() {
    for(auto const& entry : AreaMessages<CELL.area>::TABLE)
        if(entry.cell == CELL)
            return entry;
    message_missing();
    return AreaMessages<CELL.area>::TABLE[0];
}

template <TilePos POS> void show_message(Dialogue<POS>) {
    if(player_just_interacted<POS>())
        dialogue_show(message<POS>().words);
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

// The passages that lead into one area. The header of the environment that owns the area
// specializes this with `static constexpr Passage TABLE[]`. An area without passages has none.
// The entrance outdoors is listed with the interior that it leads into, so the outdoor area never
// has a table of its own.
template <uint8_t AREA> struct AreaPassages {};

// Declared and never defined. An entrance or doorway that is missing from every table reaches
// this call during constant evaluation, which fails to compile.
void passage_missing();

// The passage that has `END` as one of its ends, searching the areas from `AREA` up to NUM_AREAS.
// This and the functions below are templates so that they read the tables only once every
// environment header has registered its own.
template <TilePos END, uint8_t AREA = 1> consteval Passage passage() {
    if constexpr(requires { AreaPassages<AREA>::TABLE; }) {
        for(auto const& found : AreaPassages<AREA>::TABLE)
            if(found.outside == END || found.inside == END)
                return found;
    }
    if constexpr(AREA < NUM_AREAS) {
        return passage<END, AREA + 1>();
    } else {
        passage_missing();
        return {END, END, 0};
    }
}

// Where the player arrives after walking onto this end of a passage.
template <TilePos END> consteval TilePos passage_arrival() {
    constexpr Passage found = passage<END>();
    if constexpr(found.outside == END)
        return {found.inside.area, found.inside.x, (uint8_t)(found.inside.y + found.inside_step)};
    else
        return {found.outside.area, found.outside.x, (uint8_t)(found.outside.y + 1)};
}

// The player arrives walking away from the end that they come out of.
template <TilePos END> consteval DIRECTION passage_facing() {
    constexpr Passage found = passage<END>();
    if constexpr(found.outside == END && found.inside_step < 0)
        return NORTH;
    else
        return SOUTH;
}

template <TilePos POS> void take_passage(Gameplay<POS>) {
    if(player_is_walking_onto<POS>())
        teleport_player<passage_arrival<POS>()>(passage_facing<POS>());
}

} // namespace handler_impl

#endif
