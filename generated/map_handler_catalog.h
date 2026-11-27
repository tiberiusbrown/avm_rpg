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
struct MapOperation_show_message {
    template <TilePos POS, HandlerStage STAGE> static constexpr bool implemented =
        handler_impl::implemented_show_message<POS, STAGE>();
    template <TilePos POS, HandlerStage STAGE> static void run() {
        Handler<POS, STAGE>{}.show_message();
    }
};
using AllMapHandlers = TypeList<
    MapSite<positions::AldenDoor, MapOperation_enter_house>,
    MapSite<positions::AldenSign, MapOperation_show_message>,
    MapSite<positions::BarnDoor, MapOperation_show_message>,
    MapSite<positions::BarnSign, MapOperation_show_message>,
    MapSite<positions::BramDoor, MapOperation_show_message>,
    MapSite<positions::BramSign, MapOperation_show_message>,
    MapSite<positions::ElizaDoor, MapOperation_enter_house>,
    MapSite<positions::ElizaSign, MapOperation_show_message>,
    MapSite<positions::InnDoor, MapOperation_enter_house>,
    MapSite<positions::InnSign, MapOperation_show_message>,
    MapSite<positions::NorthHouseDoor, MapOperation_show_message>,
    MapSite<positions::NorthHouseSign, MapOperation_show_message>,
    MapSite<positions::ShopDoor, MapOperation_enter_house>,
    MapSite<positions::ShopSign, MapOperation_show_message>,
    MapSite<positions::SouthHouseDoor, MapOperation_show_message>,
    MapSite<positions::SouthHouseSign, MapOperation_show_message>,
    MapSite<positions::AldenExit, MapOperation_leave_house>,
    MapSite<positions::ElizaExit, MapOperation_leave_house>,
    MapSite<positions::InnExit, MapOperation_leave_house>,
    MapSite<positions::ShopExit, MapOperation_leave_house>
>;
