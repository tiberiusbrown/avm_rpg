#include "battle_ai.h"

// Enemy utility weights, in whole points. The enemy values the damage it deals above the damage it
// takes, and it values eliminations above ordinary HP. Gap and strike terms steer it toward the
// party. The utility counts living units and HP as differences, so constant offsets never matter.
static constexpr int16_t UTILITY_ENEMY_UNIT = 550;
static constexpr int16_t UTILITY_PARTY_UNIT = 600;
static constexpr int16_t UTILITY_PARTY_HP = 180;
static constexpr int16_t UTILITY_ENEMY_HP = 90;
// Per half move each enemy still needs before it can attack its nearest party member, and again for
// the enemy closest to attacking. A unit that covers two cells in one move needs one half move per
// cell, and any other unit needs two, so a cell is worth 20 to a stepping unit.
static constexpr int16_t UTILITY_GAP = 10;
static constexpr int16_t UTILITY_NEAREST = 10;
// A strike that the side to act can make now, and one that the other side makes after one activation.
// Mutual engagement is worth more when the enemy acts next, and less when the party acts next.
static constexpr int16_t UTILITY_STRIKE_NOW = 120;
static constexpr int16_t UTILITY_STRIKE_LATER = 60;

// Windows start beyond every score the search produces.
static constexpr int16_t SCORE_INFINITE = 32000;

// Greedy values: a win above everything and a losing sacrifice below everything; otherwise a net
// exchange of units and HP, from which a move then subtracts its attack gap to the nearest opponent
// (at most about ten cells, under one HP's worth).
static constexpr int16_t GREEDY_WIN = 10000, GREEDY_LOSS = -10000;
static constexpr int16_t GREEDY_KILL = 100, GREEDY_HP = 10;

// An iteration is started only when the budget left is at least this many times the nodes of the
// last completed iteration. A deeper iteration that cannot finish would only spend time and be
// discarded.
static constexpr uint8_t ITERATION_GROWTH = 2;

static constexpr BattleAction PASS_ACTION{BATTLE_NO_UNIT, 0};

// The explicit search stack. Each ply keeps its alpha and the next and end indices of its ordered
// actions in the shared action stack. A ply's beta is always its parent's alpha negated: the
// parent's alpha changes only after the child returns, so it is read from the parent rather than
// stored. The search fails hard: a node's value is its alpha, clamped to its window, which picks
// the same root action and score as an exact search and needs no separate best score. Positions are
// kept separately, so the root reads the caller's state and never copies it.
struct SearchPly {
    int16_t alpha;
    uint8_t next, end;
};

// The most legal actions one unit of a type can have anywhere on the board, derived from its rules
// and the board's size at compile time. Along each direction a ray yields its moves up to the first
// unit and at most one attack on that unit, within the cells left before the edge; a knight has its
// leaps that land on the board and at most one attack per direction. On the 8x4 board that is 4 for
// a fighter or a brute, 7 for an archer, a spearman, or a wolf, and 10 for a rogue (six leaps at
// most). `moves` is the most moves alone, which with one attack per opponent also bounds the unit.
struct UnitLimit {
    uint8_t actions, moves;
};

// The most actions along one direction with `room` cells before the edge.
static constexpr uint8_t ray_limit(BattleUnitRules const& rules, uint8_t room) {
    uint8_t range = rules.movement_type == BATTLE_MOVEMENT_KNIGHT ? 0 : rules.move_range;
    uint8_t best = room < range ? room : range; // No unit along the ray: moves only.
    for(uint8_t first = 1; first <= room; ++first) {
        uint8_t moves = first - 1 < range ? (uint8_t)(first - 1) : range;
        uint8_t attack = first >= rules.attack_min && first <= rules.attack_max ? 1 : 0;
        if(moves + attack > best)
            best = (uint8_t)(moves + attack);
    }
    return best;
}

static constexpr UnitLimit unit_limit(BattleUnitRules const& rules) {
    constexpr int8_t leap_x[8] = {1, 2, 2, 1, -1, -2, -2, -1};
    constexpr int8_t leap_y[8] = {-2, -1, 1, 2, 2, 1, -1, -2};
    UnitLimit limit{0, 0};
    for(uint8_t y = 0; y < BATTLE_ROWS; ++y)
        for(uint8_t x = 0; x < BATTLE_COLUMNS; ++x) {
            uint8_t rooms[4] = {y, (uint8_t)(BATTLE_COLUMNS - 1 - x),
                                (uint8_t)(BATTLE_ROWS - 1 - y), x};
            uint8_t moves = 0, actions = 0;
            for(uint8_t room : rooms)
                actions = (uint8_t)(actions + ray_limit(rules, room));
            if(rules.movement_type == BATTLE_MOVEMENT_KNIGHT) {
                for(uint8_t k = 0; k < 8; ++k) {
                    int nx = x + leap_x[k], ny = y + leap_y[k];
                    if(nx >= 0 && nx < BATTLE_COLUMNS && ny >= 0 && ny < BATTLE_ROWS)
                        ++moves;
                }
                actions = (uint8_t)(actions + moves);
            } else {
                for(uint8_t room : rooms)
                    moves = (uint8_t)(moves + (room < rules.move_range ? room : rules.move_range));
            }
            if(actions > limit.actions)
                limit.actions = actions;
            if(moves > limit.moves)
                limit.moves = moves;
        }
    return limit;
}

struct UnitLimits {
    UnitLimit types[BATTLE_UNIT_TYPE_COUNT];
};

static constexpr UnitLimits make_unit_limits() {
    UnitLimits limits{};
    for(uint8_t t = 0; t < BATTLE_UNIT_TYPE_COUNT; ++t)
        limits.types[t] = unit_limit(BATTLE_UNIT_RULES[t]);
    return limits;
}

// Constant, so on the AVM it sits in program memory and costs no RAM.
static constexpr UnitLimits UNIT_LIMITS = make_unit_limits();
static_assert(UNIT_LIMITS.types[BATTLE_TYPE_FIGHTER].actions == 4 &&
                  UNIT_LIMITS.types[BATTLE_TYPE_ARCHER].actions == 7 &&
                  UNIT_LIMITS.types[BATTLE_TYPE_WOLF].actions == 7 &&
                  UNIT_LIMITS.types[BATTLE_TYPE_ROGUE].actions == 10 &&
                  UNIT_LIMITS.types[BATTLE_TYPE_ROGUE].moves == 6,
              "per-type action limits on the 8x4 board");

// The most actions one side can list: each living unit's limit, with its attacks capped by the
// opponents it faces. A side with no action still lists its forced pass.
static uint8_t side_actions(BattleState const& state, BATTLE_TEAM team, uint8_t opponents) {
    uint8_t total = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp || unit.team != team)
            continue;
        UnitLimit const& limit = UNIT_LIMITS.types[unit.type];
        uint8_t most = (uint8_t)(limit.moves + opponents);
        total = (uint8_t)(total + (limit.actions < most ? limit.actions : most));
    }
    return total ? total : 1;
}

// The plies on the stack list their actions one after another, each starting where its parent's
// list ends. The stack is battle_action_scratch, which nothing else uses while the search runs. Its
// 80 bytes hold two plies of any battle of up to four a side, even four rogues a side (40 actions
// each); three plies of a mixed four-on-four battle; and four plies of any two-on-two battle (at
// most 16 a side). battle_search_depth_limit stops the search at the deepest ply the stack holds.
static constexpr uint16_t ACTION_STACK_CAPACITY = BATTLE_ACTION_SCRATCH_CAPACITY;

static SearchPly plies[BATTLE_AI_DEPTH];
static_assert(ACTION_STACK_CAPACITY >= 2 * BATTLE_SIDE_CAPACITY * 10 &&
                  ACTION_STACK_CAPACITY <= 255,
              "two plies of four rogues a side fit, and the stack's indices are bytes");

uint8_t battle_search_depth_limit(BattleState const& state) {
    // Sides alternate and never gain units, so the living units now bound every list below this
    // position: the side to act lists at even plies and the other side at odd ones.
    uint8_t mine = 0, theirs = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp)
            ++(unit.team == state.turn ? mine : theirs);
    }
    BATTLE_TEAM other_team = state.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    uint8_t own = side_actions(state, state.turn, theirs);
    uint8_t other = side_actions(state, other_team, mine);
    uint16_t total = 0;
    uint8_t depth = 0;
    while(depth < BATTLE_AI_DEPTH) {
        total = (uint16_t)(total + ((depth & 1) ? other : own));
        if(total > ACTION_STACK_CAPACITY)
            break;
        ++depth;
    }
    // Even a contrived side of eight units lists at most 8 x 7 actions (six leaps and one attack
    // each), so a root list of any position fits and the limit is at least one.
    return depth ? depth : 1;
}

// Each ply's killer: the last move that cut a node off at that ply, tried first among that ply's
// moves in the next node there. Killers are reset at the start of every search, so results stay
// deterministic.
static uint8_t killers[BATTLE_AI_DEPTH];
// The children of ply k are built in ply_state[k], whether they are expanded or scored as leaves.
static BattleState ply_state[BATTLE_AI_DEPTH];

// Work counters, kept on the host and in the battle probe guest. Shipping guests compile every
// update out, so the counters take no RAM there.
#if !defined(RPG_AVM) || defined(RPG_BATTLE_STATS)
static constexpr bool COUNT_STATS = true;
#else
static constexpr bool COUNT_STATS = false;
#endif
static BattleSearchStats stats;

static BATTLE_TEAM opponent(BATTLE_TEAM team) {
    return team == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
}

// The team to act at a ply. Sides alternate strictly, because every child is one activation.
static BATTLE_TEAM side_at(BATTLE_TEAM root, uint8_t ply) {
    return (ply & 1) ? opponent(root) : root;
}

static uint8_t difference(uint8_t a, uint8_t b) {
    return (uint8_t)(a > b ? a - b : b - a);
}

// How far a distance lies outside an attack range.
static constexpr uint8_t outside(uint8_t distance, uint8_t lo, uint8_t hi) {
    if(distance < lo)
        return (uint8_t)(lo - distance);
    return distance > hi ? (uint8_t)(distance - hi) : 0;
}

// outside() for each unit type's attack range and every distance on the board, built at compile
// time. The gaps below are read once per pair of units in every evaluation and once per move in
// every ordering, so a lookup replaces the comparisons.
struct OutsideTable {
    uint8_t cells[BATTLE_UNIT_TYPE_COUNT][BATTLE_COLUMNS];
    uint8_t slow[BATTLE_UNIT_TYPE_COUNT]; // 1 when a move covers one cell, 0 when it can cover two.
};

static constexpr OutsideTable make_outside() {
    OutsideTable table{};
    for(uint8_t t = 0; t < BATTLE_UNIT_TYPE_COUNT; ++t) {
        BattleUnitRules const& rules = BATTLE_UNIT_RULES[t];
        for(uint8_t d = 0; d < BATTLE_COLUMNS; ++d)
            table.cells[t][d] = outside(d, rules.attack_min, rules.attack_max);
        bool fast = rules.movement_type == BATTLE_MOVEMENT_KNIGHT || rules.move_range > 1;
        table.slow[t] = fast ? 0 : 1;
    }
    return table;
}

// Constant, so on the AVM it sits in program memory and costs no RAM.
static constexpr OutsideTable OUTSIDE = make_outside();

// The attack gap for one row of the outside table: line up on the row and then correct the distance
// along it, or line up on the column and correct along that, whichever is shorter.
static uint8_t table_gap(uint8_t const* out, uint8_t dx, uint8_t dy) {
    uint8_t by_row = (uint8_t)(dy + out[dx]);
    uint8_t by_column = (uint8_t)(dx + out[dy]);
    return by_row < by_column ? by_row : by_column;
}

uint8_t battle_attack_gap(BATTLE_UNIT_TYPE type, uint8_t dx, uint8_t dy) {
    return table_gap(OUTSIDE.cells[type], dx, dy);
}

// The enemy's utility in a position, from the enemy's perspective. The party's score is its exact
// negation, so the search stays zero-sum. Every opposing pair is examined once: its attack gap
// feeds the positional terms, and a pair on one row or column is checked against both units' attack
// ranges, read from the outside table that the rules derive, and for a unit standing between them
// with battle_line_blocked, the same tests battle_can_attack makes.
static int16_t enemy_utility(BattleState const& state) {
    // Bit i is set for each living party unit i, gathered once.
    uint8_t party_mask = 0;
    int16_t utility = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp)
            continue;
        if(unit.team == BATTLE_PARTY) {
            party_mask = (uint8_t)(party_mask | (1 << i));
            utility = (int16_t)(utility - UTILITY_PARTY_UNIT - UTILITY_PARTY_HP * unit.hp);
        } else {
            utility = (int16_t)(utility + UTILITY_ENEMY_UNIT + UTILITY_ENEMY_HP * unit.hp);
        }
    }
    if(!party_mask)
        return utility;

    // Every term is added to the utility as soon as it is known, so the loop keeps one sum and the
    // nearest gap live rather than a counter per term. A strike the side to act makes next is worth
    // more than one the other side makes later.
    uint8_t nearest = 0xFF;
    for(uint8_t e = 0; e < state.unit_count; ++e) {
        BattleUnit const& enemy = state.units[e];
        if(!enemy.hp || enemy.team != BATTLE_ENEMY)
            continue;
        uint8_t const* out = OUTSIDE.cells[enemy.type];
        uint8_t ex = battle_column(enemy.cell), ey = battle_row(enemy.cell);
        uint8_t reach = 0xFF;
        uint8_t heroes = party_mask;
        for(uint8_t k = 0; heroes; ++k, heroes = (uint8_t)(heroes >> 1)) {
            if(!(heroes & 1))
                continue;
            BattleUnit const& hero = state.units[k];
            uint8_t dx = difference(ex, battle_column(hero.cell));
            uint8_t dy = difference(ey, battle_row(hero.cell));
            uint8_t gap = table_gap(out, dx, dy);
            if(gap < reach)
                reach = gap;
            if(dx && dy)
                continue;
            uint8_t distance = (uint8_t)(dx + dy);
            // A distance is in a unit's attack range exactly when it lies zero cells outside it.
            bool enemy_hits = !out[distance];
            bool hero_hits = !OUTSIDE.cells[hero.type][distance];
            if(!(enemy_hits || hero_hits) ||
               (distance > 1 && battle_line_blocked(state, enemy.cell, hero.cell)))
                continue;
            bool enemy_next = state.turn == BATTLE_ENEMY;
            if(enemy_hits)
                utility =
                    (int16_t)(utility + (enemy_next ? UTILITY_STRIKE_NOW : UTILITY_STRIKE_LATER));
            if(hero_hits)
                utility =
                    (int16_t)(utility - (enemy_next ? UTILITY_STRIKE_LATER : UTILITY_STRIKE_NOW));
        }
        // Half moves: a fast unit covers two cells a move, and any other unit one.
        reach = (uint8_t)(reach << OUTSIDE.slow[enemy.type]);
        utility = (int16_t)(utility - UTILITY_GAP * reach);
        if(reach < nearest)
            nearest = reach;
    }
    return (int16_t)(utility - UTILITY_NEAREST * nearest);
}

int16_t battle_score(BattleState const& state, BATTLE_TEAM side, uint8_t ply) {
    if(!state.ended) {
        int16_t utility = enemy_utility(state);
        return side == BATTLE_ENEMY ? utility : (int16_t)-utility;
    }
    // The result names the winner. state.turn is not used, because a battle-ending action leaves it
    // where it was.
    BATTLE_TEAM winner = state.result == BATTLE_VICTORY ? BATTLE_PARTY : BATTLE_ENEMY;
    int16_t magnitude = (int16_t)(BATTLE_SCORE_WIN - ply);
    return winner == side ? magnitude : (int16_t)-magnitude;
}

// The smallest attack gap from a cell to any opponent of `team`, for one row of the outside table.
static uint8_t nearest_gap(BattleState const& state, BATTLE_TEAM team, uint8_t const* out,
                           uint8_t cell) {
    uint8_t x = battle_column(cell), y = battle_row(cell);
    uint8_t nearest = 0xFF;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& foe = state.units[i];
        if(!foe.hp || foe.team == team)
            continue;
        uint8_t gap = table_gap(out, difference(x, battle_column(foe.cell)),
                                difference(y, battle_row(foe.cell)));
        if(gap < nearest)
            nearest = gap;
    }
    return nearest;
}

// Each acting unit's approach target, set once per ordering on the ordering's own frame: the cell
// of the opponent it is closest to attacking. A move approaches when it leaves a smaller gap to
// that opponent than the unit has now, so a move costs two gaps rather than one per opponent.
static void set_targets(BattleState const& state, uint8_t* targets) {
    for(uint8_t u = 0; u < state.unit_count; ++u) {
        BattleUnit const& unit = state.units[u];
        if(!unit.hp || unit.team != state.turn)
            continue;
        uint8_t const* out = OUTSIDE.cells[unit.type];
        uint8_t nearest = 0xFF;
        uint8_t ux = battle_column(unit.cell), uy = battle_row(unit.cell);
        for(uint8_t i = 0; i < state.unit_count; ++i) {
            BattleUnit const& foe = state.units[i];
            if(!foe.hp || foe.team == unit.team)
                continue;
            uint8_t gap = table_gap(out, difference(ux, battle_column(foe.cell)),
                                    difference(uy, battle_row(foe.cell)));
            if(gap < nearest) {
                nearest = gap;
                targets[u] = foe.cell;
            }
        }
    }
}

// The attack gap from one cell to another for a unit's attack range.
static uint8_t gap_between(uint8_t const* out, uint8_t from, uint8_t to) {
    return table_gap(out, difference(battle_column(from), battle_column(to)),
                     difference(battle_row(from), battle_row(to)));
}

// Whether a packed move approaches its unit's target. set_targets must have run for this position.
static bool approaches(BattleState const& state, uint8_t const* targets, uint8_t packed) {
    uint8_t u = (uint8_t)(packed >> 5);
    BattleUnit const& unit = state.units[u];
    uint8_t const* out = OUTSIDE.cells[unit.type];
    uint8_t target = targets[u];
    return gap_between(out, (uint8_t)(packed & (BATTLE_CELLS - 1)), target) <
           gap_between(out, unit.cell, target);
}

// Whether a packed attack removes its victim.
static bool kills(BattleState const& state, uint8_t packed) {
    BattleUnit const& victim =
        state.units[battle_unit_at(state, (uint8_t)(packed & (BATTLE_CELLS - 1)))];
    return victim.hp <= battle_attack_damage(state.units[packed >> 5]);
}

// Whether a packed action that defeats an opponent defeats them all. An attack removes one victim;
// a move's engagement can remove several. Only kills are asked, so the loop runs for few actions;
// it counts here rather than calling battle_engagement, which would add a frame below the search's.
static bool wins(BattleState const& state, uint8_t packed) {
    uint8_t destination = (uint8_t)(packed & (BATTLE_CELLS - 1));
    bool attack = battle_unit_at(state, destination) != BATTLE_NO_UNIT;
    uint8_t opponents = 0, defeated = attack ? 1 : 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp || unit.team == state.turn)
            continue;
        ++opponents;
        if(!attack && unit.hp <= BATTLE_ENGAGEMENT_DAMAGE &&
           battle_cells_adjacent(unit.cell, destination))
            ++defeated;
    }
    return defeated == opponents;
}

// Whether a packed action's destination has its bit set in a set of row masks, bit x of rows[y].
static bool in_rows(uint8_t const* rows, uint8_t packed) {
    uint8_t cell = (uint8_t)(packed & (BATTLE_CELLS - 1));
    return (rows[battle_row(cell)] >> battle_column(cell)) & 1;
}

// Marks the four cells orthogonally adjacent to `cell` in row masks.
static void mark_neighbors(uint8_t* rows, uint8_t cell) {
    uint8_t x = battle_column(cell), y = battle_row(cell);
    uint8_t bit = (uint8_t)(1 << x);
    rows[y] = (uint8_t)(rows[y] | (bit << 1) | (bit >> 1));
    // Bits shifted past column 7 drop out of the byte; a shift below column 0 drops out too.
    if(y)
        rows[y - 1] = (uint8_t)(rows[y - 1] | bit);
    if(y + 1 < BATTLE_ROWS)
        rows[y + 1] = (uint8_t)(rows[y + 1] | bit);
}

// Reverses actions[begin, end).
static void reverse(uint8_t* actions, uint8_t begin, uint8_t end) {
    while(begin + 1 < end) {
        --end;
        uint8_t swap = actions[begin];
        actions[begin] = actions[end];
        actions[end] = swap;
        ++begin;
    }
}

// Moves every action that satisfies `first` ahead of the rest, swapping from both ends. Each action
// is tested once, and the result depends only on the input order, so it is deterministic.
// Returns the index of the first action that does not satisfy `first`.
template <typename PREDICATE>
static uint8_t partition(uint8_t* actions, uint8_t begin, uint8_t end, PREDICATE first) {
    while(begin < end) {
        if(first(actions[begin])) {
            ++begin;
            continue;
        }
        --end;
        uint8_t swap = actions[begin];
        actions[begin] = actions[end];
        actions[end] = swap;
    }
    return begin;
}

// Kept out of line, so its working values never sit on the search's deepest frames.
RPG_NOINLINE uint8_t battle_order_actions(BattleState const& state, uint8_t* actions, uint8_t count,
                                          uint8_t attacks, bool approach) {
    // Two masks of cells beside living opponents: rows[0..3] for any opponent, rows[4..7] for one
    // that a single engagement hit defeats. A move's engagement is then one bit test. The same
    // bytes hold the approach targets afterwards.
    uint8_t rows[2 * BATTLE_ROWS] = {};
    bool fragile = false; // An opponent that one engagement hit defeats.
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp || unit.team == state.turn)
            continue;
        mark_neighbors(rows, unit.cell);
        if(unit.hp <= BATTLE_ENGAGEMENT_DAMAGE) {
            mark_neighbors(rows + BATTLE_ROWS, unit.cell);
            fragile = true;
        }
    }

    // Kills to the front of each group: [attack kills | other attacks][move kills | other moves]. A
    // move can kill only a fragile opponent, so without one the move pass is skipped.
    uint8_t* const killing = rows + BATTLE_ROWS;
    uint8_t attack_kills =
        partition(actions, 0, attacks, [&](uint8_t packed) { return kills(state, packed); });
    uint8_t move_kills = 0;
    if(fragile) {
        move_kills = (uint8_t)(partition(actions, attacks, count,
                                         [&](uint8_t packed) { return in_rows(killing, packed); }) -
                               attacks);
        // Rotate the move kills ahead of the other attacks, so every kill leads.
        reverse(actions, attack_kills, attacks);
        reverse(actions, attacks, (uint8_t)(attacks + move_kills));
        reverse(actions, attack_kills, (uint8_t)(attacks + move_kills));
    }
    uint8_t damaging = (uint8_t)(attack_kills + move_kills);
    partition(actions, 0, damaging, [&](uint8_t packed) { return wins(state, packed); });
    // Then the other attacks, which all damage, then the moves that engage an enemy, then quiet
    // moves.
    uint8_t quiet = (uint8_t)(attacks + move_kills);
    if(!approach)
        return quiet;
    quiet = partition(actions, quiet, count, [&](uint8_t packed) { return in_rows(rows, packed); });
    if(quiet == count || !approach)
        return quiet;
    set_targets(state, rows);
    partition(actions, quiet, count,
              [&](uint8_t packed) { return approaches(state, rows, packed); });
    return quiet;
}

BATTLE_SEARCH_PRIORITY battle_search_priority(BattleState const& state, BattleAction action) {
    switch(battle_action_kind(state, action)) {
        case BATTLE_ACTION_WIN:
            return BATTLE_PRIORITY_WIN;
        case BATTLE_ACTION_KILL:
            return BATTLE_PRIORITY_KILL;
        case BATTLE_ACTION_ATTACK:
            return BATTLE_PRIORITY_ATTACK;
        default:
            break;
    }
    uint8_t targets[BATTLE_UNIT_CAPACITY];
    set_targets(state, targets);
    return approaches(state, targets, battle_pack(action)) ? BATTLE_PRIORITY_APPROACH
                                                           : BATTLE_PRIORITY_OTHER;
}

// Moves `wanted` to index `to` when it appears at or after `to + 1` in the list, keeping the order
// of the actions it passes.
static void promote(uint8_t* actions, uint8_t to, uint8_t count, uint8_t wanted) {
    for(uint8_t i = (uint8_t)(to + 1); i < count; ++i) {
        if(actions[i] != wanted)
            continue;
        for(; i > to; --i)
            actions[i] = actions[i - 1];
        actions[to] = wanted;
        return;
    }
}

// Prepares a node for its children: its legal actions in search order from `base` in the action
// stack, or one pending forced pass when it has none. The root moves `first` to the front; a deeper
// ply moves its killer to the front of its moves. A node whose children are all leaves skips the
// approach ordering of its moves, which costs more than the leaves it saves.
static void start(uint8_t ply, BattleState const& state, uint8_t base, uint8_t depth,
                  uint8_t first) {
    SearchPly& node = plies[ply];
    uint8_t* actions = battle_action_scratch + base;
    uint8_t attacks = 0;
    uint8_t count = battle_actions(state, actions, attacks);
    if(!count) {
        // A side with no legal action has one child, its forced pass, which takes no stack space:
        // next one past end marks it pending.
        node.next = (uint8_t)(base + 1);
        node.end = base;
        return;
    }
    uint8_t quiet = battle_order_actions(state, actions, count, attacks, ply + 1 < depth);
    if(ply == 0)
        promote(actions, 0, count, first);
    else
        promote(actions, quiet, count, killers[ply]);
    node.next = base;
    node.end = (uint8_t)(base + count);
}

// Credits a child's score, already from this node's perspective. Once alpha reaches beta, or the
// child is a win on this node's own action, which nothing can beat, the node lists no more
// children. Returns whether the child cut the node off.
static bool absorb(SearchPly& node, int16_t score, uint8_t ply) {
    if(score > node.alpha)
        node.alpha = score;
    int16_t beta = ply ? (int16_t)-plies[ply - 1].alpha : SCORE_INFINITE;
    if(node.alpha >= beta) {
        node.next = node.end;
        return true;
    }
    if(score >= (int16_t)(BATTLE_SCORE_WIN - ply - 1))
        node.next = node.end;
    return false;
}

static uint16_t saturating_add(uint16_t a, uint16_t b) {
    uint16_t sum = (uint16_t)(a + b);
    return sum < a ? (uint16_t)0xFFFF : sum;
}

// Records a move that cut a node off at `ply` as that ply's killer. Attacks are searched first
// anyway. A killer is promoted only among the next node's quiet moves (see start), so a move that
// engages an enemy is never treated as a quiet move: it stays in its damaging group. The search
// never passes a forced pass here: a node that passes has no other child.
static void note_cutoff(BattleState const& parent, uint8_t ply, uint8_t packed) {
    if(ply && battle_unit_at(parent, (uint8_t)(packed & (BATTLE_CELLS - 1))) == BATTLE_NO_UNIT)
        killers[ply] = packed;
}

// Positions between a budgeted search's progress steps. Zero, for no budget or one below
// BATTLE_AI_PROGRESS_STEPS, reports no progress. One byte holds it for budgets below 2,560.
static uint8_t progress_interval(uint16_t budget) {
    uint16_t interval = (uint16_t)(budget / BATTLE_AI_PROGRESS_STEPS);
    return (uint8_t)(interval > 0xFF ? 0xFF : interval);
}

// Reports one progress step, numbered by the positions visited so far, and returns the countdown
// to the next. Kept out of line, so the search's frame does not grow by the call's work; it runs
// only a few times a second.
RPG_NOINLINE static uint8_t report_progress(uint16_t budget, uint16_t visited) {
    uint8_t interval = progress_interval(budget);
    if(COUNT_STATS)
        ++stats.progress;
    battle_think_progress((uint8_t)(visited / interval));
    return interval;
}

// Negamax with alpha-beta pruning over an explicit stack, so the search never recurses. Each ply
// expands its children in search order. A child that is terminal or at the depth limit is scored
// where it was built and does not occupy a ply. The root keeps the first action that raises its
// alpha, which is the first action in root order with the best score. Returns the positions
// visited, and stores the result in `found` once the tree is finished. Returns `budget` without
// finishing when one more position would exceed it; a budget of zero is unlimited. Inlined into its
// only caller, deepen, so the guest stack carries one frame for both.
static inline __attribute__((always_inline)) uint16_t
search_tree(BattleState const& root, uint8_t depth, uint16_t budget, uint8_t first,
            BattleSearchResult& found, uint16_t whole_budget, uint16_t used, uint8_t& until) {
    uint16_t nodes = 0;
    for(uint8_t i = 0; i < BATTLE_AI_DEPTH; ++i)
        killers[i] = 0; // No killer yet. A legal byte here only reorders, deterministically.
    SearchPly& top = plies[0];
    top.alpha = (int16_t)-SCORE_INFINITE;
    start(0, root, 0, depth, first);
    uint8_t root_child = 0;
    uint8_t root_best = 0;

    // `ply` is the node being expanded. Its children are built in ply_state[ply].
    uint8_t ply = 0;
    for(;;) {
        SearchPly& node = plies[ply];
        // A node is finished when next reaches end, or when its forced pass, pending at end + 1,
        // has been taken, which leaves next at end + 2.
        if(node.next == node.end || node.next == (uint8_t)(node.end + 2)) {
            if(ply == 0) {
                // A root whose list is empty took its forced pass.
                found.action = node.end == 0 ? PASS_ACTION : battle_unpack(root_best);
                found.score = node.alpha;
                found.depth = depth;
                return nodes;
            }
            // The node is finished, so the parent receives its value, negated. The parent's next is
            // one past the action that led here, or past end when that was a pass.
            int16_t score = (int16_t)-node.alpha;
            --ply;
            SearchPly& parent_node = plies[ply];
            uint8_t taken = parent_node.next;
            if(ply == 0 && score > parent_node.alpha)
                root_best = root_child;
            if(absorb(parent_node, score, ply) && taken <= parent_node.end)
                note_cutoff(ply == 0 ? root : ply_state[ply - 1], ply,
                            battle_action_scratch[taken - 1]);
            continue;
        }
        if(nodes == budget && budget) {
            found.depth = 0;
            return nodes;
        }
        ++nodes;
        if(until && !--until)
            until = report_progress(whole_budget, (uint16_t)(used + nodes));

        BattleState const& parent = ply == 0 ? root : ply_state[ply - 1];
        bool pass = node.next > node.end;
        uint8_t packed = 0;
        if(pass)
            node.next = (uint8_t)(node.end + 2);
        else
            packed = battle_action_scratch[node.next++];
        if(ply == 0)
            root_child = packed;
        if(COUNT_STATS && stats.deepest < ply + 1)
            stats.deepest = (uint8_t)(ply + 1);
        BattleState& child = ply_state[ply];
        child = parent;
        if(pass)
            battle_pass(child);
        else
            battle_apply(child, battle_unpack(packed));

        if(ply + 1 < depth && !child.ended) {
            // Descend. The child's window is the parent's window negated and swapped: its alpha
            // starts at the parent's beta negated, which is the grandparent's alpha, and its beta
            // is the parent's alpha negated. Its actions follow the parent's on the action stack.
            SearchPly& next = plies[ply + 1];
            next.alpha = ply ? plies[ply - 1].alpha : (int16_t)-SCORE_INFINITE;
            uint8_t base = node.end;
            ++ply;
            start(ply, child, base, depth, 0);
        } else {
            // Score the leaf from the child's side, and credit the node with the negation.
            if(COUNT_STATS && stats.leaves != 0xFFFF)
                ++stats.leaves;
            int16_t score = (int16_t)-battle_score(child, side_at(root.turn, ply + 1), ply + 1);
            if(ply == 0 && score > node.alpha)
                root_best = packed;
            if(absorb(node, score, ply) && !pass)
                note_cutoff(parent, ply, packed);
        }
    }
}

static uint8_t clamp_depth(uint8_t depth) {
    if(depth < 1)
        return 1;
    return depth > BATTLE_AI_DEPTH ? BATTLE_AI_DEPTH : depth;
}

// A score that only a terminal position reaches. Deeper search cannot change it: the win or loss it
// names is already within the horizon.
static bool decided(int16_t score) {
    return score > BATTLE_SCORE_WIN - 100 || score < -(BATTLE_SCORE_WIN - 100);
}

// Iterative deepening from `shallowest` to `deepest` under a node budget, as battle_think
// describes. `deepest` is lowered to what the action stack holds for the position. battle_search is
// the single iteration at its depth. Kept out of line, so the enemy turn's frame does not carry the
// loop's values.
RPG_NOINLINE static BattleSearchResult deepen(BattleState const& state, uint8_t shallowest,
                                              uint8_t deepest, uint16_t budget) {
    if(COUNT_STATS)
        stats = BattleSearchStats{};
    BattleSearchResult result{PASS_ACTION, 0, 0};
    if(state.ended) {
        result.score = battle_score(state, state.turn, 0);
        return result;
    }
    uint8_t limit = battle_search_depth_limit(state);
    if(deepest > limit)
        deepest = limit;
    if(shallowest > deepest)
        shallowest = deepest;
    // The first progress step comes after one interval, so a quick decision reports none.
    uint8_t until = progress_interval(budget);
    uint16_t used = 0, last = 0;
    uint8_t first = battle_pass_marker(state);
    for(uint8_t depth = shallowest; depth <= deepest; ++depth) {
        uint16_t left = 0;
        if(budget) {
            left = (uint16_t)(budget - used);
            if(!left || (uint32_t)last * ITERATION_GROWTH > left)
                break;
        }
        BattleSearchResult found{PASS_ACTION, 0, 0};
        uint16_t nodes = search_tree(state, depth, left, first, found, budget, used, until);
        used = saturating_add(used, nodes);
        if(COUNT_STATS) {
            stats.iteration_nodes[depth - 1] = nodes;
            stats.nodes = used;
            stats.exhausted = !found.depth;
            if(found.depth)
                stats.completed = depth;
        }
        if(!found.depth)
            break;
        result = found;
        last = nodes;
        first = found.action.unit_index == BATTLE_NO_UNIT ? battle_pass_marker(state)
                                                          : battle_pack(found.action);
        if(decided(found.score))
            break;
    }
    return result;
}

BattleSearchResult battle_search(BattleState const& state, uint8_t depth) {
    depth = clamp_depth(depth);
    return deepen(state, depth, depth, 0);
}

BattleSearchResult battle_think(BattleState const& state, uint8_t max_depth, uint16_t budget) {
    return deepen(state, 1, clamp_depth(max_depth), budget);
}

#if !defined(RPG_AVM)
// Host programs draw nothing; the stats count the calls.
void battle_think_progress(uint8_t) {}
#endif

BattleSearchStats battle_search_stats() {
    return stats;
}

uint16_t battle_search_workspace_bytes() {
    return (uint16_t)(sizeof(plies) + sizeof(killers) + ACTION_STACK_CAPACITY + sizeof(ply_state));
}

int16_t battle_greedy_score(BattleState const& state, BattleAction action) {
    BattleUnit const& actor = state.units[action.unit_index];
    uint8_t opponents =
        battle_living(state, actor.team == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY);
    uint8_t target = battle_unit_at(state, action.destination);
    if(target != BATTLE_NO_UNIT) {
        uint8_t hp = state.units[target].hp, damage = battle_attack_damage(actor);
        if(hp <= damage)
            return opponents == 1 ? GREEDY_WIN : (int16_t)(GREEDY_KILL + GREEDY_HP * hp);
        return (int16_t)(GREEDY_HP * damage);
    }
    BattleEngagement engagement = battle_engagement(state, action.unit_index, action.destination);
    if(engagement.kills && engagement.kills == opponents)
        return GREEDY_WIN; // A winning sacrifice still wins.
    bool falls = actor.hp <= BATTLE_MOVE_COST;
    if(falls && battle_living(state, actor.team) == 1)
        return GREEDY_LOSS; // The side's last unit would fall without winning.
    int16_t value =
        (int16_t)(GREEDY_KILL * engagement.kills +
                  GREEDY_HP * (engagement.hits - BATTLE_MOVE_COST) - (falls ? GREEDY_KILL : 0));
    return (int16_t)(value -
                     nearest_gap(state, actor.team, OUTSIDE.cells[actor.type], action.destination));
}

// Kept out of line, so the enemy turn's frame does not carry the greedy fallback's values.
RPG_NOINLINE BattleAction battle_greedy_action(BattleState const& state) {
    BattleAction best{BATTLE_NO_UNIT, 0};
    int16_t best_score = INT16_MIN;
    // The search is not running, so the shared scratch is free.
    uint8_t* actions = battle_action_scratch;
    uint8_t attacks = 0;
    uint8_t count = battle_actions(state, actions, attacks);
    for(uint8_t i = 0; i < count; ++i) {
        // A strictly greater score keeps the first candidate in fixed order on a tie.
        BattleAction action = battle_unpack(actions[i]);
        int16_t score = battle_greedy_score(state, action);
        if(score > best_score) {
            best = action;
            best_score = score;
        }
    }
    return best;
}

// Kept out of line, so its loop stays off the enemy turn's frame.
RPG_NOINLINE uint16_t battle_ai_budget(BattleState const& state) {
    uint8_t living = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i)
        living = (uint8_t)(living + (state.units[i].hp != 0));
    if(living <= 4)
        return BATTLE_AI_NODE_BUDGET;
    return (uint16_t)(BATTLE_AI_NODE_BUDGET * 4 / living);
}

BattleAction battle_enemy_action(BattleState const& state) {
    BattleSearchResult result = battle_think(state, BATTLE_AI_DEPTH, battle_ai_budget(state));
    if(result.depth && battle_legal(state, result.action))
        return result.action;
    return battle_greedy_action(state);
}
