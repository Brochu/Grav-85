#include "qg_input.hpp"
#include "shared.hpp"
#include "SDL3/SDL.h"

#include <cassert>

static const i32 g_keycode_map[(u16)key_code::COUNT] = {
    SDLK_W, SDLK_A, SDLK_S, SDLK_D, SDLK_R,
    SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT,
    SDLK_RETURN, SDLK_SPACE, SDLK_ESCAPE,
    SDLK_PAGEUP, SDLK_PAGEDOWN,
};

void input_init(input_state* state) {
    state->down = 0;
    state->pressed = 0;
    state->released = 0;
    state->prev_down = 0;
    state->binding_count = 0;
}

void input_update(input_state* state) {
    state->pressed = state->down & ~state->prev_down;
    state->released = ~state->down & state->prev_down;
    state->prev_down = state->down;
}

void input_bind_key(input_state* state, key_code key, u8 action) {
    assert(state->binding_count < 64);
    state->bindings[state->binding_count++] = { key, action };
}

void input_handle_key(input_state* state, i32 keycode, bool is_down) {
    // Find which key_code this SDL keycode maps to
    for (u16 k = 0; k < (u16)key_code::COUNT; k++) {
        if (g_keycode_map[k] == keycode) {
            // Scan bindings for this key_code
            for (u32 i = 0; i < state->binding_count; i++) {
                if ((u16)state->bindings[i].key == k) {
                    u32 bit = 1u << state->bindings[i].action;
                    if (is_down) {
                        state->down |= bit;
                    } else {
                        state->down &= ~bit;
                    }
                }
            }
            break;
        }
    }
}

bool input_down(input_state* state, u8 action) {
    return (state->down & (1u << action)) != 0;
}

bool input_pressed(input_state* state, u8 action) {
    return (state->pressed & (1u << action)) != 0;
}

bool input_released(input_state* state, u8 action) {
    return (state->released & (1u << action)) != 0;
}
