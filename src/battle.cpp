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

// Sprite rows: heroes use rows 0 and 1, and the two creatures use rows 16 and 20.
constexpr BattleEncounter PROGMEM BATTLE_GROVE{
    BATTLEFIELD_GROVE,
    4,
    {
        {battle_cell(2, 1), 5, 0},
        {battle_cell(2, 2), 5, 1},
        {battle_cell(5, 1), 4, 16},
        {battle_cell(5, 2), 4, 20},
    },
};

void battle_begin(BattleState& state, BattleEncounter const PROGMEM* encounter) {
    state.field = encounter->field;
    state.unit_count = encounter->unit_count;
    for(uint8_t i = 0; i < BATTLE_UNIT_CAPACITY; ++i) {
        state.units[i].cell = encounter->units[i].cell;
        state.units[i].hp = encounter->units[i].hp;
        state.units[i].sprite_row = encounter->units[i].sprite_row;
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

bool battle_legal(BattleState const& state, BattleAction action) {
    if(state.ended || action.unit_index >= state.unit_count || action.destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = state.units[action.unit_index];
    if(!actor.hp || actor.team() != state.turn)
        return false;

    // Compare coordinates rather than cells, so a row wrap cannot look adjacent.
    int dx = (int)battle_column(action.destination) - battle_column(actor.cell);
    int dy = (int)battle_row(action.destination) - battle_row(actor.cell);
    if(dx < 0)
        dx = -dx;
    if(dy < 0)
        dy = -dy;
    if(dx + dy != 1)
        return false;

    uint8_t occupant = battle_unit_at(state, action.destination);
    return occupant == BATTLE_NO_UNIT || state.units[occupant].team() != actor.team();
}

bool battle_can_act(BattleState const& state) {
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp || unit.team() != state.turn)
            continue;
        uint8_t cell = unit.cell;
        for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
            uint8_t neighbor = 0;
            if(battle_neighbor(cell, dir, neighbor) && battle_legal(state, {i, neighbor}))
                return true;
        }
    }
    return false;
}

uint8_t battle_living(BattleState const& state, BATTLE_TEAM team) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i)
        if(state.units[i].hp && state.units[i].team() == team)
            ++count;
    return count;
}

// Ends the battle once a side has no living units. Returns whether the battle has ended.
static bool battle_check(BattleState& state) {
    if(state.ended)
        return true;
    if(!battle_living(state, BATTLE_ENEMY)) {
        state.ended = true;
        state.result = BATTLE_VICTORY;
    } else if(!battle_living(state, BATTLE_PARTY)) {
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
        target.hp =
            target.hp > BATTLE_ATTACK_DAMAGE ? (uint8_t)(target.hp - BATTLE_ATTACK_DAMAGE) : 0;
    }
    ++state.activations;

    if(battle_check(state))
        return;
    if(state.activations > BATTLE_EXHAUSTION_START)
        battle_exhaust(actor);
    if(battle_check(state))
        return;
    battle_next_turn(state);
}

void battle_pass(BattleState& state) {
    ++state.activations;
    if(state.activations > BATTLE_EXHAUSTION_START) {
        // The first living unit on the side pays, so the choice is deterministic.
        for(uint8_t i = 0; i < state.unit_count; ++i) {
            BattleUnit& unit = state.units[i];
            if(unit.hp && unit.team() == state.turn) {
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
