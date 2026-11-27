/* Generated from TMX/TSX; do not edit. */

#pragma once
#include "chunk.h"
#include "map_positions.h"
namespace handler_impl {
struct EmptyHandler {};
template <typename T> inline constexpr bool authored_handler = false;
template <> inline constexpr bool authored_handler<void> = true;
template <TilePos POS, HandlerStage STAGE> struct Tag {};
template <TilePos POS> using Gameplay = Tag<POS, STAGE_GAMEPLAY>;
template <TilePos POS> using Tiles = Tag<POS, STAGE_TILES>;
template <TilePos POS> using Dialogue = Tag<POS, STAGE_DIALOGUE>;
template <TilePos POS, HandlerStage STAGE>
EmptyHandler enter_house(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler leave_house(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler show_message(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_enter_house() {
    return authored_handler<decltype(enter_house(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_enter_house() {
    enter_house(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_leave_house() {
    return authored_handler<decltype(leave_house(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_leave_house() {
    leave_house(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_show_message() {
    return authored_handler<decltype(show_message(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_show_message() {
    show_message(Tag<POS, STAGE>{});
}
}
template <TilePos POS, HandlerStage STAGE>
struct Handler {
    void enter_house() {
        handler_impl::dispatch_enter_house<POS, STAGE>();
    }
    void leave_house() {
        handler_impl::dispatch_leave_house<POS, STAGE>();
    }
    void show_message() {
        handler_impl::dispatch_show_message<POS, STAGE>();
    }
};
