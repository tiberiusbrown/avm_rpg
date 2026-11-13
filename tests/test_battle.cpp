#include "battle.h"
#include "battle_ai.h"
#include "battle_cursor.h"
#include "battle_motion.h"
#include "battle_reference.h"
#include "tile_ids.h"
#include "world_assets.h"
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

// Grove layout: the rogue at (2,1) and the archer at (1,2) for the party; the wolf at (6,1) and the
// brute at (5,2) for the enemy.
static constexpr uint8_t ROGUE = 0, ARCHER = 1, WOLF = 2, BRUTE = 3;

static uint8_t cell(unsigned x, unsigned y) {
    return (uint8_t)(y * BATTLE_COLUMNS + x);
}

static BattleState fresh() {
    BattleState state;
    battle_begin(state, &BATTLE_GROVE);
    return state;
}

// Moves a unit directly, so a test can build the position it needs.
static void place(BattleState& state, uint8_t unit, unsigned x, unsigned y) {
    state.units[unit].cell = cell(x, y);
}

static bool same_state(BattleState const& a, BattleState const& b) {
    return memcmp(&a, &b, sizeof(BattleState)) == 0;
}

// One unit of a custom board: its column, row, type, team, and HP.
struct Placement {
    unsigned x, y;
    BATTLE_UNIT_TYPE type;
    BATTLE_TEAM team;
    uint8_t hp;
};

// A running battle on the Grove field with exactly the listed units, in that index order.
static BattleState board(BATTLE_TEAM turn, std::initializer_list<Placement> units,
                         uint8_t activations = 0) {
    BattleState s{};
    s.field = BATTLEFIELD_GROVE;
    for(Placement const& p : units)
        s.units[s.unit_count++] = BattleUnit{cell(p.x, p.y), p.hp, p.type, p.team};
    s.turn = turn;
    s.activations = activations;
    s.ended = false;
    s.result = BATTLE_DEFEAT;
    return s;
}

static BattleAction act(uint8_t unit, unsigned x, unsigned y) {
    return BattleAction{unit, cell(x, y)};
}

static bool same_action(BattleAction a, BattleAction b) {
    return a.unit_index == b.unit_index && a.destination == b.destination;
}

// Deterministic pseudo-random choices, so every position the tests build is the same on every run.
static uint32_t random_seed = 2026;

static unsigned next_random(unsigned bound) {
    random_seed = random_seed * 1103515245u + 12345u;
    return (random_seed >> 16) % bound;
}

// The legal actions of a position, decoded in enumeration order.
struct ActionSet {
    uint8_t count, attacks;
    BattleAction actions[BATTLE_ACTION_CAPACITY];
};

static ActionSet legal_set(BattleState const& s) {
    ActionSet set{};
    uint8_t packed[BATTLE_ACTION_CAPACITY];
    set.count = battle_actions(s, packed, set.attacks);
    for(uint8_t i = 0; i < set.count; ++i)
        set.actions[i] = battle_unpack(packed[i]);
    return set;
}

static bool has_action(BattleState const& s, BattleAction action) {
    ActionSet set = legal_set(s);
    for(uint8_t i = 0; i < set.count; ++i)
        if(same_action(set.actions[i], action))
            return true;
    return false;
}

// The destinations one unit can reach, as a bit set over the 32 cells.
static uint32_t destinations(BattleState const& s, uint8_t unit) {
    uint8_t packed[BATTLE_UNIT_ACTION_CAPACITY];
    uint8_t attacks = 0;
    uint8_t count = battle_unit_actions(s, unit, packed, attacks);
    uint32_t cells = 0;
    for(uint8_t i = 0; i < count; ++i)
        cells |= 1u << battle_unpack(packed[i]).destination;
    return cells;
}

static uint32_t cells_of(std::initializer_list<uint8_t> list) {
    uint32_t cells = 0;
    for(uint8_t c : list)
        cells |= 1u << c;
    return cells;
}

// The position after `plies` random legal activations, starting from the Grove encounter with its
// activation count set to `start`. Pass-only plies happen when the side to act has no move.
static BattleState random_play(unsigned plies, unsigned start) {
    BattleState s = fresh();
    s.activations = (uint8_t)start;
    for(unsigned ply = 0; ply < plies && !s.ended; ++ply) {
        battle_settle(s);
        if(s.ended)
            break;
        ActionSet set = legal_set(s);
        if(set.count == 0)
            break;
        battle_apply(s, set.actions[next_random(set.count)]);
    }
    battle_settle(s);
    return s;
}

// A random running board: two to four units of random types, HP, and cells, with at least one
// living unit on each team, a random side to act, and a random activation count.
static BattleState random_board() {
    BattleState s{};
    s.field = BATTLEFIELD_GROVE;
    s.unit_count = (uint8_t)(2 + next_random(3));
    for(uint8_t i = 0; i < s.unit_count; ++i) {
        uint8_t c = 0;
        for(;;) {
            c = (uint8_t)next_random(BATTLE_CELLS);
            bool taken = false;
            for(uint8_t j = 0; j < i; ++j)
                taken = taken || s.units[j].cell == c;
            if(!taken)
                break;
        }
        BATTLE_TEAM team = i == 0   ? BATTLE_PARTY
                           : i == 1 ? BATTLE_ENEMY
                                    : (BATTLE_TEAM)next_random(2);
        s.units[i] = BattleUnit{c, (uint8_t)(1 + next_random(6)),
                                (BATTLE_UNIT_TYPE)next_random(BATTLE_UNIT_TYPE_COUNT), team};
    }
    s.turn = (BATTLE_TEAM)next_random(2);
    s.activations = (uint8_t)next_random(26);
    s.ended = false;
    s.result = BATTLE_DEFEAT;
    return s;
}

// --- Independent statement of the rules -------------------------------------------------------
// The oracle restates each archetype from the specification and walks cells by column and row. It
// shares no geometry with the rules, so the enumeration and legality checks are tested against it.

struct OracleType {
    bool knight;
    int move_range, attack_min, attack_max, damage;
    bool knockback;
};

static OracleType oracle_type(BATTLE_UNIT_TYPE type) {
    switch(type) {
        case BATTLE_TYPE_FIGHTER:
            return {false, 1, 1, 1, 2, false};
        case BATTLE_TYPE_ROGUE:
            return {true, 0, 1, 1, 2, false};
        case BATTLE_TYPE_ARCHER:
            return {false, 1, 2, 4, 2, false};
        case BATTLE_TYPE_SPEARMAN:
            return {false, 1, 1, 2, 2, false};
        case BATTLE_TYPE_WOLF:
            return {false, 2, 1, 1, 2, false};
        case BATTLE_TYPE_BRUTE:
            return {false, 1, 1, 1, 1, true};
        default:
            abort();
    }
}

static int oracle_occupant(BattleState const& s, int x, int y) {
    for(uint8_t i = 0; i < s.unit_count; ++i)
        if(s.units[i].hp && s.units[i].cell == cell((unsigned)x, (unsigned)y))
            return i;
    return -1;
}

// Whether every cell strictly between (x, y) and (x + dx, y + dy) on a straight line is empty.
static bool oracle_clear(BattleState const& s, int x, int y, int dx, int dy) {
    int distance = abs(dx) + abs(dy);
    int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    for(int k = 1; k < distance; ++k)
        if(oracle_occupant(s, x + k * sx, y + k * sy) >= 0)
            return false;
    return true;
}

static bool oracle_legal(BattleState const& s, uint8_t unit, uint8_t destination) {
    if(s.ended || unit >= s.unit_count || destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = s.units[unit];
    if(actor.hp == 0 || actor.team != s.turn)
        return false;
    int x = actor.cell % 8, y = actor.cell / 8;
    int dx = destination % 8 - x, dy = destination / 8 - y;
    OracleType rules = oracle_type(actor.type);
    int occupant = oracle_occupant(s, destination % 8, destination / 8);
    bool straight = (dx == 0) != (dy == 0);
    int distance = abs(dx) + abs(dy);
    if(occupant < 0) {
        if(rules.knight)
            return (abs(dx) == 1 && abs(dy) == 2) || (abs(dx) == 2 && abs(dy) == 1);
        return straight && distance <= rules.move_range && oracle_clear(s, x, y, dx, dy);
    }
    if(s.units[occupant].team == actor.team)
        return false;
    return straight && distance >= rules.attack_min && distance <= rules.attack_max &&
           oracle_clear(s, x, y, dx, dy);
}

// --- Data ----------------------------------------------------------------------------------

static void unit_definitions() {
    for(uint8_t t = 0; t < BATTLE_UNIT_TYPE_COUNT; ++t) {
        BattleUnitType const& type = BATTLE_UNIT_TYPES[t];
        OracleType rules = oracle_type((BATTLE_UNIT_TYPE)t);
        CHECK((type.movement_type == BATTLE_MOVEMENT_KNIGHT) == rules.knight);
        CHECK(type.move_range == rules.move_range);
        CHECK(type.attack_min == rules.attack_min && type.attack_max == rules.attack_max);
        CHECK(type.damage == rules.damage);
        CHECK((type.effect == BATTLE_EFFECT_KNOCKBACK) == rules.knockback);
        CHECK(type.sprite_row < ACTOR_ROW_COUNT);
        // Each archetype has artwork of its own.
        for(uint8_t u = 0; u < t; ++u)
            CHECK(BATTLE_UNIT_TYPES[u].sprite_row != type.sprite_row);
    }
    // People for the four human archetypes, the dog for the wolf, and the ogre for the brute.
    CHECK(BATTLE_UNIT_TYPES[BATTLE_TYPE_WOLF].sprite_row == 17);
    CHECK(BATTLE_UNIT_TYPES[BATTLE_TYPE_BRUTE].sprite_row == 25);
    CHECK(BATTLE_UNIT_TYPES[BATTLE_TYPE_ROGUE].sprite_row < 16);
    CHECK(BATTLE_UNIT_TYPES[BATTLE_TYPE_ARCHER].sprite_row < 16);

    // The copied state stays compact: no ability data is stored per unit.
    static_assert(sizeof(BattleUnit) == 4, "a unit is its cell, HP, type, and team");
    static_assert(sizeof(BattleState) == 22, "the copied battle state did not grow");
    static_assert(sizeof(BattleUnitType) == 7, "a unit type is seven bytes of program memory");
}

static void initial_layout() {
    BattleState s = fresh();
    CHECK(s.field == BATTLEFIELD_GROVE && BATTLE_GROVE.field == BATTLEFIELD_GROVE);
    CHECK(s.unit_count == 4);
    CHECK(s.turn == BATTLE_PARTY && !s.ended && s.activations == 0);

    struct Expected {
        uint8_t x, y, hp;
        BATTLE_TEAM team;
        BATTLE_UNIT_TYPE type;
    } const expected[4] = {
        {2, 1, 5, BATTLE_PARTY, BATTLE_TYPE_ROGUE},
        {1, 2, 4, BATTLE_PARTY, BATTLE_TYPE_ARCHER},
        {6, 1, 4, BATTLE_ENEMY, BATTLE_TYPE_WOLF},
        {5, 2, 6, BATTLE_ENEMY, BATTLE_TYPE_BRUTE},
    };
    for(uint8_t i = 0; i < 4; ++i) {
        CHECK(battle_column(s.units[i].cell) == expected[i].x &&
              battle_row(s.units[i].cell) == expected[i].y);
        CHECK(s.units[i].hp == expected[i].hp && s.units[i].team == expected[i].team);
        CHECK(s.units[i].type == expected[i].type);
        CHECK(battle_unit_at(s, s.units[i].cell) == i);
    }
    CHECK(battle_living(s, BATTLE_PARTY) == 2 && battle_living(s, BATTLE_ENEMY) == 2);

    // The arrangement shows the new mechanics at once. The archer has a clear line of four cells to
    // the brute, the wolf can run two cells toward the party, and the rogue can leap to the centre.
    CHECK(battle_can_attack(s, ARCHER, BRUTE) && battle_legal(s, act(ARCHER, 5, 2)));
    CHECK(destinations(s, ROGUE) & (1u << cell(4, 2)));
    BattleState enemy = s;
    enemy.turn = BATTLE_ENEMY;
    CHECK(battle_legal(enemy, act(WOLF, 4, 1)));

    // Begin discards any earlier state.
    s.activations = 9;
    s.ended = true;
    battle_begin(s, &BATTLE_GROVE);
    CHECK(!s.ended && s.activations == 0 && s.turn == BATTLE_PARTY);
    CHECK(s.units[BRUTE].hp == 6 && s.units[ROGUE].hp == 5);
}

// A unit's team is stored, not inferred from artwork, so a unit's team and sprite row are
// independent, and any archetype can fight on either side.
static void teams_are_explicit() {
    BattleUnit unit{0, 5, BATTLE_TYPE_ROGUE, BATTLE_PARTY};
    CHECK(unit.team == BATTLE_PARTY &&
          battle_sprite_row(unit) == BATTLE_UNIT_TYPES[BATTLE_TYPE_ROGUE].sprite_row);
    unit.type = BATTLE_TYPE_WOLF;
    CHECK(unit.team == BATTLE_PARTY && battle_sprite_row(unit) == 17);
    unit.team = BATTLE_ENEMY;
    CHECK(unit.team == BATTLE_ENEMY && battle_sprite_row(unit) == 17);

    // A wolf on the party's side acts on the party's turn and bites enemies, whatever its artwork.
    BattleState s = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                         {4, 1, BATTLE_TYPE_ROGUE, BATTLE_ENEMY, 5}});
    CHECK(battle_legal(s, act(0, 4, 1)) && battle_legal(s, act(0, 1, 1)));
    CHECK(!battle_legal(s, act(1, 3, 3)));
    s.turn = BATTLE_ENEMY;
    CHECK(battle_legal(s, act(1, 3, 1)) && battle_legal(s, act(1, 6, 0)));
}

static void battlefield_data() {
    static_assert(sizeof(BATTLE_GROVE_TILES) == BATTLE_CELLS, "exactly 32 terrain tiles");
    static_assert(BATTLE_COLUMNS == 8 && BATTLE_ROWS == 4, "the board is 8 by 4");

    // Fixed arrangement, row-major from the top left.
    uint8_t const expected[BATTLE_CELLS] = {
        TILE_GRASS_06,   TILE_GRASS_07, TILE_GRASS_06, TILE_FLOWERS_26, TILE_GRASS_06,
        TILE_GRASS_07,   TILE_GRASS_16, TILE_GRASS_06, TILE_GRASS_07,   TILE_GRASS_06,
        TILE_GRASS_16,   TILE_GRASS_06, TILE_GRASS_06, TILE_FLOWERS_27, TILE_GRASS_06,
        TILE_GRASS_07,   TILE_GRASS_06, TILE_GRASS_16, TILE_GRASS_07,   TILE_GRASS_06,
        TILE_FLOWERS_26, TILE_GRASS_06, TILE_GRASS_06, TILE_GRASS_07,   TILE_GRASS_07,
        TILE_GRASS_06,   TILE_GRASS_06, TILE_GRASS_06, TILE_GRASS_07,   TILE_GRASS_16,
        TILE_FLOWERS_27, TILE_GRASS_06,
    };
    BattleState s = fresh();
    CHECK(s.field < BATTLEFIELD_COUNT);
    Battlefield const& field = BATTLEFIELDS[s.field];
    CHECK(field.tileset == TILESET_WORLD);
    CHECK(field.tiles == BATTLE_GROVE_TILES);
    for(uint8_t i = 0; i < BATTLE_CELLS; ++i) {
        uint8_t tile = field.tiles[i];
        CHECK(tile == expected[i]);
        CHECK(tile < TILE_COUNT);
        CHECK(TILE_COLLISION[(uint16_t)((uint16_t)TILESET_WORLD << 8 | tile)] == 0);
    }
}

static void board_edges() {
    uint8_t out = 0;
    CHECK(!battle_neighbor(cell(0, 0), NORTH, out) && !battle_neighbor(cell(0, 0), WEST, out));
    CHECK(!battle_neighbor(cell(7, 3), SOUTH, out) && !battle_neighbor(cell(7, 3), EAST, out));
    CHECK(battle_neighbor(cell(7, 0), SOUTH, out) && out == cell(7, 1));
    CHECK(battle_neighbor(cell(6, 2), EAST, out) && out == cell(7, 2));
    CHECK(!battle_neighbor(cell(3, 3), STILL, out));

    CHECK(battle_direction(cell(3, 1), cell(3, 0)) == NORTH);
    CHECK(battle_direction(cell(3, 1), cell(6, 1)) == EAST);
    CHECK(battle_direction(cell(3, 1), cell(3, 3)) == SOUTH);
    CHECK(battle_direction(cell(3, 1), cell(1, 1)) == WEST);
    // A knight offset faces along its longer axis.
    CHECK(battle_direction(cell(3, 1), cell(5, 2)) == EAST);
    CHECK(battle_direction(cell(3, 1), cell(2, 3)) == SOUTH);
}

// --- Movement ------------------------------------------------------------------------------

static void fighter_movement() {
    // 1. A fighter moves exactly one orthogonal cell.
    BattleState s = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                         {7, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(destinations(s, 0) == cells_of({cell(3, 0), cell(4, 1), cell(3, 2), cell(2, 1)}));
    CHECK(!battle_legal(s, act(0, 5, 1)) && !battle_legal(s, act(0, 4, 2)));
    // The spearman, archer, and brute move the same way.
    for(BATTLE_UNIT_TYPE type : {BATTLE_TYPE_ARCHER, BATTLE_TYPE_SPEARMAN, BATTLE_TYPE_BRUTE}) {
        s.units[0].type = type;
        CHECK(destinations(s, 0) == cells_of({cell(3, 0), cell(4, 1), cell(3, 2), cell(2, 1)}));
    }
}

static void rogue_movement() {
    // 2. A rogue in the open reaches every knight-shaped cell on the board, and nothing else.
    BattleState s = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                         {7, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    uint32_t leaps =
        cells_of({cell(4, 3), cell(5, 0), cell(5, 2), cell(2, 3), cell(1, 0), cell(1, 2)});
    CHECK(destinations(s, 0) == leaps);
    // 3. No ordinary orthogonal move, of one cell or two.
    CHECK(!battle_legal(s, act(0, 3, 0)) && !battle_legal(s, act(0, 4, 1)) &&
          !battle_legal(s, act(0, 5, 1)) && !battle_legal(s, act(0, 3, 3)));
    // Near the middle of the board all eight leaps exist.
    place(s, 0, 3, 2);
    BattleState mid = board(BATTLE_PARTY, {{3, 2, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                           {7, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    uint8_t packed[BATTLE_UNIT_ACTION_CAPACITY];
    uint8_t attacks = 0;
    // From (3,2) the leaps two rows down leave the board, so six remain; from a 5-row-tall view all
    // eight are tried, and only on-board ones appear.
    CHECK(battle_unit_actions(mid, 0, packed, attacks) == 6 && attacks == 0);

    // 4. The rogue jumps over units, friendly and enemy, between it and its landing cell.
    BattleState over = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                            {4, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                            {3, 2, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                            {4, 2, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_legal(over, act(0, 5, 2)) && battle_legal(over, act(0, 4, 3)));
    // 5. It cannot land on an occupied cell: a friend there is illegal, and an enemy on a
    // knight-shaped cell is not an attack.
    BattleState land = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                            {5, 2, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                            {1, 0, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(land, act(0, 5, 2)) && !battle_legal(land, act(0, 1, 0)));
    CHECK(battle_legal(land, act(0, 5, 0)));
    battle_apply(land, act(0, 5, 0));
    CHECK(land.units[0].cell == cell(5, 0) && land.units[0].hp == 5 && land.activations == 1);
}

static void wolf_movement() {
    // 6. A wolf moves one or two cells straight.
    BattleState s = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                         {7, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(destinations(s, 0) == cells_of({cell(3, 0), cell(4, 1), cell(5, 1), cell(3, 2),
                                          cell(3, 3), cell(2, 1), cell(1, 1)}));
    battle_apply(s, act(0, 5, 1));
    CHECK(s.units[0].cell == cell(5, 1) && s.activations == 1 && s.turn == BATTLE_ENEMY);

    // 7. A unit on the first cell blocks the second, friend or foe, and it cannot run through.
    BattleState blocked = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                               {4, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                               {3, 2, BATTLE_TYPE_ARCHER, BATTLE_ENEMY, 4}});
    CHECK(!battle_legal(blocked, act(0, 5, 1)) && !battle_legal(blocked, act(0, 3, 3)));
    // The adjacent enemy archer is an attack, and the run west is still open.
    CHECK(battle_legal(blocked, act(0, 3, 2)) && battle_legal(blocked, act(0, 1, 1)));
    // A wolf cannot run into an enemy two cells away: that is neither a move nor an attack.
    BattleState far = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                           {5, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(far, act(0, 5, 1)) && battle_legal(far, act(0, 4, 1)));

    // 8. No change of direction: an L-shaped two-cell move and a diagonal are not moves.
    CHECK(!battle_legal(s, act(0, 6, 0)) && !battle_legal(s, act(0, 4, 2)));
    s.turn = BATTLE_PARTY;
    CHECK(!battle_legal(s, act(0, 6, 2)) && !battle_legal(s, act(0, 4, 0)));
}

static void board_boundaries() {
    // 9. Moves never wrap from one row's end to the next row's start.
    BattleState s = board(BATTLE_PARTY, {{7, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                         {0, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(s, act(0, 0, 2)) && !battle_legal(s, act(0, 1, 2)));
    CHECK(destinations(s, 0) ==
          cells_of({cell(7, 0), cell(7, 2), cell(7, 3), cell(6, 1), cell(5, 1)}));
    BattleState left = board(BATTLE_PARTY, {{0, 2, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 4},
                                            {7, 0, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(destinations(left, 0) == cells_of({cell(1, 0), cell(2, 1), cell(2, 3)}));
    // An archer at a row's end cannot shoot the next row's first cell.
    BattleState shot = board(BATTLE_PARTY, {{6, 0, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                            {0, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_can_attack(shot, 0, 1));
    // Cells past the last cell are off the board, and so is an unknown unit.
    CHECK(!battle_legal(s, act(0, 7, 4)) && !battle_legal(s, {0, 255}));
    CHECK(!battle_legal(s, {BATTLE_UNIT_CAPACITY, cell(7, 0)}));
}

// --- Attacks -------------------------------------------------------------------------------

static void melee_attacks() {
    // 10. A fighter strikes an adjacent enemy for 2 and stays put.
    BattleState s = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                         {4, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                         {3, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_legal(s, act(0, 4, 1)) && !battle_legal(s, act(0, 3, 3)));
    battle_apply(s, act(0, 4, 1));
    CHECK(s.units[1].hp == 3 && s.units[1].cell == cell(4, 1) && s.units[0].cell == cell(3, 1));
    CHECK(s.activations == 1 && s.turn == BATTLE_ENEMY);

    // 11. A rogue strikes adjacent enemies only, never one on a knight-shaped cell.
    BattleState r = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                         {3, 2, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                         {5, 2, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_legal(r, act(0, 3, 2)) && !battle_legal(r, act(0, 5, 2)));
    CHECK(battle_can_attack(r, 0, 1) && !battle_can_attack(r, 0, 2));
    battle_apply(r, act(0, 3, 2));
    CHECK(r.units[1].hp == 3 && r.units[0].cell == cell(3, 1));

    // 20. A wolf bites adjacent enemies only, even in a clear line two cells away.
    BattleState w = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                         {3, 0, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                         {5, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_can_attack(w, 0, 1) && !battle_can_attack(w, 0, 2));
    battle_apply(w, act(0, 3, 0));
    CHECK(w.units[1].hp == 3 && w.units[0].cell == cell(3, 1));
}

static void archer_attacks() {
    // 12. An archer shoots along a row or column at ranges 2, 3, and 4.
    for(unsigned range = 2; range <= 4; ++range) {
        BattleState row =
            board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                 {1 + range, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
        CHECK(battle_legal(row, act(0, 1 + range, 1)));
        battle_apply(row, act(0, 1 + range, 1));
        CHECK(row.units[1].hp == 3 && row.units[0].cell == cell(1, 1));
    }
    BattleState column = board(BATTLE_PARTY, {{2, 0, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                              {2, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_can_attack(column, 0, 1));

    // 13. An adjacent enemy cannot be shot.
    BattleState near = board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                            {2, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(near, act(0, 2, 1)) && !battle_can_attack(near, 0, 1));
    // 14. Nor one five cells away.
    BattleState beyond = board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                              {6, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(beyond, act(0, 6, 1)));
    // 15. Nor one on a diagonal, or off the lines entirely.
    BattleState diagonal = board(BATTLE_PARTY, {{1, 0, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                                {3, 2, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                                {4, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(diagonal, act(0, 3, 2)) && !battle_legal(diagonal, act(0, 4, 1)));

    // 16. Any living unit between blocks the shot, friend or foe; a defeated unit does not.
    BattleState friendly = board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                                {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                                {4, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(friendly, act(0, 4, 1)));
    BattleState hostile = board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                               {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                               {5, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_legal(hostile, act(0, 3, 1)) && !battle_legal(hostile, act(0, 5, 1)));
    hostile.units[1].hp = 0;
    CHECK(battle_legal(hostile, act(0, 5, 1)));
    BattleState up = board(BATTLE_PARTY, {{2, 3, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                          {2, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                          {2, 0, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_can_attack(up, 0, 2));
}

static void spearman_attacks() {
    // 17. A spearman thrusts at ranges 1 and 2 and stays put.
    BattleState s = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_SPEARMAN, BATTLE_PARTY, 5},
                                         {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                         {2, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_legal(s, act(0, 3, 1)) && battle_legal(s, act(0, 2, 3)));
    battle_apply(s, act(0, 2, 3));
    CHECK(s.units[2].hp == 3 && s.units[0].cell == cell(2, 1));

    // 18. The two-cell thrust needs the cell between to be empty.
    BattleState blocked = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_SPEARMAN, BATTLE_PARTY, 5},
                                               {2, 2, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                               {2, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(blocked, act(0, 2, 3)));
    // 19. Three cells is out of reach.
    BattleState beyond = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_SPEARMAN, BATTLE_PARTY, 5},
                                              {5, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(beyond, act(0, 5, 1)));
}

static void brute_attacks() {
    // 21. A brute deals exactly 1. 22. A surviving victim is pushed one cell straight away.
    BattleState s = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                         {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    battle_apply(s, act(0, 3, 1));
    CHECK(s.units[1].hp == 4 && s.units[1].cell == cell(4, 1));
    CHECK(s.units[0].cell == cell(2, 1) && s.activations == 1 && s.turn == BATTLE_ENEMY);
    // 25. Occupancy follows: the old cell is empty and the new one holds the victim.
    CHECK(battle_unit_at(s, cell(3, 1)) == BATTLE_NO_UNIT && battle_unit_at(s, cell(4, 1)) == 1);
    // Pushes go every way.
    BattleState north = board(BATTLE_PARTY, {{2, 2, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                             {2, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    battle_apply(north, act(0, 2, 1));
    CHECK(north.units[1].cell == cell(2, 0));
    BattleState west = board(BATTLE_PARTY, {{2, 2, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                            {1, 2, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    battle_apply(west, act(0, 1, 2));
    CHECK(west.units[1].cell == cell(0, 2));

    // 23. No push off the board, or into a unit of either side, and no collision damage.
    BattleState edge = board(BATTLE_PARTY, {{6, 0, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                            {7, 0, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    battle_apply(edge, act(0, 7, 0));
    CHECK(edge.units[1].cell == cell(7, 0) && edge.units[1].hp == 4);
    BattleState top = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                           {3, 0, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    battle_apply(top, act(0, 3, 0));
    CHECK(top.units[1].cell == cell(3, 0));
    BattleState wall = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                            {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                            {4, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                            {3, 2, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5}});
    battle_apply(wall, act(0, 3, 1));
    CHECK(wall.units[1].cell == cell(3, 1) && wall.units[1].hp == 4 && wall.units[2].hp == 5);
    BattleState ally = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                            {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                            {4, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5}});
    battle_apply(ally, act(0, 3, 1));
    CHECK(ally.units[1].cell == cell(3, 1) && ally.units[2].hp == 5);
    // A defeated unit's cell is empty, so a push may enter it.
    ally.units[2].hp = 0;
    ally.turn = BATTLE_PARTY;
    battle_apply(ally, act(0, 3, 1));
    CHECK(ally.units[1].cell == cell(4, 1) && ally.units[1].hp == 3);

    // 24. A victim the blow defeats is not pushed: it is removed where it stood.
    BattleState kill = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                            {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 1},
                                            {7, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    battle_apply(kill, act(0, 3, 1));
    CHECK(kill.units[1].hp == 0 && kill.units[1].cell == cell(3, 1));
    CHECK(battle_unit_at(kill, cell(3, 1)) == BATTLE_NO_UNIT &&
          battle_unit_at(kill, cell(4, 1)) == BATTLE_NO_UNIT);
}

// --- Core semantics ------------------------------------------------------------------------

static void activations_and_alternation() {
    // 26. Every special move or attack is one activation, and 27. sides alternate after each.
    struct Case {
        BattleState state;
        BattleAction action;
    };

    Case const cases[] = {
        {board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                              {7, 3, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 5, 2)},
        {board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                              {7, 3, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 5, 1)},
        {board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                              {5, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 5, 1)},
        {board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_SPEARMAN, BATTLE_PARTY, 4},
                              {3, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 3, 1)},
        {board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                              {2, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 2, 1)},
    };
    for(Case const& c : cases) {
        BattleState s = c.state;
        CHECK(battle_legal(s, c.action));
        battle_apply(s, c.action);
        CHECK(s.activations == 1 && s.turn == BATTLE_ENEMY && !s.ended);
        // The same unit may act again on its side's next turn.
        s.turn = BATTLE_PARTY;
        CHECK(battle_can_act(s));
    }

    BattleState g = fresh();
    battle_apply(g, act(ROGUE, 4, 2));
    CHECK(g.turn == BATTLE_ENEMY && !battle_legal(g, act(ARCHER, 1, 1)));
    battle_apply(g, act(WOLF, 4, 1));
    CHECK(g.turn == BATTLE_PARTY && g.activations == 2);
}

static void exhaustion() {
    // 28. Activations one to twenty are free; from the twenty-first the acting unit loses 1 HP.
    BattleState s = fresh();
    s.activations = 19;
    battle_apply(s, act(ROGUE, 4, 2));
    CHECK(s.activations == 20 && s.units[ROGUE].hp == 5);
    battle_apply(s, act(WOLF, 4, 1));
    CHECK(s.activations == 21 && s.units[WOLF].hp == 3 && s.units[BRUTE].hp == 6);

    // Exhaustion applies after a ranged attack and after a knockback, to the attacker only.
    BattleState shot = fresh();
    shot.activations = 20;
    battle_apply(shot, act(ARCHER, 5, 2));
    CHECK(shot.units[BRUTE].hp == 4 && shot.units[ARCHER].hp == 3);
    BattleState push = board(
        BATTLE_ENEMY,
        {{2, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5}, {3, 1, BATTLE_TYPE_BRUTE, BATTLE_ENEMY, 6}},
        20);
    battle_apply(push, act(1, 2, 1));
    CHECK(push.units[0].hp == 4 && push.units[0].cell == cell(1, 1) && push.units[1].hp == 5);

    // Exhaustion can remove the acting unit, and the battle goes on while a side remains.
    BattleState k = fresh();
    k.activations = 20;
    k.units[WOLF].hp = 1;
    k.turn = BATTLE_ENEMY;
    battle_apply(k, act(WOLF, 4, 1));
    CHECK(k.units[WOLF].hp == 0 && battle_unit_at(k, cell(4, 1)) == BATTLE_NO_UNIT && !k.ended);
}

static void forced_pass() {
    // An archer cornered by two adjacent enemies has no move and no shot, so its side must pass.
    BattleState s = board(BATTLE_PARTY, {{0, 0, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                         {1, 0, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                         {0, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_can_act(s));
    CHECK(battle_settle(s) == 1 && s.turn == BATTLE_ENEMY && s.activations == 1 &&
          s.units[0].hp == 4);
    // After exhaustion, the first living unit on the passing side pays one HP.
    BattleState p = fresh();
    p.activations = 20;
    battle_pass(p);
    CHECK(p.activations == 21 && p.units[ROGUE].hp == 4 && p.units[ARCHER].hp == 4);
    BattleState q = fresh();
    q.units[ROGUE].hp = 0;
    q.activations = 20;
    battle_pass(q);
    CHECK(q.units[ARCHER].hp == 3);
    // A side that can act is never passed, and an ended battle is not passed either.
    BattleState v = fresh();
    CHECK(battle_settle(v) == 0 && v.turn == BATTLE_PARTY && v.activations == 0);
    v.ended = true;
    CHECK(battle_settle(v) == 0 && v.activations == 0);
}

static void outcomes() {
    // 29. Killing the last enemy wins at once, with no exhaustion charge on the twenty-first
    // action, including a ranged kill.
    BattleState v = fresh();
    v.activations = 20;
    v.units[WOLF].hp = 0;
    v.units[BRUTE].hp = 2;
    battle_apply(v, act(ARCHER, 5, 2));
    CHECK(v.ended && v.result == BATTLE_VICTORY && v.units[ARCHER].hp == 4 && v.activations == 21);
    CHECK(!battle_can_act(v) && !battle_legal(v, act(ROGUE, 4, 2)));

    // A brute's blow that kills the last party member is a defeat.
    BattleState d = board(BATTLE_ENEMY, {{2, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 1},
                                         {3, 1, BATTLE_TYPE_BRUTE, BATTLE_ENEMY, 6}});
    battle_apply(d, act(1, 2, 1));
    CHECK(d.ended && d.result == BATTLE_DEFEAT && d.units[0].cell == cell(2, 1));

    // An enemy removed by its own exhaustion charge can end the battle in victory.
    BattleState x = fresh();
    x.activations = 20;
    x.units[BRUTE].hp = 0;
    x.units[WOLF].hp = 1;
    x.turn = BATTLE_ENEMY;
    battle_apply(x, act(WOLF, 4, 1));
    CHECK(x.ended && x.result == BATTLE_VICTORY);

    // Exhaustion that removes the last party member is a defeat.
    BattleState e = fresh();
    e.units[ARCHER].hp = 0;
    e.units[ROGUE].hp = 1;
    e.activations = 20;
    battle_apply(e, act(ROGUE, 4, 2));
    CHECK(e.ended && e.result == BATTLE_DEFEAT);
}

static void determinism_and_copies() {
    // 30. The same action on the same position always gives the same position.
    for(unsigned seed = 0; seed < 200; ++seed) {
        BattleState s = random_board();
        ActionSet set = legal_set(s);
        for(uint8_t i = 0; i < set.count; ++i) {
            BattleState a = s, b = s;
            battle_apply(a, set.actions[i]);
            battle_apply(b, set.actions[i]);
            CHECK(same_state(a, b));
        }
    }
    // 31. Copies are independent, including after a knockback.
    BattleState original = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                                {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    BattleState copy = original;
    battle_apply(copy, act(0, 3, 1));
    CHECK(original.units[1].cell == cell(3, 1) && original.units[1].hp == 5 &&
          original.turn == BATTLE_PARTY);
    CHECK(copy.units[1].cell == cell(4, 1) && copy.units[1].hp == 4 && copy.turn == BATTLE_ENEMY);
}

// --- Enumeration ---------------------------------------------------------------------------

// Checks every enumeration property at one position: each legal action appears exactly once,
// nothing illegal appears, attacks lead, encoding round-trips, per-unit lists agree, and the
// buffers are never overrun.
static void check_enumeration(BattleState const& s) {
    uint8_t packed[BATTLE_ACTION_CAPACITY + 8];
    memset(packed, 0xEE, sizeof(packed));
    uint8_t attacks = 0;
    uint8_t count = battle_actions(s, packed, attacks);
    CHECK(count <= BATTLE_ACTION_CAPACITY && attacks <= count);
    for(uint8_t i = count; i < sizeof(packed); ++i)
        CHECK(packed[i] == 0xEE);

    unsigned seen[BATTLE_UNIT_CAPACITY][BATTLE_CELLS] = {};
    for(uint8_t i = 0; i < count; ++i) {
        CHECK(packed[i] < 0x80);
        BattleAction action = battle_unpack(packed[i]);
        CHECK(battle_pack(action) == packed[i]);
        CHECK(action.unit_index < s.unit_count && action.destination < BATTLE_CELLS);
        CHECK(oracle_legal(s, action.unit_index, action.destination));
        CHECK(battle_legal(s, action));
        // Attacks come first, and each is an attack; the moves after them are moves.
        bool attack = battle_unit_at(s, action.destination) != BATTLE_NO_UNIT;
        CHECK(attack == (i < attacks));
        ++seen[action.unit_index][action.destination];
    }
    for(uint8_t u = 0; u < BATTLE_UNIT_CAPACITY; ++u)
        for(uint8_t c = 0; c < BATTLE_CELLS; ++c) {
            bool legal = oracle_legal(s, u, c);
            CHECK(seen[u][c] == (legal ? 1u : 0u));
            CHECK(battle_legal(s, {u, c}) == legal);
        }
    CHECK(battle_can_act(s) == (count != 0));

    // Per-unit lists hold the same actions, in the same order, within their own bound.
    unsigned per_unit = 0;
    for(uint8_t u = 0; u < s.unit_count; ++u) {
        uint8_t own[BATTLE_UNIT_ACTION_CAPACITY + 4];
        memset(own, 0xEE, sizeof(own));
        uint8_t own_attacks = 0;
        uint8_t own_count = battle_unit_actions(s, u, own, own_attacks);
        CHECK(own_count <= BATTLE_UNIT_ACTION_CAPACITY && own[BATTLE_UNIT_ACTION_CAPACITY] == 0xEE);
        for(uint8_t i = 0; i < own_count; ++i) {
            CHECK(battle_unpack(own[i]).unit_index == u);
            CHECK(seen[u][battle_unpack(own[i]).destination] == 1);
        }
        per_unit += own_count;
    }
    CHECK(per_unit == count);
    uint8_t none_attacks = 9;
    CHECK(battle_unit_actions(s, BATTLE_NO_UNIT, packed, none_attacks) == 0 && none_attacks == 0);
}

static void legal_action_enumeration() {
    check_enumeration(fresh());
    BattleState enemy = fresh();
    enemy.turn = BATTLE_ENEMY;
    check_enumeration(enemy);

    // The Grove opening: the archer shot at the brute is the only attack.
    ActionSet opening = legal_set(fresh());
    CHECK(opening.attacks == 1 && same_action(opening.actions[0], act(ARCHER, 5, 2)));

    // Different archetypes in the same cell generate different action sets.
    uint32_t sets[BATTLE_UNIT_TYPE_COUNT];
    for(uint8_t t = 0; t < BATTLE_UNIT_TYPE_COUNT; ++t) {
        BattleState s = board(BATTLE_PARTY, {{3, 1, (BATTLE_UNIT_TYPE)t, BATTLE_PARTY, 4},
                                             {3, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                             {6, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
        check_enumeration(s);
        sets[t] = destinations(s, 0);
    }
    // The stepping fighter and brute share moves and targets; the others differ from them and from
    // each other.
    CHECK(sets[BATTLE_TYPE_FIGHTER] == sets[BATTLE_TYPE_BRUTE]);
    uint8_t distinct[] = {BATTLE_TYPE_FIGHTER, BATTLE_TYPE_ROGUE, BATTLE_TYPE_ARCHER,
                          BATTLE_TYPE_SPEARMAN, BATTLE_TYPE_WOLF};
    for(uint8_t a = 0; a < 5; ++a)
        for(uint8_t b = (uint8_t)(a + 1); b < 5; ++b)
            CHECK(sets[distinct[a]] != sets[distinct[b]]);
    // The spearman reaches the enemy two cells south; the archer reaches the one three cells east.
    CHECK(sets[BATTLE_TYPE_SPEARMAN] & (1u << cell(3, 3)));
    CHECK(sets[BATTLE_TYPE_ARCHER] & (1u << cell(6, 1)));
    CHECK(!(sets[BATTLE_TYPE_FIGHTER] & (1u << cell(3, 3))));

    // Defeated units have no actions, and neither does a unit of the other side.
    BattleState dead = fresh();
    dead.units[ARCHER].hp = 0;
    check_enumeration(dead);
    CHECK(destinations(dead, ARCHER) == 0 && destinations(dead, WOLF) == 0);
    // Ended battles have no actions.
    BattleState over = fresh();
    over.ended = true;
    check_enumeration(over);

    // Three units of one side against one, of the archetypes with the most moves: nothing is
    // truncated, and every action is still listed once.
    BattleState crowd = board(BATTLE_PARTY, {{2, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                             {5, 2, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                             {4, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                             {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    check_enumeration(crowd);
    CHECK(legal_set(crowd).count > 2 * BATTLE_MOVE_LIMIT);
    // Four units on one side, an unreachable shape in play, still fits the buffer.
    BattleState four = board(BATTLE_PARTY, {{1, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                            {6, 1, BATTLE_TYPE_WOLF, BATTLE_PARTY, 4},
                                            {1, 2, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 4},
                                            {6, 2, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 4}});
    check_enumeration(four);

    // The capacity is the largest total over every split of the four slots, and each per-unit bound
    // holds for every archetype.
    static_assert(BATTLE_ACTION_CAPACITY == 32,
                  "four units of eight moves, or three with three attacks");
    static_assert(BATTLE_UNIT_ACTION_CAPACITY == 11, "eight moves and three attacks");
    for(uint8_t actors = 0; actors <= BATTLE_UNIT_CAPACITY; ++actors)
        CHECK(battle_action_bound(actors) <= BATTLE_ACTION_CAPACITY);

    // Random boards of every archetype and random play from the Grove opening.
    unsigned largest = 0;
    for(unsigned seed = 0; seed < 3000; ++seed) {
        BattleState s = random_board();
        check_enumeration(s);
        unsigned count = legal_set(s).count;
        if(count > largest)
            largest = count;
    }
    for(unsigned seed = 0; seed < 300; ++seed)
        check_enumeration(random_play(seed % 11, (seed * 7) % 26));
    CHECK(largest <= BATTLE_ACTION_CAPACITY);
    printf("largest random action list: %u of %u\n", largest, (unsigned)BATTLE_ACTION_CAPACITY);
}

static void action_encoding() {
    for(uint8_t u = 0; u < BATTLE_UNIT_CAPACITY; ++u)
        for(uint8_t c = 0; c < BATTLE_CELLS; ++c) {
            uint8_t packed = battle_pack({u, c});
            CHECK(packed < BATTLE_PACKED_PASS);
            CHECK(same_action(battle_unpack(packed), {u, c}));
        }
}

// --- Search ordering and scores -------------------------------------------------------------

static void search_ordering() {
    // Ordered lists hold the same actions, with every group in priority order.
    for(unsigned seed = 0; seed < 500; ++seed) {
        BattleState s = seed & 1 ? random_board() : random_play(seed % 11, (seed * 3) % 26);
        if(s.ended)
            continue;
        uint8_t packed[BATTLE_ACTION_CAPACITY];
        uint8_t attacks = 0;
        uint8_t count = battle_actions(s, packed, attacks);
        uint8_t ordered[BATTLE_ACTION_CAPACITY];
        memcpy(ordered, packed, count);
        battle_order_actions(s, ordered, count, attacks, true);
        unsigned previous = 0;
        for(uint8_t i = 0; i < count; ++i) {
            BattleAction action = battle_unpack(ordered[i]);
            unsigned priority = battle_search_priority(s, action);
            // A win and a kill share the first group, because they never occur together.
            unsigned group = priority == BATTLE_PRIORITY_WIN ? BATTLE_PRIORITY_KILL : priority;
            CHECK(group >= previous);
            previous = group;
            CHECK(memchr(packed, ordered[i], count) != nullptr);
            BATTLE_ACTION_KIND kind = battle_action_kind(s, action);
            CHECK((kind == BATTLE_ACTION_MOVE) == (i >= attacks));
            // The kind uses the attacker's own damage, so a brute's 1 HP blow kills only a 1 HP
            // victim.
            if(kind == BATTLE_ACTION_KILL || kind == BATTLE_ACTION_WIN)
                CHECK(s.units[battle_unit_at(s, action.destination)].hp <=
                      battle_attack_damage(s.units[action.unit_index]));
        }
    }
    BattleState brute = board(BATTLE_ENEMY, {{2, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 2},
                                             {3, 1, BATTLE_TYPE_BRUTE, BATTLE_ENEMY, 6}});
    CHECK(battle_action_kind(brute, act(1, 2, 1)) == BATTLE_ACTION_ATTACK);
    brute.units[0].hp = 1;
    CHECK(battle_action_kind(brute, act(1, 2, 1)) == BATTLE_ACTION_WIN);
}

static void terminal_scores() {
    // The enemy's winning blow leaves state.turn on the enemy; the score still names the enemy.
    BattleState win = board(BATTLE_ENEMY, {{2, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 2},
                                           {3, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}});
    battle_apply(win, act(1, 2, 1));
    CHECK(win.ended && win.result == BATTLE_DEFEAT && win.turn == BATTLE_ENEMY);
    CHECK(battle_score(win, BATTLE_ENEMY, 1) == BATTLE_SCORE_WIN - 1);
    CHECK(battle_score(win, BATTLE_PARTY, 1) == -(BATTLE_SCORE_WIN - 1));
    CHECK(battle_score(win, BATTLE_ENEMY, 1) > battle_score(win, BATTLE_ENEMY, 3));
    CHECK(battle_score(win, BATTLE_PARTY, 3) > battle_score(win, BATTLE_PARTY, 1));

    // Every static score is far below any terminal score, and the two sides' scores are exact
    // negations.
    unsigned largest = 0;
    for(unsigned seed = 0; seed < 500; ++seed) {
        BattleState r = seed & 1 ? random_board() : random_play(seed % 11, (seed * 5) % 26);
        if(r.ended)
            continue;
        int16_t enemy = battle_score(r, BATTLE_ENEMY, 0);
        CHECK(enemy == -battle_score(r, BATTLE_PARTY, 0));
        unsigned magnitude = (unsigned)(enemy < 0 ? -enemy : enemy);
        if(magnitude > largest)
            largest = magnitude;
    }
    CHECK(largest < 6000 && largest > 0);
    CHECK(BATTLE_SCORE_WIN - BATTLE_AI_DEPTH > (int16_t)largest);
}

static void attack_gaps() {
    // The gap counts the cells left to line up and come within range.
    CHECK(battle_attack_gap(BATTLE_TYPE_FIGHTER, 1, 0) == 0);
    CHECK(battle_attack_gap(BATTLE_TYPE_FIGHTER, 3, 2) == 4);
    CHECK(battle_attack_gap(BATTLE_TYPE_FIGHTER, 1, 1) == 1);
    // An archer wants distance: adjacent is one cell too close, and two to four cells in line is in
    // range.
    CHECK(battle_attack_gap(BATTLE_TYPE_ARCHER, 1, 0) == 1);
    CHECK(battle_attack_gap(BATTLE_TYPE_ARCHER, 0, 3) == 0);
    CHECK(battle_attack_gap(BATTLE_TYPE_ARCHER, 6, 1) == 3);
    CHECK(battle_attack_gap(BATTLE_TYPE_ARCHER, 0, 0) == 2);
    // A spearman threatens two cells away.
    CHECK(battle_attack_gap(BATTLE_TYPE_SPEARMAN, 2, 0) == 0 &&
          battle_attack_gap(BATTLE_TYPE_SPEARMAN, 3, 0) == 1);
}

// The enemy's utility changes by exactly the amounts its weights name, on positions where nothing
// else changes.
static void utility_weights() {
    BattleState base = board(BATTLE_ENEMY, {{0, 0, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                            {0, 3, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                            {6, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 4},
                                            {7, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 4}});
    int16_t start = battle_score(base, BATTLE_ENEMY, 0);
    BattleState hurt = base;
    hurt.units[0].hp = 3;
    CHECK(battle_score(hurt, BATTLE_ENEMY, 0) - start == 360);
    BattleState weakened = base;
    weakened.units[2].hp = 2;
    CHECK(battle_score(weakened, BATTLE_ENEMY, 0) - start == -180);
    // A stepping enemy one cell closer to attacking gains 20 for the gap sum and 20 more when it is
    // the nearest enemy. The fighter at (6,1) is four cells from (0,3)'s line, not nearer than
    // (7,3).
    BattleState closer = base;
    closer.units[3].cell = cell(6, 3);
    CHECK(battle_score(closer, BATTLE_ENEMY, 0) - start == 40);
    // A wolf covers two cells a move, so each cell of its gap is worth half as much: 10 for the sum
    // and 10 for the nearest enemy.
    BattleState wolves = base;
    wolves.units[3].type = BATTLE_TYPE_WOLF;
    BattleState wolf_closer = wolves;
    wolf_closer.units[3].cell = cell(6, 3);
    CHECK(battle_score(wolf_closer, BATTLE_ENEMY, 0) - battle_score(wolves, BATTLE_ENEMY, 0) == 20);

    // Strikes count by the real rules. A party archer and a party spearman in the same cell differ
    // only in reach, so the scores differ only in strikes: with the enemy to act, an archer shot in
    // reply at range three costs the enemy 60, and the spearman cannot reach.
    BattleState archer = board(BATTLE_ENEMY, {{1, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                              {4, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 4},
                                              {1, 3, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 4}});
    BattleState spearman = archer;
    spearman.units[0].type = BATTLE_TYPE_SPEARMAN;
    CHECK(battle_score(archer, BATTLE_ENEMY, 0) - battle_score(spearman, BATTLE_ENEMY, 0) == -60);
    // A unit in the line blocks the shot, so the two score the same.
    BattleState archer_blocked = archer;
    archer_blocked.units[2].cell = cell(2, 1);
    BattleState spearman_blocked = archer_blocked;
    spearman_blocked.units[0].type = BATTLE_TYPE_SPEARMAN;
    CHECK(battle_score(archer_blocked, BATTLE_ENEMY, 0) ==
          battle_score(spearman_blocked, BATTLE_ENEMY, 0));
    // An adjacent archer cannot shoot, so it threatens nothing either.
    BattleState archer_adjacent = archer;
    archer_adjacent.units[1].cell = cell(2, 1);
    BattleState spearman_adjacent = archer_adjacent;
    spearman_adjacent.units[0].type = BATTLE_TYPE_SPEARMAN;
    CHECK(battle_score(archer_adjacent, BATTLE_ENEMY, 0) -
              battle_score(spearman_adjacent, BATTLE_ENEMY, 0) ==
          60);
}

// --- Search correctness ----------------------------------------------------------------------

static void expect_same_as_reference(BattleState const& s, uint8_t depth) {
    BattleState before = s;
    BattleSearchResult fast = battle_search(s, depth);
    CHECK(same_state(before, s));
    uint64_t nodes = 0;
    BattleSearchResult slow = reference_search(s, depth, nodes);
    CHECK(fast.score == slow.score);
    CHECK(same_action(fast.action, slow.action));
    if(fast.action.unit_index != BATTLE_NO_UNIT)
        CHECK(battle_legal(s, fast.action));
    CHECK(battle_search_stats().completed == (s.ended ? 0 : depth));
}

static void negamax_matches_reference() {
    expect_same_as_reference(fresh(), BATTLE_AI_DEPTH);
    BattleState enemy = fresh();
    enemy.turn = BATTLE_ENEMY;
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
        expect_same_as_reference(enemy, depth);

    // Random play from the Grove opening and random boards of every archetype cover sprints, leaps,
    // ranged attacks, knockbacks, forced passes, and exhaustion.
    for(unsigned seed = 0; seed < 240; ++seed) {
        BattleState s = seed & 1 ? random_board() : random_play(seed % 11, (seed * 11) % 26);
        uint8_t deepest = seed % 4 == 0 ? BATTLE_AI_DEPTH : 3;
        for(uint8_t depth = 1; depth <= deepest; ++depth)
            expect_same_as_reference(s, depth);
        BattleAction chosen = battle_enemy_action(s);
        CHECK(battle_legal(s, chosen) || !battle_can_act(s) || s.ended);
    }
}

static void search_simulates_mechanics() {
    // A knockback that moves the last enemy out of a lethal line is found and scored correctly.
    BattleState push = board(BATTLE_PARTY, {{3, 1, BATTLE_TYPE_BRUTE, BATTLE_PARTY, 6},
                                            {4, 1, BATTLE_TYPE_SPEARMAN, BATTLE_ENEMY, 5},
                                            {0, 3, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4}});
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
        expect_same_as_reference(push, depth);

    // A ranged kill of the last enemy is a win one ply deep, and it comes first.
    BattleState shot = board(BATTLE_PARTY, {{0, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                            {4, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 2}});
    BattleSearchResult won = battle_search(shot, BATTLE_AI_DEPTH);
    CHECK(same_action(won.action, act(0, 4, 1)) && won.score == BATTLE_SCORE_WIN - 1);
    // The win ends the root's search at its first child.
    CHECK(battle_search_stats().nodes == 1);

    // A wolf sprint and a knight leap reach the last enemy for a kill on the next turn.
    BattleState sprint = board(BATTLE_ENEMY, {{2, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 2},
                                              {5, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}});
    expect_same_as_reference(sprint, 3);
    BattleState leap = board(BATTLE_ENEMY, {{2, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 2},
                                            {4, 3, BATTLE_TYPE_ROGUE, BATTLE_ENEMY, 4}});
    expect_same_as_reference(leap, 3);

    // A forced pass inside the tree: the cornered archer passes, and the search passes with it.
    BattleState cornered = board(BATTLE_PARTY, {{0, 0, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                                {1, 0, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                                {0, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    BattleSearchResult pass = battle_search(cornered, 2);
    CHECK(pass.action.unit_index == BATTLE_NO_UNIT);
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
        expect_same_as_reference(cornered, depth);
    BattleState enemy_view = cornered;
    enemy_view.turn = BATTLE_ENEMY;
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
        expect_same_as_reference(enemy_view, depth);

    // The contrived pass: the side to act has no living unit, and the pass ends the battle.
    BattleState contrived = fresh();
    contrived.turn = BATTLE_ENEMY;
    contrived.units[WOLF].hp = 0;
    contrived.units[BRUTE].hp = 0;
    BattleSearchResult lost = battle_search(contrived, BATTLE_AI_DEPTH);
    CHECK(lost.action.unit_index == BATTLE_NO_UNIT && lost.score == -(BATTLE_SCORE_WIN - 1));
    CHECK(battle_search_stats().nodes == 1 && battle_search_stats().leaves == 1);
}

static void exhaustion_search() {
    // A lone enemy with 1 HP loses its last HP on its next activation after the twentieth, so every
    // action loses, and the search reports that loss one ply deep.
    BattleState doomed = board(BATTLE_ENEMY,
                               {{0, 0, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                {0, 3, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                {7, 3, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 1}},
                               20);
    CHECK(battle_search(doomed, BATTLE_AI_DEPTH).score == -(BATTLE_SCORE_WIN - 1));
    BattleState early = doomed;
    early.activations = 19;
    CHECK(battle_search(early, BATTLE_AI_DEPTH).score > -(BATTLE_SCORE_WIN - 1));

    // Self-elimination is avoided: the 1 HP wolf would die to exhaustion if it acted, so the
    // healthy brute acts.
    BattleState choice = board(BATTLE_ENEMY,
                               {{0, 0, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                {0, 3, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                {5, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 1},
                                {5, 2, BATTLE_TYPE_BRUTE, BATTLE_ENEMY, 6}},
                               20);
    BattleAction spare = battle_enemy_action(choice);
    CHECK(spare.unit_index == 3 && battle_legal(choice, spare));
}

// The search explores a bounded tree, respects its depth, and counts what it visits.
static void search_bounds() {
    BattleState root = fresh();
    root.turn = BATTLE_ENEMY;
    BattleSearchResult deep = battle_search(root, 200);
    CHECK(battle_search_stats().deepest == BATTLE_AI_DEPTH &&
          battle_search_stats().completed == BATTLE_AI_DEPTH);
    CHECK(deep.action.unit_index != BATTLE_NO_UNIT && battle_legal(root, deep.action));
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth) {
        battle_search(root, depth);
        CHECK(battle_search_stats().deepest == depth);
    }
    // Depth one examines each legal action once, and each child is a leaf.
    unsigned legal = legal_set(root).count;
    battle_search(root, 1);
    CHECK(battle_search_stats().nodes == legal && battle_search_stats().leaves == legal);
    // Pruning visits fewer positions than the unpruned reference, and the counts repeat.
    battle_search(root, BATTLE_AI_DEPTH);
    BattleSearchStats first = battle_search_stats();
    battle_search(root, BATTLE_AI_DEPTH);
    BattleSearchStats second = battle_search_stats();
    CHECK(first.nodes == second.nodes && first.leaves == second.leaves);
    uint64_t unpruned = 0;
    reference_search(root, BATTLE_AI_DEPTH, unpruned);
    CHECK(first.nodes < unpruned && first.leaves <= first.nodes && first.leaves > 0);

    // The workspace: four plies of 6 bytes, an 80-byte shared action stack (four plies of a
    // two-on-two battle at 20 actions each), four killer bytes, and four 22-byte positions.
    CHECK(battle_search_workspace_bytes() == 4 * 6 + 4 + 80 + 4 * 22);
    printf(
        "search workspace %u bytes; enemy opening depth %u: %u nodes, %u leaves (unpruned %llu)\n",
        (unsigned)battle_search_workspace_bytes(), (unsigned)BATTLE_AI_DEPTH, (unsigned)first.nodes,
        (unsigned)first.leaves, (unsigned long long)unpruned);
}

// --- Budgeted deepening ----------------------------------------------------------------------

// The deepening search's result must be a completed iteration's: legal, and worth exactly the
// reference's best score at that depth.
static void expect_completed(BattleState const& s, BattleSearchResult result) {
    BattleSearchStats stats = battle_search_stats();
    if(stats.completed == 0) {
        CHECK(result.action.unit_index == BATTLE_NO_UNIT);
        return;
    }
    uint64_t nodes = 0;
    BattleSearchResult best = reference_search(s, stats.completed, nodes);
    CHECK(result.score == best.score);
    if(result.action.unit_index == BATTLE_NO_UNIT) {
        CHECK(!battle_can_act(s));
        return;
    }
    CHECK(battle_legal(s, result.action));
    CHECK(reference_action_value(s, result.action, stats.completed) == best.score);
}

static void budget_boundaries() {
    BattleState s = fresh();
    s.turn = BATTLE_ENEMY;
    BattleState untouched = s;

    // Unlimited: every depth completes, and each iteration is no smaller than the last.
    BattleSearchResult full = battle_think(s, BATTLE_AI_DEPTH, 0);
    BattleSearchStats all = battle_search_stats();
    CHECK(all.completed == BATTLE_AI_DEPTH && !all.exhausted && same_state(s, untouched));
    expect_completed(s, full);
    uint16_t n[BATTLE_AI_DEPTH];
    for(uint8_t d = 0; d < BATTLE_AI_DEPTH; ++d)
        n[d] = all.iteration_nodes[d];
    CHECK(all.nodes == n[0] + n[1] + n[2] + n[3]);
    // The deepened result matches a plain search of the same depth in score.
    CHECK(full.score == battle_search(s, BATTLE_AI_DEPTH).score);

    // A budget of exactly depth one completes depth one and does not start depth two.
    BattleSearchResult one = battle_think(s, BATTLE_AI_DEPTH, n[0]);
    CHECK(battle_search_stats().completed == 1 && battle_search_stats().nodes == n[0]);
    CHECK(!battle_search_stats().exhausted);
    expect_completed(s, one);
    // One node short of depth one completes nothing, and the enemy falls back to the greedy choice.
    battle_think(s, BATTLE_AI_DEPTH, (uint16_t)(n[0] - 1));
    CHECK(battle_search_stats().completed == 0 && battle_search_stats().exhausted);

    // Exactly the first three iterations completes three.
    uint16_t three = (uint16_t)(n[0] + n[1] + n[2]);
    BattleSearchResult third = battle_think(s, BATTLE_AI_DEPTH, three);
    CHECK(battle_search_stats().completed == 3);
    expect_completed(s, third);

    // Depth four is attempted only when the budget left covers the growth estimate. A budget that
    // passes the estimate but runs out inside depth four discards it, at the last node and halfway.
    uint16_t estimate = (uint16_t)(2 * n[2]);
    CHECK(n[3] > estimate);
    for(uint16_t extra : {(uint16_t)(n[3] - 1), estimate, (uint16_t)((estimate + n[3]) / 2)}) {
        BattleSearchResult cut = battle_think(s, BATTLE_AI_DEPTH, (uint16_t)(three + extra));
        BattleSearchStats stats = battle_search_stats();
        CHECK(stats.completed == 3 && stats.exhausted && stats.iteration_nodes[3] == extra);
        CHECK(stats.nodes == three + extra);
        CHECK(same_action(cut.action, third.action) && cut.score == third.score);
        CHECK(same_state(s, untouched));
    }
    // Below the estimate, depth four is never started.
    battle_think(s, BATTLE_AI_DEPTH, (uint16_t)(three + estimate - 1));
    CHECK(battle_search_stats().completed == 3 && !battle_search_stats().exhausted);
    CHECK(battle_search_stats().iteration_nodes[3] == 0);
    // Exhaustion inside depth two, partway through the root's children.
    battle_think(s, BATTLE_AI_DEPTH, (uint16_t)(n[0] + n[1] - 1));
    CHECK(battle_search_stats().completed == 1);

    // An immediate win stops the deepening at depth one.
    BattleState win = board(BATTLE_ENEMY, {{2, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 2},
                                           {3, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4},
                                           {7, 3, BATTLE_TYPE_BRUTE, BATTLE_ENEMY, 6}});
    BattleSearchResult quick = battle_think(win, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET);
    CHECK(same_action(quick.action, act(1, 2, 1)) && quick.score == BATTLE_SCORE_WIN - 1);
    CHECK(battle_search_stats().completed == 1 && battle_search_stats().nodes == 1);
    CHECK(same_action(battle_enemy_action(win), act(1, 2, 1)));

    // No legal action: the result is a pass, from a completed iteration.
    BattleState cornered = board(BATTLE_ENEMY, {{1, 0, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                                {0, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                                {0, 0, BATTLE_TYPE_ARCHER, BATTLE_ENEMY, 4}});
    BattleSearchResult none = battle_think(cornered, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET);
    CHECK(none.action.unit_index == BATTLE_NO_UNIT && battle_search_stats().completed >= 1);
    CHECK(battle_enemy_action(cornered).unit_index == BATTLE_NO_UNIT);
    // A zero-node budget is unlimited, and a budget too small for depth one leaves the greedy
    // choice.
    BattleState greedy = fresh();
    greedy.turn = BATTLE_ENEMY;
    battle_think(greedy, BATTLE_AI_DEPTH, 1);
    CHECK(battle_search_stats().completed == 0);

    // Over many positions and budgets, every result is a completed iteration's, and the counts
    // never exceed the budget.
    for(unsigned seed = 0; seed < 120; ++seed) {
        BattleState r = seed & 1 ? random_board() : random_play(seed % 11, (seed * 13) % 26);
        if(r.ended)
            continue;
        uint16_t budget = (uint16_t)(1 + next_random(400));
        BattleState copy = r;
        BattleSearchResult result = battle_think(r, BATTLE_AI_DEPTH, budget);
        CHECK(same_state(r, copy));
        CHECK(battle_search_stats().nodes <= budget);
        expect_completed(r, result);
        BattleSearchResult again = battle_think(r, BATTLE_AI_DEPTH, budget);
        CHECK(same_action(again.action, result.action) && again.score == result.score);
    }
}

// --- Behavior ------------------------------------------------------------------------------

static unsigned attack_gap_to_party(BattleState const& s, uint8_t unit, uint8_t from) {
    unsigned best = 99;
    for(uint8_t i = 0; i < s.unit_count; ++i) {
        BattleUnit const& u = s.units[i];
        if(!u.hp || u.team != BATTLE_PARTY)
            continue;
        unsigned dx = (unsigned)abs(battle_column(from) - battle_column(u.cell));
        unsigned dy = (unsigned)abs(battle_row(from) - battle_row(u.cell));
        unsigned gap = battle_attack_gap(s.units[unit].type, (uint8_t)dx, (uint8_t)dy);
        if(gap < best)
            best = gap;
    }
    return best;
}

// True when some party unit could attack the given unit in this position.
static bool threatened(BattleState const& s, uint8_t unit) {
    for(uint8_t i = 0; i < s.unit_count; ++i)
        if(s.units[i].team == BATTLE_PARTY && battle_can_attack(s, i, unit))
            return true;
    return false;
}

static void enemy_behavior() {
    // Wolf sprint: a lone wolf far from a fighter closes two cells in one move.
    BattleState sprint = board(BATTLE_ENEMY, {{0, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                              {7, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}});
    BattleAction run = battle_enemy_action(sprint);
    CHECK(run.unit_index == 1 && battle_unit_at(sprint, run.destination) == BATTLE_NO_UNIT);
    unsigned run_length =
        (unsigned)(abs(battle_column(run.destination) - 7) + abs(battle_row(run.destination) - 1));
    CHECK(run_length == 2);
    CHECK(attack_gap_to_party(sprint, 1, run.destination) + 2 ==
          attack_gap_to_party(sprint, 1, cell(7, 1)));

    // Brute knockback: the 1 HP fighter cannot escape, the party fighter beside it would finish it,
    // and the adjacent archer cannot shoot. The brute's blow pushes the party fighter away.
    BattleState knock = board(BATTLE_ENEMY, {{1, 1, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                             {0, 0, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                             {0, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 1},
                                             {1, 0, BATTLE_TYPE_BRUTE, BATTLE_ENEMY, 6}});
    CHECK(threatened(knock, 2));
    BattleAction shove = battle_enemy_action(knock);
    CHECK(same_action(shove, act(3, 1, 1)));
    BattleState shoved = knock;
    battle_apply(shoved, shove);
    CHECK(shoved.units[0].cell == cell(1, 2) && !threatened(shoved, 2));

    // Archer minimum range: an enemy fighter beside an archer strikes it rather than step into its
    // line of fire.
    BattleState close = board(BATTLE_ENEMY, {{2, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                             {0, 3, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                             {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 2},
                                             {7, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!threatened(close, 2));
    BattleAction stay = battle_enemy_action(close);
    BattleState after = close;
    battle_apply(after, stay);
    CHECK(!after.ended && !threatened(after, 2));
    CHECK(same_action(stay, act(2, 2, 1)));

    // Spearman reach: a 2 HP wolf two cells from a spearman leaves the thrust's reach.
    BattleState reach = board(BATTLE_ENEMY, {{2, 1, BATTLE_TYPE_SPEARMAN, BATTLE_PARTY, 5},
                                             {0, 3, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                             {4, 1, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 2},
                                             {7, 3, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(threatened(reach, 2));
    BattleAction away = battle_enemy_action(reach);
    BattleState moved = reach;
    battle_apply(moved, away);
    CHECK(!threatened(moved, 2));

    // Blocked line: an injured enemy in an archer's open line moves out of it or behind cover.
    BattleState open = board(BATTLE_ENEMY, {{0, 1, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                            {0, 3, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                            {3, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 2},
                                            {6, 2, BATTLE_TYPE_BRUTE, BATTLE_ENEMY, 6}});
    CHECK(threatened(open, 2));
    BattleState sheltered = open;
    battle_apply(sheltered, battle_enemy_action(open));
    CHECK(!threatened(sheltered, 2));
    // A friendly unit in the line already shelters it, so the archer's shot is not counted.
    BattleState covered = open;
    place(covered, 3, 2, 1);
    CHECK(!threatened(covered, 2));

    // Tactical aggression: a healthy enemy beside a hero attacks it rather than stepping back.
    BattleState adjacent = board(BATTLE_ENEMY, {{3, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                                {0, 3, BATTLE_TYPE_ARCHER, BATTLE_PARTY, 4},
                                                {4, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                                {7, 3, BATTLE_TYPE_WOLF, BATTLE_ENEMY, 4}});
    BattleAction strike = battle_enemy_action(adjacent);
    CHECK(battle_unit_at(adjacent, strike.destination) != BATTLE_NO_UNIT);
    // A kill is taken.
    BattleState finish = adjacent;
    finish.units[0].hp = 2;
    CHECK(same_action(battle_enemy_action(finish), act(2, 3, 1)));
    // With nothing in reach, the enemy closes in.
    BattleState far = board(BATTLE_ENEMY, {{1, 1, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                           {1, 2, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                           {6, 1, BATTLE_TYPE_FIGHTER, BATTLE_ENEMY, 5},
                                           {7, 2, BATTLE_TYPE_BRUTE, BATTLE_ENEMY, 6}});
    BattleAction advance = battle_enemy_action(far);
    CHECK(battle_unit_at(far, advance.destination) == BATTLE_NO_UNIT);
    CHECK(attack_gap_to_party(far, advance.unit_index, advance.destination) <
          attack_gap_to_party(far, advance.unit_index, far.units[advance.unit_index].cell));
}

static void party_search_uses_leaps() {
    // Rogue leap: the 2 HP enemy rogue is cornered at (7,3). The party rogue at (5,3) reaches the
    // cell beside it, (7,2), only by a knight leap: one orthogonal path to it passes the enemy and
    // the other the party's own fighter. The same search the enemy uses, run for the party, finds
    // that leap and the forced win within three plies, whatever the enemy does in between.
    BattleState wall = board(BATTLE_PARTY, {{5, 3, BATTLE_TYPE_ROGUE, BATTLE_PARTY, 5},
                                            {6, 2, BATTLE_TYPE_FIGHTER, BATTLE_PARTY, 5},
                                            {7, 3, BATTLE_TYPE_ROGUE, BATTLE_ENEMY, 2}});
    CHECK(legal_set(wall).attacks == 0);
    CHECK(battle_unit_at(wall, cell(7, 3)) == 2 &&
          battle_unit_at(wall, cell(5, 2)) == BATTLE_NO_UNIT);
    BattleSearchResult leap = battle_search(wall, 3);
    CHECK(same_action(leap.action, act(0, 7, 2)));
    CHECK(leap.score >= BATTLE_SCORE_WIN - 3);
    // Two plies cannot see the win, so the leap is found by searching through the enemy's reply.
    CHECK(battle_search(wall, 2).score < BATTLE_SCORE_WIN - 3);
}

// --- Presentation helpers --------------------------------------------------------------------

static unsigned ease_to(int16_t start, int16_t target) {
    int16_t rendered = start;
    unsigned frames = 0;
    while(rendered != target) {
        int16_t before = rendered;
        rendered = cursor_ease(rendered, target);
        CHECK(rendered != before);
        CHECK((int)(target - rendered) * (int)(target - before) >= 0);
        CHECK(++frames <= 16);
    }
    CHECK(cursor_pixel(rendered) == target / CURSOR_SUBPIXELS);
    return frames;
}

static void cursor_easing() {
    constexpr int16_t span = (int16_t)(BATTLE_COLUMNS * TILE * CURSOR_SUBPIXELS);
    for(int16_t start = 0; start <= span; ++start) {
        for(uint8_t column = 0; column < BATTLE_COLUMNS; ++column)
            ease_to(start, cursor_subpixel((int16_t)(column * TILE)));
        for(uint8_t row = 0; row < BATTLE_ROWS; ++row)
            ease_to(start, cursor_subpixel((int16_t)(row * TILE)));
    }
    CHECK(ease_to(0, cursor_subpixel(TILE)) == 9);
    CHECK(cursor_ease(255, 256) == 256 && cursor_ease(257, 256) == 256);
    CHECK(cursor_pixel(cursor_subpixel(5) + 7) == 5 && cursor_pixel(cursor_subpixel(5) + 8) == 6);
}

static void lunge_timing() {
    CHECK(battle_lunge_pixels(0) == 2 && battle_lunge_pixels(1) == BATTLE_LUNGE_PIXELS);
    for(uint8_t frame = 2; frame < 14; ++frame)
        CHECK(battle_lunge_pixels(frame) == BATTLE_LUNGE_PIXELS);
    CHECK(battle_lunge_pixels(14) == 2 && battle_lunge_pixels(15) == 0);
    CHECK(battle_lunge_pixels(16) == 0 && battle_lunge_pixels(255) == 0);
}

static void flash_timing() {
    unsigned hidden = 0, changes = 0;
    bool previous = battle_flash_hidden(0);
    CHECK(previous);
    for(uint8_t frame = 0; frame < BATTLE_FLASH_FRAMES; ++frame) {
        bool now = battle_flash_hidden(frame);
        hidden += now ? 1 : 0;
        changes += now != previous ? 1 : 0;
        previous = now;
    }
    CHECK(hidden == 24 && changes == 11);
    CHECK(!battle_flash_hidden(BATTLE_FLASH_FRAMES - 1));
}

static void target_frames() {
    CHECK(BATTLE_TARGET_FRAMES == CURSOR_FRAME_COUNT);
    CHECK(BATTLE_TARGET_ATTACK == BATTLE_TARGET_MOVE + BATTLE_TARGET_FRAMES);
    for(unsigned phase = 0; phase < 256; ++phase) {
        uint8_t move = battle_target_frame(BATTLE_TARGET_MOVE, (uint8_t)phase);
        uint8_t attack = battle_target_frame(BATTLE_TARGET_ATTACK, (uint8_t)phase);
        CHECK(move == BATTLE_TARGET_MOVE + phase % BATTLE_TARGET_FRAMES);
        CHECK(attack - BATTLE_TARGET_ATTACK == move - BATTLE_TARGET_MOVE);
        CHECK(attack >= BATTLE_TARGET_ATTACK && attack < BATTLE_TARGET_ATTACK + BATTLE_TARGET_FRAMES);
    }
}

int main() {
    unit_definitions();
    initial_layout();
    teams_are_explicit();
    battlefield_data();
    board_edges();
    fighter_movement();
    rogue_movement();
    wolf_movement();
    board_boundaries();
    melee_attacks();
    archer_attacks();
    spearman_attacks();
    brute_attacks();
    activations_and_alternation();
    exhaustion();
    forced_pass();
    outcomes();
    determinism_and_copies();
    action_encoding();
    legal_action_enumeration();
    search_ordering();
    terminal_scores();
    attack_gaps();
    utility_weights();
    negamax_matches_reference();
    search_simulates_mechanics();
    exhaustion_search();
    search_bounds();
    budget_boundaries();
    enemy_behavior();
    party_search_uses_leaps();
    cursor_easing();
    lunge_timing();
    flash_timing();
    target_frames();
    printf("PASS battle rules: %u checks\n", checks);
    return 0;
}
