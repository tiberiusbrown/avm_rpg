#ifndef BATTLE_H
#define BATTLE_H

#include "rpg.h"

// A battle takes place on a fixed board of 8x4 cells that fills the display. Cells are row-major:
// cell = y * BATTLE_COLUMNS + x. Rules compare columns and rows, so a row wrap is never adjacency.
inline constexpr uint8_t BATTLE_COLUMNS = 8, BATTLE_ROWS = 4;
inline constexpr uint8_t BATTLE_CELLS = BATTLE_COLUMNS * BATTLE_ROWS;

// Up to four units on each side, in eight unit slots. A slot's team is stored on its unit, so the
// slots of the two sides may come in any order. The rules and the search accept any split of the
// slots; an encounter has at most BATTLE_SIDE_CAPACITY units per side.
inline constexpr uint8_t BATTLE_SIDE_CAPACITY = 4;
inline constexpr uint8_t BATTLE_UNIT_CAPACITY = 2 * BATTLE_SIDE_CAPACITY;
inline constexpr uint8_t BATTLE_NO_UNIT = BATTLE_UNIT_CAPACITY;

// The most HP a unit can hold.
inline constexpr uint8_t BATTLE_MAX_HP = 15;

// The blood price: every voluntary move costs its unit this much HP, after the move's engagement
// damage. A forced pass costs the passing side's first living unit the same.
inline constexpr uint8_t BATTLE_MOVE_COST = 1;
// Engagement damage: a unit that moves deals this much to every enemy orthogonally adjacent to its
// destination.
inline constexpr uint8_t BATTLE_ENGAGEMENT_DAMAGE = 1;

enum BATTLE_TEAM : uint8_t { BATTLE_PARTY, BATTLE_ENEMY };

// Valid once a battle has ended. A battle always ends with one side wiped out.
enum BATTLE_RESULT : uint8_t { BATTLE_VICTORY, BATTLE_DEFEAT };

// Terrain only. Its tile IDs belong to one tileset, which need not be the overworld area's.
// A cell whose tile has collision in its tileset blocks: no unit stands on it or moves through it,
// no shot or thrust passes it, and no knockback pushes a unit onto it. A rogue's leap still passes
// over it, as it passes over units, but cannot land on it. `blocked` holds the same information as
// the tiles' collision, one bit per cell (bit x of blocked[y]), so the rules read four bytes
// instead of the collision table; the tests check that the two agree. `screened` says, for every
// pair of cells, whether blocking terrain stands strictly between them on one row or column: for
// cells a and b, bit b % 8 of screened[a][b / 8]. Terrain never moves, so it is computed once at
// compile time, and a shot checks it with one load.
struct Battlefield {
    uint8_t tileset;
    uint8_t const PROGMEM* tiles; // BATTLE_CELLS row-major tile IDs.
    uint8_t blocked[4];           // One mask per row; BATTLE_ROWS is 4.
    uint8_t screened[32][4];      // BATTLE_CELLS by BATTLE_CELLS bits.
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

// Whether a battlefield's cell is blocked by its terrain.
bool battle_blocked(uint8_t field, uint8_t cell);

// How a unit reaches empty cells. An orthogonal unit moves straight north, east, south, or west for
// one cell up to its move range, and every cell it passes and lands on must be empty. A knight unit
// leaps two cells along one axis and one along the other, over anything between, onto an empty
// cell.
enum BATTLE_MOVEMENT_TYPE : uint8_t { BATTLE_MOVEMENT_ORTHOGONAL, BATTLE_MOVEMENT_KNIGHT };

// What an attack does after its damage. A knockback pushes a surviving victim one cell directly
// away from the attacker when that cell is on the board and empty.
enum BATTLE_ATTACK_EFFECT : uint8_t { BATTLE_EFFECT_NONE, BATTLE_EFFECT_KNOCKBACK };

// The immutable definition shared by every unit of one type. Live units store an index into the
// table in PROGMEM, so the battle state carries only compact identifiers. Movement and attack are
// separate patterns. Every attack strikes along a row or column at a distance from attack_min to
// attack_max, and every cell between the attacker and its victim must be empty. The attacker never
// moves when it attacks.
struct BattleUnitType {
    uint8_t sprite_row; // Actor sheet row. The row picks artwork and nothing else.
    BATTLE_MOVEMENT_TYPE movement_type;
    uint8_t move_range; // Farthest orthogonal move. Knight units leave it zero.
    uint8_t attack_min, attack_max;
    uint8_t damage; // HP removed from a struck unit.
    BATTLE_ATTACK_EFFECT effect;
};

// The six archetypes. The team is stored on each unit, so any type can fight on either side.
enum BATTLE_UNIT_TYPE : uint8_t {
    BATTLE_TYPE_FIGHTER,  // Steps one cell; strikes an adjacent unit for 3.
    BATTLE_TYPE_ROGUE,    // Leaps like a chess knight; strikes an adjacent unit for 3.
    BATTLE_TYPE_ARCHER,   // Steps one cell; shoots along a clear line 2 to 4 cells away for 3.
    BATTLE_TYPE_SPEARMAN, // Steps one cell; thrusts along a clear line 1 or 2 cells away for 3.
    BATTLE_TYPE_WOLF,     // Runs one or two cells straight; bites an adjacent unit for 3.
    BATTLE_TYPE_BRUTE,    // Steps one cell; strikes an adjacent unit for 2 and knocks it back.
    BATTLE_UNIT_TYPE_COUNT,
};

// The archetype definitions. Sprite rows come from actors16.png: people for the fighter, rogue,
// archer, and spearman, the four-legged dog for the wolf, and the hulking ogre for the brute.
// Normal attacks deal 3, and the brute's 2 plus knockback. Encounters give each unit its HP; the
// balanced starting values are 10 for a fighter, rogue, or spearman, 9 for an archer or wolf, and
// 12 for a brute. This one initializer builds the program-memory table and the rule copy below.
// clang-format off
#define BATTLE_UNIT_TYPE_INITIALIZER                                                               \
    {                                                                                              \
        {2, BATTLE_MOVEMENT_ORTHOGONAL, 1, 1, 1, 3, BATTLE_EFFECT_NONE},       /* Fighter */       \
        {1, BATTLE_MOVEMENT_KNIGHT, 0, 1, 1, 3, BATTLE_EFFECT_NONE},           /* Rogue */         \
        {0, BATTLE_MOVEMENT_ORTHOGONAL, 1, 2, 4, 3, BATTLE_EFFECT_NONE},       /* Archer */        \
        {3, BATTLE_MOVEMENT_ORTHOGONAL, 1, 1, 2, 3, BATTLE_EFFECT_NONE},       /* Spearman */      \
        {17, BATTLE_MOVEMENT_ORTHOGONAL, 2, 1, 1, 3, BATTLE_EFFECT_NONE},      /* Wolf */          \
        {25, BATTLE_MOVEMENT_ORTHOGONAL, 1, 1, 1, 2, BATTLE_EFFECT_KNOCKBACK}, /* Brute */         \
    }
// clang-format on

extern BattleUnitType const PROGMEM BATTLE_UNIT_TYPES[BATTLE_UNIT_TYPE_COUNT];

// A compile-time copy of the definitions, for checks and for tables derived from them. Nothing
// reads it at run time.
inline constexpr BattleUnitType BATTLE_UNIT_DEFINITIONS[BATTLE_UNIT_TYPE_COUNT] =
    BATTLE_UNIT_TYPE_INITIALIZER;

// The rule fields of each unit type, without the artwork, derived from the same initializer at
// compile time. The rules read them through battle_rules. Like every constant object on the AVM,
// the table sits in program memory and costs no RAM.
struct BattleUnitRules {
    BATTLE_MOVEMENT_TYPE movement_type;
    uint8_t move_range;
    uint8_t attack_min, attack_max;
    uint8_t damage;
    BATTLE_ATTACK_EFFECT effect;
};

constexpr BattleUnitRules battle_rules_from(BattleUnitType const& type) {
    return BattleUnitRules{type.movement_type, type.move_range, type.attack_min,
                           type.attack_max,    type.damage,     type.effect};
}

static_assert(BATTLE_UNIT_TYPE_COUNT == 6, "BATTLE_UNIT_RULES lists every type");
inline constexpr BattleUnitRules BATTLE_UNIT_RULES[BATTLE_UNIT_TYPE_COUNT] = {
    battle_rules_from(BATTLE_UNIT_DEFINITIONS[0]), battle_rules_from(BATTLE_UNIT_DEFINITIONS[1]),
    battle_rules_from(BATTLE_UNIT_DEFINITIONS[2]), battle_rules_from(BATTLE_UNIT_DEFINITIONS[3]),
    battle_rules_from(BATTLE_UNIT_DEFINITIONS[4]), battle_rules_from(BATTLE_UNIT_DEFINITIONS[5]),
};

inline BattleUnitRules const& battle_rules(BATTLE_UNIT_TYPE type) {
    return BATTLE_UNIT_RULES[type];
}

// Whether a straight-line distance is within an attack range. Attacks are always along one row or
// column, so callers pass the distance only for a pair that shares one.
constexpr bool battle_in_attack_range(BattleUnitRules const& rules, uint8_t distance) {
    return distance >= rules.attack_min && distance <= rules.attack_max;
}

// A unit's board cell, health, type, and team. The team is stored, so artwork never decides it. The
// fields are packed into two bytes (the cell in the first; HP, type, and team in the second), so a
// battle state with eight slots is no larger than four unpacked units, and every copy the search
// makes stays at 22 bytes.
struct BattleUnit {
    uint8_t cell : 5;
    uint8_t hp : 4; // At most BATTLE_MAX_HP.
    BATTLE_UNIT_TYPE type : 3;
    BATTLE_TEAM team : 1;
};

static_assert(sizeof(BattleUnit) == 2, "a unit packs into two bytes");
static_assert(BATTLE_CELLS <= 32 && BATTLE_MAX_HP <= 15 && BATTLE_UNIT_TYPE_COUNT <= 8,
              "the unit's bit fields hold every cell, HP value, and type");

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

// A unit moves or attacks by naming the cell it targets. An empty destination is a move, and a
// destination holding an enemy is an attack. Every legal action has exactly one outcome.
struct BattleAction {
    uint8_t unit_index;
    uint8_t destination;
};

// Search scratch stores an action in one byte: bits 0 to 4 hold the destination and bits 5 to 7 the
// unit. Every byte value names some action, so a forced pass is written as unit 0 targeting its own
// cell, which is never legal (see battle_pass_marker).
static_assert(BATTLE_CELLS == 32 && BATTLE_UNIT_CAPACITY <= 8,
              "a packed action holds a cell and a unit");

constexpr uint8_t battle_pack(BattleAction action) {
    return (uint8_t)(action.unit_index << 5 | action.destination);
}

constexpr BattleAction battle_unpack(uint8_t packed) {
    return BattleAction{(uint8_t)(packed >> 5), (uint8_t)(packed & (BATTLE_CELLS - 1))};
}

// The packed byte that stands for a forced pass in a position: unit 0 targeting its own cell. A
// unit never legally targets its own cell, alive (a friendly cell) or defeated, so the byte never
// names a legal action of that position.
constexpr uint8_t battle_pass_marker(BattleState const& state) {
    return state.units[0].cell;
}

// Bounds on legal actions. A unit has at most eight moves: four directions of two cells for the
// longest orthogonal range, or eight knight leaps. An attack pairs an acting unit with an opponent,
// so a side of a units facing BATTLE_UNIT_CAPACITY - a opponents has at most a * (capacity - a)
// attacks. The bound below is the largest total over every split of the slots.
inline constexpr uint8_t BATTLE_MOVE_LIMIT = 8;

constexpr uint8_t battle_action_bound(uint8_t actors) {
    return (uint8_t)(actors * BATTLE_MOVE_LIMIT + actors * (BATTLE_UNIT_CAPACITY - actors));
}

constexpr uint8_t battle_action_capacity() {
    uint8_t most = 0;
    for(uint8_t actors = 0; actors <= BATTLE_UNIT_CAPACITY; ++actors)
        if(battle_action_bound(actors) > most)
            most = battle_action_bound(actors);
    return most;
}

// Every legal action of one side fits here, so no enumeration is ever truncated.
inline constexpr uint8_t BATTLE_ACTION_CAPACITY = battle_action_capacity();
// Every legal action of one unit fits here: its moves and one attack on each other unit.
inline constexpr uint8_t BATTLE_UNIT_ACTION_CAPACITY = BATTLE_MOVE_LIMIT + BATTLE_UNIT_CAPACITY - 1;

// Nocturne Grove: a grassy clearing with two heroes against two creatures.
extern uint8_t const PROGMEM BATTLE_GROVE_TILES[BATTLE_CELLS];
extern BattleEncounter const PROGMEM BATTLE_GROVE;

// Starts a fresh battle from the encounter. Every call discards any earlier state.
void battle_begin(BattleState& state, BattleEncounter const PROGMEM* encounter);

// The living unit on a cell, or BATTLE_NO_UNIT. Defeated units occupy no cell.
uint8_t battle_unit_at(BattleState const& state, uint8_t cell);

// The cell one step from `cell` in `dir`. False when that step leaves the board.
bool battle_neighbor(uint8_t cell, DIRECTION dir, uint8_t& neighbor);

// The direction from `from` toward `to` along the axis with the larger difference, preferring the
// columns on a tie. For cells on one row or column it is the exact direction.
DIRECTION battle_direction(uint8_t from, uint8_t to);

// The HP one attack by this unit removes, and the sprite row it is drawn with.
uint8_t battle_attack_damage(BattleUnit const& unit);
uint8_t battle_sprite_row(BattleUnit const& unit);

// True when the unit is alive and its movement pattern reaches the cell, which must be empty and
// open, along with every cell an orthogonal move passes; a knight's leap passes over anything.
// Ignores whose turn it is.
bool battle_can_move_to(BattleState const& state, uint8_t unit_index, uint8_t destination);

// Whether a living unit or blocking terrain stands strictly between two cells on one row or column.
// The rules use it for attacks and orthogonal moves, and the evaluation uses it for the same
// attacks.
bool battle_line_blocked(BattleState const& state, uint8_t a, uint8_t b);

// True when both units are alive and on opposing teams, they share a row or column at a distance
// within the attacker's range, and no living unit or blocking terrain stands between them. Ignores
// whose turn it is, so the evaluation can ask about either side.
bool battle_can_attack(BattleState const& state, uint8_t attacker, uint8_t victim);

// An action is legal when the battle is running, the unit is alive and belongs to the side to act,
// and the destination is either an empty cell its movement reaches or an enemy it can attack. A
// friendly destination is never legal. Player input and enemy AI both use this check.
bool battle_legal(BattleState const& state, BattleAction action);

// The canonical legal-action enumeration, which player input, rendering, and the search all use. It
// writes packed actions to `out`: first every attack, by acting unit and then victim, in ascending
// index order; then every move, by unit in ascending index order, then north, east, south, and west
// (nearest cell first) or the knight offsets clockwise from north-north-east. It returns the count
// and stores the number of leading attacks in `attacks`. Each legal action appears exactly once.
// `out` must hold BATTLE_ACTION_CAPACITY actions.
uint8_t battle_actions(BattleState const& state, uint8_t* out, uint8_t& attacks);

// The same enumeration for one unit, in the same order. It is empty when the unit is not in the
// battle or cannot act. `out` must hold BATTLE_UNIT_ACTION_CAPACITY actions.
uint8_t battle_unit_actions(BattleState const& state, uint8_t unit_index, uint8_t* out,
                            uint8_t& attacks);

// Shared scratch for action lists, so no caller keeps a list on its own frame. While the enemy's
// search runs it is the search's action stack; between searches, battle_can_act, the battle
// screen's destination markers, and the greedy choice list actions here briefly. No list here
// survives a call into another of those users. It holds BATTLE_ACTION_CAPACITY actions and more.
inline constexpr uint8_t BATTLE_ACTION_SCRATCH_CAPACITY = 80;
static_assert(BATTLE_ACTION_SCRATCH_CAPACITY >= BATTLE_ACTION_CAPACITY, "a whole side's list fits");
extern uint8_t battle_action_scratch[BATTLE_ACTION_SCRATCH_CAPACITY];

// True when the side to act has at least one legal action. It lists in battle_action_scratch.
bool battle_can_act(BattleState const& state);

// What a legal action does to the opposing side, from the most urgent kind to the least. A win
// defeats every remaining opponent, by an attack or by a move's engagement damage, and ends the
// battle at once, even when a sacrificing mover falls too. A kill defeats at least one opponent.
// Damage is any other action that hurts an opponent: every attack, and a move that engages an
// adjacent enemy. A quiet move hurts no opponent; it only costs its mover HP.
enum BATTLE_ACTION_KIND : uint8_t {
    BATTLE_ACTION_WIN,
    BATTLE_ACTION_KILL,
    BATTLE_ACTION_ATTACK, // Damage without a kill: every other attack, and engaging moves.
    BATTLE_ACTION_MOVE,   // A quiet move.
};

BATTLE_ACTION_KIND battle_action_kind(BattleState const& state, BattleAction action);

// The engagement a move to `destination` would make: how many living enemies of the unit stand
// orthogonally adjacent to it, which each take BATTLE_ENGAGEMENT_DAMAGE, and how many of them that
// defeats. The destination is assumed empty, as for any legal move.
struct BattleEngagement {
    uint8_t hits, kills;
};

BattleEngagement battle_engagement(BattleState const& state, uint8_t unit_index,
                                   uint8_t destination);

// Whether two cells share an edge: a row apart (a difference of BATTLE_COLUMNS), or one apart on
// the same row, so a row wrap is never adjacency.
constexpr bool battle_cells_adjacent(uint8_t a, uint8_t b) {
    uint8_t difference = a > b ? (uint8_t)(a - b) : (uint8_t)(b - a);
    return difference == BATTLE_COLUMNS || (difference == 1 && battle_row(a) == battle_row(b));
}

// Resolves a legal action, all of it, before deciding whether the battle has ended.
//
// A move (an empty destination) has three effects in order: the unit stands on its destination;
// every living enemy orthogonally adjacent to the destination loses BATTLE_ENGAGEMENT_DAMAGE, once
// each; then the mover pays BATTLE_MOVE_COST. A unit with 1 HP may move, deal its engagement
// damage, and fall. When the engagement defeats the last opponent, the battle ends there, the
// acting side wins, and the price is never paid, so a 1 HP unit that finishes the battle this way
// survives it. Engagement damage ignores the mover's attack pattern and never triggers an attack
// effect.
//
// An attack (an enemy destination) takes the attacker's damage, and the attacker stays where it is.
// A knockback then pushes a surviving victim one cell directly away when that cell is on the board
// and empty; a push is not a move, so it costs nothing and engages no one. Attacks cost the
// attacker nothing.
//
// Then the battle ends once a side has no living unit. The acting side wins when no opponent is
// left; it wins before its mover pays, so the two sides never fall together. When an action removes
// only the acting side's last unit (a move that costs its last HP without winning), the opponent
// wins. There is no draw.
void battle_apply(BattleState& state, BattleAction action);

// Forced pass for a side with no legal action. The side's first living unit, in slot order, loses
// BATTLE_MOVE_COST, which can defeat it and end the battle. It counts as an activation. A side
// passes only when no unit can move or attack; a move that would cost a unit its last HP is still a
// move.
void battle_pass(BattleState& state);

// Passes while the side to act has no legal action. Returns the number of passes. Every pass costs
// HP and no action restores any, so it always returns.
uint8_t battle_settle(BattleState& state);

uint8_t battle_living(BattleState const& state, BATTLE_TEAM team);

// Blocks until the battle is won or lost and its result is acknowledged with A, then returns that
// result. Story code calls it like dialogue_show. The AVM screen is in battle_screen.cpp, and host
// tests substitute their own definition.
BATTLE_RESULT battle_show(BattleEncounter const PROGMEM* encounter);

#endif
