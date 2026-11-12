#include "battle.h"
#include "battle_ai.h"
#include "battle_cursor.h"
#include "battle_motion.h"
#include "battle_reference.h"
#include "tile_ids.h"
#include "world_assets.h"
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

// Grove layout: heroes A and B at (2,1) and (2,2); creatures A and B at (5,1) and (5,2).
static constexpr uint8_t HERO_A = 0, HERO_B = 1, FOE_A = 2, FOE_B = 3;

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

// Deterministic pseudo-random choices, so every position the tests build is the same on every run.
static uint32_t random_seed = 2026;

static unsigned next_random(unsigned bound) {
    random_seed = random_seed * 1103515245u + 12345u;
    return (random_seed >> 16) % bound;
}

// The position after `plies` random legal activations, starting from the Grove encounter with its
// activation count set to `start`. Pass-only plies happen when the side to act has no move.
static BattleState random_position(unsigned plies, unsigned start) {
    BattleState s = fresh();
    s.activations = (uint8_t)start;
    for(unsigned ply = 0; ply < plies && !s.ended; ++ply) {
        battle_settle(s);
        if(s.ended)
            break;
        BattleAction options[16];
        unsigned count = 0;
        BattleActionCursor cursor = battle_actions(s);
        BattleAction action{};
        while(count < 16 && battle_next_action(s, cursor, action))
            options[count++] = action;
        if(count == 0)
            break;
        battle_apply(s, options[next_random(count)]);
    }
    battle_settle(s);
    return s;
}

// The Grove board with the given side to act and activation count. Tests then set the units they
// need.
static BattleState custom(BATTLE_TEAM turn, uint8_t activations) {
    BattleState s = fresh();
    s.turn = turn;
    s.activations = activations;
    return s;
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
        {2, 1, 5, BATTLE_PARTY, BATTLE_TYPE_HERO_A},
        {2, 2, 5, BATTLE_PARTY, BATTLE_TYPE_HERO_B},
        {5, 1, 4, BATTLE_ENEMY, BATTLE_TYPE_CREATURE_A},
        {5, 2, 4, BATTLE_ENEMY, BATTLE_TYPE_CREATURE_B},
    };

    for(uint8_t i = 0; i < 4; ++i) {
        CHECK(battle_column(s.units[i].cell) == expected[i].x &&
              battle_row(s.units[i].cell) == expected[i].y);
        CHECK(s.units[i].hp == expected[i].hp && s.units[i].team == expected[i].team);
        CHECK(s.units[i].type == expected[i].type);
    }

    // Placement never overlaps, and each living unit is found on its own cell.
    for(uint8_t i = 0; i < 4; ++i) {
        CHECK(battle_unit_at(s, s.units[i].cell) == i);
        for(uint8_t j = (uint8_t)(i + 1); j < 4; ++j)
            CHECK(s.units[i].cell != s.units[j].cell);
    }
    CHECK(battle_unit_at(s, cell(0, 0)) == BATTLE_NO_UNIT);
    CHECK(battle_living(s, BATTLE_PARTY) == 2 && battle_living(s, BATTLE_ENEMY) == 2);

    // Begin discards any earlier state.
    s.activations = 9;
    s.ended = true;
    battle_begin(s, &BATTLE_GROVE);
    CHECK(!s.ended && s.activations == 0 && s.turn == BATTLE_PARTY);
    CHECK(s.units[FOE_B].hp == 4 && s.units[HERO_A].hp == 5);
}

// Unit types carry the sprite row and combat values. The Grove encounter keeps the original
// artwork, HP, damage, and movement for each of its units.
static void unit_definitions() {
    CHECK(BATTLE_UNIT_TYPES[BATTLE_TYPE_HERO_A].sprite_row == 0);
    CHECK(BATTLE_UNIT_TYPES[BATTLE_TYPE_HERO_B].sprite_row == 1);
    CHECK(BATTLE_UNIT_TYPES[BATTLE_TYPE_CREATURE_A].sprite_row == 16);
    CHECK(BATTLE_UNIT_TYPES[BATTLE_TYPE_CREATURE_B].sprite_row == 20);
    for(uint8_t type = 0; type < BATTLE_UNIT_TYPE_COUNT; ++type) {
        CHECK(BATTLE_UNIT_TYPES[type].movement_type == BATTLE_MOVEMENT_ORTHOGONAL_ONE);
        CHECK(BATTLE_UNIT_TYPES[type].attack_type == BATTLE_ATTACK_ADJACENT);
        CHECK(BATTLE_UNIT_TYPES[type].damage == 2);
    }

    BattleState s = fresh();
    CHECK(battle_sprite_row(s.units[HERO_A]) == 0 && battle_sprite_row(s.units[HERO_B]) == 1);
    CHECK(battle_sprite_row(s.units[FOE_A]) == 16 && battle_sprite_row(s.units[FOE_B]) == 20);
    CHECK(battle_attack_damage(s.units[HERO_A]) == 2 && battle_attack_damage(s.units[FOE_B]) == 2);

    // Team is stored on the unit. Creature artwork on the party team is a party unit, and hero
    // artwork on the enemy team is an enemy unit, which the rules and the enumeration both honor.
    BattleState t = fresh();
    t.units[FOE_A].team = BATTLE_PARTY;
    place(t, FOE_A, 3, 1);
    t.units[HERO_A].team = BATTLE_ENEMY;
    CHECK(battle_sprite_row(t.units[FOE_A]) == 16 && t.units[FOE_A].team == BATTLE_PARTY);
    // On the party turn the creature-artwork unit acts, and the hero-artwork unit does not.
    CHECK(battle_legal(t, {FOE_A, cell(3, 0)}) && battle_legal(t, {FOE_A, cell(2, 1)}));
    CHECK(!battle_legal(t, {HERO_A, cell(2, 0)}));
    // On the enemy turn the roles swap, and the hero-artwork unit strikes the party's creature.
    t.turn = BATTLE_ENEMY;
    CHECK(!battle_legal(t, {FOE_A, cell(3, 0)}));
    CHECK(battle_legal(t, {HERO_A, cell(2, 0)}) && battle_legal(t, {HERO_A, cell(3, 1)}));
    CHECK(battle_can_strike(t, FOE_A, HERO_A) && battle_can_strike(t, HERO_A, FOE_A));

    // The AI reads combat values from the unit, never from the artwork. Swapping the two heroes'
    // artwork changes nothing about the decision, because their combat values are the same.
    BattleState enemy_turn = fresh();
    enemy_turn.turn = BATTLE_ENEMY;
    place(enemy_turn, HERO_A, 4, 1);
    BattleState swapped = enemy_turn;
    BATTLE_UNIT_TYPE first = swapped.units[HERO_A].type;
    swapped.units[HERO_A].type = swapped.units[HERO_B].type;
    swapped.units[HERO_B].type = first;
    BattleAction chosen = battle_enemy_action(enemy_turn);
    BattleAction chosen_swapped = battle_enemy_action(swapped);
    CHECK(chosen.unit_index == chosen_swapped.unit_index &&
          chosen.destination == chosen_swapped.destination);
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
    CHECK(field.tileset == TILESET_WORLD &&
          BATTLEFIELDS[BATTLE_GROVE.field].tileset == TILESET_WORLD);
    CHECK(field.tiles == BATTLE_GROVE_TILES);
    for(uint8_t i = 0; i < BATTLE_CELLS; ++i) {
        uint8_t tile = field.tiles[i];
        CHECK(tile == expected[i]);
        CHECK(tile < TILE_COUNT);
        // Every terrain tile is in the world tileset and has no collision.
        CHECK(TILE_COLLISION[(uint16_t)((uint16_t)TILESET_WORLD << 8 | tile)] == 0);
    }
    CHECK(field.tiles[cell(3, 0)] == TILE_FLOWERS_26 && field.tiles[cell(5, 1)] == TILE_FLOWERS_27);
}

static void board_edges() {
    uint8_t out = 0;
    CHECK(!battle_neighbor(cell(0, 0), NORTH, out) && !battle_neighbor(cell(0, 0), WEST, out));
    CHECK(!battle_neighbor(cell(7, 3), SOUTH, out) && !battle_neighbor(cell(7, 3), EAST, out));
    CHECK(battle_neighbor(cell(7, 0), SOUTH, out) && out == cell(7, 1));
    CHECK(battle_neighbor(cell(0, 1), NORTH, out) && out == cell(0, 0));
    CHECK(battle_neighbor(cell(6, 2), EAST, out) && out == cell(7, 2));
    CHECK(battle_neighbor(cell(1, 2), WEST, out) && out == cell(0, 2));
    CHECK(!battle_neighbor(cell(3, 3), STILL, out));

    CHECK(battle_direction(cell(3, 1), cell(3, 0)) == NORTH);
    CHECK(battle_direction(cell(3, 1), cell(4, 1)) == EAST);
    CHECK(battle_direction(cell(3, 1), cell(3, 2)) == SOUTH);
    CHECK(battle_direction(cell(3, 1), cell(2, 1)) == WEST);
}

static void movement_rules() {
    BattleState s = fresh();
    // Hero A at (2,1) moves to empty orthogonal neighbors.
    CHECK(battle_legal(s, {HERO_A, cell(2, 0)}));
    CHECK(battle_legal(s, {HERO_A, cell(3, 1)}));
    CHECK(battle_legal(s, {HERO_A, cell(1, 1)}));
    // South is hero B, so it is a friendly-occupied destination.
    CHECK(!battle_legal(s, {HERO_A, cell(2, 2)}));
    // Diagonals and two-step moves are not adjacent.
    CHECK(!battle_legal(s, {HERO_A, cell(1, 0)}));
    CHECK(!battle_legal(s, {HERO_A, cell(3, 0)}));
    CHECK(!battle_legal(s, {HERO_A, cell(1, 2)}));
    CHECK(!battle_legal(s, {HERO_A, cell(3, 2)}));
    CHECK(!battle_legal(s, {HERO_A, cell(2, 3)}));
    CHECK(!battle_legal(s, {HERO_A, cell(4, 1)}));

    // Row wrap is not adjacency: column 7 of one row is not next to column 0 of the next.
    place(s, HERO_A, 0, 1);
    CHECK(!battle_legal(s, {HERO_A, cell(7, 0)}));
    place(s, HERO_A, 7, 0);
    CHECK(!battle_legal(s, {HERO_A, cell(0, 1)}));

    // Cells past the last cell are off the board, and so is an unknown unit.
    place(s, HERO_A, 7, 3);
    CHECK(!battle_legal(s, {HERO_A, BATTLE_CELLS}));
    CHECK(!battle_legal(s, {HERO_A, 255}));
    CHECK(!battle_legal(s, {BATTLE_UNIT_CAPACITY, cell(2, 1)}));

    // A unit cannot move on the other team's turn.
    BattleState t = fresh();
    CHECK(!battle_legal(t, {FOE_A, cell(5, 0)}));
    t.turn = BATTLE_ENEMY;
    CHECK(battle_legal(t, {FOE_A, cell(5, 0)}) && !battle_legal(t, {HERO_A, cell(2, 0)}));
}

static void move_and_attack() {
    // A move into an empty cell.
    BattleState m = fresh();
    battle_apply(m, {HERO_A, cell(3, 1)});
    CHECK(m.units[HERO_A].cell == cell(3, 1) && m.units[HERO_A].hp == 5);
    CHECK(m.turn == BATTLE_ENEMY && m.activations == 1 && !m.ended);

    // An attack on an adjacent creature deals two damage, and the attacker stays where it is.
    BattleState a = fresh();
    place(a, HERO_A, 4, 1);
    CHECK(battle_legal(a, {HERO_A, cell(5, 1)}));
    battle_apply(a, {HERO_A, cell(5, 1)});
    CHECK(a.units[FOE_A].hp == 2 && a.units[FOE_A].cell == cell(5, 1));
    CHECK(a.units[HERO_A].cell == cell(4, 1) && a.units[HERO_A].hp == 5);
    CHECK(a.turn == BATTLE_ENEMY && a.activations == 1);

    // A creature on the next attack is removed from the board. The attacker does not move in.
    battle_apply(a, {FOE_A, cell(4, 1)});
    CHECK(a.units[HERO_A].hp == 3);
    a.turn = BATTLE_PARTY;
    battle_apply(a, {HERO_A, cell(5, 1)});
    CHECK(a.units[FOE_A].hp == 0);
    CHECK(battle_unit_at(a, cell(5, 1)) == BATTLE_NO_UNIT);
    CHECK(a.units[HERO_A].cell == cell(4, 1));
    CHECK(battle_living(a, BATTLE_ENEMY) == 1 && !a.ended);

    // The vacated cell is an ordinary empty destination again.
    a.turn = BATTLE_PARTY;
    CHECK(battle_legal(a, {HERO_A, cell(5, 1)}));

    // Damage saturates at zero, so a unit with one HP left is simply removed.
    BattleState z = fresh();
    z.units[FOE_B].hp = 1;
    place(z, HERO_B, 4, 2);
    battle_apply(z, {HERO_B, cell(5, 2)});
    CHECK(z.units[FOE_B].hp == 0 && battle_unit_at(z, cell(5, 2)) == BATTLE_NO_UNIT);
}

static void alternation() {
    BattleState s = fresh();
    CHECK(s.turn == BATTLE_PARTY);
    battle_apply(s, {HERO_A, cell(3, 1)});
    CHECK(s.turn == BATTLE_ENEMY);
    // A party unit has no action on the enemy's turn.
    CHECK(!battle_legal(s, {HERO_B, cell(3, 2)}));

    battle_apply(s, {FOE_B, cell(5, 3)});
    CHECK(s.turn == BATTLE_PARTY && s.activations == 2);
    // The same hero acts again on consecutive party turns.
    CHECK(battle_legal(s, {HERO_A, cell(2, 1)}));
    battle_apply(s, {HERO_A, cell(2, 1)});
    CHECK(s.units[HERO_A].cell == cell(2, 1) && s.turn == BATTLE_ENEMY);
    CHECK(s.activations == 3);
}

static void exhaustion() {
    // Activations one through twenty are free.
    BattleState s = fresh();
    s.activations = 19;
    battle_apply(s, {HERO_A, cell(3, 1)});
    CHECK(s.activations == 20 && s.units[HERO_A].hp == 5);

    // Activation twenty-one charges only the unit that acted.
    battle_apply(s, {FOE_B, cell(5, 3)});
    CHECK(s.activations == 21 && s.units[FOE_B].hp == 3);
    CHECK(s.units[FOE_A].hp == 4 && s.units[HERO_A].hp == 5 && s.units[HERO_B].hp == 5);

    // Exhaustion can remove the acting unit, and the battle goes on while a side remains.
    BattleState k = fresh();
    k.activations = 20;
    k.units[FOE_B].hp = 1;
    k.turn = BATTLE_ENEMY;
    battle_apply(k, {FOE_B, cell(5, 3)});
    CHECK(k.units[FOE_B].hp == 0 && battle_unit_at(k, cell(5, 3)) == BATTLE_NO_UNIT);
    CHECK(!k.ended);
}

static void forced_pass() {
    // Before exhaustion a pass costs nothing but still counts as an activation.
    BattleState s = fresh();
    battle_pass(s);
    CHECK(s.turn == BATTLE_ENEMY && s.activations == 1);
    CHECK(s.units[HERO_A].hp == 5 && s.units[FOE_A].hp == 4);

    // After exhaustion, the first living unit on the passing side pays one HP.
    BattleState p = fresh();
    p.activations = 20;
    battle_pass(p);
    CHECK(p.activations == 21 && p.units[HERO_A].hp == 4 && p.units[HERO_B].hp == 5);
    CHECK(p.units[FOE_A].hp == 4 && p.units[FOE_B].hp == 4 && p.turn == BATTLE_ENEMY);

    // The payer is the first living unit, so a defeated first unit shifts the cost along.
    BattleState q = fresh();
    q.units[HERO_A].hp = 0;
    q.activations = 20;
    battle_pass(q);
    CHECK(q.units[HERO_A].hp == 0 && q.units[HERO_B].hp == 4);

    // A side that can act is never passed, and an ended battle is not passed either.
    BattleState v = fresh();
    CHECK(battle_settle(v) == 0 && v.turn == BATTLE_PARTY && v.activations == 0);
    v.ended = true;
    CHECK(battle_settle(v) == 0 && v.activations == 0);
}

static void outcomes() {
    // Killing the last creature wins at once, and the attacker takes no exhaustion charge even
    // though this action is the twenty-first.
    BattleState v = fresh();
    v.activations = 20;
    v.units[FOE_A].hp = 0;
    v.units[FOE_B].hp = 2;
    place(v, HERO_A, 4, 2);
    battle_apply(v, {HERO_A, cell(5, 2)});
    CHECK(v.ended && v.result == BATTLE_VICTORY);
    CHECK(v.units[HERO_A].hp == 5 && v.activations == 21);
    CHECK(!battle_can_act(v) && !battle_legal(v, {HERO_A, cell(4, 1)}));

    // A creature that the exhaustion charge removes ends the battle as a victory.
    BattleState x = fresh();
    x.activations = 20;
    x.units[FOE_A].hp = 0;
    x.units[FOE_B].hp = 1;
    x.turn = BATTLE_ENEMY;
    battle_apply(x, {FOE_B, cell(5, 3)});
    CHECK(x.ended && x.result == BATTLE_VICTORY);

    // A creature's attack that removes the last party member ends the battle as a defeat.
    BattleState d = fresh();
    d.units[HERO_B].hp = 0;
    d.units[HERO_A].hp = 2;
    place(d, HERO_A, 4, 1);
    d.turn = BATTLE_ENEMY;
    battle_apply(d, {FOE_A, cell(4, 1)});
    CHECK(d.ended && d.result == BATTLE_DEFEAT);

    // Exhaustion that removes the last party member is also a defeat.
    BattleState e = fresh();
    e.units[HERO_B].hp = 0;
    e.units[HERO_A].hp = 1;
    e.activations = 20;
    battle_apply(e, {HERO_A, cell(3, 1)});
    CHECK(e.ended && e.result == BATTLE_DEFEAT);
    CHECK(!battle_legal(e, {HERO_A, cell(2, 1)}));
}

// Oracle for legality, written from the rules and not from battle_legal, so the enumeration is
// checked against an independent statement of what is legal.
static bool oracle_legal(BattleState const& s, uint8_t unit, uint8_t destination) {
    if(s.ended || unit >= s.unit_count || destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = s.units[unit];
    if(actor.hp == 0 || actor.team != s.turn)
        return false;
    int dx = (int)battle_column(destination) - (int)battle_column(actor.cell);
    int dy = (int)battle_row(destination) - (int)battle_row(actor.cell);
    if(dx < 0)
        dx = -dx;
    if(dy < 0)
        dy = -dy;
    if(dx + dy != 1)
        return false;
    for(uint8_t i = 0; i < s.unit_count; ++i) {
        BattleUnit const& other = s.units[i];
        if(other.hp && other.cell == destination && other.team == actor.team)
            return false;
    }
    return true;
}

// Checks every enumeration property at one position: each legal action appears exactly once,
// nothing illegal appears, ownership and eliminated units are respected, and per-unit cursors
// agree.
static void check_enumeration(BattleState const& s) {
    unsigned seen[BATTLE_UNIT_CAPACITY][BATTLE_CELLS] = {};
    unsigned total = 0;
    BattleActionCursor cursor = battle_actions(s);
    BattleAction action{};
    while(battle_next_action(s, cursor, action)) {
        CHECK(action.unit_index < s.unit_count && action.destination < BATTLE_CELLS);
        CHECK(oracle_legal(s, action.unit_index, action.destination));
        CHECK(battle_legal(s, action));
        CHECK(s.units[action.unit_index].hp > 0 && s.units[action.unit_index].team == s.turn);
        ++seen[action.unit_index][action.destination];
        ++total;
    }
    for(uint8_t u = 0; u < BATTLE_UNIT_CAPACITY; ++u) {
        for(uint8_t c = 0; c < BATTLE_CELLS; ++c)
            CHECK(seen[u][c] == (oracle_legal(s, u, c) ? 1u : 0u));
    }
    CHECK(battle_can_act(s) == (total != 0));

    unsigned per_unit = 0;
    for(uint8_t u = 0; u < s.unit_count; ++u) {
        BattleActionCursor own = battle_unit_actions(s, u);
        while(battle_next_action(s, own, action)) {
            CHECK(action.unit_index == u);
            ++per_unit;
        }
    }
    CHECK(per_unit == total);
}

static void legal_action_enumeration() {
    // The opening positions, both sides to act.
    BattleState party = fresh();
    check_enumeration(party);
    BattleState enemy = custom(BATTLE_ENEMY, 0);
    check_enumeration(enemy);

    // An adjacent pair: the attack is an action whose destination holds the enemy, which is the
    // same model as a move whose destination is empty.
    BattleState adjacent = fresh();
    place(adjacent, HERO_A, 4, 1);
    check_enumeration(adjacent);
    BattleActionCursor cursor = battle_unit_actions(adjacent, HERO_A);
    BattleAction action{};
    bool attack = false, move = false;
    while(battle_next_action(adjacent, cursor, action)) {
        attack = attack || (action.destination == cell(5, 1));
        move = move || (action.destination == cell(4, 0));
    }
    CHECK(attack && move);

    // Eliminated units have no actions, and neither does a unit on the other side.
    BattleState dead = fresh();
    dead.units[HERO_B].hp = 0;
    check_enumeration(dead);
    cursor = battle_unit_actions(dead, HERO_B);
    CHECK(!battle_next_action(dead, cursor, action));
    cursor = battle_unit_actions(dead, FOE_A);
    CHECK(!battle_next_action(dead, cursor, action));
    cursor = battle_unit_actions(dead, BATTLE_NO_UNIT);
    CHECK(!battle_next_action(dead, cursor, action));

    // Ended battles have no actions at all.
    BattleState over = fresh();
    over.ended = true;
    check_enumeration(over);

    // Random legal positions across both sides, including those near exhaustion.
    for(unsigned seed = 0; seed < 300; ++seed)
        check_enumeration(random_position(seed % 9, (seed * 7) % 26));
}

// Destination masks are what the screen draws. Each bit must agree with the legality rule.
static void destination_masks() {
    BattleState s = fresh();
    // Hero A at (2,1) can enter north, east, and west. Hero B is south, so there is no mark there.
    BattleDestinations d = battle_destinations(s, HERO_A);
    CHECK(d.moves == ((1 << NORTH) | (1 << EAST) | (1 << WEST)) && d.attacks == 0);

    // With an enemy to the east, that direction is an attack mark, not a move mark.
    place(s, HERO_A, 4, 1);
    d = battle_destinations(s, HERO_A);
    CHECK(d.attacks == (1 << EAST));
    CHECK(d.moves == ((1 << NORTH) | (1 << SOUTH) | (1 << WEST)));

    // No marks for no unit, for a unit off the side to act, or for a defeated unit.
    CHECK(battle_destinations(s, BATTLE_NO_UNIT).moves == 0);
    CHECK(battle_destinations(s, FOE_A).moves == 0 && battle_destinations(s, FOE_A).attacks == 0);
    BattleState dead = fresh();
    dead.units[HERO_A].hp = 0;
    CHECK(battle_destinations(dead, HERO_A).moves == 0 &&
          battle_destinations(dead, HERO_A).attacks == 0);

    // Every mark on random positions is a legal action in its direction, and every legal action
    // has a mark.
    for(unsigned seed = 0; seed < 200; ++seed) {
        BattleState r = random_position(seed % 9, (seed * 3) % 26);
        for(uint8_t u = 0; u < r.unit_count; ++u) {
            BattleDestinations marks = battle_destinations(r, u);
            for(int dir = NORTH; dir <= WEST; ++dir) {
                uint8_t neighbor = 0;
                bool exists = battle_neighbor(r.units[u].cell, (DIRECTION)dir, neighbor);
                bool legal = exists && oracle_legal(r, u, neighbor);
                bool move = legal && battle_unit_at(r, neighbor) == BATTLE_NO_UNIT;
                bool strike = legal && !move;
                CHECK(((marks.moves >> dir) & 1) == (move ? 1 : 0));
                CHECK(((marks.attacks >> dir) & 1) == (strike ? 1 : 0));
            }
        }
    }
}

// The earlier greedy choices, kept as a baseline. The same assertions hold for the greedy choice
// that the search falls back on.
static void greedy_choices() {
    // Kills beat attacks, even when the kill comes later in unit order.
    BattleState kill = fresh();
    kill.turn = BATTLE_ENEMY;
    place(kill, HERO_A, 4, 1);
    place(kill, HERO_B, 4, 2);
    kill.units[HERO_B].hp = 2;
    BattleAction first = battle_greedy_action(kill);
    CHECK(first.unit_index == FOE_B && first.destination == cell(4, 2));
    CHECK(battle_legal(kill, first));

    // An attack beats any move.
    BattleState attack = fresh();
    attack.turn = BATTLE_ENEMY;
    place(attack, HERO_A, 4, 1);
    BattleAction hit = battle_greedy_action(attack);
    CHECK(hit.unit_index == FOE_A && hit.destination == cell(4, 1));

    // With no attack, the move that closes most distance wins. Creature A's move west and
    // creature B's move west both leave distance two. The tie goes to the lower unit index.
    BattleState approach = fresh();
    approach.turn = BATTLE_ENEMY;
    BattleAction step = battle_greedy_action(approach);
    CHECK(step.unit_index == FOE_A && step.destination == cell(4, 1));
    CHECK(battle_legal(approach, step));
    CHECK(battle_greedy_action(approach).destination == step.destination);

    // The choice does not depend on the state's history beyond its position.
    BattleState copy = approach;
    battle_apply(copy, step);
    CHECK(approach.units[FOE_A].cell == cell(5, 1) && approach.activations == 0);
    CHECK(copy.units[FOE_A].cell == cell(4, 1) && copy.activations == 1);

    // The greedy score ranks a kill above an attack above a move.
    BattleAction move{FOE_A, cell(4, 1)};
    CHECK(battle_greedy_score(attack, hit) > battle_greedy_score(approach, move));
    CHECK(battle_greedy_score(kill, first) > battle_greedy_score(attack, hit));
}

static void copies_are_independent() {
    BattleState original = fresh();
    BattleState copy = original;
    battle_apply(copy, {HERO_A, cell(3, 1)});
    CHECK(original.units[HERO_A].cell == cell(2, 1) && original.turn == BATTLE_PARTY);
    CHECK(copy.units[HERO_A].cell == cell(3, 1) && copy.turn == BATTLE_ENEMY);
}

// Negamax tactics: the search finds wins and avoids losing replies.
static void negamax_tactics() {
    // An enemy kill that ends the battle is chosen at once, and it is scored as a win one ply deep.
    BattleState win = custom(BATTLE_ENEMY, 0);
    win.units[HERO_B].hp = 0;
    win.units[HERO_A].hp = 2;
    place(win, HERO_A, 2, 1);
    place(win, FOE_A, 3, 1);
    win.units[FOE_B].hp = 0;
    BattleSearchResult won = battle_search(win, BATTLE_AI_DEPTH);
    CHECK(won.action.unit_index == FOE_A && won.action.destination == cell(2, 1));
    CHECK(won.score == BATTLE_SCORE_WIN - 1);

    // A creature with 2 HP next to a 5-HP hero can strike it, but the hero's reply kills the
    // creature. The search must not take that strike. The creature's safe moves are north, east,
    // and south.
    BattleState safe = custom(BATTLE_ENEMY, 0);
    place(safe, HERO_A, 1, 1);
    place(safe, FOE_A, 2, 1);
    safe.units[HERO_B].hp = 0;
    safe.units[FOE_B].hp = 0;
    safe.units[FOE_A].hp = 2;
    BattleSearchResult avoided = battle_search(safe, BATTLE_AI_DEPTH);
    CHECK(avoided.score > -BATTLE_SCORE_WIN + BATTLE_AI_DEPTH);
    CHECK(battle_legal(safe, avoided.action));
    CHECK(!(avoided.action.unit_index == FOE_A && avoided.action.destination == cell(1, 1)));
}

// Terminal scores take their sign from the winner, not from state.turn, and they depend on ply.
static void negamax_scores() {
    // The enemy's winning kill leaves state.turn on the enemy, because the battle ended before the
    // turn could pass. The winner is still the enemy, so the score is favorable to the enemy.
    BattleState win = custom(BATTLE_ENEMY, 0);
    win.units[HERO_B].hp = 0;
    win.units[HERO_A].hp = 2;
    place(win, HERO_A, 2, 1);
    place(win, FOE_A, 3, 1);
    win.units[FOE_B].hp = 0;
    battle_apply(win, {FOE_A, cell(2, 1)});
    CHECK(win.ended && win.result == BATTLE_DEFEAT && win.turn == BATTLE_ENEMY);
    CHECK(battle_score(win, BATTLE_ENEMY, 1) == BATTLE_SCORE_WIN - 1);
    CHECK(battle_score(win, BATTLE_PARTY, 1) == -(BATTLE_SCORE_WIN - 1));

    // A party victory is favorable to the party, and the turn stays with the party that won.
    BattleState victory = custom(BATTLE_PARTY, 0);
    victory.units[FOE_A].hp = 0;
    victory.units[FOE_B].hp = 2;
    place(victory, HERO_A, 4, 2);
    battle_apply(victory, {HERO_A, cell(5, 2)});
    CHECK(victory.ended && victory.result == BATTLE_VICTORY && victory.turn == BATTLE_PARTY);
    CHECK(battle_score(victory, BATTLE_PARTY, 1) == BATTLE_SCORE_WIN - 1);
    CHECK(battle_score(victory, BATTLE_ENEMY, 1) == -(BATTLE_SCORE_WIN - 1));

    // Faster wins score better, and delayed losses score better than faster ones.
    CHECK(battle_score(win, BATTLE_ENEMY, 1) > battle_score(win, BATTLE_ENEMY, 3));
    CHECK(battle_score(win, BATTLE_PARTY, 3) > battle_score(win, BATTLE_PARTY, 1));

    // Every static score is far below any terminal score, so a terminal result always dominates.
    unsigned largest = 0;
    for(unsigned seed = 0; seed < 300; ++seed) {
        BattleState r = random_position(seed % 9, (seed * 5) % 26);
        if(r.ended)
            continue;
        int16_t enemy = battle_score(r, BATTLE_ENEMY, 0);
        int16_t party = battle_score(r, BATTLE_PARTY, 0);
        CHECK(enemy == -party);
        int magnitude = enemy < 0 ? -enemy : enemy;
        if((unsigned)magnitude > largest)
            largest = (unsigned)magnitude;
    }
    CHECK(largest < 4000 && largest > 0);
    CHECK(BATTLE_SCORE_WIN - BATTLE_AI_DEPTH > (int16_t)largest);
}

static void expect_same_as_reference(BattleState const& s, uint8_t depth) {
    BattleState before = s;
    BattleSearchResult fast = battle_search(s, depth);
    CHECK(same_state(before, s));
    uint64_t nodes = 0;
    BattleSearchResult slow = reference_search(s, depth, nodes);
    CHECK(fast.score == slow.score);
    CHECK(fast.action.unit_index == slow.action.unit_index);
    CHECK(fast.action.destination == slow.action.destination);
    if(fast.action.unit_index != BATTLE_NO_UNIT)
        CHECK(battle_legal(s, fast.action));
}

// Alpha-beta and the explicit stack must select the same action and score as the unpruned search,
// at every depth, from many positions, including those near exhaustion.
static void negamax_matches_reference() {
    expect_same_as_reference(fresh(), BATTLE_AI_DEPTH);
    BattleState enemy = custom(BATTLE_ENEMY, 0);
    expect_same_as_reference(enemy, 1);
    expect_same_as_reference(enemy, 2);
    expect_same_as_reference(enemy, 3);
    expect_same_as_reference(enemy, BATTLE_AI_DEPTH);

    for(unsigned seed = 0; seed < 400; ++seed) {
        unsigned plies = seed % 9;
        unsigned start = (seed * 11) % 26;
        BattleState s = random_position(plies, start);
        if(s.ended)
            continue;
        for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
            expect_same_as_reference(s, depth);
        BattleAction chosen = battle_enemy_action(s);
        CHECK(battle_legal(s, chosen) || !battle_can_act(s));
    }
}

// Exhaustion is simulated. A lone creature with 1 HP must lose its last HP on its next activation
// after the twentieth, so every move loses it, and the search reports that loss one ply deep.
static void exhaustion_search() {
    BattleState doomed = custom(BATTLE_ENEMY, 20);
    place(doomed, HERO_A, 0, 0);
    place(doomed, HERO_B, 0, 3);
    doomed.units[FOE_A].hp = 0;
    place(doomed, FOE_B, 7, 3);
    doomed.units[FOE_B].hp = 1;
    BattleSearchResult lost = battle_search(doomed, BATTLE_AI_DEPTH);
    CHECK(lost.score == -(BATTLE_SCORE_WIN - 1));

    // One activation earlier, the same creature survives its first move, so the loss is not one ply
    // deep.
    BattleState early = doomed;
    early.activations = 19;
    BattleSearchResult later = battle_search(early, BATTLE_AI_DEPTH);
    CHECK(later.score > -(BATTLE_SCORE_WIN - 1));

    // Self-elimination is avoided. Creature A has 1 HP, and it would die to exhaustion if it acts.
    // Creature B has 5 HP and can take the cost, so the search must act with B.
    BattleState choice = custom(BATTLE_ENEMY, 20);
    place(choice, HERO_A, 0, 0);
    place(choice, HERO_B, 0, 3);
    place(choice, FOE_A, 5, 1);
    place(choice, FOE_B, 5, 2);
    choice.units[FOE_A].hp = 1;
    choice.units[FOE_B].hp = 5;
    BattleSearchResult spare = battle_search(choice, BATTLE_AI_DEPTH);
    CHECK(spare.action.unit_index == FOE_B);
    CHECK(battle_legal(choice, spare.action));
}

// A side with no legal action must pass. Two living units per side always keep an action on this
// board, so a forced pass cannot arise in play. The tests check the search's path for the
// contrived case, and they check the rule that makes the contrived case unreachable.
static void forced_passes_search() {
    // Every placement of two heroes and two creatures leaves both sides with an action, so the
    // side to act never needs a forced pass on this board.
    for(uint8_t a = 0; a < BATTLE_CELLS; ++a)
        for(uint8_t b = (uint8_t)(a + 1); b < BATTLE_CELLS; ++b)
            for(uint8_t c = 0; c < BATTLE_CELLS; ++c) {
                if(c == a || c == b)
                    continue;
                for(uint8_t d = (uint8_t)(c + 1); d < BATTLE_CELLS; ++d) {
                    if(d == a || d == b)
                        continue;
                    BattleState s = fresh();
                    s.units[HERO_A].cell = a;
                    s.units[HERO_B].cell = b;
                    s.units[FOE_A].cell = c;
                    s.units[FOE_B].cell = d;
                    s.turn = BATTLE_PARTY;
                    CHECK(battle_can_act(s));
                    s.turn = BATTLE_ENEMY;
                    CHECK(battle_can_act(s));
                }
            }

    // The contrived case: the side to act has no living unit, yet the battle has not ended. The
    // search must pass once, which counts as one activation, and the pass ends the battle.
    BattleState contrived = custom(BATTLE_ENEMY, 0);
    contrived.units[FOE_A].hp = 0;
    contrived.units[FOE_B].hp = 0;
    CHECK(!battle_can_act(contrived));
    BattleSearchResult pass = battle_search(contrived, BATTLE_AI_DEPTH);
    CHECK(pass.action.unit_index == BATTLE_NO_UNIT);
    CHECK(pass.score == -(BATTLE_SCORE_WIN - 1));
    BattleSearchStats stats = battle_search_stats();
    CHECK(stats.nodes == 1 && stats.leaves == 1 && stats.deepest == 1);
    BattleState settled = contrived;
    CHECK(battle_settle(settled) == 1 && settled.ended && settled.activations == 1);

    // The forced pass is not a defeat for a side that still has units, so passing never ends the
    // battle.
    BattleState passing = custom(BATTLE_PARTY, 0);
    battle_pass(passing);
    CHECK(!passing.ended && passing.turn == BATTLE_ENEMY && passing.activations == 1);
}

// The search explores a bounded tree, respects its depth, and counts what it visits.
static void search_bounds() {
    // Requested depths beyond the capacity are clamped, so no frame past the last is ever touched.
    BattleState root = fresh();
    root.turn = BATTLE_ENEMY;
    BattleSearchResult deep = battle_search(root, 200);
    BattleSearchStats clamped = battle_search_stats();
    CHECK(clamped.deepest == BATTLE_AI_DEPTH);
    CHECK(deep.action.unit_index != BATTLE_NO_UNIT && battle_legal(root, deep.action));

    // Each requested depth is reached and no deeper.
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth) {
        battle_search(root, depth);
        CHECK(battle_search_stats().deepest == depth);
    }

    // Depth one examines each legal action once, and each child is a leaf.
    uint64_t legal = 0;
    BattleActionCursor cursor = battle_actions(root);
    BattleAction action{};
    while(battle_next_action(root, cursor, action))
        ++legal;
    battle_search(root, 1);
    BattleSearchStats one = battle_search_stats();
    CHECK(one.nodes == legal && one.leaves == legal);

    // Pruning visits fewer positions than the unpruned reference, and the counts are repeatable.
    battle_search(root, BATTLE_AI_DEPTH);
    BattleSearchStats first = battle_search_stats();
    battle_search(root, BATTLE_AI_DEPTH);
    BattleSearchStats second = battle_search_stats();
    CHECK(first.nodes == second.nodes && first.leaves == second.leaves);
    uint64_t unpruned = 0;
    reference_search(root, BATTLE_AI_DEPTH, unpruned);
    CHECK(first.nodes < unpruned && first.leaves <= first.nodes && first.leaves > 0);

    // The static workspace is the frame array plus two counters, and its footprint is bounded.
    CHECK(battle_search_workspace_bytes() <= 256);
    printf("search workspace %u bytes, depth %u: %u nodes, %u leaves (unpruned %llu)\n",
           (unsigned)battle_search_workspace_bytes(), (unsigned)BATTLE_AI_DEPTH,
           (unsigned)first.nodes, (unsigned)first.leaves, (unsigned long long)unpruned);
}

// Eases one coordinate from `start` to `target`. Every frame must move, the position must never
// pass the target, and the walk must end exactly on the target within a few halvings.
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

// The cursor eases from every sub-pixel start on the board to every cell origin. Starts on either
// side of the target are both covered, so overshoot and stalls would both show up here.
static void cursor_easing() {
    constexpr int16_t span = (int16_t)(BATTLE_COLUMNS * TILE * CURSOR_SUBPIXELS);
    for(int16_t start = 0; start <= span; ++start) {
        for(uint8_t column = 0; column < BATTLE_COLUMNS; ++column)
            ease_to(start, cursor_subpixel((int16_t)(column * TILE)));
        for(uint8_t row = 0; row < BATTLE_ROWS; ++row)
            ease_to(start, cursor_subpixel((int16_t)(row * TILE)));
    }

    // A one-cell move settles in nine frames, and the endpoint is exact.
    CHECK(ease_to(0, cursor_subpixel(TILE)) == 9);
    CHECK(cursor_ease(255, 256) == 256 && cursor_ease(257, 256) == 256);
    CHECK(cursor_ease(0, 1) == 1 && cursor_ease(1, 0) == 0);

    // Rendering rounds to the nearest pixel: a half-pixel offset rounds up.
    CHECK(cursor_pixel(cursor_subpixel(5)) == 5);
    CHECK(cursor_pixel(cursor_subpixel(5) + 7) == 5 && cursor_pixel(cursor_subpixel(5) + 8) == 6);
}

// The attacker lunges out in two steps, holds for a quarter second, and returns in two steps.
static void lunge_timing() {
    CHECK(battle_lunge_pixels(0) == 2 && battle_lunge_pixels(1) == BATTLE_LUNGE_PIXELS);
    for(uint8_t frame = 2; frame < 14; ++frame)
        CHECK(battle_lunge_pixels(frame) == BATTLE_LUNGE_PIXELS);
    CHECK(battle_lunge_pixels(14) == 2 && battle_lunge_pixels(15) == 0);
    CHECK(battle_lunge_pixels(16) == 0 && battle_lunge_pixels(255) == 0);
}

// A flash starts hidden, alternates every four frames, and ends visible after about a second.
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
    // Six hidden phases of four frames, with a change at each phase boundary.
    CHECK(hidden == 24);
    CHECK(changes == 11);
    CHECK(!battle_flash_hidden(BATTLE_FLASH_FRAMES - 1));
}

// A unit's team is stored, not inferred from artwork, so a unit's team and sprite row are
// independent.
static void teams_are_explicit() {
    BattleUnit unit{0, 5, BATTLE_TYPE_HERO_A, BATTLE_PARTY};
    CHECK(unit.team == BATTLE_PARTY && battle_sprite_row(unit) == 0);
    unit.type = BATTLE_TYPE_CREATURE_B;
    CHECK(unit.team == BATTLE_PARTY && battle_sprite_row(unit) == 20);
    unit.team = BATTLE_ENEMY;
    CHECK(unit.team == BATTLE_ENEMY && battle_sprite_row(unit) == 20);
}

int main() {
    initial_layout();
    unit_definitions();
    teams_are_explicit();
    battlefield_data();
    board_edges();
    movement_rules();
    move_and_attack();
    alternation();
    exhaustion();
    forced_pass();
    outcomes();
    legal_action_enumeration();
    destination_masks();
    greedy_choices();
    copies_are_independent();
    negamax_tactics();
    negamax_scores();
    negamax_matches_reference();
    exhaustion_search();
    forced_passes_search();
    search_bounds();
    cursor_easing();
    lunge_timing();
    flash_timing();
    printf("PASS battle rules: %u checks\n", checks);
    return 0;
}
