#pragma once
#include "shared_types.hpp"

enum class input_action : u8 {
    MENU_UP = 0, MENU_DOWN, MENU_LEFT, MENU_RIGHT, MENU_CONFIRM, MENU_CANCEL,
    GRAVITY_UP, GRAVITY_DOWN, GRAVITY_LEFT, GRAVITY_RIGHT, RESET,
    DEBUG_PREV_LEVEL, DEBUG_NEXT_LEVEL,
    COUNT
};

struct input_state {
    u32 down;       // currently held
    u32 pressed;    // just pressed this frame
    u32 released;   // just released this frame
    u32 prev_down;  // previous frame (for edge detection)
};

// Engine-internal functions (not exposed to game via module def)
void input_init(input_state* state);
void input_update(input_state* state);
void input_handle_key(input_state* state, i32 keycode, bool is_down);

// Query functions
bool input_down(input_state* state, input_action action);
bool input_pressed(input_state* state, input_action action);
bool input_released(input_state* state, input_action action);
