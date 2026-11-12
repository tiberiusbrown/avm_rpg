#include "battle.h"
#include "battle_ai.h"
#include "battle_cursor.h"
#include "tile_ids.h"
#include "world_assets.h"
#include <stdio.h>
#include <stdlib.h>

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
    state.units[unit].x = (uint8_t)x;
    state.units[unit].y = (uint8_t)y;
}

static void initial_layout() {
    BattleState s = fresh();
    CHECK(s.field == &BATTLE_GROVE.field && s.unit_count == 4);
    CHECK(s.turn == BATTLE_PARTY && !s.ended && s.activations == 0);

    struct Expected {
        uint8_t x, y, hp;
        BATTLE_TEAM team;
    } const expected[4] = {
        {2, 1, 5, BATTLE_PARTY},
        {2, 2, 5, BATTLE_PARTY},
        {5, 1, 4, BATTLE_ENEMY},
        {5, 2, 4, BATTLE_ENEMY},
    };

    for(uint8_t i = 0; i < 4; ++i) {
        CHECK(s.units[i].x == expected[i].x && s.units[i].y == expected[i].y);
        CHECK(s.units[i].hp == expected[i].hp && s.units[i].team == expected[i].team);
    }
    CHECK(s.units[HERO_A].sprite_row == 0 && s.units[HERO_B].sprite_row == 1);
    CHECK(s.units[FOE_A].sprite_row == 16 && s.units[FOE_B].sprite_row == 20);

    // Placement never overlaps, and each living unit is found on its own cell.
    for(uint8_t i = 0; i < 4; ++i) {
        CHECK(battle_unit_at(s, cell(s.units[i].x, s.units[i].y)) == i);
        for(uint8_t j = (uint8_t)(i + 1); j < 4; ++j)
            CHECK(cell(s.units[i].x, s.units[i].y) != cell(s.units[j].x, s.units[j].y));
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
    CHECK(BATTLE_GROVE.field.tileset == TILESET_WORLD && s.field->tileset == TILESET_WORLD);
    CHECK(s.field->tiles == BATTLE_GROVE_TILES);
    for(uint8_t i = 0; i < BATTLE_CELLS; ++i) {
        uint8_t tile = s.field->tiles[i];
        CHECK(tile == expected[i]);
        CHECK(tile < TILE_COUNT);
        // Every terrain tile is in the world tileset and has no collision.
        CHECK(TILE_COLLISION[(uint16_t)((uint16_t)TILESET_WORLD << 8 | tile)] == 0);
    }
    CHECK(s.field->tiles[cell(3, 0)] == TILE_FLOWERS_26 &&
          s.field->tiles[cell(5, 1)] == TILE_FLOWERS_27);
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
    CHECK(m.units[HERO_A].x == 3 && m.units[HERO_A].y == 1 && m.units[HERO_A].hp == 5);
    CHECK(m.turn == BATTLE_ENEMY && m.activations == 1 && !m.ended);

    // An attack on an adjacent creature deals two damage, and the attacker stays where it is.
    BattleState a = fresh();
    place(a, HERO_A, 4, 1);
    CHECK(battle_legal(a, {HERO_A, cell(5, 1)}));
    battle_apply(a, {HERO_A, cell(5, 1)});
    CHECK(a.units[FOE_A].hp == 2 && a.units[FOE_A].x == 5 && a.units[FOE_A].y == 1);
    CHECK(a.units[HERO_A].x == 4 && a.units[HERO_A].y == 1 && a.units[HERO_A].hp == 5);
    CHECK(a.turn == BATTLE_ENEMY && a.activations == 1);

    // A creature on the next attack is removed from the board. The attacker does not move in.
    battle_apply(a, {FOE_A, cell(4, 1)});
    CHECK(a.units[HERO_A].hp == 3);
    a.turn = BATTLE_PARTY;
    battle_apply(a, {HERO_A, cell(5, 1)});
    CHECK(a.units[FOE_A].hp == 0);
    CHECK(battle_unit_at(a, cell(5, 1)) == BATTLE_NO_UNIT);
    CHECK(a.units[HERO_A].x == 4 && a.units[HERO_A].y == 1);
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
    CHECK(s.units[HERO_A].x == 2 && s.units[HERO_A].y == 1 && s.turn == BATTLE_ENEMY);
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

static void enemy_choices() {
    // Kills beat attacks, even when the kill comes later in unit order.
    BattleState kill = fresh();
    kill.turn = BATTLE_ENEMY;
    place(kill, HERO_A, 4, 1);
    place(kill, HERO_B, 4, 2);
    kill.units[HERO_B].hp = 2;
    BattleAction first = battle_enemy_action(kill);
    CHECK(first.unit_index == FOE_B && first.destination == cell(4, 2));
    CHECK(battle_legal(kill, first));

    // An attack beats any move.
    BattleState attack = fresh();
    attack.turn = BATTLE_ENEMY;
    place(attack, HERO_A, 4, 1);
    BattleAction hit = battle_enemy_action(attack);
    CHECK(hit.unit_index == FOE_A && hit.destination == cell(4, 1));

    // With no attack, the move that closes most distance wins. Creature A's move west and
    // creature B's move west both leave distance two. The tie goes to the lower unit index.
    BattleState approach = fresh();
    approach.turn = BATTLE_ENEMY;
    BattleAction step = battle_enemy_action(approach);
    CHECK(step.unit_index == FOE_A && step.destination == cell(4, 1));
    CHECK(battle_legal(approach, step));
    CHECK(battle_enemy_action(approach).destination == step.destination);

    // The choice does not depend on the state's history beyond its position.
    BattleState copy = approach;
    battle_apply(copy, step);
    CHECK(approach.units[FOE_A].x == 5 && approach.activations == 0);
    CHECK(copy.units[FOE_A].x == 4 && copy.activations == 1);
}

static void copies_are_independent() {
    BattleState original = fresh();
    BattleState copy = original;
    battle_apply(copy, {HERO_A, cell(3, 1)});
    CHECK(original.units[HERO_A].x == 2 && original.turn == BATTLE_PARTY);
    CHECK(copy.units[HERO_A].x == 3 && copy.turn == BATTLE_ENEMY);
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

int main() {
    initial_layout();
    battlefield_data();
    board_edges();
    movement_rules();
    move_and_attack();
    alternation();
    exhaustion();
    forced_pass();
    outcomes();
    enemy_choices();
    copies_are_independent();
    cursor_easing();
    printf("PASS battle rules: %u checks\n", checks);
    return 0;
}
