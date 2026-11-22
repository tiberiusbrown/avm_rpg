# Quests

Quests are compile-time definitions in their own headers. Each quest owns its status, text,
objective, map marker and event handlers. The engine reaches them only through `src/quest.h`, so
adding a quest does not change the Journal, the map or the dialogue code.

The shipping catalog contains Dry Roots, The Span Below, and Tracks in the Dust.
`src/quest_runtime.cpp` compiles the registry.

## Files

| File | Role |
| --- | --- |
| `src/quest_ids.h` | `QUEST_ID` and `QUEST_ID_LIMIT`, the stable identities. |
| `src/quest.h` | Public API, `QUEST_STATUS`, event subscriptions and the `Quest<ID>` contract. |
| `src/quest_dispatch.h` | Compile-time validation, subscription traits and the per-ID operation tables. |
| `src/quest_registry.h` | The shipping catalog: includes each quest header. |
| `src/quests/` | Authored quest definitions. |
| `src/quest_runtime.cpp` | Generated dispatch, pending evaluation, selection and the public API. |
| `src/journal.h` | Journal layout constants, the view struct and the summary word wrap. |
| `src/logic/area_village.h`, `src/logic/area_cave.h` | Resident conversations, inscriptions, and battle hooks that update saved facts or emit events. |
| `src/screens.cpp`, `src/render.cpp` | The Journal screen and the tracked-quest map marker. |

## Identity and registration

`QUEST_ID` is an explicit `uint8_t` enum. `QUEST_NONE` is never a quest. `QUEST_ID_LIMIT` is one
past the largest ID, and it can be overridden with `RPG_QUEST_ID_LIMIT` for a test catalog.

Each quest specializes `Quest<ID>` in its own header:

```cpp
template <> struct Quest<QUEST_DRY_ROOTS> { /* members below */ };
```

To register a quest, add its ID to `quest_ids.h`, add its header to `quest_registry.h`, and
define the specialization. The contiguous range `1` to `QUEST_ID_LIMIT - 1` must be fully defined.

## Quest members

| Member | Required | Signature | Purpose |
| --- | --- | --- | --- |
| `status` | yes | `static QUEST_STATUS status()` | Derived from game state each time it is asked. |
| `title` | yes | `static char const PROGMEM* title()` | The Journal name. |
| `summary` | yes | `static char const PROGMEM* summary()` | Progress text. It may depend on the status. |
| `objective` | no | `static char const PROGMEM* objective()` | The current goal, or `nullptr` for none. |
| `marker` | no | `static bool marker(uint8_t area, TilePos& out)` | The objective's map position in an area. |
| `start` | no | `static void start()` | Records the start fact. Without it, `quest_start` refuses. |
| `consistent` | no | `static bool consistent()` | Rejects impossible saved states. |
| `evaluate` | with `FlagChanged` | `static void evaluate()` | Progress when a watched flag changes. |
| `on` | with `NpcTalked` | `static void on(NpcTalked<ID>)` | Handles one processed conversation. |

Optional members are checked for their exact signature when they are declared. A member with the
wrong signature is a compile error, not an ignored member. Quests declare subscriptions with
`using Watches = Watch<...>;`, and a quest without one receives no events.

Status, text and marker queries must not change game state. `start`, `evaluate`, event handlers, and explicit content interactions may set flags.

## Derived status and persistence

Status derives from saved facts. For example, The Span Below is completed
when the bridge flag is set and active once the maintenance notice is read.

`Game::selected_quest` stores the tracked quest or `QUEST_NONE`. Save version 21
rejects earlier development saves because map positions, identities, and flag
indices changed. The save checks the tracked ID and calls each quest’s
`consistent()`. Dry Roots requires the offer and both accounts for a settlement.
Tracks in the Dust requires a victory before the tally can be recorded.
`save_restore()` clears a selection that names an inactive quest.

Computed statuses, summaries, markers, the Journal cursor and pending evaluations are not saved.

## Events

There are two kinds of subscription, and they are delivered differently.

**`FlagChanged<FLAG>`** is a state change. `Game::set_flag`, `clear_flag` and `toggle_flag` notify
quests only when they change a valid flag. A notification sets a pending bit for each quest that
watches the flag, using a PROGMEM table indexed by flag, so an unwatched flag does no quest work.
The quest is evaluated later, at the settle described below.

**`NpcTalked<ID>`** is a discrete occurrence. The conversation handler calls
`quest_emit(NpcTalked<ID>{})` after the dialogue and the NPC's own movement have run. Each
occurrence is delivered once, immediately, to the quests that name that NPC. Delivery is a
per-NPC sequence of direct calls, generated at compile time, so it never scans the catalog.

Handlers should check status themselves when progress depends on it. Dry Roots checks that the inquiry is active and both accounts are recorded.

## Settling

`update_visible_world` calls `quest_settle()` after the dialogue and every world stage, and before
the frame ends. It never runs inside a stage, so it cannot reenter the world update.

Each settle takes up to four passes. In each pass it snapshots and clears the pending bits, then
evaluates the snapshotted quests in ID order. Only active quests are evaluated. A notification
raised during a pass waits for the next pass, so a cascade of up to four steps completes in one
frame, and a deeper cascade continues in the next frame. Pending bits that remain at the end of a
settle are kept, so a notification is never dropped. Settling finishes by clearing a tracked
quest that is no longer active.

`quest_reset()` clears pending bits, and it runs on a new game and on a loaded save. Loading
replays nothing, because flags are restored without notifications and discrete events are not
saved.

## Selection

Tracking is separate from status and from the Journal's highlight.

- Only an active quest can be tracked. `quest_select` refuses any other quest.
- At most one quest is tracked. Selecting another replaces it.
- A quest's first `quest_start` tracks it, but only when nothing is tracked.
- Completion or any other end of activity clears tracking. Nothing else is selected.
- Tracking changes no quest state.

## Journal

`journal_screen()` opens from the pause menu like Party and Audio. It shows the listed quests, which
are those that are not "not started". The list is found by scanning the ID range on each query,
so the Journal stores no list. The cursor is a row index, and it is transient.

- Up and Down move the highlight. A opens the highlighted quest. B closes the Journal.
- In the detail view, A tracks or untracks an active quest, and Up and Down scroll the summary.
  B returns to the list. A completed or failed quest answers A with the cursor-edge sound.

The list highlights the selected quest and right-aligns its status. The detail view keeps the
tracking action near the status and the current goal below the summary. Chevron icons appear
when more entries or summary lines are available.

The summary is wrapped once, when the quest opens. The wrap records line offsets into the summary
in program memory, and it keeps at most `JOURNAL_SUMMARY_LINES` lines. Nothing is copied, so the
Journal has no text buffer. Measurement and drawing use small chunks on the stack.
The native tests check the wrap with a fixed-width stand-in font, which
confirms that lines fit their width and that no text is lost. The device suite starts Dry Roots for one
pause menu, opens its detail view, captures it for review, and checks that the view leaves the game
state unchanged. It does not assert the rendered pixels. The world does not simulate while the Journal is open.

## Map markers

While the world map is open, the tracked quest's marker is drawn if the quest is active, it has a
marker in the displayed area, and that position lies on the display. `quest_map_marker` checks the
tracked quest only. A marker is refused if its area is not the requested one, or if its position
is outside that area, whatever the quest computed.

The marker is a black-edged white diamond that blinks opposite the player's marker. It needs no
asset. Its position uses the same tile-to-map mapping as the player.

Dry Roots uses `npc_location()` to guide the player to the next witness or back
to Tessa. The mine quests mark the entrance on the outdoor map. Tracks in the
Dust also supplies the rail chamber and tally locations to cave-area queries.

## Compile-time and runtime checks

The compiler enforces:

- every ID from `1` to `QUEST_ID_LIMIT - 1` has a definition,
- the status, title and summary members exist with their exact signatures,
- optional members that are declared have their exact signatures,
- `Watches` is a `Watch<...>` of `FlagChanged` and `NpcTalked` events,
- no subscription is duplicated,
- a flag subscription names a valid flag, and a conversation subscription names a valid NPC,
- every subscribed event has its handler.

`tests/test_quest_validation.py` compiles a negative case for each of these.

Runtime tests cover the rest: status derivation, the dialogue and conversation paths, persistence,
selection and tracking rules, marker geometry and filtering, the Journal listing, the wrapping of
the shipped text, the bounded settle, flag and conversation selectivity, cascades, and the
behavior of 40 quests.

The compiler does not check quest semantics. A quest that sets its completion flag from the wrong
place is caught only by a behavioral test.

## Adding a quest

1. Add `QUEST_NEW_NAME` to `quest_ids.h`. Give it the next free value.
2. Add any flags it needs to `flags.h`, before `FLAG_COUNT`.
3. Create `src/quests/new_name.h` with `template <> struct Quest<QUEST_NEW_NAME> { ... };`.
4. Include the header from `quest_registry.h`.
5. Add tests in `tests/test_quests.cpp`, and run `ctest` for `quests` and `quest_validation`.
6. If the quest is started from a conversation, call `quest_start` from that conversation. If it
   reacts to one, subscribe with `NpcTalked` and emit the event from its handler.

Keep a quest's progress in game flags, so that it is saved and restored with the rest of the game.
A quest that reacts to a flag change should subscribe with `FlagChanged`, not poll each frame.

## Millcross examples

- **Dry Roots** starts with Tessa’s offer. Confirming either resolution emits
  `NpcTalked<NPC_TESSA>` after both witnesses’ accounts have been heard. Its
  completion changes Tessa’s location and the dialogue of affected residents.
- **The Span Below** starts when the player reads the maintenance notice.
  The bridge plate completes the quest, extends the span, and opens the
  maintenance hatch even if the notice was skipped.
- **Tracks in the Dust** becomes active at Nell’s warning or on beating the
  mine wolf. Finding and reading the tally after victory completes it.
  The tally stays recorded if the wolf returns under The World Remembers.

Dry Roots and Tracks in the Dust award experience once. When someone levels
up, exploration presents the level-up notice after the dialogue closes.
Earned perk choices stay pending in Party after the notice is dismissed.

## Limitations

- Summaries and objectives are fixed PROGMEM strings, chosen by status. Formatted or streamed
  summaries are not implemented. The interface can add them without changing the registry.
- Static, declared map markers are not implemented. A marker is computed by the quest.
- `FlagChanged` is exercised only by the 40-ID test catalog. The shipping catalog has no flag
  subscriptions, so no shipping evaluation runs.
- `QUEST_FAILED` is part of the derived model and the Journal, but no quest produces it yet.
- Only `NpcTalked` has an emitter. Battles, items, party changes, area entry and map positions have
  no events yet, because the spec asks for only the sources needed now.
