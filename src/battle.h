#ifndef BATTLE_H
#define BATTLE_H

#include "rpg.h"

// A battle takes place on a fixed board of 8x4 cells that fills the display. Cells are row-major:
// cell = y * BATTLE_COLUMNS + x. battle_neighbor is the only place that knows the board edges.
inline constexpr uint8_t BATTLE_COLUMNS = 8, BATTLE_ROWS = 4;
inline constexpr uint8_t BATTLE_CELLS = BATTLE_COLUMNS * BATTLE_ROWS;

// Two party members and two enemies.
inline constexpr uint8_t BATTLE_UNIT_CAPACITY = 4;
inline constexpr uint8_t BATTLE_NO_UNIT = BATTLE_UNIT_CAPACITY;

inline constexpr uint8_t BATTLE_ATTACK_DAMAGE = 2;
// Completed activations after this many lose one HP from the acting unit.
inline constexpr uint8_t BATTLE_EXHAUSTION_START = 20;

enum BATTLE_TEAM : uint8_t { BATTLE_PARTY, BATTLE_ENEMY };

// Valid once a battle has ended. A battle always ends with one side wiped out.
enum BATTLE_RESULT : uint8_t { BATTLE_VICTORY, BATTLE_DEFEAT };

// Terrain only. Its tile IDs belong to one tileset, which need not be the overworld area's.
struct Battlefield {
    uint8_t tileset;
    uint8_t const PROGMEM* tiles; // BATTLE_CELLS row-major tile IDs.
};

// Position, health, team, and the actor sheet row used to draw the unit.
struct BattleUnit {
    uint8_t x, y, hp;
    BATTLE_TEAM team;
    uint8_t sprite_row;
};

// A fixed starting arrangement. Story code names one of these and passes it to battle_show.
struct BattleEncounter {
    Battlefield field;
    uint8_t unit_count;
    BattleUnit units[BATTLE_UNIT_CAPACITY];
};

// Live combat state. It is plain data and copies cheaply, so a search can branch from it.
struct BattleState {
    Battlefield const PROGMEM* field;
    BattleUnit units[BATTLE_UNIT_CAPACITY];
    uint8_t unit_count;
    BATTLE_TEAM turn;
    uint8_t activations; // Completed activations, including forced passes.
    bool ended;
    BATTLE_RESULT result; // Meaningful only once ended is set.
};

// A unit moves or attacks by naming the adjacent cell it targets.
struct BattleAction {
    uint8_t unit_index;
    uint8_t destination;
};

// Nocturne Grove: a grassy clearing with two heroes against two creatures.
extern uint8_t const PROGMEM BATTLE_GROVE_TILES[BATTLE_CELLS];
extern BattleEncounter const PROGMEM BATTLE_GROVE;

// Starts a fresh battle from the encounter. Every call discards any earlier state.
void battle_begin(BattleState& state, BattleEncounter const PROGMEM* encounter);

// The living unit on a cell, or BATTLE_NO_UNIT. Defeated units occupy no cell.
uint8_t battle_unit_at(BattleState const& state, uint8_t cell);

// The cell one step from `cell` in `dir`. False when that step leaves the board.
bool battle_neighbor(uint8_t cell, DIRECTION dir, uint8_t& neighbor);

// An action is legal when the battle is running, the unit is alive and belongs to the side to
// act, the destination is orthogonally adjacent, and the destination is empty or holds an enemy.
// Player input and enemy AI both use this check.
bool battle_legal(BattleState const& state, BattleAction action);

// True when the side to act has at least one legal action.
bool battle_can_act(BattleState const& state);

// Resolves a legal action. An empty destination is a move. An enemy destination takes
// BATTLE_ATTACK_DAMAGE, and the attacker stays where it is. Victory or defeat is checked before
// exhaustion, so a battle-ending action carries no exhaustion charge.
void battle_apply(BattleState& state, BattleAction action);

// Forced pass for a side with no legal action. It counts as an activation.
void battle_pass(BattleState& state);

// Passes while the side to act has no legal action. Returns the number of passes.
uint8_t battle_settle(BattleState& state);

uint8_t battle_living(BattleState const& state, BATTLE_TEAM team);

// Blocks until the battle is won or lost and its result is acknowledged with A, then returns that
// result. Story code calls it like dialogue_show. The AVM screen is in battle_screen.cpp, and host
// tests substitute their own definition.
BATTLE_RESULT battle_show(BattleEncounter const PROGMEM* encounter);

#endif
