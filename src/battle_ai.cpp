#include "battle_ai.h"

// Static evaluation weights, from one side's perspective. A unit outweighs all the HP it could
// hold, so losing one is worse than minor damage. A four-unit board keeps every static score within
// about
// +-4000, far below BATTLE_SCORE_WIN.
static constexpr int16_t EVAL_UNIT = 1000;       // Per living unit ahead of the opponent.
static constexpr int16_t EVAL_HP = 50;           // Per HP, for each unit.
static constexpr int16_t EVAL_KILL_THREAT = 300; // Per adjacent enemy this side can kill.
static constexpr int16_t EVAL_ATTACK_THREAT =
    100; // Per adjacent enemy this side can hit without killing.
static constexpr int16_t EVAL_DISTANCE = 10; // Per cell of distance to the nearest foe, per unit.

// Scores above and below every search score. Windows start at these bounds.
static constexpr int16_t SCORE_INFINITE = 32000;

// A frame lists its children by action kind in order, then one forced pass, then nothing more.
static constexpr uint8_t STAGE_PASS = BATTLE_ACTION_MOVE + 1;
static constexpr uint8_t STAGE_DONE = STAGE_PASS + 1;
static constexpr BattleAction PASS_ACTION{BATTLE_NO_UNIT, 0};

// Bit unit * 4 + direction names one candidate. Four units with four directions fit in 16 bits.
static_assert(BATTLE_UNIT_CAPACITY * 4 <= 16, "legal-action slots must fit in one 16-bit mask");

// One node on the explicit search stack. It holds only what the node needs to resume after a child
// returns: the position, its window, the best child so far, and where it is in the child list.
struct SearchFrame {
    BattleState state;
    BATTLE_TEAM side;         // The team to act here. Its scores are measured for that team.
    int16_t alpha, beta;      // The window, from this node's perspective.
    int16_t best;             // Best child score so far, from this node's perspective.
    BattleAction current;     // The child being searched.
    BattleAction best_action; // The child that produced best.
    uint16_t legal;           // Bit unit * 4 + direction for each legal action at this node.
    uint8_t slot;             // Next candidate bit to consider within the current stage.
    uint8_t stage;            // The action kind being listed, STAGE_PASS, or STAGE_DONE.
};

// Frame i holds the node at ply i. The last frame is scratch for a child scored without expansion.
// Battles are single-threaded and searches never nest, so one static workspace serves every call.
// Its size is sizeof(SearchFrame) * (BATTLE_AI_DEPTH + 1) plus the counters.
static SearchFrame frames[BATTLE_AI_DEPTH + 1];
static BattleSearchStats stats;

static BATTLE_TEAM opponent(BATTLE_TEAM team) {
    return team == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
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

// The distance from a living unit to the nearest living unit of the other team.
static int16_t nearest_foe(BattleState const& state, uint8_t index) {
    BattleUnit const& unit = state.units[index];
    int16_t nearest = 2 * BATTLE_COLUMNS;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& foe = state.units[i];
        if(foe.hp && foe.team != unit.team) {
            int16_t distance = cell_distance(unit.cell, foe.cell);
            if(distance < nearest)
                nearest = distance;
        }
    }
    return nearest;
}

// What one side gains by striking a unit it can reach. A kill is worth more than a hit.
static int16_t strike_value(BattleState const& state, uint8_t attacker, uint8_t victim) {
    if(!battle_can_strike(state, attacker, victim))
        return 0;
    BattleUnit const& target = state.units[victim];
    return target.hp <= battle_attack_damage(state.units[attacker]) ? EVAL_KILL_THREAT
                                                                    : EVAL_ATTACK_THREAT;
}

// Scores a position from one side's perspective. Living units and HP count for the side and against
// its opponent. Each unit prefers to be near the other team, and each adjacent pair scores both
// sides' possible strikes. Terminal positions never reach here.
static int16_t evaluate(BattleState const& state, BATTLE_TEAM side) {
    BATTLE_TEAM foe = opponent(side);
    int16_t score =
        (int16_t)(EVAL_UNIT * ((int16_t)battle_living(state, side) - battle_living(state, foe)));
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp)
            continue;
        int16_t value = (int16_t)(EVAL_HP * unit.hp - EVAL_DISTANCE * nearest_foe(state, i));
        score = (int16_t)(score + (unit.team == side ? value : -value));
    }
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        for(uint8_t j = 0; j < state.unit_count; ++j) {
            if(state.units[i].team != side || state.units[j].team != foe)
                continue;
            score = (int16_t)(score + strike_value(state, i, j) - strike_value(state, j, i));
        }
    }
    return score;
}

int16_t battle_score(BattleState const& state, BATTLE_TEAM side, uint8_t ply) {
    if(!state.ended)
        return evaluate(state, side);
    // The result names the winner. state.turn is not used, because a battle-ending action leaves it
    // where it was.
    BATTLE_TEAM winner = state.result == BATTLE_VICTORY ? BATTLE_PARTY : BATTLE_ENEMY;
    int16_t magnitude = (int16_t)(BATTLE_SCORE_WIN - ply);
    return winner == side ? magnitude : (int16_t)-magnitude;
}

// The candidate slots holding a legal action. The enumeration supplies the actions, so the slots
// match what the player and the rules see.
static uint16_t legal_slots(BattleState const& state) {
    uint16_t slots = 0;
    BattleActionCursor cursor = battle_actions(state);
    BattleAction action{};
    while(battle_next_action(state, cursor, action)) {
        DIRECTION dir = battle_direction(state.units[action.unit_index].cell, action.destination);
        uint8_t slot = (uint8_t)(action.unit_index * 4 + dir);
        slots = (uint16_t)(slots | (1u << slot));
    }
    return slots;
}

// The action a slot names. Only slots with a legal bit set are read, so the neighbor exists.
static BattleAction slot_action(BattleState const& state, uint8_t slot) {
    uint8_t unit = (uint8_t)(slot / 4);
    uint8_t destination = 0;
    battle_neighbor(state.units[unit].cell, (DIRECTION)(slot % 4), destination);
    return BattleAction{unit, destination};
}

// Lists the next child of a frame in move order: wins, kills, other attacks, moves, and then a
// forced pass when the side has no legal action. Each legal action is listed exactly once.
static bool next_child(SearchFrame& frame, BattleAction& action) {
    while(frame.stage <= BATTLE_ACTION_MOVE) {
        while(frame.slot < 16) {
            uint8_t slot = frame.slot;
            ++frame.slot;
            if(!((frame.legal >> slot) & 1))
                continue;
            BattleAction candidate = slot_action(frame.state, slot);
            if(battle_action_kind(frame.state, candidate) == frame.stage) {
                action = candidate;
                return true;
            }
        }
        ++frame.stage;
        frame.slot = 0;
    }
    if(frame.stage == STAGE_PASS) {
        frame.stage = STAGE_DONE;
        if(!frame.legal) {
            action = PASS_ACTION;
            return true;
        }
    }
    return false;
}

// Credits a child's score, already from this frame's perspective, to the frame. Once the score
// reaches beta, the frame is cut off and lists no more children.
static void absorb(SearchFrame& frame, int16_t score, BattleAction action) {
    if(score > frame.best) {
        frame.best = score;
        frame.best_action = action;
    }
    if(score > frame.alpha)
        frame.alpha = score;
    if(frame.alpha >= frame.beta)
        frame.stage = STAGE_DONE;
}

// Scores are ordered by band. Each band is wider than any distance a move can score inside it.
static constexpr int16_t SCORE_KILL = 3000, SCORE_ATTACK = 2000, SCORE_MOVE = 1000;

// Manhattan distance from a cell to the nearest living unit of `team`.
static int16_t distance_to_team(BattleState const& state, uint8_t cell, BATTLE_TEAM team) {
    int16_t nearest = 255;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp && unit.team == team) {
            int16_t distance = cell_distance(cell, unit.cell);
            if(distance < nearest)
                nearest = distance;
        }
    }
    return nearest;
}

int16_t battle_greedy_score(BattleState const& state, BattleAction action) {
    uint8_t target = battle_unit_at(state, action.destination);
    if(target != BATTLE_NO_UNIT) {
        uint8_t damage = battle_attack_damage(state.units[action.unit_index]);
        return state.units[target].hp <= damage ? SCORE_KILL : SCORE_ATTACK;
    }
    return (int16_t)(SCORE_MOVE -
                     distance_to_team(state, action.destination, opponent(state.turn)));
}

// Credits a child of the root. The static evaluation is antisymmetric, so approaching a foe cancels
// out of it, and many root moves tie on search score alone. A tie goes to the action the greedy
// preference ranks higher, then to the first in move order. The window keeps one point below the
// best score, so every child that could tie is still searched exactly.
static void absorb_root(SearchFrame& root, int16_t& best_key, int16_t score, BattleAction action) {
    int16_t key = action.unit_index == BATTLE_NO_UNIT ? 0 : battle_greedy_score(root.state, action);
    if(score > root.best || (score == root.best && key > best_key)) {
        root.best = score;
        root.best_action = action;
        best_key = key;
        root.alpha = (int16_t)(score - 1);
    }
}

// Negamax with alpha-beta pruning over an explicit stack, so the search never recurses. A ply
// expands to its children in move order. A child that is terminal or at the depth limit is scored
// at once, and it does not occupy a frame. Pruning changes only how many positions are visited, not
// the root action, because the root's choice and its tie-break are exact for every child that can
// win.
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

    SearchFrame& top = frames[0];
    top.state = root;
    top.side = root.turn;
    top.alpha = (int16_t)-SCORE_INFINITE;
    top.beta = SCORE_INFINITE;
    top.best = (int16_t)-SCORE_INFINITE;
    top.legal = legal_slots(root);
    top.slot = 0;
    top.stage = 0;
    int16_t best_key = -1;

    // `ply` is the frame being expanded. Its children are built in the next frame, which is where a
    // leaf is scored too, so no copy of a position lives on the call stack.
    uint8_t ply = 0;
    for(;;) {
        SearchFrame& frame = frames[ply];
        BattleAction action{};
        if(next_child(frame, action)) {
            frame.current = action;
            ++stats.nodes;
            SearchFrame& child = frames[ply + 1];
            child.state = frame.state;
            if(action.unit_index == BATTLE_NO_UNIT)
                battle_pass(child.state);
            else
                battle_apply(child.state, action);
            child.side = opponent(frame.side);
            if(stats.deepest < ply + 1)
                stats.deepest = (uint8_t)(ply + 1);

            if(!child.state.ended && ply + 1 < depth) {
                // Descend. The child's window is the parent's window negated and swapped.
                child.alpha = (int16_t)-frame.beta;
                child.beta = (int16_t)-frame.alpha;
                child.best = (int16_t)-SCORE_INFINITE;
                child.legal = legal_slots(child.state);
                child.slot = 0;
                child.stage = 0;
                ++ply;
            } else {
                // Score a leaf from the child's perspective, then credit the parent with the
                // negation.
                ++stats.leaves;
                int16_t score = (int16_t)-battle_score(child.state, child.side, ply + 1);
                if(ply == 0)
                    absorb_root(frame, best_key, score, action);
                else
                    absorb(frame, score, action);
            }
        } else if(ply == 0) {
            result.action = frame.best_action;
            result.score = frame.best;
            return result;
        } else {
            // The frame is finished, so it returns its best score to the parent.
            int16_t score = frame.best;
            --ply;
            if(ply == 0)
                absorb_root(frames[0], best_key, (int16_t)-score, frames[0].current);
            else
                absorb(frames[ply], (int16_t)-score, frames[ply].current);
        }
    }
}

BattleSearchStats battle_search_stats() {
    return stats;
}

uint16_t battle_search_workspace_bytes() {
    return (uint16_t)(sizeof(frames) + sizeof(stats));
}

RPG_NOINLINE BattleAction battle_greedy_action(BattleState const& state) {
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

RPG_NOINLINE BattleAction battle_enemy_action(BattleState const& state) {
    BattleAction action = battle_search(state, BATTLE_AI_DEPTH).action;
    if(battle_legal(state, action))
        return action;
    return battle_greedy_action(state);
}
