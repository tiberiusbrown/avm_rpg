#ifndef LOGIC_MECHANISMS_H
#define LOGIC_MECHANISMS_H

#include "handler_defaults.h"
#include "area_ids.h"
#include "tile_ids.h"

// The machinery that the environments share: flag-driven cells (levers, plates, doors, chests and
// everything that only follows a flag), messages that only show words, and passages between areas.
// An environment header supplies the tables for its own cells and ends, and this header supplies
// the handlers that read them.

namespace handler_impl {

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

// Shows the cell's set state. Every handler of a mechanism cell runs this in the tile pass.
template <TilePos POS> void apply_state() {
    constexpr Mechanism entry = mechanism<POS>();
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
        game.toggle_flag(mechanism<POS>().flag);
}

template <TilePos POS> void use_lever(Tiles<POS>) {
    apply_state<POS>();
}

// A pressure plate goes down under the player and stays down.
template <TilePos POS> void press_plate(Gameplay<POS>) {
    if(game.player_is_on_tile<POS>())
        game.set_flag(mechanism<POS>().flag);
}

template <TilePos POS> void press_plate(Tiles<POS>) {
    apply_state<POS>();
}

// Facing a closed door and pressing A opens it for good. The open door is walked through.
template <TilePos POS> void open_door(Gameplay<POS>) {
    constexpr FLAG flag = mechanism<POS>().flag;
    if(!game.get_flag(flag) && player_just_interacted<POS>())
        game.set_flag(flag);
}

template <TilePos POS> void open_door(Tiles<POS>) {
    apply_state<POS>();
}

inline constexpr uint8_t CHEST_GOLD = 25;

// A chest gives its gold once. The open lid is drawn before the words appear.
template <TilePos POS> void open_chest(Dialogue<POS>) {
    constexpr Mechanism entry = mechanism<POS>();
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
