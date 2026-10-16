#include "rpg.h"

#include <avm.h>

static uint8_t previous_buttons;

void input_sync() {
    previous_buttons = avm_buttons();
}

uint8_t input_held() {
    return avm_buttons();
}

uint8_t input_pressed() {
    uint8_t held = avm_buttons();
    uint8_t pressed = held & (uint8_t)~previous_buttons;
    previous_buttons = held;
    return pressed;
}

void input_wait_release(uint8_t mask) {
    while(avm_buttons() & mask)
        avm_idle();
    input_sync();
}

uint8_t input_wait_press(uint8_t mask) {
    input_wait_release(mask);

    for(;;) {
        avm_idle();
        uint8_t pressed = input_pressed() & mask;
        if(pressed)
            return pressed;
    }
}

void input_wait_frame() {
    while(!avm_next_frame()) {
    }
}
