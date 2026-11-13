// The enemy's thinking indicator. The battle screen draws the board before the enemy's search
// starts, and the search calls battle_think_progress a few times a second. Each call redraws only
// the indicator's box in the display buffer and pushes the buffer without clearing it, so the board
// around the box stays as it was. The next full board drawing after the search removes it.
#include "battle_ai.h"
#include "battle_motion.h"

#include <avm.h>

void battle_think_progress(uint8_t step) {
    avm_draw_filled_rect_white(BATTLE_THINKING_X, BATTLE_THINKING_Y, BATTLE_THINKING_WIDTH,
                               BATTLE_THINKING_HEIGHT);
    avm_draw_filled_rect_black(BATTLE_THINKING_X + 1, BATTLE_THINKING_Y + 1,
                               BATTLE_THINKING_WIDTH - 2, BATTLE_THINKING_HEIGHT - 2);
    constexpr int16_t dot_y =
        BATTLE_THINKING_Y + (BATTLE_THINKING_HEIGHT - BATTLE_THINKING_DOT) / 2;
    uint8_t lit = battle_thinking_lit(step);
    for(uint8_t dot = 0; dot < BATTLE_THINKING_DOTS; ++dot) {
        int16_t x = battle_thinking_dot_x(dot);
        // An unlit dot keeps its center pixel, so the three places stay visible.
        if(dot < lit)
            avm_draw_filled_rect_white(x, dot_y, BATTLE_THINKING_DOT, BATTLE_THINKING_DOT);
        else
            avm_draw_filled_rect_white((int16_t)(x + BATTLE_THINKING_DOT / 2),
                                       (int16_t)(dot_y + BATTLE_THINKING_DOT / 2), 1, 1);
    }
    avm_display(false);
}
