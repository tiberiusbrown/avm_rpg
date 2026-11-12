#include "battle.h"
#include "battle_ai.h"
#include "battle_cursor.h"
#include "battle_motion.h"

#include <avm.h>

static constexpr uint16_t DISPLAY_WIDTH = 128;
// Frames at 50 Hz. Each side's result stays on screen for this long.
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

// The neighbor of `from` in direction d. Callers pass only directions with an on-board neighbor.
static uint8_t step(uint8_t from, uint8_t d) {
    uint8_t cell = from;
    battle_neighbor(from, (DIRECTION)d, cell);
    return cell;
}

// Unit steps for a lunge in direction d.
static int8_t direction_dx(DIRECTION d) {
    return (int8_t)(d == EAST ? 1 : d == WEST ? -1 : 0);
}

static int8_t direction_dy(DIRECTION d) {
    return (int8_t)(d == SOUTH ? 1 : d == NORTH ? -1 : 0);
}

// The direction from a cell to one of its orthogonal neighbors.
static DIRECTION direction_toward(uint8_t from, uint8_t to) {
    if(battle_column(to) != battle_column(from))
        return battle_column(to) > battle_column(from) ? EAST : WEST;
    return battle_row(to) > battle_row(from) ? SOUTH : NORTH;
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

// Everything one battle's screen needs. battle_show allocates it on its own frame and passes it to
// each helper, so no battle data is static.
struct BattleScreen {
    BattleState state;
    Cursor cursor;
    // The unit drawn at an offset from its cell: a walker, or an attacker lunging at its victim.
    // BATTLE_NO_UNIT when none. Only one unit is offset at a time.
    uint8_t mover;
    // The mover's drawn offset from its logical cell, in pixels. A walker's offset shrinks to zero
    // as it walks. A lunging attacker's offset follows battle_lunge_pixels.
    int8_t mover_dx, mover_dy;
    // Whether the mover draws its walking pose. An attacker stands still while it lunges.
    bool walking;
    // Bit i is set for each unit flashing after damage.
    uint8_t flashing;
    // Animation clock in frames. A walk advances it for the walking pose, and a flash counts it for
    // the flash phase. A walk and a flash never run together, so one clock serves both. It wraps at
    // 256, which keeps the pose cycle whole.
    uint8_t ticks;
};

// The sprite is drawn at the cell's origin minus the margin, so its interior lands on the cell.
static void draw_cursor(Cursor const& cursor) {
    avm_set_sprite(CURSOR_SPRITES);
    avm_draw_plus_mask((int16_t)(cursor_pixel(cursor.x) - CURSOR_MARGIN),
                       (int16_t)(cursor_pixel(cursor.y) - CURSOR_MARGIN), cursor.phase);
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

// Dashed box on an enemy the selected unit can attack.
static void draw_attack_mark(int16_t x, int16_t y) {
    for(uint8_t i = 0; i < TILE; i += 4) {
        avm_draw_filled_rect_white(x + i, y, 2, 1);
        avm_draw_filled_rect_white(x + i, y + TILE - 1, 2, 1);
        avm_draw_filled_rect_white(x, y + i, 1, 2);
        avm_draw_filled_rect_white(x + TILE - 1, y + i, 1, 2);
    }
}

// A black block with a white dot marks an empty cell the selected unit can enter. The block
// keeps the dot readable against the grass pattern.
static void draw_move_mark(int16_t x, int16_t y) {
    avm_draw_filled_rect_black(x + 4, y + 4, 8, 8);
    avm_draw_filled_rect_white(x + 7, y + 7, 2, 2);
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

// Draws the whole board. Terrain covers every pixel, so nothing needs clearing first. Highlights
// come before the actors: the selected unit gets a white border, and its attack targets are
// blacked out. Empty destinations get their marks in the same layer. Dashed attack borders come
// last, over the actors. The cursor is drawn separately, after the board. The mover is drawn even
// once defeated, so a unit that dies on its move is seen walking first. A flashing unit is drawn
// only on the visible phases of its flash, and it too is drawn once defeated. The mover faces
// `mover_facing`, the way it steps or the victim it lunges at. Every other actor stands facing
// south.
static void draw_board(BattleScreen const& screen, uint8_t selected, DIRECTION mover_facing) {
    BattleState const& state = screen.state;
    // Bit d is set for the selected unit's neighbor in direction d, which is either an empty cell
    // to enter or an enemy to attack. Both masks use bit positions rather than cells, which keeps
    // the frame small on the guest stack.
    uint8_t moves = 0, attacks = 0, from = 0;
    if(selected != BATTLE_NO_UNIT) {
        BattleUnit const& unit = state.units[selected];
        from = unit.cell;
        for(uint8_t d = 0; d < 4; ++d) {
            uint8_t cell = 0;
            if(!battle_neighbor(from, (DIRECTION)d, cell) ||
               !battle_legal(state, BattleAction{selected, cell}))
                continue;
            uint8_t bit = (uint8_t)(1 << d);
            if(battle_unit_at(state, cell) == BATTLE_NO_UNIT)
                moves = (uint8_t)(moves | bit);
            else
                attacks = (uint8_t)(attacks | bit);
        }
    }

    avm_set_sprite(tileset_sprites(BATTLEFIELDS[state.field].tileset));
    for(uint8_t row = 0; row < BATTLE_ROWS; ++row) {
        for(uint8_t column = 0; column < BATTLE_COLUMNS; ++column) {
            uint8_t cell = (uint8_t)(row * BATTLE_COLUMNS + column);
            avm_draw_overwrite(cell_x(cell), cell_y(cell), BATTLEFIELDS[state.field].tiles[cell]);
        }
    }

    if(selected != BATTLE_NO_UNIT && state.units[selected].hp)
        draw_selection(cell_x(state.units[selected].cell), cell_y(state.units[selected].cell));
    for(uint8_t d = 0; d < 4; ++d) {
        uint8_t bit = (uint8_t)(1 << d);
        if(!((moves | attacks) & bit))
            continue;
        uint8_t cell = step(from, d);
        if(attacks & bit)
            avm_draw_filled_rect_black(cell_x(cell), cell_y(cell), TILE, TILE);
        else
            draw_move_mark(cell_x(cell), cell_y(cell));
    }

    avm_set_sprite(ACTOR_SPRITES);
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        bool moving = i == screen.mover;
        bool flashing = ((screen.flashing >> i) & 1) != 0;
        if(!unit.hp && !moving && !flashing)
            continue;
        if(flashing && battle_flash_hidden(screen.ticks))
            continue;
        // The mover is drawn at its cell plus the offset it still has to cover.
        int16_t x = (int16_t)(cell_x(unit.cell) + (moving ? screen.mover_dx : 0));
        int16_t y = (int16_t)(cell_y(unit.cell) + (moving ? screen.mover_dy : 0));
        Actor const pose{(uint16_t)x, (uint16_t)y, moving ? mover_facing : SOUTH,
                         (uint8_t)(moving && screen.walking)};
        avm_draw_plus_mask(x, y, actor_frame(pose, unit.sprite_row, screen.ticks));
    }

    for(uint8_t d = 0; d < 4; ++d) {
        if(attacks & (1 << d)) {
            uint8_t cell = step(from, d);
            draw_attack_mark(cell_x(cell), cell_y(cell));
        }
    }
}

// Walks the mover one pixel per frame to its logical cell, as the player walks in exploration. It
// faces the way it steps, and the walking pose cycles while it moves. The logical state has already
// changed, so only the drawn offset is left to walk. With no offset, it does nothing.
static inline __attribute__((always_inline)) void walk_unit(BattleScreen& screen) {
    screen.walking = true;
    while(screen.mover_dx != 0 || screen.mover_dy != 0) {
        // The offset moves toward zero, so the walker moves the opposite way.
        DIRECTION facing;
        if(screen.mover_dx != 0) {
            facing = screen.mover_dx < 0 ? EAST : WEST;
            screen.mover_dx = (int8_t)(screen.mover_dx + (screen.mover_dx < 0 ? 1 : -1));
        } else {
            facing = screen.mover_dy < 0 ? SOUTH : NORTH;
            screen.mover_dy = (int8_t)(screen.mover_dy + (screen.mover_dy < 0 ? 1 : -1));
        }
        ++screen.ticks;
        // The walker is drawn over the terrain with no highlight, so only the moving actor changes.
        draw_board(screen, BATTLE_NO_UNIT, facing);
        avm_display(false);
        input_wait_frame();
    }
    screen.mover = BATTLE_NO_UNIT;
    screen.walking = false;
}

// Plays the flash of every flashing unit, and the lunge of `lunger` toward `toward` when it is not
// BATTLE_NO_UNIT. The lunge moves out, holds, and returns, while the flashes run over the same
// frames, so the next turn waits for the last flash.
static inline __attribute__((always_inline)) void
animate_reaction(BattleScreen& screen, uint8_t lunger, DIRECTION toward) {
    screen.mover = lunger;
    screen.walking = false;
    for(screen.ticks = 0; screen.ticks < BATTLE_FLASH_FRAMES; ++screen.ticks) {
        uint8_t reach = lunger == BATTLE_NO_UNIT ? 0 : battle_lunge_pixels(screen.ticks);
        screen.mover_dx = (int8_t)(direction_dx(toward) * reach);
        screen.mover_dy = (int8_t)(direction_dy(toward) * reach);
        // The attacker faces its victim while it is out of place, and stands south once it returns.
        draw_board(screen, BATTLE_NO_UNIT, reach ? toward : SOUTH);
        avm_display(false);
        input_wait_frame();
    }
    screen.mover = BATTLE_NO_UNIT;
    screen.flashing = 0;
    screen.mover_dx = 0;
    screen.mover_dy = 0;
}

// Applies a legal action to the rules and records its motion on the screen. The actor becomes the
// mover. A move sets up its walk from the old cell. Every unit the action damages is marked to
// flash: an attack always damages its victim, and an actor can also lose HP to exhaustion. Returns
// the direction of an attack's lunge, which means nothing for a move. Kept out of line so the rule
// updates stay out of resolve's frame, which is live while the motion plays.
RPG_NOINLINE static DIRECTION act(BattleScreen& screen, BattleAction action) {
    BattleState& state = screen.state;
    uint8_t actor = action.unit_index;
    uint8_t victim = battle_unit_at(state, action.destination);
    bool moving = victim == BATTLE_NO_UNIT;
    uint8_t actor_hp = state.units[actor].hp;
    DIRECTION toward = direction_toward(state.units[actor].cell, action.destination);
    screen.mover = actor;
    if(moving) {
        // The walk starts at the old cell, which is offset from the new one by a single step.
        screen.mover_dx = (int8_t)(cell_x(state.units[actor].cell) - cell_x(action.destination));
        screen.mover_dy = (int8_t)(cell_y(state.units[actor].cell) - cell_y(action.destination));
    }
    battle_apply(state, action);

    screen.flashing = 0;
    if(!moving)
        screen.flashing = (uint8_t)(screen.flashing | (1 << victim));
    if(state.units[actor].hp < actor_hp)
        screen.flashing = (uint8_t)(screen.flashing | (1 << actor));
    return toward;
}

// Resolves a legal action and plays its motion. A move walks the unit to its new cell. An attack
// lunges the attacker at its victim. Every unit the action damages flashes, and the caller waits
// for the flashes to end.
RPG_NOINLINE static void resolve(BattleScreen& screen, BattleAction action) {
    DIRECTION toward = act(screen, action);
    // Only a move leaves an offset to walk, and a walk clears the mover. An attack leaves the
    // attacker as the mover, which lunges toward its victim.
    if(screen.mover_dx != 0 || screen.mover_dy != 0)
        walk_unit(screen);
    if(screen.flashing)
        animate_reaction(screen, screen.mover, toward);
}

static void draw_result(BATTLE_RESULT result) {
    avm_draw_filled_rect_white(24, 18, 80, 28);
    avm_draw_filled_rect_black(25, 19, 78, 26);

    char const PROGMEM* title = result == BATTLE_VICTORY ? F("VICTORY") : F("DEFEAT");
    avm_draw_text((int16_t)((DISPLAY_WIDTH - avm_text_width(title)) / 2), 30, title);

    char const PROGMEM* hint = F("A continue");
    avm_draw_text((int16_t)((DISPLAY_WIDTH - avm_text_width(hint)) / 2), 41, hint);
}

// Handles an A press on `cell`. A friendly hero becomes the selection, replacing any other.
// Otherwise the selected hero acts on the cell if that is legal. Returns the action taken, or one
// with no unit when the press changes nothing. Kept out of line so party_turn's frame stays small.
RPG_NOINLINE static BattleAction press_a(BattleState const& state, uint8_t& selected,
                                         uint8_t cell) {
    uint8_t unit = battle_unit_at(state, cell);
    if(unit != BATTLE_NO_UNIT && state.units[unit].team() == BATTLE_PARTY) {
        selected = unit;
    } else if(selected != BATTLE_NO_UNIT) {
        BattleAction action{selected, cell};
        if(battle_legal(state, action))
            return action;
    }
    return BattleAction{BATTLE_NO_UNIT, 0};
}

// Runs the party's turn one frame at a time, so the cursor keeps easing and its dashes keep moving
// between presses. D-pad moves the logical cursor. A selects a friendly hero, which replaces the
// current selection, or confirms a destination for the selected hero. B cancels the selection.
// Illegal confirmations do nothing and do not consume the turn. Returns the legal action the player
// confirms.
RPG_NOINLINE static BattleAction party_turn(BattleScreen& screen) {
    BattleState& state = screen.state;
    Cursor& cursor = screen.cursor;
    uint8_t selected = BATTLE_NO_UNIT;
    // Presses made while the enemy acted do not count.
    input_sync();
    for(;;) {
        draw_board(screen, selected, SOUTH);
        draw_cursor(cursor);
        avm_display(false);
        uint8_t pressed =
            wait_frame_presses() & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_A | BTN_B);

        uint8_t next = cursor.cell;
        DIRECTION dir = pressed_direction(pressed);
        if(dir != STILL && battle_neighbor(cursor.cell, dir, next))
            cursor.cell = next;
        if(pressed & BTN_B)
            selected = BATTLE_NO_UNIT;
        if(pressed & BTN_A) {
            BattleAction action = press_a(state, selected, cursor.cell);
            if(action.unit_index != BATTLE_NO_UNIT) {
                // The cursor returns to the hero that acted, so that hero can be selected again
                // without moving the cursor. A move leaves the hero at its destination, and an
                // attack leaves it in place.
                bool moved = battle_unit_at(state, action.destination) == BATTLE_NO_UNIT;
                cursor.cell = moved ? action.destination : state.units[action.unit_index].cell;
                // The rendered cursor snaps there too. Easing would glide it back from the cell
                // it was on when the action was confirmed.
                cursor.x = cursor_subpixel(cell_x(cursor.cell));
                cursor.y = cursor_subpixel(cell_y(cursor.cell));
                return action;
            }
        }
        animate_cursor(cursor);
    }
}

// Shows the player's result, then chooses the enemy's action. No input is read here.
RPG_NOINLINE static BattleAction enemy_turn(BattleScreen const& screen) {
    draw_board(screen, BATTLE_NO_UNIT, SOUTH);
    avm_display(false);
    pause(RESULT_FRAMES);
    return battle_enemy_action(screen.state);
}

// Shows the board after the enemy's action, with no unit highlighted.
RPG_NOINLINE static void enemy_result(BattleScreen const& screen) {
    draw_board(screen, BATTLE_NO_UNIT, SOUTH);
    avm_display(false);
    pause(RESULT_FRAMES);
}

BATTLE_RESULT battle_show(BattleEncounter const PROGMEM* encounter) {
    // Everything this battle needs lives in one local, and each helper receives it.
    BattleScreen screen{};
    BattleState& state = screen.state;
    battle_begin(state, encounter);
    // The platform sets these at startup, and dialogue sets them to the same values.
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    // A press that led here, such as the one that closed dialogue, must not act in the battle.
    input_wait_release(BTN_ALL);

    // The cursor starts on the first hero, already settled there, so the battle does not open with
    // a sweep across the board.
    uint8_t first = state.units[0].cell;
    screen.cursor =
        Cursor{first, cursor_subpixel(cell_x(first)), cursor_subpixel(cell_y(first)), 0, 0};
    screen.mover = BATTLE_NO_UNIT;

    while(!state.ended) {
        battle_settle(state);
        if(state.ended)
            break;
        if(state.turn == BATTLE_PARTY) {
            resolve(screen, party_turn(screen));
        } else {
            resolve(screen, enemy_turn(screen));
            enemy_result(screen);
        }
    }

    draw_board(screen, BATTLE_NO_UNIT, SOUTH);
    draw_result(state.result);
    avm_display(false);
    input_wait_press(BTN_A);
    // Synchronize so that no button state carries into the dialogue or exploration that follows.
    input_sync();
    // Repaint the overworld as dialogue does when it closes, so no battle pixels remain.
    rpg_render_world();
    return state.result;
}
