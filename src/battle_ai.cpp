#include "battle_ai.h"

// Enemy utility weights, in whole points. The enemy values the damage it deals above the damage it
// takes, and it values eliminations above ordinary HP. Distance and strike terms steer it toward the
// party. The utility counts living units and HP as differences, so constant offsets never matter.
static constexpr int16_t UTILITY_ENEMY_UNIT = 550;
static constexpr int16_t UTILITY_PARTY_UNIT = 600;
static constexpr int16_t UTILITY_PARTY_HP = 180;
static constexpr int16_t UTILITY_ENEMY_HP = 90;
// Per cell of distance from each enemy to its nearest party member, and again for the nearest enemy.
static constexpr int16_t UTILITY_DISTANCE = 20;
static constexpr int16_t UTILITY_NEAREST = 20;
// A strike that the side to act can make now, and one that the other side makes after one activation.
// Mutual engagement is worth more when the enemy acts next, and less when the party acts next.
static constexpr int16_t UTILITY_STRIKE_NOW = 120;
static constexpr int16_t UTILITY_STRIKE_LATER = 60;

// Windows start beyond every score the search produces.
static constexpr int16_t SCORE_INFINITE = 32000;

// Scores are ordered by band. Each band is wider than any distance a move can score inside it.
static constexpr int16_t SCORE_KILL = 3000, SCORE_ATTACK = 2000, SCORE_MOVE = 1000;

// A ply's child list is walked through its priority masks, then a forced pass, then nothing.
static constexpr uint8_t STAGE_DONE = BATTLE_PRIORITY_COUNT + 1;
static constexpr uint8_t SLOT_PASS = 16;
static constexpr uint8_t SLOT_NONE = 17;
static constexpr BattleAction PASS_ACTION{BATTLE_NO_UNIT, 0};

// Candidate slots are unit * 4 + direction, so four units in four directions fit in one 16-bit mask.
static_assert(BATTLE_UNIT_CAPACITY * 4 <= 16, "candidate slots must fit in one 16-bit mask");

// The explicit search stack. Each ply keeps only the bookkeeping it needs to resume: its window, the
// best child so far, and the priority masks of its legal actions, with bits cleared as children are
// taken. The positions are kept separately, so the root reads the caller's state and never copies it.
struct SearchPly {
    int16_t alpha, beta, best;
    uint16_t classes[BATTLE_PRIORITY_COUNT];
    uint8_t stage;
    bool pass_pending;
};

static SearchPly plies[BATTLE_AI_DEPTH];
// Positions for plies 1 through BATTLE_AI_DEPTH - 1, and one scratch position for leaves.
static BattleState ply_state[BATTLE_AI_DEPTH > 1 ? BATTLE_AI_DEPTH - 1 : 1];
static BattleState leaf_state;
static BattleSearchStats stats;

static BATTLE_TEAM opponent(BATTLE_TEAM team) {
    return team == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
}

// The team to act at a ply. Sides alternate strictly, because every child is one activation.
static BATTLE_TEAM side_at(BATTLE_TEAM root, uint8_t ply) {
    return (ply & 1) ? opponent(root) : root;
}

// Manhattan distance between two cells. Terrain does not block movement yet.
static int16_t cell_distance(uint8_t a, uint8_t b) {
    int16_t dx = (int16_t)battle_column(a) - battle_column(b);
    int16_t dy = (int16_t)battle_row(a) - battle_row(b);
    if(dx < 0)
        dx = (int16_t)-dx;
    if(dy < 0)
        dy = (int16_t)-dy;
    return (int16_t)(dx + dy);
}

// The living cells of one side, gathered once per node for the ordering and the utility.
struct LivingCells {
    uint8_t cell[BATTLE_UNIT_CAPACITY];
    uint8_t count;
};

static LivingCells living_cells(BattleState const& state, BATTLE_TEAM team) {
    LivingCells list{};
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp && unit.team == team)
            list.cell[list.count++] = unit.cell;
    }
    return list;
}

// The distance from a cell to the nearest listed cell. Returns 99 for an empty list.
static int16_t nearest_listed(LivingCells const& list, uint8_t cell) {
    int16_t nearest = 99;
    for(uint8_t i = 0; i < list.count; ++i) {
        int16_t distance = cell_distance(cell, list.cell[i]);
        if(distance < nearest)
            nearest = distance;
    }
    return nearest;
}

// The living units of one team, by index and cell. The utility decodes columns and rows as it measures.
struct LivingUnits {
    uint8_t index[BATTLE_UNIT_CAPACITY];
    uint8_t cell[BATTLE_UNIT_CAPACITY];
    uint8_t count;
};

// The enemy's utility in a position, from the enemy's perspective. The party's score is its exact
// negation, so the search stays zero-sum.
static int16_t enemy_utility(BattleState const& state) {
    // One pass over the units collects both sides' living units and their HP.
    LivingUnits enemy_list{}, party_list{};
    int16_t enemy_hp = 0, party_hp = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp)
            continue;
        LivingUnits& list = unit.team == BATTLE_ENEMY ? enemy_list : party_list;
        uint8_t slot = list.count++;
        list.index[slot] = i;
        list.cell[slot] = unit.cell;
        if(unit.team == BATTLE_ENEMY)
            enemy_hp = (int16_t)(enemy_hp + unit.hp);
        else
            party_hp = (int16_t)(party_hp + unit.hp);
    }
    uint8_t enemies = enemy_list.count, party = party_list.count;

    // One pass over the enemy and party pairs gives each enemy's distance to its nearest party member, and
    // finds the adjacent pairs that can strike. Strikes are checked only for adjacent pairs.
    uint8_t distance_sum = 0, nearest = 2 * BATTLE_COLUMNS;
    uint8_t enemy_strikes = 0, party_strikes = 0;
    for(uint8_t a = 0; a < enemies; ++a) {
        uint8_t e = enemy_list.index[a];
        uint8_t reach = 2 * BATTLE_COLUMNS;
        for(uint8_t b = 0; b < party; ++b) {
            uint8_t p = party_list.index[b];
            int16_t dx = (int16_t)(battle_column(enemy_list.cell[a]) - battle_column(party_list.cell[b]));
            int16_t dy = (int16_t)(battle_row(enemy_list.cell[a]) - battle_row(party_list.cell[b]));
            if(dx < 0)
                dx = (int16_t)-dx;
            if(dy < 0)
                dy = (int16_t)-dy;
            int16_t distance = (int16_t)(dx + dy);
            if(distance < reach)
                reach = (uint8_t)distance;
            if(distance == 1) {
                if(battle_can_strike(state, e, p))
                    ++enemy_strikes;
                if(battle_can_strike(state, p, e))
                    ++party_strikes;
            }
        }
        distance_sum = (uint8_t)(distance_sum + reach);
        if(reach < nearest)
            nearest = reach;
    }

    int16_t utility = (int16_t)(UTILITY_ENEMY_UNIT * enemies - UTILITY_PARTY_UNIT * party -
                                UTILITY_PARTY_HP * party_hp + UTILITY_ENEMY_HP * enemy_hp -
                                UTILITY_DISTANCE * distance_sum - UTILITY_NEAREST * nearest);
    // A strike the side to act makes next is worth more than one the other side makes later.
    int16_t strikes = state.turn == BATTLE_ENEMY
                          ? (int16_t)(UTILITY_STRIKE_NOW * enemy_strikes -
                                      UTILITY_STRIKE_LATER * party_strikes)
                          : (int16_t)(UTILITY_STRIKE_LATER * enemy_strikes -
                                      UTILITY_STRIKE_NOW * party_strikes);
    return (int16_t)(utility + strikes);
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

// The priority of an action, given the opponents of its mover.
static BATTLE_SEARCH_PRIORITY priority_against(BattleState const& state, BattleAction action,
                                               LivingCells const& foes) {
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
    // A move closes the distance to the nearest opponent, or it does not.
    int16_t before = nearest_listed(foes, state.units[action.unit_index].cell);
    int16_t after = nearest_listed(foes, action.destination);
    return after < before ? BATTLE_PRIORITY_APPROACH : BATTLE_PRIORITY_OTHER;
}

BATTLE_SEARCH_PRIORITY battle_search_priority(BattleState const& state, BattleAction action) {
    BATTLE_TEAM mover = state.units[action.unit_index].team;
    return priority_against(state, action, living_cells(state, opponent(mover)));
}

// The bit position of a single set bit in a 16-bit mask, found by a short binary search.
static uint8_t bit_index(uint16_t bit) {
    uint8_t index = 0;
    if(bit & 0xFF00) {
        index += 8;
        bit >>= 8;
    }
    if(bit & 0x00F0) {
        index += 4;
        bit >>= 4;
    }
    if(bit & 0x000C) {
        index += 2;
        bit >>= 2;
    }
    if(bit & 0x0002)
        index += 1;
    return index;
}

// The action a candidate slot names. Only slots holding a legal action are read, so the neighbor exists.
static BattleAction slot_action(BattleState const& state, uint8_t slot) {
    uint8_t unit = (uint8_t)(slot >> 2);
    uint8_t destination = 0;
    battle_neighbor(state.units[unit].cell, (DIRECTION)(slot & 3), destination);
    return BattleAction{unit, destination};
}

// The distance from a column and row to the nearest opponent, given the opponents' columns and rows.
static int16_t nearest_offset(uint8_t const* columns, uint8_t const* rows, uint8_t count, int16_t x,
                              int16_t y) {
    int16_t nearest = 99;
    for(uint8_t f = 0; f < count; ++f) {
        int16_t dx = (int16_t)(x - columns[f]);
        int16_t dy = (int16_t)(y - rows[f]);
        if(dx < 0)
            dx = (int16_t)-dx;
        if(dy < 0)
            dy = (int16_t)-dy;
        if(dx + dy < nearest)
            nearest = (int16_t)(dx + dy);
    }
    return nearest;
}

// Sorts a position's legal actions into the priority masks, one bit per candidate slot. The rules give
// the wins, kills, and attacks in one pass. Each move is then placed by its distance to the opponents.
// Returns whether any action is legal.
static bool classify(BattleState const& state, SearchPly& node) {
    BattleActionMasks kinds = battle_action_masks(state);
    node.classes[BATTLE_PRIORITY_WIN] = kinds.kind[BATTLE_ACTION_WIN];
    node.classes[BATTLE_PRIORITY_KILL] = kinds.kind[BATTLE_ACTION_KILL];
    node.classes[BATTLE_PRIORITY_ATTACK] = kinds.kind[BATTLE_ACTION_ATTACK];
    node.classes[BATTLE_PRIORITY_APPROACH] = 0;
    node.classes[BATTLE_PRIORITY_OTHER] = 0;

    // Each unit's distance to the nearest opponent is taken once, and each move is then compared with it.
    uint16_t moves = kinds.kind[BATTLE_ACTION_MOVE];
    if(moves) {
        // The opponents' columns and rows are read once per node. Each move is measured from its mover's cell
        // and its one-step offset, against the same opponents.
        uint8_t foe_column[BATTLE_UNIT_CAPACITY], foe_row[BATTLE_UNIT_CAPACITY];
        uint8_t foes = 0;
        for(uint8_t i = 0; i < state.unit_count; ++i) {
            BattleUnit const& unit = state.units[i];
            if(unit.hp && unit.team != state.turn) {
                foe_column[foes] = battle_column(unit.cell);
                foe_row[foes] = battle_row(unit.cell);
                ++foes;
            }
        }
        while(moves) {
            uint16_t bit = (uint16_t)(moves & (uint16_t)-moves);
            moves = (uint16_t)(moves ^ bit);
            uint8_t slot = bit_index(bit);
            BattleUnit const& mover = state.units[slot >> 2];
            int16_t x = battle_column(mover.cell), y = battle_row(mover.cell);
            int16_t before = nearest_offset(foe_column, foe_row, foes, x, y);
            switch(slot & 3) {
                case NORTH:
                    --y;
                    break;
                case EAST:
                    ++x;
                    break;
                case SOUTH:
                    ++y;
                    break;
                default:
                    --x;
                    break;
            }
            int16_t after = nearest_offset(foe_column, foe_row, foes, x, y);
            uint8_t priority = after < before ? BATTLE_PRIORITY_APPROACH : BATTLE_PRIORITY_OTHER;
            node.classes[priority] = (uint16_t)(node.classes[priority] | bit);
        }
    }
    uint16_t all = (uint16_t)(kinds.kind[BATTLE_ACTION_WIN] | kinds.kind[BATTLE_ACTION_KILL] |
                              kinds.kind[BATTLE_ACTION_ATTACK] | kinds.kind[BATTLE_ACTION_MOVE]);
    return all != 0;
}

// Prepares a node for its children. A side with no legal action gets one forced pass.
static void start(SearchPly& node, BattleState const& state) {
    node.pass_pending = !classify(state, node);
    node.stage = 0;
}

// Yields the next child of a node as a candidate slot, SLOT_PASS, or SLOT_NONE once the node has no
// children left. Each call takes the lowest set bit of the current mask, so no candidate is visited twice.
static uint8_t next_child(SearchPly& node) {
    while(node.stage < BATTLE_PRIORITY_COUNT) {
        uint16_t mask = node.classes[node.stage];
        if(mask) {
            uint16_t bit = (uint16_t)(mask & (uint16_t)-mask);
            node.classes[node.stage] = (uint16_t)(mask ^ bit);
            return bit_index(bit);
        }
        ++node.stage;
    }
    node.stage = STAGE_DONE;
    if(node.pass_pending) {
        node.pass_pending = false;
        return SLOT_PASS;
    }
    return SLOT_NONE;
}

// Credits a child's score, already from this node's perspective. Once alpha reaches beta, the node is
// cut off and lists no more children.
static void absorb(SearchPly& node, int16_t score) {
    if(score > node.best)
        node.best = score;
    if(score > node.alpha)
        node.alpha = score;
    if(node.alpha >= node.beta)
        node.stage = STAGE_DONE;
}

// Negamax with alpha-beta pruning over an explicit stack, so the search never recurses. Each ply expands
// its children in priority order. A child that is terminal or at the depth limit is scored in the scratch
// position and does not occupy a ply. The root keeps the first action that reaches its best score.
RPG_NOINLINE BattleSearchResult battle_search(BattleState const& root, uint8_t depth) {
    stats = BattleSearchStats{};
    if(depth < 1)
        depth = 1;
    if(depth > BATTLE_AI_DEPTH)
        depth = BATTLE_AI_DEPTH;
    BattleSearchResult result{PASS_ACTION, 0};
    if(root.ended) {
        result.score = battle_score(root, root.turn, 0);
        return result;
    }

    SearchPly& top = plies[0];
    top.alpha = (int16_t)-SCORE_INFINITE;
    top.beta = SCORE_INFINITE;
    top.best = (int16_t)-SCORE_INFINITE;
    start(top, root);
    BattleAction root_child = PASS_ACTION;
    BattleAction root_best = PASS_ACTION;

    // `ply` is the node being expanded. Its child is built in the next position, or in the scratch
    // position when the child is a leaf.
    uint8_t ply = 0;
    for(;;) {
        BattleState const& parent = ply == 0 ? root : ply_state[ply - 1];
        SearchPly& node = plies[ply];
        uint8_t slot = next_child(node);
        if(slot == SLOT_NONE) {
            if(ply == 0) {
                result.action = root_best;
                result.score = node.best;
                return result;
            }
            // The node is finished, so the parent receives its best score, negated.
            int16_t score = (int16_t)-node.best;
            --ply;
            if(ply == 0 && score > plies[0].best)
                root_best = root_child;
            absorb(plies[ply], score);
            continue;
        }

        BattleAction action = slot == SLOT_PASS ? PASS_ACTION : slot_action(parent, slot);
        if(ply == 0)
            root_child = action;
        if(stats.nodes != 0xFFFF)
            ++stats.nodes;
        if(stats.deepest < ply + 1)
            stats.deepest = (uint8_t)(ply + 1);
        bool internal = ply + 1 < depth;
        BattleState& child = internal ? ply_state[ply] : leaf_state;
        child = parent;
        if(slot == SLOT_PASS)
            battle_pass(child);
        else
            battle_apply(child, action);

        if(internal && !child.ended) {
            // Descend. The child's window is the parent's window negated and swapped.
            SearchPly& next = plies[ply + 1];
            next.alpha = (int16_t)-node.beta;
            next.beta = (int16_t)-node.alpha;
            next.best = (int16_t)-SCORE_INFINITE;
            start(next, child);
            ++ply;
        } else {
            // Score the leaf from the child's side, and credit the node with the negation.
            if(stats.leaves != 0xFFFF)
                ++stats.leaves;
            int16_t score = (int16_t)-battle_score(child, side_at(root.turn, ply + 1), ply + 1);
            if(ply == 0 && score > node.best)
                root_best = action;
            absorb(node, score);
        }
    }
}

BattleSearchStats battle_search_stats() {
    return stats;
}

uint16_t battle_search_workspace_bytes() {
    return (uint16_t)(sizeof(plies) + sizeof(ply_state) + sizeof(leaf_state));
}

int16_t battle_greedy_score(BattleState const& state, BattleAction action) {
    uint8_t target = battle_unit_at(state, action.destination);
    if(target != BATTLE_NO_UNIT) {
        uint8_t damage = battle_attack_damage(state.units[action.unit_index]);
        return state.units[target].hp <= damage ? SCORE_KILL : SCORE_ATTACK;
    }
    LivingCells foes = living_cells(state, opponent(state.turn));
    return (int16_t)(SCORE_MOVE - nearest_listed(foes, action.destination));
}

BattleAction battle_greedy_action(BattleState const& state) {
    BattleAction best{BATTLE_NO_UNIT, 0};
    int16_t best_score = -1;
    BattleActionCursor cursor = battle_actions(state);
    BattleAction action{};
    while(battle_next_action(state, cursor, action)) {
        // A strictly greater score keeps the first candidate in fixed order on a tie.
        int16_t score = battle_greedy_score(state, action);
        if(score > best_score) {
            best = action;
            best_score = score;
        }
    }
    return best;
}

BattleAction battle_enemy_action(BattleState const& state) {
    BattleAction action = battle_search(state, BATTLE_AI_DEPTH).action;
    if(battle_legal(state, action))
        return action;
    return battle_greedy_action(state);
}
