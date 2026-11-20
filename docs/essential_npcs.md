# Essential NPC mechanics

One eight-slot pool and the 11x7 effective terrain cache bound NPC RAM/work.
Stable NPC identities are independent of the eight reusable actor slots. Occupied slots
form a compact prefix; retirement moves full records without resetting other actors.
`Npc` is 22 bytes and `NpcPool` is 180 bytes on AVM and the host. Definitions, routes,
character sprites, resolver functions and chunk tables remain in PROGMEM. Stage scratch
holds at most eight candidates, regardless of the identity count.

The sample names NPC_VILLAGER Iona, NPC_WANDERER Silas, and NPC_ROWAN Rowan.
`npc_name(id)` returns a PROGMEM name, or null for an unknown ID.
`dialogue_show(id, F("..."))` shows that name in a fitted panel above the main
dialogue panel, aligned with its left edge. The name remains visible across pages.
An ID with a portrait in `faces32.png` (`FACE_PRESENT`) also shows it in a 32x32 tab above
the panel's left end, and the name panel moves beside it, one pixel clear of the portrait
inside a black outline. The panel, its text width and its paging do not change. The mouth
opens and closes every five letters while text types, and is closed while the panel waits
for input and while it eases in or out. The portrait slides up with the panel.
The panel is 31 pixels tall and shows four lines of text. Its last line carries the prompt at
its right end, so text on that line stops short of it. When A starts to work, half a second
after the page finishes, a bobbing arrow shows that more text follows and a blinking A button
shows the last page. A question shows the legend `A yes  B no` right-aligned instead. B still
leaves at a page of text and closes at the end.
Dialogue panels slide up from the bottom with the name panel attached, and the render
camera centers the player in the remaining view. The gameplay camera, tile cache and
simulation remain unchanged throughout the animation. Successive dialogue, questions
and recruitment announcements reuse the open panel; exploration closes it with the
same easing curve and frame count in reverse before accepting gameplay input. Battle/area fade-out also closes it first.
Only the open flag and flash pointers for the visible page persist outside `Game`; no
framebuffer copy or tile reload is needed for the motion.
The text-only overloads use the unnamed panel. `dialogue_show(id, ...)` and `dialogue_ask(id, ...)` name the speaker, and NPC_NONE uses the unnamed panel.
Successful `party_add` calls for NPCs then show "<name> has joined the party!"
and play the `party_join` jingle. Failed additions and the initial traveler setup
remain silent; the announcement uses the ordinary close/dismiss prompt.

The replacement sample defines NPC_VILLAGER (standing, actor row 9) and NPC_WANDERER
(bounded wandering, row 2) in `src/logic/npc_logic.cpp`. The player's row is 0. The
villager follows quest flags between the map markers `VillagerWest` and `VillagerEast`,
starting at `VillagerHome`. Silas has a fixed logical `WandererHome` and bounded wandering. Each has one line of talk dialogue, shown by
`npc_dialogue` after `npc_talk`; the sign is the only other dialogue. Its welcome is
followed by a battle and an outcome line.

`NPC_ROWAN` stands at the generated `RowanHome` marker southeast of the player spawn,
using archer row 3. Talking recruits him into the saved four-member party with 18/18 HP and
4/4 AP, step movement and shoot attack. Further talks acknowledge his membership without
adding another member or resetting his HP. Recruitment retires his map actor immediately; party membership prevents him from
being cast again, including after an area change or save restoration. The traveler starts alone with 20/20 HP,
4/4 AP, and step movement/strike attack. Both start at full HP and AP through `party_new_member`.
Party current HP and AP and both class identifiers are bytes. Maximum HP is derived by `party_max_hp`
from identity (20 base HP, or 18 for Rowan), level bonuses, and Steadfast if learned. Maximum AP
is 4. Saves validate the party count, unique NPC identities, statistics, and both perk ranks. The pause menu's Party panel shows one page per member, turned with left and
right, and freezes world simulation while it eases in and out.

`NpcDefinition` is an 11-byte ROM record: behavior/facing, period, pause/walk, leash,
priority, route, sprite_row and talkable. `period` is the number of frames per movement
step; the sample NPCs use 2, so they walk at half the player's speed. Pause and walk are
frame counts. Rows are 0..15; `talkable=false` excludes a character from A targeting,
leaving nearby tile interactions available. Both NPCs and player select directional
frames through `actor_frame`: north/east/south/west groups of three, idle 0, gait 0,1,0,2.

## Declarative locations and presence

`src/logic/npc_policies.h` specializes `NpcPolicy<ID>` for every contiguous identity
from 1 through `NPC_ID_LIMIT - 1`. Add an identity in `npc_ids.h`, its definition in `logic/npc_definitions.h`,
name/dialogue and policy. Name positions in Tiled; do not duplicate coordinates in C++.
For example:

```cpp
template <> struct NpcPolicy<NPC_VILLAGER> : AtOneOf<positions::VillagerHome,
    When<positions::VillagerEast, [] {
        return game.get_flag(FLAG_VILLAGER_MOVED) && game.get_flag(FLAG_VILLAGER_EAST);
    }>,
    When<positions::VillagerWest, [] {
        return game.get_flag(FLAG_VILLAGER_MOVED) && !game.get_flag(FLAG_VILLAGER_EAST);
    }>> {};
```

The first matching `When` wins; otherwise `current()` returns the default. Predicates
must read saved state without side effects. `AtOneOf<POS>` needs no conditional cases.
The same declaration supplies `locations` for compile-time spatial indexing. Duplicate
or out-of-bounds sites fail compilation. `NO_LOCATION` represents absence and is excluded
from the index. Missing policies and invalid template identities fail compilation.
`npc_location<ID>()` inlines the policy; `npc_location(id)` uses a dense ROM resolver table.

Every frame, after tile updates and overlap repair, `STAGE_NPCS` dispatches only cached
chunks. Begin examines only active slots, refreshes their derived `assigned_site`, and
retires safely unloadable actors. Chunk specializations check only policy sites in their
chunk and evaluate only their compile-time location policy. `npc_offer_candidate` owns
area, party, active identity, definition, cache, visibility, portal and collision checks.
The inline admission front door owns cheap area/party/active-identity rejection;
its outlined `npc_detail::offer_candidate` slow path handles the remaining validation.
This preserves constant site IDs without putting eligibility back into `NpcSite::run`.
Candidates store their definition
priority, avoiding repeated ROM definition lookups during sorting. End admits candidates
in descending definition priority, then ascending stable ID. The buffer holds at most
eight candidates. Saturation waits without evicting visible or pinned actors;
conflicting candidates retry next frame.

Scene entry permits activation throughout the whole stage, including after a teleport.
Later activation tests the rendered 16x16 sprite at the actor origin, without the
(4,8) collision-foot offset. A shared signed rectangle test protects the current
`game_camera()` view plus `PLAYER_PIXELS_PER_FRAME` on each edge (currently one
pixel). Player corner assistance moves at most one pixel per axis per movement
iteration, so this also covers camera movement after the NPC stage and before drawing.
Exact edge contact is offscreen; intersecting even one protected pixel defers admission.
The 11x7 cache has enough margin to cache the full 8x8 feet before this clearance
is lost, including fractional camera phases. No extra tile of clearance is needed.
Retirement uses the same sprite geometry but additionally requires an uncached
footprint, and never unloads unfinished targets/routes. This deliberately stricter
lifecycle condition retains partially clipped sprites even when their feet leave the
cache. Ordinary `NPC_STAND`/`NPC_WANDER` actors also retain their slots while their
same-area logical home's full footprint remains cached. Their uncached feet freeze
simulation, preserving movement state until the cache covers them again. They unload
once both actor and home leave the cache and the sprite is safely offscreen. This avoids
retiring a local wanderer only to reconstruct it immediately at its home. Policy changes
are applied first; `NPC_TRAVEL` still completes off cache and emits its target event,
regardless of destination caching. A full pool waits without evicting retained residents,
including for a higher-priority candidate; no pressure-based eviction is added.
The retirement hook receives
`NPC_RETIRED_UNLOADED` exactly once before prefix compaction. Recruitment uses
`NPC_RETIRED_PARTY`; reset uses `NPC_RETIRED_RESET`. Reacquire `Npc*` after stage processing
because compaction can change slot addresses.

Admission validates the initial world state once. The first cast checks capacity and
initializes its slot without repeating collision tests. After any successful cast,
its activation hook may have changed policy, party, camera, player, terrain or actors;
later casts revalidate these conditions, including all actor occupancy. Thus overlapping
candidates cannot both cast. Failed casts do not invalidate the initial proof. Definition
priority is sampled during admission for deterministic bounded selection; eight candidate
records now include eight priority bytes, with no identity-sized runtime storage.

Quest changes retarget active ordinary actors toward the latest assigned site, independently
of their actual coordinates, so wandering never causes spurious retargeting. Same-area
travel remains pixel-by-pixel while cached. Once safely outside the cache and view, it
completes logically and releases its slot; casting at the destination still obeys terrain,
portal, actor, party and visibility checks. Other-area and absent destinations retain the
visible actor until safe retirement. No uncached terrain is read.

Unfinished `NPC_TARGET` and `NPC_ROUTE` actors are pinned even off cache. Their targets,
waypoints, detours and pauses survive until completion or scene reset. Policy changes are
applied after scripted motion completes. This can temporarily saturate the pool.

Portrait rows are independent of identities: `FACE_ROW_COUNT` bounds `FACE_PRESENT` and
sprite selection. NPCs without a portrait use text dialogue; sprite rows may be shared.
Party names use `npc_name` with a safe fallback.

## Motion

Every simulated position change is one pixel.
Walking, targets, routes, yields and terrain escapes all go through `step()` or the escape
step, and a frame moves an NPC at most once.

- **Cadence.** An NPC steps once every `period` frames. Timers, pauses and stuck limits count
  frames: a blocked attempt adds the NPC's period to its stuck count.
- **Escape.** A tile change can leave an NPC overlapping solid terrain. It then walks out one
  pixel at a time toward the nearest clear spot along a cardinal ray, up to a tile away. The
  distance to that spot falls by one per step, so the walk never jumps. If it cannot move,
  its ordinary logic still runs, so a blocked target can still fail.
- **Walk pose.** The walk pose holds between steps, so a walking NPC does not flicker.
- **Detours.** A steering NPC walks straight at its target. Blocked for `NPC_DETOUR_FRAMES`
  (12 frames), it sidesteps around the obstacle: toward the target across its way ahead, or
  away from it when that side is blocked. Each step it tries to rejoin the way ahead, and it
  sidesteps again while the detour's budget of `NPC_DETOUR_MAX` (15) pixels lasts. A leg
  (a route segment or a target) allows `NPC_DETOURS_PER_LEG` (3) detours. Arrival, a new
  target and a long stall start a fresh budget. A wanderer does the same on its heading,
  sidestepping toward its home, and it stops walking only when no detour is possible. The
  leash and the terrain cache are limits, not obstacles: at either one an NPC waits, or a
  wanderer stops, rather than detouring.

Wandering obeys a pixel leash around its home. Steering tries the axis toward the target
first, then the other, and a blocked NPC detours as described above. A frame, menu or
blocking dialogue freezes simulation.

## Cache margin and freezing

An NPC simulates only while its feet are inside the effective 11x7 terrain cache.
The margin lets a sprite leave view before reaching uncached terrain. A partly visible
uncached actor freezes; once safely offscreen, ordinary actors retire and travel completes
logically through slot release. Unfinished targets/routes keep their physical state pinned
and resume when cached again. No offscreen terrain lookup is introduced.

A successful foot-clear check is derived state within the active actor. Movement validates
the new foot position; escape steps discard the old answer. Tile edits and reloads that
discard edits invalidate actor answers, including edits made by synchronous hooks. Test
fixtures that write cache cells directly must call `note_tile_edit()`, just as runtime setters
do. This memoization does not skip the NPC stage or use dirty-chunk tracking.
Single-site policies also need no repeated resolver call: their fixed property is generated
from the policy itself and stored only as a derived active-actor flag.

## Movement and collision

Route waypoints remain two-byte tile coordinates expanded by TILE=16. Stand, wander, target
and once/loop/reverse routes remain supported. Each live actor updates once after the full
terrain cache, regardless of how many chunks are visible.

Player and NPC collision use an 8x8 foot box at (4,8), rather than the whole 16x16 artwork.
A partly clipped actor freezes when its feet leave cached terrain; it is never simulated
against arbitrary offscreen tiles.

After sixteen sustained push frames, two short walkaround lanes are checked. Where the player
can detour, the NPC holds position. Otherwise a safe short side step is animated; if none fits
or a yield becomes blocked, only that player/NPC pair may overlap until separation. The 8x8
footprints can fit side by side in a 16-pixel lane when aligned appropriately, so the NPC holds
still and permits an actual player detour there. Centered tight lanes still yield or allow
temporary passage. Yield search checks up to 15 pixels to cover contacts offset by as much as
seven pixels.

Rendering sorts the visible NPC slots together with the player by ascending Y. A stable
insertion sort stores at most nine byte indices on the stack, leaving the simulation pool
untouched. Lower actors draw last; equal Y preserves slot order and places the player last.

## Guarantees and how they are checked

The rules above make these properties hold for every NPC, not just the sample ones:

1. No NPC is removed while its scene lasts.
2. No NPC moves more than one pixel in a frame, except at a scene entry.
3. No NPC is cast where the player can see it, except at a scene entry.
4. An NPC comes into view only by walking there or by the camera moving.

`tests/test_npc_continuity.cpp` checks each property on every frame of random play, with
terrain edits under the NPCs. A planted one-pixel-jump fault in the escape step is caught by
it. The AVM stack budget (256 bytes) constrains the code: the lifecycle helpers and the
detour and yield helpers are kept out of line, so their frames do not grow the frames on the
deepest paths.

## Limits

- NPCs detour around an obstacle but do not plan a route. A detour sidesteps at most 15
  pixels, three times per leg. A target that lies inside an obstacle, or beyond a longer
  one, fails.
- There is no API for removing an NPC mid-scene, so a quest that needs one must end the scene
  first.
- A scene cut is not a pop. Entering an area, or teleporting the player across the map, changes
  the whole screen at once, and any NPC on the new screen appears with it.

## Hooks

The generic engine calls activation, movement-event, retirement and restore hooks declared in
`logic/npc_logic.h`. The sample hooks are empty. Events cover route start/completion/invalidity
and target reached/failed. Retirement reports reset, recruitment or safe unloading. Independent native policy
fixtures exercise route replacement, pauses, endpoints and targets without quest code inside
`src/npc.cpp`.

Save version 17 writes the complete pointer-free live `Game` directly from `.saved`,
without a separate buffer or checksum. Quest flags are the authoritative location state.
Restoration validates the version, player, party, flags and area bounds, then resets actors,
scratch and movement state. The normal cache repair and complete scene-entry NPC stage
reconstruct actors from policies. Older save versions are rejected without migration.
