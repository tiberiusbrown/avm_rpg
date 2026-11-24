#include "map_handler_catalog.h"
#include "runtime_workspace.h"
#include "logic/enemy_catalog.h"
#include <string.h>
#ifdef RPG_NPC_POLICY_HEADER
#include RPG_NPC_POLICY_HEADER
#endif

using EssentialSites = typename PolicyCatalog<std::make_index_sequence<NPC_ID_LIMIT - 1>>::sites;
using AllNpcSites = typename Join<EssentialSites, AuthoredEnemies::sites>::type;

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

    static constexpr bool occupied = [] {
        constexpr bool entries[] = {StaticChunk<A, I, STAGE>::Work::occupied...};
        for(bool entry : entries)
            if(entry)
                return true;
        return false;
    }();

    // The most chunks with a handler that a window of three chunk columns and three chunk rows
    // holds. A cache window intersects at most that many chunks, wherever it starts.
    static constexpr unsigned most_per_window = [] {
        constexpr bool entries[] = {StaticChunk<A, I, STAGE>::Work::occupied...};
        constexpr auto area = STATIC_AREAS[A];
        unsigned most = 0;
        for(unsigned top = 0; top < area.rows; ++top) {
            for(unsigned left = 0; left < area.columns; ++left) {
                unsigned count = 0;
                for(unsigned cy = top; cy < top + 3 && cy < area.rows; ++cy)
                    for(unsigned cx = left; cx < left + 3 && cx < area.columns; ++cx)
                        if(entries[cy * area.columns + cx])
                            ++count;
                if(count > most)
                    most = count;
            }
        }
        return most;
    }();
};

template <unsigned A, HandlerStage STAGE>
using AreaTable =
    ChunkTable<A, STAGE,
               std::make_index_sequence<(unsigned)STATIC_AREAS[A].columns * STATIC_AREAS[A].rows>>;

// Which of the nine chunks of a window have a handler, per stage. A window's chunks are the
// three-by-three block that starts at its first chunk, less a third column or row that it does not
// reach. Bit three times the row plus the column of the block is set for a chunk with a handler;
// chunks past the end of the area are never set. Rebuilding a window reads one record, the
// block's four stage masks, rather than visiting its chunks.
template <unsigned A, typename SEQUENCE> struct ChunkBlocks;

template <unsigned A, std::size_t... I> struct ChunkBlocks<A, std::index_sequence<I...>> {
    static constexpr auto occupancy PROGMEM = [] {
        constexpr auto area = STATIC_AREAS[A];
        constexpr bool has[4][sizeof...(I)] = {
            {StaticChunk<A, I, STAGE_GAMEPLAY>::Work::occupied...},
            {StaticChunk<A, I, STAGE_TILES>::Work::occupied...},
            {StaticChunk<A, I, STAGE_DIALOGUE>::Work::occupied...},
            {StaticChunk<A, I, STAGE_NPCS>::Work::occupied...}};
        struct Table {
            uint16_t block[sizeof...(I)][4];
        } table{};
        for(unsigned index = 0; index < sizeof...(I); ++index)
            for(unsigned stage = 0; stage < 4; ++stage) {
                unsigned bits = 0;
                for(unsigned row = 0; row < 3; ++row)
                    for(unsigned column = 0; column < 3; ++column)
                        if(index % area.columns + column < area.columns &&
                           index / area.columns + row < area.rows &&
                           has[stage][index + row * area.columns + column])
                            bits |= 1u << (row * 3 + column);
                table.block[index][stage] = (uint16_t)bits;
            }
        return table;
    }();

    // For every set of chunks in a block, their distances from the block's first chunk in the
    // area's chunk table, in chunk order. No set that a window can have holds more than
    // CHUNK_PLAN_CAPACITY chunks.
    static constexpr auto lists PROGMEM = [] {
        constexpr auto area = STATIC_AREAS[A];
        static_assert(2 * area.columns + 2 <= 255, "chunk distances fit in a byte");
        struct Table {
            ChunkStageList list[512];
        } table{};
        for(unsigned bits = 0; bits < 512; ++bits)
            for(unsigned row = 0; row < 3; ++row)
                for(unsigned column = 0; column < 3; ++column)
                    if((bits >> (row * 3 + column) & 1) &&
                       table.list[bits].count < CHUNK_PLAN_CAPACITY)
                        table.list[bits].offset[table.list[bits].count++] =
                            (uint8_t)(row * area.columns + column);
        return table;
    }();
};

template <unsigned A>
using AreaBlocks =
    ChunkBlocks<A,
                std::make_index_sequence<(unsigned)STATIC_AREAS[A].columns * STATIC_AREAS[A].rows>>;

// The hot traversal selects its area once. Chunk lookups then need only the
// dense table offset, without repeated area selection and coordinate validation.
#ifdef _MSC_VER
#define CHUNK_INLINE __forceinline
#else
#define CHUNK_INLINE inline __attribute__((always_inline))
#endif

#ifndef RPG_AVM
// The chunks that a cache window with this origin intersects: columns from left_chunk up to
// end_chunk, and rows top_chunk through bottom_chunk. A cache window past a map edge still fits in
// byte-sized coordinates.
struct ChunkWindow {
    uint8_t left_chunk, end_chunk, top_chunk, bottom_chunk;
};

template <unsigned A> static ChunkWindow chunk_window(uint8_t left, uint8_t top) {
    constexpr auto area = STATIC_AREAS[A];
    static_assert(MAX_MAP_WIDTH + CACHE_WIDTH <= 256 && MAX_MAP_HEIGHT + CACHE_HEIGHT <= 256);
    uint8_t right = (uint8_t)(left + CACHE_WIDTH - 1);
    uint8_t bottom = (uint8_t)(top + CACHE_HEIGHT - 1);
    if(right >= area.width)
        right = area.width - 1;
    if(bottom >= area.height)
        bottom = area.height - 1;
    return {(uint8_t)(left / CHUNK_WIDTH), (uint8_t)(right / CHUNK_WIDTH + 1),
            (uint8_t)(top / CHUNK_HEIGHT), (uint8_t)(bottom / CHUNK_HEIGHT)};
}
#endif

// A window reaches a second chunk column and row wherever it starts, and a third column or row
// when its first tile is far enough into its own chunk: for the 11 by 7 window and 8 by 4 chunks,
// from the sixth tile of a chunk's eight columns and the third of its four rows.
static constexpr uint8_t THIRD_COLUMN_FROM = 2 * CHUNK_WIDTH - (CACHE_WIDTH - 1);
static constexpr uint8_t THIRD_ROW_FROM = 2 * CHUNK_HEIGHT - (CACHE_HEIGHT - 1);
static_assert(CACHE_WIDTH - 1 >= CHUNK_WIDTH && CACHE_HEIGHT - 1 >= CHUNK_HEIGHT &&
                  CHUNK_WIDTH - 1 + CACHE_WIDTH - 1 < 3 * CHUNK_WIDTH &&
                  CHUNK_HEIGHT - 1 + CACHE_HEIGHT - 1 < 3 * CHUNK_HEIGHT,
              "chunk plans assume windows of two or three chunks each way");

// Lists the chunks of the window that have a handler, per stage in chunk order: the order that
// the stages have always visited them in. Only the chunk's distance is kept. The handler itself is
// still read from its table when it runs, which is what lets the link prove its stack use.
template <unsigned A> RPG_NOINLINE static void plan_window(uint8_t left, uint8_t top) {
    constexpr auto area = STATIC_AREAS[A];
    static_assert(AreaTable<A, STAGE_GAMEPLAY>::most_per_window <= CHUNK_PLAN_CAPACITY &&
                      AreaTable<A, STAGE_TILES>::most_per_window <= CHUNK_PLAN_CAPACITY &&
                      AreaTable<A, STAGE_DIALOGUE>::most_per_window <= CHUNK_PLAN_CAPACITY &&
                      AreaTable<A, STAGE_NPCS>::most_per_window <= CHUNK_PLAN_CAPACITY,
                  "a window collects more handlers than the chunk plan holds");
    ChunkPlan& plan = chunk_plan;
    uint8_t const chunk_left = (uint8_t)(left / CHUNK_WIDTH);
    uint8_t const chunk_top = (uint8_t)(top / CHUNK_HEIGHT);
    uint8_t const extent = (uint8_t)((left % CHUNK_WIDTH >= THIRD_COLUMN_FROM ? 1 : 0) |
                                     (top % CHUNK_HEIGHT >= THIRD_ROW_FROM ? 2 : 0));
    bool const same_chunks = plan.area == area.id && plan.chunk_left == chunk_left &&
                             plan.chunk_top == chunk_top && plan.extent == extent;
    plan.area = area.id;
    plan.left = left;
    plan.top = top;
    // The camera usually moves within the same chunks, which keep the lists they have.
    if(same_chunks)
        return;
    plan.chunk_left = chunk_left;
    plan.chunk_top = chunk_top;
    plan.extent = extent;
    // The block's chunks that the window reaches.
    uint16_t keep = 0x1FF;
    if(!(extent & 1))
        keep &= (uint16_t)~0x124; // The third column.
    if(!(extent & 2))
        keep &= (uint16_t)~0x1C0; // The third row.
    uint16_t block[4];
    memcpy_P(block,
             &AreaBlocks<A>::occupancy.block[(uint16_t)chunk_top * area.columns + chunk_left],
             sizeof(block));
    for(uint8_t stage = 0; stage < 4; ++stage) {
        uint16_t bits = block[stage] & keep;
        if(bits)
            memcpy_P(&plan.stage[stage], &AreaBlocks<A>::lists.list[bits], sizeof(ChunkStageList));
        else
            plan.stage[stage].count = 0;
    }
}

template <unsigned A, HandlerStage STAGE> static CHUNK_INLINE void process_area() {
    constexpr auto area = STATIC_AREAS[A];
#ifdef RPG_AVM
    if constexpr(!AreaTable<A, STAGE>::occupied)
        return;
#endif
    // Gameplay precedes the reload. The later stages already have the exact cache window.
    uint8_t left, top;
    if constexpr(STAGE == STAGE_GAMEPLAY) {
        Camera shown = game_camera();
        uint8_t view_x = (uint8_t)(shown.x >> TILE_SHIFT);
        uint8_t view_y = (uint8_t)(shown.y >> TILE_SHIFT);
        left = view_x >= CACHE_MARGIN ? (uint8_t)(view_x - CACHE_MARGIN) : 0;
        top = view_y >= CACHE_MARGIN ? (uint8_t)(view_y - CACHE_MARGIN) : 0;
    } else {
        left = cache.origin_tx;
        top = cache.origin_ty;
    }
#ifndef RPG_AVM
    // Native instrumentation describes the requested cache window, including empty chunks.
    ChunkWindow window = chunk_window<A>(left, top);
    for(uint8_t cy = window.top_chunk; cy <= window.bottom_chunk; ++cy)
        for(uint8_t cx = window.left_chunk; cx < window.end_chunk; ++cx)
            if(chunk_trace_count[STAGE] < CHUNK_TRACE_CAPACITY)
                chunk_trace[STAGE][chunk_trace_count[STAGE]++] = (uint16_t)cy * area.columns + cx;
#endif
    ChunkPlan& plan = chunk_plan;
    if(plan.area != area.id || plan.left != left || plan.top != top)
        plan_window<A>(left, top);
    // The list depends only on the window. A handler edits tiles and flags but does not move the
    // window and then refresh it before this stage ends, so the list outlasts the calls. The loop
    // keeps only its index across a call, which holds this function's frame to what it was.
    uint16_t const first = (uint16_t)plan.chunk_top * area.columns + plan.chunk_left;
    for(uint8_t i = 0; i < plan.stage[STAGE].count; ++i) {
        AreaTable<A, STAGE>::functions[first + plan.stage[STAGE].offset[i]]();
        if constexpr(STAGE == STAGE_GAMEPLAY || STAGE == STAGE_DIALOGUE)
            if(teleported)
                return;
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

RPG_NOINLINE TilePos npc_location(NPC_ID id) {
    if(npc_is_enemy(id))
        return AuthoredEnemies::location(id);
    if(id <= NPC_NONE || id >= NPC_ID_LIMIT)
        return NO_LOCATION;
    return Resolvers<std::make_index_sequence<NPC_ID_LIMIT - 1>>::functions[id - 1]();
}

bool npc_location_is_fixed(NPC_ID id) {
    return id > NPC_NONE && id < NPC_ID_LIMIT &&
           Resolvers<std::make_index_sequence<NPC_ID_LIMIT - 1>>::fixed[id - 1];
}
