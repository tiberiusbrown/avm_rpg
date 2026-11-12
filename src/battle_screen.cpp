#include "battle.h"
#include "battle_ai.h"

#include <avm.h>

static constexpr uint16_t DISPLAY_WIDTH = 128;
// Stands in for a cell when no cursor is shown.
static constexpr uint8_t NO_CELL = BATTLE_CELLS;
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

// One-pixel box around the cursor cell.
static void draw_cursor(int16_t x, int16_t y) {
    avm_draw_filled_rect_white(x, y, TILE, 1);
    avm_draw_filled_rect_white(x, y + TILE - 1, TILE, 1);
    avm_draw_filled_rect_white(x, y, 1, TILE);
    avm_draw_filled_rect_white(x + TILE - 1, y, 1, TILE);
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

// One pip per remaining hit point, along the lower left of the cell.
static void draw_pips(BattleUnit const& unit, int16_t x, int16_t y) {
    for(uint8_t i = 0; i < unit.hp; ++i)
        avm_draw_filled_rect_white(x + 1 + 3 * i, y + 13, 2, 2);
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

// Draws the whole board. Terrain covers every pixel, so nothing needs clearing first. Black
// cells come before the actors: `lit` is the selected or acting unit, and the selected unit's
// attack targets are blacked out. Empty destinations get their marks in the same layer. Dashed
// attack borders and the cursor come last, over the actors.
static void draw_board(BattleState const& state, uint8_t cursor, uint8_t selected, uint8_t lit) {
    // Bit d is set for the selected unit's neighbor in direction d, which is either an empty cell
    // to enter or an enemy to attack. Both masks use bit positions rather than cells, which keeps
    // the frame small on the guest stack.
    uint8_t moves = 0, attacks = 0, from = NO_CELL;
    if(selected != BATTLE_NO_UNIT) {
        BattleUnit const& unit = state.units[selected];
        from = (uint8_t)(unit.y * BATTLE_COLUMNS + unit.x);
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

    avm_set_sprite(tileset_sprites(state.field->tileset));
    for(uint8_t row = 0; row < BATTLE_ROWS; ++row) {
        for(uint8_t column = 0; column < BATTLE_COLUMNS; ++column) {
            uint8_t cell = (uint8_t)(row * BATTLE_COLUMNS + column);
            avm_draw_overwrite(cell_x(cell), cell_y(cell), state.field->tiles[cell]);
        }
    }

    if(lit != BATTLE_NO_UNIT && state.units[lit].hp) {
        BattleUnit const& unit = state.units[lit];
        avm_draw_filled_rect_black(unit.x * TILE, unit.y * TILE, TILE, TILE);
    }
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
        if(!unit.hp)
            continue;
        int16_t x = unit.x * TILE, y = unit.y * TILE;
        // Units stand still and face the other team.
        Actor pose{0, 0, unit.team == BATTLE_PARTY ? EAST : WEST, 0};
        avm_draw_plus_mask(x, y, actor_frame(pose, unit.sprite_row, 0));
        draw_pips(unit, x, y);
    }

    for(uint8_t d = 0; d < 4; ++d) {
        if(attacks & (1 << d)) {
            uint8_t cell = step(from, d);
            draw_attack_mark(cell_x(cell), cell_y(cell));
        }
    }
    if(cursor != NO_CELL)
        draw_cursor(cell_x(cursor), cell_y(cursor));
}

static void draw_result(BATTLE_RESULT result) {
    avm_draw_filled_rect_white(24, 18, 80, 28);
    avm_draw_filled_rect_black(25, 19, 78, 26);

    char const PROGMEM* title = result == BATTLE_VICTORY ? F("VICTORY") : F("DEFEAT");
    avm_draw_text((int16_t)((DISPLAY_WIDTH - avm_text_width(title)) / 2), 30, title);

    char const PROGMEM* hint = F("A continue");
    avm_draw_text((int16_t)((DISPLAY_WIDTH - avm_text_width(hint)) / 2), 41, hint);
}

// Runs the party's input until one legal action is applied. D-pad moves the cursor, A selects a
// friendly unit or confirms a destination, and B cancels the selection. Illegal confirmations do
// nothing and do not consume the turn.
static void party_turn(BattleState& state, uint8_t& cursor) {
    uint8_t selected = BATTLE_NO_UNIT;
    for(;;) {
        draw_board(state, cursor, selected, selected);
        avm_display(false);
        uint8_t pressed =
            input_wait_press(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_A | BTN_B);

        uint8_t next = cursor;
        DIRECTION dir = pressed_direction(pressed);
        if(dir != STILL && battle_neighbor(cursor, dir, next))
            cursor = next;
        if(pressed & BTN_B)
            selected = BATTLE_NO_UNIT;
        if(!(pressed & BTN_A))
            continue;

        if(selected == BATTLE_NO_UNIT) {
            uint8_t unit = battle_unit_at(state, cursor);
            if(unit != BATTLE_NO_UNIT && state.units[unit].team == BATTLE_PARTY)
                selected = unit;
            continue;
        }
        BattleAction action{selected, cursor};
        if(battle_legal(state, action)) {
            battle_apply(state, action);
            return;
        }
    }
}

// Shows the player's result, then the enemy's choice and its result. No input is read here.
static void enemy_turn(BattleState& state) {
    draw_board(state, NO_CELL, BATTLE_NO_UNIT, BATTLE_NO_UNIT);
    avm_display(false);
    pause(RESULT_FRAMES);

    BattleAction action = battle_enemy_action(state);
    battle_apply(state, action);
    draw_board(state, NO_CELL, BATTLE_NO_UNIT, action.unit_index);
    avm_display(false);
    pause(RESULT_FRAMES);
}

BATTLE_RESULT battle_show(BattleEncounter const PROGMEM* encounter) {
    // Battles never nest, so the state lives in static storage. That keeps its bytes off the
    // 256-byte guest stack, which the dialogue chain already uses. battle_begin resets every field.
    static BattleState state;
    battle_begin(state, encounter);
    // The platform sets these at startup, and dialogue sets them to the same values.
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    // A press that led here, such as the one that closed dialogue, must not act in the battle.
    input_wait_release(BTN_ALL);

    uint8_t cursor = (uint8_t)(state.units[0].y * BATTLE_COLUMNS + state.units[0].x);
    while(!state.ended) {
        battle_settle(state);
        if(state.ended)
            break;
        if(state.turn == BATTLE_PARTY)
            party_turn(state, cursor);
        else
            enemy_turn(state);
    }

    draw_board(state, NO_CELL, BATTLE_NO_UNIT, BATTLE_NO_UNIT);
    draw_result(state.result);
    avm_display(false);
    input_wait_press(BTN_A);
    // Synchronize so that no button state carries into the dialogue or exploration that follows.
    input_sync();
    // Repaint the overworld as dialogue does when it closes, so no battle pixels remain.
    rpg_render_world();
    return state.result;
}
