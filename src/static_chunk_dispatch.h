#ifndef STATIC_CHUNK_DISPATCH_H
#define STATIC_CHUNK_DISPATCH_H

#include "chunk.h"
#include "world_assets.h"
#include <utility>
#include <type_traits>

template <typename... T> struct TypeList {};

template <TilePos POS, typename OP> struct MapSite {
    static constexpr TilePos position = POS;
    template <HandlerStage STAGE>
    static constexpr bool implemented = OP::template implemented<POS, STAGE>;

    template <HandlerStage STAGE> static void run() {
#ifndef RPG_AVM
        ++feature_trace_count[STAGE];
#endif
        OP::template run<POS, STAGE>();
    }
};

constexpr bool valid_site(TilePos pos) {
    for(auto area : STATIC_AREAS)
        if(area.id == pos.area)
            return pos.x < area.width && pos.y < area.height;
    return false;
}

template <uint8_t AREA, uint8_t X, uint8_t Y, typename SITE>
inline constexpr bool site_in_chunk =
    SITE::position.area == AREA && SITE::position.x / CHUNK_WIDTH == X &&
    SITE::position.y / CHUNK_HEIGHT == Y;

template <uint8_t AREA, uint8_t X, uint8_t Y, HandlerStage STAGE, typename LIST> struct ChunkSites;

template <uint8_t AREA, uint8_t X, uint8_t Y, HandlerStage STAGE, typename... SITES>
struct ChunkSites<AREA, X, Y, STAGE, TypeList<SITES...>> {
    static constexpr bool occupied =
        ((site_in_chunk<AREA, X, Y, SITES> && SITES::template implemented<STAGE>) || ...);

    template <typename SITE> static void one() {
        static_assert(valid_site(SITE::position), "site outside generated map dimensions");
        if constexpr(site_in_chunk<AREA, X, Y, SITE> && SITE::template implemented<STAGE>) {
            if constexpr(STAGE == STAGE_GAMEPLAY || STAGE == STAGE_DIALOGUE)
                if(teleported)
                    return;
            SITE::template run<STAGE>();
        }
    }

    static void run() {
        (one<SITES>(), ...);
    }
};

#endif
