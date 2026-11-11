/* Generated from TMX/TSX; do not edit. */

#include "logic/handlers.h"
template <HandlerStage STAGE> static void run_chunk_1_4_4() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::WeaverDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_4_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::ElderDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_6_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::BakeryDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_1_6() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CottageDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_3_6() {
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
    Handler<positions::WandererHome, STAGE>{}.update_wanderer();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_5_6() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::ShopDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_1_9() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::FarmDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_3_9() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::SmithyDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_5_9() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::InnDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_6_9() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::HutDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_1_1_11() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::FisherDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_0_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::ElderExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_3_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CottageExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_5_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::WeaverExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_7_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::ShopExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_3_4() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::FarmExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_7_4() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::SmithyExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_0_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::BakeryExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_5_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::InnExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_0_7() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::HutExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> static void run_chunk_2_3_8() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::FisherExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE void run_chunk(ChunkCoord coord) {
#ifndef RPG_AVM
    if(chunk_trace_count[STAGE] < CHUNK_TRACE_CAPACITY)
        chunk_trace[STAGE][chunk_trace_count[STAGE]++] = (uint16_t)coord.y * world_area(coord.area)->chunk_columns + coord.x;
#endif
    switch(coord.area) {
        case 1:
            switch((uint16_t)coord.y * 30 + coord.x) {
                case 124: run_chunk_1_4_4<STAGE>(); break;
                case 154: run_chunk_1_4_5<STAGE>(); break;
                case 156: run_chunk_1_6_5<STAGE>(); break;
                case 181: run_chunk_1_1_6<STAGE>(); break;
                case 183: run_chunk_1_3_6<STAGE>(); break;
                case 185: run_chunk_1_5_6<STAGE>(); break;
                case 271: run_chunk_1_1_9<STAGE>(); break;
                case 273: run_chunk_1_3_9<STAGE>(); break;
                case 275: run_chunk_1_5_9<STAGE>(); break;
                case 276: run_chunk_1_6_9<STAGE>(); break;
                case 331: run_chunk_1_1_11<STAGE>(); break;
                default: break;
            }
            break;
        case 2:
            switch((uint16_t)coord.y * 9 + coord.x) {
                case 9: run_chunk_2_0_1<STAGE>(); break;
                case 12: run_chunk_2_3_1<STAGE>(); break;
                case 14: run_chunk_2_5_1<STAGE>(); break;
                case 16: run_chunk_2_7_1<STAGE>(); break;
                case 39: run_chunk_2_3_4<STAGE>(); break;
                case 43: run_chunk_2_7_4<STAGE>(); break;
                case 45: run_chunk_2_0_5<STAGE>(); break;
                case 50: run_chunk_2_5_5<STAGE>(); break;
                case 63: run_chunk_2_0_7<STAGE>(); break;
                case 75: run_chunk_2_3_8<STAGE>(); break;
                default: break;
            }
            break;
        default: break;
    }
}
template void run_chunk<STAGE_GAMEPLAY>(ChunkCoord);
template void run_chunk<STAGE_TILES>(ChunkCoord);
template void run_chunk<STAGE_DIALOGUE>(ChunkCoord);
