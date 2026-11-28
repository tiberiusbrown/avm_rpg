#include "battle.h"
#include "battle_ai.h"
#include "battle_cursor.h"
#include "battle_motion.h"
#include "music_catalog.h"
#include "party_ui.h"
#include "runtime_workspace.h"
#include "sfx.h"

#include <avm.h>
#include <stdio.h>
#include <string.h>

// Frames at 50 Hz. The party's result stays on screen for this long before the enemy chooses.
static constexpr uint8_t RESULT_FRAMES = 25;

static void pause(uint8_t frames) {
    while(frames--)
        input_wait_frame();
}

static int16_t cell_x(uint8_t cell) {
    return (int16_t)((cell % BATTLE_COLUMNS) * TILE);
}

static int16_t cell_y(uint8_t cell) {
    return (int16_t)((cell / BATTLE_COLUMNS) * TILE);
}

// Unit steps for a lunge in direction d.
static int8_t direction_dx(DIRECTION d) {
    return (int8_t)(d == EAST ? 1 : d == WEST ? -1 : 0);
}

static int8_t direction_dy(DIRECTION d) {
    return (int8_t)(d == SOUTH ? 1 : d == NORTH ? -1 : 0);
}

// The cursor is the 22x22 plus-mask sprite CURSOR_SPRITES, centered on the 16x16 cell. Its frames
// carry the black borders, the dashed ring, and the black frame, and the cell's interior is
// transparent. Frame n is dash phase n, so animating the cursor means cycling its frames.
static constexpr uint8_t CURSOR_MARGIN = 3;
// Frames per dash-phase step.
static constexpr uint8_t CURSOR_DASH_FRAMES = 6;

// The logical cell is what input moves. The rendered origin eases toward that cell, so the two are
// tracked independently. Rendered coordinates are in CURSOR_SUBPIXELS units.
struct Cursor {
    uint8_t cell;
    int16_t x, y;
    uint8_t phase;
    uint8_t frames;
};

enum SCREEN_MOTION : uint8_t {
    MOTION_STILL, MOTION_WALK, MOTION_LEAP, MOTION_CHARGE, MOTION_VAULT, MOTION_GLIDE, MOTION_SHOT
};

static constexpr uint8_t SLIDE_FRAMES = 8;

// One blocking battle owns this fixed screen. Keeping it off the call stack leaves room for
// full-byte HP and the AI while staying within the guest's 256-byte stack budget.
struct BattleScreen {
    BattleState state;
    Cursor cursor;
    // The unit drawn at an offset from its cell: a walker, or an attacker lunging at its victim.
    // BATTLE_NO_UNIT when none. Only one unit is offset at a time.
    uint8_t mover;
    // The mover's drawn offset from its destination for a move, or from its animated cell for a lunge.
    int8_t mover_dx, mover_dy;
    // During movement, a wind-up, or a projectile flight, units about to flash stay visible.
    uint8_t motion; // SCREEN_MOTION
    // Bit i is set for each unit flashing after damage.
    uint8_t flashing;
    // HP change by unit. An unflashed value records healing.
    uint8_t damage[BATTLE_UNIT_CAPACITY];
    // Presentation only. Keep the original cells visible until contact, then interpolate every
    // forced displacement together. Formation moves and swaps also slide from these cells.
    uint8_t origin[BATTLE_UNIT_CAPACITY];
    uint8_t slide;
    uint8_t actor, victim;
    uint8_t attack;
    BATTLE_ATTACK_CHOICE choice;
    char attack_banner[40];
    uint8_t banner_width, banner_y;
    // Animation clock in frames. A walk advances it for the walking pose, and a flash counts it for
    // the flash phase. A walk and a flash never run together, so one clock serves both. It wraps at
    // 256, which keeps the pose cycle whole.
    uint8_t ticks;
    // The enemy and highlighted option of an attack offer. Zero rows means it is closed.
    uint8_t offer_cell, offer_choice, offer_rows;
};

#ifdef RPG_DEVICE_QA
extern "C" {
BattleState const* volatile battle_qa_state;
volatile BATTLE_QA_PHASE battle_qa_ready;
}
#endif

// The screen stays outside the runtime workspace, so dialogue can draw the board while the battle's
// search owns that workspace. battle_running says whether a battle is in progress.
static BattleScreen active_screen;
static bool battle_running;

// A battle panel: a black interior inside a one-pixel white outline, with a one-pixel black outline
// just outside the white. The black keeps the panel apart from whatever is drawn beneath it.
static inline __attribute__((always_inline)) void draw_panel(int16_t x, int16_t y, int16_t width,
                                                             int16_t height) {
    avm_draw_filled_rect_black(x - 1, y - 1, width + 2, height + 2);
    avm_draw_filled_rect_white(x, y, width, height);
    avm_draw_filled_rect_black(x + 1, y + 1, width - 2, height - 2);
}

static char const PROGMEM* battle_unit_name(BattleUnit const& unit) {
    return unit.id & BATTLE_PARTY_SOURCE
               ? party_member_name(game.party.members[unit.id & ~BATTLE_PARTY_SOURCE])
               : battle_enemy_name(unit.id);
}

static constexpr uint8_t ATTACK_BANNER_HEIGHT = 10;

RPG_NOINLINE static void prepare_attack_banner(BattleScreen& screen) {
    auto const& state = screen.state;
    auto actor = battle_unit_name(state.units[screen.actor]);
    if(screen.choice == BATTLE_NORMAL) {
        auto victim = battle_unit_name(state.units[screen.victim]);
        snprintf_P(screen.attack_banner, sizeof(screen.attack_banner), F("%S attacks %S!"),
                   actor, victim);
    } else {
        auto ability = party_special_name(screen.attack, screen.choice == BATTLE_SIGNATURE);
        snprintf_P(screen.attack_banner, sizeof(screen.attack_banner), F("%S uses %S!"),
                   actor, ability);
    }
    screen.banner_width = avm_text_width(screen.attack_banner) + 6;

    // Prefer the edge with fewer affected units. When both edges are equally clear, keep the
    // banner farther from the primary target.
    uint8_t top = 0, bottom = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        auto const& unit = state.units[i];
        if(i != screen.actor && i != screen.victim && !screen.damage[i] &&
           unit.cell == screen.origin[i])
            continue;
        uint8_t from = battle_row(screen.origin[i]), to = battle_row(unit.cell);
        top += from == 0 || to == 0;
        bottom += from == BATTLE_ROWS - 1 || to == BATTLE_ROWS - 1;
    }
    bool at_bottom = top > bottom ||
                     (top == bottom && battle_row(screen.origin[screen.victim]) < BATTLE_ROWS / 2);
    screen.banner_y = at_bottom ? BATTLE_ROWS * TILE - ATTACK_BANNER_HEIGHT - 1 : 1;
}

RPG_NOINLINE static void draw_attack_banner(BattleScreen const& screen) {
    int16_t x = (BATTLE_COLUMNS * TILE - screen.banner_width) / 2;
    draw_panel(x, screen.banner_y, screen.banner_width, ATTACK_BANNER_HEIGHT);
    avm_draw_text(x + 3, screen.banner_y + 7, screen.attack_banner);
}

// The sprite is drawn at the cell's origin minus the margin, so its interior lands on the cell.
static void draw_cursor(Cursor const& cursor) {
    avm_set_sprite(CURSOR_SPRITES);
    avm_draw_plus_mask((int16_t)(cursor_pixel(cursor.x) - CURSOR_MARGIN),
                       (int16_t)(cursor_pixel(cursor.y) - CURSOR_MARGIN), cursor.phase);
}

// Inspect the living actor under the logical cursor, independently of the selected hero. Keep
// the panel vertically opposite the cursor so it cannot cover the actor being inspected.
RPG_NOINLINE static void draw_actor_info(BattleState const& state, Cursor const& cursor) {
    uint8_t index = battle_unit_at(state, cursor.cell);
    if(index == BATTLE_NO_UNIT)
        return;

    BattleUnit const& unit = state.units[index];
    static constexpr char PROGMEM RANK_SUFFIX[PERK_TRACK_RANKS + 1][3] = {
        "", " 1", " 2", " 3", " 4",
    };
    char classes[32];
    snprintf_P(classes, sizeof(classes), F("%S%S / %S%S"),
               party_movement_name(battle_movement_class(unit.id)), RANK_SUFFIX[battle_movement_rank(unit)],
               party_attack_name(battle_attack_class(unit.id)), RANK_SUFFIX[battle_attack_rank(unit)]);

    char stats[sizeof("HP 255/255  AP 255")];
    uint8_t max_hp = battle_max_hp(unit);
    uint8_t ap = battle_ap(unit);
    bool show_ap = unit.team == BATTLE_ENEMY
                       ? ap != 0
                       : (unit.id & BATTLE_PARTY_SOURCE) &&
                             game.party.members[unit.id & ~BATTLE_PARTY_SOURCE].level >=
                                 BATTLE_STARTER_LEVEL;
    if(show_ap)
        snprintf_P(stats, sizeof(stats), F("HP %u/%u  AP %u"), unit.hp, max_hp, ap);
    else
        snprintf_P(stats, sizeof(stats), F("HP %u/%u"), unit.hp, max_hp);

    int16_t class_width = avm_text_width(classes);
    int16_t stats_width = avm_text_width(stats);
    int16_t width = (class_width > stats_width ? class_width : stats_width) + 6;
    int16_t x = (BATTLE_COLUMNS * TILE - width) / 2;
    int16_t y = battle_row(cursor.cell) < BATTLE_ROWS / 2 ? 45 : 2;
    draw_panel(x, y, width, 17);
    avm_draw_text(x + 3, y + 7, classes);
    avm_draw_text(x + 3, y + 14, stats);
}

// Moves the rendered cursor halfway toward its logical cell and advances the dash pattern.
static void animate_cursor(Cursor& cursor) {
    cursor.x = cursor_ease(cursor.x, cursor_subpixel(cell_x(cursor.cell)));
    cursor.y = cursor_ease(cursor.y, cursor_subpixel(cell_y(cursor.cell)));
    if(++cursor.frames == CURSOR_DASH_FRAMES) {
        cursor.frames = 0;
        cursor.phase = (uint8_t)((cursor.phase + 1) % CURSOR_FRAME_COUNT);
    }
}

// Waits one frame and returns every button press seen during that wait. Polling throughout the
// frame, rather than once, keeps a short tap from being lost.
static uint8_t wait_frame_presses() {
    uint8_t pressed = 0;
    while(!avm_next_frame()) {
        pressed = (uint8_t)(pressed | input_pressed());
        avm_idle();
    }
    return pressed;
}

// An attack marker: four black-outlined white brackets at the corners of an enemy's tile. The
// middle of the tile stays transparent, so the enemy shows through.
static void draw_attack_mark(int16_t x, int16_t y, uint8_t phase) {
    avm_draw_plus_mask(x, y, battle_target_frame(BATTLE_TARGET_ATTACK, phase));
}

// A movement marker: a white diamond with a black outline, centered on an empty cell that the
// selected unit can enter.
static void draw_move_mark(int16_t x, int16_t y, uint8_t phase) {
    avm_draw_plus_mask(x, y, battle_target_frame(BATTLE_TARGET_MOVE, phase));
}

// A one-pixel white square outline, `size` pixels on each side.
static inline __attribute__((always_inline)) void draw_outline(int16_t x, int16_t y, uint8_t size) {
    avm_draw_filled_rect_white(x, y, size, 1);
    avm_draw_filled_rect_white(x, y + size - 1, size, 1);
    avm_draw_filled_rect_white(x, y, 1, size);
    avm_draw_filled_rect_white(x + size - 1, y, 1, size);
}

// The selected unit's border: a 16x16 outline and a 14x14 outline inside it, both centered on the
// cell. Together they make a two-pixel white border, which the actor is drawn over.
static inline __attribute__((always_inline)) void draw_selection(int16_t x, int16_t y) {
    draw_outline(x, y, TILE);
    draw_outline((int16_t)(x + 1), (int16_t)(y + 1), TILE - 2);
}

static DIRECTION pressed_direction(uint8_t pressed) {
    if(pressed & BTN_UP)
        return NORTH;
    if(pressed & BTN_RIGHT)
        return EAST;
    if(pressed & BTN_DOWN)
        return SOUTH;
    if(pressed & BTN_LEFT)
        return WEST;
    return STILL;
}

static void move_cursor(Cursor& cursor, uint8_t pressed) {
    DIRECTION dir = pressed_direction(pressed);
    if(dir == STILL)
        return;
    uint8_t next = cursor.cell;
    bool moved = battle_neighbor(cursor.cell, dir, next);
    if(moved)
        cursor.cell = next;
    sfx_play(moved ? SFX_CURSOR_MOVE : SFX_CURSOR_EDGE);
}

// A leap unit's height above its straight path, from the offset it still has to cover along the long
// axis.
static uint8_t leap_lift(BattleScreen const& screen) {
    int8_t ax = screen.mover_dx < 0 ? (int8_t)-screen.mover_dx : screen.mover_dx;
    int8_t ay = screen.mover_dy < 0 ? (int8_t)-screen.mover_dy : screen.mover_dy;
    uint8_t remaining = (uint8_t)(ax > ay ? ax : ay);
    if(screen.motion == MOTION_VAULT) {
        uint8_t left = remaining / BATTLE_LEAP_PIXELS;
        uint8_t half = screen.ticks + left > 8 ? 6 : 4;
        return left > half ? (uint8_t)((2 * half - left) * 2) : (uint8_t)(left * 2);
    }
    return battle_leap_lift(remaining);
}

static constexpr SCREEN_MOTION PROGMEM MOVE_MOTION[MOVEMENT_CLASS_COUNT] = {
    MOTION_WALK, MOTION_LEAP, MOTION_CHARGE, MOTION_VAULT, MOTION_GLIDE,
};

static int16_t slide_pixel(int16_t from, int16_t to, uint8_t progress) {
    return (int16_t)(from + (to - from) * progress / SLIDE_FRAMES);
}

RPG_NOINLINE static void draw_battle_terrain(uint8_t field_id) {
    auto const& field = BATTLEFIELDS[field_id];
    uint8_t tiles[BATTLE_CELLS];
    memcpy_P(tiles, field.tiles, sizeof(tiles));
    avm_set_sprite(tileset_sprites(field.tileset));
    avm_draw_tilemap_u8_overwrite(0, 0, tiles, BATTLE_COLUMNS, BATTLE_ROWS, BATTLE_COLUMNS);
}

// Draws the whole board. Terrain covers every pixel, so nothing needs clearing first. The selected
// unit gets its white border before the actors. The destination markers come after the actors, so
// they stay visible over the terrain and the unit art. The cursor is drawn separately, after the
// board. The mover is drawn even once defeated, so a unit that dies on its move is seen walking
// first. A flashing unit is drawn only on the visible phases of its flash (and throughout a walk,
// which comes before the flash), and it too is drawn once defeated. The mover faces `mover_facing`,
// the way it steps or the victim it lunges at. Every other actor stands facing south.
template <bool MARKERS = true>
RPG_NOINLINE static void draw_board(BattleScreen const& screen, uint8_t selected,
                                    DIRECTION mover_facing) {
    BattleState const& state = screen.state;
    draw_battle_terrain(state.field);

    if(selected != BATTLE_NO_UNIT && state.units[selected].hp)
        draw_selection(cell_x(state.units[selected].cell), cell_y(state.units[selected].cell));

    avm_set_sprite(ACTOR_SPRITES);
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        bool moving = i == screen.mover;
        bool flashing = ((screen.flashing >> i) & 1) != 0;
        if(!unit.hp && !moving && !flashing)
            continue;
        // Flashes start once the mover has arrived: while it walks, every unit it is about to hurt,
        // even one already defeated, stays drawn.
        if(flashing && screen.motion == MOTION_STILL && screen.slide == SLIDE_FRAMES &&
           battle_flash_hidden(screen.ticks))
            continue;
        // The mover is drawn at its cell plus the offset it still has to cover, and a leap unit is
        // raised by its arc.
        int16_t x = (int16_t)(cell_x(unit.cell) + (moving ? screen.mover_dx : 0));
        int16_t y = (int16_t)(cell_y(unit.cell) + (moving ? screen.mover_dy : 0));
        if(screen.slide < SLIDE_FRAMES && !(i == screen.actor && screen.victim == BATTLE_NO_UNIT)) {
            uint8_t origin = screen.origin[i];
            x += slide_pixel(cell_x(origin), cell_x(unit.cell), screen.slide) - cell_x(unit.cell);
            y += slide_pixel(cell_y(origin), cell_y(unit.cell), screen.slide) - cell_y(unit.cell);
        }
        if(moving && (screen.motion == MOTION_LEAP || screen.motion == MOTION_VAULT))
            y = (int16_t)(y - leap_lift(screen));
        Actor const pose{(uint16_t)x, (uint16_t)y, moving ? mover_facing : SOUTH,
                         (uint8_t)(moving && (screen.motion == MOTION_WALK ||
                                              screen.motion == MOTION_CHARGE))};
        avm_draw_plus_mask(x, y, actor_frame(pose, battle_sprite_row(unit), screen.ticks));
    }

    // Paused and animated boards have no targets to enumerate.
    if constexpr(!MARKERS)
        return;

    // The destination markers come last, over the actors. They take the cursor's phase, so every
    // marker pulses with the cursor, and none keeps a clock of its own. They come from the same
    // enumeration the rules use, so any legal cell on the board can carry one: attacks lead the
    // list and get brackets, and the moves after them get diamonds.
    // The list goes in the shared scratch, which is idle while the screen draws.
    uint8_t* actions = battle_action_scratch;
    uint8_t attacks = 0;
    uint8_t count = battle_unit_actions(state, selected, actions, attacks);
    uint8_t phase = screen.cursor.phase;
    avm_set_sprite(BATTLE_TARGET_SPRITES);
    for(uint8_t i = 0; i < count; ++i) {
        uint8_t cell = battle_unpack(actions[i]).destination;
        if(i < attacks)
            draw_attack_mark(cell_x(cell), cell_y(cell), phase);
        else
            draw_move_mark(cell_x(cell), cell_y(cell), phase);
    }
}

void render_battle(BattleScreen const& screen) {
    draw_board<false>(screen, BATTLE_NO_UNIT, SOUTH);
    draw_actor_info(screen.state, screen.cursor);
    draw_cursor(screen.cursor);
}

bool battle_render_background() {
    if(!battle_running)
        return false;
    // Dialogue shows the board from the battle's own state, without the cursor or the markers.
    draw_board<false>(active_screen, BATTLE_NO_UNIT, SOUTH);
    return true;
}

// Redraw the frozen board under each mask; the cursor appears after the reveal.
RPG_NOINLINE static void fade_battle_in(BattleScreen const& screen) {
    for(uint8_t step = FADE_STEPS; step--;) {
        draw_board<false>(screen, BATTLE_NO_UNIT, SOUTH);
        show_fade(step);
    }
}

// Four steady white corner ticks with a black surround, packed as interleaved pixels and masks.
static constexpr uint8_t PROGMEM FORMATION_MARKER[] = {
    16, 16,
    0x00, 0x00, 0x00, 0x3e, 0x1c, 0x3e, 0x04, 0x3e,
    0x04, 0x0e, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x04, 0x0e,
    0x04, 0x3e, 0x1c, 0x3e, 0x00, 0x3e, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7c, 0x38, 0x7c, 0x20, 0x7c,
    0x20, 0x70, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x20, 0x70,
    0x20, 0x7c, 0x38, 0x7c, 0x00, 0x7c, 0x00, 0x00,
};

RPG_NOINLINE static void draw_formation_board(BattleScreen const& screen, BattleRect area,
                                             uint8_t selected) {
    draw_board<false>(screen, selected, SOUTH);
    avm_set_sprite(FORMATION_MARKER);
    for(uint8_t y = area.y; y < area.y + area.height; ++y)
        for(uint8_t x = area.x; x < area.x + area.width; ++x) {
            uint8_t cell = battle_cell(x, y);
            if(!battle_blocked(screen.state.field, cell))
                avm_draw_plus_mask(cell_x(cell), cell_y(cell), 0);
        }
    draw_cursor(screen.cursor);
}

RPG_NOINLINE static void arrange_party(BattleScreen& screen) {
    BattleState& state = screen.state;
    Cursor& cursor = screen.cursor;
    auto const& party = BATTLEFIELDS[state.field].party_area;
    BattleRect area{party.x, party.y, party.width, party.height};
    int16_t panel_x = (area.x + area.width) * TILE + 2;

    uint8_t selected = BATTLE_NO_UNIT;
    bool confirming = false;
    screen.actor = BATTLE_NO_UNIT;
    input_sync();
    RPG_QA_MARK(battle_qa_ready = BATTLE_QA_FORMATION);
    for(;;) {
        bool sliding = screen.slide < SLIDE_FRAMES;
        uint8_t target = battle_unit_at(state, cursor.cell);
        bool ally = target != BATTLE_NO_UNIT && state.units[target].team == BATTLE_PARTY;
        bool legal_cell = area.contains(battle_column(cursor.cell), battle_row(cursor.cell)) &&
                          !battle_blocked(state.field, cursor.cell);
        char const PROGMEM* action = nullptr;
        if(selected == BATTLE_NO_UNIT) {
            if(ally)
                action = F("Select");
        } else if(legal_cell && target != selected && (ally || target == BATTLE_NO_UNIT)) {
            action = ally ? F("Swap") : F("Place");
        }

        draw_formation_board(screen, area, selected);
        if(confirming) {
            render_confirmation(F("Accept formation?"));
        } else {
            if(!sliding)
                draw_actor_info(state, cursor);
            // The actor info uses the opposite half, leaving room for both panels.
            if(legal_cell)
                render_formation_banner(panel_x, battle_row(cursor.cell) >= BATTLE_ROWS / 2,
                                        action, selected != BATTLE_NO_UNIT);
        }
        avm_display(false);
        uint8_t pressed = wait_frame_presses();

        if(sliding) {
            ++screen.slide;
        } else if(confirming) {
            if(pressed & BTN_B) {
                confirming = false;
                sfx_play(SFX_CANCEL);
            } else if(pressed & BTN_A) {
                RPG_QA_MARK(battle_qa_ready = BATTLE_QA_BUSY);
                input_wait_release(BTN_A | BTN_B);
                return;
            }
        } else if(pressed & BTN_B) {
            if(selected == BATTLE_NO_UNIT) {
                confirming = true;
                sfx_play(SFX_SELECT);
            } else {
                selected = BATTLE_NO_UNIT;
                sfx_play(SFX_CANCEL);
            }
        } else if(pressed & BTN_A) {
            if(!action) {
                sfx_play(SFX_ERROR);
            } else {
                if(selected == BATTLE_NO_UNIT) {
                    selected = target;
                } else {
                    for(uint8_t i = 0; i < state.unit_count; ++i)
                        screen.origin[i] = state.units[i].cell;
                    if(ally)
                        state.units[target].cell = state.units[selected].cell;
                    state.units[selected].cell = cursor.cell;
                    screen.slide = 0;
                    selected = BATTLE_NO_UNIT;
                }
                sfx_play(SFX_SELECT);
            }
        } else {
            move_cursor(cursor, pressed);
        }
        animate_cursor(cursor);
    }
}

// Moves an offset toward zero by up to `amount` pixels.
static int8_t toward_zero(int8_t offset, int8_t amount) {
    if(offset > 0)
        return (int8_t)(offset > amount ? offset - amount : 0);
    return (int8_t)(offset < -amount ? offset + amount : 0);
}

static void draw_fx(uint8_t frame, int16_t x, int16_t y);

// Show damage to crossed enemies during the move. The board already supplies the HP changes,
// so a contact spark can only occur on a unit this move actually hit.
RPG_NOINLINE static void draw_walk_contacts(BattleScreen const& screen) {
    uint8_t dest = screen.state.units[screen.actor].cell;
    int16_t x = cell_x(dest) + screen.mover_dx;
    int16_t y = cell_y(dest) + screen.mover_dy;
    int8_t reach = screen.motion == MOTION_GLIDE ? 18 : 5;
    uint8_t hits = screen.flashing & ~(1 << screen.actor);
    for(uint8_t i = 0; hits; ++i, hits >>= 1) {
        if(!(hits & 1))
            continue;
        uint8_t cell = screen.origin[i];
        int16_t dx = cell_x(cell) - x, dy = cell_y(cell) - y;
        if(dx >= -reach && dx <= reach && dy >= -reach && dy <= reach)
            draw_fx(screen.motion == MOTION_VAULT ? ATTACK_IMPACT : ATTACK_STRIKE,
                    cell_x(cell) + 8, cell_y(cell) + 6);
    }
}

// Animates the mover from its starting offset to the authoritative destination.
RPG_NOINLINE static void walk_unit(BattleScreen& screen) {
    SCREEN_MOTION motion = (SCREEN_MOTION)screen.motion;
    // The offset moves toward zero, so the walker faces the opposite way along its longer axis.
    int8_t ax = screen.mover_dx < 0 ? (int8_t)-screen.mover_dx : screen.mover_dx;
    int8_t ay = screen.mover_dy < 0 ? (int8_t)-screen.mover_dy : screen.mover_dy;
    DIRECTION facing =
        ax >= ay ? (screen.mover_dx < 0 ? EAST : WEST) : (screen.mover_dy < 0 ? SOUTH : NORTH);
    int8_t trail_dy = motion == MOTION_GLIDE ? (screen.mover_dy > 0 ? -1 : 1) : direction_dy(facing);
    int8_t step_x = motion == MOTION_WALK ? 1 : 2, step_y = step_x;
    if(motion == MOTION_LEAP || motion == MOTION_VAULT) {
        step_x = ax > ay ? BATTLE_LEAP_PIXELS : BATTLE_LEAP_PIXELS / 2;
        step_y = ay > ax ? BATTLE_LEAP_PIXELS : BATTLE_LEAP_PIXELS / 2;
    }
    static constexpr SFX PROGMEM sounds[] = {SFX_NONE, SFX_NONE, SFX_LEAP, SFX_CHARGE, SFX_VAULT, SFX_GLIDE};
    sfx_play(sounds[motion]);
    screen.ticks = 0;
    while(screen.mover_dx != 0 || screen.mover_dy != 0) {
        if(motion == MOTION_WALK && screen.ticks % BATTLE_STEP_SOUND_FRAMES == 0)
            sfx_play(SFX_STEP);
        int8_t boost = motion == MOTION_CHARGE && screen.ticks >= 3 ? 2 : 0;
        screen.mover_dx = toward_zero(screen.mover_dx, (int8_t)(step_x + boost));
        screen.mover_dy = toward_zero(screen.mover_dy, (int8_t)(step_y + boost));
        ++screen.ticks;
        // The walker is drawn over the terrain with no highlight, so only the moving actor changes.
        draw_board<false>(screen, BATTLE_NO_UNIT, facing);
        if(motion == MOTION_GLIDE || motion == MOTION_CHARGE) {
            uint8_t cell = screen.state.units[screen.mover].cell;
            int16_t x = cell_x(cell) + screen.mover_dx + 7;
            int16_t y = cell_y(cell) + screen.mover_dy + 10;
            int8_t dx = direction_dx(facing);
            avm_draw_filled_rect_white(x - 8 * dx, y - 8 * trail_dy, trail_dy ? 1 : 4, dx ? 1 : 4);
            avm_draw_filled_rect_white(x - 12 * dx, y - 12 * trail_dy + 2, trail_dy ? 1 : 3,
                                       dx ? 1 : 3);
        }
        if(motion == MOTION_CHARGE || motion == MOTION_VAULT || motion == MOTION_GLIDE)
            draw_walk_contacts(screen);
        avm_display(false);
        input_wait_frame();
    }
    screen.mover = BATTLE_NO_UNIT;
    screen.motion = MOTION_STILL;
}

// Draws a damage indicator over every flashing unit: a spiky burst on the unit's cell, with the damage
// dealt in its black middle. It stays up for the whole flash, over the visible and the
// hidden phases alike, and it is drawn after the board, so no neighbouring actor covers it.
RPG_NOINLINE static void draw_damage(BattleScreen const& screen) {
    avm_set_sprite(BATTLE_DAMAGE_SPRITES);
    for(uint8_t i = 0; i < screen.state.unit_count; ++i) {
        uint8_t damage = screen.damage[i];
        if(!damage)
            continue;
        uint8_t cell = screen.state.units[i].cell;
        int16_t x = cell_x(cell), y = cell_y(cell);
        if(screen.slide < SLIDE_FRAMES && i != screen.actor) {
            x = slide_pixel(cell_x(screen.origin[i]), x, screen.slide);
            y = slide_pixel(cell_y(screen.origin[i]), y, screen.slide);
        }
        if(!((screen.flashing >> i) & 1)) {
            avm_draw_filled_rect_black(x + 1, y + 4, 14, 7);
            avm_draw_textf(x + (damage < 10 ? 4 : 2), y + 10, F("+%u"), damage);
            continue;
        }
        avm_draw_plus_mask(x, y, battle_damage_frame(damage));
    }
}

// Whether any flashing unit has been defeated.
RPG_NOINLINE static bool any_fallen(BattleScreen const& screen) {
    for(uint8_t i = 0; i < screen.state.unit_count; ++i)
        if(((screen.flashing >> i) & 1) && !screen.state.units[i].hp)
            return true;
    return false;
}

// Extra cells in battle_effects8.png after the contact glyphs.
static constexpr uint8_t FX_ORB = ATTACK_CLASS_COUNT;
static constexpr uint8_t FX_FOCUS = FX_ORB + 1;
static constexpr uint8_t FX_RING = FX_ORB + 2;
static constexpr uint8_t FX_ORB_CORE = FX_ORB + 3;
static constexpr uint8_t FX_BLAST_RING = FX_ORB + 4;

static void draw_fx(uint8_t frame, int16_t x, int16_t y) {
    avm_set_sprite(BATTLE_FX);
    avm_draw_plus_mask(x - 4, y - 4, frame);
}

static constexpr SFX PROGMEM ATTACK_CONTACT[ATTACK_CLASS_COUNT] = {
    SFX_STRIKE, SFX_ARROW_HIT, SFX_THRUST, SFX_SMASH, SFX_BITE,
    SFX_SWEEP, SFX_DRAIN, SFX_RECOIL, SFX_FLANK, SFX_BURST,
};

// A moving dot between two cells. A phase of 0 starts at the source and 8 reaches the recipient.
static void draw_link(uint8_t from, uint8_t to, uint8_t phase) {
    int16_t x = slide_pixel(cell_x(from), cell_x(to), phase) + 7;
    int16_t y = slide_pixel(cell_y(from), cell_y(to), phase) + 7;
    avm_draw_filled_rect_black(x - 1, y - 1, 5, 5);
    avm_draw_filled_rect_white(x, y, 3, 3);
}

RPG_NOINLINE static void draw_contact(BattleScreen const& screen) {
    uint8_t attack = screen.attack, age = screen.ticks;
    uint8_t actor = screen.actor, victim = screen.victim;
    if(victim == BATTLE_NO_UNIT)
        return;
    uint8_t from = screen.origin[actor], to = screen.origin[victim];

    if(age < 16) {
        if(attack == ATTACK_REACH || (attack == ATTACK_STRIKE && screen.choice == BATTLE_SIGNATURE) ||
           (attack == ATTACK_REND && !battle_cells_adjacent(from, to))) {
            draw_link(from, to, age / 2);
            if(attack == ATTACK_REACH && screen.choice == BATTLE_SIGNATURE)
                draw_link(to, from, age / 2);
        }
        if(attack == ATTACK_ARC && screen.choice == BATTLE_SIGNATURE) {
            uint8_t quarter = age / 4;
            int16_t x = cell_x(from) + 8 + 12 * direction_dx((DIRECTION)quarter);
            int16_t y = cell_y(from) + 8 + 12 * direction_dy((DIRECTION)quarter);
            draw_fx(ATTACK_ARC, x, y);
        }
        if(screen.flashing & (1 << actor))
            draw_link(to, from, age / 2); // Shield Rebound.
    }

    for(uint8_t i = 0; i < screen.state.unit_count; ++i) {
        auto const& unit = screen.state.units[i];
        uint8_t cell = screen.origin[i];
        bool hit = (screen.flashing >> i) & 1;
        if(hit && i != victim && age < 12 && screen.choice == BATTLE_SIGNATURE) {
            if((attack == ATTACK_SHOOT || attack == ATTACK_IMPACT) && battle_cells_adjacent(cell, to) &&
               (battle_row(from) == battle_row(to) ? battle_column(cell) == battle_column(to)
                                                   : battle_row(cell) == battle_row(to)))
                draw_link(to, cell, age / 2);
            if(attack == ATTACK_BLAST && cell != unit.cell)
                draw_link(cell, to, age / 2);
        }
        if(attack == ATTACK_FLANK && age < 16 && i != actor && unit.hp &&
           unit.team == screen.state.units[actor].team && battle_cells_adjacent(cell, to)) {
            auto rules = battle_rules(unit.id);
            if(screen.choice != BATTLE_SIGNATURE ||
               (rules.geometry == BATTLE_ATTACK_LINE && battle_in_attack_range(rules, 1)))
                draw_link(cell, to, age / 2);
        }
        if(hit && (age < 12 || (age < 18 && (age & 2)))) {
            int16_t x = slide_pixel(cell_x(cell), cell_x(unit.cell), screen.slide) + 8;
            int16_t y = slide_pixel(cell_y(cell), cell_y(unit.cell), screen.slide) + 8;
            uint8_t frame = attack;
            if(attack == ATTACK_BLAST && (age < 4 || age >= 12))
                frame = screen.choice == BATTLE_SIGNATURE && age < 4 ? FX_ORB_CORE : FX_BLAST_RING;
            else if(screen.choice != BATTLE_NORMAL && age < 4)
                frame = FX_RING;
            draw_fx(frame, x, y - (age >= 12 ? 7 : 0));
        } else if(attack == ATTACK_DRAIN && screen.damage[i] && !hit && age < 24) {
            draw_link(to, cell, age < 16 ? age / 2 : 8);
            if(age >= 16)
                draw_fx(FX_FOCUS, cell_x(cell) + 8, cell_y(cell) + 2);
        }
    }
}

// Plays contact, displacement, and HP indicators after the move or projectile arrives.
RPG_NOINLINE static void animate_reaction(BattleScreen& screen, uint8_t lunger, DIRECTION toward,
                                          uint8_t frames, SFX impact, int8_t diagonal_y = 0) {
    screen.mover = lunger;
    screen.motion = MOTION_STILL;
    bool attack = screen.victim != BATTLE_NO_UNIT;
    int8_t dx = direction_dx(toward), dy = diagonal_y ? diagonal_y : direction_dy(toward);
    sfx_play(impact);
    for(screen.ticks = 0; screen.ticks < frames; ++screen.ticks) {
        if(screen.ticks == BATTLE_FALL_SOUND_FRAME && any_fallen(screen))
            sfx_play(SFX_FALL);
        if(screen.ticks >= 4 && screen.slide < SLIDE_FRAMES)
            ++screen.slide;
        uint8_t reach = lunger == BATTLE_NO_UNIT ? 0 : battle_lunge_pixels(screen.ticks);
        if(screen.attack == ATTACK_REACH)
            reach *= 2;
        screen.mover_dx = (int8_t)(dx * reach);
        screen.mover_dy = (int8_t)(dy * reach);
        draw_board<false>(screen, BATTLE_NO_UNIT, reach ? toward : SOUTH);
        if(attack && screen.ticks < 24)
            draw_contact(screen);
        if(!attack || screen.ticks >= 12)
            draw_damage(screen);
        if(attack)
            draw_attack_banner(screen);
        avm_display(false);
        input_wait_frame();
    }
    screen.mover = BATTLE_NO_UNIT;
    screen.flashing = 0;
    screen.slide = SLIDE_FRAMES;
    screen.mover_dx = screen.mover_dy = 0;
}

// Determines the motion from the acting class, then records the HP delta of each affected unit.
RPG_NOINLINE static DIRECTION act(BattleScreen& screen, BattleAction action,
                                  BATTLE_ATTACK_CHOICE choice) {
    BattleState& state = screen.state;
    uint8_t actor = action.unit_index;
    BattleUnit const& unit = state.units[actor];
    uint8_t from = unit.cell;
    DIRECTION toward = battle_direction(from, action.destination);
    screen.mover = actor;
    screen.actor = actor;
    screen.victim = battle_unit_at(state, action.destination);
    screen.attack = battle_attack_class(unit.id);
    screen.choice = choice;
    screen.slide = 0;
    bool move = screen.victim == BATTLE_NO_UNIT;
    screen.motion = move ? MOVE_MOTION[battle_movement_class(unit.id)] : MOTION_STILL;

    // Use the animation buffer for the pre-hit HP until the action has resolved.
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        screen.damage[i] = state.units[i].hp;
        screen.origin[i] = state.units[i].cell;
    }
    if(choice == BATTLE_NORMAL)
        battle_apply(state, action);
    else
        battle_apply_special(state, action, choice);
    screen.mover_dx = move ? (int8_t)(cell_x(from) - cell_x(unit.cell)) : 0;
    screen.mover_dy = move ? (int8_t)(cell_y(from) - cell_y(unit.cell)) : 0;

    screen.flashing = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        uint8_t hp = state.units[i].hp;
        uint8_t before = screen.damage[i];
        screen.damage[i] = (uint8_t)(hp > before ? hp - before : before - hp);
        if(hp < before)
            screen.flashing |= (uint8_t)(1 << i);
    }
    return toward;
}

RPG_NOINLINE static void wind_up(BattleScreen& screen, DIRECTION toward) {
    screen.motion = MOTION_SHOT;
    bool signature = screen.choice == BATTLE_SIGNATURE;
    sfx_play(signature ? SFX_SIGNATURE_CAST : SFX_SPECIAL_CAST);
    uint8_t from = screen.origin[screen.actor];
    for(uint8_t frame = 0; frame < (signature ? 9 : 6); ++frame) {
        draw_board<false>(screen, BATTLE_NO_UNIT, toward);
        draw_fx(frame < 3 || (signature && frame >= 6) ? FX_RING : FX_FOCUS,
                cell_x(from) + 8, cell_y(from) + 7);
        draw_attack_banner(screen);
        avm_display(false);
        input_wait_frame();
    }
    screen.motion = MOTION_STILL;
}

// Throughshot flies down the whole hit line. Blast sends a separate, glowing projectile.
RPG_NOINLINE static void fly_projectile(BattleScreen& screen, uint8_t to, DIRECTION toward) {
    screen.motion = MOTION_SHOT;
    bool orb = screen.attack == ATTACK_BLAST;
    sfx_play(orb ? SFX_ORB : SFX_BOW);
    uint8_t from = screen.origin[screen.actor];
    if(!orb) {
        for(uint8_t i = 0; i < screen.state.unit_count; ++i) {
            uint8_t cell = screen.origin[i];
            if(!(screen.flashing & (1 << i)) || cell == to)
                continue;
            if(battle_direction(to, cell) == toward &&
               (cell_x(from) == cell_x(cell) || cell_y(from) == cell_y(cell)))
                to = cell;
        }
    }
    uint8_t cells =
        (uint8_t)(battle_column(from) == battle_column(to)
                      ? (battle_row(from) > battle_row(to) ? battle_row(from) - battle_row(to)
                                                           : battle_row(to) - battle_row(from))
                      : (battle_column(from) > battle_column(to)
                             ? battle_column(from) - battle_column(to)
                             : battle_column(to) - battle_column(from)));
    uint8_t frames = battle_arrow_frames(cells);
    for(uint8_t frame = 0; frame < frames; ++frame) {
        draw_board<false>(screen, BATTLE_NO_UNIT, toward);
        int16_t offset = battle_arrow_offset(frame);
        int16_t x = (int16_t)(cell_x(from) + direction_dx(toward) * offset);
        int16_t y = (int16_t)(cell_y(from) + direction_dy(toward) * offset);
        if(orb)
            draw_fx(frame & 1 ? FX_ORB : FX_ORB_CORE, x + 8, y + 8);
        else {
            avm_set_sprite(BATTLE_ARROW_SPRITES);
            avm_draw_plus_mask(x, y, toward);
        }
        draw_attack_banner(screen);
        avm_display(false);
        input_wait_frame();
    }
    screen.motion = MOTION_STILL;
}

// Resolves a legal action and plays its motion. A move walks the unit to its new cell. An archer's
// attack flies an arrow to its victim. Any other attack lunges the attacker at its victim. Every
// unit the action damages flashes once the motion is done, and the caller waits for the flashes to
// end.
RPG_NOINLINE static void resolve(BattleScreen& screen, BattleAction action,
                                 BATTLE_ATTACK_CHOICE choice) {
    if(action.unit_index == BATTLE_NO_UNIT) {
        screen.actor = screen.victim = BATTLE_NO_UNIT;
        screen.slide = SLIDE_FRAMES;
        screen.flashing = 0;
        for(uint8_t i = 0; i < screen.state.unit_count; ++i) {
            BattleUnit const& unit = screen.state.units[i];
            screen.damage[i] = 0;
            if(unit.hp && unit.team == screen.state.turn) {
                screen.flashing |= (uint8_t)(1 << i);
                screen.damage[i] = BATTLE_MOVE_COST;
            }
        }
        battle_pass(screen.state);
        if(screen.flashing)
            animate_reaction(screen, BATTLE_NO_UNIT, SOUTH,
                             any_fallen(screen) ? BATTLE_FLASH_FRAMES : BATTLE_PRICE_FLASH_FRAMES,
                             SFX_PRICE);
        return;
    }
    DIRECTION toward = act(screen, action, choice);
    uint8_t actor = action.unit_index;
    BattleUnit const& unit = screen.state.units[actor];
    if(unit.team == BATTLE_PARTY) {
        // The cursor returns to the hero's resolved cell when input resumes.
        screen.cursor.cell = unit.cell;
        screen.cursor.x = cursor_subpixel(cell_x(unit.cell));
        screen.cursor.y = cursor_subpixel(cell_y(unit.cell));
    }
    uint8_t lunger = actor;
    uint8_t attack = screen.attack;
    SFX impact = ATTACK_CONTACT[attack];
    int8_t diagonal_y = 0;
    if(screen.motion != MOTION_STILL) {
        walk_unit(screen);
        lunger = BATTLE_NO_UNIT;
        impact = screen.flashing & ~(1 << actor) ? SFX_CLASH : SFX_PRICE;
    } else {
        prepare_attack_banner(screen);
        if(attack == ATTACK_RECOIL && screen.origin[actor] == unit.cell)
            impact = SFX_STRIKE;
        if(choice != BATTLE_NORMAL)
            wind_up(screen, toward);
        if(attack == ATTACK_SHOOT || attack == ATTACK_BLAST) {
            fly_projectile(screen, action.destination, toward);
            lunger = BATTLE_NO_UNIT;
        } else if(battle_row(action.destination) != battle_row(screen.origin[actor]) &&
                  battle_column(action.destination) != battle_column(screen.origin[actor])) {
            diagonal_y = battle_row(action.destination) < battle_row(screen.origin[actor]) ? -1 : 1;
        }
    }
    if(screen.flashing) {
        bool price_only = screen.flashing == (1 << actor) && screen.state.units[actor].hp;
        animate_reaction(screen, lunger, toward,
                         price_only ? BATTLE_PRICE_FLASH_FRAMES : BATTLE_FLASH_FRAMES, impact,
                         diagonal_y);
    }
    screen.slide = SLIDE_FRAMES;
}

// Handles an A press on `cell`. A friendly hero toggles its selection, replacing any other.
// Otherwise the selected hero acts on the cell if that is legal. Returns the action taken, or one
// with no unit when no action is taken. A selection chirps, deselection cancels, and an invalid
// press buzzes; a confirmed action is heard through its own motion. Kept out of line so
// party_turn's frame stays small.
RPG_NOINLINE static BattleAction press_a(BattleScreen& screen, uint8_t& selected, uint8_t cell) {
    BattleState const& state = screen.state;
    uint8_t unit = battle_unit_at(state, cell);
    if(unit != BATTLE_NO_UNIT && state.units[unit].team == BATTLE_PARTY) {
        selected = selected == unit ? BATTLE_NO_UNIT : unit;
        sfx_play(selected == BATTLE_NO_UNIT ? SFX_CANCEL : SFX_SELECT);
    } else {
        if(selected != BATTLE_NO_UNIT) {
            BattleAction action{selected, cell};
            if(battle_legal(state, action)) {
                // A special can reach this enemy too, so the offer decides between the attacks.
                if(battle_special_legal(state, action)) {
                    screen.offer_rows = battle_special_legal(state, action, BATTLE_SIGNATURE) ? 3 : 2;
                    screen.offer_cell = cell;
                    screen.offer_choice = 0;
                    sfx_play(SFX_SELECT);
                    return BattleAction{BATTLE_NO_UNIT, 0};
                }
                return action;
            }
        }
        sfx_play(SFX_ERROR);
    }
    return BattleAction{BATTLE_NO_UNIT, 0};
}

// The offer's input. UP and DOWN move the highlight between the attack and the special, A confirms
// the highlighted option, and B closes the offer without acting. The selected hero stays selected
// either way.
RPG_NOINLINE static BattleAction press_offer(BattleScreen& screen, uint8_t selected,
                                             uint8_t pressed, BATTLE_ATTACK_CHOICE& choice) {
    if(pressed & BTN_B) {
        sfx_play(SFX_CANCEL);
        screen.offer_rows = 0;
    } else if(pressed & BTN_A) {
        sfx_play(SFX_SELECT);
        screen.offer_rows = 0;
        choice = (BATTLE_ATTACK_CHOICE)screen.offer_choice;
        return BattleAction{selected, screen.offer_cell};
    } else if(pressed & (BTN_UP | BTN_DOWN)) {
        if(pressed & BTN_UP)
            screen.offer_choice = screen.offer_choice ? (uint8_t)(screen.offer_choice - 1)
                                                     : (uint8_t)(screen.offer_rows - 1);
        else if(++screen.offer_choice == screen.offer_rows)
            screen.offer_choice = 0;
        sfx_menu_tick();
    }
    return BattleAction{BATTLE_NO_UNIT, 0};
}

// The offer: the normal attack first, then the selected hero's starting special with its AP cost.
// It is centered on the board, on the half that the target's row leaves clear, and its highlight
// marks the choice. The panel is redrawn each frame, so an opening animation could change its size
// or position over the same frames later.
RPG_NOINLINE static void draw_offer(BattleScreen const& screen, uint8_t selected) {
    uint8_t attack = battle_attack_class(screen.state.units[selected].id);
    render_battle_offer(attack, screen.offer_rows, screen.offer_choice,
                        battle_row(screen.offer_cell) < BATTLE_ROWS / 2);
}

// Runs the party's turn one frame at a time, so the cursor keeps easing and its dashes keep moving
// between presses. D-pad moves the logical cursor. A toggles a friendly hero, which replaces the
// current selection, or confirms a destination for the selected hero; a destination that a special
// can also reach opens the offer, where UP and DOWN choose and A confirms. B closes the offer,
// cancels the selection, or opens the menu. Illegal confirmations do nothing and do not consume the
// turn. Returns the legal action the player confirms, and sets `special` when the confirmed option
// is the special. The board, the offer, and the cursor are redrawn every frame.
RPG_NOINLINE static BattleAction party_turn(BattleScreen& screen, BATTLE_ATTACK_CHOICE& choice) {
    BattleState& state = screen.state;
    Cursor& cursor = screen.cursor;
    uint8_t selected = BATTLE_NO_UNIT;
    choice = BATTLE_NORMAL;
    // Presses made while the enemy acted do not count.
    input_sync();
    RPG_QA_MARK(battle_qa_ready = BATTLE_QA_INPUT);
    for(;;) {
        draw_board(screen, selected, SOUTH);
        if(screen.offer_rows)
            draw_offer(screen, selected);
        else
            draw_actor_info(state, cursor);
        draw_cursor(cursor);
        avm_display(false);
        uint8_t pressed =
            wait_frame_presses() & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_A | BTN_B);

        BattleAction action{BATTLE_NO_UNIT, 0};
        if(screen.offer_rows) {
            // The offer owns the input, so the cursor and the selection hold still.
            action = press_offer(screen, selected, pressed, choice);
        } else {
            move_cursor(cursor, pressed);
            if(pressed & BTN_B) {
                if(selected != BATTLE_NO_UNIT) {
                    sfx_play(SFX_CANCEL);
                    selected = BATTLE_NO_UNIT;
                } else {
                    // Return before opening the shared menu so this input frame is no longer live.
                    RPG_QA_MARK(battle_qa_ready = BATTLE_QA_BUSY);
                    return BattleAction{BATTLE_NO_UNIT, BATTLE_CELLS};
                }
            }
            if(pressed & BTN_A)
                action = press_a(screen, selected, cursor.cell);
        }
        if(action.unit_index != BATTLE_NO_UNIT) {
            RPG_QA_MARK(battle_qa_ready = BATTLE_QA_BUSY);
            return action;
        }
        animate_cursor(cursor);
    }
}

// Shows the player's result, then chooses the enemy's action. No input is read here.
RPG_NOINLINE static BattleAction enemy_turn(BattleScreen const& screen) {
    draw_board<false>(screen, BATTLE_NO_UNIT, SOUTH);
    avm_display(false);
    sfx_play(SFX_ENEMY_TURN);
    pause(RESULT_FRAMES);
    return battle_enemy_action(screen.state);
}

RPG_NOINLINE static BATTLE_RESULT show_battle_result(BattleScreen const& screen) {
    auto const& state = screen.state;
    BattleRewards rewards = battle_finish(state);
    draw_board<false>(screen, BATTLE_NO_UNIT, SOUTH);
    render_battle_result(state.result == BATTLE_VICTORY, rewards.gold_gained);
    avm_display(false);
    sfx_play(state.result == BATTLE_VICTORY ? SFX_VICTORY : SFX_DEFEAT);
    RPG_QA_MARK(battle_qa_ready = BATTLE_QA_RESULT);
    input_wait_press(BTN_A);
    if(rewards.levels.members) {
        render_level_up_notice(F("LEVEL UP"), rewards.levels);
        avm_display(false);
        sfx_play(SFX_LEVEL_UP);
        input_wait_press(BTN_A);
    }
    BATTLE_RESULT result = state.result;
    // Dialogue still open from the battle closes over the board, before the world takes its place.
    dialogue_close();
    fade_to_black();
    battle_running = false;
    // The battle is done with the workspace. The caller's victory code may update quests.
    quest_resume_after_battle();
    if(result == BATTLE_VICTORY)
        music_play_area();
    // Synchronize so that no button state carries into the dialogue or exploration that follows.
    input_sync();
    RPG_QA_MARK(battle_qa_ready = BATTLE_QA_BUSY);
    RPG_QA_MARK(battle_qa_state = nullptr);
    return result;
}

BATTLE_RESULT battle_show(BattleEncounter const PROGMEM* encounter) {
    // Waiting quest panels show now: the battle takes over the workspace that holds them.
    show_quest_updates();

    // Battles cannot nest; reuse the one fixed screen, clearing it on every entry.
    BattleScreen& screen = active_screen;
    screen = {};
    screen.slide = SLIDE_FRAMES;
    BattleState& state = screen.state;
    battle_begin(state, encounter);
    RPG_QA_MARK(battle_qa_ready = BATTLE_QA_BUSY);
    RPG_QA_MARK(battle_qa_state = &state);
    // The platform sets these at startup, and dialogue sets them to the same values.
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    // A press that led here, such as the one that closed dialogue, must not act in the battle.
    input_wait_release(BTN_ALL);
    music_play(MUSIC_BATTLE);

    // The cursor starts on the first hero, already settled there, so the battle does not open with
    // a sweep across the board.
    uint8_t first = state.units[0].cell;
    screen.cursor =
        Cursor{first, cursor_subpixel(cell_x(first)), cursor_subpixel(cell_y(first)), 0, 0};
    screen.mover = BATTLE_NO_UNIT;
    // The world fades out while exploration still owns the runtime workspace. Once it is dark, the
    // battle takes the workspace over, and dialogue draws the board until the battle ends.
    fade_world_out();
    battle_running = true;
    fade_battle_in(screen);
    if(!state.ended) {
        arrange_party(screen);
        sfx_play(SFX_BATTLE_START);
    }

    while(!state.ended) {
        while(!state.ended && !battle_can_act(state))
            resolve(screen, BattleAction{BATTLE_NO_UNIT, 0}, BATTLE_NORMAL);
        if(state.ended)
            break;
        if(state.turn == BATTLE_PARTY) {
            BATTLE_ATTACK_CHOICE choice;
            BattleAction action = party_turn(screen, choice);
            if(action.unit_index == BATTLE_NO_UNIT && action.destination == BATTLE_CELLS) {
                MENU_EXIT selected = menu_screen(&screen);
                if(selected == MENU_AUDIO)
                    audio_screen(&screen);
                if(selected == MENU_RESIGN) {
                    state.ended = true;
                    state.result = BATTLE_DEFEAT;
                    break;
                }
                if(selected != MENU_PASS)
                    continue;
                action = BattleAction{BATTLE_NO_UNIT, 0};
                choice = BATTLE_NORMAL;
            }
            resolve(screen, action, choice);
        } else {
            // The party's turn starts on the frame after the enemy's action ends, so the cursor is
            // back at once.
            BattleAction action = enemy_turn(screen);
            resolve(screen, action, battle_enemy_attack_choice(state, action));
        }
    }

    return show_battle_result(screen);
}
