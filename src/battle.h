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

// Cells are row-major, so one index gives both the column and the row.
constexpr uint8_t battle_column(uint8_t cell) {
    return cell % BATTLE_COLUMNS;
}

constexpr uint8_t battle_row(uint8_t cell) {
    return cell / BATTLE_COLUMNS;
}

constexpr uint8_t battle_cell(uint8_t column, uint8_t row) {
    return (uint8_t)(row * BATTLE_COLUMNS + column);
}

// Battlefields that encounters can name. Encounters and battles refer to one by its index.
enum BATTLEFIELD_ID : uint8_t { BATTLEFIELD_GROVE, BATTLEFIELD_COUNT };

extern Battlefield const PROGMEM BATTLEFIELDS[BATTLEFIELD_COUNT];

// How a unit reaches the cells it can act on. One orthogonal step is the only pattern so far.
enum BATTLE_MOVEMENT_TYPE : uint8_t { BATTLE_MOVEMENT_ORTHOGONAL_ONE };

// Which reached units a unit may strike. Adjacent enemies are the only targets so far.
enum BATTLE_ATTACK_TYPE : uint8_t { BATTLE_ATTACK_ADJACENT };

// The immutable definition shared by every unit of one type. Live units store an index into the
// table in PROGMEM, so the battle state carries only compact identifiers.
struct BattleUnitType {
    uint8_t sprite_row; // Actor sheet row. The row picks artwork and nothing else.
    BATTLE_MOVEMENT_TYPE movement_type;
    BATTLE_ATTACK_TYPE attack_type;
    uint8_t damage; // HP removed from a struck unit.
};

// The unit types of the Grove encounter.
enum BATTLE_UNIT_TYPE : uint8_t {
    BATTLE_TYPE_HERO_A,
    BATTLE_TYPE_HERO_B,
    BATTLE_TYPE_CREATURE_A,
    BATTLE_TYPE_CREATURE_B,
    BATTLE_UNIT_TYPE_COUNT,
};

extern BattleUnitType const PROGMEM BATTLE_UNIT_TYPES[BATTLE_UNIT_TYPE_COUNT];

// A unit's board cell, health, type, and team. The team is stored, so artwork never decides it.
struct BattleUnit {
    uint8_t cell, hp;
    BATTLE_UNIT_TYPE type;
    BATTLE_TEAM team;
};

// A fixed starting arrangement. Story code names one of these and passes it to battle_show.
struct BattleEncounter {
    uint8_t field; // BATTLEFIELD_ID
    uint8_t unit_count;
    BattleUnit units[BATTLE_UNIT_CAPACITY];
};

// Live combat state. It is plain data and copies cheaply, so a search can branch from it.
struct BattleState {
    uint8_t field; // BATTLEFIELD_ID
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

// The direction of an adjacent cell from `from`. Only adjacent cells have a meaningful answer.
DIRECTION battle_direction(uint8_t from, uint8_t to);

// The HP one attack by this unit removes, and the sprite row it is drawn with.
uint8_t battle_attack_damage(BattleUnit const& unit);
uint8_t battle_sprite_row(BattleUnit const& unit);

// True when the attacker is alive, can reach the victim's cell, and may strike the victim. Ignores
// whose turn it is, so the evaluation can ask about either side.
bool battle_can_strike(BattleState const& state, uint8_t attacker, uint8_t victim);

// An action is legal when the battle is running, the unit is alive and belongs to the side to
// act, the destination is one its movement reaches, and the destination is empty or holds a unit
// the attacker may strike. Player input and enemy AI both use this check.
bool battle_legal(BattleState const& state, BattleAction action);

// Legal-action enumeration. A cursor walks candidate actions in a fixed order: unit index
// ascending, then north, east, south, west. It is plain data, so a search can keep one per frame.
struct BattleActionCursor {
    uint8_t unit;      // Next unit to consider.
    uint8_t direction; // Next direction to consider for that unit.
    uint8_t end;       // One past the last unit to consider.
};

// The cursor over every unit of the side to act.
BattleActionCursor battle_actions(BattleState const& state);

// The cursor over one unit alone. It is empty when that unit is not in the battle.
BattleActionCursor battle_unit_actions(BattleState const& state, uint8_t unit_index);

// Sets `action` to the next legal action the cursor reaches and returns true. Returns false once
// the cursor is exhausted. Each legal action is reached exactly once.
bool battle_next_action(BattleState const& state, BattleActionCursor& cursor, BattleAction& action);

// True when the side to act has at least one legal action.
bool battle_can_act(BattleState const& state);

// What a legal action does, from the most urgent kind to the least. A win is a kill of the last
// living opponent, which ends the battle at once. Search tries kinds in this order.
enum BATTLE_ACTION_KIND : uint8_t {
    BATTLE_ACTION_WIN,
    BATTLE_ACTION_KILL,
    BATTLE_ACTION_ATTACK,
    BATTLE_ACTION_MOVE,
};

BATTLE_ACTION_KIND battle_action_kind(BattleState const& state, BattleAction action);

// The legal destinations of one unit as bit masks by direction: bit 0 is north, then east, south,
// and west. `moves` holds empty cells the unit can enter, and `attacks` holds enemies it can
// strike.
struct BattleDestinations {
    uint8_t moves;
    uint8_t attacks;
};

BattleDestinations battle_destinations(BattleState const& state, uint8_t unit_index);

// Resolves a legal action. An empty destination is a move. An enemy destination takes the
// attacker's damage, and the attacker stays where it is. Victory or defeat is checked before
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
