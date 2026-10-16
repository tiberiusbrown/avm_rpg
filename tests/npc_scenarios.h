// Identical bounded scenarios run on the host and in the genuine AVM interpreter.
#include "npc.h"
#include "chunk.h"
#include "map_positions.h"
#include "npc_routes.h"
#include "tile_ids.h"
#include <string.h>

static uint8_t npc_test_record[SAVE_BYTES];
static unsigned npc_test_dialogues, npc_test_choices;

static uint8_t npc_test_count() {
    uint8_t count = 0;
    for(auto const& n : game.npcs.actors)
        if(n.id)
            ++count;
    return count;
}

static void npc_test_valid() {
    for(uint8_t i = 0; i < NPC_CAPACITY; ++i) {
        auto const& n = game.npcs.actors[i];
        if(!n.id)
            continue;
        CHECK(npc_definition(n.id));
        CHECK(terrain_clear(n.actor.x, n.actor.y));
        for(uint8_t j = i + 1; j < NPC_CAPACITY; ++j) {
            CHECK(!game.npcs.actors[j].id || game.npcs.actors[j].id != n.id);
            CHECK(!game.npcs.actors[j].id ||
                  !npc_overlap(n.actor.x, n.actor.y, game.npcs.actors[j].actor.x,
                               game.npcs.actors[j].actor.y));
        }
    }
}

static RPG_NOINLINE void npc_test_identity() {
    game_new();
    CHECK(NPC_GUIDE == 1 && NPC_ROWAN == 17 && NPC_GUARD == 42 && NPC_COURIER == 91);
    CHECK(sizeof(Npc) == 18 && sizeof(game.cache.tiles) == 153);
    CHECK(sizeof(RoutePoint) == 2);
    CHECK(npc_route(routes::CourierWalk)->points[0].x == 15 &&
          npc_route(routes::CourierWalk)->points[0].y == 4);
    CHECK(npc_find(NPC_GUIDE) && npc_find(NPC_GUARD));
    Npc* guide = npc_find(NPC_GUIDE);
    guide->actor.x += 2;
    for(uint8_t i = 0; i < 30; ++i) {
        npc_request(NPC_GUIDE, positions::GuideHome);
        npc_request(NPC_GUIDE, positions::GuideHome);
        game_update(0);
        CHECK(npc_find(NPC_GUIDE) == guide && guide->actor.x == 42);
        npc_test_valid();
    }
    CHECK(!game.npcs.capacity_faults);
    game.player = {224, 136, EAST, 0};
    game_update(0);
    CHECK(!npc_find(NPC_GUIDE) && !npc_find(NPC_GUARD));
    CHECK(game.get_flag(FLAG_COURIER_ARRIVED));
    game.player = {32, 40, EAST, 0};
    game_update(0);
    CHECK(npc_find(NPC_GUIDE) && npc_find(NPC_GUIDE)->actor.x == 40);
    npc_test_valid();
    // A partially clipped actor with uncached feet freezes without snapping.
    npc_find(NPC_GUIDE)->actor.x = 46;
    game.player = {112, 40, EAST, 0};
    update_visible_world(0, false);
    CHECK(npc_find(NPC_GUIDE) && npc_find(NPC_GUIDE)->actor.x == 46);
    game.player.x = 114;
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_GUIDE));
}

static RPG_NOINLINE void npc_test_relocation_and_save() {
    game_new();
    Npc* guard = npc_find(NPC_GUARD);
    CHECK(guard);
    uint16_t x = guard->actor.x, y = guard->actor.y;
    game.set_flag(FLAG_GUARD_RELOCATED); // Destination area has never executed.
    for(uint8_t i = 0; i < 12; ++i) {
        game_update(0);
        CHECK(npc_find(NPC_GUARD) == guard && guard->actor.x == x && guard->actor.y == y);
        CHECK(guard->placement == 0);
    }
    CHECK(npc_home(NPC_GUARD, npc_placement(NPC_GUARD)).area == AREA_INTERIOR);
    save_encode(npc_test_record);
    CHECK(save_decode(npc_test_record));
    CHECK(!npc_find(NPC_GUARD) && game.get_flag(FLAG_GUARD_RELOCATED));
    world_enter(AREA_INTERIOR, 16, 56, NORTH);
    CHECK(npc_find(NPC_GUARD) && npc_find(NPC_GUARD)->placement == 1);
    CHECK(npc_find(NPC_GUARD)->actor.x == (uint16_t)positions::CastleGuardPost.x * 8);
    save_encode(npc_test_record);
    world_enter(AREA_OUTDOOR, 200, 120, EAST);
    CHECK(!npc_find(NPC_GUARD));
    CHECK(save_decode(npc_test_record) && npc_find(NPC_GUARD));
    game.clear_flag(FLAG_GUARD_RELOCATED);
    game_update(0);
    CHECK(npc_find(NPC_GUARD)->placement == 1); // Visible reverse relocation also waits.
    world_enter(AREA_OUTDOOR, 32, 40, EAST);
    CHECK(npc_find(NPC_GUARD) && npc_find(NPC_GUARD)->placement == 0);
    npc_test_valid();
}

static RPG_NOINLINE void npc_test_activation() {
    game_new();
    npc_reset();
    game.player = {40, 24, EAST, 0};
    refresh_tile_cache();
    npc_request(NPC_GUIDE, positions::GuideHome);
    npc_reconcile();
    CHECK(!npc_find(NPC_GUIDE) && game.npcs.activation_deferrals == 1);
    game.player.x = 24;
    npc_reconcile();
    CHECK(npc_find(NPC_GUIDE));
    npc_reset();
    set_tile(positions::GuideHome, TILE_WALL);
    npc_request(NPC_GUIDE, positions::GuideHome);
    npc_reconcile();
    CHECK(!npc_find(NPC_GUIDE));
    refresh_tile_cache();
    npc_reconcile();
    CHECK(npc_find(NPC_GUIDE));
    // Corrupt/undefined identities and the wrong quest anchor cannot activate.
    npc_request((NPC_ID)255, positions::GuideHome);
    npc_request(NPC_GUARD, positions::CastleGuardPost);
    CHECK(game.npcs.request_count == 1);
    npc_test_valid();
}

static RPG_NOINLINE void npc_test_absence_and_saved_travel() {
    game_new();
    CHECK(!game.get_flag(FLAG_COURIER_DISPATCHED));
    game_update(0);
    // Dispatch is a persistent stage even before arrival or destination dispatch.
    CHECK(game.get_flag(FLAG_COURIER_DISPATCHED) && !game.get_flag(FLAG_COURIER_ARRIVED));
    CHECK(npc_home(NPC_COURIER, npc_placement(NPC_COURIER)).x == positions::CourierFinish.x);
    save_encode(npc_test_record);
    CHECK(save_decode(npc_test_record));
    CHECK(game.get_flag(FLAG_COURIER_ARRIVED) && !npc_find(NPC_COURIER));
    world_enter(AREA_OUTDOOR, 184, 64, NORTH);
    CHECK(npc_find(NPC_COURIER) && npc_find(NPC_COURIER)->behavior == NPC_STAND);

    game.set_flag(FLAG_ROWAN_AWAY);
    world_enter(AREA_OUTDOOR, 56, 104, NORTH);
    CHECK(!npc_find(NPC_ROWAN));
    game.clear_flag(FLAG_ROWAN_AWAY);
    game_update(0);
    CHECK(npc_find(NPC_ROWAN));
    game.set_flag(FLAG_ROWAN_AWAY);
    game_update(0);
    CHECK(npc_find(NPC_ROWAN)); // Visible departure also defers until out of sight.
    save_encode(npc_test_record);
    CHECK(save_decode(npc_test_record) && !npc_find(NPC_ROWAN));
    CHECK(npc_placement(NPC_ROWAN) == NPC_PLACEMENT_ABSENT);
    game.clear_flag(FLAG_ROWAN_AWAY);
    game_update(0);
    CHECK(npc_find(NPC_ROWAN));
}

static RPG_NOINLINE void npc_test_wander() {
    game_new();
    world_enter(AREA_OUTDOOR, 56, 104, NORTH);
    CHECK(npc_find(NPC_ROWAN));
    uint16_t previous_x = npc_find(NPC_ROWAN)->actor.x;
    uint16_t previous_y = npc_find(NPC_ROWAN)->actor.y;
    uint8_t directions = 0;
    bool walked = false, paused = false;
    for(uint16_t i = 0; i < 320; ++i) {
        game_update(0);
        auto n = npc_find(NPC_ROWAN);
        CHECK(n);
        int16_t dx = (int16_t)n->actor.x - 56, dy = (int16_t)n->actor.y - 80;
        CHECK(dx >= -12 && dx <= 12 && dy >= -8 && dy <= 8);
        CHECK(terrain_clear(n->actor.x, n->actor.y));
        int16_t mx = (int16_t)n->actor.x - (int16_t)previous_x;
        int16_t my = (int16_t)n->actor.y - (int16_t)previous_y;
        CHECK(mx >= -1 && mx <= 1 && my >= -1 && my <= 1);
        walked |= n->actor.walking != 0;
        paused |= n->actor.walking == 0;
        directions |= (uint8_t)(1u << n->actor.facing);
        previous_x = n->actor.x;
        previous_y = n->actor.y;
    }
    CHECK(walked && paused && (directions & (directions - 1)));
}

static RPG_NOINLINE void npc_test_targets_routes() {
    game_new();
    CHECK(npc_move_to(NPC_GUIDE, 56, 24));
    for(uint8_t i = 0; i < 24; ++i)
        game_update(0);
    CHECK(npc_find(NPC_GUIDE)->actor.x == 56 && npc_find(NPC_GUIDE)->behavior == NPC_STAND);
    CHECK(!npc_move_to((NPC_ID)255, 10, 10) && !npc_move_to(NPC_GUIDE, 65535, 10));
    CHECK(npc_move_to(NPC_GUIDE, 80, 24)); // House wall at x=80: bounded stuck fallback.
    for(uint8_t i = 0; i < 64; ++i)
        game_update(0);
    CHECK(npc_find(NPC_GUIDE)->state & NPC_TARGET_FAILED);
    CHECK(terrain_clear(npc_find(NPC_GUIDE)->actor.x, npc_find(NPC_GUIDE)->actor.y));

    game_new();
    game.player = {136, 16, EAST, 0};
    update_visible_world(0, false); // Four chunks, both sides of x=128.
    Npc* courier = npc_find(NPC_COURIER);
    CHECK(courier && courier->actor.x == 120);
    CHECK(npc_follow_route(NPC_COURIER, routes::CourierWalk));
    CHECK(!npc_follow_route(NPC_COURIER, 255));
    uint16_t before_x = courier->actor.x;
    game_update(0); // Point zero pauses; no movement from multiple chunk calls.
    CHECK(courier->actor.x == before_x && courier->timer == 6);
    for(uint8_t i = 0; i < 6; ++i)
        game_update(0);
    CHECK(courier->actor.x == 120);
    game_update(0);
    CHECK(courier->actor.x == 121); // One update, not four.
    bool crossed = false;
    for(uint8_t i = 0; i < 120; ++i) {
        uint16_t old = courier->actor.x;
        game_update(0);
        CHECK(npc_find(NPC_COURIER) == courier);
        CHECK(courier->actor.x >= old && courier->actor.x <= old + 1);
        crossed |= courier->actor.x >= 128;
        npc_test_valid();
    }
    CHECK(crossed && game.get_flag(FLAG_COURIER_ARRIVED));
    CHECK(courier->behavior == NPC_STAND && courier->actor.x == 184 && courier->actor.y == 48);
    save_encode(npc_test_record);
    CHECK(save_decode(npc_test_record));
    CHECK(npc_find(NPC_COURIER) && npc_find(NPC_COURIER)->actor.x == 184);

    // Both reusable endpoint policies have bounded pauses and stay on the path.
    for(uint8_t mode = 0; mode < 2; ++mode) {
        courier = npc_find(NPC_COURIER);
        courier->actor = {120, 32, EAST, 0};
        CHECK(npc_follow_route(NPC_COURIER, mode ? routes::CourierReverse : routes::CourierLoop));
        uint8_t facings = 0;
        for(uint8_t i = 0; i < 90; ++i) {
            game_update(0);
            CHECK(courier->actor.x >= 120 && courier->actor.x <= 136 && courier->actor.y == 32);
            CHECK(courier->behavior == NPC_ROUTE);
            facings |= (uint8_t)(1u << courier->actor.facing);
        }
        CHECK((facings & (1u << EAST)) && (facings & (1u << WEST)));
    }
}

static RPG_NOINLINE void npc_test_lifecycle() {
    game_new();
    Npc* courier = npc_find(NPC_COURIER);
    CHECK(courier && (courier->state & NPC_ROUTE_PENDING));
    update_visible_world(0, false);
    game_update(0, BTN_A);
    CHECK(!game.get_flag(FLAG_COURIER_DISPATCHED));
    game_update(0);
    CHECK(game.get_flag(FLAG_COURIER_DISPATCHED));
    CHECK(!(courier->state & NPC_ROUTE_PENDING));
    // A route-start hook fires once per command, not on every simulation update.
    game.clear_flag(FLAG_COURIER_DISPATCHED);
    game_update(0);
    CHECK(!game.get_flag(FLAG_COURIER_DISPATCHED));
    CHECK(npc_follow_route(NPC_COURIER, routes::CourierWalk));
    game_update(0);
    CHECK(game.get_flag(FLAG_COURIER_DISPATCHED));

    // A different route belonging to the same actor is not a delivery event.
    game_new();
    world_enter(AREA_OUTDOOR, 136, 16, EAST);
    CHECK(npc_follow_route(NPC_COURIER, routes::CourierLoop));
    for(uint8_t i = 0; i < 90; ++i)
        game_update(0);
    CHECK(!game.get_flag(FLAG_COURIER_DISPATCHED) && !game.get_flag(FLAG_COURIER_ARRIVED));
    npc_reset();
    CHECK(!game.get_flag(FLAG_COURIER_ARRIVED));

    // The delivery polyline remains generic when traversed by another identity.
    game_new();
    world_enter(AREA_OUTDOOR, 136, 16, EAST);
    courier = npc_find(NPC_COURIER);
    CHECK(courier);
    courier->id = NPC_GUIDE;
    CHECK(npc_follow_route(NPC_GUIDE, routes::CourierWalk));
    for(uint8_t i = 0; i < 140; ++i)
        npc_update();
    CHECK(courier->behavior == NPC_STAND && courier->actor.x == 184 && courier->actor.y == 48);
    CHECK(!game.get_flag(FLAG_COURIER_DISPATCHED) && !game.get_flag(FLAG_COURIER_ARRIVED));

    // Retiring a never-simulated courier must not start or complete the journey.
    game_new();
    game.player = {224, 136, EAST, 0};
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_COURIER));
    CHECK(!game.get_flag(FLAG_COURIER_DISPATCHED) && !game.get_flag(FLAG_COURIER_ARRIVED));

    // Terrain recovery is an interruption, not successful offscreen travel.
    game_new();
    game_update(0);
    CHECK(game.get_flag(FLAG_COURIER_DISPATCHED));
    memset(game.cache.tiles, TILE_WALL, sizeof(game.cache.tiles));
    npc_repair();
    CHECK(!npc_find(NPC_COURIER));
    CHECK(!game.get_flag(FLAG_COURIER_DISPATCHED) && !game.get_flag(FLAG_COURIER_ARRIVED));
    CHECK(npc_placement(NPC_COURIER) == 0);
    npc_reset();
    CHECK(!game.get_flag(FLAG_COURIER_ARRIVED));
    world_enter(AREA_OUTDOOR, 32, 40, EAST);
    CHECK(npc_find(NPC_COURIER) && npc_find(NPC_COURIER)->behavior == NPC_ROUTE);
}

static RPG_NOINLINE void npc_test_effective_terrain() {
    game_new();
    game.set_flag(FLAG_GATE_OPEN);
    refresh_tile_cache();
    Npc* guard = npc_find(NPC_GUARD);
    guard->actor = {120, 16, SOUTH, 0};
    game.clear_flag(FLAG_GATE_OPEN);
    game_update(0);
    CHECK(npc_find(NPC_GUARD));
    CHECK(terrain_clear(guard->actor.x, guard->actor.y));
    CHECK(guard->actor.x != 120 || guard->actor.y != 16);
    npc_reset();
    game.npcs.actors[0].id = NPC_GUIDE;
    game.npcs.actors[0].actor = {40, 24, SOUTH, 0};
    memset(game.cache.tiles, TILE_WALL, sizeof(game.cache.tiles));
    npc_repair();
    CHECK(!npc_find(NPC_GUIDE) && game.npcs.activation_deferrals == 1);
    CHECK(!terrain_clear(200, 32)); // No off-window terrain projection for NPCs.
}

static RPG_NOINLINE void npc_test_interactions() {
    game_new();
    world_enter(AREA_OUTDOOR, 40, 32, NORTH);
    unsigned before = npc_test_dialogues;
    uint16_t ticks = game.ticks, rng = game.rng;
    game_update(BTN_RIGHT, BTN_A);
    CHECK(npc_test_dialogues == before + 1 && game.player.x == 40);
    CHECK(game.ticks == ticks && game.rng == rng && !frame_pressed);
    // Put the guard on the open gate's target tile: NPC owns the sole A edge.
    game_new();
    game.set_flag(FLAG_GATE_OPEN);
    game.player = {112, 16, EAST, 0};
    update_visible_world(0, false);
    auto guard = npc_find(NPC_GUARD);
    guard->actor = {120, 16, WEST, 0};
    before = npc_test_choices;
    game_update(0, BTN_A);
    CHECK(npc_test_choices == before + 1);
    CHECK(game.get_flag(FLAG_GUARD_RELOCATED) && game.get_flag(FLAG_GATE_OPEN));
    CHECK(!frame_pressed && guard->actor.x == 120);
    before = npc_test_dialogues;
    game_update(0, BTN_A);
    CHECK(npc_test_dialogues == before + 1 && game.get_flag(FLAG_GATE_OPEN));
    // Tile interaction still works when the NPC is out of targeting range.
    guard->actor.x = 104;
    game_update(0, BTN_A);
    CHECK(!game.get_flag(FLAG_GATE_OPEN));
}

static void npc_test_push_step(DIRECTION dir) {
    npc_begin_frame();
    npc_update();
    npc_player_begin(dir);
    actor_move(&game.player, dir, 2);
    ++game.ticks;
}

static RPG_NOINLINE void npc_test_corridor_case(uint8_t count, bool wander, bool boundary) {
    game_new();
    world_enter(AREA_NPC_HALL, 16, 24, EAST);
    npc_reset();
    for(uint8_t i = 0; i < count; ++i) {
        auto& n = game.npcs.actors[i];
        n.id = (wander != (i != 0)) ? NPC_HALL_2 : NPC_HALL_1;
        n.actor = {(uint16_t)((wander ? 48 : 32) + i * 8), 24, WEST, 0};
        n.behavior = wander && !i ? NPC_WANDER : NPC_STAND;
    }
    if(wander)
        game.player.x = 32;
    if(boundary) {
        game.player.x = 8;
        game.npcs.actors[0].actor.x = 16;
    }
    uint16_t start = game.player.x;
    bool blocked = false, passed = false;
    for(uint8_t i = 0; i < 80; ++i) {
        uint16_t previous = game.player.x;
        npc_test_push_step(EAST);
        CHECK(terrain_clear(game.player.x, game.player.y));
        CHECK(game.player.x >= previous && game.player.x <= previous + 2);
        blocked |= game.player.x == previous;
        passed |= game.player.x > start + 40;
    }
    CHECK(blocked && passed); // Eventual progress, including two closely packed bodies.
    for(auto const& n : game.npcs.actors)
        CHECK(!(n.state & NPC_PASS_PLAYER)); // Collision restores after separation.
}

static void npc_test_yield_bay() {
    game_new();
    npc_reset();
    game.player = {34, 24, EAST, 0};
    game.camera = {0, 0};
    game.cache.origin_tx = game.cache.origin_ty = 0;
    memset(game.cache.tiles, TILE_WALL, sizeof(game.cache.tiles));
    for(uint8_t x = 1; x < 15; ++x)
        set_tile({game.area, x, 3}, TILE_PATH);
    // Only the NPC can enter this alcove; the player's sidestep lane is walled.
    set_tile({game.area, 5, 2}, TILE_PATH);
    auto& n = game.npcs.actors[0];
    n = {};
    n.id = NPC_GUIDE;
    n.actor = {40, 24, SOUTH, 0};
}

static RPG_NOINLINE void npc_test_yield_and_pressure() {
    game_new();
    npc_reset();
    game.player = {34, 24, EAST, 0};
    auto& n = game.npcs.actors[0];
    n.id = NPC_GUIDE;
    n.actor = {40, 24, SOUTH, 0};
    for(uint8_t i = 0; i < 30; ++i) {
        npc_test_push_step(EAST);
        CHECK(n.actor.x == 40 && n.actor.y == 24);
        CHECK(!(n.state & (NPC_YIELDING | NPC_PASS_PLAYER)));
    }
    CHECK(game.player.x == 34 && (n.state & NPC_CAN_DETOUR));
    // The ordinary collision is traversable by actually walking around the NPC.
    for(uint8_t i = 0; i < 3; ++i)
        npc_test_push_step(NORTH);
    for(uint8_t i = 0; i < 8; ++i)
        npc_test_push_step(EAST);
    for(uint8_t i = 0; i < 3; ++i)
        npc_test_push_step(SOUTH);
    CHECK(game.player.x > 46 && game.player.y == 24 && n.actor.y == 24);

    npc_test_yield_bay();
    for(uint8_t i = 0; i < NPC_PUSH_FRAMES; ++i) {
        npc_test_push_step(EAST);
        CHECK(n.actor.x == 40 && n.actor.y == 24 && !n.actor.walking);
        CHECK(!(n.state & NPC_PASS_PLAYER));
    }
    CHECK(n.state & NPC_YIELDING);
    CHECK(game.player.x == 34);
    for(uint8_t i = 0; i < 5; ++i) {
        uint16_t previous = n.actor.y;
        npc_test_push_step(EAST);
        CHECK(n.actor.x == 40 && n.actor.y + 1 == previous && n.actor.walking);
        CHECK(terrain_clear(n.actor.x, n.actor.y));
        CHECK(!(n.state & NPC_PASS_PLAYER));
    }
    CHECK(!(n.state & NPC_YIELDING) && n.actor.y == 19 && game.player.x > 34);

    // If another body occupies the alcove, wait and permit passage without jumping.
    npc_test_yield_bay();
    auto& other = game.npcs.actors[1];
    other = {};
    other.id = NPC_HALL_1;
    other.actor = {40, 16, SOUTH, 0};
    for(uint8_t i = 0; i < NPC_PUSH_FRAMES; ++i)
        npc_test_push_step(EAST);
    CHECK(n.state & NPC_PASS_PLAYER);
    CHECK(n.actor.y == 24 && other.actor.y == 16 && game.player.x > 34);

    // A newly closed tile can interrupt a yield, but cannot reinstate a softlock.
    npc_test_yield_bay();
    for(uint8_t i = 0; i < NPC_PUSH_FRAMES; ++i)
        npc_test_push_step(EAST);
    set_tile({game.area, 5, 2}, TILE_WALL);
    for(uint8_t i = 0; i < 4; ++i) {
        uint16_t previous = n.actor.y;
        npc_test_push_step(EAST);
        CHECK(n.actor.y == previous || n.actor.y + 1 == previous);
        CHECK(terrain_clear(game.player.x, game.player.y));
    }
    CHECK(game.player.x > 34 && (n.state & NPC_PASS_PLAYER));

    world_enter(AREA_NPC_HALL, 16, 24, EAST);
    npc_reset();
    n = {};
    n.id = NPC_HALL_1;
    n.actor = {24, 24, EAST, 0};
    for(uint8_t i = 0; i < 5; ++i)
        npc_test_push_step(EAST);
    CHECK(n.push > 0 && n.push < NPC_PUSH_FRAMES);
    npc_test_push_step(WEST);
    CHECK(n.push == 0);
}

static RPG_NOINLINE void npc_test_full_pool() {
    game_new();
    world_enter(AREA_NPC_HALL, 88, 24, EAST);
    CHECK(npc_test_count() == NPC_CAPACITY && !game.npcs.capacity_faults);
    npc_test_valid();
    for(uint8_t i = 0; i < 24; ++i) {
        game_update(0);
        CHECK(npc_test_count() == NPC_CAPACITY && !game.npcs.capacity_faults);
    }
    // Inject an additional valid visible character's eligibility. Existing visible
    // actors must survive, and over-capacity demand must be reported.
    game.npcs.requests[0] = NPC_GUIDE;
    game.npcs.request_count = 1;
    npc_reconcile(); // Wrong area is safely deferred, not a capacity fault.
    CHECK(npc_test_count() == NPC_CAPACITY);
    game_new();
    for(uint8_t i = 0; i < NPC_CAPACITY; ++i) {
        game.npcs.actors[i] = {};
        game.npcs.actors[i].id = (NPC_ID)(NPC_HALL_1 + i);
        game.npcs.actors[i].actor = {(uint16_t)(16 + i * 8), 8, SOUTH, 0};
    }
    npc_begin_frame();
    npc_request(NPC_GUIDE, positions::GuideHome);
    npc_reconcile();
    CHECK(game.npcs.capacity_faults == 1 && !npc_find(NPC_GUIDE));
    CHECK(npc_test_count() == NPC_CAPACITY);
    npc_begin_frame();
    for(uint8_t i = 0; i < NPC_CAPACITY; ++i)
        game.npcs.requests[i] = (NPC_ID)(NPC_HALL_1 + i);
    game.npcs.request_count = NPC_CAPACITY;
    npc_request(NPC_GUIDE, positions::GuideHome);
    CHECK(game.npcs.request_count == NPC_CAPACITY && game.npcs.requests[0] == NPC_GUIDE);
    CHECK(game.npcs.capacity_faults == 2);
    npc_request(NPC_GUIDE, positions::GuideHome);
    CHECK(game.npcs.capacity_faults == 2); // Deduplication precedes capacity accounting.
    // Full exploration pipeline demonstrates progress through the authored hall.
    world_enter(AREA_NPC_HALL, 16, 24, EAST);
    for(uint16_t i = 0; i < 180; ++i) {
        game_update(BTN_RIGHT);
        CHECK(terrain_clear(game.player.x, game.player.y));
        CHECK(!game.npcs.capacity_faults);
    }
    CHECK(game.player.x > 152);
    CHECK(teleport_player<positions::InteriorArrival>());
    CHECK(npc_test_count() == 0);
    game_update(0);
    CHECK(game.area == AREA_INTERIOR);
}

static void run_npc_scenarios() {
    npc_test_identity();
    npc_test_relocation_and_save();
    npc_test_activation();
    npc_test_absence_and_saved_travel();
    npc_test_wander();
    npc_test_targets_routes();
    npc_test_lifecycle();
    npc_test_effective_terrain();
    npc_test_interactions();
    npc_test_corridor_case(1, false, false);
    npc_test_corridor_case(1, true, false);
    npc_test_corridor_case(2, false, false);
    npc_test_corridor_case(2, true, false);
    npc_test_corridor_case(1, false, true);
    npc_test_yield_and_pressure();
    npc_test_full_pool();
}
