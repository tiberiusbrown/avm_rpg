# Tiled authoring and compile-time tile handlers

Install Python dependencies with `python -m pip install -r tools/requirements.txt`.
Tiled is needed for visual editing, not regeneration. The exporter uses XML and
Pillow for existing monochrome PNG atlases.

## Maps and tiles

Open `assets/maps/outdoor.tmx` or `interior.tmx`. Use finite orthogonal right-down
maps, 8x8 tiles and uncompressed CSV layers. Layers flatten in document order;
zero GIDs are transparent and empty cells become impassable sentinel tiles.
Logical dimensions are at most **240x248**, with optional chunk alignment.

Each map has an explicit `area_id` (1..255), unique C++ `symbol`, and `spawn`
referencing a single named tile in that area. Preserve area IDs after shipping.
Area constants use AREA_ (`AREA_OUTDOOR`, `AREA_INTERIOR`). Safe spawns must have
passable baseline terrain and no native handler. Player spawns use the
tile's pixel origin, so tile (4,5) places an 8x8 sprite at pixel (32,40).

Use external `assets/tilesets/world.tsx`. Atlas tiles declare stable dense
runtime_id (0..254), symbol (WALL -> TILE_WALL), and bool solid/portal metadata.
Portal metadata describes art; native chunk handlers determine teleport behavior.
The atlas is opaque black/white 8x8 art in `assets_src/tiles.png`. Add frames and
metadata together; export remaps local/firstgid references into runtime-ID order.

Infinite/compressed maps, rotated/flipped GIDs, inline tilesets, tile animations,
offsets, parallax and hidden/translucent layers are unsupported.

## Uniform point and rectangle objects

Put objects in a Logic or Locations layer. Objects have **no type or class**.
All exported positions are TilePos values containing byte area/X/Y coordinates.
The name is optional; when present it must be a globally unique C++ identifier.
The only optional custom property is `handler`, naming a native function implemented
in `src/logic/handlers.h`. There are no destination, stable_id, facing, interaction
or interaction_radius properties. An unnamed object with no handler emits no code.

Place point markers at pixel (8*tx+4, 8*ty+4). Export selects floor(x/8), floor(y/8)
regardless of alignment. Rectangles may cover part of a tile. Every tile with
positive-area overlap is included: floor(left/8) through ceil(right/8)-1, likewise
vertically. Right/bottom edges are exclusive. Exact decimal arithmetic handles
fractional boundaries without floating-point edge rounding.

A rectangle becomes independent tile points in row-major order. Named objects
covering one tile export `positions::Name`; multi-tile objects export
`positions::Name_0`, `Name_1`, etc. EasternBridge exports six constants for tiles
(15..17,7..8). Each point binds the handler to that tile's own chunk. There is no
runtime rectangle, ownership calculation, region clipping loop or deduplication.

Named rectangles also export `positions::NameRegion` for optional queries:

```cpp
static_assert(positions::EasternBridgeRegion.contains({AREA_OUTDOOR, 17, 8}));
bool near_bridge = game.player_is_in_region(positions::EasternBridgeRegion);
```

Region data has area, origin and byte width/height. Contains checks matching area
and exclusive right/bottom edges. It is query metadata; it does not control
handler dispatch. The generated names, including suffixes and Region, must be unique.

Unnamed objects bind handlers through private constexpr tile constants in the
generated handler-default header's anonymous namespace. No public location names or region constants
are emitted for them. The house lantern is a working handler-only unnamed object.
Private identifiers depend on authoring order and are never persistent identities.

## Generated defaults and tag overloads

`HandlerStage` is defined in chunk.h with STAGE_GAMEPLAY, STAGE_TILES and
STAGE_DIALOGUE. The exporter writes generated/handler_defaults.h with shared tags
and the following infrastructure for each distinct map handler name:

```cpp
namespace handler_impl {

template <TilePos const& POS, HandlerStage STAGE>
struct Tag {};

template <TilePos const& POS>
using Gameplay = Tag<POS, STAGE_GAMEPLAY>;

template <TilePos const& POS>
using Tiles = Tag<POS, STAGE_TILES>;

template <TilePos const& POS>
using Dialogue = Tag<POS, STAGE_DIALOGUE>;

template <TilePos const& POS, HandlerStage STAGE>
void update_north_gate(Tag<POS, STAGE>) {}

template <TilePos const& POS, HandlerStage STAGE>
void dispatch_update_north_gate() {
    update_north_gate(Tag<POS, STAGE>{});
}

}

template <TilePos const& POS, HandlerStage STAGE>
struct Handler {
    void update_north_gate() {
        handler_impl::dispatch_update_north_gate<POS, STAGE>();
    }
};
```

Add ordinary overloads in src/logic/handlers.h after including the generated
defaults. One stage overload can serve every position using that handler name:

```cpp
namespace handler_impl {

template <TilePos const& POS>
void update_north_gate(Tiles<POS>) {
    if(game.get_flag(FLAG_GATE_OPEN))
        set_tile<POS>(TILE_PATH);
}

template <TilePos const& POS>
void update_north_gate(Dialogue<POS>) {
    if(player_just_interacted<POS>() &&
       dialogue_ask(F("Toggle the north gate?")))
        game.toggle_flag(FLAG_GATE_OPEN);
}

}
```

Generated calls retain `Handler<positions::NorthGate, STAGE>{}.update_north_gate()`.
The helper's unqualified dependent call uses argument-dependent lookup (ADL):
Tag belongs to handler_impl, so overloads in that namespace participate even when
declared after the helper template. Do not qualify the handler call inside the
helper. Human overloads must be visible before generated chunk dispatch is
instantiated. Full LTO removes empty defaults, tags, temporary Handler objects and
constexpr position storage; there are no virtual calls or runtime stage tests.

Overloads can select a stage, a position, or an exact pair:

```cpp
namespace handler_impl {

// This position at every stage.
template <HandlerStage STAGE>
void update_custom(Tag<positions::NorthGate, STAGE>) { /* ... */ }

// This exact position/stage; non-template overloads in headers need inline.
inline void update_custom(Tiles<positions::NorthGate>) { /* ... */ }

}
```

An exact overload takes precedence over matching templates. A position-wide and
a stage-wide template can be ambiguous where they intersect; provide an exact
overload for that pair. Generic stage overloads reuse one body across all six
bridge cells and work with private positions for unnamed objects. Header-defined
function templates need no explicit specialization or `inline` boilerplate.

New handler names require no handwritten implementation or declaration: export
adds their empty fallback, helper and facade method automatically. The exporter
validates C++ identifiers and reserves infrastructure names. It does not read or
scan implementation headers. Combinations without a matching overload use the
generic empty default. Unnamed objects have private constants in the generated
header's anonymous namespace; generic overloads need not name those constants.

One `run_chunk<STAGE>(ChunkCoord)` dispatcher instantiates the same ordered call
list for all three stages. The outer loop is over stages: complete every
`process_all_chunks<STAGE_GAMEPLAY>()` call, reload the baseline, complete
`process_all_chunks<STAGE_TILES>()`, repair player overlap, then complete
`process_all_chunks<STAGE_DIALOGUE>()`. Tile overloads may read gameplay flags/time
and write through `set_tile<POS>()`; they must not mutate gameplay state. Blocking
dialogue belongs in Dialogue, after the complete cache is available for drawing.
Terrain flag changes during Dialogue become visible on the next tile pass;
`refresh_tile_cache()` explicitly reloads the baseline and completes another tile
pass if immediate refresh is needed. There is no WorldState snapshot,
TileContext or ChunkContext. Reads stay cache-only.

Each covered tile runs once per live stage, with objects ordered by name and then
unnamed authoring order; expanded cells remain row-major. Overlapping objects use
that same order. Keep persistent guards for shared rewards: the two waystone cells
share FLAG_WAYSTONE. Fresh A is consumed once. Safe spawn must be on passable
baseline terrain outside handler objects. Native tests compile an independently
exported dispatch containing new named, repeated and unnamed unimplemented handlers.

## Input and teleport helpers

All tile helpers accept the tile as a template argument:

```cpp
namespace handler_impl {

template <TilePos const& POS>
void update_house_door(Gameplay<POS>) {
    if(player_is_walking_onto<POS>())
        teleport_player<positions::InteriorArrival>(NORTH);
}

}
```

`player_is_walking_onto<POS>()` detects actual entry from the previous movement.
Standing there, moving within the tile, loading or teleporting there does not
count. `player_just_interacted<POS>()` checks a fresh A edge while facing the one
adjacent foot-center tile. Wrong area, diagonals, the player's own tile and held
A do not match. A successful check consumes the edge once and advertises the A
hint; rendering never runs gameplay handlers.

`teleport_player<TARGET>(facing=STILL)` moves to TARGET's tile origin; TARGET
already includes the area. STILL preserves facing. It checks bounds, clears
walking/input and sets global `teleported`. Gameplay and Dialogue check that bool
directly, with no optional frame pointer. It stops outgoing dispatch, then the
tile stage builds every destination chunk even while the bool remains set.
Destination gameplay and dialogue start on the next frame, preventing recursive
transitions. A teleport during Dialogue also refreshes the destination tiles.

NorthGate starts at tile (15,2), chunk (0,0). Move its marker from pixel X=124 to
X=132 and regenerate: its constexpr position becomes (16,2) and its generated call
moves to chunk (1,0), with no handwritten handler edit. Move the baseline gate
art with the marker for a playable edit. The relocation test moves the marker
alone to isolate generated coordinates and chunk assignment.

## Regenerate, build, verify and launch

From the RPG root, use the matching MSVC developer environment on Windows (the
compiler installation/version is recorded in build/CMakeCache.txt):

```sh
python tools/generate_assets.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_SDK_ROOT=../../build/avm-sdk-install -DAVM_SOURCE_ROOT=../.. -DAVM_BUILD_ROOT=../../build
cmake --build build --config RelWithDebInfo --parallel 4
ctest --test-dir build -C RelWithDebInfo --output-on-failure
cmake --build build --config RelWithDebInfo --target rpg_integration
python tools/collect_evidence.py --build build --write-docs
```

For an installed SDK elsewhere, use its absolute path; omit AVM_SOURCE_ROOT and
AVM_BUILD_ROOT without a matching Ardens library. Native-only builds use
`-DRPG_BUILD_GUEST=OFF`. Full-device/restart timing tests explicitly report a skip
in SDK-only mode; SDK LLDB/probe/profile checks still run. CMake regenerates
changed TMX/TSX/PNG/template inputs. `--check` compares generated bytes without
writing. Identical inputs emit identical source bytes.

The default build updates root `rpg.arduboy`; open it in Ardens or an FX-compatible
player. SDK debugging uses `avm-lldb build/artifacts/rpg.elf`. D-pad moves; face
the gate, a bridge tile or house lantern and press A. B opens the travel menu.
Gate/bridge toggle independently; the lantern grants five gold once and remembers
the answer after save/load. The platform is southeast, and the waystone is east
of the central path. NPCs and combat remain separate future updates.
