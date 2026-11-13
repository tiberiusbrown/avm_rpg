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

// Scores are ordered by band. Each band is wider than any gap a move can score inside it.
static constexpr int16_t SCORE_KILL = 3000, SCORE_ATTACK = 2000, SCORE_MOVE = 1000;

// An iteration is started only when the budget left is at least this many times the nodes of the
// last completed iteration. A deeper iteration that cannot finish would only spend time and be
// discarded.
static constexpr uint8_t ITERATION_GROWTH = 2;

static constexpr BattleAction PASS_ACTION{BATTLE_NO_UNIT, 0};

// The explicit search stack. Each ply keeps its window and the next and end indices of its ordered
// actions in the shared action stack. The search fails hard: a node's value is its alpha, clamped
// to its window, which picks the same root action and score as an exact search and needs no
// separate best score. Positions are kept separately, so the root reads the caller's state and
// never copies it.
struct SearchPly {
    int16_t alpha, beta;
    uint8_t next, end;
};

// The most actions one side can have: each unit's moves and one attack per opponent.
static constexpr uint8_t side_actions(uint8_t actors, uint8_t opponents) {
    return (uint8_t)(actors * BATTLE_MOVE_LIMIT + actors * opponents);
}

// The plies on the stack list their actions one after another, each starting where its parent's
// list ends. Sides alternate and never gain units, so the lists of a full stack are bounded by the
// largest alternating total over every split of the four slots. A side with no living opponent has
// its one list at the root only: every child of that root has ended.
static constexpr uint16_t action_stack_capacity() {
    uint16_t most = BATTLE_ACTION_CAPACITY;
    for(uint8_t actors = 1; actors < BATTLE_UNIT_CAPACITY; ++actors)
        for(uint8_t opponents = 1; actors + opponents <= BATTLE_UNIT_CAPACITY; ++opponents) {
            uint16_t total = 0;
            for(uint8_t ply = 0; ply < BATTLE_AI_DEPTH; ++ply)
                total = (uint16_t)(total + ((ply & 1) ? side_actions(opponents, actors)
                                                      : side_actions(actors, opponents)));
            if(total > most)
                most = total;
        }
    return most;
}

static constexpr uint16_t ACTION_STACK_CAPACITY = action_stack_capacity();
static_assert(ACTION_STACK_CAPACITY <= 255, "action stack indices are bytes");

static SearchPly plies[BATTLE_AI_DEPTH];
static uint8_t action_stack[ACTION_STACK_CAPACITY];
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

    int16_t gaps = 0;
    uint8_t nearest = 0xFF;
    int8_t strikes_enemy = 0, strikes_party = 0;
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
            if((enemy_hits || hero_hits) && distance > 1 &&
               battle_line_blocked(state, enemy.cell, hero.cell))
                continue;
            strikes_enemy = (int8_t)(strikes_enemy + enemy_hits);
            strikes_party = (int8_t)(strikes_party + hero_hits);
        }
        // Half moves: a fast unit covers two cells a move, and any other unit one.
        reach = (uint8_t)(reach << OUTSIDE.slow[enemy.type]);
        gaps = (int16_t)(gaps + reach);
        if(reach < nearest)
            nearest = reach;
    }

    utility = (int16_t)(utility - UTILITY_GAP * gaps - UTILITY_NEAREST * nearest);
    // A strike the side to act makes next is worth more than one the other side makes later.
    if(state.turn == BATTLE_ENEMY)
        return (int16_t)(utility + UTILITY_STRIKE_NOW * strikes_enemy -
                         UTILITY_STRIKE_LATER * strikes_party);
    return (int16_t)(utility + UTILITY_STRIKE_LATER * strikes_enemy -
                     UTILITY_STRIKE_NOW * strikes_party);
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

// Each acting unit's approach target, set once per ordering on the ordering's own frame: its row of
// the outside table, the cell of the opponent it is closest to attacking, and its gap to that
// opponent now. A move approaches when it leaves a smaller gap to that same opponent, so a move
// costs one gap rather than one per opponent.
struct ApproachTarget {
    uint8_t const* out;
    uint8_t x, y, gap;
};

static void set_targets(BattleState const& state, ApproachTarget* targets) {
    for(uint8_t u = 0; u < state.unit_count; ++u) {
        BattleUnit const& unit = state.units[u];
        if(!unit.hp || unit.team != state.turn)
            continue;
        ApproachTarget& target = targets[u];
        target.out = OUTSIDE.cells[unit.type];
        target.gap = 0xFF;
        uint8_t ux = battle_column(unit.cell), uy = battle_row(unit.cell);
        for(uint8_t i = 0; i < state.unit_count; ++i) {
            BattleUnit const& foe = state.units[i];
            if(!foe.hp || foe.team == unit.team)
                continue;
            uint8_t fx = battle_column(foe.cell), fy = battle_row(foe.cell);
            uint8_t gap = table_gap(target.out, difference(ux, fx), difference(uy, fy));
            if(gap < target.gap) {
                target.gap = gap;
                target.x = fx;
                target.y = fy;
            }
        }
    }
}

// Whether a packed move approaches its unit's target. set_targets must have run for this position.
static bool approaches(ApproachTarget const* targets, uint8_t packed) {
    ApproachTarget const& target = targets[packed >> 5];
    uint8_t cell = (uint8_t)(packed & (BATTLE_CELLS - 1));
    uint8_t gap = table_gap(target.out, difference(battle_column(cell), target.x),
                            difference(battle_row(cell), target.y));
    return gap < target.gap;
}

// Whether a packed attack removes its victim.
static bool kills(BattleState const& state, uint8_t packed) {
    BattleUnit const& victim =
        state.units[battle_unit_at(state, (uint8_t)(packed & (BATTLE_CELLS - 1)))];
    return victim.hp <= battle_attack_damage(state.units[packed >> 5]);
}

// Moves every action that satisfies `first` ahead of the rest, swapping from both ends. Each action
// is tested once, and the result depends only on the input order, so it is deterministic.
template <typename PREDICATE>
static void partition(uint8_t* actions, uint8_t begin, uint8_t end, PREDICATE first) {
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
}

// Kept out of line, so its working values never sit on the search's deepest frames.
RPG_NOINLINE void battle_order_actions(BattleState const& state, uint8_t* actions, uint8_t count,
                                       uint8_t attacks, bool approach) {
    partition(actions, 0, attacks, [&](uint8_t packed) { return kills(state, packed); });
    if(count == attacks || !approach)
        return;
    ApproachTarget targets[BATTLE_UNIT_CAPACITY];
    set_targets(state, targets);
    partition(actions, attacks, count, [&](uint8_t packed) { return approaches(targets, packed); });
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
    ApproachTarget targets[BATTLE_UNIT_CAPACITY];
    set_targets(state, targets);
    return approaches(targets, battle_pack(action)) ? BATTLE_PRIORITY_APPROACH
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
// stack, or one forced pass when it has none. The root moves `first` to the front; a deeper ply
// moves its killer to the front of its moves. A node whose children are all leaves skips the
// approach ordering of its moves, which costs more than the leaves it saves.
static void start(uint8_t ply, BattleState const& state, uint8_t base, uint8_t depth,
                  uint8_t first) {
    SearchPly& node = plies[ply];
    uint8_t* actions = action_stack + base;
    uint8_t attacks = 0;
    uint8_t count = battle_actions(state, actions, attacks);
    if(!count) {
        actions[0] = BATTLE_PACKED_PASS;
        count = 1;
    } else {
        battle_order_actions(state, actions, count, attacks, ply + 1 < depth);
        if(ply == 0)
            promote(actions, 0, count, first);
        else
            promote(actions, attacks, count, killers[ply]);
    }
    node.next = base;
    node.end = (uint8_t)(base + count);
}

// Credits a child's score, already from this node's perspective. Once alpha reaches beta, or the
// child is a win on this node's own action, which nothing can beat, the node lists no more
// children. Returns whether the child cut the node off.
static bool absorb(SearchPly& node, int16_t score, uint8_t ply) {
    if(score > node.alpha)
        node.alpha = score;
    if(node.alpha >= node.beta) {
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

// Records a move that cut a node off at `ply` as that ply's killer. Attacks are already searched
// first.
static void note_cutoff(BattleState const& parent, uint8_t ply, uint8_t packed) {
    if(ply && packed != BATTLE_PACKED_PASS &&
       battle_unit_at(parent, (uint8_t)(packed & (BATTLE_CELLS - 1))) == BATTLE_NO_UNIT)
        killers[ply] = packed;
}

// Negamax with alpha-beta pruning over an explicit stack, so the search never recurses. Each ply
// expands its children in search order. A child that is terminal or at the depth limit is scored
// where it was built and does not occupy a ply. The root keeps the first action that raises its
// alpha, which is the first action in root order with the best score. Returns the positions
// visited, and stores the result in `found` once the tree is finished. Returns `budget` without
// finishing when one more position would exceed it; a budget of zero is unlimited. Inlined into its
// only caller, deepen, so the guest stack carries one frame for both.
static inline __attribute__((always_inline)) uint16_t search_tree(BattleState const& root,
                                                                  uint8_t depth, uint16_t budget,
                                                                  uint8_t first,
                                                                  BattleSearchResult& found) {
    uint16_t nodes = 0;
    for(uint8_t i = 0; i < BATTLE_AI_DEPTH; ++i)
        killers[i] = BATTLE_PACKED_PASS;
    SearchPly& top = plies[0];
    top.alpha = (int16_t)-SCORE_INFINITE;
    top.beta = SCORE_INFINITE;
    start(0, root, 0, depth, first);
    uint8_t root_child = BATTLE_PACKED_PASS;
    uint8_t root_best = BATTLE_PACKED_PASS;

    // `ply` is the node being expanded. Its children are built in ply_state[ply].
    uint8_t ply = 0;
    for(;;) {
        SearchPly& node = plies[ply];
        if(node.next == node.end) {
            if(ply == 0) {
                found.action =
                    root_best == BATTLE_PACKED_PASS ? PASS_ACTION : battle_unpack(root_best);
                found.score = node.alpha;
                found.depth = depth;
                return nodes;
            }
            // The node is finished, so the parent receives its value, negated.
            int16_t score = (int16_t)-node.alpha;
            --ply;
            SearchPly& parent_node = plies[ply];
            if(ply == 0 && score > parent_node.alpha)
                root_best = root_child;
            if(absorb(parent_node, score, ply))
                note_cutoff(ply == 0 ? root : ply_state[ply - 1], ply,
                            action_stack[parent_node.next - 1]);
            continue;
        }
        if(nodes == budget && budget) {
            found.depth = 0;
            return nodes;
        }
        ++nodes;

        BattleState const& parent = ply == 0 ? root : ply_state[ply - 1];
        uint8_t packed = action_stack[node.next++];
        if(ply == 0)
            root_child = packed;
        if(COUNT_STATS && stats.deepest < ply + 1)
            stats.deepest = (uint8_t)(ply + 1);
        BattleState& child = ply_state[ply];
        child = parent;
        if(packed == BATTLE_PACKED_PASS)
            battle_pass(child);
        else
            battle_apply(child, battle_unpack(packed));

        if(ply + 1 < depth && !child.ended) {
            // Descend. The child's window is the parent's window negated and swapped, and its
            // actions follow the parent's on the action stack.
            SearchPly& next = plies[ply + 1];
            next.alpha = (int16_t)-node.beta;
            next.beta = (int16_t)-node.alpha;
            uint8_t base = node.end;
            ++ply;
            start(ply, child, base, depth, BATTLE_PACKED_PASS);
        } else {
            // Score the leaf from the child's side, and credit the node with the negation.
            if(COUNT_STATS && stats.leaves != 0xFFFF)
                ++stats.leaves;
            int16_t score = (int16_t)-battle_score(child, side_at(root.turn, ply + 1), ply + 1);
            if(ply == 0 && score > node.alpha)
                root_best = packed;
            if(absorb(node, score, ply))
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
// describes. battle_search is the single iteration at its depth. Kept out of line, so the enemy
// turn's frame does not carry the loop's values.
RPG_NOINLINE static BattleSearchResult deepen(BattleState const& state, uint8_t shallowest,
                                              uint8_t deepest, uint16_t budget) {
    if(COUNT_STATS)
        stats = BattleSearchStats{};
    BattleSearchResult result{PASS_ACTION, 0, 0};
    if(state.ended) {
        result.score = battle_score(state, state.turn, 0);
        return result;
    }
    uint16_t used = 0, last = 0;
    uint8_t first = BATTLE_PACKED_PASS;
    for(uint8_t depth = shallowest; depth <= deepest; ++depth) {
        uint16_t left = 0;
        if(budget) {
            left = (uint16_t)(budget - used);
            if(!left || (uint32_t)last * ITERATION_GROWTH > left)
                break;
        }
        BattleSearchResult found{PASS_ACTION, 0, 0};
        uint16_t nodes = search_tree(state, depth, left, first, found);
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
        first = found.action.unit_index == BATTLE_NO_UNIT ? BATTLE_PACKED_PASS
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

BattleSearchStats battle_search_stats() {
    return stats;
}

uint16_t battle_search_workspace_bytes() {
    return (uint16_t)(sizeof(plies) + sizeof(killers) + sizeof(action_stack) + sizeof(ply_state));
}

int16_t battle_greedy_score(BattleState const& state, BattleAction action) {
    uint8_t target = battle_unit_at(state, action.destination);
    BattleUnit const& actor = state.units[action.unit_index];
    if(target != BATTLE_NO_UNIT)
        return state.units[target].hp <= battle_attack_damage(actor) ? SCORE_KILL : SCORE_ATTACK;
    return (int16_t)(SCORE_MOVE -
                     nearest_gap(state, actor.team, OUTSIDE.cells[actor.type], action.destination));
}

// Kept out of line, so the enemy turn's frame does not carry the greedy fallback's values.
RPG_NOINLINE BattleAction battle_greedy_action(BattleState const& state) {
    BattleAction best{BATTLE_NO_UNIT, 0};
    int16_t best_score = -1;
    // The search is not running, so the action stack serves as scratch.
    uint8_t* actions = action_stack;
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

BattleAction battle_enemy_action(BattleState const& state) {
    BattleSearchResult result = battle_think(state, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET);
    if(result.depth && battle_legal(state, result.action))
        return result.action;
    return battle_greedy_action(state);
}
