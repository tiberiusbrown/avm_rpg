#include "battle.h"
#include "battle_fixtures.h"
#include "battle_ai.h"
#include "battle_cursor.h"
#include "equipment.h"
#include "battle_motion.h"
#include "battle_reference.h"
#include "tile_ids.h"
#include "world_assets.h"
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A cave encounter, which draws one of the mine battlefields.
static constexpr BattleEncounter PROGMEM BATTLE_MINE_SCOUTS =
    battle_encounter(BATTLEFIELDS_CAVE, ENEMY_WOLF);

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
    battle_fixture_begin(state, &BATTLE_TEST_GROVE);
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
    ENEMY_ID id;
    BATTLE_TEAM team;
    uint8_t hp;
};

// A running battle on the Grove field with exactly the listed units, in that index order.
static BattleState board(BATTLE_TEAM turn, std::initializer_list<Placement> units,
                         uint8_t activations = 0) {
    BattleState s{};
    s.field = BATTLEFIELD_GROVE;
    for(Placement const& p : units) {
        // No unit stands on blocking terrain, so a test position that puts one there is a mistake.
        if(battle_blocked(BATTLEFIELD_GROVE, cell(p.x, p.y))) {
            fprintf(stderr, "board: unit on blocking terrain at (%u,%u)\n", p.x, p.y);
            exit(1);
        }
        s.units[s.unit_count++] = BattleUnit{cell(p.x, p.y), p.hp, p.id, p.team};
    }
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

// A random running board: two to eight units of random types, HP, and cells, at most four on each
// team and at least one, a random side to act, and a random activation count.
static BattleState random_board(uint8_t most = BATTLE_UNIT_CAPACITY) {
    static constexpr ENEMY_ID types[] = {BATTLE_TEST_FIGHTER, BATTLE_TEST_ROGUE, BATTLE_TEST_ARCHER,
                                         BATTLE_TEST_SPEARMAN, ENEMY_WOLF, ENEMY_BRUTE};
    BattleState s{};
    s.field = BATTLEFIELD_GROVE;
    s.unit_count = (uint8_t)(2 + next_random(most - 1));
    uint8_t per_team[2] = {};
    for(uint8_t i = 0; i < s.unit_count; ++i) {
        uint8_t c = 0;
        for(;;) {
            c = (uint8_t)next_random(BATTLE_CELLS);
            bool taken = battle_blocked(BATTLEFIELD_GROVE, c);
            for(uint8_t j = 0; j < i; ++j)
                taken = taken || s.units[j].cell == c;
            if(!taken)
                break;
        }
        BATTLE_TEAM team = i == 0   ? BATTLE_PARTY
                           : i == 1 ? BATTLE_ENEMY
                                    : (BATTLE_TEAM)next_random(2);
        if(per_team[team] == BATTLE_SIDE_CAPACITY)
            team = team == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
        ++per_team[team];
        s.units[i] = BattleUnit{c, (uint8_t)(1 + next_random(12)),
                               types[next_random(sizeof(types) / sizeof(*types))], team};
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
    int move_range, attack_min, attack_max, damage, injured_damage;
    bool knockback;
    bool diagonal = false;
    BATTLE_MOVEMENT_TYPE movement = BATTLE_MOVEMENT_ORTHOGONAL;
};

static OracleType oracle_type(uint8_t type) {
    switch(type) {
        case BATTLE_TEST_FIGHTER:
        case ENEMY_GIANT_RAT:
            return {1, 1, 1, 3, 3, false};
        case BATTLE_TEST_ROGUE:
            return {0, 1, 1, 3, 3, false, false, BATTLE_MOVEMENT_LEAP};
        case BATTLE_TEST_ARCHER:
            return {1, 2, 4, 3, 3, false};
        case BATTLE_TEST_SPEARMAN:
            return {1, 1, 2, 3, 3, false};
        case ENEMY_WOLF: // Rend: 2 against a healthy victim, 4 against one below half HP.
            return {2, 1, 1, 2, 4, false};
        case ENEMY_BRUTE:
        case ENEMY_GOLEM:
            return {1, 1, 1, 2, 2, true};
        case ENEMY_GIANT_BAT:
            return {2, 1, 1, 3, 3, false, true, BATTLE_MOVEMENT_GLIDE};
        case ENEMY_GIANT_SPIDER:
            return {0, 1, 1, 2, 2, false, false, BATTLE_MOVEMENT_LEAP};
        case ENEMY_SKELETON:
            return {1, 1, 2, 3, 3, false};
        case ENEMY_FLOATING_EYE:
            return {2, 2, 3, 2, 2, false, false, BATTLE_MOVEMENT_VAULT};
        case ENEMY_GIANT_BEETLE:
            return {2, 1, 1, 2, 2, true};
        case ENEMY_SNAKE:
            return {0, 1, 1, 3, 3, false, false, BATTLE_MOVEMENT_LEAP};
        case ENEMY_SLIME:
            return {1, 1, 1, 2, 2, false};
        case ENEMY_ZOMBIE:
            return {1, 1, 1, 2, 4, false};
        case ENEMY_GOBLIN:
            return {0, 2, 4, 3, 3, false, false, BATTLE_MOVEMENT_LEAP};
        case ENEMY_GHOST:
            return {2, 1, 1, 2, 2, false, false, BATTLE_MOVEMENT_GLIDE};
        case ENEMY_MUSHROOM:
            return {1, 2, 3, 2, 2, false};
        case ENEMY_HOODED_CULTIST:
            return {2, 2, 4, 3, 3, false, false, BATTLE_MOVEMENT_VAULT};
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

// Whether a battlefield cell's tile has collision in its tileset: the independent statement of
// which cells block.
static bool oracle_blocked(uint8_t field, unsigned x, unsigned y) {
    Battlefield const& battlefield = BATTLEFIELDS[field];
    uint8_t tile = battlefield.tiles[y * BATTLE_COLUMNS + x];
    return TILE_COLLISION[(uint16_t)((uint16_t)battlefield.tileset << 8 | tile)] != 0;
}

// Whether every cell strictly between (x, y) and (x + dx, y + dy) on a straight line is empty: no
// living unit and no blocking terrain.
static bool oracle_clear(BattleState const& s, int x, int y, int dx, int dy) {
    int distance = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    for(int k = 1; k < distance; ++k)
        if(oracle_occupant(s, x + k * sx, y + k * sy) >= 0 ||
           oracle_blocked(s.field, (unsigned)(x + k * sx), (unsigned)(y + k * sy)))
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
    PartyMember const* member = actor.id & BATTLE_PARTY_SOURCE
                                    ? &game.party.members[actor.id & ~BATTLE_PARTY_SOURCE]
                                    : nullptr;
    static constexpr OracleType ATTACKS[] = {
        {0, 1, 1, 3, 3, false}, {0, 2, 4, 3, 3, false},
        {0, 1, 2, 3, 3, false}, {0, 1, 1, 2, 2, true},
        {0, 1, 1, 2, 4, false}, {0, 1, 1, 3, 3, false, true},
        {0, 1, 1, 2, 2, false}, {0, 1, 1, 3, 3, false},
        {0, 1, 1, 2, 2, false}, {0, 2, 3, 2, 2, false},
    };
    static_assert(sizeof(ATTACKS) / sizeof(*ATTACKS) == ATTACK_CLASS_COUNT);
    OracleType rules = member ? ATTACKS[member->attack_class] : oracle_type(actor.id);
    int occupant = oracle_occupant(s, destination % 8, destination / 8);
    bool straight = (dx == 0) != (dy == 0);
    int distance = abs(dx) + abs(dy);
    if(occupant < 0) {
        // No unit stands on blocking terrain, and no move ends there; a leap passes over it.
        if(oracle_blocked(s.field, (unsigned)(destination % 8), (unsigned)(destination / 8)))
            return false;
        if(member) {
            static constexpr BATTLE_MOVEMENT_TYPE MOVES[MOVEMENT_CLASS_COUNT] = {
                BATTLE_MOVEMENT_ORTHOGONAL, BATTLE_MOVEMENT_LEAP, BATTLE_MOVEMENT_ORTHOGONAL,
                BATTLE_MOVEMENT_VAULT, BATTLE_MOVEMENT_GLIDE,
            };
            rules.movement = MOVES[member->movement_class];
            rules.move_range = member->movement_class == MOVEMENT_STEP ? 1 : 2;
        }
        if(rules.movement == BATTLE_MOVEMENT_GLIDE)
            return abs(dx) == abs(dy) && distance > 0 && distance <= 2 * rules.move_range &&
                   oracle_clear(s, x, y, dx, dy);
        if(rules.movement == BATTLE_MOVEMENT_VAULT)
            return straight && distance == rules.move_range;
        if(rules.movement == BATTLE_MOVEMENT_LEAP)
            return (abs(dx) == 1 && abs(dy) == 2) || (abs(dx) == 2 && abs(dy) == 1);
        return straight && distance <= rules.move_range && oracle_clear(s, x, y, dx, dy);
    }
    if(s.units[occupant].team == actor.team)
        return false;
    if(rules.diagonal)
        return abs(dx) == 1 && abs(dy) == 1;
    int attack_max = actor.id == ENEMY_HOODED_CULTIST ? BATTLE_COLUMNS - 1 : rules.attack_max;
    if(!straight || distance < rules.attack_min || distance > attack_max)
        return false;
    if(member && member->attack_class == ATTACK_REACH && distance == 2 &&
       party_attack_perk_rank(*member) == 3) {
        int mid_x = x + dx / 2, mid_y = y + dy / 2;
        int screen = oracle_occupant(s, mid_x, mid_y);
        if(screen >= 0 && s.units[screen].team == actor.team &&
           !oracle_blocked(s.field, (unsigned)mid_x, (unsigned)mid_y))
            return true;
    }
    return oracle_clear(s, x, y, dx, dy);
}

// --- Data ----------------------------------------------------------------------------------

static void unit_definitions() {
    uint32_t creature_rows = 0;
    for(uint8_t t = 0; t < BATTLE_TEST_SOURCE_COUNT; ++t) {
        EnemyInfo const& info = ENEMY_INFO[t];
        BattleUnitRules const& type = battle_rules(t);
        OracleType rules = oracle_type((ENEMY_ID)t);
        CHECK(type.movement_type == rules.movement);
        CHECK(type.move_range == rules.move_range);
        CHECK(type.attack_min == rules.attack_min && type.attack_max == rules.attack_max);
        CHECK(type.damage == rules.damage && type.injured_damage == rules.injured_damage);
        CHECK((type.effect == BATTLE_EFFECT_KNOCKBACK) == rules.knockback);
        CHECK((type.geometry == BATTLE_ATTACK_DIAGONAL) == rules.diagonal);
        CHECK(info.sprite_row < ACTOR_ROW_COUNT);
        if(t < ENEMY_COUNT)
            creature_rows |= uint32_t(1) << info.sprite_row;
        // Each archetype has artwork of its own.
        for(uint8_t u = 0; u < t; ++u)
            CHECK(ENEMY_INFO[u].sprite_row != info.sprite_row);
    }
    CHECK(creature_rows == 0xffff0000u);
    // The human rows belong to test fixtures.
    CHECK(ENEMY_INFO[ENEMY_WOLF].sprite_row == 17);
    CHECK(ENEMY_INFO[ENEMY_BRUTE].sprite_row == 25);
    CHECK(ENEMY_INFO[ENEMY_GIANT_BAT].sprite_row == 18 && ENEMY_INFO[ENEMY_GIANT_BAT].max_hp == 6);
    CHECK(ENEMY_INFO[ENEMY_GIANT_SPIDER].sprite_row == 19 &&
          ENEMY_INFO[ENEMY_GIANT_SPIDER].max_hp == 7);
    CHECK(ENEMY_INFO[ENEMY_SKELETON].sprite_row == 23 && ENEMY_INFO[ENEMY_SKELETON].max_hp == 10);
    CHECK(ENEMY_INFO[ENEMY_FLOATING_EYE].sprite_row == 29 &&
          ENEMY_INFO[ENEMY_FLOATING_EYE].max_hp == 8);
    CHECK(battle_rules(ENEMY_GIANT_SPIDER).effect == BATTLE_EFFECT_FLANK);
    CHECK(battle_rules(ENEMY_FLOATING_EYE).effect == BATTLE_EFFECT_SPLASH);
    CHECK(ENEMY_INFO[BATTLE_TEST_ROGUE].sprite_row < 16);
    CHECK(ENEMY_INFO[BATTLE_TEST_ARCHER].sprite_row < 16);

    // The copied state stays compact: no ability data is stored per unit.
    static_assert(sizeof(BattleUnit) == 4, "placement, HP, source ID and enemy AP");
    static_assert(sizeof(BattleState) == 39, "compact battle state with a 16-bit activation counter");
    static_assert(sizeof(EnemyInfo) == 6, "immutable enemy information");
}

static void creature_actions() {
    BattleState bat = board(BATTLE_ENEMY, {{2, 0, ENEMY_GIANT_BAT, BATTLE_ENEMY, 6},
                                          {1, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    CHECK(battle_legal(bat, act(0, 4, 2)) && !battle_legal(bat, act(0, 3, 0)));
    battle_apply(bat, act(0, 1, 1));
    CHECK(bat.units[1].hp == 7);

    BattleState spiders = board(BATTLE_ENEMY, {{3, 1, ENEMY_GIANT_SPIDER, BATTLE_ENEMY, 7},
                                              {4, 2, ENEMY_GIANT_SPIDER, BATTLE_ENEMY, 7},
                                              {4, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    CHECK(battle_legal(spiders, act(0, 5, 2)));
    battle_apply(spiders, act(0, 4, 1));
    CHECK(spiders.units[2].hp == 6);

    BattleState skeleton = board(BATTLE_ENEMY, {{3, 1, ENEMY_SKELETON, BATTLE_ENEMY, 10},
                                               {5, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    battle_apply(skeleton, act(0, 5, 1));
    CHECK(skeleton.units[1].hp == 6);

    BattleState eye = board(BATTLE_ENEMY, {{2, 1, ENEMY_FLOATING_EYE, BATTLE_ENEMY, 8},
                                          {2, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                          {5, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                          {5, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    CHECK(battle_legal(eye, act(0, 2, 3)));
    battle_apply(eye, act(0, 5, 1));
    CHECK(eye.units[1].hp == 10 && eye.units[2].hp == 8 && eye.units[3].hp == 8);
}

static void enemy_ranks_and_ap() {
    BattleState state = board(BATTLE_ENEMY, {{3, 1, ENEMY_SKELETON, BATTLE_ENEMY, 10},
                                            {5, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5}});
    auto& skeleton = state.units[0];
    CHECK(battle_movement_rank(skeleton) == 2 && battle_attack_rank(skeleton) == 1);
    CHECK(battle_move_cost(skeleton, true) == 0 && battle_move_cost(skeleton, false) == 1);
    BattleAction target = act(0, 5, 1);
    CHECK(battle_enemy_attack_choice(state, target) == BATTLE_NORMAL);
    skeleton.ap = 2;
    CHECK(battle_enemy_attack_choice(state, act(0, 3, 2)) == BATTLE_NORMAL);
    CHECK(battle_enemy_attack_choice(state, target) == BATTLE_STARTER);
    CHECK(!battle_special_legal(state, target, BATTLE_SIGNATURE));
    Party party = game.party;
    BattleSearchResult found = battle_search(state, 1);
    CHECK(same_action(found.action, target) && found.score == BATTLE_SCORE_WIN - 1);
    CHECK(battle_ap(skeleton) == 2 && state.units[1].hp == 5);
    BattleState copy = state;
    battle_simulate_action(copy, target);
    CHECK(copy.ended && copy.result == BATTLE_DEFEAT && battle_ap(copy.units[0]) == 0);
    CHECK(battle_ap(skeleton) == 2);

    state = board(BATTLE_ENEMY, {{2, 1, ENEMY_FLOATING_EYE, BATTLE_ENEMY, 8},
                                {5, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                {5, 3, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    state.units[0].ap = 3;
    CHECK(battle_enemy_attack_choice(state, target) == BATTLE_SIGNATURE);
    battle_apply_special(state, target, BATTLE_SIGNATURE);
    CHECK(battle_ap(state.units[0]) == 0 && state.turn == BATTLE_PARTY);
    CHECK(state.units[1].hp == 8 && state.units[2].hp == 8 && state.units[2].cell == cell(5, 2));
    CHECK(memcmp(&party, &game.party, sizeof(Party)) == 0);
}

static void initial_layout() {
    BattleState s = fresh();
    CHECK(s.field == BATTLEFIELD_GROVE && BATTLE_MINE_SCOUTS.fields == BATTLEFIELDS_CAVE);
    CHECK(s.unit_count == 4);
    CHECK(s.turn == BATTLE_PARTY && !s.ended && s.activations == 0);

    struct Expected {
        uint8_t x, y, hp;
        BATTLE_TEAM team;
        ENEMY_ID id;
    } const expected[4] = {
        {2, 1, 10, BATTLE_PARTY, BATTLE_TEST_ROGUE},
        {1, 2, 9, BATTLE_PARTY, BATTLE_TEST_ARCHER},
        {6, 1, 9, BATTLE_ENEMY, ENEMY_WOLF},
        {5, 2, 12, BATTLE_ENEMY, ENEMY_BRUTE},
    };

    for(uint8_t i = 0; i < 4; ++i) {
        CHECK(battle_column(s.units[i].cell) == expected[i].x &&
              battle_row(s.units[i].cell) == expected[i].y);
        CHECK(s.units[i].hp == expected[i].hp && s.units[i].team == expected[i].team);
        CHECK(s.units[i].id == expected[i].id);
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
    battle_fixture_begin(s, &BATTLE_TEST_GROVE);
    CHECK(!s.ended && s.activations == 0 && s.turn == BATTLE_PARTY);
    CHECK(s.units[BRUTE].hp == 12 && s.units[ROGUE].hp == 10);
}

// A unit's team is stored, not inferred from artwork, so a unit's team and sprite row are
// independent, and any archetype can fight on either side.
static void teams_are_explicit() {
    BattleUnit unit{0, 5, BATTLE_TEST_ROGUE, BATTLE_PARTY};
    CHECK(unit.team == BATTLE_PARTY &&
          battle_sprite_row(unit) == ENEMY_INFO[BATTLE_TEST_ROGUE].sprite_row);
    unit.id = ENEMY_WOLF;
    CHECK(unit.team == BATTLE_PARTY && battle_sprite_row(unit) == 17);
    unit.team = BATTLE_ENEMY;
    CHECK(unit.team == BATTLE_ENEMY && battle_sprite_row(unit) == 17);

    // A wolf on the party's side acts on the party's turn and bites enemies, whatever its artwork.
    BattleState s = board(BATTLE_PARTY, {{3, 1, ENEMY_WOLF, BATTLE_PARTY, 4},
                                         {4, 1, BATTLE_TEST_ROGUE, BATTLE_ENEMY, 5}});
    CHECK(battle_legal(s, act(0, 4, 1)) && battle_legal(s, act(0, 1, 1)));
    CHECK(!battle_legal(s, act(1, 3, 3)));
    s.turn = BATTLE_ENEMY;
    CHECK(battle_legal(s, act(1, 3, 1)) && battle_legal(s, act(1, 6, 2)));
}

static void battlefield_data() {
    static_assert(BATTLE_COLUMNS == 8 && BATTLE_ROWS == 4, "the board is 8 by 4");

    BattleState s = fresh();
    CHECK(s.field < BATTLEFIELD_COUNT);
    Battlefield const& field = BATTLEFIELDS[s.field];
    CHECK(field.tileset == TILESET_WORLD);
    unsigned blocked = 0;
    for(uint8_t i = 0; i < BATTLE_CELLS; ++i) {
        uint8_t tile = field.tiles[i];
        CHECK(tile < TILE_COUNT);
        // The blocked masks agree with the tileset's collision, cell by cell.
        bool collides = oracle_blocked(s.field, battle_column(i), battle_row(i));
        CHECK(battle_blocked(s.field, i) == collides);
        CHECK(((field.blocked[battle_row(i)] >> battle_column(i)) & 1) == collides);
        blocked += collides;
    }
    CHECK(blocked == 4);
    CHECK(battle_blocked(s.field, cell(4, 0)) && battle_blocked(s.field, cell(6, 0)));
    CHECK(battle_blocked(s.field, cell(1, 3)) && battle_blocked(s.field, cell(4, 3)));
    // No unit starts on terrain, and the opening shot and the wolf's run stay open.
    for(uint8_t u = 0; u < s.unit_count; ++u)
        CHECK(!battle_blocked(s.field, s.units[u].cell));
    // The screening bits agree with a walk over the cells between every aligned pair.
    for(uint8_t a = 0; a < BATTLE_CELLS; ++a)
        for(uint8_t b = 0; b < BATTLE_CELLS; ++b) {
            int ax = a % 8, ay = a / 8, bx = b % 8, by = b / 8;
            bool screened = false;
            if(a != b && (ax == bx || ay == by)) {
                int sx = bx > ax ? 1 : bx < ax ? -1 : 0, sy = by > ay ? 1 : by < ay ? -1 : 0;
                for(int x = ax + sx, y = ay + sy; x != bx || y != by; x += sx, y += sy)
                    screened = screened || oracle_blocked(s.field, (unsigned)x, (unsigned)y);
            }
            CHECK(((field.screened[a][b / 8] >> (b % 8)) & 1) == screened);
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
    BattleState s = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5},
                                         {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(destinations(s, 0) == cells_of({cell(3, 0), cell(4, 1), cell(3, 2), cell(2, 1)}));
    CHECK(!battle_legal(s, act(0, 5, 1)) && !battle_legal(s, act(0, 4, 2)));
    // The spearman, archer, and brute move the same way.
    for(ENEMY_ID type : {BATTLE_TEST_ARCHER, BATTLE_TEST_SPEARMAN, ENEMY_BRUTE}) {
        s.units[0].id = type;
        CHECK(destinations(s, 0) == cells_of({cell(3, 0), cell(4, 1), cell(3, 2), cell(2, 1)}));
    }
}

static void rogue_movement() {
    // 2. A rogue reaches every open knight-shaped cell on the board, and nothing else: the boulder
    // at (4,3) is a knight's leap away but blocks it.
    BattleState s = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                         {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    uint32_t leaps = cells_of({cell(5, 0), cell(5, 2), cell(2, 3), cell(1, 0), cell(1, 2)});
    CHECK(!battle_legal(s, act(0, 4, 3)));
    CHECK(destinations(s, 0) == leaps);
    // 3. No ordinary orthogonal move, of one cell or two.
    CHECK(!battle_legal(s, act(0, 3, 0)) && !battle_legal(s, act(0, 4, 1)) &&
          !battle_legal(s, act(0, 5, 1)) && !battle_legal(s, act(0, 3, 3)));
    // Near the middle of the board all eight leaps exist.
    place(s, 0, 3, 2);
    BattleState mid = board(BATTLE_PARTY, {{3, 2, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                           {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    uint8_t packed[BATTLE_UNIT_ACTION_CAPACITY];
    uint8_t attacks = 0;
    // From (3,2) the leaps two rows down leave the board, so six remain on it, and two of those
    // land on the pine at (4,0) and the rocks at (1,3).
    CHECK(battle_unit_actions(mid, 0, packed, attacks) == 4 && attacks == 0);

    // 4. The rogue jumps over units, friendly and enemy, between it and its landing cell.
    BattleState over = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                            {4, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5},
                                            {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                            {4, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_legal(over, act(0, 5, 2)) && battle_legal(over, act(0, 2, 3)));
    // 5. It cannot land on an occupied cell: a friend there is illegal, and an enemy on a
    // knight-shaped cell is not an attack.
    BattleState land = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                            {5, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5},
                                            {1, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(land, act(0, 5, 2)) && !battle_legal(land, act(0, 1, 0)));
    CHECK(battle_legal(land, act(0, 5, 0)));
    battle_apply(land, act(0, 5, 0));
    CHECK(land.units[0].cell == cell(5, 0) && land.units[0].hp == 4 && land.activations == 1);
}

static void wolf_movement() {
    // 6. A wolf moves one or two cells straight.
    BattleState s = board(BATTLE_PARTY, {{3, 1, ENEMY_WOLF, BATTLE_PARTY, 4},
                                         {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(destinations(s, 0) == cells_of({cell(3, 0), cell(4, 1), cell(5, 1), cell(3, 2),
                                          cell(3, 3), cell(2, 1), cell(1, 1)}));
    battle_apply(s, act(0, 5, 1));
    CHECK(s.units[0].cell == cell(5, 1) && s.activations == 1 && s.turn == BATTLE_ENEMY);

    // 7. A unit on the first cell blocks the second, friend or foe, and it cannot run through.
    BattleState blocked = board(BATTLE_PARTY, {{3, 1, ENEMY_WOLF, BATTLE_PARTY, 4},
                                               {4, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4},
                                               {3, 2, BATTLE_TEST_ARCHER, BATTLE_ENEMY, 4}});
    CHECK(!battle_legal(blocked, act(0, 5, 1)) && !battle_legal(blocked, act(0, 3, 3)));
    // The adjacent enemy archer is an attack, and the run west is still open.
    CHECK(battle_legal(blocked, act(0, 3, 2)) && battle_legal(blocked, act(0, 1, 1)));
    // A wolf cannot run into an enemy two cells away: that is neither a move nor an attack.
    BattleState far = board(BATTLE_PARTY, {{3, 1, ENEMY_WOLF, BATTLE_PARTY, 4},
                                           {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(far, act(0, 5, 1)) && battle_legal(far, act(0, 4, 1)));

    // 8. No change of direction: an L-shaped two-cell move and a diagonal are not moves.
    CHECK(!battle_legal(s, act(0, 6, 0)) && !battle_legal(s, act(0, 4, 2)));
    s.turn = BATTLE_PARTY;
    CHECK(!battle_legal(s, act(0, 6, 2)) && !battle_legal(s, act(0, 4, 0)));
}

static void board_boundaries() {
    // 9. Moves never wrap from one row's end to the next row's start.
    BattleState s = board(BATTLE_PARTY, {{7, 1, ENEMY_WOLF, BATTLE_PARTY, 4},
                                         {0, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_legal(s, act(0, 0, 2)) && !battle_legal(s, act(0, 1, 2)));
    CHECK(destinations(s, 0) ==
          cells_of({cell(7, 0), cell(7, 2), cell(7, 3), cell(6, 1), cell(5, 1)}));
    BattleState left = board(BATTLE_PARTY, {{0, 2, BATTLE_TEST_ROGUE, BATTLE_PARTY, 4},
                                            {7, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(destinations(left, 0) == cells_of({cell(1, 0), cell(2, 1), cell(2, 3)}));
    // An archer at a row's end cannot shoot the next row's first cell.
    BattleState shot = board(BATTLE_PARTY, {{6, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4},
                                            {0, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_can_attack(shot, 0, 1));
    // Cells past the last cell are off the board, and so is an unknown unit.
    CHECK(!battle_legal(s, act(0, 7, 4)) && !battle_legal(s, {0, 255}));
    CHECK(!battle_legal(s, {BATTLE_UNIT_CAPACITY, cell(7, 0)}));
}

// --- Terrain -------------------------------------------------------------------------------

static void terrain() {
    // A step cannot enter a tree or a rock, and a run stops before one: from (2,0) the wolf reaches
    // (3,0) but not the pine at (4,0), and from between the two trees at (5,0) it cannot go east or
    // west at all.
    BattleState run = board(BATTLE_PARTY, {{2, 0, ENEMY_WOLF, BATTLE_PARTY, 9},
                                           {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(run, act(0, 3, 0)) && !battle_legal(run, act(0, 4, 0)));
    CHECK(!battle_can_move_to(run, 0, cell(4, 0)));
    BattleState pinned = board(BATTLE_PARTY, {{5, 0, ENEMY_WOLF, BATTLE_PARTY, 9},
                                              {0, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(destinations(pinned, 0) == cells_of({cell(5, 1), cell(5, 2)}));
    BattleState step = board(BATTLE_PARTY, {{1, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                            {7, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(step, act(0, 1, 3)) && battle_legal(step, act(0, 0, 2)));

    // A leap passes over terrain as it passes over units, but cannot land on it.
    BattleState leap = board(BATTLE_PARTY, {{3, 0, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                            {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(leap, act(0, 5, 1)));
    BattleState landing = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                               {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(landing, act(0, 4, 3)));

    // Terrain screens shots and thrusts: along row 0 the pine stands between (2,0) and (5,0), and
    // along row 3 the boulder between (2,3) and (5,3). Range alone would allow both.
    BattleState screened = board(BATTLE_PARTY, {{2, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                                {5, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_can_attack(screened, 0, 1) && !battle_legal(screened, act(0, 5, 0)));
    CHECK(battle_line_blocked(screened, cell(2, 0), cell(5, 0)));
    BattleState low = board(BATTLE_PARTY, {{2, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                           {5, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_can_attack(low, 0, 1));
    BattleState open = board(BATTLE_PARTY, {{0, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                            {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_can_attack(open, 0, 1));
    BattleState thrust = board(BATTLE_PARTY, {{3, 3, BATTLE_TEST_SPEARMAN, BATTLE_PARTY, 10},
                                              {5, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(thrust, act(0, 5, 3)));
    // Adjacent blows have nothing between, so terrain never stops them.
    BattleState beside = board(BATTLE_PARTY, {{3, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                              {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(beside, act(0, 3, 1)));

    // No knockback onto terrain: the brute's blow from (4,2) would push north onto the pine at
    // (4,0), and from (2,3) east onto the boulder.
    BattleState north = board(BATTLE_PARTY, {{4, 2, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                             {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(north, act(0, 4, 1));
    CHECK(north.units[1].cell == cell(4, 1) && north.units[1].hp == 8);
    BattleState east = board(BATTLE_PARTY, {{2, 3, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                            {3, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(east, act(0, 3, 3));
    CHECK(east.units[1].cell == cell(3, 3) && east.units[1].hp == 8);

    // The evaluation counts strikes by the same rules: a screened archer threatens nothing, so its
    // score matches a spearman's in the same cell, while an open line costs the enemy 60.
    BattleState archer = board(BATTLE_ENEMY, {{2, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                              {5, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    BattleState spearman = archer;
    spearman.units[0].id = BATTLE_TEST_SPEARMAN;
    CHECK(battle_score(archer, BATTLE_ENEMY, 0) == battle_score(spearman, BATTLE_ENEMY, 0));

    // The enemy's choices respect terrain: a lone fighter between the trees attacks the hero below
    // it rather than look for a way through them.
    BattleState hemmed = board(BATTLE_ENEMY, {{5, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                              {5, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(legal_set(hemmed).count == 1);
    CHECK(same_action(battle_enemy_action(hemmed), act(1, 5, 1)));
}

// --- Attacks -------------------------------------------------------------------------------

static void melee_attacks() {
    // 10. A fighter strikes an adjacent enemy for 3 and stays put, at no cost to itself.
    BattleState s = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                         {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                         {3, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(s, act(0, 4, 1)) && !battle_legal(s, act(0, 3, 3)));
    battle_apply(s, act(0, 4, 1));
    CHECK(s.units[1].hp == 7 && s.units[1].cell == cell(4, 1) && s.units[0].cell == cell(3, 1));
    CHECK(s.activations == 1 && s.turn == BATTLE_ENEMY);

    // 11. A rogue strikes adjacent enemies only, never one on a knight-shaped cell.
    BattleState r = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                         {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                         {5, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(r, act(0, 3, 2)) && !battle_legal(r, act(0, 5, 2)));
    CHECK(battle_can_attack(r, 0, 1) && !battle_can_attack(r, 0, 2));
    battle_apply(r, act(0, 3, 2));
    CHECK(r.units[1].hp == 7 && r.units[0].cell == cell(3, 1));

    // 20. A wolf bites adjacent enemies only, even in a clear line two cells away. Its Rend deals 2
    // to a victim at half HP or more, so the healthy fighter drops to 8.
    BattleState w = board(BATTLE_PARTY, {{3, 1, ENEMY_WOLF, BATTLE_PARTY, 9},
                                         {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                         {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_can_attack(w, 0, 1) && !battle_can_attack(w, 0, 2));
    battle_apply(w, act(0, 3, 0));
    CHECK(w.units[1].hp == 8 && w.units[0].cell == cell(3, 1));
}

static void archer_attacks() {
    // 12. An archer shoots along a row or column at ranges 2, 3, and 4.
    for(unsigned range = 2; range <= 4; ++range) {
        BattleState row =
            board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                 {1 + range, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
        CHECK(battle_legal(row, act(0, 1 + range, 1)));
        battle_apply(row, act(0, 1 + range, 1));
        CHECK(row.units[1].hp == 7 && row.units[0].cell == cell(1, 1));
    }
    BattleState column = board(BATTLE_PARTY, {{2, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                              {2, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_can_attack(column, 0, 1));

    // 13. An adjacent enemy cannot be shot.
    BattleState near = board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                            {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(near, act(0, 2, 1)) && !battle_can_attack(near, 0, 1));
    // 14. Nor one five cells away.
    BattleState beyond = board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                              {6, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(beyond, act(0, 6, 1)));
    // 15. Nor one on a diagonal, or off the lines entirely.
    BattleState diagonal = board(BATTLE_PARTY, {{1, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                                {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(diagonal, act(0, 3, 2)) && !battle_legal(diagonal, act(0, 4, 1)));

    // 16. Any living unit between blocks the shot, friend or foe; a defeated unit does not.
    BattleState friendly = board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                                {3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                                {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(friendly, act(0, 4, 1)));
    BattleState hostile = board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                               {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                               {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(hostile, act(0, 3, 1)) && !battle_legal(hostile, act(0, 5, 1)));
    hostile.units[1].hp = 0;
    CHECK(battle_legal(hostile, act(0, 5, 1)));
    BattleState up = board(BATTLE_PARTY, {{2, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                          {2, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                          {2, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_can_attack(up, 0, 2));
}

static void spearman_attacks() {
    // 17. A spearman thrusts at ranges 1 and 2 and stays put.
    BattleState s = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_SPEARMAN, BATTLE_PARTY, 10},
                                         {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                         {2, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(s, act(0, 3, 1)) && battle_legal(s, act(0, 2, 3)));
    battle_apply(s, act(0, 2, 3));
    CHECK(s.units[2].hp == 7 && s.units[0].cell == cell(2, 1));

    // 18. The two-cell thrust needs the cell between to be empty.
    BattleState blocked = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_SPEARMAN, BATTLE_PARTY, 10},
                                               {2, 2, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                               {2, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(blocked, act(0, 2, 3)));
    // 19. Three cells is out of reach.
    BattleState beyond = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_SPEARMAN, BATTLE_PARTY, 10},
                                              {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_legal(beyond, act(0, 5, 1)));
}

static void brute_attacks() {
    // 21. A brute deals exactly 2. 22. A surviving victim is pushed one cell straight away.
    BattleState s = board(BATTLE_PARTY, {{2, 1, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                         {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(s, act(0, 3, 1));
    CHECK(s.units[1].hp == 8 && s.units[1].cell == cell(4, 1));
    CHECK(s.units[0].cell == cell(2, 1) && s.activations == 1 && s.turn == BATTLE_ENEMY);
    // 25. Occupancy follows: the old cell is empty and the new one holds the victim.
    CHECK(battle_unit_at(s, cell(3, 1)) == BATTLE_NO_UNIT && battle_unit_at(s, cell(4, 1)) == 1);
    // Pushes go every way.
    BattleState north = board(BATTLE_PARTY, {{2, 2, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                             {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(north, act(0, 2, 1));
    CHECK(north.units[1].cell == cell(2, 0));
    BattleState west = board(BATTLE_PARTY, {{2, 2, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                            {1, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(west, act(0, 1, 2));
    CHECK(west.units[1].cell == cell(0, 2));

    // 23. No push off the board, or into a unit of either side, and no collision damage.
    BattleState edge = board(BATTLE_PARTY, {{6, 1, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                            {7, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(edge, act(0, 7, 1));
    CHECK(edge.units[1].cell == cell(7, 1) && edge.units[1].hp == 8);
    BattleState top = board(BATTLE_PARTY, {{3, 1, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                           {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(top, act(0, 3, 0));
    CHECK(top.units[1].cell == cell(3, 0));
    BattleState wall = board(BATTLE_PARTY, {{2, 1, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                            {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {3, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    battle_apply(wall, act(0, 3, 1));
    CHECK(wall.units[1].cell == cell(3, 1) && wall.units[1].hp == 8 && wall.units[2].hp == 10);
    BattleState ally = board(BATTLE_PARTY, {{2, 1, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                            {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {4, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    battle_apply(ally, act(0, 3, 1));
    CHECK(ally.units[1].cell == cell(3, 1) && ally.units[2].hp == 10);
    // A defeated unit's cell is empty, so a push may enter it.
    ally.units[2].hp = 0;
    ally.turn = BATTLE_PARTY;
    battle_apply(ally, act(0, 3, 1));
    CHECK(ally.units[1].cell == cell(4, 1) && ally.units[1].hp == 6);

    // 24. A victim the blow defeats is not pushed: it is removed where it stood.
    BattleState kill = board(BATTLE_PARTY, {{2, 1, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                            {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 1},
                                            {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
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
        {board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                              {7, 3, ENEMY_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 5, 2)},
        {board(BATTLE_PARTY, {{3, 1, ENEMY_WOLF, BATTLE_PARTY, 4},
                              {7, 3, ENEMY_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 5, 1)},
        {board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4},
                              {5, 1, ENEMY_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 5, 1)},
        {board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_SPEARMAN, BATTLE_PARTY, 4},
                              {3, 1, ENEMY_WOLF, BATTLE_ENEMY, 4}}),
         act(0, 3, 1)},
        {board(BATTLE_PARTY, {{1, 1, ENEMY_BRUTE, BATTLE_PARTY, 6},
                              {2, 1, ENEMY_WOLF, BATTLE_ENEMY, 4}}),
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

static unsigned total_hp(BattleState const& s) {
    unsigned total = 0;
    for(uint8_t i = 0; i < s.unit_count; ++i)
        total += s.units[i].hp;
    return total;
}

static void blood_price() {
    // 1-4. Every voluntary move costs its unit exactly 1 HP, whatever its pattern or distance: one
    // cell, a wolf's two-cell run, or a rogue's leap. Nothing else changes when no enemy is
    // adjacent.
    for(uint8_t t = 0; t < BATTLE_TEST_SOURCE_COUNT; ++t) {
        BattleState s = board(BATTLE_PARTY, {{3, 1, (ENEMY_ID)t, BATTLE_PARTY, 10},
                                             {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
        ActionSet set = legal_set(s);
        CHECK(set.count > 0);
        for(uint8_t i = 0; i < set.count; ++i) {
            BattleState moved = s;
            battle_apply(moved, set.actions[i]);
            CHECK(moved.units[0].cell == set.actions[i].destination && moved.units[0].hp == 9);
            CHECK(moved.units[1].hp == 10 && !moved.ended && moved.turn == BATTLE_ENEMY);
        }
    }
    BattleState run = board(BATTLE_PARTY, {{1, 1, ENEMY_WOLF, BATTLE_PARTY, 9},
                                           {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(run, act(0, 3, 1));
    CHECK(run.units[0].cell == cell(3, 1) && run.units[0].hp == 8);

    // 5. A normal attack costs its attacker nothing.
    BattleState shot = fresh();
    battle_apply(shot, act(ARCHER, 5, 2));
    CHECK(shot.units[ARCHER].hp == 9 && shot.units[BRUTE].hp == 9);

    // 6. No late-battle exhaustion: a long battle charges an attacker nothing and a mover only 1.
    BattleState late = fresh();
    late.activations = 200;
    battle_apply(late, act(ARCHER, 5, 2));
    CHECK(late.units[ARCHER].hp == 9 && late.activations == 201);
    battle_apply(late, act(WOLF, 4, 1));
    CHECK(late.units[WOLF].hp == 8);

    // 7-8. A unit with 1 HP may move; it arrives and then falls.
    BattleState last = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 1},
                                            {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                            {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(last, act(0, 4, 1)) && oracle_legal(last, 0, cell(4, 1)));
    battle_apply(last, act(0, 4, 1));
    CHECK(last.units[0].hp == 0 && last.units[0].cell == cell(4, 1) && !last.ended);
    CHECK(battle_unit_at(last, cell(4, 1)) == BATTLE_NO_UNIT && last.turn == BATTLE_ENEMY);

    // 9. The price comes after the engagement: a 1 HP mover still hits the enemy beside it.
    BattleState price = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 1},
                                             {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                             {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(price, act(0, 4, 1));
    CHECK(price.units[2].hp == 9 && price.units[0].hp == 0);
}

static void engagement_damage() {
    // 10. One adjacent enemy loses 1 HP, and 14. the mover loses only its price.
    BattleState one = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                           {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(one, act(0, 4, 1));
    CHECK(one.units[1].hp == 9 && one.units[0].hp == 9 && one.units[1].cell == cell(5, 1));
    CHECK(battle_engagement(board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                                 {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}}),
                            0, cell(4, 1))
              .hits == 1);

    // 11. Two adjacent enemies each lose 1; 12. a diagonal enemy and 13. a friend lose nothing.
    BattleState two = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                           {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                           {5, 2, ENEMY_BRUTE, BATTLE_ENEMY, 12},
                                           {5, 3, ENEMY_WOLF, BATTLE_ENEMY, 9},
                                           {3, 2, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9}});
    CHECK(battle_legal(two, act(0, 4, 2)));
    BattleEngagement plan = battle_engagement(two, 0, cell(4, 2));
    CHECK(plan.hits == 2 && plan.kills == 0);
    battle_apply(two, act(0, 4, 2));
    CHECK(two.units[1].hp == 9 && two.units[2].hp == 11);
    CHECK(two.units[3].hp == 9 && two.units[4].hp == 9 && two.units[0].hp == 9);

    // 16. An archer engages an adjacent enemy it could never shoot.
    BattleState archer = board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                              {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(archer, act(0, 2, 1));
    CHECK(archer.units[1].hp == 9 && !battle_can_attack(archer, 0, 1));

    // 17. A brute's engagement deals 1 and pushes no one.
    BattleState brute = board(BATTLE_PARTY, {{1, 1, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(brute, act(0, 2, 1));
    CHECK(brute.units[1].hp == 9 && brute.units[1].cell == cell(3, 1));

    // 18. A wolf's run engages only at its destination, not beside the cell it runs through.
    BattleState run = board(BATTLE_PARTY, {{1, 1, ENEMY_WOLF, BATTLE_PARTY, 9},
                                           {2, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                           {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(run, act(0, 3, 1)));
    battle_apply(run, act(0, 3, 1));
    CHECK(run.units[1].hp == 10 && run.units[2].hp == 9 && run.units[0].hp == 8);

    // 19. A rogue's leap engages only at its landing cell, not beside its start or the cells it
    // passes.
    BattleState leap = board(BATTLE_PARTY, {{1, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                            {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {3, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(leap, act(0, 3, 2)));
    battle_apply(leap, act(0, 3, 2));
    CHECK(leap.units[1].hp == 10 && leap.units[2].hp == 9);

    // 20. A knockback is not a move: the pushed unit pays nothing and engages no one, though it
    // lands beside an enemy of its own.
    BattleState push = board(BATTLE_PARTY, {{1, 1, ENEMY_BRUTE, BATTLE_PARTY, 12},
                                            {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {3, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9}});
    battle_apply(push, act(0, 2, 1));
    CHECK(push.units[1].cell == cell(3, 1) && push.units[1].hp == 8);
    CHECK(push.units[2].hp == 9 && push.units[0].hp == 12);

    // 21. A normal attack engages no one: the enemy beside the attacker is untouched.
    BattleState strike = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                              {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                              {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(strike, act(0, 4, 1));
    CHECK(strike.units[1].hp == 7 && strike.units[2].hp == 10 && strike.units[0].hp == 10);
}

static void sacrifices_and_outcomes() {
    // 22. A 1 HP mover defeats an enemy and then falls; the battle goes on while both sides live.
    BattleState trade = board(BATTLE_PARTY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 1},
                                             {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                             {5, 1, ENEMY_WOLF, BATTLE_ENEMY, 1},
                                             {7, 3, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    CHECK(battle_action_kind(trade, act(0, 4, 1)) == BATTLE_ACTION_KILL);
    battle_apply(trade, act(0, 4, 1));
    CHECK(trade.units[0].hp == 0 && trade.units[2].hp == 0 && !trade.ended);
    CHECK(trade.turn == BATTLE_ENEMY && trade.activations == 1);

    // 23. A 1 HP rogue leaps between two 1 HP enemies and takes both with it.
    BattleState pair = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 1},
                                            {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                            {4, 1, ENEMY_WOLF, BATTLE_ENEMY, 1},
                                            {5, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 1},
                                            {7, 0, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    CHECK(battle_legal(pair, act(0, 4, 2)));
    BattleEngagement both = battle_engagement(pair, 0, cell(4, 2));
    CHECK(both.hits == 2 && both.kills == 2);
    battle_apply(pair, act(0, 4, 2));
    CHECK(pair.units[2].hp == 0 && pair.units[3].hp == 0 && pair.units[0].hp == 0 && !pair.ended);

    // 24-25, 27-28. A move whose engagement removes the last opponent wins for the acting side at
    // once: the battle ends before the blood price, so a 1 HP mover that would otherwise fall with
    // its victim survives, and the two sides never fall together. The party's last rogue leaps
    // beside the last wolf...
    BattleState party_win = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 1},
                                                 {5, 2, ENEMY_WOLF, BATTLE_ENEMY, 1}});
    CHECK(battle_action_kind(party_win, act(0, 3, 3)) == BATTLE_ACTION_KIND{BATTLE_ACTION_MOVE});
    CHECK(battle_action_kind(party_win, act(0, 4, 2)) == BATTLE_ACTION_WIN);
    CHECK(battle_legal(party_win, act(0, 4, 2)));
    battle_apply(party_win, act(0, 4, 2));
    CHECK(party_win.ended && party_win.result == BATTLE_VICTORY);
    CHECK(party_win.units[0].hp == 1 && party_win.units[0].cell == cell(4, 2));
    CHECK(party_win.units[1].hp == 0 && party_win.activations == 1);
    CHECK(battle_living(party_win, BATTLE_PARTY) == 1 &&
          battle_living(party_win, BATTLE_ENEMY) == 0);
    CHECK(battle_score(party_win, BATTLE_PARTY, 1) == BATTLE_SCORE_WIN - 1);
    // ...and the enemy's last wolf runs beside the party's last archer.
    BattleState enemy_win = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 1},
                                                 {5, 1, ENEMY_WOLF, BATTLE_ENEMY, 1}});
    CHECK(battle_legal(enemy_win, act(1, 3, 1)));
    battle_apply(enemy_win, act(1, 3, 1));
    CHECK(enemy_win.ended && enemy_win.result == BATTLE_DEFEAT);
    CHECK(enemy_win.units[0].hp == 0 && enemy_win.units[1].hp == 1);
    CHECK(battle_score(enemy_win, BATTLE_ENEMY, 1) == BATTLE_SCORE_WIN - 1);
    CHECK(battle_score(enemy_win, BATTLE_PARTY, 1) == -(BATTLE_SCORE_WIN - 1));
    // A winning engagement by a healthy mover costs nothing either.
    BattleState clean = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                             {5, 2, ENEMY_WOLF, BATTLE_ENEMY, 1}});
    battle_apply(clean, act(0, 4, 2));
    CHECK(clean.ended && clean.result == BATTLE_VICTORY && clean.units[0].hp == 5);
    // An engagement that defeats some opponents but not the last still charges the price.
    BattleState partial = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                               {5, 2, ENEMY_WOLF, BATTLE_ENEMY, 1},
                                               {7, 3, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    battle_apply(partial, act(0, 4, 2));
    CHECK(!partial.ended && partial.units[1].hp == 0 && partial.units[0].hp == 4);

    // 26. A sacrifice that leaves an opponent standing loses when it removes the side's last unit.
    BattleState lost = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 1},
                                            {5, 2, ENEMY_WOLF, BATTLE_ENEMY, 5}});
    battle_apply(lost, act(0, 4, 2));
    CHECK(lost.ended && lost.result == BATTLE_DEFEAT && lost.units[1].hp == 4);
    BattleState enemy_lost = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 3},
                                                  {5, 1, ENEMY_WOLF, BATTLE_ENEMY, 1}});
    battle_apply(enemy_lost, act(1, 3, 1));
    CHECK(enemy_lost.ended && enemy_lost.result == BATTLE_VICTORY && enemy_lost.units[0].hp == 2);

    // A ranged kill of the last enemy wins at once; the archer pays nothing.
    BattleState v = fresh();
    v.units[WOLF].hp = 0;
    v.units[BRUTE].hp = 3;
    battle_apply(v, act(ARCHER, 5, 2));
    CHECK(v.ended && v.result == BATTLE_VICTORY && v.units[ARCHER].hp == 9 && v.activations == 1);
    CHECK(!battle_can_act(v) && !battle_legal(v, act(ROGUE, 4, 2)));
    // A brute's blow that kills the last party member is a defeat, and the dead are not pushed.
    BattleState d = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 2},
                                         {3, 1, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    battle_apply(d, act(1, 2, 1));
    CHECK(d.ended && d.result == BATTLE_DEFEAT && d.units[0].cell == cell(2, 1));
}

static void forced_pass() {
    // 29. An archer cornered by two adjacent enemies has no move and no shot, so its side passes,
    // and every pass costs 1 HP from the start of the battle.
    BattleState s = board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                         {1, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                         {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_can_act(s));
    CHECK(battle_settle(s) == 1 && s.turn == BATTLE_ENEMY && s.activations == 1 &&
          s.units[0].hp == 8);
    CHECK(s.units[1].hp == 10 && s.units[2].hp == 10); // A pass engages no one.
    // 30. Every living teammate pays, including voluntary passes when actions are available.
    BattleState p = fresh();
    battle_pass(p);
    CHECK(p.activations == 1 && p.units[ROGUE].hp == 9 && p.units[ARCHER].hp == 8);
    CHECK(p.units[WOLF].hp == 9 && p.units[BRUTE].hp == 12 && p.turn == BATTLE_ENEMY);
    BattleState enemy = fresh();
    enemy.turn = BATTLE_ENEMY;
    battle_pass(enemy);
    CHECK(enemy.activations == 1 && enemy.units[WOLF].hp == 8 && enemy.units[BRUTE].hp == 11);
    CHECK(enemy.units[ROGUE].hp == 10 && enemy.units[ARCHER].hp == 9 && enemy.turn == BATTLE_PARTY);
    BattleState q = fresh();
    q.units[ROGUE].hp = 0;
    battle_pass(q);
    CHECK(q.units[ARCHER].hp == 8);
    // 31-32. A pass can defeat its payer and end the battle.
    BattleState fatal = s;
    fatal.turn = BATTLE_PARTY;
    fatal.units[0].hp = 1;
    battle_pass(fatal);
    CHECK(fatal.units[0].hp == 0 && fatal.ended && fatal.result == BATTLE_DEFEAT);
    BattleState payer = board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 1},
                                             {7, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                             {1, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_pass(payer);
    CHECK(payer.units[0].hp == 0 && !payer.ended && payer.units[1].hp == 8);
    for(BATTLE_TEAM team : {BATTLE_PARTY, BATTLE_ENEMY}) {
        BattleState wiped = board(team, {{0, 0, BATTLE_TEST_ARCHER, team, 1},
                                         {7, 3, BATTLE_TEST_ARCHER, team, 1},
                                         {1, 0, BATTLE_TEST_FIGHTER, (BATTLE_TEAM)(team ^ 1), 10}});
        battle_pass(wiped);
        CHECK(wiped.ended && wiped.activations == 1 && !wiped.units[0].hp && !wiped.units[1].hp);
        CHECK(wiped.result == (team == BATTLE_PARTY ? BATTLE_DEFEAT : BATTLE_VICTORY));
        BattleState ended = wiped;
        battle_pass(wiped);
        CHECK(!memcmp(&wiped, &ended, sizeof(wiped)));
    }

    // 33. A unit that can move never passes, even when the move would cost its last HP.
    BattleState brink = board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 1},
                                             {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_can_act(brink) && battle_settle(brink) == 0 && brink.units[0].hp == 1);

    // 34. Settling always returns. The cornered archer passes on every party turn while the enemy's
    // archers step about; every pass and every step costs HP, so the battle ends within its total
    // HP.
    BattleState pinned = board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 3},
                                              {1, 0, BATTLE_TEST_ARCHER, BATTLE_ENEMY, 2},
                                              {0, 1, BATTLE_TEST_ARCHER, BATTLE_ENEMY, 2}});
    unsigned budget = total_hp(pinned), passes = 0;
    while(!pinned.ended) {
        unsigned before = total_hp(pinned);
        uint8_t settled = battle_settle(pinned);
        passes += settled;
        CHECK(settled == 0 || total_hp(pinned) < before);
        if(!pinned.ended)
            battle_apply(pinned, legal_set(pinned).actions[0]);
        CHECK(pinned.activations <= budget);
    }
    CHECK(passes >= 1);

    // A side that can act is never passed, and an ended battle is not passed either.
    BattleState v = fresh();
    CHECK(battle_settle(v) == 0 && v.turn == BATTLE_PARTY && v.activations == 0);
    v.ended = true;
    CHECK(battle_settle(v) == 0 && v.activations == 0);
}

// A party policy for the termination tests: 0 random, 1 greedy, 2 the enemy's own search, 3 evasive
// (the move that leaves the mover farthest from every enemy, attacking only when it cannot move).
static BattleAction policy_action(BattleState const& s, uint8_t policy) {
    ActionSet set = legal_set(s);
    if(policy == 0)
        return set.actions[next_random(set.count)];
    if(policy == 1)
        return battle_greedy_action(s);
    if(policy == 2)
        return battle_enemy_action(s);
    BattleAction best = set.actions[0];
    int best_distance = -1;
    for(uint8_t i = set.attacks; i < set.count; ++i) {
        int nearest = 99;
        for(uint8_t u = 0; u < s.unit_count; ++u)
            if(s.units[u].hp && s.units[u].team != s.turn) {
                int d = abs(battle_column(s.units[u].cell) -
                            battle_column(set.actions[i].destination)) +
                        abs(battle_row(s.units[u].cell) - battle_row(set.actions[i].destination));
                nearest = d < nearest ? d : nearest;
            }
        if(nearest > best_distance) {
            best_distance = nearest;
            best = set.actions[i];
        }
    }
    return best;
}

static void termination() {
    // The original enemy presets cannot heal, so every action reduces the total living HP.
    for(unsigned seed = 0; seed < 2000; ++seed) {
        BattleState s = seed & 1 ? random_board() : random_play(seed % 13, 0);
        if(s.ended)
            continue;
        unsigned before = total_hp(s);
        ActionSet set = legal_set(s);
        if(!set.count) {
            BattleState passed = s;
            battle_pass(passed);
            CHECK(total_hp(passed) < before);
            continue;
        }
        for(uint8_t i = 0; i < set.count; ++i) {
            BattleState t = s;
            battle_apply(t, set.actions[i]);
            CHECK(total_hp(t) < before && t.activations == s.activations + 1);
        }
    }

    // Whole battles end within as many activations as the units' starting HP, whatever either side
    // does, including a party that keeps running from contact.
    for(uint8_t policy = 0; policy < 4; ++policy) {
        for(unsigned game = 0; game < (policy == 0 ? 40u : 1u); ++game) {
            for(int encounter = 0; encounter < 2; ++encounter) {
                BattleState s = encounter ? random_board() : fresh();
                unsigned budget = s.activations + total_hp(s);
                while(!s.ended) {
                    unsigned before = total_hp(s);
                    battle_settle(s);
                    if(s.ended)
                        break;
                    BattleAction a =
                        s.turn == BATTLE_PARTY ? policy_action(s, policy) : battle_enemy_action(s);
                    CHECK(a.unit_index == BATTLE_NO_UNIT || battle_legal(s, a));
                    battle_apply(s, a);
                    CHECK(total_hp(s) < before);
                    CHECK(s.activations <= budget);
                }
                CHECK(s.activations <= budget);
            }
        }
    }
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
    BattleState original = board(BATTLE_PARTY, {{2, 1, ENEMY_BRUTE, BATTLE_PARTY, 6},
                                                {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    BattleState copy = original;
    battle_apply(copy, act(0, 3, 1));
    CHECK(original.units[1].cell == cell(3, 1) && original.units[1].hp == 5 &&
          original.turn == BATTLE_PARTY);
    CHECK(copy.units[1].cell == cell(4, 1) && copy.units[1].hp == 3 && copy.turn == BATTLE_ENEMY);
}

// --- Enumeration ---------------------------------------------------------------------------

// Checks every enumeration property at one position: each legal action appears exactly once,
// nothing illegal appears, attacks lead, encoding round-trips, per-unit lists agree, and the
// buffers are never overrun.
static void check_enumeration(BattleState const& s, bool use_base_oracle = true) {
    uint8_t packed[BATTLE_ACTION_CAPACITY + 8];
    memset(packed, 0xEE, sizeof(packed));
    uint8_t attacks = 0;
    uint8_t count = battle_actions(s, packed, attacks);
    CHECK(count <= BATTLE_ACTION_CAPACITY && attacks <= count);
    for(uint8_t i = count; i < sizeof(packed); ++i)
        CHECK(packed[i] == 0xEE);

    unsigned seen[BATTLE_UNIT_CAPACITY][BATTLE_CELLS] = {};
    for(uint8_t i = 0; i < count; ++i) {
        CHECK(packed[i] != battle_pass_marker(s));
        BattleAction action = battle_unpack(packed[i]);
        CHECK(battle_pack(action) == packed[i]);
        CHECK(action.unit_index < s.unit_count && action.destination < BATTLE_CELLS);
        if(use_base_oracle)
            CHECK(oracle_legal(s, action.unit_index, action.destination));
        CHECK(battle_legal(s, action));
        // Attacks come first, and each is an attack; the moves after them are moves.
        bool attack = battle_unit_at(s, action.destination) != BATTLE_NO_UNIT;
        CHECK(attack == (i < attacks));
        ++seen[action.unit_index][action.destination];
    }
    for(uint8_t u = 0; u < BATTLE_UNIT_CAPACITY; ++u)
        for(uint8_t c = 0; c < BATTLE_CELLS; ++c) {
            bool legal = use_base_oracle ? oracle_legal(s, u, c) : battle_legal(s, {u, c});
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
        uint8_t listed = 0, listed_attacks = 0;
        for(uint8_t i = 0; i < count; ++i) {
            if(battle_unpack(packed[i]).unit_index != u)
                continue;
            CHECK(listed < own_count && own[listed++] == packed[i]);
            listed_attacks += i < attacks;
        }
        CHECK(listed == own_count && listed_attacks == own_attacks);
        per_unit += own_count;
    }
    CHECK(per_unit == count);
    uint8_t none_attacks = 9;
    CHECK(battle_unit_actions(s, BATTLE_NO_UNIT, packed, none_attacks) == 0 && none_attacks == 0);
}

static void legal_action_enumeration() {
    check_enumeration(fresh());
    BattleState cultist = board(BATTLE_ENEMY, {{1, 1, ENEMY_HOODED_CULTIST, BATTLE_ENEMY, 14},
                                              {7, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    check_enumeration(cultist);
    CHECK(battle_legal(cultist, act(0, 7, 1)));
    BattleState enemy = fresh();
    enemy.turn = BATTLE_ENEMY;
    check_enumeration(enemy);

    // The Grove opening: the archer shot at the brute is the only attack.
    ActionSet opening = legal_set(fresh());
    CHECK(opening.attacks == 1 && same_action(opening.actions[0], act(ARCHER, 5, 2)));

    // Different archetypes in the same cell generate different action sets.
    uint32_t sets[BATTLE_TEST_SOURCE_COUNT];
    for(uint8_t t = 0; t < BATTLE_TEST_SOURCE_COUNT; ++t) {
        BattleState s = board(BATTLE_PARTY, {{3, 1, (ENEMY_ID)t, BATTLE_PARTY, 4},
                                             {3, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {6, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
        check_enumeration(s);
        sets[t] = destinations(s, 0);
    }
    // The stepping fighter and brute share moves and targets; the others differ from them and from
    // each other.
    CHECK(sets[BATTLE_TEST_FIGHTER] == sets[ENEMY_BRUTE]);
    uint8_t distinct[] = {BATTLE_TEST_FIGHTER, BATTLE_TEST_ROGUE, BATTLE_TEST_ARCHER,
                          BATTLE_TEST_SPEARMAN, ENEMY_WOLF};
    for(uint8_t a = 0; a < 5; ++a)
        for(uint8_t b = (uint8_t)(a + 1); b < 5; ++b)
            CHECK(sets[distinct[a]] != sets[distinct[b]]);
    // The spearman reaches the enemy two cells south; the archer reaches the one three cells east.
    CHECK(sets[BATTLE_TEST_SPEARMAN] & (1u << cell(3, 3)));
    CHECK(sets[BATTLE_TEST_ARCHER] & (1u << cell(6, 1)));
    CHECK(!(sets[BATTLE_TEST_FIGHTER] & (1u << cell(3, 3))));

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
    BattleState crowd = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                             {5, 2, ENEMY_WOLF, BATTLE_PARTY, 4},
                                             {4, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    check_enumeration(crowd);
    CHECK(legal_set(crowd).count > 2 * BATTLE_MOVE_LIMIT);
    // Four units on one side, an unreachable shape in play, still fits the buffer.
    BattleState four = board(BATTLE_PARTY, {{1, 1, ENEMY_WOLF, BATTLE_PARTY, 4},
                                            {6, 1, ENEMY_WOLF, BATTLE_PARTY, 4},
                                            {1, 2, BATTLE_TEST_ROGUE, BATTLE_PARTY, 4},
                                            {6, 2, BATTLE_TEST_ROGUE, BATTLE_PARTY, 4}});
    check_enumeration(four);

    // The capacity is the largest total over every split of the eight slots, and each per-unit
    // bound holds for every archetype.
    static_assert(BATTLE_ACTION_CAPACITY == 56, "eight units of seven moves");
    static_assert(BATTLE_UNIT_ACTION_CAPACITY == 14, "seven moves and seven attacks");
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
    // Every unit slot and cell round-trips through one byte.
    for(uint8_t u = 0; u < BATTLE_UNIT_CAPACITY; ++u)
        for(uint8_t c = 0; c < BATTLE_CELLS; ++c)
            CHECK(same_action(battle_unpack(battle_pack({u, c})), {u, c}));
    // The pass marker names unit 0 targeting its own cell, which is never legal, alive or defeated.
    for(unsigned seed = 0; seed < 2000; ++seed) {
        BattleState s = random_board();
        BattleAction marker = battle_unpack(battle_pass_marker(s));
        CHECK(marker.unit_index == 0 && marker.destination == s.units[0].cell);
        CHECK(!battle_legal(s, marker) && !oracle_legal(s, 0, s.units[0].cell));
        s.units[0].hp = 0;
        CHECK(!battle_legal(s, battle_unpack(battle_pass_marker(s))));
    }
}

// Units keep full-byte HP and source IDs independently of placement.
static void unit_packing() {
    BattleUnit unit{31, BATTLE_MAX_HP, ENEMY_BRUTE, BATTLE_ENEMY};
    CHECK(unit.cell == 31 && unit.hp == 255 && unit.id == ENEMY_BRUTE &&
          unit.team == BATTLE_ENEMY);
    unit.hp = (uint8_t)(unit.hp - 1);
    unit.cell = 0;
    CHECK(unit.cell == 0 && unit.hp == 254 && unit.id == ENEMY_BRUTE &&
          unit.team == BATTLE_ENEMY);
    for(uint8_t t = 0; t < BATTLE_TEST_SOURCE_COUNT; ++t) {
        unit.id = (ENEMY_ID)t;
        CHECK(unit.id == t && unit.hp == 254 && unit.cell == 0);
    }
    unit.team = BATTLE_PARTY;
    CHECK(unit.team == BATTLE_PARTY && unit.id == BATTLE_TEST_SOURCE_COUNT - 1);
    // The Grove encounter fits the side limit.
    BattleState grove = fresh();
    CHECK(battle_living(grove, BATTLE_PARTY) <= BATTLE_SIDE_CAPACITY &&
          battle_living(grove, BATTLE_ENEMY) <= BATTLE_SIDE_CAPACITY);
}

// --- Search ordering and scores -------------------------------------------------------------

static void search_ordering() {
    // Ordered lists hold the same actions, every group in priority order: wins, kills, other damage
    // (attacks and engaging moves), approach moves, other moves. The returned index starts the
    // quiet moves.
    for(unsigned seed = 0; seed < 600; ++seed) {
        BattleState s = seed & 1 ? random_board() : random_play(seed % 11, 0);
        if(s.ended)
            continue;
        uint8_t packed[BATTLE_ACTION_CAPACITY];
        uint8_t attacks = 0;
        uint8_t count = battle_actions(s, packed, attacks);
        uint8_t ordered[BATTLE_ACTION_CAPACITY];
        memcpy(ordered, packed, count);
        uint8_t quiet = battle_order_actions(s, ordered, count, attacks, true);
        CHECK(quiet <= count);
        unsigned previous = 0;
        uint8_t opponents = battle_living(s, s.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY);
        for(uint8_t i = 0; i < count; ++i) {
            BattleAction action = battle_unpack(ordered[i]);
            unsigned priority = battle_search_priority(s, action);
            CHECK(priority >= previous);
            previous = priority;
            CHECK(memchr(packed, ordered[i], count) != nullptr);
            BATTLE_ACTION_KIND kind = battle_action_kind(s, action);
            CHECK((kind == BATTLE_ACTION_MOVE) == (i >= quiet));
            // Kills use the attacker's damage against the victim, or a move's engagement hits.
            uint8_t victim = battle_unit_at(s, action.destination);
            unsigned defeated =
                victim != BATTLE_NO_UNIT
                    ? (s.units[victim].hp <= battle_attack_damage(s, action.unit_index, victim)
                           ? 1u
                           : 0u)
                    : battle_engagement(s, action.unit_index, action.destination).kills;
            CHECK((kind == BATTLE_ACTION_WIN) == (defeated && defeated == opponents));
            CHECK((kind == BATTLE_ACTION_KILL) == (defeated && defeated != opponents));
            // Applying the action agrees with its kind.
            BattleState t = s;
            battle_apply(t, action);
            bool party_acted = s.turn == BATTLE_PARTY;
            if(kind == BATTLE_ACTION_WIN)
                CHECK(t.ended && (t.result == BATTLE_VICTORY) == party_acted);
        }
    }
    // A brute's 2 HP blow kills only a victim with 2 HP or less.
    BattleState brute = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 3},
                                             {3, 1, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    CHECK(battle_action_kind(brute, act(1, 2, 1)) == BATTLE_ACTION_ATTACK);
    brute.units[0].hp = 2;
    CHECK(battle_action_kind(brute, act(1, 2, 1)) == BATTLE_ACTION_WIN);
    // A move that engages is damage, one that defeats an engaged enemy is a kill, and a move beside
    // no enemy is quiet.
    BattleState moves = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                             {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 2},
                                             {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_action_kind(moves, act(0, 3, 1)) == BATTLE_ACTION_ATTACK);
    CHECK(battle_action_kind(moves, act(0, 2, 0)) == BATTLE_ACTION_MOVE);
    moves.units[1].hp = 1;
    CHECK(battle_action_kind(moves, act(0, 3, 1)) == BATTLE_ACTION_KILL);
    // A killing move is ordered ahead of an attack that only damages.
    moves.units[2].cell = cell(2, 2);
    uint8_t list[BATTLE_ACTION_CAPACITY];
    uint8_t attacks = 0;
    uint8_t count = battle_actions(moves, list, attacks);
    battle_order_actions(moves, list, count, attacks, true);
    CHECK(list[0] == battle_pack(act(0, 3, 1)) && list[1] == battle_pack(act(0, 2, 2)));
}

static void terminal_scores() {
    // The enemy's winning blow leaves state.turn on the enemy; the score still names the enemy.
    BattleState win = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 2},
                                           {3, 1, ENEMY_WOLF, BATTLE_ENEMY, 4}});
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
    CHECK(largest > 0 && largest < BATTLE_SCORE_WIN - BATTLE_AI_DEPTH);
    // The extremes at the HP limit: four full-health units against one 1 HP unit, each way round,
    // in contact and far apart. Every static score stays far inside int16_t and below any terminal
    // score.
    for(int way = 0; way < 2; ++way)
        for(int spread = 0; spread < 2; ++spread) {
            BATTLE_TEAM strong = way ? BATTLE_ENEMY : BATTLE_PARTY;
            BATTLE_TEAM weak = way ? BATTLE_PARTY : BATTLE_ENEMY;
            BattleState e =
                board(BATTLE_ENEMY, {{0, 0, BATTLE_TEST_ARCHER, strong, BATTLE_MAX_HP},
                                     {0, 1, BATTLE_TEST_SPEARMAN, strong, BATTLE_MAX_HP},
                                     {0, 2, BATTLE_TEST_ARCHER, strong, BATTLE_MAX_HP},
                                     {0, 3, BATTLE_TEST_SPEARMAN, strong, BATTLE_MAX_HP},
                                     {spread ? 7u : 1u, 1, BATTLE_TEST_FIGHTER, weak, 1}});
            for(BATTLE_TEAM turn : {BATTLE_PARTY, BATTLE_ENEMY}) {
                e.turn = turn;
                int score = battle_score(e, BATTLE_ENEMY, 0);
                CHECK(score > -20000 && score < 20000);
            }
        }
    CHECK(BATTLE_SCORE_WIN - BATTLE_AI_DEPTH > (int16_t)largest);
}

static void attack_gaps() {
    // The gap counts the cells left to line up and come within range.
    CHECK(battle_attack_gap(BATTLE_TEST_FIGHTER, 1, 0) == 0);
    CHECK(battle_attack_gap(BATTLE_TEST_FIGHTER, 3, 2) == 4);
    CHECK(battle_attack_gap(BATTLE_TEST_FIGHTER, 1, 1) == 1);
    // An archer wants distance: adjacent is one cell too close, and two to four cells in line is in
    // range.
    CHECK(battle_attack_gap(BATTLE_TEST_ARCHER, 1, 0) == 1);
    CHECK(battle_attack_gap(BATTLE_TEST_ARCHER, 0, 3) == 0);
    CHECK(battle_attack_gap(BATTLE_TEST_ARCHER, 6, 1) == 3);
    CHECK(battle_attack_gap(BATTLE_TEST_ARCHER, 0, 0) == 2);
    // A spearman threatens two cells away.
    CHECK(battle_attack_gap(BATTLE_TEST_SPEARMAN, 2, 0) == 0 &&
          battle_attack_gap(BATTLE_TEST_SPEARMAN, 3, 0) == 1);
}

// The enemy's utility changes by exactly the amounts its weights name, on positions where nothing
// else changes.
static void utility_weights() {
    BattleState base = board(BATTLE_ENEMY, {{0, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5},
                                            {0, 3, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5},
                                            {6, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                            {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4}});
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
    wolves.units[3].id = ENEMY_WOLF;
    BattleState wolf_closer = wolves;
    wolf_closer.units[3].cell = cell(6, 3);
    CHECK(battle_score(wolf_closer, BATTLE_ENEMY, 0) - battle_score(wolves, BATTLE_ENEMY, 0) == 20);

    // Strikes count by the real rules. A party archer and a party spearman in the same cell differ
    // only in reach, so the scores differ only in strikes: with the enemy to act, an archer shot in
    // reply at range three costs the enemy 60, and the spearman cannot reach.
    BattleState archer = board(BATTLE_ENEMY, {{1, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4},
                                              {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                              {0, 3, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 4}});
    BattleState spearman = archer;
    spearman.units[0].id = BATTLE_TEST_SPEARMAN;
    CHECK(battle_score(archer, BATTLE_ENEMY, 0) - battle_score(spearman, BATTLE_ENEMY, 0) == -60);
    // A unit in the line blocks the shot, so the two score the same.
    BattleState archer_blocked = archer;
    archer_blocked.units[2].cell = cell(2, 1);
    BattleState spearman_blocked = archer_blocked;
    spearman_blocked.units[0].id = BATTLE_TEST_SPEARMAN;
    CHECK(battle_score(archer_blocked, BATTLE_ENEMY, 0) ==
          battle_score(spearman_blocked, BATTLE_ENEMY, 0));
    // An adjacent archer cannot shoot, so it threatens nothing either.
    BattleState archer_adjacent = archer;
    archer_adjacent.units[1].cell = cell(2, 1);
    BattleState spearman_adjacent = archer_adjacent;
    spearman_adjacent.units[0].id = BATTLE_TEST_SPEARMAN;
    CHECK(battle_score(archer_adjacent, BATTLE_ENEMY, 0) -
              battle_score(spearman_adjacent, BATTLE_ENEMY, 0) ==
          60);
}

// --- Search correctness ----------------------------------------------------------------------

// The search clamps its depth to what the action stack holds for the position, so the reference
// searches to the same clamped depth.
static void expect_same_as_reference(BattleState const& s, uint8_t depth) {
    BattleState before = s;
    BattleSearchResult fast = battle_search(s, depth);
    CHECK(same_state(before, s));
    uint8_t limit = battle_search_depth_limit(s);
    CHECK(limit >= 1 && limit <= BATTLE_AI_DEPTH);
    if(depth > limit)
        depth = limit;
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
        // Unpruned references grow quickly with more units, so larger boards stop at three plies.
        uint8_t deepest = seed % 4 == 0 && s.unit_count <= 4 ? BATTLE_AI_DEPTH : 3;
        for(uint8_t depth = 1; depth <= deepest; ++depth)
            expect_same_as_reference(s, depth);
        BattleAction chosen = battle_enemy_action(s);
        CHECK(chosen.unit_index == BATTLE_NO_UNIT || battle_legal(s, chosen) || s.ended);
    }
}

static void expect_completed(BattleState const& s, BattleSearchResult result);

static void search_matches_reference_on_engagements() {
    BattleState const positions[] = {
        // One engagement target, and two.
        board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                             {1, 2, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                             {4, 1, ENEMY_WOLF, BATTLE_ENEMY, 9},
                             {5, 2, ENEMY_BRUTE, BATTLE_ENEMY, 12}}),
        board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                             {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                             {4, 1, ENEMY_WOLF, BATTLE_ENEMY, 4},
                             {5, 2, ENEMY_BRUTE, BATTLE_ENEMY, 5}}),
        // Two engagement kills that do not end the battle, and a sacrifice that does.
        board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 1},
                             {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                             {4, 1, ENEMY_WOLF, BATTLE_ENEMY, 1},
                             {5, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 1},
                             {7, 0, ENEMY_BRUTE, BATTLE_ENEMY, 6}}),
        board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 1},
                             {7, 3, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 2},
                             {5, 1, ENEMY_WOLF, BATTLE_ENEMY, 1}}),
        // An unsuccessful sacrifice beside a sturdy enemy, and ordinary attacks with a knockback.
        board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 1},
                             {5, 0, ENEMY_WOLF, BATTLE_ENEMY, 6}}),
        board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                             {2, 2, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                             {3, 1, ENEMY_BRUTE, BATTLE_ENEMY, 12},
                             {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}}),
        // A blocked archer shot, and a forced pass.
        board(BATTLE_PARTY, {{0, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                             {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                             {4, 1, ENEMY_WOLF, BATTLE_ENEMY, 2}}),
        board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 2},
                             {1, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                             {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}}),
    };
    for(BattleState const& position : positions) {
        for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
            expect_same_as_reference(position, depth);
        // The budgeted choice is a completed iteration's and legal, and repeats exactly.
        BattleState copy = position;
        BattleSearchResult first =
            battle_think(position, BATTLE_AI_DEPTH, battle_ai_budget(position));
        BattleSearchResult again =
            battle_think(position, BATTLE_AI_DEPTH, battle_ai_budget(position));
        CHECK(same_state(copy, position) && same_action(first.action, again.action));
        CHECK(first.score == again.score &&
              battle_search_stats().nodes <= battle_ai_budget(position));
        expect_completed(position, first);
    }
}

static void search_simulates_mechanics() {
    // A knockback that moves the last enemy out of a lethal line is found and scored correctly.
    BattleState push = board(BATTLE_PARTY, {{3, 1, ENEMY_BRUTE, BATTLE_PARTY, 6},
                                            {4, 1, BATTLE_TEST_SPEARMAN, BATTLE_ENEMY, 5},
                                            {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4}});
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
        expect_same_as_reference(push, depth);

    // A ranged kill of the last enemy is a win one ply deep, and it comes first.
    BattleState shot = board(BATTLE_PARTY, {{0, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4},
                                            {4, 1, ENEMY_WOLF, BATTLE_ENEMY, 2}});
    BattleSearchResult won = battle_search(shot, BATTLE_AI_DEPTH);
    CHECK(same_action(won.action, act(0, 4, 1)) && won.score == BATTLE_SCORE_WIN - 1);
    // The win ends the root's search at its first child.
    CHECK(battle_search_stats().nodes == 1);

    // A wolf sprint and a knight leap reach the last enemy for a kill on the next turn.
    BattleState sprint = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 2},
                                              {5, 1, ENEMY_WOLF, BATTLE_ENEMY, 4}});
    expect_same_as_reference(sprint, 3);
    BattleState leap = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 2},
                                            {5, 3, BATTLE_TEST_ROGUE, BATTLE_ENEMY, 4}});
    expect_same_as_reference(leap, 3);

    // A forced pass inside the tree: the cornered archer passes, and the search passes with it.
    BattleState cornered = board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4},
                                                {1, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                                {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
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

static void voluntary_pass_search() {
    BattleState positions[] = {
        board(BATTLE_PARTY, {{2, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 3},
                             {4, 2, BATTLE_TEST_SPEARMAN, BATTLE_ENEMY, 11}}),
        board(BATTLE_ENEMY, {{0, 0, ENEMY_BRUTE, BATTLE_PARTY, 4},
                             {0, 1, BATTLE_TEST_ARCHER, BATTLE_ENEMY, 12}}),
    };
    for(BattleState const& s : positions) {
        BattleState before = s;
        CHECK(battle_can_act(s));
        BattleSearchResult best = battle_search(s, 3);
        CHECK(best.action.unit_index == BATTLE_NO_UNIT);
        CHECK(reference_action_value(s, {BATTLE_NO_UNIT, 0}, 3) == best.score);
        ActionSet actions = legal_set(s);
        for(unsigned i = 0; i < actions.count; ++i)
            CHECK(reference_action_value(s, actions.actions[i], 3) < best.score);
        CHECK(battle_enemy_action(s).unit_index == BATTLE_NO_UNIT);
        expect_completed(s, battle_think(s, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET));
        for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
            expect_same_as_reference(s, depth);
        CHECK(same_state(before, s));
    }
}

static void sacrifice_search() {
    // A lone 1 HP wolf with no attack falls to the blood price of any move, so every action loses
    // at once, and the search reports that loss one ply deep.
    BattleState doomed = board(BATTLE_ENEMY, {{0, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                              {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                              {7, 3, ENEMY_WOLF, BATTLE_ENEMY, 1}});
    CHECK(battle_search(doomed, BATTLE_AI_DEPTH).score == -(BATTLE_SCORE_WIN - 1));
    BattleState healthier = doomed;
    healthier.units[2].hp = 2;
    CHECK(battle_search(healthier, BATTLE_AI_DEPTH).score > -(BATTLE_SCORE_WIN - 1));

    // Self-elimination is avoided: the 1 HP wolf would fall if it moved, so the healthy brute acts.
    BattleState choice = board(BATTLE_ENEMY, {{0, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                              {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                              {5, 1, ENEMY_WOLF, BATTLE_ENEMY, 1},
                                              {5, 2, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    BattleAction spare = battle_enemy_action(choice);
    CHECK(spare.unit_index == 3 && battle_legal(choice, spare));

    // A winning sacrifice is found at once and scored as a win one ply deep: the enemy's last wolf
    // runs beside the party's last archer, and both fall.
    BattleState sacrifice = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 1},
                                                 {5, 1, ENEMY_WOLF, BATTLE_ENEMY, 1}});
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth) {
        BattleSearchResult won = battle_search(sacrifice, depth);
        CHECK(same_action(won.action, act(1, 3, 1)) && won.score == BATTLE_SCORE_WIN - 1);
        expect_same_as_reference(sacrifice, depth);
    }
    CHECK(same_action(battle_enemy_action(sacrifice), act(1, 3, 1)));
    CHECK(battle_think(sacrifice, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET).depth == 1);
    CHECK(same_action(battle_greedy_action(sacrifice), act(1, 3, 1)));
    // The party's version: its last rogue leaps between two 1 HP enemies, the last of the enemy
    // side.
    BattleState double_kill = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 1},
                                                   {4, 1, ENEMY_WOLF, BATTLE_ENEMY, 1},
                                                   {5, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 1}});
    CHECK(battle_legal(double_kill, act(0, 4, 2)));
    BattleSearchResult both = battle_search(double_kill, 2);
    CHECK(same_action(both.action, act(0, 4, 2)) && both.score == BATTLE_SCORE_WIN - 1);
    CHECK(battle_search_priority(double_kill, act(0, 4, 2)) == BATTLE_PRIORITY_WIN);
    CHECK(same_action(battle_greedy_action(double_kill), act(0, 4, 2)));

    // A losing sacrifice is avoided: the last wolf, at 1 HP beside a 9 HP archer, strikes rather
    // than move, since any move would cost its last HP and lose.
    BattleState hopeless = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                                {3, 1, ENEMY_WOLF, BATTLE_ENEMY, 1}});
    BattleAction bite = battle_enemy_action(hopeless);
    CHECK(same_action(bite, act(1, 2, 1)));
    CHECK(same_action(battle_greedy_action(hopeless), act(1, 2, 1)));
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth)
        expect_same_as_reference(hopeless, depth);
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
    CHECK(battle_search_stats().nodes == legal + 1 && battle_search_stats().leaves == legal + 1);
    // Pruning visits fewer positions than the unpruned reference, and the counts repeat.
    battle_search(root, BATTLE_AI_DEPTH);
    BattleSearchStats first = battle_search_stats();
    battle_search(root, BATTLE_AI_DEPTH);
    BattleSearchStats second = battle_search_stats();
    CHECK(first.nodes == second.nodes && first.leaves == second.leaves);
    uint64_t unpruned = 0;
    reference_search(root, BATTLE_AI_DEPTH, unpruned);
    CHECK(first.nodes < unpruned && first.leaves <= first.nodes && first.leaves > 0);

    // The workspace: four plies of 4 bytes, four killer bytes, the 82-byte shared action scratch
    // (two plies of four rogues a side, four of any two-on-two battle), and four compact positions.
    CHECK(battle_search_workspace_bytes() == 4 * 4 + 4 + 82 + 4 * sizeof(BattleState));
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
    CHECK(result.action.unit_index == BATTLE_NO_UNIT || battle_legal(s, result.action));
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
    BattleState win = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 2},
                                           {3, 1, ENEMY_WOLF, BATTLE_ENEMY, 4},
                                           {7, 3, ENEMY_BRUTE, BATTLE_ENEMY, 6}});
    BattleSearchResult quick = battle_think(win, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET);
    CHECK(same_action(quick.action, act(1, 2, 1)) && quick.score == BATTLE_SCORE_WIN - 1);
    CHECK(battle_search_stats().completed == 1 && battle_search_stats().nodes == 1);
    CHECK(same_action(battle_enemy_action(win), act(1, 2, 1)));

    // No legal action: the result is a pass, from a completed iteration.
    BattleState cornered = board(BATTLE_ENEMY, {{1, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5},
                                                {0, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5},
                                                {0, 0, BATTLE_TEST_ARCHER, BATTLE_ENEMY, 4}});
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
        unsigned gap = battle_attack_gap(s.units[unit].id, (uint8_t)dx, (uint8_t)dy);
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
    BattleState sprint = board(BATTLE_ENEMY, {{0, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                              {7, 1, ENEMY_WOLF, BATTLE_ENEMY, 9}});
    BattleAction run = battle_enemy_action(sprint);
    CHECK(run.unit_index == 1 && battle_unit_at(sprint, run.destination) == BATTLE_NO_UNIT);
    unsigned run_length =
        (unsigned)(abs(battle_column(run.destination) - 7) + abs(battle_row(run.destination) - 1));
    CHECK(run_length == 2);
    CHECK(attack_gap_to_party(sprint, 1, run.destination) + 2 ==
          attack_gap_to_party(sprint, 1, cell(7, 1)));

    // Brute knockback: the 3 HP fighter cannot escape, the party fighter beside it would finish it
    // with one blow, and the adjacent archer cannot shoot. The brute's blow pushes the party
    // fighter away.
    BattleState knock = board(BATTLE_ENEMY, {{1, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                             {0, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                             {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 3},
                                             {1, 0, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    CHECK(threatened(knock, 2));
    BattleAction shove = battle_enemy_action(knock);
    CHECK(same_action(shove, act(3, 1, 1)));
    BattleState shoved = knock;
    battle_apply(shoved, shove);
    CHECK(shoved.units[0].cell == cell(1, 2) && !threatened(shoved, 2));

    // Archer minimum range: an enemy fighter beside an archer strikes it rather than step into its
    // line of fire.
    BattleState close = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                             {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 3},
                                             {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!threatened(close, 2));
    BattleAction stay = battle_enemy_action(close);
    BattleState after = close;
    battle_apply(after, stay);
    CHECK(!after.ended && !threatened(after, 2));
    CHECK(same_action(stay, act(2, 2, 1)));

    // Spearman reach: a 4 HP wolf two cells from a spearman leaves the thrust's reach (a run costs
    // it 1, and a thrust would then finish it).
    BattleState reach = board(BATTLE_ENEMY, {{2, 1, BATTLE_TEST_SPEARMAN, BATTLE_PARTY, 10},
                                             {0, 3, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                             {4, 1, ENEMY_WOLF, BATTLE_ENEMY, 4},
                                             {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(threatened(reach, 2));
    BattleAction away = battle_enemy_action(reach);
    BattleState moved = reach;
    battle_apply(moved, away);
    CHECK(!threatened(moved, 2));

    // Blocked line: an injured enemy in an archer's open line moves out of it or behind cover.
    BattleState open = board(BATTLE_ENEMY, {{0, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                            {0, 3, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                            {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                            {6, 2, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    CHECK(threatened(open, 2));
    BattleState sheltered = open;
    battle_apply(sheltered, battle_enemy_action(open));
    CHECK(!threatened(sheltered, 2));
    // A friendly unit in the line already shelters it, so the archer's shot is not counted.
    BattleState covered = open;
    place(covered, 3, 2, 1);
    CHECK(!threatened(covered, 2));

    // Tactical aggression: a healthy enemy beside a hero attacks it rather than stepping back.
    BattleState adjacent = board(BATTLE_ENEMY, {{3, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                                {0, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 9},
                                                {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {7, 3, ENEMY_WOLF, BATTLE_ENEMY, 9}});
    BattleAction strike = battle_enemy_action(adjacent);
    CHECK(battle_unit_at(adjacent, strike.destination) != BATTLE_NO_UNIT);
    // A kill is taken.
    BattleState finish = adjacent;
    finish.units[0].hp = 3;
    CHECK(same_action(battle_enemy_action(finish), act(2, 3, 1)));
    // With nothing in reach, the enemy closes in.
    BattleState far = board(BATTLE_ENEMY, {{1, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                           {1, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                           {6, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                           {7, 2, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
    BattleAction advance = battle_enemy_action(far);
    CHECK(battle_unit_at(far, advance.destination) == BATTLE_NO_UNIT);
    CHECK(attack_gap_to_party(far, advance.unit_index, advance.destination) <
          attack_gap_to_party(far, advance.unit_index, far.units[advance.unit_index].cell));
}

static void party_search_uses_leaps() {
    // Rogue leap: the 2 HP enemy brute stands at (0,1). The party rogue at (2,1) reaches the corner
    // beside it, (0,0), only by a knight leap: one orthogonal path to it passes the brute and the
    // other the party's own fighter. The leap engages the brute on landing, and the same search the
    // enemy uses, run for the party, finds the forced win within three plies, whatever the brute
    // does.
    BattleState wall = board(BATTLE_PARTY, {{2, 1, BATTLE_TEST_ROGUE, BATTLE_PARTY, 10},
                                            {2, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                            {0, 1, ENEMY_BRUTE, BATTLE_ENEMY, 2}});
    CHECK(legal_set(wall).attacks == 0 && battle_legal(wall, act(0, 0, 0)));
    CHECK(battle_unit_at(wall, cell(1, 1)) == BATTLE_NO_UNIT &&
          battle_unit_at(wall, cell(0, 1)) == 2);
    BattleSearchResult leap = battle_search(wall, 3);
    CHECK(same_action(leap.action, act(0, 0, 0)));
    CHECK(leap.score >= BATTLE_SCORE_WIN - 3);
    // Two plies cannot see the win, so the leap is found by searching through the brute's reply.
    CHECK(battle_search(wall, 2).score < BATTLE_SCORE_WIN - 3);
}

// --- Three and four units a side -----------------------------------------------------------

// A four-on-four encounter: the rules, the copied state, and the encounter table all hold eight
// units.
static constexpr BattleFixture PROGMEM TEST_FOUR_A_SIDE{
    BATTLEFIELD_GROVE,
    8,
    {
        {battle_cell(1, 0), 5, BATTLE_TEST_FIGHTER, BATTLE_PARTY},
        {battle_cell(1, 1), 5, BATTLE_TEST_ROGUE, BATTLE_PARTY},
        {battle_cell(0, 2), 4, BATTLE_TEST_ARCHER, BATTLE_PARTY},
        {battle_cell(1, 3), 5, BATTLE_TEST_SPEARMAN, BATTLE_PARTY},
        {battle_cell(6, 0), 4, ENEMY_WOLF, BATTLE_ENEMY},
        {battle_cell(6, 1), 6, ENEMY_BRUTE, BATTLE_ENEMY},
        {battle_cell(7, 2), 4, BATTLE_TEST_ARCHER, BATTLE_ENEMY},
        {battle_cell(6, 3), BATTLE_MAX_HP, BATTLE_TEST_FIGHTER, BATTLE_ENEMY},
    },
};

// The most legal actions one unit of each type can have on the 8x4 board, restated from the
// specification: a stepping unit gets a move or an attack in each direction; an archer, a spearman,
// or a wolf at most two per direction, and only one toward the near edge of a four-row board; a
// rogue at most six leaps (two of its eight leave a four-row board from any cell) and four adjacent
// attacks. Attacks never exceed the opponents.
static unsigned oracle_unit_limit(uint8_t type, unsigned opponents) {
    OracleType rules = oracle_type(type);
    unsigned moves = rules.movement == BATTLE_MOVEMENT_LEAP ? 6 : rules.move_range == 2 ? 7 : 4;
    unsigned per_type = type == BATTLE_TEST_FIGHTER || type == ENEMY_BRUTE ? 4
                        : type == BATTLE_TEST_ROGUE                              ? 10
                                                                                 : 7;
    return per_type < moves + opponents ? per_type : moves + opponents;
}

static void four_a_side() {
    BattleState s;
    battle_fixture_begin(s, &TEST_FOUR_A_SIDE);
    CHECK(s.unit_count == 8 && sizeof(BattleState) == 39);
    CHECK(battle_living(s, BATTLE_PARTY) == 4 && battle_living(s, BATTLE_ENEMY) == 4);
    for(uint8_t i = 0; i < 8; ++i)
        CHECK(battle_unit_at(s, s.units[i].cell) == i);
    CHECK(s.units[7].hp == BATTLE_MAX_HP && s.units[7].id == BATTLE_TEST_FIGHTER);
    check_enumeration(s);
    BattleState enemy = s;
    enemy.turn = BATTLE_ENEMY;
    check_enumeration(enemy);
    // Units in the high slots act like any other: the enemy fighter in slot 7 steps, and slot 6's
    // archer has a clear shot along row 2 at the party archer five cells away only once it closes
    // in.
    CHECK(battle_legal(enemy, act(7, 5, 3)) && !battle_legal(enemy, act(6, 0, 2)));
    battle_apply(enemy, act(6, 6, 2));
    CHECK(enemy.units[6].cell == cell(6, 2) && enemy.turn == BATTLE_PARTY);

    // The per-unit limits behind the search's stack bound hold on random boards of every size.
    for(unsigned seed = 0; seed < 3000; ++seed) {
        BattleState r = random_board();
        for(uint8_t u = 0; u < r.unit_count; ++u) {
            if(!r.units[u].hp || r.units[u].team != r.turn)
                continue;
            uint8_t packed[BATTLE_UNIT_ACTION_CAPACITY];
            uint8_t attacks = 0;
            unsigned count = battle_unit_actions(r, u, packed, attacks);
            unsigned opponents =
                battle_living(r, r.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY);
            CHECK(count <= oracle_unit_limit(r.units[u].id, opponents));
        }
    }

    // The search's depth follows the composition: any two-on-two battle and the Grove get four
    // plies, a mixed four-on-four battle three, and four rogues a side still two.
    CHECK(battle_search_depth_limit(fresh()) == BATTLE_AI_DEPTH);
    BattleState rogues2 = board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                               {0, 3, BATTLE_TEST_ROGUE, BATTLE_PARTY, 5},
                                               {7, 0, BATTLE_TEST_ROGUE, BATTLE_ENEMY, 5},
                                               {7, 3, BATTLE_TEST_ROGUE, BATTLE_ENEMY, 5}});
    CHECK(battle_search_depth_limit(rogues2) == BATTLE_AI_DEPTH);
    CHECK(battle_search_depth_limit(s) == 3);
    BattleState rogues4 = s;
    for(uint8_t i = 0; i < 8; ++i)
        rogues4.units[i].id = BATTLE_TEST_ROGUE;
    CHECK(battle_search_depth_limit(rogues4) == 2);
    // A defeated unit no longer counts, so the limit can only rise as a battle goes on.
    BattleState fewer = rogues4;
    fewer.units[0].hp = 0;
    fewer.units[1].hp = 0;
    CHECK(battle_search_depth_limit(fewer) >= battle_search_depth_limit(rogues4));

    // The search matches the unpruned reference on four-a-side and three-a-side positions, at every
    // depth the stack allows (three plies at most, to keep the reference fast).
    expect_same_as_reference(s, 1);
    expect_same_as_reference(s, 2);
    expect_same_as_reference(enemy, 2);
    expect_same_as_reference(rogues4, 3);
    CHECK(battle_search_stats().completed == 2);
    for(unsigned seed = 0; seed < 60; ++seed) {
        BattleState r = random_board();
        if(r.unit_count < 6)
            continue;
        expect_same_as_reference(r, 1);
        expect_same_as_reference(r, 2);
        if(seed % 4 == 0)
            expect_same_as_reference(r, 3);
    }

    // The enemy's budgeted choice on a full board is a completed iteration's, and legal.
    // The budget scales down with more living units.
    CHECK(battle_ai_budget(fresh()) == BATTLE_AI_NODE_BUDGET && battle_ai_budget(enemy) == 240);
    BattleState six = enemy;
    six.units[0].hp = 0;
    six.units[4].hp = 0;
    CHECK(battle_ai_budget(six) == 320);
    BattleSearchResult think = battle_think(enemy, BATTLE_AI_DEPTH, battle_ai_budget(enemy));
    CHECK(think.depth >= 1 && battle_search_stats().nodes <= battle_ai_budget(enemy));
    CHECK(think.action.unit_index == BATTLE_NO_UNIT || battle_legal(enemy, think.action));
    CHECK(battle_enemy_action(enemy).unit_index == BATTLE_NO_UNIT ||
          battle_legal(enemy, battle_enemy_action(enemy)));

    // A forced pass on a full board: the cornered archer's whole side is boxed in.
    BattleState boxed = board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4},
                                             {1, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {7, 3, BATTLE_TEST_ARCHER, BATTLE_PARTY, 4},
                                             {6, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {7, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(!battle_can_act(boxed));
    BattleSearchResult passed = battle_search(boxed, 3);
    CHECK(passed.action.unit_index == BATTLE_NO_UNIT && passed.depth == 3);
    expect_same_as_reference(boxed, 3);
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

// An arrow starts half a cell out from its archer and lands half a cell short of its target's cell,
// four pixels a frame: 5 frames at range 2, 9 at range 3, 13 at range 4.
static void arrow_timing() {
    static_assert(BATTLE_CELL_PIXELS == TILE, "an arrow crosses whole cells");
    static_assert(BATTLE_ARROW_FRAMES == 4 && NORTH == 0 && EAST == 1 && SOUTH == 2 && WEST == 3,
                  "an arrow's frame is its direction");
    CHECK(battle_arrow_frames(2) == 5 && battle_arrow_frames(3) == 9 &&
          battle_arrow_frames(4) == 13);
    for(uint8_t cells = 2; cells <= 4; ++cells) {
        uint8_t frames = battle_arrow_frames(cells);
        CHECK(battle_arrow_offset(0) == TILE / 2);
        CHECK(battle_arrow_offset((uint8_t)(frames - 1)) == cells * TILE - TILE / 2);
        for(uint8_t frame = 1; frame < frames; ++frame)
            CHECK(battle_arrow_offset(frame) - battle_arrow_offset((uint8_t)(frame - 1)) ==
                  BATTLE_ARROW_PIXELS);
    }
}

static void leap_arc() {
    // A leap covers its two long-axis cells in BATTLE_LEAP_FRAMES frames, rising from the start and
    // landing level, highest at mid-flight, and the same on the way down as on the way up.
    static_assert(BATTLE_LEAP_FRAMES * BATTLE_LEAP_PIXELS == 2 * TILE, "a leap lands on its cell");
    CHECK(battle_leap_lift(2 * TILE) == 0 && battle_leap_lift(0) == 0);
    CHECK(battle_leap_lift(TILE) == BATTLE_LEAP_HEIGHT);
    for(uint8_t left = 0; left <= BATTLE_LEAP_FRAMES; ++left) {
        uint8_t remaining = (uint8_t)(left * BATTLE_LEAP_PIXELS);
        CHECK(battle_leap_lift(remaining) == battle_leap_lift((uint8_t)(2 * TILE - remaining)));
        CHECK(battle_leap_lift(remaining) <= BATTLE_LEAP_HEIGHT);
        if(left > 0 && left <= BATTLE_LEAP_FRAMES / 2)
            CHECK(battle_leap_lift(remaining) >
                  battle_leap_lift((uint8_t)(remaining - BATTLE_LEAP_PIXELS)));
    }
}

static void thinking_indicator() {
    // The box sits at the screen's center and holds its three dots with a margin on every side.
    CHECK(BATTLE_THINKING_X * 2 + BATTLE_THINKING_WIDTH == 128 + 1 ||
          BATTLE_THINKING_X * 2 + BATTLE_THINKING_WIDTH == 128 - 1 ||
          BATTLE_THINKING_X * 2 + BATTLE_THINKING_WIDTH == 128);
    CHECK(BATTLE_THINKING_Y * 2 + BATTLE_THINKING_HEIGHT >= 63 &&
          BATTLE_THINKING_Y * 2 + BATTLE_THINKING_HEIGHT <= 65);
    CHECK(battle_thinking_dot_x(0) > BATTLE_THINKING_X + 1);
    CHECK(battle_thinking_dot_x(BATTLE_THINKING_DOTS - 1) + BATTLE_THINKING_DOT <
          BATTLE_THINKING_X + BATTLE_THINKING_WIDTH - 1);
    CHECK(battle_thinking_dot_x(0) - BATTLE_THINKING_X ==
          BATTLE_THINKING_X + BATTLE_THINKING_WIDTH -
              (battle_thinking_dot_x(BATTLE_THINKING_DOTS - 1) + BATTLE_THINKING_DOT));
    // Steps light one more dot each, then none.
    CHECK(battle_thinking_lit(1) == 1 && battle_thinking_lit(3) == 3 &&
          battle_thinking_lit(4) == 0 && battle_thinking_lit(5) == 1);

    // A budgeted search reports one step per budget / BATTLE_AI_PROGRESS_STEPS positions over all
    // its iterations, the first after one interval; an unbudgeted one reports none.
    unsigned searched = 0;
    for(unsigned trial = 0; trial < 120; ++trial) {
        BattleState s = random_board();
        if(s.ended)
            continue;
        uint16_t budget = battle_ai_budget(s);
        battle_think(s, BATTLE_AI_DEPTH, budget);
        BattleSearchStats st = battle_search_stats();
        CHECK(st.progress == st.nodes / (budget / BATTLE_AI_PROGRESS_STEPS));
        CHECK(st.progress <= BATTLE_AI_PROGRESS_STEPS);
        battle_search(s, 2);
        CHECK(battle_search_stats().progress == 0);
        ++searched;
    }
    CHECK(searched > 50);
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
        CHECK(attack >= BATTLE_TARGET_ATTACK &&
              attack < BATTLE_TARGET_ATTACK + BATTLE_TARGET_FRAMES);
    }
}

static void random_battle_deployment() {
    CHECK(battlefields_for_area(AREA_OUTDOOR) == BATTLEFIELDS_WORLD);
    CHECK(battlefields_for_tileset(TILESET_CAVE) == BATTLEFIELDS_CAVE);
    CHECK(battlefields_for_tileset(TILESET_DUNGEON) == BATTLEFIELDS_DUNGEON);
    CHECK(battlefields_for_area(AREA_INTERIORS) == BATTLEFIELDS_HOUSE);

    bool obstacle_seen[TILESET_COUNT][256]{};
    uint8_t obstacle_types[TILESET_COUNT]{};
    for(uint8_t field = 0; field < BATTLEFIELD_COUNT; ++field) {
        auto const& board = BATTLEFIELDS[field];
        CHECK(battlefields_for_tileset(board.tileset) & battlefields(field));
        uint8_t party_cells = 0, enemy_cells = 0;
        uint8_t sparse = 0, dense = 0, flowers = 0;
        for(uint8_t cell = 0; cell < BATTLE_CELLS; ++cell) {
            uint8_t x = battle_column(cell), y = battle_row(cell);
            bool party = board.party_area.contains(x, y);
            bool enemy = board.enemy_area.contains(x, y);
            CHECK(!party || !enemy);
            CHECK(battle_blocked(field, cell) == oracle_blocked(field, x, y));
            uint8_t tile = board.tiles[cell];
            if(battle_blocked(field, cell)) {
                if(!obstacle_seen[board.tileset][tile]) {
                    obstacle_seen[board.tileset][tile] = true;
                    ++obstacle_types[board.tileset];
                }
            } else if(board.tileset == TILESET_WORLD) {
                sparse += tile == TILE_GRASS_08 || tile == TILE_GRASS_09 ||
                          tile == TILE_GRASS_0A || tile == TILE_GRASS_59;
                dense += tile == TILE_GRASS_06 || tile == TILE_GRASS_07 ||
                         tile == TILE_GRASS_16 || tile == TILE_GRASS_17;
                flowers += tile == TILE_FLOWERS_26 || tile == TILE_FLOWERS_27;
            }
            party_cells += party && !battle_blocked(field, cell);
            enemy_cells += enemy && !battle_blocked(field, cell);
            for(uint8_t other = 0; other < BATTLE_CELLS; ++other) {
                int step = 0;
                if(cell != other && y == battle_row(other))
                    step = cell < other ? 1 : -1;
                else if(cell != other && x == battle_column(other))
                    step = cell < other ? BATTLE_COLUMNS : -BATTLE_COLUMNS;
                bool screened = false;
                if(step)
                    for(int between = cell + step; between != other; between += step)
                        screened |= oracle_blocked(field, battle_column((uint8_t)between),
                                                   battle_row((uint8_t)between));
                CHECK(((board.screened[cell][other / 8] >> (other % 8)) & 1) == screened);
            }
        }
        CHECK(party_cells >= BATTLE_SIDE_CAPACITY && enemy_cells >= BATTLE_SIDE_CAPACITY);
        if(board.tileset == TILESET_WORLD)
            CHECK(sparse && dense && flowers >= 3 && flowers <= 4);
    }
    for(uint8_t count : obstacle_types)
        CHECK(count >= 2);

    constexpr auto selection =
        battlefields(BATTLEFIELD_MEADOW, BATTLEFIELD_MINE_CROSSING, BATTLEFIELD_HOUSE_HALL);
    constexpr BattleEncounter encounter =
        battle_encounter(selection, ENEMY_WOLF, ENEMY_BRUTE, ENEMY_WOLF, ENEMY_BRUTE);
    CHECK(battle_encounter_valid(&encounter));
    CHECK(!battle_encounter_valid(nullptr));
    for(auto fields : {BattlefieldSelection(0), battlefields(BATTLEFIELD_COUNT)}) {
        auto invalid = encounter;
        invalid.fields = fields;
        CHECK(!battle_encounter_valid(&invalid));
        BattleState state;
        battle_begin(state, &invalid);
        CHECK(state.ended && state.result == BATTLE_DEFEAT && !state.unit_count);
    }
    auto invalid = encounter;
    invalid.enemies[3] = BATTLE_TEST_FIGHTER;
    CHECK(!battle_encounter_valid(&invalid));

    game = {};
    game.party.count = PARTY_CAPACITY;
    for(auto& member : game.party.members)
        member.hp = 10;
    BattlefieldSelection seen = 0;
    uint32_t party_positions = 0, enemy_positions = 0;
    for(uint16_t seed = 1; seed <= 256; ++seed) {
        game.rng = seed;
        BattleState state;
        battle_begin(state, &encounter);
        CHECK(state.unit_count == BATTLE_UNIT_CAPACITY);
        CHECK(selection & battlefields(state.field));
        seen |= battlefields(state.field);
        uint32_t occupied = 0;
        for(uint8_t i = 0; i < state.unit_count; ++i) {
            auto const& unit = state.units[i];
            auto const& board = BATTLEFIELDS[state.field];
            auto const& area = unit.team == BATTLE_PARTY ? board.party_area : board.enemy_area;
            uint32_t bit = uint32_t(1) << unit.cell;
            CHECK(area.contains(battle_column(unit.cell), battle_row(unit.cell)));
            CHECK(!battle_blocked(state.field, unit.cell) && !(occupied & bit));
            occupied |= bit;
        }
        party_positions |= uint32_t(1) << state.units[0].cell;
        enemy_positions |= uint32_t(1) << state.units[BATTLE_SIDE_CAPACITY].cell;
        CHECK(game.rng != seed);
        game.rng = seed;
        BattleState repeated;
        battle_begin(repeated, &encounter);
        CHECK(!memcmp(&state, &repeated, sizeof(state)));
    }
    CHECK(seen == selection);
    CHECK((party_positions & (party_positions - 1)) && (enemy_positions & (enemy_positions - 1)));
}

static constexpr BattleEncounter MIXED_CAVE_FIXTURE =
    battle_encounter(BATTLEFIELDS_CAVE, ENEMY_WOLF, ENEMY_BRUTE);

static void saved_party_battles() {
    game = {};
    game.party.count = 2;
    game.party.members[0] = {12, 4, MOVEMENT_LEAP, ATTACK_SHOOT, NPC_ALDEN, 1, 0};
    game.party.members[1] = {11, 4, MOVEMENT_CHARGE, ATTACK_IMPACT, NPC_ELIZA, 1, 0};
    Party saved = game.party;
    BattleState state;
    battle_begin(state, &MIXED_CAVE_FIXTURE);
    CHECK(state.unit_count == 4 && state.units[0].hp == 12 && state.units[1].hp == 11);
    CHECK(state.units[0].id == battle_party_id(0) && state.units[1].id == battle_party_id(1));
    CHECK(state.units[2].id == ENEMY_WOLF && state.units[2].hp == ENEMY_INFO[ENEMY_WOLF].max_hp);
    CHECK(state.units[3].id == ENEMY_BRUTE && state.units[3].hp == ENEMY_INFO[ENEMY_BRUTE].max_hp);
    CHECK(battle_max_hp(state.units[0]) == 12 && battle_max_hp(state.units[1]) == 12);
    CHECK(battle_sprite_row(state.units[0]) == 0 && battle_sprite_row(state.units[1]) == 8);
    // Use an exact board position for the movement and damage assertions below.
    state.field = BATTLEFIELD_GROVE;
    state.units[0].cell = cell(2, 1);
    state.units[1].cell = cell(1, 2);
    state.units[2].cell = cell(6, 1);
    state.units[3].cell = cell(5, 2);
    CHECK(battle_can_move_to(state, 0, cell(3, 3)));
    CHECK(!battle_can_move_to(state, 0, cell(3, 1)));
    CHECK(battle_can_attack(state, 0, 2)); // Knight movement with the archer's range-four shot.
    CHECK(battle_rules(state.units[1].id).move_range == 2);
    CHECK(battle_rules(state.units[1].id).effect == BATTLE_EFFECT_KNOCKBACK);
    BattleState child = state;
    battle_apply(child, act(0, 6, 1));
    CHECK(child.units[2].hp == state.units[2].hp - 3 && child.units[0].hp == 12);
    CHECK(!memcmp(&game.party, &saved, sizeof(Party)));
    BattleSearchResult choice = battle_search(state, 2);
    CHECK(choice.depth && !memcmp(&game.party, &saved, sizeof(Party)));
    state.units[0].hp = 11;
    state.units[1].hp = 0;
    battle_finish(state);
    saved.members[0].hp = 11;
    saved.members[1].hp = 0;
    CHECK(!memcmp(&game.party, &saved, sizeof(Party)));
    battle_begin(state, &MIXED_CAVE_FIXTURE);
    CHECK(state.unit_count == 3 && battle_living(state, BATTLE_PARTY) == 1);
    game.party.members[0].hp = 0;
    battle_begin(state, &MIXED_CAVE_FIXTURE);
    CHECK(state.ended && state.result == BATTLE_DEFEAT);

    // Every independent class combination reaches the shared rule and AI tables.
    game.party.count = 1;
    for(uint8_t m = 0; m < MOVEMENT_CLASS_COUNT; ++m)
        for(uint8_t a = 0; a < ATTACK_CLASS_COUNT; ++a) {
            game.party.members[0] = {12, 4, m, a, NPC_ALDEN, 1, 0};
            battle_begin(state, &MIXED_CAVE_FIXTURE);
            auto const& rules = battle_rules(state.units[0].id);
            CHECK(rules.movement_type == MOVEMENT_INFO[m].movement_type);
            CHECK(rules.move_range == MOVEMENT_INFO[m].move_range);
            CHECK(rules.attack_min == ATTACK_INFO[a].attack_min);
            CHECK(rules.attack_max == ATTACK_INFO[a].attack_max);
            CHECK(rules.damage == ATTACK_INFO[a].damage &&
                  rules.injured_damage == ATTACK_INFO[a].injured_damage &&
                  rules.effect == ATTACK_INFO[a].effect &&
                  rules.geometry == ATTACK_INFO[a].geometry);
            uint64_t nodes = 0;
            BattleSearchResult reference = reference_search(state, 1, nodes);
            choice = battle_search(state, 1);
            CHECK(choice.score == reference.score && same_action(choice.action, reference.action));
        }
    // Full-health passes can last beyond 255 activations without wrapping or truncating HP.
    state = board(BATTLE_PARTY, {{0, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 255},
                                  {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 255}});
    for(unsigned i = 0; i < 508; ++i)
        battle_pass(state);
    CHECK(!state.ended && state.activations == 508);
    CHECK(state.units[0].hp == 1 && state.units[1].hp == 1);
    battle_pass(state);
    CHECK(state.ended && state.result == BATTLE_DEFEAT && state.activations == 509);
    CHECK(state.units[0].hp == 0 && state.units[1].hp == 1);
}

// Test primary damage in isolation when the fixture has no supporting units.
static uint8_t damage_in_duel(BattleUnit attacker, BattleUnit victim, bool special = false) {
    BattleState state{};
    state.units[0] = attacker;
    state.units[1] = victim;
    state.unit_count = 2;
    return battle_attack_damage(state, 0, 1, special);
}

// Rend deals 2 to a victim at half its maximum HP or more and 4 to one below half. Every other
// attack deals the same damage whatever its victim's HP.
static void rend_damage() {
    BattleUnit wolf{cell(3, 1), 9, ENEMY_WOLF, BATTLE_PARTY};
    BattleUnit fighter{cell(3, 0), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
    for(uint8_t hp = 1; hp <= 10; ++hp) {
        fighter.hp = hp;
        CHECK(damage_in_duel(wolf, fighter) == (2 * hp >= 10 ? 2 : 4));
        CHECK(damage_in_duel(fighter, wolf) == 3);
    }
    // A party victim's maximum comes from the saved member: the traveler's is 12.
    game = {};
    game.party.count = 1;
    game.party.members[0] = {12, 4, MOVEMENT_STEP, ATTACK_STRIKE, NPC_ALDEN, 1, 0};
    BattleUnit traveler{cell(3, 0), 12, battle_party_id(0), BATTLE_PARTY};
    for(uint8_t hp = 1; hp <= 12; ++hp) {
        traveler.hp = hp;
        CHECK(damage_in_duel(wolf, traveler) == (2 * hp >= 12 ? 2 : 4));
    }
}

// A party member's level adds 1 to its normal damage at level 5 and 1 more at level 9. The bonus
// carries into the starting special's damage, applies to Rend's injured damage too, and never
// reaches enemies.
static void level_attack_damage() {
    game = {};
    game.party.count = 1;
    game.party.members[0] = {12, 4, MOVEMENT_STEP, ATTACK_STRIKE, NPC_ALDEN, 1, 0};
    BattleUnit traveler{cell(3, 0), 12, battle_party_id(0), BATTLE_PARTY};
    BattleUnit fighter{cell(3, 1), ENEMY_INFO[BATTLE_TEST_FIGHTER].max_hp, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
    uint8_t bonus[PARTY_MAX_LEVEL] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2};
    for(uint8_t level = 1; level <= PARTY_MAX_LEVEL; ++level) {
        game.party.members[0].level = level;
        CHECK(damage_in_duel(traveler, fighter) == 3 + bonus[level - 1]);
        // Strike's special adds 3 more to a victim at full HP.
        CHECK(damage_in_duel(traveler, fighter, true) == 6 + bonus[level - 1]);
    }
    // Rend deals 2 to a healthy victim and 4 to an injured one, and the level-9 bonus adds 2 to both.
    game.party.members[0].level = 9;
    game.party.members[0].attack_class = ATTACK_REND;
    CHECK(damage_in_duel(traveler, fighter) == 2 + 2);
    fighter.hp = 1;
    CHECK(damage_in_duel(traveler, fighter) == 4 + 2);
    fighter.hp = ENEMY_INFO[BATTLE_TEST_FIGHTER].max_hp;
    // The bonus is the attacker's: an enemy attacking the same party member gets none.
    CHECK(damage_in_duel(fighter, traveler) == 3);
}

// A victory restores one AP to every saved member, including one that fell, up to its maximum. A
// defeat restores none, and play itself never changes AP.
static void victory_ap_recovery() {
    game = {};
    game.party.count = 3;
    game.party.members[0] = {12, 0, MOVEMENT_STEP, ATTACK_STRIKE, NPC_ALDEN, 1, 0};
    game.party.members[1] = {0, 3, MOVEMENT_LEAP, ATTACK_SHOOT, NPC_BRAM, 1, 0};
    game.party.members[2] = {11, 4, MOVEMENT_CHARGE, ATTACK_REND, NPC_ELIZA, 1, 0};
    Party saved = game.party;
    BattleState state;
    battle_begin(state, &MIXED_CAVE_FIXTURE);
    battle_pass(state);
    CHECK(!memcmp(&saved, &game.party, sizeof(Party)));

    state.ended = true;
    state.result = BATTLE_DEFEAT;
    LevelUpNotice notice = battle_finish(state).levels;
    CHECK(!notice.members && !notice.perk_earned);
    CHECK(game.party.members[0].ap == 0 && game.party.members[1].ap == 3 &&
          game.party.members[2].ap == 4);
    CHECK(game.party.members[0].experience == 0 && game.party.members[2].experience == 0);

    game.party = saved;
    state.result = BATTLE_VICTORY;
    notice = battle_finish(state).levels;
    CHECK(!notice.members && !notice.perk_earned);
    CHECK(game.party.members[0].ap == 1);
    CHECK(game.party.members[1].ap == 4); // Recovered from a member who fell.
    CHECK(game.party.members[2].ap == 4); // Already full, so the recovery is capped.
    // Every member earns experience, including the one who fell, and no one is at a new level yet.
    for(uint8_t i = 0; i < 3; ++i)
        CHECK(game.party.members[i].level == 1 && game.party.members[i].experience == 2);

    // A victory that completes a level processes it at once and keeps only the surplus.
    game.party = saved;
    game.party.members[0].experience = 9;
    game.party.members[1].level = 2;
    game.party.members[1].experience = 13;
    notice = battle_finish(state).levels;
    CHECK(notice.members == 0x03 && notice.perk_earned);
    CHECK(game.party.members[0].level == 2 && game.party.members[0].experience == 1);
    CHECK(game.party.members[1].level == 3 && game.party.members[1].experience == 0);
    CHECK(party_has_unspent_perks(game.party));

    game.party.members[0].level = 2;
    game.party.members[0].experience = 13;
    notice = battle_finish(state).levels;
    CHECK(notice.members == 0x01 && !notice.perk_earned);
    CHECK(party_has_unspent_perks(game.party));
}

static void victory_gold() {
    game = {};
    BattleState state{};
    state.result = BATTLE_VICTORY;
    state.unit_count = 4;
    for(auto& unit : state.units)
        unit.team = BATTLE_ENEMY;

    CHECK(battle_finish(state).gold_gained == 0 && game.gold == 0);
    state.ended = true;
    state.result = BATTLE_DEFEAT; // Includes the result set by resignation.
    CHECK(battle_finish(state).gold_gained == 0 && game.gold == 0);
    state.result = BATTLE_VICTORY;
    for(uint8_t enemies = 0; enemies <= 4; ++enemies) {
        state.unit_count = enemies;
        game.gold = 20;
        CHECK(battle_finish(state).gold_gained == 3 * enemies);
        CHECK(game.gold == 20 + 3 * enemies);
    }
    game.gold = UINT16_MAX - 5;
    CHECK(battle_finish(state).gold_gained == 5 && game.gold == UINT16_MAX);
    CHECK(battle_finish(state).gold_gained == 0 && game.gold == UINT16_MAX);
}

// A saved member's starting special costs AP and needs the normal attack's reach. It deals the
// normal damage plus its bonus for the victim: the full bonus at full HP, and the other one below
// it.
static void special_attacks() {
    game = {};
    game.party.count = 1;
    game.party.members[0] = {20, 4, MOVEMENT_STEP, ATTACK_STRIKE, NPC_ALDEN, 1, 0};
    ENEMY_ID member = (ENEMY_ID)battle_party_id(0);
    BattleState s = board(
        BATTLE_PARTY, {{3, 1, member, BATTLE_PARTY, 20}, {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    for(uint8_t level = 1; level < BATTLE_STARTER_LEVEL; ++level) {
        game.party.members[0].level = level;
        CHECK(battle_legal(s, act(0, 4, 1)));
        CHECK(!battle_special_legal(s, act(0, 4, 1)));
        BattleState locked = s;
        battle_apply_special(locked, act(0, 4, 1));
        CHECK(same_state(locked, s) && game.party.members[0].ap == 4);
    }
    game.party.members[0].level = BATTLE_STARTER_LEVEL;
    CHECK(battle_special_legal(s, act(0, 4, 1)) && battle_legal(s, act(0, 4, 1)));
    CHECK(!battle_special_legal(s, act(0, 3, 2))); // A move is not an attack.
    CHECK(!battle_special_legal(s, act(0, 3, 0))); // Nor is an empty cell in the attack's reach.
    BattleState enemy_turn = s;
    enemy_turn.turn = BATTLE_ENEMY;
    CHECK(!battle_special_legal(enemy_turn, act(1, 3, 1))); // Enemies have no AP to spend.

    // Without the AP, the special is not legal, and applying it changes nothing.
    game.party.members[0].ap = 1;
    BattleState short_of_ap = s;
    CHECK(!battle_special_legal(short_of_ap, act(0, 4, 1)));
    battle_apply_special(short_of_ap, act(0, 4, 1));
    CHECK(same_state(short_of_ap, s) && game.party.members[0].ap == 1);

    // Power Strike: 3 normal plus 3 bonus against a healthy fighter. It spends 2 AP and ends the
    // turn.
    game.party.members[0].ap = 4;
    battle_apply_special(s, act(0, 4, 1));
    CHECK(s.units[1].hp == 4 && game.party.members[0].ap == 2);
    CHECK(s.turn == BATTLE_ENEMY && s.activations == 1);

    // The damage each starting special deals, by the victim's HP: a healthy fighter (10 of 10), one
    // at 6 HP, and one at 4 HP. Rend's normal damage halves at 4 HP, so Wound Hunter reads 3, 5,
    // then 7.
    struct Expected {
        uint8_t full, injured, low;
    };

    static constexpr Expected EXPECTED[ATTACK_CLASS_COUNT] = {
        {6, 6, 6}, // Strike: Power Strike
        {7, 4, 4}, // Shoot: Opening Shot
        {5, 5, 5}, // Reach: Driving Thrust
        {5, 5, 5}, // Impact: Crushing Bash
        {3, 5, 7}, // Rend: Wound Hunter
        {5, 5, 5}, // Arc: Crescent Cut
        {5, 5, 5}, // Drain: Siphon
        {6, 6, 6}, // Recoil: Spring Blow
        {4, 4, 4}, // Flank: Pincer Strike without support
        {4, 4, 4}, // Blast: Detonation
    };
    BattleUnit attacker{cell(3, 1), 20, battle_party_id(0), BATTLE_PARTY};
    BattleUnit victim{cell(4, 1), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
    for(uint8_t a = 0; a < ATTACK_CLASS_COUNT; ++a) {
        game.party.members[0].attack_class = a;
        victim.hp = 10;
        CHECK(damage_in_duel(attacker, victim, true) == EXPECTED[a].full);
        victim.hp = 6;
        CHECK(damage_in_duel(attacker, victim, true) == EXPECTED[a].injured);
        victim.hp = 4;
        CHECK(damage_in_duel(attacker, victim, true) == EXPECTED[a].low);
    }

    // Driving Thrust pushes a surviving victim, as the Reach normal attack never does. Both spend
    // the turn, and only the special costs AP.
    game.party.members[0].attack_class = ATTACK_REACH;
    game.party.members[0].ap = 4;
    BattleState normal = board(
        BATTLE_PARTY, {{2, 1, member, BATTLE_PARTY, 20}, {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(normal, act(0, 3, 1));
    CHECK(normal.units[1].cell == cell(3, 1) && normal.units[1].hp == 7);
    CHECK(game.party.members[0].ap == 4);
    BattleState thrust = board(
        BATTLE_PARTY, {{2, 1, member, BATTLE_PARTY, 20}, {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply_special(thrust, act(0, 3, 1));
    CHECK(thrust.units[1].cell == cell(4, 1) && thrust.units[1].hp == 5);
    CHECK(game.party.members[0].ap == 2);
}

static void signature_attacks() {
    game = {};
    game.party.count = 3;
    auto hero = (ENEMY_ID)battle_party_id(0);
    auto ally = (ENEMY_ID)battle_party_id(1);
    auto other = (ENEMY_ID)battle_party_id(2);
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    game.party.members[1] = party_new_member(NPC_ELIZA, MOVEMENT_STEP, ATTACK_STRIKE);
    game.party.members[2] = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_SHOOT);

    auto use_signature = [&](BattleState& state, unsigned x, unsigned y) {
        member.ap = 4;
        auto action = act(0, x, y);
        CHECK(battle_special_legal(state, action, BATTLE_SIGNATURE));
        battle_apply_special(state, action, BATTLE_SIGNATURE);
        CHECK(member.ap == 1 && state.activations == 1);
        CHECK(state.ended || state.turn == BATTLE_ENEMY);
    };

    BattleState breach = board(BATTLE_PARTY, {{2, 1, hero, BATTLE_PARTY, 16},
                                              {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    member.level = 4;
    CHECK(!battle_special_legal(breach, act(0, 3, 1), BATTLE_SIGNATURE));
    member.level = BATTLE_SIGNATURE_LEVEL;
    member.ap = 2;
    BattleState before = breach;
    CHECK(!battle_special_legal(breach, act(0, 3, 1), BATTLE_SIGNATURE));
    battle_apply_special(breach, act(0, 3, 1), BATTLE_SIGNATURE);
    CHECK(same_state(breach, before) && member.ap == 2);
    use_signature(breach, 3, 1);
    CHECK(breach.units[0].cell == cell(3, 1));
    CHECK(breach.units[1].hp == 4 && breach.units[1].cell == cell(2, 1));
    breach = before;
    breach.units[1].hp = 6;
    use_signature(breach, 3, 1);
    CHECK(breach.ended && breach.result == BATTLE_VICTORY && breach.units[0].cell == cell(3, 1));

    member.level = 8;
    member.perk_ranks = 4 << 3;
    member.attack_class = ATTACK_SHOOT;
    BattleState split = board(BATTLE_PARTY, {{1, 1, hero, BATTLE_PARTY, 16},
                                            {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {5, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {5, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {7, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    use_signature(split, 5, 1);
    CHECK(split.units[1].hp == 4 && split.units[2].hp == 4 && split.units[3].hp == 4);
    CHECK(split.units[4].hp == 5);

    member.attack_class = ATTACK_REACH;
    BattleState hook = board(BATTLE_PARTY, {{1, 1, hero, BATTLE_PARTY, 16},
                                           {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                           {2, 1, ally, BATTLE_PARTY, 9}});
    use_signature(hook, 3, 1);
    CHECK(hook.units[1].hp == 3 && hook.units[1].cell == cell(2, 1));
    CHECK(hook.units[2].hp == 9 && hook.units[2].cell == cell(3, 1));
    hook.turn = BATTLE_PARTY;
    member.ap = 4;
    CHECK(!battle_special_legal(hook, act(0, 2, 1), BATTLE_SIGNATURE));

    member.attack_class = ATTACK_IMPACT;
    BattleState slam = board(BATTLE_PARTY, {{2, 0, hero, BATTLE_PARTY, 16},
                                           {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                           {1, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                           {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(!battle_special_legal(slam, act(0, 2, 1), BATTLE_SIGNATURE));
    slam.field = BATTLEFIELD_MEADOW;
    use_signature(slam, 2, 1);
    CHECK(slam.units[1].hp == 6 && slam.units[2].hp == 6 && slam.units[3].hp == 6);

    member.attack_class = ATTACK_REND;
    BattleState frenzy = board(BATTLE_PARTY, {{2, 1, hero, BATTLE_PARTY, 16},
                                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                             {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                             {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                             {2, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    use_signature(frenzy, 3, 1);
    CHECK(frenzy.units[1].hp == 7 && !frenzy.units[2].hp);
    CHECK(frenzy.units[3].hp == 4 && frenzy.units[4].hp == 5);

    member.attack_class = ATTACK_ARC;
    BattleState whirlwind = board(BATTLE_PARTY, {{3, 1, hero, BATTLE_PARTY, 16},
                                                {2, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {4, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {2, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    uint8_t arc = battle_attack_damage(whirlwind, 0, 1);
    uint8_t diagonal = battle_attack_damage(whirlwind, 0, 2);
    use_signature(whirlwind, 2, 0);
    CHECK(whirlwind.units[1].hp == 10 - arc && whirlwind.units[4].hp == 10 - arc);
    CHECK(whirlwind.units[2].hp == 10 - diagonal && whirlwind.units[3].hp == 10 - arc);

    member.attack_class = ATTACK_DRAIN;
    BattleState channel = board(BATTLE_PARTY, {{2, 1, hero, BATTLE_PARTY, 16},
                                               {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                               {7, 3, ally, BATTLE_PARTY, 4},
                                               {6, 3, other, BATTLE_PARTY, 10},
                                               {4, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 0}});
    BattleState channel_kill = channel;
    channel_kill.units[1].hp = 4;
    use_signature(channel, 3, 1);
    CHECK(channel.units[0].hp == 16 && channel.units[1].hp == 4);
    CHECK(channel.units[2].hp == 9 && channel.units[3].hp == 12 && !channel.units[4].hp);
    use_signature(channel_kill, 3, 1);
    CHECK(channel_kill.ended && channel_kill.units[0].hp == 16 && channel_kill.units[2].hp == 9);

    member.attack_class = ATTACK_RECOIL;
    BattleState sweep = board(BATTLE_PARTY, {{3, 1, hero, BATTLE_PARTY, 16},
                                            {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {1, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {2, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    use_signature(sweep, 4, 1);
    CHECK(sweep.units[0].cell == cell(1, 1) && sweep.units[1].cell == cell(3, 1));
    CHECK(sweep.units[1].hp == 4 && sweep.units[2].hp == 4 && sweep.units[3].hp == 4);

    member.attack_class = ATTACK_FLANK;
    game.party.members[1].level = 5;
    BattleState arms = board(BATTLE_PARTY, {{2, 1, hero, BATTLE_PARTY, 16},
                                           {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 20},
                                           {3, 2, ally, BATTLE_PARTY, 10},
                                           {4, 1, other, BATTLE_PARTY, 10}});
    CHECK(!battle_can_attack(arms, 3, 1));
    use_signature(arms, 3, 1);
    CHECK(arms.units[1].hp == 7 && arms.units[2].hp == 10 && arms.units[3].hp == 10);
    CHECK(game.party.members[1].ap == 4 && game.party.members[2].ap == 4);

    member.attack_class = ATTACK_BLAST;
    BattleState implosion = board(BATTLE_PARTY, {{1, 1, hero, BATTLE_PARTY, 16},
                                                {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 1},
                                                {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {3, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    use_signature(implosion, 3, 1);
    CHECK(implosion.units[1].hp == 7 && !implosion.units[2].hp);
    CHECK(implosion.units[3].hp == 7 && implosion.units[3].cell == cell(5, 1));
    CHECK(implosion.units[4].hp == 7 && implosion.units[4].cell == cell(3, 2));
}

static void maximum_battle_damage_frame() {
    game = {};
    game.party.count = PARTY_CAPACITY;
    for(uint8_t i = 0; i < game.party.count; ++i) {
        auto& member = game.party.members[i];
        member = party_new_member(i ? (NPC_ID)i : NPC_ALDEN, MOVEMENT_STEP, ATTACK_FLANK);
        member.level = PARTY_MAX_LEVEL;
        member.perk_ranks = CONVERGING_BLADES_RANK << 3;
    }
    BattleState state = board(BATTLE_PARTY, {
        {2, 1, (ENEMY_ID)battle_party_id(0), BATTLE_PARTY, 20},
        {3, 0, (ENEMY_ID)battle_party_id(1), BATTLE_PARTY, 20},
        {3, 2, (ENEMY_ID)battle_party_id(2), BATTLE_PARTY, 20},
        {4, 1, (ENEMY_ID)battle_party_id(3), BATTLE_PARTY, 20},
        {3, 1, ENEMY_BRUTE, BATTLE_ENEMY, BATTLE_MAX_HP},
    });
    BattleAction action = act(0, 3, 1);
    CHECK(battle_special_legal(state, action, BATTLE_SIGNATURE));
    battle_apply_special(state, action, BATTLE_SIGNATURE);
    uint8_t damage = BATTLE_MAX_HP - state.units[4].hp;
    CHECK(damage == 44 && damage <= BATTLE_DAMAGE_FRAMES);
    CHECK(battle_damage_frame(9) == 8 && battle_damage_frame(10) == 9);
    CHECK(battle_damage_frame(damage) == damage - 1);
    CHECK(battle_damage_frame(98) == 97 && battle_damage_frame(99) == 98);
    CHECK(battle_damage_frame(100) == 98 && battle_damage_frame(255) == 98);
}

static void attack_perks() {
    game = {};
    game.party.count = 1;
    PartyMember& member = game.party.members[0];
    BattleUnit attacker{cell(1, 1), 20, battle_party_id(0), BATTLE_PARTY};
    BattleUnit victim{cell(2, 1), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};

    struct Case {
        ATTACK_CLASS attack;
        uint8_t range, hp, normal, rank_one, rank_two;
    };

    static constexpr Case CASES[] = {
        {ATTACK_STRIKE, 1, 10, 3, 1, 1}, {ATTACK_STRIKE, 1, 9, 3, 0, 1},
        {ATTACK_SHOOT, 2, 10, 3, 0, 1},  {ATTACK_SHOOT, 3, 10, 3, 0, 1},
        {ATTACK_SHOOT, 4, 10, 3, 1, 1},  {ATTACK_REACH, 1, 10, 3, 0, 2},
        {ATTACK_REACH, 2, 10, 3, 1, 0},  {ATTACK_IMPACT, 1, 10, 2, 0, 1},
        {ATTACK_REND, 1, 10, 2, 0, 0},   {ATTACK_REND, 1, 8, 2, 0, 1},
        {ATTACK_REND, 1, 7, 2, 2, 1},    {ATTACK_REND, 1, 5, 2, 2, 1},
        {ATTACK_REND, 1, 4, 4, 0, 1},
    };
    for(Case const& test : CASES) {
        member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, test.attack);
        member.level = 4;
        victim.cell = cell(1 + test.range, 1);
        victim.hp = test.hp;
        CHECK(damage_in_duel(attacker, victim) == test.normal);
        uint8_t special = damage_in_duel(attacker, victim, true);

        CHECK(party_learn_perk(member, PERK_ATTACK));
        CHECK(damage_in_duel(attacker, victim) == test.normal + test.rank_one);
        CHECK(damage_in_duel(attacker, victim, true) == special + test.rank_one);
        CHECK(party_learn_perk(member, PERK_ATTACK));
        uint8_t bonus = test.rank_one + test.rank_two;
        CHECK(damage_in_duel(attacker, victim) == test.normal + bonus);
        CHECK(damage_in_duel(attacker, victim, true) == special + bonus);
        member.level = 5;
        CHECK(damage_in_duel(attacker, victim) == test.normal + bonus + 1);
        member.level = 9;
        CHECK(damage_in_duel(attacker, victim, true) == special + bonus + 2);
        CHECK(damage_in_duel(victim, attacker) == 3);

        BattleState step = board(BATTLE_PARTY, {{1, 1, (ENEMY_ID)attacker.id, BATTLE_PARTY, 20},
                                                {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
        CHECK(battle_engagement(step, 0, cell(2, 1)).damage == 1);
    }

    // Exercise an exact three-quarter boundary, plus an odd maximum with a fractional threshold.
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_REND);
    member.level = 2;
    CHECK(party_learn_perk(member, PERK_ATTACK));
    for(uint8_t enemy : {BATTLE_TEST_FIGHTER, ENEMY_BRUTE, ENEMY_WOLF}) {
        victim.id = enemy;
        uint8_t maximum = ENEMY_INFO[enemy].max_hp;
        for(uint8_t hp = 1; hp <= maximum; ++hp) {
            victim.hp = hp;
            CHECK(damage_in_duel(attacker, victim) == (4 * hp < 3 * maximum ? 4 : 2));
        }
    }
}

static void perk_victory_recovery() {
    game = {};
    game.party.count = 2;
    for(uint8_t i = 0; i < game.party.count; ++i) {
        PartyMember& member = game.party.members[i];
        member = party_new_member(NPC_ALDEN, MOVEMENT_LEAP, ATTACK_REACH);
        member.level = 2;
        member.hp = 0;
        member.ap = 0;
    }
    CHECK(party_learn_perk(game.party.members[0], PERK_MOVEMENT));
    BattleState victory{};
    victory.ended = true;
    victory.result = BATTLE_VICTORY;
    battle_finish(victory);
    CHECK(game.party.members[0].ap == 1 && game.party.members[1].ap == 1);
    CHECK(game.party.members[0].hp == 0);
    game.party.members[0].equipment[SLOT_RELIC] = equipment_encode(ITEM_MOON_SHARD);
    game.party.members[0].ap = 4;
    game.party.members[1].ap = 4;
    battle_finish(victory);
    CHECK(game.party.members[0].ap == 5 && game.party.members[1].ap == 4);
    CHECK(game.party.members[0].hp == 0);
}

static void perk_winning_action(BattleState const& state, BattleAction action, bool wins) {
    CHECK(battle_legal(state, action));
    CHECK((battle_action_kind(state, action) == BATTLE_ACTION_WIN) == wins);
    CHECK((battle_search_priority(state, action) == BATTLE_PRIORITY_WIN) == wins);
    BattleState child = state;
    battle_apply(child, action);
    CHECK((child.ended && child.result == BATTLE_VICTORY) == wins);
    if(wins) {
        CHECK(same_action(battle_search(state, 1).action, action));
        CHECK(same_action(battle_think(state, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET).action, action));
        CHECK(same_action(battle_greedy_action(state), action));
    }
}

static void charge_perks() {
    game = {};
    game.party.count = 1;
    PartyMember& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_CHARGE, ATTACK_IMPACT);
    member.level = 4;
    ENEMY_ID id = (ENEMY_ID)battle_party_id(0);
    BattleState charge = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 1},
                                              {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 2},
                                              {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 3}});
    BattleAction long_move = act(0, 3, 1);
    for(uint8_t rank = 0; rank <= 2; ++rank) {
        if(rank)
            CHECK(party_learn_perk(member, PERK_MOVEMENT));
        BattleEngagement hit = battle_engagement(charge, 0, long_move.destination);
        CHECK(hit.hits == 2 && hit.kills == (rank == 2 ? 2 : 0));
        CHECK(hit.damage == (rank == 2 ? 5 : 2 + rank));
        perk_winning_action(charge, long_move, rank == 2);
        BattleState result = charge;
        battle_apply(result, long_move);
        CHECK(result.units[0].hp == (rank == 2));
        CHECK(result.units[1].hp == (rank == 2 ? 0 : 1));
        CHECK(result.units[2].hp == (rank == 2 ? 0 : 2 - rank));
        if(rank) {
            BattleState short_move = charge;
            short_move.units[0].cell = cell(2, 1);
            battle_apply(short_move, long_move);
            CHECK(!short_move.units[0].hp && short_move.units[1].hp == 1 &&
                  short_move.units[2].hp == 1);
        }
    }
    expect_same_as_reference(charge, 2);

    // The extra hit is in the direction of travel, including west, north, and south.
    constexpr uint8_t PATHS[][3] = {
        {battle_cell(3, 2), battle_cell(2, 2), battle_cell(1, 2)},
        {battle_cell(3, 2), battle_cell(3, 1), battle_cell(3, 0)},
        {battle_cell(2, 1), battle_cell(2, 2), battle_cell(2, 3)},
    };
    for(auto const& path : PATHS) {
        BattleState s = charge;
        s.unit_count = 2;
        s.units[0].cell = path[0];
        s.units[1] = BattleUnit{path[2], 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
        CHECK(battle_legal(s, {0, path[1]}));
        battle_apply(s, {0, path[1]});
        CHECK(s.units[1].hp == 8);
    }
}

static void step_and_leap_perks() {
    game = {};
    game.party.count = 2;
    auto id = (ENEMY_ID)battle_party_id(0);
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    game.party.members[1] = party_new_member(NPC_ELIZA, MOVEMENT_STEP, ATTACK_STRIKE);
    member.level = 4;
    BattleState guard = board(BATTLE_ENEMY, {{3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                             {2, 1, id, BATTLE_PARTY, 3},
                                             {2, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    CHECK(battle_attack_impact(guard, 0, 1).kills == 1);
    CHECK(party_learn_perk(member, PERK_MOVEMENT));
    CHECK(battle_attack_impact(guard, 0, 1).damage == 2);
    CHECK(battle_attack_impact(guard, 0, 1).kills == 0);
    CHECK(battle_search_priority(guard, act(0, 2, 1)) == BATTLE_PRIORITY_ATTACK);
    BattleState hit = guard;
    battle_apply(hit, act(0, 2, 1));
    CHECK(hit.units[1].hp == 1);
    hit = guard;
    hit.units[2].hp = 0;
    CHECK(battle_attack_damage(hit, 0, 1) == 3);
    hit = guard;
    hit.units[2].cell = cell(1, 2);
    CHECK(battle_attack_damage(hit, 0, 1) == 3);
    hit = guard;
    hit.units[2].team = BATTLE_ENEMY;
    CHECK(battle_attack_damage(hit, 0, 1) == 3);
    hit = guard;
    hit.units[0].id = battle_party_id(1);
    hit.units[1].hp = 10;
    game.party.members[1].level = BATTLE_STARTER_LEVEL;
    battle_apply_special(hit, act(0, 2, 1));
    CHECK(hit.units[1].hp == 5);

    // Reduced direct damage also limits Drain's healing against a surviving target.
    auto& opponent = game.party.members[1];
    opponent = party_new_member(NPC_ELIZA, MOVEMENT_CHARGE, ATTACK_DRAIN);
    opponent.level = 4;
    CHECK(party_learn_perk(opponent, PERK_ATTACK));
    hit = guard;
    hit.units[0].id = battle_party_id(1);
    hit.units[1].hp = 10;
    battle_apply(hit, act(0, 2, 1));
    CHECK(hit.units[0].hp == 10 && hit.units[1].hp == 9);

    // A supported Step member still takes the full damage from rank-2 splash and engagement.
    opponent.attack_class = ATTACK_BLAST;
    opponent.level = 8;
    CHECK(party_learn_perk(opponent, PERK_ATTACK));
    CHECK(party_learn_perk(opponent, PERK_MOVEMENT));
    CHECK(party_learn_perk(opponent, PERK_MOVEMENT));
    auto foe = (ENEMY_ID)battle_party_id(1);
    BattleState splash = board(BATTLE_ENEMY, {{2, 1, foe, BATTLE_ENEMY, 10},
                                              {5, 2, id, BATTLE_PARTY, 10},
                                              {6, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                              {5, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    battle_apply(splash, act(0, 5, 1));
    CHECK(splash.units[1].hp == 8 && splash.units[2].hp == 10);
    BattleState rush = board(BATTLE_ENEMY, {{2, 1, foe, BATTLE_ENEMY, 10},
                                            {5, 1, id, BATTLE_PARTY, 10},
                                            {5, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    battle_apply(rush, act(0, 4, 1));
    CHECK(rush.units[1].hp == 7);

    BattleState step =
        board(BATTLE_PARTY, {{2, 1, id, BATTLE_PARTY, 1}, {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    BattleAction engage = act(0, 3, 1);
    int16_t sacrifice_value = battle_greedy_score(step, engage);
    CHECK(party_learn_perk(member, PERK_MOVEMENT));
    CHECK(battle_greedy_score(step, engage) > sacrifice_value);
    BattleState quiet = step;
    battle_apply(quiet, act(0, 1, 1));
    CHECK(quiet.ended && !quiet.units[0].hp);
    BattleState pass = step;
    battle_pass(pass);
    CHECK(pass.ended && !pass.units[0].hp);
    battle_apply(step, engage);
    CHECK(!step.ended && step.units[0].hp == 1 && step.units[1].hp == 9);

    member = party_new_member(NPC_ALDEN, MOVEMENT_LEAP, ATTACK_STRIKE);
    member.level = 4;
    BattleState landing = board(BATTLE_PARTY, {{1, 0, id, BATTLE_PARTY, 10},
                                               {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                               {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                               {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                               {3, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    for(uint8_t rank = 0; rank <= 2; ++rank) {
        if(rank)
            CHECK(party_learn_perk(member, PERK_MOVEMENT));
        for(uint8_t enemies = 0; enemies <= 3; ++enemies) {
            BattleState s = landing;
            for(uint8_t i = enemies + 1; i <= 3; ++i)
                s.units[i].hp = 0;
            // A distant opponent keeps the battle running even when the landing is quiet.
            s.units[s.unit_count++] = BattleUnit{cell(7, 3), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
            uint8_t damage = enemies && enemies <= rank ? 2 : 1;
            CHECK(battle_engagement(s, 0, cell(3, 1)).damage == enemies * damage);
            battle_apply(s, act(0, 3, 1));
            CHECK(s.units[0].hp == 9 && s.units[4].hp == 10);
            for(uint8_t i = 1; i <= enemies; ++i)
                CHECK(s.units[i].hp == 10 - damage);
        }
    }
}

static void reach_and_impact_perks() {
    game = {};
    game.party.count = 1;
    auto id = (ENEMY_ID)battle_party_id(0);
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_REACH);
    member.level = 4;
    for(uint8_t rank = 0; rank <= 2; ++rank) {
        if(rank)
            CHECK(party_learn_perk(member, PERK_ATTACK));
        for(uint8_t distance : {1, 2}) {
            uint8_t x = (uint8_t)(1 + distance);
            BattleState s = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 20},
                                                 {x, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
            BattleState special = s;
            BattleAction reach = act(0, x, 1);
            battle_apply(s, reach);
            uint8_t damage =
                (uint8_t)(3 + (rank && distance == 2) + 2 * (rank == 2 && distance == 1));
            CHECK(s.units[1].hp == 10 - damage && s.units[1].cell == cell(x, 1));
            member.ap = 4;
            battle_apply_special(special, reach);
            CHECK(special.units[1].hp == 8 - damage && special.units[1].cell == cell(x + 1, 1));
        }
    }
    BattleState blocked = board(
        BATTLE_PARTY, {{1, 0, id, BATTLE_PARTY, 20}, {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(blocked, act(0, 3, 0));
    CHECK(blocked.units[1].hp == 6 && blocked.units[1].cell == cell(3, 0));

    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_IMPACT);
    member.level = 4;
    BattleState strike = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 20},
                                              {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 3},
                                              {3, 1, BATTLE_TEST_ARCHER, BATTLE_PARTY, 10}});
    BattleAction attack = act(0, 2, 1);
    perk_winning_action(strike, attack, false);
    CHECK(party_learn_perk(member, PERK_ATTACK));
    perk_winning_action(strike, attack, true);
    expect_same_as_reference(strike, 2);

    for(uint8_t rank = 1; rank <= 2; ++rank) {
        if(rank == 2)
            CHECK(party_learn_perk(member, PERK_ATTACK));
        for(bool special : {false, true}) {
            for(uint8_t obstruction = 0; obstruction < 5; ++obstruction) {
                BattleState s = strike;
                s.units[1].hp = 10;
                if(obstruction == 1)
                    s.units[2].hp = 0;
                if(obstruction == 2)
                    s.units[2].team = BATTLE_ENEMY;
                if(obstruction == 3 || obstruction == 4) {
                    s.unit_count = 2;
                    s.units[0].cell = obstruction == 3 ? cell(2, 0) : cell(6, 1);
                    s.units[1].cell = obstruction == 3 ? cell(3, 0) : cell(7, 1);
                }
                uint8_t destination = s.units[1].cell;
                uint8_t damage =
                    2 + (rank == 2) + (obstruction == 0 || obstruction == 2) + (special ? 3 : 0);
                CHECK(battle_attack_damage(s, 0, 1, special) == damage);
                member.ap = 4;
                if(special)
                    battle_apply_special(s, {0, destination});
                else
                    battle_apply(s, {0, destination});
                CHECK(s.units[1].hp == 10 - damage);
                CHECK(s.units[1].cell == destination + (obstruction == 1));
            }
        }
    }
}

static void new_combat_geometry() {
    game = {};
    game.party.count = 1;
    PartyMember& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_VAULT, ATTACK_ARC);
    member.level = BATTLE_STARTER_LEVEL;
    for(uint8_t field = 0; field < BATTLEFIELD_COUNT; ++field) {
        BattleState s{};
        s.field = field;
        s.turn = BATTLE_PARTY;
        s.units[0] = BattleUnit{0, 20, battle_party_id(0), BATTLE_PARTY};
        for(uint8_t from = 0; from < BATTLE_CELLS; ++from) {
            if(battle_blocked(field, from))
                continue;
            s.units[0].cell = from;
            for(MOVEMENT_CLASS movement : {MOVEMENT_VAULT, MOVEMENT_GLIDE}) {
                member.movement_class = movement;
                s.unit_count = 1;
                check_enumeration(s);
                for(uint8_t to = 0; to < BATTLE_CELLS; ++to) {
                    bool reaches = oracle_legal(s, 0, to);
                    CHECK(battle_can_move_to(s, 0, to) == reaches);
                    if(!reaches)
                        continue;
                    s.unit_count = 2;
                    s.units[1] = BattleUnit{to, 10, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
                    CHECK(!battle_can_move_to(s, 0, to));
                    CHECK(!battle_legal(s, {0, to}));
                    uint8_t middle = (uint8_t)((from + to) / 2);
                    if((movement == MOVEMENT_VAULT ||
                        abs((int)battle_column(from) - battle_column(to)) == 2) &&
                       !battle_blocked(field, middle)) {
                        s.units[1].cell = middle;
                        CHECK(battle_can_move_to(s, 0, to) == (movement == MOVEMENT_VAULT));
                        check_enumeration(s);
                    }
                    s.unit_count = 1;
                }
            }
            for(uint8_t attack = ATTACK_ARC; attack < ATTACK_CLASS_COUNT; ++attack) {
                member.attack_class = attack;
                s.unit_count = 2;
                for(uint8_t to = 0; to < BATTLE_CELLS; ++to) {
                    if(to == from || battle_blocked(field, to))
                        continue;
                    s.units[1] = BattleUnit{to, 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
                    bool legal = oracle_legal(s, 0, to);
                    CHECK(battle_can_attack(s, 0, 1) == legal);
                    CHECK(battle_legal(s, {0, to}) == legal);
                    CHECK(battle_special_legal(s, {0, to}) == legal);
                    uint8_t packed[BATTLE_UNIT_ACTION_CAPACITY], attacks;
                    battle_unit_actions(s, 0, packed, attacks);
                    CHECK(attacks == legal);
                    if(legal)
                        CHECK(packed[0] == to);

                    if(attack != ATTACK_BLAST || !legal)
                        continue;
                    uint8_t step = battle_row(from) == battle_row(to) ? 1 : BATTLE_COLUMNS;
                    uint8_t between = (uint8_t)(from + (to > from ? step : -step));
                    s.units[2] = BattleUnit{between, 10, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
                    s.unit_count = 3;
                    CHECK(!battle_can_attack(s, 0, 1));
                    CHECK(!battle_special_legal(s, {0, to}));
                    s.units[2].hp = 0;
                    CHECK(battle_can_attack(s, 0, 1));
                    s.unit_count = 2;
                }
            }
        }
    }

    // The actual diagonal path matters even between two occupied corner cells.
    member.movement_class = MOVEMENT_GLIDE;
    auto id = (ENEMY_ID)battle_party_id(0);
    BattleState corners = board(BATTLE_PARTY, {{2, 0, id, BATTLE_PARTY, 20},
                                               {3, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                               {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_can_move_to(corners, 0, cell(3, 1)));
    CHECK(battle_can_move_to(corners, 0, cell(4, 2)));
}

static void new_movement_effects() {
    game = {};
    game.party.count = 1;
    PartyMember& member = game.party.members[0];
    auto id = (ENEMY_ID)battle_party_id(0);
    member = party_new_member(NPC_ALDEN, MOVEMENT_VAULT, ATTACK_ARC);
    member.level = 4;
    BattleState vault = board(BATTLE_PARTY, {{1, 2, id, BATTLE_PARTY, 1},
                                             {2, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 3},
                                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 2},
                                             {7, 3, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    BattleAction landing = act(0, 3, 2);
    for(uint8_t rank = 0; rank <= 2; ++rank) {
        if(rank)
            CHECK(party_learn_perk(member, PERK_MOVEMENT));
        BattleState s = vault;
        CHECK(battle_legal(s, landing));
        BattleEngagement hit = battle_engagement(s, 0, landing.destination);
        CHECK(hit.hits == 2 && hit.damage == 2 + rank + (rank == 2));
        CHECK(hit.kills == (rank == 2 ? 2 : 0));
        CHECK(battle_search_priority(s, landing) ==
              (rank == 2 ? BATTLE_PRIORITY_KILL : BATTLE_PRIORITY_ATTACK));
        battle_apply(s, landing);
        CHECK(s.units[0].cell == landing.destination && !s.units[0].hp && s.activations == 1);
        CHECK(s.units[1].hp == 2 - rank && s.units[2].hp == (rank == 2 ? 0 : 1));
        CHECK(s.units[3].hp == 10 && s.ended && s.result == BATTLE_DEFEAT);
    }

    // A rank-2 Vault can kill a 3 HP enemy only when it jumps over that enemy.
    vault.units[2].hp = 0;
    vault.units[3].hp = 0;
    perk_winning_action(vault, landing, true);
    CHECK(same_action(battle_search(vault, 2).action, landing));
    battle_apply(vault, landing);
    CHECK(vault.ended && vault.result == BATTLE_VICTORY && vault.units[0].hp == 1);

    BattleState over_ally = board(BATTLE_PARTY, {{1, 2, id, BATTLE_PARTY, 5},
                                                 {2, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                                 {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                 {0, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(over_ally, landing);
    CHECK(over_ally.units[0].hp == 4 && over_ally.units[1].hp == 10);
    CHECK(over_ally.units[2].hp == 8 && over_ally.units[3].hp == 10);
    BattleState over_terrain =
        board(BATTLE_PARTY, {{3, 0, id, BATTLE_PARTY, 5}, {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_legal(over_terrain, act(0, 5, 0)));
    battle_apply(over_terrain, act(0, 5, 0));
    CHECK(over_terrain.units[0].hp == 4 && over_terrain.units[1].hp == 8);

    member = party_new_member(NPC_ALDEN, MOVEMENT_GLIDE, ATTACK_STRIKE);
    member.level = 4;
    BattleState glide = board(BATTLE_PARTY, {{1, 0, id, BATTLE_PARTY, 1},
                                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 2},
                                             {4, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 2},
                                             {2, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 4}});
    BattleAction short_glide = act(0, 2, 1);
    for(uint8_t rank = 0; rank <= 2; ++rank) {
        if(rank)
            CHECK(party_learn_perk(member, PERK_MOVEMENT));
        CHECK(battle_engagement(glide, 0, short_glide.destination).damage == (rank ? 2 : 1));
        BattleState s = glide;
        battle_apply(s, short_glide);
        CHECK(s.units[1].hp == (rank ? 0 : 1) && s.units[2].hp == 2 && s.units[3].hp == 4);

        s = glide;
        s.units[2].cell = cell(2, 0);
        CHECK(battle_engagement(s, 0, short_glide.destination).damage == 2);
        s.units[2].hp = 0;
        CHECK(battle_engagement(s, 0, short_glide.destination).damage == (rank ? 2 : 1));

        s = glide;
        CHECK(battle_engagement(s, 0, landing.destination).damage == (rank == 2 ? 4 : 2));
        perk_winning_action(s, landing, rank == 2);
        battle_apply(s, landing);
        CHECK(s.units[0].cell == landing.destination && s.units[0].hp == (rank == 2));
        CHECK(s.units[1].hp == (rank == 2 ? 0 : 1) && s.units[2].hp == s.units[1].hp);
    }
}

static void new_direct_attacks() {
    game = {};
    game.party.count = 1;
    auto id = (ENEMY_ID)battle_party_id(0);
    PartyMember& member = game.party.members[0];

    struct AttackCase {
        ATTACK_CLASS attack;
        uint8_t x, y, base, bonus, rank_one, rank_two;
    };

    static constexpr AttackCase CASES[] = {
        {ATTACK_ARC, 4, 2, 3, 2, 1, 1},    {ATTACK_DRAIN, 4, 1, 2, 3, 0, 1},
        {ATTACK_RECOIL, 4, 1, 3, 3, 1, 1}, {ATTACK_FLANK, 4, 1, 2, 2, 0, 1},
        {ATTACK_BLAST, 5, 1, 2, 2, 0, 0},
    };
    for(auto const& test : CASES) {
        for(uint8_t rank = 0; rank <= 2; ++rank) {
            member = party_new_member(NPC_ALDEN, MOVEMENT_VAULT, test.attack);
            member.level = BATTLE_STARTER_LEVEL;
            if(rank) {
                member.level = 9;
                for(uint8_t learned = 0; learned < rank; ++learned)
                    CHECK(party_learn_perk(member, PERK_ATTACK));
            }
            for(bool special : {false, true}) {
                member.ap = 4;
                BattleState s =
                    board(BATTLE_PARTY, {{3, 1, id, BATTLE_PARTY, 8},
                                         {test.x, test.y, ENEMY_BRUTE, BATTLE_ENEMY, 12}});
                BattleAction action = act(0, test.x, test.y);
                CHECK(battle_legal(s, action) && battle_special_legal(s, action));
                if(special)
                    battle_apply_special(s, action);
                else
                    battle_apply(s, action);
                CHECK(s.units[1].hp == 12 - test.base - (special ? test.bonus : 0) -
                                           (rank ? 2 + test.rank_one : 0) -
                                           (rank == 2 ? test.rank_two : 0));
                CHECK(s.units[1].cell == action.destination);
                CHECK(s.units[0].cell == (test.attack == ATTACK_RECOIL ? cell(2, 1) : cell(3, 1)));
                CHECK(s.units[0].hp == 8 + (test.attack == ATTACK_DRAIN ? 1 + (rank != 0) : 0));
                CHECK(s.activations == 1 && s.turn == BATTLE_ENEMY && !s.ended);
                CHECK(member.ap == (special ? 2 : 4));
            }
        }
    }
}

static void drain_and_recoil() {
    game = {};
    game.party.count = 1;
    auto id = (ENEMY_ID)battle_party_id(0);
    PartyMember& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_GLIDE, ATTACK_DRAIN);
    auto drain = [&](uint8_t hp, uint8_t victim_hp) {
        return board(BATTLE_ENEMY, {{2, 1, id, BATTLE_ENEMY, hp},
                                    {3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, victim_hp}});
    };
    BattleState s = drain(12, 10);
    battle_apply(s, act(0, 3, 1));
    CHECK(s.units[0].hp == 12 && s.units[1].hp == 8);
    s = drain(9, 1);
    battle_apply(s, act(0, 3, 1));
    CHECK(s.ended && s.result == BATTLE_DEFEAT && s.units[0].hp == 10);
    member.level = 2;
    CHECK(party_learn_perk(member, PERK_ATTACK));
    s = drain(9, 1);
    battle_apply(s, act(0, 3, 1));
    CHECK(s.ended && s.units[0].hp == 11);
    s = drain(9, 10);
    battle_apply(s, act(0, 3, 1));
    CHECK(s.units[0].hp == 10 && s.units[1].hp == 8);
    member.level = BATTLE_STARTER_LEVEL;
    s = drain(9, 2);
    battle_apply_special(s, act(0, 3, 1));
    CHECK(s.ended && s.units[0].hp == 11 && member.ap == 2);
    s = drain(party_max_hp(member) - 1, 3);
    battle_apply_special(s, act(0, 3, 1));
    CHECK(s.ended && s.units[0].hp == party_max_hp(member) && member.ap == 0);

    member.level = 4;
    CHECK(party_learn_perk(member, PERK_ATTACK));
    s = drain(9, 10);
    battle_apply(s, act(0, 3, 1));
    CHECK(s.units[0].hp == 11 && s.units[1].hp == 7);

    member = party_new_member(NPC_ALDEN, MOVEMENT_LEAP, ATTACK_RECOIL);
    static constexpr int offsets[][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for(auto const& offset : offsets) {
        unsigned tx = 3 + offset[0], ty = 1 + offset[1];
        unsigned rx = 3 - offset[0], ry = 1 - offset[1];
        s = board(BATTLE_PARTY,
                  {{3, 1, id, BATTLE_PARTY, 1}, {tx, ty, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 3}});
        CHECK(!battle_can_move_to(s, 0, cell(rx, ry)));
        battle_apply(s, act(0, tx, ty));
        CHECK(s.ended && s.result == BATTLE_VICTORY && s.units[0].hp == 1);
        CHECK(s.units[0].cell == cell(rx, ry) && s.units[1].cell == cell(tx, ty));
    }
    for(uint8_t block = 0; block < 3; ++block) {
        static constexpr uint8_t origins[][2] = {{0, 1}, {3, 0}, {2, 1}};
        unsigned x = origins[block][0], y = origins[block][1];
        unsigned target_x = block == 1 ? x - 1 : x + 1; // The pine is east of (3,0).
        s = board(BATTLE_PARTY,
                  {{x, y, id, BATTLE_PARTY, 1}, {target_x, y, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
        if(block == 2) {
            s.units[2] = BattleUnit{cell(1, 1), 10, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
            s.unit_count = 3;
        }
        uint8_t original = s.units[0].cell;
        for(uint8_t rank = 0; rank <= 2; ++rank) {
            member.level = 4;
            member.perk_ranks = (uint8_t)(rank << 3);
            BattleState blocked = s;
            battle_apply(blocked, {0, s.units[1].cell});
            CHECK(blocked.units[0].cell == original && blocked.units[0].hp == 1);
            CHECK(blocked.units[1].hp == (rank == 2 ? 6 : 7));
        }
        member.perk_ranks = 0;
    }
    s = board(BATTLE_PARTY, {{2, 1, id, BATTLE_PARTY, 1},
                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                             {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply_special(s, act(0, 3, 1));
    CHECK(s.units[0].cell == cell(1, 1) && s.units[0].hp == 1);
    CHECK(s.units[1].hp == 4 && s.units[2].hp == 10);
    CHECK(s.activations == 1 && s.turn == BATTLE_ENEMY && member.ap == 2);

    CHECK(party_learn_perk(member, PERK_ATTACK));
    s = board(BATTLE_PARTY, {{2, 1, id, BATTLE_PARTY, 1}, {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4}});
    perk_winning_action(s, act(0, 3, 1), true);
    battle_apply(s, act(0, 3, 1));
    CHECK(s.units[0].cell == cell(1, 1) && s.units[0].hp == 1);
}

static void arc_corner_perk() {
    game = {};
    game.party.count = 1;
    auto id = (ENEMY_ID)battle_party_id(0);
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_ARC);
    member.level = 4;
    BattleState terrain = board(
        BATTLE_PARTY, {{3, 0, id, BATTLE_PARTY, 20}, {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    BattleState occupied = board(BATTLE_PARTY, {{2, 1, id, BATTLE_PARTY, 20},
                                                {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {2, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    for(uint8_t rank = 1; rank <= 2; ++rank) {
        CHECK(party_learn_perk(member, PERK_ATTACK));
        CHECK(battle_can_attack(terrain, 0, 1) && battle_can_attack(occupied, 0, 1));
        CHECK(battle_attack_damage(terrain, 0, 1) == 2 + rank);
        CHECK(battle_attack_damage(occupied, 0, 1, true) == 4 + rank);
        occupied.units[2].hp = 0;
        CHECK(battle_attack_damage(occupied, 0, 1, true) == 5 + rank);
        occupied.units[2].hp = 10;
    }
}

static void flank_formation() {
    game = {};
    game.party.count = 1;
    auto id = (ENEMY_ID)battle_party_id(0);
    PartyMember& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_FLANK);
    member.level = 9;
    CHECK(party_learn_perk(member, PERK_ATTACK));
    BattleState original = board(BATTLE_PARTY, {{2, 1, id, BATTLE_PARTY, 20},
                                                {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {3, 0, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                                {4, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    for(unsigned formation = 0; formation < 6; ++formation) {
        BattleState s = original;
        if(formation == 0)
            s.unit_count = 2;
        if(formation == 1)
            s.unit_count = 3;
        if(formation >= 3) {
            s.unit_count = 3;
            if(formation == 3)
                s.units[2].cell = cell(2, 0);
            else if(formation == 4)
                s.units[2].hp = 0;
            else
                s.units[2].team = BATTLE_ENEMY;
        }
        bool supported = formation == 1 || formation == 2;
        CHECK(battle_attack_damage(s, 0, 1) == (supported ? 7 : 4));
        CHECK(battle_attack_damage(s, 0, 1, true) == (supported ? 9 : 6));
        CHECK(party_learn_perk(member, PERK_ATTACK));
        CHECK(battle_attack_damage(s, 0, 1) == (supported ? 8 : 5));
        CHECK(battle_attack_damage(s, 0, 1, true) == (supported ? 10 : 7));
        member.perk_ranks = 1 << 3;
        perk_winning_action(s, act(0, 3, 1), false);
        BattleState normal = s;
        battle_apply(normal, act(0, 3, 1));
        CHECK(normal.units[1].hp == (supported ? 3 : 6));
        member.ap = 4;
        battle_apply_special(s, act(0, 3, 1));
        CHECK(s.units[1].hp == (supported ? 1 : 4) && member.ap == 2);
    }
    member.perk_ranks = 0;
    member.level = 1;
    CHECK(battle_attack_damage(original, 0, 1) == 4);
    original.units[2].hp = 0;
    original.units[3].hp = 0;
    CHECK(battle_attack_damage(original, 0, 1) == 2);
}

static void blast_splash_and_search() {
    game = {};
    game.party.count = 1;
    auto id = (ENEMY_ID)battle_party_id(0);
    PartyMember& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_BLAST);
    member.level = BATTLE_STARTER_LEVEL;
    BattleState original = board(BATTLE_PARTY, {{2, 1, id, BATTLE_PARTY, 20},
                                                {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {5, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {5, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {6, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    BattleAction blast = act(0, 5, 1);
    BattleState s = original;
    battle_apply(s, blast);
    CHECK(s.units[0].hp == 20 && s.units[1].hp == 8);
    CHECK(s.units[2].hp == 9 && s.units[3].hp == 9 && s.units[4].hp == 9);

    s = original;
    s.units[1].hp = 2;
    s.units[2].team = BATTLE_PARTY; // Adjacent friendly.
    s.units[4].cell = cell(6, 2);   // Adjacent to a splash victim only.
    battle_apply_special(s, blast);
    CHECK(!s.units[1].hp && s.units[2].hp == 10 && s.units[3].hp == 9 && s.units[4].hp == 10);
    CHECK(member.ap == 2 && s.turn == BATTLE_ENEMY && s.activations == 1);

    member.level = 9;
    CHECK(party_learn_perk(member, PERK_ATTACK));
    for(uint8_t rank = 1; rank <= 2; ++rank) {
        if(rank == 2)
            CHECK(party_learn_perk(member, PERK_ATTACK));
        for(bool special : {false, true}) {
            s = original;
            member.ap = 4;
            if(special)
                battle_apply_special(s, blast);
            else
                battle_apply(s, blast);
            CHECK(s.units[1].hp == (special ? 3 : 5));
            CHECK(s.units[2].hp == 10 - rank && s.units[3].hp == 10 - rank &&
                  s.units[4].hp == 10 - rank);
            CHECK(member.ap == (special ? 2 : 4));
        }
        s = original;
        s.units[2].team = BATTLE_PARTY;
        s.units[3].hp = 0;
        CHECK(battle_attack_damage(s, 0, 1) == 4);
        s.units[2].team = BATTLE_ENEMY;
        CHECK(battle_attack_damage(s, 0, 1) == 5);
    }

    s = original;
    s.units[1].hp = 6; // The three splash victims fall but the primary survives.
    for(uint8_t i = 2; i < s.unit_count; ++i)
        s.units[i].hp = 2;
    CHECK(battle_attack_impact(s, 0, 1).kills == 3);
    CHECK(battle_search_priority(s, blast) == BATTLE_PRIORITY_KILL);
    BattleState partial = s;
    battle_apply(partial, blast);
    CHECK(!partial.ended && partial.units[1].hp == 1 && battle_living(partial, BATTLE_ENEMY) == 1);
    s.units[1].hp = 5;
    perk_winning_action(s, blast, true);
    CHECK(same_action(battle_search(s, 2).action, blast));
    CHECK(same_action(battle_greedy_action(s), blast));
    expect_same_as_reference(s, 2);
    battle_apply(s, blast);
    CHECK(s.ended && s.result == BATTLE_VICTORY && battle_living(s, BATTLE_ENEMY) == 0);
    CHECK(s.units[0].hp == 20 && s.activations == 1);
}

static void third_rank_movement() {
    game = {};
    game.party.count = 1;
    auto id = (ENEMY_ID)battle_party_id(0);
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    member.level = 6;
    for(uint8_t rank = 0; rank < 3; ++rank)
        CHECK(party_learn_perk(member, PERK_MOVEMENT));

    BattleState guard = board(BATTLE_ENEMY, {{3, 1, id, BATTLE_PARTY, 10},
                                             {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                             {3, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    CHECK(battle_attack_damage(guard, 1, 0) == 1);
    battle_apply(guard, act(1, 3, 1));
    CHECK(guard.units[0].hp == 9);
    guard.units[2].hp = 0;
    CHECK(battle_attack_damage(guard, 1, 0) == 3);

    member.movement_class = MOVEMENT_LEAP;
    BattleState leap = board(BATTLE_PARTY, {{1, 0, id, BATTLE_PARTY, 10},
                                            {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                            {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                            {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                            {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    BattleAction land = act(0, 3, 1);
    CHECK(battle_legal(leap, land));
    for(uint8_t neighbors = 1; neighbors <= 4; ++neighbors) {
        BattleState s = leap;
        s.unit_count = neighbors + 1;
        auto engagement = battle_engagement(s, 0, land.destination);
        uint8_t damage = neighbors <= 2 ? 3 : 2;
        CHECK(engagement.hits == neighbors && engagement.damage == damage * neighbors);
        battle_apply(s, land);
        CHECK(s.units[0].hp == 9);
        for(uint8_t i = 1; i < s.unit_count; ++i)
            CHECK(s.units[i].hp == 5 - damage);
    }

    member.movement_class = MOVEMENT_CHARGE;
    for(uint8_t distance : {1, 2}) {
        unsigned x = 1 + distance;
        BattleAction charge = act(0, x, 1);
        BattleState start = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 10},
                                                 {x + 1, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                                 {x, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                                 {x + 2, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 0}});
        CHECK(battle_legal(start, charge));
        for(uint8_t blocked : {0, 1}) {
            BattleState s = start;
            s.units[3].hp = blocked;
            battle_apply(s, charge);
            CHECK(s.units[0].hp == 9 && s.units[1].hp == 4 - distance);
            CHECK(s.units[1].cell == cell(x + 2 - blocked, 1));
            CHECK(s.units[2].hp == 5 - distance && s.units[3].hp == blocked);
        }
        start.units[1].hp = 1;
        battle_apply(start, charge);
        CHECK(start.units[1].hp == 0 && start.units[1].cell == cell(x + 1, 1));
    }
    BattleState terrain =
        board(BATTLE_PARTY, {{1, 0, id, BATTLE_PARTY, 10}, {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    battle_apply(terrain, act(0, 2, 0));
    CHECK(terrain.units[1].hp == 3 && terrain.units[1].cell == cell(3, 0));

    member.movement_class = MOVEMENT_VAULT;
    BattleState vault = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 10},
                                             {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {2, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {4, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5}});
    auto e = battle_engagement(vault, 0, cell(3, 1));
    CHECK(e.hits == 3 && e.damage == 6);
    battle_apply(vault, land);
    CHECK(vault.units[0].hp == 9 && vault.units[1].hp == 2 && vault.units[2].hp == 4 &&
          vault.units[3].hp == 3 && vault.units[4].hp == 5);
    BattleState diagonal_kill =
        board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 1}, {2, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 1}});
    CHECK(battle_action_kind(diagonal_kill, land) == BATTLE_ACTION_WIN);
    battle_apply(diagonal_kill, land);
    CHECK(diagonal_kill.ended && diagonal_kill.result == BATTLE_VICTORY &&
          diagonal_kill.units[0].hp == 1);

    member.movement_class = MOVEMENT_GLIDE;
    BattleState glide = board(BATTLE_PARTY, {{0, 0, id, BATTLE_PARTY, 10},
                                             {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {1, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                             {2, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 5}});
    for(uint8_t distance : {1, 2}) {
        BattleState s = glide;
        BattleAction slip = act(0, distance, distance);
        CHECK(battle_legal(s, slip));
        auto damage = battle_engagement(s, 0, slip.destination);
        CHECK(damage.hits == (distance == 1 ? 2 : 3) && damage.damage == (distance == 1 ? 2 : 5));
        battle_apply(s, slip);
        CHECK(s.units[0].hp == 9 && s.units[1].hp == 4 && s.units[4].hp == 5);
        CHECK(s.units[2].hp == (distance == 1 ? 4 : 3) && s.units[3].hp == (distance == 1 ? 5 : 3));
    }
    glide.units[0].hp = 1;
    glide.units[1].hp = 1;
    glide.units[2].hp = glide.units[3].hp = 0;
    BattleAction passing_cut = act(0, 2, 2);
    CHECK(battle_action_kind(glide, passing_cut) == BATTLE_ACTION_WIN);
    uint8_t actions[BATTLE_ACTION_CAPACITY], attacks;
    uint8_t count = battle_actions(glide, actions, attacks);
    battle_order_actions(glide, actions, count, attacks, true);
    CHECK(battle_action_kind(glide, battle_unpack(actions[0])) == BATTLE_ACTION_WIN);
    battle_apply(glide, passing_cut);
    CHECK(glide.ended && glide.result == BATTLE_VICTORY && glide.units[0].hp == 1);

    BattleState cutback = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 10},
                                               {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                               {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    battle_apply(cutback, act(0, 2, 2));
    CHECK(cutback.units[1].hp == 4 && cutback.units[2].hp == 3);
}

static void third_rank_attacks() {
    game = {};
    game.party.count = 2;
    auto& defender = game.party.members[1];
    defender = party_new_member(NPC_ELIZA, MOVEMENT_STEP, ATTACK_STRIKE);
    defender.level = 6;
    for(uint8_t rank = 0; rank < 3; ++rank)
        CHECK(party_learn_perk(defender, PERK_MOVEMENT));
    auto id = (ENEMY_ID)battle_party_id(0);
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    member.level = 6;
    for(uint8_t rank = 0; rank < 3; ++rank)
        CHECK(party_learn_perk(member, PERK_ATTACK));

    for(bool special : {false, true}) {
        auto attack = [&](BattleState& s, uint8_t destination) {
            BattleAction a{0, destination};
            CHECK(battle_legal(s, a));
            member.ap = 4;
            if(special) {
                CHECK(battle_special_legal(s, a));
                battle_apply_special(s, a);
            } else {
                battle_apply(s, a);
            }
            CHECK(member.ap == (special ? 2 : 4));
        };

        member.attack_class = ATTACK_STRIKE;
        BattleState duel = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 11},
                                                {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {2, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
        CHECK(battle_attack_damage(duel, 0, 1, special) == (special ? 9 : 6));
        place(duel, 2, 3, 0);
        CHECK(battle_attack_damage(duel, 0, 1, special) == (special ? 11 : 8));

        member.attack_class = ATTACK_SHOOT;
        BattleState shot = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 11},
                                                {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                                {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 2}});
        perk_winning_action(shot, act(0, 3, 1), true);
        auto impact = battle_attack_impact(shot, 0, 1);
        CHECK(impact.kills == 2 && impact.damage == 6);
        attack(shot, cell(3, 1));
        CHECK(!shot.units[1].hp && !shot.units[2].hp && shot.ended);

        member.attack_class = ATTACK_REACH;
        BattleState reach = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 11},
                                                 {3, 1, BATTLE_TEST_ARCHER, BATTLE_ENEMY, 10},
                                                 {2, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
        CHECK(!battle_can_attack(reach, 1, 0));
        CHECK(battle_can_attack(reach, 0, 1) && has_action(reach, act(0, 3, 1)));
        check_enumeration(reach);
        expect_same_as_reference(reach, 2);
        BattleState enemy_screen = reach;
        enemy_screen.units[2].team = BATTLE_ENEMY;
        CHECK(!battle_can_attack(enemy_screen, 0, 1) && !has_action(enemy_screen, act(0, 3, 1)));
        BattleState rock = board(
            BATTLE_PARTY, {{2, 3, id, BATTLE_PARTY, 11}, {0, 3, BATTLE_TEST_ARCHER, BATTLE_ENEMY, 10}});
        CHECK(!battle_can_attack(rock, 0, 1));
        attack(reach, cell(3, 1));
        CHECK(reach.units[1].hp == (special ? 3 : 5) &&
              reach.units[1].cell == cell(special ? 4 : 3, 1));

        member.attack_class = ATTACK_IMPACT;
        BattleState collision = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 11},
                                                     {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                                     {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 3}});
        perk_winning_action(collision, act(0, 2, 1), true);
        attack(collision, cell(2, 1));
        CHECK(!collision.units[1].hp && !collision.units[2].hp && collision.ended);
        collision.turn = BATTLE_PARTY;
        collision.ended = false;
        collision.units[1].hp = collision.units[2].hp = 10;
        attack(collision, cell(2, 1));
        CHECK(collision.units[1].hp == (special ? 2 : 5) && collision.units[1].cell == cell(2, 1));
        CHECK(collision.units[2].hp == 7);
        collision.turn = BATTLE_PARTY;
        collision.units[1].hp = 10;
        collision.units[2].hp = 3;
        attack(collision, cell(2, 1));
        CHECK(collision.units[1].cell == cell(2, 1) && !collision.units[2].hp);

        member.attack_class = ATTACK_REND;
        auto normal_rush = special ? 9 : 6;
        duel.units[1].hp = 5;
        uint8_t half = (uint8_t)(battle_max_hp(duel.units[0]) / 2);
        duel.units[0].hp = half;
        CHECK(battle_attack_damage(duel, 0, 1, special) == normal_rush + 2);
        duel.units[0].hp = (uint8_t)(half + 1);
        CHECK(battle_attack_damage(duel, 0, 1, special) == normal_rush);
        duel.units[0].hp = 11;

        member.attack_class = ATTACK_ARC;
        BattleState arc = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 11},
                                               {2, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                               {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                               {1, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
        CHECK(battle_attack_damage(arc, 0, 1, special) == (special ? 9 : 7));
        arc.units[3].team = BATTLE_PARTY;
        CHECK(battle_attack_damage(arc, 0, 1, special) == (special ? 9 : 7));
        arc.units[2].hp = 0;
        CHECK(battle_attack_damage(arc, 0, 1, special) == (special ? 7 : 5));
        arc.units[3].hp = 0;
        CHECK(battle_attack_damage(arc, 0, 1, special) == (special ? 8 : 6));

        member.attack_class = ATTACK_DRAIN;
        BattleState drain = board(
            BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 11}, {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
        BattleState guarded = drain;
        guarded.units[1].id = battle_party_id(1);
        guarded.units[2] = {cell(2, 2), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
        guarded.unit_count = 3;
        attack(guarded, cell(2, 1));
        CHECK(guarded.units[1].hp == (special ? 5 : 8));
        CHECK(guarded.units[0].hp == (special ? 14 : 12));
        BattleState near_full = drain;
        near_full.units[0].hp = 13;
        attack(near_full, cell(2, 1));
        CHECK(near_full.units[0].hp == 14);
        BattleState finish = drain;
        finish.units[1].hp = 1;
        attack(finish, cell(2, 1));
        CHECK(finish.units[0].hp == 14 && finish.ended);
        attack(drain, cell(2, 1));
        CHECK(drain.units[0].hp == 14 && drain.units[1].hp == (special ? 3 : 6));

        member.attack_class = ATTACK_RECOIL;
        for(uint8_t obstruction = 0; obstruction < 3; ++obstruction) {
            BattleState recoil = board(BATTLE_PARTY, {{3, 1, id, BATTLE_PARTY, 11},
                                                      {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                      {2, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 0}});
            if(obstruction) {
                recoil.units[2].hp = 10;
                place(recoil, 2, obstruction, 1);
            }
            attack(recoil, cell(4, 1));
            CHECK(recoil.units[0].cell == cell(1 + obstruction, 1));
            CHECK(recoil.units[1].hp == (special ? 1 : 4) + (obstruction == 2));
        }

        member.attack_class = ATTACK_FLANK;
        BattleState flank = board(BATTLE_PARTY, {{1, 1, id, BATTLE_PARTY, 11},
                                                 {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                 {2, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                                 {3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
        CHECK(battle_attack_damage(flank, 0, 1, special) == (special ? 11 : 9));
        flank.units[3].hp = 0;
        CHECK(battle_attack_damage(flank, 0, 1, special) == (special ? 10 : 8));
        flank.units[2].hp = 0;
        CHECK(battle_attack_damage(flank, 0, 1, special) == (special ? 6 : 4));

        member.attack_class = ATTACK_BLAST;
        BattleState blast = board(BATTLE_PARTY, {{2, 1, id, BATTLE_PARTY, 11},
                                                 {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                                 {5, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 2},
                                                 {6, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 1},
                                                 {4, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10},
                                                 {7, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
        CHECK(battle_attack_damage(blast, 0, 1, special) == (special ? 6 : 4));
        impact = battle_attack_impact(blast, 0, 1);
        CHECK(impact.damage == 7 && impact.kills == 3);
        attack(blast, cell(5, 1));
        CHECK(!blast.units[1].hp && !blast.units[2].hp && !blast.units[3].hp);
        CHECK(blast.units[4].hp == 10 && blast.units[5].hp == 10);
    }
}

static void fourth_rank_movement() {
    game = {};
    game.party.count = 1;
    auto hero = (ENEMY_ID)battle_party_id(0);
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    member.level = 8;
    for(uint8_t rank = 0; rank < 4; ++rank)
        CHECK(party_learn_perk(member, PERK_MOVEMENT));

    BattleState guard = board(BATTLE_ENEMY, {{3, 1, hero, BATTLE_PARTY, 10},
                                            {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 1},
                                            {3, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    CHECK(battle_attack_impact(guard, 1, 0).reflected == 1);
    battle_apply(guard, act(1, 3, 1));
    CHECK(guard.units[0].hp == 9 && !guard.units[1].hp && guard.result == BATTLE_VICTORY);

    member.movement_class = MOVEMENT_LEAP;
    BattleState leap = board(BATTLE_PARTY, {{1, 0, hero, BATTLE_PARTY, 10},
                                           {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 6}});
    BattleState crowded = leap;
    crowded.units[crowded.unit_count++] = {cell(4, 1), 6, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
    battle_apply(crowded, act(0, 3, 1));
    CHECK(crowded.units[1].cell == cell(2, 1) && crowded.units[2].cell == cell(4, 1));
    battle_apply(leap, act(0, 3, 1));
    CHECK(leap.units[1].hp == 3 && leap.units[1].cell == cell(1, 0) && leap.units[0].hp == 9);

    member.movement_class = MOVEMENT_CHARGE;
    BattleState charge = board(BATTLE_PARTY, {{1, 1, hero, BATTLE_PARTY, 10},
                                             {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 6},
                                             {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 6}});
    BattleAction trample = act(0, 3, 1);
    CHECK(battle_legal(charge, trample));
    check_enumeration(charge, false);
    CHECK(battle_engagement(charge, 0, trample.destination).damage == 6);
    battle_apply(charge, trample);
    CHECK(charge.units[1].hp == 3 && charge.units[1].cell == cell(2, 1));
    CHECK(charge.units[2].hp == 3 && charge.units[2].cell == cell(5, 1));

    member.movement_class = MOVEMENT_VAULT;
    BattleState vault = board(BATTLE_PARTY, {{1, 1, hero, BATTLE_PARTY, 10},
                                            {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 6},
                                            {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 6}});
    BattleAction high = act(0, 4, 1);
    CHECK(battle_legal(vault, high));
    check_enumeration(vault, false);
    CHECK(battle_engagement(vault, 0, high.destination).damage == 4);
    battle_apply(vault, high);
    CHECK(vault.units[1].hp == 5 && vault.units[2].hp == 3);
    BattleState terrain = board(BATTLE_PARTY, {{2, 0, hero, BATTLE_PARTY, 10},
                                               {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 6}});
    CHECK(battle_legal(terrain, act(0, 5, 0)));
    check_enumeration(terrain, false);

    member.movement_class = MOVEMENT_GLIDE;
    BattleState glide = board(BATTLE_PARTY, {{0, 0, hero, BATTLE_PARTY, 10},
                                            {0, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                            {0, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                            {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5}});
    CHECK(battle_engagement(glide, 0, cell(2, 2)).damage == 6);
    battle_apply(glide, act(0, 2, 2));
    CHECK(glide.units[1].hp == 3 && glide.units[2].hp == 3 && glide.units[3].hp == 3);
}

static void fourth_rank_attacks() {
    game = {};
    game.party.count = 2;
    auto hero = (ENEMY_ID)battle_party_id(0);
    auto ally = (ENEMY_ID)battle_party_id(1);
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    game.party.members[1] = party_new_member(NPC_ELIZA, MOVEMENT_STEP, ATTACK_SHOOT);
    member.level = 8;
    for(uint8_t rank = 0; rank < 4; ++rank)
        CHECK(party_learn_perk(member, PERK_ATTACK));

    BattleState lock = board(BATTLE_ENEMY, {{3, 1, hero, BATTLE_PARTY, 10},
                                           {2, 1, ENEMY_WOLF, BATTLE_ENEMY, 9}});
    CHECK(!battle_legal(lock, act(1, 1, 1)) && battle_legal(lock, act(1, 3, 1)));
    check_enumeration(lock, false);
    lock.units[lock.unit_count++] = {cell(2, 2), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
    CHECK(battle_legal(lock, act(1, 1, 1)));

    member.attack_class = ATTACK_SHOOT;
    BattleState shot = board(BATTLE_PARTY, {{0, 1, hero, BATTLE_PARTY, 10},
                                           {5, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                           {7, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 8}});
    CHECK(battle_legal(shot, act(0, 5, 1)) && !battle_legal(shot, act(0, 7, 1)));
    CHECK(battle_attack_impact(shot, 0, 1).damage == 10);
    check_enumeration(shot, false);
    BattleState screened = shot;
    screened.units[screened.unit_count++] = {cell(6, 1), 10, ally, BATTLE_PARTY};
    CHECK(battle_attack_impact(screened, 0, 1).damage == 5);
    battle_apply_special(shot, act(0, 5, 1));
    CHECK(member.ap == 2 && shot.units[1].hp == 1 && shot.units[2].hp == 2);

    member.attack_class = ATTACK_REACH;
    BattleState fence = board(BATTLE_ENEMY, {{4, 1, hero, BATTLE_PARTY, 10},
                                            {3, 1, ally, BATTLE_PARTY, 10},
                                            {1, 1, ENEMY_WOLF, BATTLE_ENEMY, 9}});
    CHECK(!battle_legal(fence, act(2, 2, 1)) && battle_legal(fence, act(2, 0, 1)));
    check_enumeration(fence, false);
    fence.units[2].cell = cell(2, 1);
    CHECK(battle_legal(fence, act(2, 1, 1)));
    CHECK(battle_can_attack(fence, 0, 2));

    member.attack_class = ATTACK_IMPACT;
    BattleState impact = board(BATTLE_PARTY, {{0, 1, hero, BATTLE_PARTY, 10},
                                              {1, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                              {2, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                              {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                              {6, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    CHECK(battle_attack_impact(impact, 0, 1).damage == 11);
    BattleState jammed = impact;
    jammed.units[4].cell = cell(4, 1);
    jammed.units[4].team = BATTLE_PARTY;
    battle_apply(jammed, act(0, 1, 1));
    CHECK(jammed.units[2].hp == 7 && jammed.units[2].cell == cell(2, 1));
    battle_apply(impact, act(0, 1, 1));
    CHECK(impact.units[1].hp == 5 && impact.units[1].cell == cell(3, 1));
    CHECK(impact.units[2].hp == 7 && impact.units[2].cell == cell(4, 1));
    CHECK(impact.units[3].hp == 7 && impact.units[3].cell == cell(5, 1));
    CHECK(impact.units[4].hp == 10);

    member.attack_class = ATTACK_REND;
    BattleState hunt = board(BATTLE_PARTY, {{1, 1, hero, BATTLE_PARTY, 10},
                                           {3, 1, ENEMY_BRUTE, BATTLE_ENEMY, 6}});
    CHECK(!battle_can_attack(hunt, 0, 1));
    hunt.units[1].hp = 5;
    CHECK(battle_special_legal(hunt, act(0, 3, 1)));
    check_enumeration(hunt, false);
    battle_apply(hunt, act(0, 3, 1));
    CHECK(hunt.units[0].cell == cell(1, 1) && !hunt.units[1].hp);

    member.attack_class = ATTACK_ARC;
    BattleState arc = board(BATTLE_PARTY, {{2, 1, hero, BATTLE_PARTY, 10},
                                          {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 7},
                                          {2, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 7},
                                          {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 7}});
    CHECK(battle_attack_impact(arc, 0, 1).kills == 3);
    CHECK(battle_search_priority(arc, act(0, 3, 2)) == BATTLE_PRIORITY_WIN);
    battle_apply(arc, act(0, 3, 2));
    CHECK(arc.ended && arc.result == BATTLE_VICTORY);

    member.attack_class = ATTACK_DRAIN;
    BattleState drain = board(BATTLE_PARTY, {{2, 1, hero, BATTLE_PARTY, 10},
                                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                             {7, 3, ally, BATTLE_PARTY, 10},
                                             {4, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 0}});
    battle_apply(drain, act(0, 3, 1));
    CHECK(drain.units[0].hp == 15 && drain.units[2].hp == 12 && !drain.units[3].hp);

    member.attack_class = ATTACK_RECOIL;
    BattleState recoil = board(BATTLE_PARTY, {{2, 1, hero, BATTLE_PARTY, 10},
                                             {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10}});
    battle_apply(recoil, act(0, 3, 1));
    CHECK(recoil.units[0].cell == cell(0, 1) && recoil.units[1].cell == cell(2, 1));
    CHECK(recoil.units[0].hp == 10 && recoil.units[1].hp == 4);

    member.attack_class = ATTACK_FLANK;
    BattleState flank = board(BATTLE_PARTY, {{2, 1, hero, BATTLE_PARTY, 10},
                                            {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {3, 1, ally, BATTLE_PARTY, 10}});
    CHECK(battle_can_attack(flank, 0, 1) && battle_attack_damage(flank, 0, 1) == 8);
    check_enumeration(flank, false);
    flank.units[1].cell = cell(4, 1);
    CHECK(battle_can_attack(flank, 0, 1));
    flank.units[2].hp = 0;
    CHECK(!battle_can_attack(flank, 0, 1));

    member.attack_class = ATTACK_BLAST;
    BattleState blast = board(BATTLE_PARTY, {{1, 1, hero, BATTLE_PARTY, 10},
                                            {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                            {4, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {4, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            {5, 1, ally, BATTLE_PARTY, 10}});
    battle_apply(blast, act(0, 3, 1));
    CHECK(!blast.units[1].hp);
    CHECK(blast.units[2].hp == 8 && blast.units[2].cell == cell(4, 1));
    CHECK(blast.units[3].hp == 9 && blast.units[3].cell == cell(5, 3));
}

static void new_classes_in_search() {
    game = {};
    game.party.count = PARTY_CAPACITY;
    for(uint8_t slot = 0; slot < PARTY_CAPACITY; ++slot) {
        game.party.members[slot] = party_new_member(NPC_ALDEN, MOVEMENT_GLIDE, ATTACK_ARC);
        game.party.members[slot].level = 4;
        CHECK(party_learn_perk(game.party.members[slot], PERK_MOVEMENT));
        CHECK(party_learn_perk(game.party.members[slot], PERK_ATTACK));
    }

    auto id = (ENEMY_ID)battle_party_id(0);
    BattleState diagonal =
        board(BATTLE_ENEMY, {{2, 1, id, BATTLE_ENEMY, 20}, {3, 2, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 3}});
    CHECK(battle_attack_gap(id, 1, 1) == 0 && battle_attack_gap(id, 1, 0) == 1);
    CHECK(same_action(battle_enemy_action(diagonal), act(0, 3, 2)));
    expect_same_as_reference(diagonal, 3);

    BattleState glide = board(
        BATTLE_ENEMY, {{0, 0, id, BATTLE_ENEMY, 20}, {3, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    BattleAction glide_approach = act(0, 2, 2);
    CHECK(battle_search_priority(glide, glide_approach) == BATTLE_PRIORITY_APPROACH);
    CHECK(same_action(battle_greedy_action(glide), glide_approach));
    battle_apply(glide, glide_approach);
    CHECK(battle_can_attack(glide, 0, 1));

    game.party.members[0].movement_class = MOVEMENT_VAULT;
    game.party.members[0].attack_class = ATTACK_STRIKE;
    BattleState vault = board(
        BATTLE_ENEMY, {{0, 1, id, BATTLE_ENEMY, 20}, {6, 1, BATTLE_TEST_FIGHTER, BATTLE_PARTY, 10}});
    BattleAction vault_approach = act(0, 2, 1);
    CHECK(battle_search_priority(vault, vault_approach) == BATTLE_PRIORITY_APPROACH);
    CHECK(same_action(battle_greedy_action(vault), vault_approach));

    for(unsigned seed = 0; seed < 250; ++seed) {
        BattleState s = random_board();
        bool capstones = (seed & 2) != 0;
        for(uint8_t i = 0; i < PARTY_CAPACITY; ++i) {
            auto& member = game.party.members[i];
            member.movement_class = (uint8_t)((seed + i) % MOVEMENT_CLASS_COUNT);
            member.attack_class = (uint8_t)((seed / MOVEMENT_CLASS_COUNT + i) % ATTACK_CLASS_COUNT);
            member.level = 10;
            member.perk_ranks = (seed + i) % 2 ? 3 | (2 << 3) : 2 | (3 << 3);
            if(capstones)
                member.perk_ranks = (seed + i) % 2 ? 4 | (1 << 3) : 1 | (4 << 3);
            if(i < s.unit_count)
                s.units[i].id = battle_party_id(i);
        }
        check_enumeration(s, !capstones);
        uint8_t actions[BATTLE_ACTION_CAPACITY], attacks;
        uint8_t count = battle_actions(s, actions, attacks);
        battle_order_actions(s, actions, count, attacks, true);
        auto enemy = s.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
        for(uint8_t i = 0; i < count; ++i) {
            BattleAction action = battle_unpack(actions[i]);
            BattleState successor = s;
            battle_apply(successor, action);
            auto kind = battle_action_kind(s, action);
            uint8_t kills = battle_living(s, enemy) - battle_living(successor, enemy);
            CHECK(total_hp(successor) < total_hp(s) || kills);
            CHECK((kind == BATTLE_ACTION_WIN) == (battle_living(successor, enemy) == 0));
            CHECK((kind == BATTLE_ACTION_KILL) == (kills && battle_living(successor, enemy)));
            if(i)
                CHECK(battle_search_priority(s, battle_unpack(actions[i - 1])) <=
                      battle_search_priority(s, action));
        }
        uint8_t depth = battle_search_depth_limit(s);
        CHECK(depth >= 1 && depth <= BATTLE_AI_DEPTH);
        expect_same_as_reference(s, depth < 2 ? depth : 2);
    }
}

static void equipment_attacks() {
    game = {};
    game.party.count = 2;
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    auto hero = (ENEMY_ID)battle_party_id(0);
    auto ally = (ENEMY_ID)battle_party_id(1);
    BattleState original = board(BATTLE_PARTY, {
                                                   {2, 1, hero, BATTLE_PARTY, 20},
                                                   {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                   {2, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                   {3, 2, ally, BATTLE_PARTY, 20},
                                               });
    for(ITEM_ID item : {ITEM_FIRSTSTRIKE_SWORD, ITEM_HUNTERS_BLADE, ITEM_NEEDLE_SPEAR,
                        ITEM_CRACKED_SABER, ITEM_BRASS_KNUCKLE, ITEM_STONEBREAKER, ITEM_WOLF_FANG,
                        ITEM_WOODCUT_AXE, ITEM_MUD_SLING, ITEM_BARN_FORK}) {
        auto state = original;
        member.equipment[SLOT_WEAPON] = equipment_encode(item);
        member.attack_class = ATTACK_STRIKE;
        uint8_t bonus = 1;
        bool once = false;
        switch(item) {
            case ITEM_FIRSTSTRIKE_SWORD:
                bonus = 2;
                once = true;
                break;
            case ITEM_HUNTERS_BLADE:
                state.units[1].hp = 9;
                bonus = 3;
                once = true;
                break;
            case ITEM_CRACKED_SABER:
                state.units[0].hp = 4; // Half of the 9 HP that the saber leaves.
                bonus = 3;
                once = true;
                break;
            case ITEM_STONEBREAKER:
                place(state, 0, 4, 2);
                place(state, 1, 4, 1);
                break;
            case ITEM_WOODCUT_AXE:
                place(state, 1, 2, 0);
                break;
            case ITEM_MUD_SLING:
                place(state, 0, 4, 1);
                place(state, 1, 7, 1);
                member.attack_class = ATTACK_SHOOT;
                bonus = 2;
                break;
            case ITEM_BARN_FORK:
                state.units[0].hp = party_max_hp(member); // The fork rewards full HP.
                break;
            default:
                break;
        }
        BattleAction action{0, state.units[1].cell};
        CHECK(battle_legal(state, action));
        CHECK(battle_attack_damage(state, 0, 1) == 3 + bonus);
        CHECK(battle_attack_impact(state, 0, 1).damage == 3 + bonus);
        CHECK(state.units[0].equipment_used() == 0);
        uint8_t hp = state.units[1].hp;
        battle_apply(state, action);
        CHECK(state.units[1].hp == hp - (3 + bonus));
        CHECK(state.units[0].equipment_used() == (once ? 1 << SLOT_WEAPON : 0));
        if(once)
            CHECK(battle_attack_damage(state, 0, 1) == (item == ITEM_CRACKED_SABER ? 4 : 3));
    }

    member.level = BATTLE_STARTER_LEVEL;
    member.attack_class = ATTACK_BLAST;
    member.equipment[SLOT_WEAPON] = equipment_encode(ITEM_FIRSTSTRIKE_SWORD);
    member.equipment[SLOT_RELIC] = equipment_encode(ITEM_GRAVE_TOKEN);
    BattleState blast = board(BATTLE_PARTY, {
                                                {1, 1, hero, BATTLE_PARTY, 1},
                                                {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                                {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                            });
    CHECK(battle_attack_impact(blast, 0, 1).damage == 11);
    BattleState special = blast;
    battle_apply_special(special, act(0, 3, 1), BATTLE_STARTER);
    CHECK(special.units[1].hp == 3 && special.units[2].hp == 6);
    CHECK(special.units[0].equipment_used() == 0);
    battle_apply(blast, act(0, 3, 1));
    CHECK(blast.units[1].hp == 3 && blast.units[2].hp == 6);
    CHECK(battle_attack_damage(blast, 0, 1) == 5);
}

static void equipment_moves_and_guards() {
    game = {};
    game.party.count = 2;
    auto& member = game.party.members[0];
    member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    auto hero = (ENEMY_ID)battle_party_id(0);
    auto ally = (ENEMY_ID)battle_party_id(1);
    auto move = board(BATTLE_PARTY, {
                                        {4, 1, hero, BATTLE_PARTY, 10},
                                        {3, 0, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 5},
                                        {3, 2, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 4},
                                    });
    member.equipment[SLOT_WEAPON] = equipment_encode(ITEM_THORN_PIKE);
    auto thorn = battle_engagement(move, 0, cell(3, 1));
    CHECK(thorn.damage == 3 && thorn.kills == 0);
    member.equipment[SLOT_WEAPON] = equipment_encode(ITEM_PATHFINDERS_KNIFE);
    member.equipment[SLOT_ARMOR] = equipment_encode(ITEM_SCOUT_SANDALS);
    member.equipment[SLOT_RELIC] = equipment_encode(ITEM_THORN_RING);
    auto engagement = battle_engagement(move, 0, cell(3, 1));
    CHECK(engagement.damage == 6 && engagement.kills == 1);
    CHECK(battle_move_cost(move.units[0], true) == 0 && !move.units[0].equipment_used());
    CHECK(battle_action_kind(move, act(0, 3, 1)) == BATTLE_ACTION_KILL);
    auto simulated = move;
    battle_simulate_action(simulated, act(0, 3, 1));
    CHECK(simulated.units[0].equipment_used() == 7 && move.units[0].equipment_used() == 0);
    battle_apply(move, act(0, 3, 1));
    CHECK(same_state(simulated, move));
    CHECK(move.units[0].hp == 10 && move.units[1].hp == 3 && move.units[2].hp == 0);
    CHECK(battle_move_cost(move.units[0], true) == 1);
    static constexpr auto encounter = battle_encounter(ENEMY_WOLF);
    battle_begin(move, &encounter);
    CHECK(move.units[0].equipment_used() == 0 && battle_move_cost(move.units[0], true) == 0);

    member.equipment[SLOT_ARMOR] = equipment_encode(ITEM_ASH_CLOAK);
    move = board(BATTLE_PARTY, {
                                   {2, 1, hero, BATTLE_PARTY, 10},
                                   {7, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 9},
                               });
    CHECK(battle_move_cost(move.units[0], true) == 1);
    battle_apply(move, act(0, 3, 1));
    CHECK(move.units[0].hp == 10 && battle_move_cost(move.units[0], false) == 1);
    member.equipment[SLOT_ARMOR] = equipment_encode(ITEM_LEAD_BOOTS);
    member.level = 4;
    member.perk_ranks = FOOTWORK_RANK;
    CHECK(battle_move_cost(move.units[0], false) == 2);
    CHECK(battle_move_cost(move.units[0], true) == 1);
    member.perk_ranks = 0;

    auto guard = board(BATTLE_ENEMY, {
                                         {2, 1, hero, BATTLE_PARTY, 10},
                                         {3, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 10},
                                         {2, 0, ally, BATTLE_PARTY, 20},
                                     });
    for(ITEM_ID item : {ITEM_GUARD_VEST, ITEM_OAK_SHIELD, ITEM_REED_ARMOR, ITEM_RIVER_CLOAK,
                        ITEM_PADDED_SHAWL, ITEM_LEAD_BOOTS, ITEM_TURNSHOE}) {
        auto state = guard;
        member.equipment[SLOT_ARMOR] = equipment_encode(item);
        if(item == ITEM_OAK_SHIELD)
            state.units[0].hp = 6; // Half of the maximum, where the shield guards.
        if(item == ITEM_REED_ARMOR)
            state.units[2].hp = 0;
        if(item == ITEM_TURNSHOE)
            place(state, 0, 3, 0);
        if(item == ITEM_RIVER_CLOAK) {
            state.units[1].id = BATTLE_TEST_ARCHER;
            place(state, 1, 6, 1);
        }
        bool once = item != ITEM_OAK_SHIELD && item != ITEM_LEAD_BOOTS;
        CHECK(battle_attack_damage(state, 1, 0) == (once ? 1 : 2));
        CHECK(!state.units[0].equipment_used());
        battle_apply(state, {1, state.units[0].cell});
        CHECK(state.units[0].hp == (once ? 9 : item == ITEM_OAK_SHIELD ? 4 : 8));
        CHECK(state.units[0].equipment_used() == (once ? 1 << SLOT_ARMOR : 0));
        CHECK(battle_attack_damage(state, 1, 0) == (once ? 3 : 2));
    }

    member.equipment[SLOT_ARMOR] = equipment_encode(ITEM_GUARD_VEST);
    CHECK(equipment_acquire(ITEM_LANTERN_WICK));
    game.party.members[1].equipment[SLOT_RELIC] = equipment_encode(ITEM_LANTERN_WICK);
    CHECK(battle_attack_damage(guard, 1, 0) == 1 && !guard.units[2].equipment_used());
    battle_apply(guard, act(1, 2, 1));
    CHECK(guard.units[0].hp == 9 && guard.units[0].equipment_used() == 1 << SLOT_ARMOR);
    CHECK(guard.units[2].equipment_used() == 1 << SLOT_RELIC);
    CHECK(battle_attack_damage(guard, 1, 0) == 3);
}

static void equipment_victory_rewards() {
    game = {};
    game.party.count = PARTY_CAPACITY;
    for(auto& member : game.party.members)
        member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    game.party.members[0].equipment[SLOT_RELIC] = equipment_encode(ITEM_FERRY_BEAD);
    game.party.members[1].equipment[SLOT_RELIC] = equipment_encode(ITEM_SPLIT_BUTTON);
    game.party.members[2].equipment[SLOT_RELIC] = equipment_encode(ITEM_SURVIVORS_RIBBON);
    game.party.members[3].equipment[SLOT_RELIC] = equipment_encode(ITEM_SLUICE_TALLY);
    auto victory = board(BATTLE_PARTY, {
                                           {2, 1, (ENEMY_ID)battle_party_id(0), BATTLE_PARTY, 18},
                                           {3, 1, (ENEMY_ID)battle_party_id(2), BATTLE_PARTY, 1},
                                           {1, 1, (ENEMY_ID)battle_party_id(1), BATTLE_PARTY, 1},
                                           {5, 1, (ENEMY_ID)battle_party_id(3), BATTLE_PARTY, 0},
                                           {6, 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY, 0},
                                       });
    victory.ended = true;
    victory.result = BATTLE_VICTORY;
    CHECK(battle_finish(victory).gold_gained == 4);
    CHECK(game.gold == 4 && game.party.members[3].hp == 0);
    CHECK(game.party.members[0].hp == 18);
    CHECK(game.party.members[1].hp == 7 && game.party.members[2].hp == 2);
    game.gold = UINT16_MAX - 1;
    CHECK(battle_finish(victory).gold_gained == 1 && game.gold == UINT16_MAX);
}

int main() {
    equipment_attacks();
    equipment_moves_and_guards();
    equipment_victory_rewards();
    attack_perks();
    perk_victory_recovery();
    charge_perks();
    step_and_leap_perks();
    reach_and_impact_perks();
    random_battle_deployment();
    saved_party_battles();
    rend_damage();
    level_attack_damage();
    victory_ap_recovery();
    victory_gold();
    special_attacks();
    signature_attacks();
    maximum_battle_damage_frame();
    unit_definitions();
    creature_actions();
    enemy_ranks_and_ap();
    initial_layout();
    teams_are_explicit();
    battlefield_data();
    board_edges();
    fighter_movement();
    rogue_movement();
    wolf_movement();
    board_boundaries();
    terrain();
    melee_attacks();
    archer_attacks();
    spearman_attacks();
    brute_attacks();
    activations_and_alternation();
    blood_price();
    engagement_damage();
    sacrifices_and_outcomes();
    forced_pass();
    termination();
    determinism_and_copies();
    action_encoding();
    unit_packing();
    legal_action_enumeration();
    search_ordering();
    terminal_scores();
    attack_gaps();
    utility_weights();
    negamax_matches_reference();
    search_simulates_mechanics();
    search_matches_reference_on_engagements();
    voluntary_pass_search();
    sacrifice_search();
    search_bounds();
    budget_boundaries();
    enemy_behavior();
    party_search_uses_leaps();
    four_a_side();
    cursor_easing();
    lunge_timing();
    flash_timing();
    arrow_timing();
    leap_arc();
    thinking_indicator();
    target_frames();
    new_combat_geometry();
    new_movement_effects();
    new_direct_attacks();
    drain_and_recoil();
    arc_corner_perk();
    flank_formation();
    blast_splash_and_search();
    third_rank_movement();
    third_rank_attacks();
    fourth_rank_movement();
    fourth_rank_attacks();
    new_classes_in_search();
    printf("PASS battle rules: %u checks\n", checks);
    return 0;
}
