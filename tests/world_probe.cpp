#include "battle.h"
#include "chunk.h"
#include "map_positions.h"
#include "tile_ids.h"
#include <avm.h>
#include <string.h>

volatile uint8_t world_phase;
volatile uint16_t probe_checks, probe_failure;
// LLDB enables the exhaustive geometry matrix before setup in full verification.
volatile bool probe_exhaustive;
volatile uint32_t probe_collision_hash;
static unsigned dialogues;

void fade_world_out() {}

void fade_world_in() {}

bool dialogue_show(char const PROGMEM*) {
    ++dialogues;
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

// The probes do not run the battle screen. Every battle is a win.
BATTLE_RESULT battle_show(BattleEncounter const PROGMEM*) {
    return BATTLE_VICTORY;
}

static void check(bool value, uint16_t line) {
    probe_checks = (uint16_t)(probe_checks + 1);
    if(!value && !probe_failure)
        probe_failure = line;
}

#define CHECK(value) check((value), __LINE__)
#include "world_scenarios.h"

extern "C" RPG_NOINLINE void probe_done() {
    avm_debug_break();
}

// Verify the batched edge probe against independent per-pixel geometry on the
// actual guest, including quadrant/tile boundaries and 16-bit coordinate wraps.
RPG_NOINLINE static void run_edge_probe() {
    static constexpr PixelPos PROGMEM POSITIONS[] = {
        {0, 0}, {4, 0}, {7, 7}, {8, 8}, {12, 9}, {15, 15}, {16, 16}, {23, 7}, {65530, 65530}};
    game.dimensions = {3, 2};
    game.cache.origin_tx = game.cache.origin_ty = 0;
    uint32_t hash = 2166136261u;
    for(uint8_t area = AREA_OUTDOOR; area <= AREA_DUNGEON; ++area) {
        game.area = area;
        for(uint16_t tile = 0; tile < 256; ++tile) {
            uint16_t mask = tile_collision((uint8_t)tile);
            // Independently compared with TSX metadata by the host, for every tile
            // in every area, including the outside sentinel.
            hash = (hash ^ (uint8_t)mask) * 16777619u;
            hash = (hash ^ (uint8_t)(mask >> 8)) * 16777619u;
            bool seen = false;
            // Scan the existing table rather than allocating a second copy in
            // the guest's limited RAM/stack. Most duplicates match the first IDs.
            for(uint16_t previous = 0; !probe_exhaustive && previous < tile; ++previous) {
                if(tile_collision((uint8_t)previous) == mask) {
                    seen = true;
                    break;
                }
            }
            if(!probe_exhaustive && seen)
                continue;
            memset(game.cache.tiles, tile, sizeof(game.cache.tiles));
            for(uint8_t position = 0; position < sizeof(POSITIONS) / sizeof(POSITIONS[0]);
                ++position) {
                uint16_t x = POSITIONS[position].x, y = POSITIONS[position].y;
                for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
                    bool horizontal = dir == EAST || dir == WEST;
                    uint8_t expected = 0;
                    for(uint8_t i = 0; i < 8; ++i) {
                        uint16_t wx =
                            (uint16_t)(horizontal ? x + (dir == EAST ? FOOT_RIGHT : FOOT_LEFT)
                                                  : x + FOOT_LEFT + i);
                        uint16_t wy =
                            (uint16_t)(horizontal ? y + FOOT_TOP + i
                                                  : y + (dir == SOUTH ? FOOT_BOTTOM : FOOT_TOP));
                        bool blocked = !game.dimensions.pixel_bounds(wx, wy);
                        if(!blocked) {
                            uint8_t id = cached_tile({game.area, (uint8_t)(wx >> TILE_SHIFT),
                                                      (uint8_t)(wy >> TILE_SHIFT)});
                            blocked = collision_pixel_solid(tile_collision(id), wx & 15, wy & 15);
                        }
                        if(blocked)
                            expected |= (uint8_t)(1u << i);
                    }
                    CHECK(player_leading_edge(x, y, dir) == expected);
                }
            }
        }
    }
    probe_collision_hash = hash;
}

extern "C" void setup() {
    run_world_scenarios();
    game_new();
    game_update(0, BTN_A);
    // The sign shows its welcome, runs a battle (stubbed here), and then shows the outcome line.
    CHECK(dialogues == 2 && !frame_pressed);
    game.gold = 19;
    CHECK(save_restore() && game.gold == 19 && game.player.x == positions::PlayerStart.x * TILE);
    game.save_version = 3;
    CHECK(!save_restore());
    run_edge_probe();
    probe_done();
}

extern "C" void loop() {
    avm_idle();
}
