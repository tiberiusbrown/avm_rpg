#include "chunk.h"
#include "tile_ids.h"
#include "scaling_policies.h"
#include "logic/npc_logic.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if(!(x)) {                                                                                 \
            std::fprintf(stderr, "%d: %s\n", __LINE__, #x);                                        \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while(0)

NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    static constexpr NpcDefinition DEFINITIONS[] = {{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 0, 0},
                                                    {NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 1, 0},
                                                    {NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 2, 0}};
    return id && id < NPC_ID_LIMIT ? &DEFINITIONS[id % 3] : nullptr;
}

char const PROGMEM* npc_name(NPC_ID) {
    return F("Fixture");
}

static unsigned activation_mutation;

void npc_on_activate(Npc& n) {
    if(n.id != (NPC_ID)2)
        return;
    TilePos next = npc_location((NPC_ID)5);
    switch(activation_mutation) {
        case 1:
            memset(cache.tiles, TILE_TREE, sizeof(cache.tiles));
            note_tile_edit();
            break;
        case 2:
            game.player.x = next.x * TILE;
            game.player.y = next.y * TILE;
            break;
        case 3:
            game.clear_flag(FLAG_VILLAGER_EAST);
            break;
        case 4:
            game.party.members[game.party.count++] = {1, 1, 0, 0, (NPC_ID)5};
            break;
        case 5:
            n.actor.x = next.x * TILE;
            n.actor.y = next.y * TILE;
            break;
    }
}

void npc_on_event(Npc&, NPC_EVENT) {}

void npc_on_retire(Npc const&, NPC_RETIRE_REASON) {}

void npc_logic_restore() {}

void npc_talk(NPC_ID) {}

void npc_dialogue() {}

bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return false;
}

bool dialogue_show(NPC_ID, char const PROGMEM*) {
    return true;
}

template <typename> struct SiteCount;

template <typename... T> struct SiteCount<TypeList<T...>> {
    static constexpr unsigned value = sizeof...(T);
};

static void stage(bool reverse) {
    npc_stage_begin();
    if(reverse) {
        for(int y = 2; y >= 0; --y)
            for(int x = 2; x >= 0; --x)
                run_chunk<STAGE_NPCS>(game.area, cache.origin_tx / 8 + x,
                                      cache.origin_ty / 4 + y);
    } else {
        for(int y = 0; y < 3; ++y)
            for(int x = 0; x < 3; ++x)
                run_chunk<STAGE_NPCS>(game.area, cache.origin_tx / 8 + x,
                                      cache.origin_ty / 4 + y);
    }
    npc_stage_end();
}

int main() {
    static_assert(NPC_ID_LIMIT == 65 && NPC_CAPACITY == 8);
    using Sites = PolicyCatalog<std::make_index_sequence<64>>::sites;
    static_assert(SiteCount<Sites>::value == 128);
    static_assert(sizeof(NpcPool) == 180);
    game_new();
    // With no quest crowd, empty distant chunks evaluate no identities.
    npc_stage_begin();
    run_chunk<STAGE_NPCS>(AREA_OUTDOOR, 29, 61);
    CHECK(npc_site_checks == 0);
    npc_stage_end();

    game.set_flag(FLAG_VILLAGER_EAST);
    world_enter(positions::TestField.area, positions::TestField.x * TILE,
                positions::TestField.y * TILE, SOUTH);
    // A clear synthetic cache isolates selection from authored terrain.
    npc_reset();
    memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
    note_tile_edit();
    stage(false);
    NPC_ID selected[NPC_CAPACITY];
    for(unsigned i = 0; i < NPC_CAPACITY; ++i) {
        selected[i] = game.npcs.actors[i].id;
        CHECK(selected[i] == (NPC_ID)(2 + 3 * i));
        for(unsigned j = 0; j < i; ++j)
            CHECK(selected[j] != selected[i]);
    }
    npc_reset();
    stage(true);
    for(unsigned i = 0; i < NPC_CAPACITY; ++i)
        CHECK(selected[i] == game.npcs.actors[i].id);
    CHECK(npc_site_checks >= 64);
    // Discovery outside their sites is independent of catalog length.
    npc_stage_begin();
    run_chunk<STAGE_NPCS>(AREA_OUTDOOR, 29, 61);
    CHECK(npc_site_checks == 0);
    npc_stage_end();
    // Hooks run between sorted casts. Each mutation invalidates a different part
    // of the admission proof; no later candidate may reuse stale eligibility.
    for(unsigned mutation = 1; mutation <= 5; ++mutation) {
        game_new();
        game.set_flag(FLAG_VILLAGER_EAST);
        world_enter(positions::TestField.area, positions::TestField.x * TILE,
                    positions::TestField.y * TILE, SOUTH);
        npc_reset();
        memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
        note_tile_edit();
        activation_mutation = mutation;
        stage(false);
        CHECK(npc_find((NPC_ID)2));
        CHECK(!npc_find((NPC_ID)5));
        if(mutation == 1 || mutation == 3)
            CHECK(!game.npcs.actors[1].id);
        for(unsigned i = 0; i < NPC_CAPACITY && game.npcs.actors[i].id; ++i)
            for(unsigned j = 0; j < i; ++j)
                CHECK(!npc_overlap(game.npcs.actors[i].actor.x, game.npcs.actors[i].actor.y,
                                   game.npcs.actors[j].actor.x, game.npcs.actors[j].actor.y));
        activation_mutation = 0;
    }
    // Full pool of ordinary off-cache residents with cached homes. Even a
    // higher-priority offer must wait; retention neither evicts nor duplicates.
    game_new();
    game.set_flag(FLAG_VILLAGER_EAST);
    world_enter(positions::TestField.area, positions::TestField.x * TILE,
                positions::TestField.y * TILE, SOUTH);
    npc_reset();
    memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
    note_tile_edit();
    for(unsigned i = 0; i < NPC_CAPACITY; ++i) {
        auto& n = game.npcs.actors[i];
        n = {};
        n.id = (NPC_ID)(1 + 3 * i);
        n.assigned_site = npc_location(n.id);
        n.behavior = NPC_WANDER;
        n.actor.x = n.actor.y = 0;
    }
    stage(false);
    CHECK(!npc_find((NPC_ID)2));
    for(unsigned i = 0; i < NPC_CAPACITY; ++i)
        CHECK(game.npcs.actors[i].id == (NPC_ID)(1 + 3 * i));
    // Both the actors and homes are now outside the effective cache and view.
    cache.origin_tx += CACHE_WIDTH;
    cache.origin_ty += CACHE_HEIGHT;
    npc_stage_begin();
    CHECK(!game.npcs.actors[0].id);
    npc_stage_end();
    std::puts("64-identity policy and bounded activation checks passed");
}
