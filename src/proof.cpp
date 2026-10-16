#include <avm.h>

static uint8_t x = 24, y = 24, area, action;

volatile uint8_t proof_marker;
volatile uint16_t proof_serial;

static uint8_t const PROGMEM TILE_SPRITE[] = {8, 8, 0x55, 0, 0x55, 0, 0x55, 0, 0x55, 0};
static uint8_t const PROGMEM PLAYER_SPRITE[] = {8, 8, 0, 0x38, 0x7e, 0xdb, 0xdb, 0x7e, 0x38, 0};

extern "C" void setup() {
    avm_set_frame_rate(20);
    avm_set_text_font(AVM_FONT_BR5D);
}

extern "C" __attribute__((noinline)) void proof_present() {
    avm_display(true);
}

extern "C" void loop() {
    uint8_t buttons;

    if(!avm_next_frame())
        return;

    proof_marker = 1;
    avm_poll_buttons();
    buttons = avm_buttons();

    if((buttons & AVM_BUTTON_L) && x)
        --x;
    if((buttons & AVM_BUTTON_R) && x < 120)
        ++x;
    if((buttons & AVM_BUTTON_U) && y)
        --y;
    if((buttons & AVM_BUTTON_D) && y < 56)
        ++y;

    if(avm_just_pressed(AVM_BUTTON_A)) {
        area ^= 1;
        action = 1;
    }
    if(avm_just_pressed(AVM_BUTTON_B))
        action ^= 1;

    avm_draw_sprite_overwrite(8, 16, TILE_SPRITE, 0);
    if(area)
        avm_draw_filled_rect_white(70, 20, 16, 24);
    avm_draw_sprite_self_masked(x, y, PLAYER_SPRITE, 0);
    avm_draw_text_P(2, 8, area ? F("Area 2 / A switch") : F("Area 1 / A switch"));
    if(action)
        avm_draw_text_P(2, 62, F("B toggles this line"));

    proof_marker = 2;
    proof_present();
    proof_marker = 3;
    proof_marker = 0;
    ++proof_serial;
}
