#ifndef BATTLE_H
#define BATTLE_H

#include "rpg.h"

// A battle takes place on a fixed board of 8x4 cells that fills the display. Cells are row-major:
// cell = y * BATTLE_COLUMNS + x. Rules compare columns and rows, so a row wrap is never adjacency.
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
    BATTLE_TYPE_FIGHTER,  // Steps one cell; strikes an adjacent unit for 2.
    BATTLE_TYPE_ROGUE,    // Leaps like a chess knight; strikes an adjacent unit for 2.
    BATTLE_TYPE_ARCHER,   // Steps one cell; shoots along a clear line 2 to 4 cells away for 2.
    BATTLE_TYPE_SPEARMAN, // Steps one cell; thrusts along a clear line 1 or 2 cells away for 2.
    BATTLE_TYPE_WOLF,     // Runs one or two cells straight; bites an adjacent unit for 2.
    BATTLE_TYPE_BRUTE,    // Steps one cell; strikes an adjacent unit for 1 and knocks it back.
    BATTLE_UNIT_TYPE_COUNT,
};

// The archetype definitions. Sprite rows come from actors16.png: people for the fighter, rogue,
// archer, and spearman, the four-legged dog for the wolf, and the hulking ogre for the brute. This
// one initializer builds the program-memory table and the rule copy below.
// clang-format off
#define BATTLE_UNIT_TYPE_INITIALIZER                                                               \
    {                                                                                              \
        {2, BATTLE_MOVEMENT_ORTHOGONAL, 1, 1, 1, 2, BATTLE_EFFECT_NONE},       /* Fighter */       \
        {1, BATTLE_MOVEMENT_KNIGHT, 0, 1, 1, 2, BATTLE_EFFECT_NONE},           /* Rogue */         \
        {0, BATTLE_MOVEMENT_ORTHOGONAL, 1, 2, 4, 2, BATTLE_EFFECT_NONE},       /* Archer */        \
        {3, BATTLE_MOVEMENT_ORTHOGONAL, 1, 1, 2, 2, BATTLE_EFFECT_NONE},       /* Spearman */      \
        {17, BATTLE_MOVEMENT_ORTHOGONAL, 2, 1, 1, 2, BATTLE_EFFECT_NONE},      /* Wolf */          \
        {25, BATTLE_MOVEMENT_ORTHOGONAL, 1, 1, 1, 1, BATTLE_EFFECT_KNOCKBACK}, /* Brute */         \
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

// A unit moves or attacks by naming the cell it targets. An empty destination is a move, and a
// destination holding an enemy is an attack. Every legal action has exactly one outcome.
struct BattleAction {
    uint8_t unit_index;
    uint8_t destination;
};

// Search scratch stores an action in one byte: bits 0 to 4 hold the destination and bits 5 and 6
// the unit. Bit 7 is never set by a packed action, so BATTLE_PACKED_PASS names a forced pass.
static_assert(BATTLE_CELLS == 32 && BATTLE_UNIT_CAPACITY <= 4,
              "a packed action holds a cell and a unit");
inline constexpr uint8_t BATTLE_PACKED_PASS = 0x80;

constexpr uint8_t battle_pack(BattleAction action) {
    return (uint8_t)(action.unit_index << 5 | action.destination);
}

constexpr BattleAction battle_unpack(uint8_t packed) {
    return BattleAction{(uint8_t)(packed >> 5), (uint8_t)(packed & (BATTLE_CELLS - 1))};
}

// Bounds on legal actions. A unit has at most eight moves: four directions of two cells for the
// longest orthogonal range, or eight knight leaps. An attack pairs an acting unit with an opponent,
// so a side of a units facing BATTLE_UNIT_CAPACITY - a opponents has at most a * (capacity - a)
// attacks. The bound below is the largest total over every split of the four slots.
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

// True when the unit is alive and its movement pattern reaches the cell, which must be empty, along
// with every cell an orthogonal move passes. Ignores whose turn it is.
bool battle_can_move_to(BattleState const& state, uint8_t unit_index, uint8_t destination);

// Whether a living unit stands strictly between two cells on one row or column. The rules use it
// for attacks and orthogonal moves, and the evaluation uses it for the same attacks.
bool battle_line_blocked(BattleState const& state, uint8_t a, uint8_t b);

// True when both units are alive and on opposing teams, they share a row or column at a distance
// within the attacker's range, and no living unit stands between them. Ignores whose turn it is, so
// the evaluation can ask about either side.
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

// True when the side to act has at least one legal action.
bool battle_can_act(BattleState const& state);

// What a legal action does, from the most urgent kind to the least. A win is a kill of the last
// living opponent, which ends the battle at once.
enum BATTLE_ACTION_KIND : uint8_t {
    BATTLE_ACTION_WIN,
    BATTLE_ACTION_KILL,
    BATTLE_ACTION_ATTACK,
    BATTLE_ACTION_MOVE,
};

BATTLE_ACTION_KIND battle_action_kind(BattleState const& state, BattleAction action);

// Resolves a legal action. An empty destination is a move. An enemy destination takes the
// attacker's damage, and the attacker stays where it is. A knockback then pushes a surviving victim
// one cell directly away when that cell is on the board and empty. Victory or defeat is checked
// before exhaustion, so a battle-ending action carries no exhaustion charge.
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
