/* Generated from TMX/TSX; do not edit. */

#include "logic/handlers.h"
template <HandlerStage STAGE> static void run_chunk_1_0_0() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::EasternBridge_0, STAGE>{}.update_bridge();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::HouseDoor, STAGE>{}.update_house_door();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::NorthGate, STAGE>{}.update_north_gate();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_1_0() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::EasternBridge_1, STAGE>{}.update_bridge();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::EasternBridge_2, STAGE>{}.update_bridge();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_0_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::EasternBridge_3, STAGE>{}.update_bridge();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::Waystone_0, STAGE>{}.update_waystone();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_1_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::EasternBridge_4, STAGE>{}.update_bridge();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::EasternBridge_5, STAGE>{}.update_bridge();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::Waystone_1, STAGE>{}.update_waystone();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_1_2() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::TidalPlatform, STAGE>{}.update_platform();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_0_0() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<GENERATED_TILE_2_2_0, STAGE>{}.update_lantern();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_0_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::InteriorExit, STAGE>{}.update_interior_exit();
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
            switch((uint16_t)coord.y * 2 + coord.x) {
                case 0: run_chunk_1_0_0<STAGE>(); break;
                case 1: run_chunk_1_1_0<STAGE>(); break;
                case 2: run_chunk_1_0_1<STAGE>(); break;
                case 3: run_chunk_1_1_1<STAGE>(); break;
                case 5: run_chunk_1_1_2<STAGE>(); break;
                default: break;
            }
            break;
        case 2:
            switch((uint16_t)coord.y * 1 + coord.x) {
                case 0: run_chunk_2_0_0<STAGE>(); break;
                case 1: run_chunk_2_0_1<STAGE>(); break;
                default: break;
            }
            break;
        default: break;
    }
}
template void run_chunk<STAGE_GAMEPLAY>(ChunkCoord);
template void run_chunk<STAGE_TILES>(ChunkCoord);
template void run_chunk<STAGE_DIALOGUE>(ChunkCoord);
