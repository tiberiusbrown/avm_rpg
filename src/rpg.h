#ifndef RPG_H
#define RPG_H

#include <stdint.h>
#include <utility>

#include "world_types.h"
#include "supplies.h"
#include "area_ids.h"
#include "npc_ids.h"
#include "quest_ids.h"

#ifdef RPG_AVM
#include <avm/pgmspace.h>
#else
#define PROGMEM
#define F(text) (text)
#define memcpy_P(dst, src, size) memcpy((dst), (src), (size))
#endif

#ifdef _MSC_VER
#define RPG_NOINLINE __declspec(noinline)
#else
#define RPG_NOINLINE __attribute__((noinline))
#endif

// Device QA guests (RPG_DEVICE_QA) record the simulation phase and frame markers that the device
// tests read. Shipping guests compile both out.
#if defined(RPG_AVM) && defined(RPG_DEVICE_QA)
extern volatile uint8_t world_phase;
#define RPG_PHASE(value) (world_phase = (value))
#define RPG_QA_MARK(call) call
extern "C" void rpg_frame_begin();
extern "C" void rpg_frame_end();
extern "C" void rpg_render_begin();
extern "C" void rpg_render_end();
// A Journal redraw, which is not a frame and does not advance the frame serial.
extern "C" void rpg_qa_journal_begin();
extern "C" void rpg_qa_journal_end();
#else
#define RPG_PHASE(value) ((void)0)
#define RPG_QA_MARK(call) ((void)0)
#endif

inline constexpr uint8_t SAVE_VERSION = 22;

enum {
    BTN_B = 4,
    BTN_A = 8,
    BTN_DOWN = 16,
    BTN_LEFT = 32,
    BTN_RIGHT = 64,
    BTN_UP = 128,
    BTN_ALL = 252
};

enum DIRECTION : uint8_t { NORTH, EAST, SOUTH, WEST, STILL = 4 };

// DIALOGUE_TYPING means the overlay is open and its letters are still animating.
enum DIALOGUE_PROMPT : uint8_t {
    DIALOGUE_CLOSED,
    DIALOGUE_MORE,
    DIALOGUE_DONE,
    DIALOGUE_CHOICE,
    DIALOGUE_TYPING,
    DIALOGUE_OPTIONS,
};

struct Actor {
    uint16_t x, y;
    DIRECTION facing;
    uint8_t walking;
};

#include "npc_types.h"

// Each walking pose lasts this many simulation ticks, which is 160 ms at 50 Hz.
inline constexpr uint16_t WALK_POSE_TICKS = 8;

// Character rows in actors16.png; the generated sprite sheet asserts this count.
inline constexpr uint8_t ACTOR_ROW_COUNT = 32;

constexpr uint16_t actor_frame(Actor const& actor, uint8_t sprite_row, uint16_t ticks) {
    uint8_t phase = (ticks / WALK_POSE_TICKS) & 3;
    uint8_t step = actor.walking && (phase & 1) ? (phase == 1 ? 1 : 2) : 0;
    return (uint16_t)sprite_row * 12 + actor.facing * 3 + step;
}

struct Camera {
    uint16_t x, y;
};

inline constexpr uint8_t OPTION_SLIDER_MAX = 15;
inline constexpr uint8_t OPTION_VOLUME_MAX = OPTION_SLIDER_MAX * 2;

struct AudioOptions {
    uint8_t music = OPTION_SLIDER_MAX;
    uint8_t sfx = OPTION_SLIDER_MAX;
};

inline constexpr uint8_t PARTY_CAPACITY = 4;

enum MOVEMENT_CLASS : uint8_t {
    MOVEMENT_STEP,
    MOVEMENT_LEAP,
    MOVEMENT_CHARGE,
    MOVEMENT_VAULT,
    MOVEMENT_GLIDE,
    MOVEMENT_CLASS_COUNT
};

enum ATTACK_CLASS : uint8_t {
    ATTACK_STRIKE,
    ATTACK_SHOOT,
    ATTACK_REACH,
    ATTACK_IMPACT,
    ATTACK_REND,
    ATTACK_ARC,
    ATTACK_DRAIN,
    ATTACK_RECOIL,
    ATTACK_FLANK,
    ATTACK_BLAST,
    ATTACK_CLASS_COUNT
};

// Maximum HP and AP are not stored: party_max_hp and party_max_ap derive them for each member.
struct PartyMember {
    uint8_t hp, ap;
    uint8_t movement_class, attack_class;
    NPC_ID id; // NPC_NONE identifies the traveler; other identities prevent duplicate recruits.
    uint8_t level;      // 1 through PARTY_MAX_LEVEL
    uint8_t experience; // toward the next level only; it resets to zero at each level-up
    uint8_t perk_ranks; // movement in bits 0–2, attack in bits 3–5
    uint8_t equipment[SLOT_COUNT]; // zero is empty, otherwise item ID plus one
};

struct Party {
    PartyMember members[PARTY_CAPACITY];
    uint8_t count;
};

constexpr ITEM_ID equipment_get(PartyMember const& member, EQUIPMENT_SLOT slot) {
    return slot < SLOT_COUNT ? equipment_decode(member.equipment[slot]) : ITEM_NONE;
}

static_assert(sizeof(PartyMember) == 11, "byte-sized party statistics, identity and progress");

inline constexpr uint8_t PARTY_MAX_LEVEL = 10;

// Experience needed to go from a level to the next: 10 at level 1, rising by 5 each level, so it
// fits a byte through the cap. Experience is not cumulative.
constexpr uint8_t party_experience_needed(uint8_t level) {
    return (uint8_t)(5 * (level + 1));
}

#define MOVEMENT_PERK(cls, rank, id, amount, range, name, text)                                    \
    inline constexpr uint8_t id##_CLASS = cls, id##_RANK = rank, id##_BONUS = amount,              \
                             id##_RANGE = range;
#define ATTACK_PERK MOVEMENT_PERK
#include "perks.def"
#undef ATTACK_PERK
#undef MOVEMENT_PERK

enum PERK_TRACK_KIND : uint8_t { PERK_MOVEMENT, PERK_ATTACK };

inline constexpr uint8_t PERK_TRACK_RANKS = 4;
inline constexpr uint8_t PERK_TRACK_COUNT = MOVEMENT_CLASS_COUNT + (uint8_t)ATTACK_CLASS_COUNT;

struct PerkTrack {
    uint8_t first, ranks;
};

struct PerkTracks {
    PerkTrack entries[PERK_TRACK_COUNT];
};

inline constexpr PerkTracks PERK_TRACKS = [] {
    PerkTracks tracks{};
    uint8_t entry = 0;
    auto add = [&](uint8_t track, uint8_t rank) {
        auto& info = tracks.entries[track];
        if(!info.ranks)
            info.first = entry;
        return ++info.ranks == rank && rank <= PERK_TRACK_RANKS && info.first + rank - 1 == entry++;
    };
#define MOVEMENT_PERK(cls, rank, id, amount, range, name, text)                                    \
    if(!add(cls, rank))                                                                            \
        return PerkTracks{};
#define ATTACK_PERK(cls, rank, id, amount, range, name, text)                                      \
    if(!add(MOVEMENT_CLASS_COUNT + (uint8_t)cls, rank))                                            \
        return PerkTracks{};
#include "perks.def"
#undef ATTACK_PERK
#undef MOVEMENT_PERK
    return tracks;
}();

static_assert(
    [] {
        for(auto const& track : PERK_TRACKS.entries)
            if(!track.ranks)
                return false;
        return true;
    }(),
    "every class has contiguous ranks in the perk catalog");

constexpr uint8_t party_perk_limit(PartyMember const& member, PERK_TRACK_KIND track) {
    if(track == PERK_MOVEMENT)
        return member.movement_class < MOVEMENT_CLASS_COUNT
                   ? PERK_TRACKS.entries[member.movement_class].ranks
                   : 0;
    return member.attack_class < ATTACK_CLASS_COUNT
               ? PERK_TRACKS.entries[MOVEMENT_CLASS_COUNT + member.attack_class].ranks
               : 0;
}

constexpr uint8_t party_movement_perk_rank(PartyMember const& member) {
    return member.perk_ranks & 7;
}

constexpr uint8_t party_attack_perk_rank(PartyMember const& member) {
    return (member.perk_ranks >> 3) & 7;
}

constexpr uint8_t party_perk_rank(PartyMember const& member, PERK_TRACK_KIND track) {
    return track == PERK_MOVEMENT ? party_movement_perk_rank(member)
                                 : party_attack_perk_rank(member);
}

constexpr uint8_t party_unspent_perks(PartyMember const& member) {
    int remaining = member.level / 2 - party_movement_perk_rank(member) -
                    party_attack_perk_rank(member);
    return remaining > 0 ? (uint8_t)remaining : 0;
}

bool party_can_learn_perk(PartyMember const& member, PERK_TRACK_KIND track);
bool party_learn_perk(PartyMember& member, PERK_TRACK_KIND track);

// Checks level, XP toward the next level, and ranks funded by the earned milestones.
constexpr bool party_progress_valid(PartyMember const& member) {
    if(member.level < 1 || member.level > PARTY_MAX_LEVEL || (member.perk_ranks & 0xc0) ||
       party_movement_perk_rank(member) > party_perk_limit(member, PERK_MOVEMENT) ||
       party_attack_perk_rank(member) > party_perk_limit(member, PERK_ATTACK) ||
       party_movement_perk_rank(member) + party_attack_perk_rank(member) > member.level / 2)
        return false;
    return member.level == PARTY_MAX_LEVEL ? member.experience == 0
                                           : member.experience < party_experience_needed(member.level);
}

// The experience a member earns from each victory, before any perk or item changes it.
inline constexpr uint8_t PARTY_VICTORY_EXPERIENCE = 2;

constexpr uint8_t party_victory_experience(PartyMember const&) {
    return PARTY_VICTORY_EXPERIENCE;
}

// Every member's AP maximum is the same until items and perks change it.
inline constexpr uint8_t PARTY_BASE_AP = 4;

// The maximum HP of a party member: the base of its identity, the traveler's 20 or Rowan's 18, plus
// 2 at level 3, 2 more at level 7, and any equipped HP bonuses.
constexpr uint8_t party_max_hp(PartyMember const& member) {
    uint8_t base = member.id == NPC_ROWAN ? 18 : 20;
    uint8_t boots = equipment_get(member, SLOT_ARMOR) == ITEM_TRAVELERS_BOOTS
                        ? TRAVELERS_BOOTS_HP : 0;
    uint8_t pendant = equipment_get(member, SLOT_RELIC) == ITEM_VITAL_PENDANT
                          ? VITAL_PENDANT_HP : 0;
    return (uint8_t)(boots + pendant + base + (member.level >= 3 ? 2 : 0) +
                     (member.level >= 7 ? 2 : 0));
}

// The normal-attack damage a party member adds: 1 at level 5 and 1 more at level 9.
constexpr uint8_t party_attack_bonus(PartyMember const& member) {
    return (uint8_t)((member.level >= 5 ? 1 : 0) + (member.level >= 9 ? 1 : 0));
}

constexpr uint8_t party_max_ap(PartyMember const&) {
    return PARTY_BASE_AP;
}

// Base AP recovery after each victory, capped at the member's maximum.
inline constexpr uint8_t PARTY_VICTORY_AP = 1;

constexpr uint8_t party_victory_ap(PartyMember const& member) {
    uint8_t dawn = equipment_get(member, SLOT_RELIC) == ITEM_DAWN_PENDANT ? DAWN_PENDANT_AP : 0;
    return PARTY_VICTORY_AP + dawn;
}

// A member at full HP and AP, level 1 with no experience, as the traveler and new recruits join.
PartyMember party_new_member(NPC_ID id, MOVEMENT_CLASS movement, ATTACK_CLASS attack);

// Adds experience to one member and processes each level it completes at once. Returns the number
// of levels gained. At the cap, the experience is discarded.
uint8_t party_gain_experience(PartyMember& member, uint8_t amount);

struct LevelUpNotice {
    uint8_t members = 0;
    bool perk_earned = false;

    void award(Party& party, uint8_t slot, uint8_t experience);
};

bool party_has_unspent_perks(Party const& party);

bool party_contains(NPC_ID id);
// Rejects duplicate identities, HP or AP above the member's maximum, invalid classes, and a full
// party without changing state. Successful NPC recruitment shows a blocking announcement and plays
// a jingle.
bool party_add(PartyMember member);

// Records that a flag changed value, so quests that watch it are evaluated at the next settle.
void quest_flag_changed(FLAG flag);

struct Game {
    // The live object is AVM's saved prefix. Bump the version when its byte layout changes.
    uint8_t save_version;
    Actor player;
    uint16_t rng, gold, ticks;
    WorldFlags flags;
    uint16_t remember_timer;
    uint8_t area, level;
    Party party;
    Supplies supplies;
    MapDimensions dimensions; // Active area dimensions, cached on entry or teleport.
    TilePos previous_player_tile;
    bool can_interact;
    NpcPool npcs;
    AudioOptions options;
    // The tracked quest, or QUEST_NONE. Only an active quest is tracked; see quest.h.
    QUEST_ID selected_quest;

    bool get_flag(FLAG flag) const {
        return flags.get_flag(flag);
    }

    // Mutators notify quests only when they change a flag, so no-op writes cost nothing.
    void set_flag(FLAG flag) {
        if(flags.set_flag(flag))
            quest_flag_changed(flag);
    }

    void clear_flag(FLAG flag) {
        if(flags.clear_flag(flag))
            quest_flag_changed(flag);
    }

    void toggle_flag(FLAG flag) {
        if(flags.toggle_flag(flag))
            quest_flag_changed(flag);
    }

    bool player_is_on_tile(TilePos tile) const;

    template <TilePos POS> bool player_is_on_tile() const {
        return POS.area == area && dimensions.pixel_bounds(player.x, player.y) &&
               ((player.x + FOOT_CENTER_X) >> TILE_SHIFT) == POS.x &&
               ((player.y + FOOT_CENTER_Y) >> TILE_SHIFT) == POS.y;
    }

    bool player_is_in_region(TileRegion region) const;
};

extern Game game;
extern bool save_exists;

inline constexpr uint16_t SAVE_BYTES = sizeof(Game);

struct Area {
    uint8_t width, height;
    uint8_t chunk_columns, chunk_rows;
    uint8_t const PROGMEM* tiles;
    PixelPos spawn;
};

static_assert(sizeof(Area::width) == 1 && sizeof(Area::height) == 1 &&
                  sizeof(Area::chunk_columns) == 1 && sizeof(Area::chunk_rows) == 1,
              "8-bit logical and chunk-grid dimensions");
static_assert(sizeof(Actor::x) == 2 && sizeof(Actor::y) == 2 && sizeof(Camera::x) == 2 &&
                  sizeof(Camera::y) == 2,
              "16-bit actor and camera pixels");

Area const PROGMEM* world_area(uint8_t id);
void reload_tile_window(TileCache& cache, uint8_t const PROGMEM* tiles, MapDimensions dimensions,
                        Camera camera);
// Any collision at all makes a tile solid for NPCs, navigation and cached reads.
bool tile_solid(uint8_t tile);
// Sixteen triangles: four per 8x8 quadrant (top-left, top-right, bottom-left,
// bottom-right), each nibble ordered north, east, south, west. Player movement only.
uint16_t tile_collision(uint8_t tile);
// The AMBIENT_CLASS of a tile in the active area's tileset.
uint8_t tile_ambient(uint8_t tile);
bool collision_pixel_solid(uint16_t mask, uint8_t x, uint8_t y);
#ifndef RPG_AVM
// Host tests substitute masks for tiles that the shipped tileset does not use.
void tile_collision_override(uint8_t tile, uint16_t mask);
void tile_collision_reset();
#endif
bool tile_portal(uint8_t tile);
// Wrong-area and out-of-window reads return the solid TILE_OUTSIDE sentinel.
uint8_t cached_tile(TilePos p);
bool terrain_clear(uint16_t x, uint16_t y);
// One pixel of a step, from a footprint already known clear: checks only the tiles it enters.
bool terrain_step_clear(uint16_t x, uint16_t y, DIRECTION dir);
// Player foot box against collision masks. Full tiles block the whole box.
bool player_terrain_clear(uint16_t x, uint16_t y);
// Bit i is set when pixel i of the foot box's leading edge is blocked: the right or left
// column for east and west steps, and the bottom or top row for south and north steps.
uint8_t player_leading_edge(uint16_t x, uint16_t y, DIRECTION dir);
void reload_tile_cache();
// Exploration takes back the runtime workspace after a battle: it rebuilds the camera and tile window
// from the world assets and persistent flags, and invalidates every cached collision answer.
void world_reclaim_workspace();
bool update_visible_world(uint8_t pressed = 0, bool simulate = true);
// Places the player in an area and resets its NPCs. The visible world is stale until
// update_visible_world runs. Returns false, placing nothing, for an unknown area.
bool world_place(uint8_t area, uint16_t x, uint16_t y, DIRECTION facing);
// world_place followed by update_visible_world.
void world_enter(uint8_t area, uint16_t x, uint16_t y, DIRECTION facing);
uint16_t rng_next();
// Starts a new game at the outdoor spawn. The visible world is stale until
// update_visible_world runs.
void game_start();
// game_start followed by update_visible_world.
void game_new();

// The counter sets this during dispatch. Exploration clears it before opening the modal.
extern bool shop_requested;

// Returns true when the update teleported the player, so the area changed.
bool game_update(uint8_t held, uint8_t pressed = 0);
bool actor_move(Actor* actor, DIRECTION dir, uint8_t pixels);
// Player walking speed, in pixels per frame.
inline constexpr uint8_t PLAYER_PIXELS_PER_FRAME = 1;
// A blocked step is passed by sidestepping this many pixels or fewer.
inline constexpr uint8_t CORNER_ASSIST_PIXELS = 6;
// Center the player in the uncovered viewport, clamped to the map.
Camera game_camera(uint8_t view_height = 64);
void render_battle_result(bool victory, uint8_t gold_gained);
void render_battle_offer(char const PROGMEM* special, uint8_t ap, uint8_t choice, bool at_bottom);
void render_level_up_notice(char const PROGMEM* title, LevelUpNotice notice);

// Block until acknowledged (true) or dismissed with B (false).
bool dialogue_show(char const PROGMEM* words);
// Show the speaker's name in a left-aligned panel above the dialogue. An ID with a portrait also
// shows it in a tab above the panel's left end, with the name beside it.
// NPC_NONE and unknown IDs use the unnamed overlay.
bool dialogue_show(NPC_ID id, char const PROGMEM* words);
// Announce a successful recruitment with a jingle; called by party_add after updating the party.
void party_joined(NPC_ID id);
// Wrap/page the question, then return A = yes or B = no. NPC_NONE and unknown IDs use the unnamed
// overlay, as dialogue_show does.
bool dialogue_ask(char const PROGMEM* question);
bool dialogue_ask(NPC_ID id, char const PROGMEM* question);
// Show 2–255 single-line options. Return the zero-based choice, or 255 for B when allowed.
uint8_t dialogue_options_ex(char const PROGMEM* const PROGMEM* options, uint8_t count,
                            bool allow_back = true);

namespace dialogue_detail {

template<unsigned Size>
struct DialogueLabel {
    char text[Size];

    constexpr DialogueLabel(char const (&label)[Size]) : text{} {
        for(unsigned i = 0; i < Size; ++i)
            text[i] = label[i];
    }
};

template<DialogueLabel Label, typename Indices = std::make_index_sequence<sizeof(Label.text)>>
struct DialogueLabelStorage;

template<DialogueLabel Label, std::size_t... I>
struct DialogueLabelStorage<Label, std::index_sequence<I...>> {
    static constexpr char PROGMEM TEXT[] = {Label.text[I]...};
};

template<bool AllowBack, DialogueLabel... Labels>
uint8_t choose() {
    static_assert(sizeof...(Labels) >= 2 && sizeof...(Labels) <= 255);
    static constexpr char const PROGMEM* const PROGMEM OPTIONS[] = {
        DialogueLabelStorage<Labels>::TEXT...,
    };
    return dialogue_options_ex(OPTIONS, sizeof...(Labels), AllowBack);
}

} // namespace dialogue_detail

template<dialogue_detail::DialogueLabel... Labels>
uint8_t dialogue_options() {
    return dialogue_detail::choose<true, Labels...>();
}

template<dialogue_detail::DialogueLabel... Labels>
uint8_t dialogue_options_noback() {
    return dialogue_detail::choose<false, Labels...>();
}

inline constexpr uint8_t DIALOGUE_OPTION_ROWS = 3;
void render_dialogue_options(char const PROGMEM* const* rows, uint8_t count, uint8_t selected,
                             uint8_t baseline, bool above, bool below, bool allow_back);
// Validate the loaded live object before using saved world or quest state.
bool save_valid();
// Validate and place the loaded player, rebuilding transient state. The caller refreshes the world.
bool save_restore();
// Rendering may shrink the world viewport without changing Game or reloading tiles.
void rpg_render_world(uint8_t covered = 0);
// While a battle runs, draws its board as dialogue's background and returns true. Otherwise it draws
// nothing and returns false.
bool battle_render_background();
extern bool dialogue_panel_open;
// Non-dialogue screens close the retained panel before taking over. Blocks through the animation.
void dialogue_close();
// Eight 20 ms steps per half, using flash-resident ordered-dither erase masks.
inline constexpr uint8_t FADE_STEPS = 8;
void show_fade(uint8_t step);
void fade_to_black();
void fade_world_out();
// Requires the destination cache, tiles and actors to be ready. Consumes transition button presses.
void fade_world_in();
// One frame of the title screen. `ticks` counts frames and wraps at TITLE_CYCLE.
void render_title(uint16_t ticks);

enum PAUSE_ITEM : uint8_t {
    PAUSE_RESUME,
    PAUSE_SAVE,
    PAUSE_PARTY,
    PAUSE_SUPPLIES,
    PAUSE_JOURNAL,
    PAUSE_MAP,
    PAUSE_RESIGN,
    PAUSE_AUDIO,
    PAUSE_PASS,
    PAUSE_LOAD,
    PAUSE_ITEM_COUNT
};

enum MENU_EXIT : uint8_t {
    MENU_RESUME,
    MENU_LOAD,
    MENU_AUDIO,
    MENU_PARTY,
    MENU_SUPPLIES,
    MENU_JOURNAL,
    MENU_MAP,
    MENU_PASS,
    MENU_RESIGN
};

constexpr bool menu_opens_screen(MENU_EXIT action) {
    return action >= MENU_AUDIO && action <= MENU_MAP;
}

enum OPTION_ITEM : uint8_t { OPTION_MUSIC, OPTION_SFX, OPTION_ITEM_COUNT };

inline constexpr uint8_t AUDIO_WIDTH = 88;

struct BattleScreen;
void render_battle(BattleScreen const& screen);
struct PauseMenuView;
uint8_t menu_width(PauseMenuView const& view);
void render_menu(uint8_t revealed, uint8_t width, PauseMenuView const& view,
                 char const PROGMEM* feedback, MENU_EXIT action,
                 BattleScreen const* battle = nullptr);
void render_menu_confirmation(PAUSE_ITEM selection);
void render_audio(uint8_t revealed, OPTION_ITEM selection, bool closing,
                  BattleScreen const* battle = nullptr);
uint8_t option_level(OPTION_ITEM item);
void options_apply();
void option_adjust(OPTION_ITEM item, bool increase);
inline constexpr uint8_t PARTY_WIDTH = 128;
struct PartyUi;
// Draws the Party panel for the page in `ui`, sliding in or out at `revealed` pixels.
void render_party(uint8_t revealed, bool closing, PartyUi const& ui);
// Measures the open detail page's description, so its scroll limits are known. Call it when a
// press opens a detail page.
void party_layout(PartyUi& ui);
struct JournalView;
// Fills the detail view's wrapped summary for a quest, after its name has been chosen.
void journal_open(JournalView& view, QUEST_ID id);
// Measures the positions of the detail view's text. It runs when the view opens and when tracking
// changes, so the frame draws without measuring.
void render_journal(uint8_t revealed, bool closing, JournalView const& view);

void input_sync();
uint8_t input_held();
uint8_t input_pressed();
void input_wait_release(uint8_t mask = BTN_ALL);
uint8_t input_wait_press(uint8_t mask = BTN_ALL);
void input_wait_frame();

void title_screen();
void explore();
// Return to the caller before opening Audio/Party or performing an action, keeping modal frames
// shallow.
MENU_EXIT menu_screen(BattleScreen const* battle = nullptr, PAUSE_ITEM initial = PAUSE_RESUME);
void audio_screen(BattleScreen const* battle = nullptr);
void party_screen();
void journal_screen();
void map_screen();
struct WorldMapView;
void render_world_map(WorldMapView const& view, uint8_t ticks, bool markers = true);
void transition_world_map(WorldMapView const& view, uint8_t ticks, bool closing);
extern uint8_t const PROGMEM WORLD_MAP_SPRITE[];
void game_save();
bool game_load();
char const PROGMEM* music_next();

extern uint8_t const PROGMEM TILE_SPRITES[];
// The sprite sheet of a tileset. Each area draws with the tileset from area_tileset(). After the
// atlas frames it holds the wind frames that tile_wind_frames describes.
uint8_t const PROGMEM* tileset_sprites(uint8_t tileset);
// The sheet that draws a tileset's atlas frames in one phase of the tile cycle. Phase 0 is
// tileset_sprites(). Selecting the sheet is the whole cost of the animation.
uint8_t const PROGMEM* tileset_phase_sprites(uint8_t tileset, uint8_t phase);
extern uint8_t const PROGMEM ACTOR_SPRITES[];
// 32x32 NPC faces, two frames per NPC_ID: frame NPC_ID * 2 is the mouth closed, +1 the mouth open.
extern uint8_t const PROGMEM FACE_SPRITES[];
// One byte per NPC_ID: nonzero when the ID has a portrait. An all-black row of faces32.png has
// none.
extern uint16_t const PROGMEM FACE_ROW_COUNT;
extern uint8_t const PROGMEM FACE_PRESENT[];
// 7x7 dialogue prompt indicators: the A button filled and outlined (0, 1), and a down arrow
// high and low (2, 3).
inline constexpr uint8_t PROMPT_FRAME_COUNT = 4;
extern uint8_t const PROGMEM PROMPT_SPRITES[];
// The ambient creatures and effects, ambient16.png: 16x16 plus-mask frames that ambient.h names.
extern uint8_t const PROGMEM AMBIENT_SPRITES[];
// The title screen: the whole 128x64 background picture, the game's name as one plus-mask
// sprite, and title16.png's 16x16 plus-mask frames that title.h names.
extern uint8_t const PROGMEM TITLE_SCENE[];
extern uint8_t const PROGMEM TITLE_LOGO[];
extern uint8_t const PROGMEM TITLE_SPRITES[];
// The title's foreground layers, plus-mask sprites drawn over the background. A pine is a sheet
// of bands eight rows high, top band first; the rock is one frame.
extern uint8_t const PROGMEM TITLE_PINE_SMALL[];
extern uint8_t const PROGMEM TITLE_PINE_LEFT[];
extern uint8_t const PROGMEM TITLE_PINE_RIGHT[];
extern uint8_t const PROGMEM TITLE_ROCK[];
// The battle cursor: 22x22 plus-mask frames, one per dash phase. The generated sheet asserts the
// count.
inline constexpr uint8_t CURSOR_FRAME_COUNT = 4;
extern uint8_t const PROGMEM CURSOR_SPRITES[];
// The battle destination markers in battle_targets16.png. Frames 0 to 3 are the movement diamond,
// and frames 4 to 7 are the attack brackets. A marker shows its base plus the cursor's phase, so
// every marker pulses with the cursor. The generated sheet asserts the layout.
inline constexpr uint8_t BATTLE_TARGET_FRAMES = 4;
inline constexpr uint8_t BATTLE_TARGET_MOVE = 0;
inline constexpr uint8_t BATTLE_TARGET_ATTACK = 4;
static_assert(BATTLE_TARGET_FRAMES == CURSOR_FRAME_COUNT, "markers pulse with the cursor's phases");

constexpr uint8_t battle_target_frame(uint8_t base, uint8_t phase) {
    return (uint8_t)(base + phase % BATTLE_TARGET_FRAMES);
}

extern uint8_t const PROGMEM BATTLE_TARGET_SPRITES[];
// The archer's arrow in flight, battle_arrows16.png: one 16x16 plus-mask frame per DIRECTION, so a
// direction is its own frame index. The generated sheet asserts the count.
inline constexpr uint8_t BATTLE_ARROW_FRAMES = 4;
extern uint8_t const PROGMEM BATTLE_ARROW_SPRITES[];
// The damage indicators, battle_damage16.png: one 16x16 plus-mask frame for each loss of 1 to 9
// HP, a spiky burst with the digit in its black middle. Frame n - 1 shows a loss of n, and a larger
// loss shows the last frame. The generated sheet asserts the count.
inline constexpr uint8_t BATTLE_DAMAGE_FRAMES = 9;

constexpr uint8_t battle_damage_frame(uint8_t lost) {
    return (uint8_t)((lost > BATTLE_DAMAGE_FRAMES ? BATTLE_DAMAGE_FRAMES : lost) - 1);
}

extern uint8_t const PROGMEM BATTLE_DAMAGE_SPRITES[];

#endif
