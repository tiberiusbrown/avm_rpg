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

// Heroes use people rows 0 and 1, and the creatures use rows 16 and 20. Every type takes one
// orthogonal step and strikes an adjacent enemy for 2 HP.
constexpr BattleUnitType PROGMEM BATTLE_UNIT_TYPES[BATTLE_UNIT_TYPE_COUNT] = {
    {0, BATTLE_MOVEMENT_ORTHOGONAL_ONE, BATTLE_ATTACK_ADJACENT, 2},
    {1, BATTLE_MOVEMENT_ORTHOGONAL_ONE, BATTLE_ATTACK_ADJACENT, 2},
    {16, BATTLE_MOVEMENT_ORTHOGONAL_ONE, BATTLE_ATTACK_ADJACENT, 2},
    {20, BATTLE_MOVEMENT_ORTHOGONAL_ONE, BATTLE_ATTACK_ADJACENT, 2},
};

constexpr BattleEncounter PROGMEM BATTLE_GROVE{
    BATTLEFIELD_GROVE,
    4,
    {
        {battle_cell(2, 1), 5, BATTLE_TYPE_HERO_A, BATTLE_PARTY},
        {battle_cell(2, 2), 5, BATTLE_TYPE_HERO_B, BATTLE_PARTY},
        {battle_cell(5, 1), 4, BATTLE_TYPE_CREATURE_A, BATTLE_ENEMY},
        {battle_cell(5, 2), 4, BATTLE_TYPE_CREATURE_B, BATTLE_ENEMY},
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
    if(battle_column(to) != battle_column(from))
        return battle_column(to) > battle_column(from) ? EAST : WEST;
    return battle_row(to) > battle_row(from) ? SOUTH : NORTH;
}

uint8_t battle_attack_damage(BattleUnit const& unit) {
    return BATTLE_UNIT_TYPES[unit.type].damage;
}

uint8_t battle_sprite_row(BattleUnit const& unit) {
    return BATTLE_UNIT_TYPES[unit.type].sprite_row;
}

// Whether two cells share an edge. Coordinates are compared, so a row wrap cannot look adjacent.
static bool battle_adjacent(uint8_t a, uint8_t b) {
    int dx = (int)battle_column(a) - battle_column(b);
    int dy = (int)battle_row(a) - battle_row(b);
    if(dx < 0)
        dx = -dx;
    if(dy < 0)
        dy = -dy;
    return dx + dy == 1;
}

// Whether the unit's movement pattern reaches a cell from where it stands.
static bool battle_reaches(BattleUnit const& actor, uint8_t destination) {
    switch(BATTLE_UNIT_TYPES[actor.type].movement_type) {
        case BATTLE_MOVEMENT_ORTHOGONAL_ONE:
            return battle_adjacent(actor.cell, destination);
    }
    return false;
}

// Whether the unit's attack pattern lets it strike another unit it has reached.
static bool battle_strikes(BattleUnit const& actor, BattleUnit const& victim) {
    switch(BATTLE_UNIT_TYPES[actor.type].attack_type) {
        case BATTLE_ATTACK_ADJACENT:
            return victim.team != actor.team;
    }
    return false;
}

bool battle_can_strike(BattleState const& state, uint8_t attacker, uint8_t victim) {
    BattleUnit const& actor = state.units[attacker];
    BattleUnit const& target = state.units[victim];
    return actor.hp && target.hp && battle_reaches(actor, target.cell) &&
           battle_strikes(actor, target);
}

bool battle_legal(BattleState const& state, BattleAction action) {
    if(state.ended || action.unit_index >= state.unit_count || action.destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = state.units[action.unit_index];
    if(!actor.hp || actor.team != state.turn || !battle_reaches(actor, action.destination))
        return false;

    uint8_t occupant = battle_unit_at(state, action.destination);
    return occupant == BATTLE_NO_UNIT || battle_strikes(actor, state.units[occupant]);
}

BattleActionCursor battle_actions(BattleState const& state) {
    return BattleActionCursor{0, 0, state.unit_count};
}

BattleActionCursor battle_unit_actions(BattleState const& state, uint8_t unit_index) {
    if(unit_index >= state.unit_count)
        return BattleActionCursor{0, 0, 0};
    return BattleActionCursor{unit_index, 0, (uint8_t)(unit_index + 1)};
}

bool battle_next_action(BattleState const& state, BattleActionCursor& cursor,
                        BattleAction& action) {
    while(cursor.unit < cursor.end) {
        BattleUnit const& unit = state.units[cursor.unit];
        while(cursor.direction < 4) {
            DIRECTION dir = (DIRECTION)cursor.direction;
            ++cursor.direction;
            uint8_t destination = 0;
            if(!battle_neighbor(unit.cell, dir, destination))
                continue;
            BattleAction candidate{cursor.unit, destination};
            if(battle_legal(state, candidate)) {
                action = candidate;
                return true;
            }
        }
        ++cursor.unit;
        cursor.direction = 0;
    }
    return false;
}

bool battle_can_act(BattleState const& state) {
    BattleActionCursor cursor = battle_actions(state);
    BattleAction action{};
    return battle_next_action(state, cursor, action);
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

BattleDestinations battle_destinations(BattleState const& state, uint8_t unit_index) {
    BattleDestinations result{0, 0};
    BattleActionCursor cursor = battle_unit_actions(state, unit_index);
    BattleAction action{};
    while(battle_next_action(state, cursor, action)) {
        uint8_t bit =
            (uint8_t)(1 << battle_direction(state.units[unit_index].cell, action.destination));
        if(battle_unit_at(state, action.destination) == BATTLE_NO_UNIT)
            result.moves = (uint8_t)(result.moves | bit);
        else
            result.attacks = (uint8_t)(result.attacks | bit);
    }
    return result;
}

// Whether the unit's movement pattern lets it step at all. Candidates are orthogonal neighbors, which are
// adjacent by construction, so only the pattern's permission is left to decide.
static bool battle_moves(BattleUnit const& actor) {
    switch(BATTLE_UNIT_TYPES[actor.type].movement_type) {
        case BATTLE_MOVEMENT_ORTHOGONAL_ONE:
            return true;
    }
    return false;
}

BattleActionMasks battle_action_masks(BattleState const& state) {
    BattleActionMasks masks{};
    if(state.ended)
        return masks;

    // Bit x of rows[team][y] marks a living unit of that team at column x of row y.
    uint8_t rows[2][BATTLE_ROWS] = {};
    uint8_t living[2] = {};
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp)
            continue;
        uint8_t row = battle_row(unit.cell);
        rows[unit.team][row] = (uint8_t)(rows[unit.team][row] | (1 << battle_column(unit.cell)));
        ++living[unit.team];
    }

    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& actor = state.units[i];
        if(!actor.hp || actor.team != state.turn || !battle_moves(actor))
            continue;
        BATTLE_TEAM foe = actor.team == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
        for(uint8_t d = 0; d < 4; ++d) {
            uint8_t destination = 0;
            if(!battle_neighbor(actor.cell, (DIRECTION)d, destination))
                continue;
            uint8_t column = battle_column(destination), row = battle_row(destination);
            if((rows[actor.team][row] >> column) & 1)
                continue; // A friend stands there, so the action is illegal.
            uint16_t bit = (uint16_t)(1u << (i * 4 + d));
            if(!((rows[foe][row] >> column) & 1)) {
                masks.kind[BATTLE_ACTION_MOVE] = (uint16_t)(masks.kind[BATTLE_ACTION_MOVE] | bit);
                continue;
            }
            // An enemy stands there. The attack pattern decides whether it can be struck.
            BattleUnit const& target = state.units[battle_unit_at(state, destination)];
            if(!battle_strikes(actor, target))
                continue;
            BATTLE_ACTION_KIND kind = BATTLE_ACTION_ATTACK;
            if(target.hp <= battle_attack_damage(actor))
                kind = living[foe] == 1 ? BATTLE_ACTION_WIN : BATTLE_ACTION_KILL;
            masks.kind[kind] = (uint16_t)(masks.kind[kind] | bit);
        }
    }
    return masks;
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

void battle_apply(BattleState& state, BattleAction action) {
    BattleUnit& actor = state.units[action.unit_index];
    uint8_t occupant = battle_unit_at(state, action.destination);
    if(occupant == BATTLE_NO_UNIT) {
        actor.cell = action.destination;
    } else {
        BattleUnit& target = state.units[occupant];
        uint8_t damage = battle_attack_damage(actor);
        target.hp = target.hp > damage ? (uint8_t)(target.hp - damage) : 0;
    }
    ++state.activations;

    if(battle_check(state))
        return;
    // Exhaustion is the only change left, so the sides are checked again only when it applies.
    if(state.activations > BATTLE_EXHAUSTION_START) {
        battle_exhaust(actor);
        if(battle_check(state))
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
