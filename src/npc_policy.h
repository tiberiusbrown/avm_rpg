#ifndef NPC_POLICY_H
#define NPC_POLICY_H

#include "static_chunk_dispatch.h"
#include "map_positions.h"

inline constexpr TilePos NO_LOCATION{0, 0, 0};

template <TilePos POS, auto PREDICATE> struct When {
    static constexpr TilePos position = POS;

    static bool applies() {
        return PREDICATE();
    }
};

template <typename> inline constexpr bool is_when = false;
template <TilePos POS, auto PREDICATE> inline constexpr bool is_when<When<POS, PREDICATE>> = true;

template <TilePos DEFAULT, typename... CASES> struct AtOneOf {
    using location_policy = AtOneOf;
    static_assert((is_when<CASES> && ...), "unsupported NPC policy case; use When");
    static_assert((requires {
                      CASES::position;
                      CASES::applies();
                  } && ...), "location cases must provide a position and predicate");
    static constexpr TilePos locations[] = {DEFAULT, CASES::position...};

    static consteval bool valid() {
        for(unsigned i = 0; i < sizeof...(CASES) + 1; ++i) {
            if(locations[i] != NO_LOCATION && !valid_site(locations[i]))
                return false;
            for(unsigned j = 0; j < i; ++j)
                if(locations[i] == locations[j])
                    return false;
        }
        return true;
    }

    static_assert(valid(), "invalid or duplicate NPC policy location");

    static TilePos current() {
        TilePos result = DEFAULT;
        (void)((CASES::applies() ? (result = CASES::position, true) : false) || ...);
        return result;
    }
};

template <NPC_ID ID> struct NpcPolicy;

template <typename> inline constexpr bool is_location_policy = false;
template <TilePos DEFAULT, typename... CASES>
inline constexpr bool is_location_policy<AtOneOf<DEFAULT, CASES...>> = true;

template <NPC_ID ID> struct CheckedNpcPolicy {
    static_assert(
        requires { typename NpcPolicy<ID>::location_policy; },
        "every NPC identity must inherit an AtOneOf policy");
    using type = typename NpcPolicy<ID>::location_policy;
    static_assert(is_location_policy<type> && std::is_base_of_v<type, NpcPolicy<ID>>,
                  "unsupported NPC policy structure; inherit AtOneOf");
    static_assert(&NpcPolicy<ID>::current == &type::current,
                  "do not override the derived location resolver");
    static_assert(&NpcPolicy<ID>::locations == &type::locations,
                  "do not override the derived site catalog");
};

template <NPC_ID ID> using LocationPolicy = typename CheckedNpcPolicy<ID>::type;

template <NPC_ID ID> TilePos npc_location() {
    static_assert(ID > NPC_NONE && ID < NPC_ID_LIMIT, "invalid NPC identity");
    static_assert(
        requires {
            NpcPolicy<ID>::current();
            NpcPolicy<ID>::locations;
        }, "every NPC identity requires an NpcPolicy specialization");
    using Policy = LocationPolicy<ID>;
    return Policy::current();
}

template <NPC_ID ID, TilePos POS> struct NpcSite {
    static constexpr TilePos position = POS;
    template <HandlerStage> static constexpr bool implemented = true;

    template <HandlerStage> static void run() {
#ifndef RPG_AVM
        ++npc_site_checks;
#endif
        if(!party_contains(ID) && !npc_find(ID) && npc_location<ID>() == POS)
            npc_offer_candidate(ID, POS);
    }
};

template <typename... LISTS> struct Join;

template <> struct Join<> {
    using type = TypeList<>;
};

template <typename... T> struct Join<TypeList<T...>> {
    using type = TypeList<T...>;
};

template <typename... A, typename... B, typename... REST>
struct Join<TypeList<A...>, TypeList<B...>, REST...> : Join<TypeList<A..., B...>, REST...> {};

template <NPC_ID ID, typename SEQUENCE> struct PolicySites;

template <NPC_ID ID, std::size_t... I> struct PolicySites<ID, std::index_sequence<I...>> {
    using Policy = LocationPolicy<ID>;
    using type = typename Join<
        typename Join<TypeList<>>::type,
        typename std::conditional<Policy::locations[I] == NO_LOCATION, TypeList<>,
                                  TypeList<NpcSite<ID, Policy::locations[I]>>>::type...>::type;
};

template <typename SEQUENCE> struct PolicyCatalog;

template <std::size_t... I> struct PolicyCatalog<std::index_sequence<I...>> {
    static_assert((requires { NpcPolicy<(NPC_ID)(I + 1)>::locations; } && ...),
                  "missing NpcPolicy specialization in contiguous NPC identity range");
    using sites = typename Join<typename PolicySites<
        (NPC_ID)(I + 1),
        std::make_index_sequence<sizeof(LocationPolicy<(NPC_ID)(I + 1)>::locations) /
                                 sizeof(TilePos)>>::type...>::type;
};

#endif
