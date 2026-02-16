#pragma once
#include "shared_types.hpp"

enum class key_code : u16 {
    W, A, S, D, R,
    UP, DOWN, LEFT, RIGHT,
    RETURN, SPACE, ESCAPE,
    PAGE_UP, PAGE_DOWN,
    COUNT
};

struct key_binding { key_code key; u8 action; };

struct input_state {
    u32 down;       // currently held
    u32 pressed;    // just pressed this frame
    u32 released;   // just released this frame
    u32 prev_down;  // previous frame (for edge detection)
    key_binding bindings[64];
    u32 binding_count;
};

// Engine-internal functions (not exposed to game via module def)
void input_init(input_state* state);
void input_update(input_state* state);
void input_handle_key(input_state* state, i32 keycode, bool is_down);

// Exposed to game
void input_bind_key(input_state* state, key_code key, u8 action);
bool input_down(input_state* state, u8 action);
bool input_pressed(input_state* state, u8 action);
bool input_released(input_state* state, u8 action);
