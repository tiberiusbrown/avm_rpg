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
EmptyHandler light_orb(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler maintenance_hatch(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler open_chest(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler open_door(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler open_shop(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler press_plate(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler read_bridge_notice(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler read_mine_ledger(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler read_wolf_warning(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler rest_at_inn(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler show_message(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler show_state(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler steam_vent(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler take_passage(Tag<POS, STAGE>) { return {}; }
template <TilePos POS, HandlerStage STAGE>
EmptyHandler use_lever(Tag<POS, STAGE>) { return {}; }
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
constexpr bool implemented_light_orb() {
    return authored_handler<decltype(light_orb(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_light_orb() {
    light_orb(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_maintenance_hatch() {
    return authored_handler<decltype(maintenance_hatch(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_maintenance_hatch() {
    maintenance_hatch(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_open_chest() {
    return authored_handler<decltype(open_chest(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_open_chest() {
    open_chest(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_open_door() {
    return authored_handler<decltype(open_door(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_open_door() {
    open_door(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_open_shop() {
    return authored_handler<decltype(open_shop(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_open_shop() {
    open_shop(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_press_plate() {
    return authored_handler<decltype(press_plate(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_press_plate() {
    press_plate(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_read_bridge_notice() {
    return authored_handler<decltype(read_bridge_notice(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_read_bridge_notice() {
    read_bridge_notice(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_read_mine_ledger() {
    return authored_handler<decltype(read_mine_ledger(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_read_mine_ledger() {
    read_mine_ledger(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_read_wolf_warning() {
    return authored_handler<decltype(read_wolf_warning(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_read_wolf_warning() {
    read_wolf_warning(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_rest_at_inn() {
    return authored_handler<decltype(rest_at_inn(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_rest_at_inn() {
    rest_at_inn(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_show_message() {
    return authored_handler<decltype(show_message(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_show_message() {
    show_message(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_show_state() {
    return authored_handler<decltype(show_state(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_show_state() {
    show_state(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_steam_vent() {
    return authored_handler<decltype(steam_vent(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_steam_vent() {
    steam_vent(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_take_passage() {
    return authored_handler<decltype(take_passage(Tag<POS, STAGE>{}))>;
}
template <TilePos POS, HandlerStage STAGE>
void dispatch_take_passage() {
    take_passage(Tag<POS, STAGE>{});
}
template <TilePos POS, HandlerStage STAGE>
constexpr bool implemented_use_lever() {
    return authored_handler<decltype(use_lever(Tag<POS, STAGE>{}))>;
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
    void maintenance_hatch() {
        handler_impl::dispatch_maintenance_hatch<POS, STAGE>();
    }
    void open_chest() {
        handler_impl::dispatch_open_chest<POS, STAGE>();
    }
    void open_door() {
        handler_impl::dispatch_open_door<POS, STAGE>();
    }
    void open_shop() {
        handler_impl::dispatch_open_shop<POS, STAGE>();
    }
    void press_plate() {
        handler_impl::dispatch_press_plate<POS, STAGE>();
    }
    void read_bridge_notice() {
        handler_impl::dispatch_read_bridge_notice<POS, STAGE>();
    }
    void read_mine_ledger() {
        handler_impl::dispatch_read_mine_ledger<POS, STAGE>();
    }
    void read_wolf_warning() {
        handler_impl::dispatch_read_wolf_warning<POS, STAGE>();
    }
    void rest_at_inn() {
        handler_impl::dispatch_rest_at_inn<POS, STAGE>();
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
    void use_lever() {
        handler_impl::dispatch_use_lever<POS, STAGE>();
    }
};
