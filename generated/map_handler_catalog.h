/* Generated from TMX/TSX; do not edit. */

#pragma once
#include "logic/handlers.h"
#include "static_chunk_dispatch.h"
struct MapOperation_enter_house {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_enter_house<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.enter_house();
    }
};
struct MapOperation_leave_house {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_leave_house<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.leave_house();
    }
};
struct MapOperation_light_orb {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_light_orb<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.light_orb();
    }
};
struct MapOperation_open_chest {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_open_chest<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.open_chest();
    }
};
struct MapOperation_open_door {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_open_door<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.open_door();
    }
};
struct MapOperation_open_shop {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_open_shop<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.open_shop();
    }
};
struct MapOperation_press_plate {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_press_plate<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.press_plate();
    }
};
struct MapOperation_read_sign {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_read_sign<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.read_sign();
    }
};
struct MapOperation_rest_at_inn {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_rest_at_inn<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.rest_at_inn();
    }
};
struct MapOperation_show_message {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_show_message<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.show_message();
    }
};
struct MapOperation_show_state {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_show_state<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.show_state();
    }
};
struct MapOperation_steam_vent {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_steam_vent<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.steam_vent();
    }
};
struct MapOperation_take_passage {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_take_passage<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.take_passage();
    }
};
struct MapOperation_update_wanderer {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_update_wanderer<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.update_wanderer();
    }
};
struct MapOperation_use_lever {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_use_lever<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.use_lever();
    }
};
using AllMapHandlers = TypeList<
    MapSite<positions::BakeryDoor, MapOperation_enter_house>,
    MapSite<positions::CaveHole, MapOperation_take_passage>,
    MapSite<positions::CaveMouth, MapOperation_take_passage>,
    MapSite<positions::CaveSign, MapOperation_show_message>,
    MapSite<positions::CottageDoor, MapOperation_enter_house>,
    MapSite<positions::ElderDoor, MapOperation_enter_house>,
    MapSite<positions::FarmDoor, MapOperation_enter_house>,
    MapSite<positions::FisherDoor, MapOperation_enter_house>,
    MapSite<positions::GroveSign, MapOperation_read_sign>,
    MapSite<positions::HutDoor, MapOperation_enter_house>,
    MapSite<positions::InnDoor, MapOperation_enter_house>,
    MapSite<positions::MineEntrance, MapOperation_take_passage>,
    MapSite<positions::ShopDoor, MapOperation_enter_house>,
    MapSite<positions::SmithyDoor, MapOperation_enter_house>,
    MapSite<positions::WandererHome, MapOperation_update_wanderer>,
    MapSite<positions::WeaverDoor, MapOperation_enter_house>,
    MapSite<positions::BakeryExit, MapOperation_leave_house>,
    MapSite<positions::CottageExit, MapOperation_leave_house>,
    MapSite<positions::ElderExit, MapOperation_leave_house>,
    MapSite<positions::FarmExit, MapOperation_leave_house>,
    MapSite<positions::FisherExit, MapOperation_leave_house>,
    MapSite<positions::HutExit, MapOperation_leave_house>,
    MapSite<positions::InnExit, MapOperation_leave_house>,
    MapSite<positions::InnRest, MapOperation_rest_at_inn>,
    MapSite<positions::ShopCounter, MapOperation_open_shop>,
    MapSite<positions::ShopExit, MapOperation_leave_house>,
    MapSite<positions::SmithyExit, MapOperation_leave_house>,
    MapSite<positions::WeaverExit, MapOperation_leave_house>,
    MapSite<positions::CaveBridge_0, MapOperation_show_state>,
    MapSite<positions::CaveBridge_1, MapOperation_show_state>,
    MapSite<positions::CaveChest, MapOperation_open_chest>,
    MapSite<positions::CaveExit, MapOperation_take_passage>,
    MapSite<positions::CaveGate, MapOperation_show_state>,
    MapSite<positions::CaveLadder, MapOperation_take_passage>,
    MapSite<positions::CaveLever, MapOperation_use_lever>,
    MapSite<positions::CaveNookGate, MapOperation_show_state>,
    MapSite<positions::CaveNotice, MapOperation_show_message>,
    MapSite<positions::CavePlate, MapOperation_press_plate>,
    MapSite<positions::CaveVaultLever, MapOperation_use_lever>,
    MapSite<positions::CaveVent, MapOperation_steam_vent>,
    MapSite<positions::DungeonBlockLever, MapOperation_use_lever>,
    MapSite<positions::DungeonBlocks_0, MapOperation_show_state>,
    MapSite<positions::DungeonBlocks_1, MapOperation_show_state>,
    MapSite<positions::DungeonCellDoor, MapOperation_open_door>,
    MapSite<positions::DungeonChest, MapOperation_open_chest>,
    MapSite<positions::DungeonExit, MapOperation_take_passage>,
    MapSite<positions::DungeonGate, MapOperation_show_state>,
    MapSite<positions::DungeonLever, MapOperation_use_lever>,
    MapSite<positions::DungeonOrb, MapOperation_light_orb>,
    MapSite<positions::DungeonPlaque, MapOperation_show_message>,
    MapSite<positions::DungeonPlate, MapOperation_press_plate>,
    MapSite<positions::DungeonRuneEast, MapOperation_show_state>,
    MapSite<positions::DungeonRuneWest, MapOperation_show_state>,
    MapSite<positions::DungeonSpikes_0, MapOperation_show_state>,
    MapSite<positions::DungeonSpikes_1, MapOperation_show_state>,
    MapSite<positions::DungeonVaultDoor, MapOperation_show_state>
>;
