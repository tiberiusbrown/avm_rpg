/* Generated from TMX/TSX; do not edit. */

#include "logic/handlers.h"
template <HandlerStage STAGE> static void run_chunk_1_0_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::GroveSign, STAGE>{}.read_sign();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::VillagerHome, STAGE>{}.update_villager();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_1_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::WandererHome, STAGE>{}.update_wanderer();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE void run_chunk(ChunkCoord coord) {
#ifndef RPG_AVM
    if(chunk_trace_count[STAGE] < 4)
        chunk_trace[STAGE][chunk_trace_count[STAGE]++] = (uint16_t)coord.y * world_area(coord.area)->chunk_columns + coord.x;
#endif
    switch(coord.area) {
        case 1:
            switch((uint16_t)coord.y * 3 + coord.x) {
                case 3: run_chunk_1_0_1<STAGE>(); break;
                case 4: run_chunk_1_1_1<STAGE>(); break;
                default: break;
            }
            break;
        default: break;
    }
}
template void run_chunk<STAGE_GAMEPLAY>(ChunkCoord);
template void run_chunk<STAGE_TILES>(ChunkCoord);
template void run_chunk<STAGE_DIALOGUE>(ChunkCoord);
