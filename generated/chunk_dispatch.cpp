/* Generated from TMX/TSX; do not edit. */

#include "logic/handlers.h"
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_3_0() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveMouth, STAGE>{}.take_passage();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_1_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveHole, STAGE>{}.take_passage();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_3_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveSign, STAGE>{}.read_notice();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_4_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::MineDoor, STAGE>{}.take_passage();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_4_4() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::WeaverDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_4_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::ElderDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_6_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::BakeryDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_1_6() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CottageDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_3_6() {
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
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_5_6() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::ShopDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_1_9() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::FarmDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_3_9() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::SmithyDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_5_9() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::InnDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_6_9() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::HutDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_1_1_11() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::FisherDoor, STAGE>{}.enter_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_0_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::ElderExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_3_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CottageExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_5_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::WeaverExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_7_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::ShopExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_3_4() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::FarmExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_7_4() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::SmithyExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_0_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::BakeryExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_5_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::InnExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_0_7() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::HutExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_2_3_8() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::FisherExit, STAGE>{}.leave_house();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_1_0() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveLadder, STAGE>{}.take_passage();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_2_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveBridge_0, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveBridge_1, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_3_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveChest, STAGE>{}.open_chest();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_2_2() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveVent, STAGE>{}.steam_vent();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_4_2() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveVaultLever, STAGE>{}.use_lever();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_0_3() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CavePlate, STAGE>{}.press_plate();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_1_3() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveNookGate, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_3_3() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveGate, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveLever, STAGE>{}.use_lever();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_3_2_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveExit, STAGE>{}.take_passage();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::CaveNotice, STAGE>{}.read_notice();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_2_0() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonChest, STAGE>{}.open_chest();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_2_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonRuneWest, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonVaultDoor, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_3_1() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonRuneEast, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_0_2() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonPlate, STAGE>{}.press_plate();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_2_2() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonBlocks_0, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonBlocks_1, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonOrb, STAGE>{}.light_orb();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_0_3() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonCellDoor, STAGE>{}.open_door();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_3_3() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonGate, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_2_4() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonSpikes_0, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_3_4() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonBlockLever, STAGE>{}.use_lever();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_2_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonPlaque, STAGE>{}.read_notice();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonSpikes_1, STAGE>{}.show_state();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_3_5() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonLever, STAGE>{}.use_lever();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE static void run_chunk_4_2_6() {
#ifndef RPG_AVM
    ++feature_trace_count[STAGE];
#endif
    Handler<positions::DungeonExit, STAGE>{}.take_passage();
    if constexpr(STAGE != STAGE_TILES) {
        if(teleported)
            return;
    }
}
template <HandlerStage STAGE> RPG_NOINLINE void run_chunk(uint8_t area, uint8_t x, uint8_t y) {
#ifndef RPG_AVM
    if(chunk_trace_count[STAGE] < CHUNK_TRACE_CAPACITY)
        chunk_trace[STAGE][chunk_trace_count[STAGE]++] = (uint16_t)y * world_area(area)->chunk_columns + x;
#endif
    switch(area) {
        case 1:
            switch((uint16_t)y * 30 + x) {
                case 3: run_chunk_1_3_0<STAGE>(); break;
                case 31: run_chunk_1_1_1<STAGE>(); break;
                case 33: run_chunk_1_3_1<STAGE>(); break;
                case 34: run_chunk_1_4_1<STAGE>(); break;
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
            switch((uint16_t)y * 9 + x) {
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
        case 3:
            switch((uint16_t)y * 5 + x) {
                case 1: run_chunk_3_1_0<STAGE>(); break;
                case 7: run_chunk_3_2_1<STAGE>(); break;
                case 8: run_chunk_3_3_1<STAGE>(); break;
                case 12: run_chunk_3_2_2<STAGE>(); break;
                case 14: run_chunk_3_4_2<STAGE>(); break;
                case 15: run_chunk_3_0_3<STAGE>(); break;
                case 16: run_chunk_3_1_3<STAGE>(); break;
                case 18: run_chunk_3_3_3<STAGE>(); break;
                case 27: run_chunk_3_2_5<STAGE>(); break;
                default: break;
            }
            break;
        case 4:
            switch((uint16_t)y * 6 + x) {
                case 2: run_chunk_4_2_0<STAGE>(); break;
                case 8: run_chunk_4_2_1<STAGE>(); break;
                case 9: run_chunk_4_3_1<STAGE>(); break;
                case 12: run_chunk_4_0_2<STAGE>(); break;
                case 14: run_chunk_4_2_2<STAGE>(); break;
                case 18: run_chunk_4_0_3<STAGE>(); break;
                case 21: run_chunk_4_3_3<STAGE>(); break;
                case 26: run_chunk_4_2_4<STAGE>(); break;
                case 27: run_chunk_4_3_4<STAGE>(); break;
                case 32: run_chunk_4_2_5<STAGE>(); break;
                case 33: run_chunk_4_3_5<STAGE>(); break;
                case 38: run_chunk_4_2_6<STAGE>(); break;
                default: break;
            }
            break;
        default: break;
    }
}
template void run_chunk<STAGE_GAMEPLAY>(uint8_t, uint8_t, uint8_t);
template void run_chunk<STAGE_TILES>(uint8_t, uint8_t, uint8_t);
template void run_chunk<STAGE_DIALOGUE>(uint8_t, uint8_t, uint8_t);
