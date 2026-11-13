#ifndef BATTLE_AI_H
#define BATTLE_AI_H

#include "battle.h"

// The deepest search, in plies. Each ply is one activation, and a forced pass is one too. The
// static workspace is sized for this depth.
inline constexpr uint8_t BATTLE_AI_DEPTH = 4;

// Positions the enemy may visit while it chooses an action, summed over every iteration of the
// deepening search, for a battle of four living units. A position costs more to search the more
// units live, so a battle with more units gets proportionally fewer (battle_ai_budget). The value
// was chosen from AVM measurements (docs/performance.md), not derived from a time limit: the cost
// of a position varies, so it bounds the work rather than the seconds.
inline constexpr uint16_t BATTLE_AI_NODE_BUDGET = 300;

// A win or loss scores this much, less the plies it takes. Static utilities stay far below it, so a
// terminal result always outweighs material, HP, and position.
inline constexpr int16_t BATTLE_SCORE_WIN = 30000;

// Move-ordering priorities, searched from the lowest value up. A win is a kill of the last living
// opponent, so wins and other kills never occur together. An approach move brings its unit closer
// to attacking its nearest opponent. Ordering never removes an action from the search.
enum BATTLE_SEARCH_PRIORITY : uint8_t {
    BATTLE_PRIORITY_WIN,
    BATTLE_PRIORITY_KILL,
    BATTLE_PRIORITY_ATTACK,
    BATTLE_PRIORITY_APPROACH,
    BATTLE_PRIORITY_OTHER,
    BATTLE_PRIORITY_COUNT,
};

// The chosen action and its score from the side to act's perspective, and the depth of the
// completed search that chose it. The action is BattleAction{BATTLE_NO_UNIT, 0} when that side has
// no legal action and must pass, and when a bounded search completed no iteration, which leaves
// depth zero.
struct BattleSearchResult {
    BattleAction action;
    int16_t score;
    uint8_t depth;
};

// Work counters for the most recent search, kept on the host and in guests built with
// RPG_BATTLE_STATS. Other guests compile them out and report zeros. The node counters saturate
// instead of wrapping.
struct BattleSearchStats {
    uint16_t nodes;    // Positions reached by one action or forced pass, in every iteration.
    uint16_t leaves;   // Positions scored without expanding: terminal ones and depth-limit ones.
    uint8_t deepest;   // Greatest ply reached.
    uint8_t completed; // Deepest iteration searched to the end. Its result is the one returned.
    bool exhausted;    // An iteration ran out of budget, and its partial result was discarded.
    uint16_t iteration_nodes[BATTLE_AI_DEPTH]; // Nodes of each iteration, partial ones included.
};

// The deepest search the static action stack holds from this position: BATTLE_AI_DEPTH for battles
// of up to three units a side, fewer for larger ones. Units never return, so the limit holds below
// it.
uint8_t battle_search_depth_limit(BattleState const& state);

// Negamax with alpha-beta pruning, searched completely to `depth` plies (clamped to 1 through
// BATTLE_AI_DEPTH, then to battle_search_depth_limit) on an explicit stack. The state is not
// modified. Production code never recurses. The result is deterministic. Ties go to the first
// action in search order, and the action is always legal when one exists.
BattleSearchResult battle_search(BattleState const& state, uint8_t depth);

// Iterative deepening under a node budget: depth one, then two, and so on to `max_depth`, stopping
// once `budget` positions have been visited in all. An iteration whose budget runs out is
// discarded, and an iteration is not started when the budget left is below the growth estimate from
// the last one. Each iteration tries the previous iteration's choice first. The result is that of
// the deepest completed iteration. When none completed, its depth is zero and the action has no
// unit. A budget of zero means no limit.
// The deepest iteration is also limited by battle_search_depth_limit.
BattleSearchResult battle_think(BattleState const& state, uint8_t max_depth, uint16_t budget);

BattleSearchStats battle_search_stats();

// Bytes of static search workspace, not counting the counters, which only tests and probes read.
uint16_t battle_search_workspace_bytes();

// Orders one position's legal actions in place for the search, as battle_actions wrote them: kills
// first, then other attacks, then, when `approach` is set, approach moves before other moves. Each
// group is partitioned in place, so the order inside a group is deterministic but not the
// enumeration order. The search leaves `approach` clear at nodes whose children are all leaves. The
// reference search uses it to match the search's root order.
void battle_order_actions(BattleState const& state, uint8_t* actions, uint8_t count,
                          uint8_t attacks, bool approach);

// The ordering priority of one legal action.
BATTLE_SEARCH_PRIORITY battle_search_priority(BattleState const& state, BattleAction action);

// The score of a position from one side's perspective. A terminal position scores by its winner and
// its ply. Any other position scores by the enemy's utility, negated for the party. Terminal positions
// name the winner from `result`, since a battle-ending action leaves state.turn where it was.
int16_t battle_score(BattleState const& state, BATTLE_TEAM side, uint8_t ply);

// Cells a unit of `type` must still travel before it can attack a unit that is dx columns and dy
// rows away: it lines up on the row or column and then reaches its attack range along it. Units
// between them are ignored, so it is an estimate.
uint8_t battle_attack_gap(BATTLE_UNIT_TYPE type, uint8_t dx, uint8_t dy);

// The enemy's node budget for a position: BATTLE_AI_NODE_BUDGET with up to four living units,
// scaled by four over the living units beyond that (200 with six, 150 with eight).
uint16_t battle_ai_budget(BattleState const& state);

// The enemy's choice: battle_think at BATTLE_AI_DEPTH within battle_ai_budget. When no
// iteration completes, or its action is not legal, the greedy choice is used instead.
BattleAction battle_enemy_action(BattleState const& state);

// The greedy choice for the side to act. Kills beat attacks, attacks beat moves, and a move scores
// higher the smaller the attack gap it leaves to the nearest living opponent. Ties go to the first
// action in enumeration order. It is a deterministic baseline and a fallback, and it is always a
// legal action when one exists.
BattleAction battle_greedy_action(BattleState const& state);

// The greedy preference of one legal action.
int16_t battle_greedy_score(BattleState const& state, BattleAction action);

#endif
