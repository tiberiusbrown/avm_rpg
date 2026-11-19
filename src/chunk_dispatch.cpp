#include "map_handler_catalog.h"
#include "runtime_workspace.h"
#ifndef RPG_NPC_POLICY_HEADER
#define RPG_NPC_POLICY_HEADER "logic/npc_policies.h"
#endif
#include RPG_NPC_POLICY_HEADER

using AllNpcSites = typename PolicyCatalog<std::make_index_sequence<NPC_ID_LIMIT - 1>>::sites;

using ChunkFunction = void (*)();

template <unsigned A, unsigned INDEX, HandlerStage STAGE> struct StaticChunk {
    static constexpr auto area = STATIC_AREAS[A];
    using Sites = typename std::conditional<STAGE == STAGE_NPCS, AllNpcSites, AllMapHandlers>::type;
    using Work = ChunkSites<area.id, INDEX % area.columns, INDEX / area.columns, STAGE, Sites>;

    static RPG_NOINLINE void run() {
        Work::run();
    }

    static consteval ChunkFunction function() {
        if constexpr(Work::occupied)
            return &run;
        return nullptr;
    }
};

template <unsigned A, HandlerStage STAGE, typename SEQUENCE> struct ChunkTable;

template <unsigned A, HandlerStage STAGE, std::size_t... I>
struct ChunkTable<A, STAGE, std::index_sequence<I...>> {
    static constexpr ChunkFunction PROGMEM functions[] = {StaticChunk<A, I, STAGE>::function()...};

    struct RowSpan {
        uint8_t first, end;
    };

    static constexpr auto rows PROGMEM = [] {
        struct Rows {
            RowSpan span[STATIC_AREAS[A].rows];
        } result{};
        constexpr bool entries[] = {StaticChunk<A, I, STAGE>::Work::occupied...};
        constexpr auto area = STATIC_AREAS[A];
        for(unsigned y = 0; y < area.rows; ++y) {
            uint8_t first = area.columns, end = 0;
            for(uint8_t x = 0; x < area.columns; ++x)
                if(entries[y * area.columns + x]) {
                    if(first == area.columns)
                        first = x;
                    end = x + 1;
                }
            result.span[y] = {first, end};
        }
        return result;
    }();
    static constexpr bool occupied = [] {
        constexpr bool entries[] = {StaticChunk<A, I, STAGE>::Work::occupied...};
        for(bool entry : entries)
            if(entry)
                return true;
        return false;
    }();
};

template <unsigned A, HandlerStage STAGE>
using AreaTable =
    ChunkTable<A, STAGE,
               std::make_index_sequence<(unsigned)STATIC_AREAS[A].columns * STATIC_AREAS[A].rows>>;

// The hot traversal selects its area once. Chunk lookups then need only the
// dense table offset, without repeated area selection and coordinate validation.
#ifdef _MSC_VER
#define CHUNK_INLINE __forceinline
#else
#define CHUNK_INLINE inline __attribute__((always_inline))
#endif

template <unsigned A, HandlerStage STAGE> static CHUNK_INLINE void process_area() {
    constexpr auto area = STATIC_AREAS[A];
#ifdef RPG_AVM
    if constexpr(!AreaTable<A, STAGE>::occupied)
        return;
#endif
    Camera shown = STAGE == STAGE_GAMEPLAY ? game_camera() : camera;
    uint8_t left = (uint8_t)(shown.x >> TILE_SHIFT), top = (uint8_t)(shown.y >> TILE_SHIFT);
    left = left >= CACHE_MARGIN ? (uint8_t)(left - CACHE_MARGIN) : 0;
    top = top >= CACHE_MARGIN ? (uint8_t)(top - CACHE_MARGIN) : 0;
    uint16_t right = (uint16_t)left + CACHE_WIDTH - 1;
    uint16_t bottom = (uint16_t)top + CACHE_HEIGHT - 1;
    if(right >= area.width)
        right = area.width - 1;
    if(bottom >= area.height)
        bottom = area.height - 1;
    for(uint8_t cy = (uint8_t)(top / CHUNK_HEIGHT); cy <= bottom / CHUNK_HEIGHT; ++cy) {
        uint16_t row = (uint16_t)cy * area.columns;
        uint8_t first = (uint8_t)(left / CHUNK_WIDTH);
        uint8_t end = (uint8_t)(right / CHUNK_WIDTH + 1);
#ifndef RPG_AVM
        // Native instrumentation describes the requested cache window, including
        // empty chunks, independently of the immutable empty-entry optimization.
        for(uint8_t cx = first; cx < end; ++cx)
            if(chunk_trace_count[STAGE] < CHUNK_TRACE_CAPACITY)
                chunk_trace[STAGE][chunk_trace_count[STAGE]++] = row + cx;
#endif
        // Immutable per-stage row bounds skip only compile-time empty entries.
        // Dense function tables and cached-chunk ordering remain unchanged.
        auto const& span = AreaTable<A, STAGE>::rows.span[cy];
        if(first < span.first)
            first = span.first;
        if(end > span.end)
            end = span.end;
        for(uint8_t cx = first; cx < end; ++cx) {
            uint16_t index = row + cx;
            if(auto function = AreaTable<A, STAGE>::functions[index])
                function();
            if constexpr(STAGE == STAGE_GAMEPLAY || STAGE == STAGE_DIALOGUE)
                if(teleported)
                    return;
        }
    }
}

template <HandlerStage STAGE, unsigned A = 0> static void process_selected_area() {
    if(game.area == STATIC_AREAS[A].id) {
        process_area<A, STAGE>();
        return;
    }
    if constexpr(A + 1 < sizeof(STATIC_AREAS) / sizeof(STATIC_AREAS[0]))
        process_selected_area<STAGE, A + 1>();
}

template <HandlerStage STAGE> void process_all_chunks() {
    process_selected_area<STAGE>();
}

template void process_all_chunks<STAGE_GAMEPLAY>();
template void process_all_chunks<STAGE_TILES>();
template void process_all_chunks<STAGE_DIALOGUE>();
template void process_all_chunks<STAGE_NPCS>();

template <HandlerStage STAGE, std::size_t A = 0>
static ChunkFunction lookup(uint8_t id, uint8_t x, uint8_t y) {
    constexpr auto area = STATIC_AREAS[A];
    if(id == area.id) {
        if(x >= area.columns || y >= area.rows)
            return nullptr;
        return AreaTable<A, STAGE>::functions[(uint16_t)y * area.columns + x];
    }
    if constexpr(A + 1 < sizeof(STATIC_AREAS) / sizeof(STATIC_AREAS[0]))
        return lookup<STAGE, A + 1>(id, x, y);
    return nullptr;
}

template <HandlerStage STAGE> RPG_NOINLINE void run_chunk(uint8_t area, uint8_t x, uint8_t y) {
#ifndef RPG_AVM
    auto dimensions = world_area(area);
    if(dimensions && x < dimensions->chunk_columns && y < dimensions->chunk_rows &&
       chunk_trace_count[STAGE] < CHUNK_TRACE_CAPACITY)
        chunk_trace[STAGE][chunk_trace_count[STAGE]++] =
            (uint16_t)y * dimensions->chunk_columns + x;
#endif
    if(auto function = lookup<STAGE>(area, x, y))
        function();
}

template void run_chunk<STAGE_GAMEPLAY>(uint8_t, uint8_t, uint8_t);
template void run_chunk<STAGE_TILES>(uint8_t, uint8_t, uint8_t);
template void run_chunk<STAGE_DIALOGUE>(uint8_t, uint8_t, uint8_t);
template void run_chunk<STAGE_NPCS>(uint8_t, uint8_t, uint8_t);

namespace {
template <typename> struct Resolvers;

template <std::size_t... I> struct Resolvers<std::index_sequence<I...>> {
    static constexpr TilePos (*const PROGMEM functions[])() = {&npc_location<(NPC_ID)(I + 1)>...};
    static constexpr bool PROGMEM fixed[] = {
        (sizeof(LocationPolicy<(NPC_ID)(I + 1)>::locations) == sizeof(TilePos))...};
};
} // namespace

TilePos npc_location(NPC_ID id) {
    if(id <= NPC_NONE || id >= NPC_ID_LIMIT)
        return NO_LOCATION;
    return Resolvers<std::make_index_sequence<NPC_ID_LIMIT - 1>>::functions[id - 1]();
}

bool npc_location_is_fixed(NPC_ID id) {
    return id > NPC_NONE && id < NPC_ID_LIMIT &&
           Resolvers<std::make_index_sequence<NPC_ID_LIMIT - 1>>::fixed[id - 1];
}
