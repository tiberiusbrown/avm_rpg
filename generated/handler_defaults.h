/* Generated from TMX/TSX; do not edit. */

#pragma once
#include "chunk.h"
#include "map_positions.h"
namespace handler_impl {
template <TilePos POS, HandlerStage STAGE> struct Tag {};
template <TilePos POS> using Gameplay = Tag<POS, STAGE_GAMEPLAY>;
template <TilePos POS> using Tiles = Tag<POS, STAGE_TILES>;
template <TilePos POS> using Dialogue = Tag<POS, STAGE_DIALOGUE>;
template <TilePos POS, HandlerStage STAGE>
void read_sign(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_villager(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_wanderer(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void dispatch_read_sign() {
    read_sign(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_villager() {
    update_villager(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_wanderer() {
    update_wanderer(Tag<POS, STAGE>{});
}
}
template <TilePos POS, HandlerStage STAGE>
struct Handler {
    void read_sign() {
        handler_impl::dispatch_read_sign<POS, STAGE>();
    }
    void update_villager() {
        handler_impl::dispatch_update_villager<POS, STAGE>();
    }
    void update_wanderer() {
        handler_impl::dispatch_update_wanderer<POS, STAGE>();
    }
};
