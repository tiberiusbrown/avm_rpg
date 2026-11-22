- Use C++20 for game code and native C++ tests.
- Use snake_case for functions and variables, CamelCase for struct types, and ALL_CAPS for constants, macros, enum types, and enum values.
- Indent with four spaces; do not use tabs.
- Put spaces around binary and ternary operators and after commas.
- Do not put a space before opening parentheses after function names or control words: `if(condition)`, `update(game)`.
- Separate for-loop fields with spaces: `for(int i = 0; i < n; ++i)`.
- Put each statement on its own line; avoid multiple semicolon-delimited statements per line.
- Separate function definitions, include groups, and global-declaration groups with a blank line.
- Separate semantically distinct sections within a function with a blank line.
- Follow `.clang-format` for C++ formatting.
- Use `PROGMEM` and `F()` for program data and text; provide their non-AVM definitions for native tests.
- Call `avm_*` methods directly; avoid platform wrappers such as `text()` in `render.cpp`.
- Use the C++ AVM overloads instead of `_P` variants.
- Declare generated assets `constexpr`.
- Use procedural screen functions with local input/display loops; call and return instead of global mode dispatch.
- Access the single global `game` directly for gameplay; do not pass `Game&` through engine or screen APIs. Customize generated `Handler<POS, HandlerStage>` through ordinary `handler_impl` overloads accepting `Gameplay<POS>`, `Tiles<POS>` or `Dialogue<POS>` tags. Generated generic functions provide empty defaults. Complete all `STAGE_GAMEPLAY` work, reload the cache, complete all `STAGE_TILES` work, complete `STAGE_NPCS`, update active actors, then run `STAGE_DIALOGUE`. Blocking interactions belong in Dialogue; its terrain flag changes take effect on the next tile pass unless explicitly refreshed. Newly authored handler names must export/compile without handwritten definitions. Do not reintroduce context objects or a frozen world snapshot. `teleported` and consumed frame input are transient globals.
- Runtime tile reads use `cached_tile`; wrong-area and out-of-window reads return `TILE_OUTSIDE`. Do not add offscreen projection or direct PROGMEM tile lookup APIs. Saves validate structure/bounds before loading, then repair blocked positions using the loaded cache.
- Keep guest full LTO enabled and verify the single global `Game`, the absence of game-reference helper arguments and elimination of map tile-position storage.
- Preserve deterministic rules, fixed capacities, and AVM memory/stack budgets.
- Build and test in RelWithDebInfo; keep the playable `rpg.arduboy` updated in the repository root.
- Test gameplay behavior and meaningful invariants, not frozen framebuffer contents or exact visual/animation sequences. Routine art, layout, and animation edits should not require test updates; keep display captures for human review.
- Do not hardcode map positions in game code. Name the position as a point object in the map's Logic group and use the generated `positions::` constant. Derive nearby positions as offsets from a marker.
- Before editing maps or tiles, read `assets/maps/AUTHORING.md`.

- Define each essential NPC with `NpcPolicy<ID> : AtOneOf<...>` and `NpcContent<ID>` in its area header; use `When<POS, predicate>` for saved quest conditions. Keep possible sites and conditions together, without a second location registry or per-identity coordinates. Add positions in Tiled. Identity, portrait row and active slot are independent.
- Preserve the eight-slot compact actor prefix, bounded candidate priority (higher priority then lower ID), guarded offscreen activation/retirement and pinned essential NPCs with unfinished target/route movement. Reacquire actor pointers after NPC-stage compaction. Run the full NPC stage every frame and throughout destination scene preparation.
- Python emits map catalogs and area dimensions; C++20 owns chunk specialization and dense PROGMEM function tables. Keep 24-bit code pointers, full LTO and complete stack bounds. Save loading reconstructs transient actors from their location policies.

## UI panel style

- Use Journal, Party, and Supplies as the visual reference for menus and popups. Reuse the shared panel drawing helpers in `src/render.cpp`.
- Draw panel titles in `AVM_FONT_PIXELOIDSANSBOLD` with a horizontal divider below. Use `AVM_FONT_BR5D` for rows and prompts. Keep the battle special-attack popup titleless.
- Highlight the selected row with a filled, seven-pixel-high white bar and draw the row text in black inside it. Keep one clear black pixel between the bar and any adjacent horizontal divider or panel border when the first or last row is selected. Adjust the row positions or panel height to provide that gap.
- Use the shared arrow sprites for scrolling or directional adjustments. Show them where movement in that direction is available.
- Use the shared A and B button sprites with adjacent action labels in prompts and footers, including battle confirmations.

## World content

Before authoring maps, NPCs, quests, encounters, or environmental content,
read [the content authoring guide](docs/content_authoring.md) and
[the world bible](docs/world_bible.md). Keep established setting facts current
with the shipped content and use the linked map guides for technical rules.
