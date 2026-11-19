#ifndef RUNTIME_WORKSPACE_H
#define RUNTIME_WORKSPACE_H

#include "battle_ai.h"
#include "npc_types.h"
#include "world_types.h"

// One block of guest RAM serves exploration and battle, and only one of them is active at a time.
// Exploration owns it normally. A battle takes it after the world has faded out, and battle exit
// gives it back: world_reclaim_workspace rebuilds the tile window and camera from the world assets
// and the persistent flags, and invalidates every cached collision answer. Nothing exploration
// keeps in this block survives a battle, so the saved Game holds none of it.

// One entry of an NPC stage's admission list: the identity, its home, and its priority.
struct NpcCandidate {
    NPC_ID id;
    TilePos site;
    uint8_t priority;
};

// The foot position that last passed player_terrain_clear. Its answer holds only until a tile edit,
// a reload that discards edits, or a change of area or position.
struct FootCache {
    uint16_t x, y;
    uint8_t area;
    bool known;
    bool tiles_edited; // A tile edit since the last reload, which discards the edit.
};

// Camera comes first, so every member starts at the same offset on the host and on the AVM. A host
// copy of the workspace then differs from the guest's only in its trailing padding.
struct ExplorationWorkspace {
    Camera camera;
    TileCache cache;
    NpcCandidate candidates[NPC_CAPACITY];
    uint8_t candidate_count;
    FootCache foot;
};

// Each ply keeps its alpha and the next and end indices of its ordered actions in the shared action
// stack. A ply's beta is its parent's alpha, negated.
struct SearchPly {
    int16_t alpha;
    uint8_t next, end;
};

struct BattleWorkspace {
    // The children of ply k are built in ply_state[k], whether they are expanded or scored as leaves.
    BattleState ply_state[BATTLE_AI_DEPTH];
    SearchPly plies[BATTLE_AI_DEPTH];
    // Each ply's killer: the last move that cut a node off at that ply.
    uint8_t killers[BATTLE_AI_DEPTH];
    // Shared scratch for action lists. While the enemy's search runs it is the search's action stack;
    // between searches, battle_can_act, the battle screen's destination markers, and the greedy choice
    // list actions here briefly. No list here survives a call into another of those users.
    uint8_t action_scratch[BATTLE_ACTION_SCRATCH_CAPACITY];
};

union RuntimeWorkspace {
    ExplorationWorkspace explore;
    BattleWorkspace battle;
};

extern RuntimeWorkspace runtime_workspace;

// Names for the members. A constexpr reference is an address constant, so each name costs no RAM and
// no loads beyond the member's own.
inline constexpr TileCache& cache = runtime_workspace.explore.cache;
inline constexpr Camera& camera = runtime_workspace.explore.camera;
inline constexpr NpcCandidate (&candidates)[NPC_CAPACITY] = runtime_workspace.explore.candidates;
inline constexpr uint8_t& candidate_count = runtime_workspace.explore.candidate_count;
inline constexpr FootCache& foot_cache = runtime_workspace.explore.foot;
inline constexpr BattleState (&ply_state)[BATTLE_AI_DEPTH] = runtime_workspace.battle.ply_state;
inline constexpr SearchPly (&plies)[BATTLE_AI_DEPTH] = runtime_workspace.battle.plies;
inline constexpr uint8_t (&killers)[BATTLE_AI_DEPTH] = runtime_workspace.battle.killers;
inline constexpr uint8_t (&battle_action_scratch)[BATTLE_ACTION_SCRATCH_CAPACITY] =
    runtime_workspace.battle.action_scratch;

static_assert(sizeof(RuntimeWorkspace) == 226, "the battle search is the larger of the two shares");
#ifdef RPG_AVM
static_assert(sizeof(ExplorationWorkspace) == 131, "tile window, camera, admission and foot cache");
#endif

#endif
