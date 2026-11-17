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
void enter_house(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void leave_house(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void light_orb(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void open_chest(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void open_door(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void press_plate(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void read_sign(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void show_message(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void show_state(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void steam_vent(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void take_passage(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void update_wanderer(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void use_lever(Tag<POS, STAGE>) {}
template <TilePos POS, HandlerStage STAGE>
void dispatch_enter_house() {
    enter_house(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_leave_house() {
    leave_house(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_light_orb() {
    light_orb(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_open_chest() {
    open_chest(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_open_door() {
    open_door(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_press_plate() {
    press_plate(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_read_sign() {
    read_sign(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_show_message() {
    show_message(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_show_state() {
    show_state(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_steam_vent() {
    steam_vent(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_take_passage() {
    take_passage(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_update_wanderer() {
    update_wanderer(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_use_lever() {
    use_lever(Tag<POS, STAGE>{});
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
    void light_orb() {
        handler_impl::dispatch_light_orb<POS, STAGE>();
    }
    void open_chest() {
        handler_impl::dispatch_open_chest<POS, STAGE>();
    }
    void open_door() {
        handler_impl::dispatch_open_door<POS, STAGE>();
    }
    void press_plate() {
        handler_impl::dispatch_press_plate<POS, STAGE>();
    }
    void read_sign() {
        handler_impl::dispatch_read_sign<POS, STAGE>();
    }
    void show_message() {
        handler_impl::dispatch_show_message<POS, STAGE>();
    }
    void show_state() {
        handler_impl::dispatch_show_state<POS, STAGE>();
    }
    void steam_vent() {
        handler_impl::dispatch_steam_vent<POS, STAGE>();
    }
    void take_passage() {
        handler_impl::dispatch_take_passage<POS, STAGE>();
    }
    void update_wanderer() {
        handler_impl::dispatch_update_wanderer<POS, STAGE>();
    }
    void use_lever() {
        handler_impl::dispatch_use_lever<POS, STAGE>();
    }
};
