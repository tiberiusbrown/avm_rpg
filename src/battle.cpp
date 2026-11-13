#include "battle.h"
#include "tile_ids.h"

// Grass with a few flowers, in a fixed arrangement. Every tile here has zero collision.
constexpr uint8_t PROGMEM BATTLE_GROVE_TILES[BATTLE_CELLS] = {
    TILE_GRASS_06,   TILE_GRASS_07, TILE_GRASS_06, TILE_FLOWERS_26, TILE_GRASS_06,
    TILE_GRASS_07,   TILE_GRASS_16, TILE_GRASS_06, TILE_GRASS_07,   TILE_GRASS_06,
    TILE_GRASS_16,   TILE_GRASS_06, TILE_GRASS_06, TILE_FLOWERS_27, TILE_GRASS_06,
    TILE_GRASS_07,   TILE_GRASS_06, TILE_GRASS_16, TILE_GRASS_07,   TILE_GRASS_06,
    TILE_FLOWERS_26, TILE_GRASS_06, TILE_GRASS_06, TILE_GRASS_07,   TILE_GRASS_07,
    TILE_GRASS_06,   TILE_GRASS_06, TILE_GRASS_06, TILE_GRASS_07,   TILE_GRASS_16,
    TILE_FLOWERS_27, TILE_GRASS_06,
};

// The grove clearing, in the world tileset.
constexpr Battlefield PROGMEM BATTLEFIELDS[BATTLEFIELD_COUNT] = {
    {TILESET_WORLD, BATTLE_GROVE_TILES},
};

constexpr BattleUnitType PROGMEM BATTLE_UNIT_TYPES[BATTLE_UNIT_TYPE_COUNT] =
    BATTLE_UNIT_TYPE_INITIALIZER;

// Every type's moves fit the per-unit bound that sizes the action buffers, every attack range is a
// nonempty span starting at one cell or more, and a knight has no orthogonal range.
constexpr bool battle_types_fit() {
    for(BattleUnitType const& type : BATTLE_UNIT_DEFINITIONS) {
        uint8_t moves =
            type.movement_type == BATTLE_MOVEMENT_KNIGHT ? 8 : (uint8_t)(4 * type.move_range);
        if(moves > BATTLE_MOVE_LIMIT || type.attack_min < 1 || type.attack_min > type.attack_max)
            return false;
        if(type.movement_type == BATTLE_MOVEMENT_KNIGHT && type.move_range)
            return false;
    }
    return true;
}

static_assert(battle_types_fit(), "a unit type exceeds BATTLE_MOVE_LIMIT or has an empty range");

// Nocturne Grove: the party's rogue and archer against a wolf and a brute. The archer can shoot the
// brute at range four at once, the wolf can run at the party, and the rogue can leap toward the
// centre, where the brute can knock it back.
constexpr BattleEncounter PROGMEM BATTLE_GROVE{
    BATTLEFIELD_GROVE,
    4,
    {
        {battle_cell(2, 1), 5, BATTLE_TYPE_ROGUE, BATTLE_PARTY},
        {battle_cell(1, 2), 4, BATTLE_TYPE_ARCHER, BATTLE_PARTY},
        {battle_cell(6, 1), 4, BATTLE_TYPE_WOLF, BATTLE_ENEMY},
        {battle_cell(5, 2), 6, BATTLE_TYPE_BRUTE, BATTLE_ENEMY},
    },
};

void battle_begin(BattleState& state, BattleEncounter const PROGMEM* encounter) {
    state.field = encounter->field;
    state.unit_count = encounter->unit_count;
    for(uint8_t i = 0; i < BATTLE_UNIT_CAPACITY; ++i) {
        state.units[i].cell = encounter->units[i].cell;
        state.units[i].hp = encounter->units[i].hp;
        state.units[i].type = encounter->units[i].type;
        state.units[i].team = encounter->units[i].team;
    }
    state.turn = BATTLE_PARTY;
    state.activations = 0;
    state.ended = false;
    state.result = BATTLE_DEFEAT;
}

uint8_t battle_unit_at(BattleState const& state, uint8_t cell) {
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp && unit.cell == cell)
            return i;
    }
    return BATTLE_NO_UNIT;
}

bool battle_neighbor(uint8_t cell, DIRECTION dir, uint8_t& neighbor) {
    uint8_t x = cell % BATTLE_COLUMNS, y = cell / BATTLE_COLUMNS;
    switch(dir) {
        case NORTH:
            if(!y)
                return false;
            neighbor = (uint8_t)(cell - BATTLE_COLUMNS);
            return true;
        case EAST:
            if(x + 1 == BATTLE_COLUMNS)
                return false;
            neighbor = (uint8_t)(cell + 1);
            return true;
        case SOUTH:
            if(y + 1 == BATTLE_ROWS)
                return false;
            neighbor = (uint8_t)(cell + BATTLE_COLUMNS);
            return true;
        case WEST:
            if(!x)
                return false;
            neighbor = (uint8_t)(cell - 1);
            return true;
        default:
            return false;
    }
}

DIRECTION battle_direction(uint8_t from, uint8_t to) {
    int8_t dx = (int8_t)(battle_column(to) - battle_column(from));
    int8_t dy = (int8_t)(battle_row(to) - battle_row(from));
    int8_t ax = dx < 0 ? (int8_t)-dx : dx, ay = dy < 0 ? (int8_t)-dy : dy;
    if(ax >= ay && ax)
        return dx > 0 ? EAST : WEST;
    return dy > 0 ? SOUTH : NORTH;
}

uint8_t battle_attack_damage(BattleUnit const& unit) {
    return battle_rules(unit.type).damage;
}

uint8_t battle_sprite_row(BattleUnit const& unit) {
    return BATTLE_UNIT_TYPES[unit.type].sprite_row;
}

// The distance from `a` to `b` when they share a row or column, or zero when they share neither
// (or are the same cell).
static uint8_t battle_line_distance(uint8_t a, uint8_t b) {
    uint8_t ax = battle_column(a), ay = battle_row(a), bx = battle_column(b), by = battle_row(b);
    if(ay == by)
        return (uint8_t)(ax > bx ? ax - bx : bx - ax);
    if(ax == bx)
        return (uint8_t)(ay > by ? ay - by : by - ay);
    return 0;
}

// Only the units are examined, so the cost does not grow with the distance.
bool battle_line_blocked(BattleState const& state, uint8_t a, uint8_t b) {
    uint8_t lo = a < b ? a : b, hi = a < b ? b : a;
    bool same_row = battle_row(a) == battle_row(b);
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        uint8_t c = unit.cell;
        if(!unit.hp || c <= lo || c >= hi)
            continue;
        // Between two cells of a row lies every cell with a larger index and a smaller one; on a
        // column only the cells of the same column count.
        if(same_row || battle_column(c) == battle_column(a))
            return true;
    }
    return false;
}

bool battle_can_move_to(BattleState const& state, uint8_t unit_index, uint8_t destination) {
    if(unit_index >= state.unit_count || destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = state.units[unit_index];
    if(!actor.hp || battle_unit_at(state, destination) != BATTLE_NO_UNIT)
        return false;
    BattleUnitRules const& rules = battle_rules(actor.type);
    if(rules.movement_type == BATTLE_MOVEMENT_KNIGHT) {
        int8_t dx = (int8_t)(battle_column(destination) - battle_column(actor.cell));
        int8_t dy = (int8_t)(battle_row(destination) - battle_row(actor.cell));
        uint8_t product = (uint8_t)((dx < 0 ? -dx : dx) * (dy < 0 ? -dy : dy));
        return product == 2;
    }
    uint8_t distance = battle_line_distance(actor.cell, destination);
    return distance && distance <= rules.move_range &&
           !battle_line_blocked(state, actor.cell, destination);
}

bool battle_can_attack(BattleState const& state, uint8_t attacker, uint8_t victim) {
    BattleUnit const& actor = state.units[attacker];
    BattleUnit const& target = state.units[victim];
    if(!actor.hp || !target.hp || actor.team == target.team)
        return false;
    uint8_t distance = battle_line_distance(actor.cell, target.cell);
    if(!battle_in_attack_range(battle_rules(actor.type), distance))
        return false;
    return distance == 1 || !battle_line_blocked(state, actor.cell, target.cell);
}

bool battle_legal(BattleState const& state, BattleAction action) {
    if(state.ended || action.unit_index >= state.unit_count || action.destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = state.units[action.unit_index];
    if(!actor.hp || actor.team != state.turn)
        return false;
    uint8_t occupant = battle_unit_at(state, action.destination);
    if(occupant == BATTLE_NO_UNIT)
        return battle_can_move_to(state, action.unit_index, action.destination);
    return battle_can_attack(state, action.unit_index, occupant);
}

// Whether a cell holds a living unit, from one occupancy mask per row: bit x of rows[y].
static bool battle_occupied(uint8_t const* rows, uint8_t cell) {
    return (rows[battle_row(cell)] >> battle_column(cell)) & 1;
}

// Appends the empty cells a straight move reaches, nearest first, stopping at the first occupied
// cell.
static inline __attribute__((always_inline)) uint8_t battle_ray(uint8_t const* rows, uint8_t* out,
                                                                uint8_t count, uint8_t unit_bits,
                                                                uint8_t cell, int8_t delta,
                                                                uint8_t steps) {
    while(steps--) {
        cell = (uint8_t)(cell + delta);
        if(battle_occupied(rows, cell))
            break;
        out[count++] = (uint8_t)(unit_bits | cell);
    }
    return count;
}

// Appends one knight leap of (dx, dy) when it lands on the board on an empty cell. The offsets are
// immediates at each call, so leaps need no table.
static inline __attribute__((always_inline)) uint8_t battle_leap(uint8_t const* rows, uint8_t* out,
                                                                 uint8_t count, uint8_t unit_bits,
                                                                 uint8_t x, uint8_t y, int8_t dx,
                                                                 int8_t dy) {
    // Unsigned wrap turns a step off the left or top edge into a large value.
    uint8_t nx = (uint8_t)(x + dx), ny = (uint8_t)(y + dy);
    if(nx >= BATTLE_COLUMNS || ny >= BATTLE_ROWS)
        return count;
    uint8_t cell = battle_cell(nx, ny);
    if(!battle_occupied(rows, cell))
        out[count++] = (uint8_t)(unit_bits | cell);
    return count;
}

static uint8_t smaller(uint8_t a, uint8_t b) {
    return a < b ? a : b;
}

// The enumeration behind battle_actions and battle_unit_actions, for units first to end - 1. The
// row occupancy masks are four bytes on the stack, built once per call.
static uint8_t battle_enumerate(BattleState const& state, uint8_t first, uint8_t end, uint8_t* out,
                                uint8_t& attacks) {
    uint8_t count = 0;
    attacks = 0;
    if(state.ended)
        return 0;
    uint8_t rows[BATTLE_ROWS] = {};
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp)
            rows[battle_row(unit.cell)] =
                (uint8_t)(rows[battle_row(unit.cell)] | (1 << battle_column(unit.cell)));
    }

    // Attacks pair each acting unit with each opponent, so there are at most four pair checks. A
    // shot longer than one cell needs every cell between to be empty.
    for(uint8_t a = first; a < end; ++a) {
        BattleUnit const& actor = state.units[a];
        if(!actor.hp || actor.team != state.turn)
            continue;
        BattleUnitRules const& rules = battle_rules(actor.type);
        uint8_t lo = rules.attack_min, hi = rules.attack_max;
        for(uint8_t v = 0; v < state.unit_count; ++v) {
            BattleUnit const& target = state.units[v];
            if(!target.hp || target.team == actor.team)
                continue;
            uint8_t distance = battle_line_distance(actor.cell, target.cell);
            if(distance < lo || distance > hi)
                continue;
            if(distance > 1) {
                int8_t delta =
                    (int8_t)(battle_row(actor.cell) == battle_row(target.cell) ? 1
                                                                               : BATTLE_COLUMNS);
                if(target.cell < actor.cell)
                    delta = (int8_t)-delta;
                uint8_t c = (uint8_t)(actor.cell + delta);
                while(c != target.cell && !battle_occupied(rows, c))
                    c = (uint8_t)(c + delta);
                if(c != target.cell)
                    continue;
            }
            out[count++] = (uint8_t)(a << 5 | target.cell);
        }
    }
    attacks = count;

    for(uint8_t a = first; a < end; ++a) {
        BattleUnit const& actor = state.units[a];
        if(!actor.hp || actor.team != state.turn)
            continue;
        uint8_t x = battle_column(actor.cell), y = battle_row(actor.cell);
        uint8_t unit_bits = (uint8_t)(a << 5);
        BattleUnitRules const& rules = battle_rules(actor.type);
        if(rules.movement_type == BATTLE_MOVEMENT_KNIGHT) {
            // Clockwise from north-north-east.
            count = battle_leap(rows, out, count, unit_bits, x, y, 1, -2);
            count = battle_leap(rows, out, count, unit_bits, x, y, 2, -1);
            count = battle_leap(rows, out, count, unit_bits, x, y, 2, 1);
            count = battle_leap(rows, out, count, unit_bits, x, y, 1, 2);
            count = battle_leap(rows, out, count, unit_bits, x, y, -1, 2);
            count = battle_leap(rows, out, count, unit_bits, x, y, -2, 1);
            count = battle_leap(rows, out, count, unit_bits, x, y, -2, -1);
            count = battle_leap(rows, out, count, unit_bits, x, y, -1, -2);
            continue;
        }
        // North, east, south, and west, each as far as the range and the board edge allow.
        uint8_t range = rules.move_range;
        count = battle_ray(rows, out, count, unit_bits, actor.cell, -(int8_t)BATTLE_COLUMNS,
                           smaller(y, range));
        count = battle_ray(rows, out, count, unit_bits, actor.cell, 1,
                           smaller((uint8_t)(BATTLE_COLUMNS - 1 - x), range));
        count = battle_ray(rows, out, count, unit_bits, actor.cell, BATTLE_COLUMNS,
                           smaller((uint8_t)(BATTLE_ROWS - 1 - y), range));
        count = battle_ray(rows, out, count, unit_bits, actor.cell, -1, smaller(x, range));
    }
    return count;
}

uint8_t battle_actions(BattleState const& state, uint8_t* out, uint8_t& attacks) {
    return battle_enumerate(state, 0, state.unit_count, out, attacks);
}

uint8_t battle_unit_actions(BattleState const& state, uint8_t unit_index, uint8_t* out,
                            uint8_t& attacks) {
    if(unit_index >= state.unit_count) {
        attacks = 0;
        return 0;
    }
    return battle_enumerate(state, unit_index, (uint8_t)(unit_index + 1), out, attacks);
}

uint8_t battle_action_scratch[BATTLE_ACTION_SCRATCH_CAPACITY];

// One unit at a time, stopping at the first unit that can act. Kept out of line, so its loop stays
// off the battle screen's frames.
RPG_NOINLINE bool battle_can_act(BattleState const& state) {
    uint8_t attacks = 0;
    for(uint8_t u = 0; u < state.unit_count; ++u)
        if(state.units[u].team == state.turn &&
           battle_unit_actions(state, u, battle_action_scratch, attacks))
            return true;
    return false;
}

BATTLE_ACTION_KIND battle_action_kind(BattleState const& state, BattleAction action) {
    uint8_t occupant = battle_unit_at(state, action.destination);
    if(occupant == BATTLE_NO_UNIT)
        return BATTLE_ACTION_MOVE;
    BattleUnit const& victim = state.units[occupant];
    if(victim.hp > battle_attack_damage(state.units[action.unit_index]))
        return BATTLE_ACTION_ATTACK;
    return battle_living(state, victim.team) == 1 ? BATTLE_ACTION_WIN : BATTLE_ACTION_KILL;
}

uint8_t battle_living(BattleState const& state, BATTLE_TEAM team) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i)
        if(state.units[i].hp && state.units[i].team == team)
            ++count;
    return count;
}

// Ends the battle once a side has no living units. Returns whether the battle has ended. One pass
// counts both sides.
static bool battle_check(BattleState& state) {
    if(state.ended)
        return true;
    uint8_t living[2] = {};
    for(uint8_t i = 0; i < state.unit_count; ++i)
        if(state.units[i].hp)
            ++living[state.units[i].team];
    if(!living[BATTLE_ENEMY]) {
        state.ended = true;
        state.result = BATTLE_VICTORY;
    } else if(!living[BATTLE_PARTY]) {
        state.ended = true;
        state.result = BATTLE_DEFEAT;
    }
    return state.ended;
}

static void battle_next_turn(BattleState& state) {
    state.turn = state.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
}

static void battle_exhaust(BattleUnit& unit) {
    if(unit.hp)
        --unit.hp;
}

// Pushes a struck unit one cell directly away from its attacker when that cell is on the board and
// empty. Attacks are always along a row or column, so the push follows that line.
static void battle_knock_back(BattleState& state, BattleUnit const& actor, BattleUnit& target) {
    DIRECTION away = battle_direction(actor.cell, target.cell);
    uint8_t cell = target.cell;
    if(battle_neighbor(target.cell, away, cell) && battle_unit_at(state, cell) == BATTLE_NO_UNIT)
        target.cell = cell;
}

void battle_apply(BattleState& state, BattleAction action) {
    BattleUnit& actor = state.units[action.unit_index];
    uint8_t occupant = battle_unit_at(state, action.destination);
    // A running battle has living units on both sides, so only a removal can end it: the sides are
    // counted after a blow that defeats its victim and after an exhaustion charge that defeats the
    // actor.
    bool removed = false;
    if(occupant == BATTLE_NO_UNIT) {
        actor.cell = action.destination;
    } else {
        BattleUnit& target = state.units[occupant];
        BattleUnitRules const& rules = battle_rules(actor.type);
        uint8_t damage = rules.damage;
        target.hp = target.hp > damage ? (uint8_t)(target.hp - damage) : 0;
        removed = !target.hp;
        if(target.hp && rules.effect == BATTLE_EFFECT_KNOCKBACK)
            battle_knock_back(state, actor, target);
    }
    ++state.activations;

    if(removed && battle_check(state))
        return;
    if(state.activations > BATTLE_EXHAUSTION_START) {
        battle_exhaust(actor);
        if(!actor.hp && battle_check(state))
            return;
    }
    battle_next_turn(state);
}

void battle_pass(BattleState& state) {
    ++state.activations;
    if(state.activations > BATTLE_EXHAUSTION_START) {
        // The first living unit on the side pays, so the choice is deterministic.
        for(uint8_t i = 0; i < state.unit_count; ++i) {
            BattleUnit& unit = state.units[i];
            if(unit.hp && unit.team == state.turn) {
                battle_exhaust(unit);
                break;
            }
        }
    }
    if(battle_check(state))
        return;
    battle_next_turn(state);
}

uint8_t battle_settle(BattleState& state) {
    uint8_t passes = 0;
    while(!state.ended && !battle_can_act(state)) {
        battle_pass(state);
        ++passes;
    }
    return passes;
}
