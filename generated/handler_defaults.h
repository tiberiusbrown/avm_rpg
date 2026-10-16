/* Generated from TMX/TSX; do not edit. */

#pragma once
#include "chunk.h"
#include "map_positions.h"
namespace {
inline constexpr TilePos GENERATED_TILE_2_2_0{2, 6, 4};
}
namespace handler_impl {
template <TilePos const& POS, HandlerStage STAGE> struct Tag {};
template <TilePos const& POS> using Gameplay = Tag<POS, STAGE_GAMEPLAY>;
template <TilePos const& POS> using Tiles = Tag<POS, STAGE_TILES>;
template <TilePos const& POS> using Dialogue = Tag<POS, STAGE_DIALOGUE>;
template <TilePos const& POS, HandlerStage STAGE>
void update_bridge(Tag<POS, STAGE>) {}
template <TilePos const& POS, HandlerStage STAGE>
void update_house_door(Tag<POS, STAGE>) {}
template <TilePos const& POS, HandlerStage STAGE>
void update_interior_exit(Tag<POS, STAGE>) {}
template <TilePos const& POS, HandlerStage STAGE>
void update_lantern(Tag<POS, STAGE>) {}
template <TilePos const& POS, HandlerStage STAGE>
void update_north_gate(Tag<POS, STAGE>) {}
template <TilePos const& POS, HandlerStage STAGE>
void update_platform(Tag<POS, STAGE>) {}
template <TilePos const& POS, HandlerStage STAGE>
void update_waystone(Tag<POS, STAGE>) {}
template <TilePos const& POS, HandlerStage STAGE>
void dispatch_update_bridge() {
    update_bridge(Tag<POS, STAGE>{});
}
template <TilePos const& POS, HandlerStage STAGE>
void dispatch_update_house_door() {
    update_house_door(Tag<POS, STAGE>{});
}
template <TilePos const& POS, HandlerStage STAGE>
void dispatch_update_interior_exit() {
    update_interior_exit(Tag<POS, STAGE>{});
}
template <TilePos const& POS, HandlerStage STAGE>
void dispatch_update_lantern() {
    update_lantern(Tag<POS, STAGE>{});
}
template <TilePos const& POS, HandlerStage STAGE>
void dispatch_update_north_gate() {
    update_north_gate(Tag<POS, STAGE>{});
}
template <TilePos const& POS, HandlerStage STAGE>
void dispatch_update_platform() {
    update_platform(Tag<POS, STAGE>{});
}
template <TilePos const& POS, HandlerStage STAGE>
void dispatch_update_waystone() {
    update_waystone(Tag<POS, STAGE>{});
}
}
template <TilePos const& POS, HandlerStage STAGE>
struct Handler {
    void update_bridge() {
        handler_impl::dispatch_update_bridge<POS, STAGE>();
    }
    void update_house_door() {
        handler_impl::dispatch_update_house_door<POS, STAGE>();
    }
    void update_interior_exit() {
        handler_impl::dispatch_update_interior_exit<POS, STAGE>();
    }
    void update_lantern() {
        handler_impl::dispatch_update_lantern<POS, STAGE>();
    }
    void update_north_gate() {
        handler_impl::dispatch_update_north_gate<POS, STAGE>();
    }
    void update_platform() {
        handler_impl::dispatch_update_platform<POS, STAGE>();
    }
    void update_waystone() {
        handler_impl::dispatch_update_waystone<POS, STAGE>();
    }
};
