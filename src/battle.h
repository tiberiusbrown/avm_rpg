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
inline constexpr uint8_t BATTLE_MAX_HP = 255;

// The blood price: every voluntary move costs its unit this much HP, after the move's engagement
// damage. A pass costs every living member of the passing team the same.
inline constexpr uint8_t BATTLE_MOVE_COST = 1;
// Engagement damage: a unit that moves deals this much to every enemy orthogonally adjacent to its
// destination.
inline constexpr uint8_t BATTLE_ENGAGEMENT_DAMAGE = 1;

enum BATTLE_TEAM : uint8_t { BATTLE_PARTY, BATTLE_ENEMY };

// Valid once a battle has ended, through team elimination or the party's resignation.
enum BATTLE_RESULT : uint8_t { BATTLE_VICTORY, BATTLE_DEFEAT };

// Marker values used only by the instrumented device-QA guest.
enum BATTLE_QA_PHASE : uint8_t { BATTLE_QA_BUSY, BATTLE_QA_INPUT, BATTLE_QA_RESULT };

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
// one cell up to its move range, and every cell it passes and lands on must be empty. A leap unit
// jumps two cells along one axis and one along the other, over anything between, onto an empty
// cell.
enum BATTLE_MOVEMENT_TYPE : uint8_t { BATTLE_MOVEMENT_ORTHOGONAL, BATTLE_MOVEMENT_LEAP };

// What an attack does after its damage. A knockback pushes a surviving victim one cell directly
// away from the attacker when that cell is on the board and empty.
enum BATTLE_ATTACK_EFFECT : uint8_t { BATTLE_EFFECT_NONE, BATTLE_EFFECT_KNOCKBACK };

// Movement and attack classes are independent. Their definitions stay in program memory.
struct MovementInfo {
    BATTLE_MOVEMENT_TYPE movement_type;
    uint8_t move_range;
};

// The damage an attack deals depends on its victim: `damage` against a victim at half its maximum
// HP or more, and `injured_damage` against a victim below half. Only Rend differs between the two.
struct AttackInfo {
    uint8_t attack_min, attack_max, damage, injured_damage;
    BATTLE_ATTACK_EFFECT effect;
};

inline constexpr MovementInfo MOVEMENT_INFO[MOVEMENT_CLASS_COUNT] = {
    {BATTLE_MOVEMENT_ORTHOGONAL, 1},
    {BATTLE_MOVEMENT_LEAP, 0},
    {BATTLE_MOVEMENT_ORTHOGONAL, 2},
};
inline constexpr AttackInfo ATTACK_INFO[ATTACK_CLASS_COUNT] = {
    {1, 1, 3, 3, BATTLE_EFFECT_NONE}, {2, 4, 3, 3, BATTLE_EFFECT_NONE},
    {1, 2, 3, 3, BATTLE_EFFECT_NONE}, {1, 1, 2, 2, BATTLE_EFFECT_KNOCKBACK},
    {1, 1, 2, 4, BATTLE_EFFECT_NONE},
};

// Each attack class's starting special. A special is an attack on the victim its class's normal
// attack reaches, and it costs its unit AP. Its damage is the normal damage plus `bonus_full` when
// the victim is at its maximum HP, or `bonus_injured` otherwise. Its effect applies with the
// class's own effect.
struct SpecialInfo {
    uint8_t ap, bonus_full, bonus_injured;
    BATTLE_ATTACK_EFFECT effect;
};

inline constexpr SpecialInfo SPECIAL_INFO[ATTACK_CLASS_COUNT] = {
    {2, 3, 3, BATTLE_EFFECT_NONE},      // Strike: Power Strike
    {2, 4, 1, BATTLE_EFFECT_NONE},      // Shoot: Opening Shot
    {2, 2, 2, BATTLE_EFFECT_KNOCKBACK}, // Reach: Driving Thrust
    {2, 3, 3, BATTLE_EFFECT_KNOCKBACK}, // Impact: Crushing Bash
    {2, 1, 3, BATTLE_EFFECT_NONE},      // Rend: Wound Hunter
};

// The display names of the starting specials, indexed by attack class.
inline constexpr char PROGMEM BATTLE_SPECIAL_NAMES[ATTACK_CLASS_COUNT][15] = {
    "Power Strike", "Opening Shot", "Driving Thrust", "Crushing Bash", "Wound Hunter",
};

// Enemy IDs index immutable definitions, independently of the unit's team or artwork.
enum ENEMY_ID : uint8_t {
    ENEMY_FIGHTER,
    ENEMY_ROGUE,
    ENEMY_ARCHER,
    ENEMY_SPEARMAN,
    ENEMY_WOLF,
    ENEMY_BRUTE,
    ENEMY_COUNT
};

struct EnemyInfo {
    uint8_t max_hp, movement_class, attack_class, sprite_row;
};

extern EnemyInfo const PROGMEM ENEMY_INFO[ENEMY_COUNT];

// A source ID below ENEMY_COUNT indexes EnemyInfo. The high bit instead references one of the
// saved party's four slots. Team is independent, allowing rule fixtures to use either source.
inline constexpr uint8_t BATTLE_PARTY_SOURCE = 0x80;
static_assert(ENEMY_COUNT <= BATTLE_PARTY_SOURCE);

constexpr uint8_t battle_party_id(uint8_t slot) {
    return (uint8_t)(BATTLE_PARTY_SOURCE | slot);
}

inline uint8_t battle_movement_class(uint8_t id) {
    return id & BATTLE_PARTY_SOURCE ? game.party.members[id & ~BATTLE_PARTY_SOURCE].movement_class
                                    : ENEMY_INFO[id].movement_class;
}

inline uint8_t battle_attack_class(uint8_t id) {
    return id & BATTLE_PARTY_SOURCE ? game.party.members[id & ~BATTLE_PARTY_SOURCE].attack_class
                                    : ENEMY_INFO[id].attack_class;
}

// A ROM cross-product lets hot rule paths read both definitions through one reference. It is
// derived from the independent class tables; no combined class ID is stored on any member.
struct BattleUnitRules {
    BATTLE_MOVEMENT_TYPE movement_type;
    uint8_t move_range;
    uint8_t attack_min, attack_max, damage, injured_damage;
    BATTLE_ATTACK_EFFECT effect;
};

inline constexpr uint8_t BATTLE_RULE_COUNT = (uint8_t)MOVEMENT_CLASS_COUNT * ATTACK_CLASS_COUNT;

struct BattleRuleTable {
    BattleUnitRules classes[BATTLE_RULE_COUNT];
};

inline constexpr BattleRuleTable BATTLE_RULES = [] {
    BattleRuleTable table{};
    for(uint8_t m = 0; m < MOVEMENT_CLASS_COUNT; ++m)
        for(uint8_t a = 0; a < ATTACK_CLASS_COUNT; ++a) {
            auto const& move = MOVEMENT_INFO[m];
            auto const& attack = ATTACK_INFO[a];
            table.classes[m * ATTACK_CLASS_COUNT + a] = {
                move.movement_type, move.move_range,       attack.attack_min, attack.attack_max,
                attack.damage,      attack.injured_damage, attack.effect};
        }
    return table;
}();

inline uint8_t battle_rule_index(uint8_t id) {
    return (uint8_t)(battle_movement_class(id) * ATTACK_CLASS_COUNT + battle_attack_class(id));
}

inline BattleUnitRules const& battle_rules(uint8_t id) {
    return BATTLE_RULES.classes[battle_rule_index(id)];
}

// Whether a straight-line distance is within an attack range. Attacks are always along one row or
// column, so callers pass the distance only for a pair that shares one.
constexpr bool battle_in_attack_range(BattleUnitRules const& rules, uint8_t distance) {
    return distance >= rules.attack_min && distance <= rules.attack_max;
}

// Only HP and a source ID describe an enemy in RAM. The other byte holds board placement and
// team. Allied units reference saved party slots; search copies never duplicate their classes.
struct BattleUnit {
    uint8_t cell : 5;
    BATTLE_TEAM team : 1;
    uint8_t hp;
    uint8_t id;

    constexpr BattleUnit() : cell(0), team(BATTLE_PARTY), hp(0), id(0) {}

    constexpr BattleUnit(uint8_t cell, uint8_t hp, uint8_t id, BATTLE_TEAM team)
        : cell(cell), team(team), hp(hp), id(id) {}
};

static_assert(sizeof(BattleUnit) == 3, "one placement byte plus full HP and source ID");
static_assert(sizeof(EnemyInfo) == 4, "enemy statistics and artwork stay in ROM");

// Encounters author only enemies and deployment cells. Allies always come from game.party.
struct BattleEncounter {
    uint8_t field;
    uint8_t enemy_count;
    uint8_t enemies[BATTLE_SIDE_CAPACITY];
    uint8_t party_cells[BATTLE_SIDE_CAPACITY];
    uint8_t enemy_cells[BATTLE_SIDE_CAPACITY];
};

// Live combat state. It is plain data and copies cheaply, so a search can branch from it.
#pragma pack(push, 1)

struct BattleState {
    uint8_t field; // BATTLEFIELD_ID
    BattleUnit units[BATTLE_UNIT_CAPACITY];
    uint8_t unit_count;
    BATTLE_TEAM turn;
    uint16_t activations; // Full uint8 HP can require more than 255 activations.
    bool ended;
    BATTLE_RESULT result; // Meaningful only once ended is set.
};

#pragma pack(pop)

// A unit moves or attacks by naming the cell it targets. An empty destination is a move, and a
// destination holding an enemy is an attack. BATTLE_NO_UNIT instead names a team pass.
// Every action has exactly one outcome.
struct BattleAction {
    uint8_t unit_index;
    uint8_t destination;
};

// Search scratch stores an action in one byte: bits 0 to 4 hold the destination and bits 5 to 7 the
// unit. Every byte value names some action, so a pass is written as unit 0 targeting its own
// cell, which is never legal (see battle_pass_marker).
static_assert(BATTLE_CELLS == 32 && BATTLE_UNIT_CAPACITY <= 8,
              "a packed action holds a cell and a unit");

constexpr uint8_t battle_pack(BattleAction action) {
    return (uint8_t)(action.unit_index << 5 | action.destination);
}

constexpr BattleAction battle_unpack(uint8_t packed) {
    return BattleAction{(uint8_t)(packed >> 5), (uint8_t)(packed & (BATTLE_CELLS - 1))};
}

// The packed byte that stands for a pass in a position: unit 0 targeting its own cell. A
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

// Nocturne Grove: the saved player party against two creatures.
extern uint8_t const PROGMEM BATTLE_GROVE_TILES[BATTLE_CELLS];
extern BattleEncounter const PROGMEM BATTLE_GROVE;

// Deploys the saved party and the encounter's enemies; zero-HP members do not deploy.
void battle_begin(BattleState& state, BattleEncounter const PROGMEM* encounter);
// Writes final ally HP back to the saved roster, without changing its classes or membership. After
// a victory, every member of the saved party, including one that fell, recovers AP up to its
// maximum.
LevelUpNotice battle_finish(BattleState const& state);
uint8_t battle_max_hp(BattleUnit const& unit);

// The living unit on a cell, or BATTLE_NO_UNIT. Defeated units occupy no cell.
uint8_t battle_unit_at(BattleState const& state, uint8_t cell);

// The cell one step from `cell` in `dir`. False when that step leaves the board.
bool battle_neighbor(uint8_t cell, DIRECTION dir, uint8_t& neighbor);

// The direction from `from` toward `to` along the axis with the larger difference, preferring the
// columns on a tie. For cells on one row or column it is the exact direction.
DIRECTION battle_direction(uint8_t from, uint8_t to);

// The HP one attack by `attacker` removes from `victim`. A victim below half its maximum HP takes
// the attacker's injured damage.
uint8_t battle_attack_damage(BattleUnit const& attacker, BattleUnit const& victim);
// The HP a starting special by `attacker` removes from `victim`: the normal damage plus the
// special's bonus for the victim's state.
uint8_t battle_special_damage(BattleUnit const& attacker, BattleUnit const& victim);
// The sprite row a unit is drawn with.
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

// Scratch for action lists, so no caller keeps a list on its own frame. It is battle_action_scratch
// in the runtime workspace (runtime_workspace.h), and it holds BATTLE_ACTION_CAPACITY actions and more.
inline constexpr uint8_t BATTLE_ACTION_SCRATCH_CAPACITY = 82;
static_assert(BATTLE_ACTION_SCRATCH_CAPACITY >= BATTLE_ACTION_CAPACITY, "a whole side's list fits");

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
// orthogonally adjacent to it, how many are defeated, and how much HP is lost in total. The
// destination is assumed empty, as for any legal move.
struct BattleEngagement {
    uint8_t hits, kills, damage;
};

uint8_t battle_engagement_damage(BattleUnit const& actor, uint8_t destination);

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
// every living enemy orthogonally adjacent to the destination takes engagement damage, once
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
// BATTLE_NO_UNIT applies a team-wide pass instead of a unit action.
void battle_apply(BattleState& state, BattleAction action);

// Whether a saved party member can make its starting special attack on the enemy the action names:
// it is alive, belongs to the side to act, its normal attack reaches that enemy, and its AP covers
// the special's cost. Only saved members have AP, so no other unit can.
bool battle_special_legal(BattleState const& state, BattleAction action);
// Resolves a legal special attack the way battle_apply resolves an attack, and first spends the
// special's AP from the saved member. An illegal action does nothing.
void battle_apply_special(BattleState& state, BattleAction action);

// Every living unit on the passing side loses BATTLE_MOVE_COST, which can defeat the whole team.
// It counts as one activation. Player and AI may pass voluntarily; settling passes automatically
// when no unit can move or attack. A move that would cost a unit its last HP is still a move.
void battle_pass(BattleState& state);

// Passes while the side to act has no legal action. Returns the number of passes. Every pass costs
// HP and no action restores any, so it always returns.
uint16_t battle_settle(BattleState& state);

uint8_t battle_living(BattleState const& state, BATTLE_TEAM team);

// Blocks until the battle is won or lost and its result is acknowledged with A, then returns that
// result. Story code calls it like dialogue_show. The AVM screen is in battle_screen.cpp, and host
// tests substitute their own definition.
BATTLE_RESULT battle_show(BattleEncounter const PROGMEM* encounter);

#endif
