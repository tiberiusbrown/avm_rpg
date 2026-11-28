#ifndef RUNTIME_WORKSPACE_H
#define RUNTIME_WORKSPACE_H

#include "battle_ai.h"
#include "npc_types.h"
#include "quest.h"
#include "world_types.h"

// One block of guest RAM serves exploration and battle, and only one of them is active at a time.
// Exploration owns it normally. A battle takes it after the world has faded out, and battle exit
// gives it back: world_reclaim_workspace rebuilds the tile window and camera from the world assets
// and the persistent flags, and invalidates every cached collision answer. Nothing exploration
// keeps in this block survives a battle, so the saved Game holds none of it. The battle screen
// shows waiting quest notices first, and quest_resume_after_battle rebuilds the quest state.

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

// A function that runs one chunk's handlers for one stage.
using ChunkFunction = void (*)();

// The most chunks with a handler for one stage that any cache window can intersect. The window is
// 11x7 tiles and chunks are 8x4, so it touches at most nine chunks, and the authored handlers fill
// at most five of them. chunk_dispatch.cpp checks that they never fill more than this.
inline constexpr uint8_t CHUNK_PLAN_CAPACITY = 6;

// One stage's chunks within a window, in chunk order: each is the distance in the area's chunk
// table from the window's first chunk.
struct ChunkStageList {
    uint8_t count;
    uint8_t offset[CHUNK_PLAN_CAPACITY];
};

// The chunks of one cache window that have a handler, per stage. A walk reruns the same window
// every frame until the camera crosses a tile, so the window is resolved against the chunk tables
// once and each stage then visits its list. The lists hold nothing the chunk tables do not, so
// they can be dropped at any time: area 0 marks them as empty.
struct ChunkPlan {
    // The cache window the lists were built for: the area and the cache origin.
    uint8_t area, left, top;
    // The chunks that window intersects: the first one's column and row, and whether the window
    // reaches a third column and a third row. A different origin often intersects the same chunks.
    uint8_t chunk_left, chunk_top, extent;
    ChunkStageList stage[4];
};

// Camera comes first, so every member starts at the same offset on the host and on the AVM. The
// quest state comes last: its pointers are wider on the host, so a host copy of the workspace
// differs from the guest's only there.
struct ExplorationWorkspace {
    Camera camera;
    TileCache cache;
    NpcCandidate candidates[NPC_CAPACITY];
    uint8_t candidate_count;
    FootCache foot;
    ChunkPlan plan;
    // The area whose tiles the window holds, or 0 when the window must be loaded. The window is
    // exactly the assets' tiles at its origin, until a tile edit or a battle changes it.
    uint8_t tile_area;
    QuestWorkspace quest;
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
    uint8_t order_rows[3 * BATTLE_ROWS];
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
inline constexpr ChunkPlan& chunk_plan = runtime_workspace.explore.plan;
inline constexpr uint8_t& tile_area = runtime_workspace.explore.tile_area;
inline constexpr QuestWorkspace& quest_workspace = runtime_workspace.explore.quest;
inline constexpr BattleState (&ply_state)[BATTLE_AI_DEPTH] = runtime_workspace.battle.ply_state;
inline constexpr SearchPly (&plies)[BATTLE_AI_DEPTH] = runtime_workspace.battle.plies;
inline constexpr uint8_t (&killers)[BATTLE_AI_DEPTH] = runtime_workspace.battle.killers;
inline constexpr uint8_t (&battle_action_scratch)[BATTLE_ACTION_SCRATCH_CAPACITY] =
    runtime_workspace.battle.action_scratch;

static_assert(sizeof(RuntimeWorkspace) == 270, "the battle search is the larger of the two shares");
#ifdef RPG_AVM
static_assert(sizeof(ChunkPlan) == 6 + 4 * (1 + CHUNK_PLAN_CAPACITY), "byte-sized plan entries");
static_assert(sizeof(QuestWorkspace) == 3 * 7 + 1 + 2 + QUEST_BYTES, "byte-sized quest state");
static_assert(sizeof(ExplorationWorkspace) == 131 + sizeof(ChunkPlan) + 1 + sizeof(QuestWorkspace),
              "tile window, camera, admission, foot cache, chunk plan and quest state");
static_assert(sizeof(ExplorationWorkspace) <= sizeof(BattleWorkspace),
              "exploration must fit in the share that the battle search sizes");
#endif

#endif
