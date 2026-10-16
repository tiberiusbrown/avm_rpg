/* Generated from TMX/TSX; do not edit. */

#pragma once
#include "chunk.h"
#include "map_positions.h"
namespace {
inline constexpr TilePos GENERATED_TILE_2_2_0{2, 6, 4};
}
namespace handler_impl {
template <TilePos POS, HandlerStage STAGE> struct Tag {};
template <TilePos POS> using Gameplay = Tag<POS, STAGE_GAMEPLAY>;
template <TilePos POS> using Tiles = Tag<POS, STAGE_TILES>;
template <TilePos POS> using Dialogue = Tag<POS, STAGE_DIALOGUE>;
template <TilePos POS, HandlerStage STAGE>
void update_bridge(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_castle_guard(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_courier(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_guide(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_door(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_exit(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_npc_1(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_npc_2(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_npc_3(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_npc_4(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_npc_5(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_npc_6(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_npc_7(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_hall_npc_8(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_house_door(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_interior_exit(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_lantern(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_north_gate(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_platform(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_rowan(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_village_guard(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_waystone(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_bridge() {
    update_bridge(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_castle_guard() {
    update_castle_guard(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_courier() {
    update_courier(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_guide() {
    update_guide(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_door() {
    update_hall_door(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_exit() {
    update_hall_exit(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_npc_1() {
    update_hall_npc_1(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_npc_2() {
    update_hall_npc_2(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_npc_3() {
    update_hall_npc_3(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_npc_4() {
    update_hall_npc_4(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_npc_5() {
    update_hall_npc_5(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_npc_6() {
    update_hall_npc_6(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_npc_7() {
    update_hall_npc_7(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_hall_npc_8() {
    update_hall_npc_8(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_house_door() {
    update_house_door(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_interior_exit() {
    update_interior_exit(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_lantern() {
    update_lantern(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_north_gate() {
    update_north_gate(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_platform() {
    update_platform(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_rowan() {
    update_rowan(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_village_guard() {
    update_village_guard(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_waystone() {
    update_waystone(Tag<POS, STAGE>{});
}
}
template <TilePos POS, HandlerStage STAGE>
struct Handler {
    void update_bridge() {
        handler_impl::dispatch_update_bridge<POS, STAGE>();
    }
    void update_castle_guard() {
        handler_impl::dispatch_update_castle_guard<POS, STAGE>();
    }
    void update_courier() {
        handler_impl::dispatch_update_courier<POS, STAGE>();
    }
    void update_guide() {
        handler_impl::dispatch_update_guide<POS, STAGE>();
    }
    void update_hall_door() {
        handler_impl::dispatch_update_hall_door<POS, STAGE>();
    }
    void update_hall_exit() {
        handler_impl::dispatch_update_hall_exit<POS, STAGE>();
    }
    void update_hall_npc_1() {
        handler_impl::dispatch_update_hall_npc_1<POS, STAGE>();
    }
    void update_hall_npc_2() {
        handler_impl::dispatch_update_hall_npc_2<POS, STAGE>();
    }
    void update_hall_npc_3() {
        handler_impl::dispatch_update_hall_npc_3<POS, STAGE>();
    }
    void update_hall_npc_4() {
        handler_impl::dispatch_update_hall_npc_4<POS, STAGE>();
    }
    void update_hall_npc_5() {
        handler_impl::dispatch_update_hall_npc_5<POS, STAGE>();
    }
    void update_hall_npc_6() {
        handler_impl::dispatch_update_hall_npc_6<POS, STAGE>();
    }
    void update_hall_npc_7() {
        handler_impl::dispatch_update_hall_npc_7<POS, STAGE>();
    }
    void update_hall_npc_8() {
        handler_impl::dispatch_update_hall_npc_8<POS, STAGE>();
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
    void update_rowan() {
        handler_impl::dispatch_update_rowan<POS, STAGE>();
    }
    void update_village_guard() {
        handler_impl::dispatch_update_village_guard<POS, STAGE>();
    }
    void update_waystone() {
        handler_impl::dispatch_update_waystone<POS, STAGE>();
    }
};
