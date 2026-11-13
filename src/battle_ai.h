#ifndef BATTLE_AI_H
#define BATTLE_AI_H

#include "battle.h"

// Plies searched from the side to act. Each ply is one activation, and a forced pass is one too.
inline constexpr uint8_t BATTLE_AI_DEPTH = 4;

// A win or loss scores this much, less the plies it takes. Static utilities stay far below it, so a
// terminal result always outweighs material, HP, and position.
inline constexpr int16_t BATTLE_SCORE_WIN = 30000;

// Move-ordering priorities, searched from the lowest value up. A win is a kill of the last living
// opponent. An approach move closes the distance to the nearest living opponent. Ordering never removes
// an action from the search.
enum BATTLE_SEARCH_PRIORITY : uint8_t {
    BATTLE_PRIORITY_WIN,
    BATTLE_PRIORITY_KILL,
    BATTLE_PRIORITY_ATTACK,
    BATTLE_PRIORITY_APPROACH,
    BATTLE_PRIORITY_OTHER,
    BATTLE_PRIORITY_COUNT,
};

// The chosen action and its score from the side to act's perspective. The action is
// BattleAction{BATTLE_NO_UNIT, 0} only when that side has no legal action and must pass.
struct BattleSearchResult {
    BattleAction action;
    int16_t score;
};

// Work counters for the most recent search. The counters saturate instead of wrapping.
struct BattleSearchStats {
    uint16_t nodes;  // Positions reached by one action or forced pass, pruned or not.
    uint16_t leaves; // Positions scored without expanding: terminal ones and depth-limit ones.
    uint8_t deepest; // Greatest ply reached.
};

// Negamax with alpha-beta pruning, searched to `depth` plies (clamped to 1 through BATTLE_AI_DEPTH)
// on an explicit stack. The state is not modified. Production code never recurses. The result is
// deterministic. Ties go to the first action in move order, and the root's action is always legal
// when one exists.
BattleSearchResult battle_search(BattleState const& state, uint8_t depth);

BattleSearchStats battle_search_stats();

// Bytes of static search workspace, not counting the counters.
uint16_t battle_search_workspace_bytes();

// The move-ordering priority of one legal action. Reference searches use it to match the search's order.
BATTLE_SEARCH_PRIORITY battle_search_priority(BattleState const& state, BattleAction action);

// The score of a position from one side's perspective. A terminal position scores by its winner and
// its ply. Any other position scores by the enemy's utility, negated for the party. Terminal positions
// name the winner from `result`, since a battle-ending action leaves state.turn where it was.
int16_t battle_score(BattleState const& state, BATTLE_TEAM side, uint8_t ply);

// The enemy's choice: the search at BATTLE_AI_DEPTH. When that result is not legal, the greedy choice
// is used instead.
BattleAction battle_enemy_action(BattleState const& state);

// The greedy choice for the side to act. Kills beat attacks, attacks beat moves, and a move scores
// higher the closer it leaves its unit to the nearest living opponent. Ties go to the lowest unit
// index, then to the direction order north, east, south, west. It is a deterministic baseline and a
// fallback, and it is always a legal action when one exists.
BattleAction battle_greedy_action(BattleState const& state);

// The greedy preference of one legal action.
int16_t battle_greedy_score(BattleState const& state, BattleAction action);

#endif
