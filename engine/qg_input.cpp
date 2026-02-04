#include "qg_input.hpp"
#include "shared.hpp"
#include "SDL3/SDL.h"

struct key_binding {
    i32 keycode;
    input_action action;
};

//TODO: Think about remapping later if requried
static const key_binding g_key_bindings[] = {
    // WASD - gravity only
    { SDLK_W, input_action::GRAVITY_UP },
    { SDLK_A, input_action::GRAVITY_LEFT },
    { SDLK_S, input_action::GRAVITY_DOWN },
    { SDLK_D, input_action::GRAVITY_RIGHT },

    // Arrow keys - both menu and gravity
    { SDLK_UP,    input_action::MENU_UP },
    { SDLK_UP,    input_action::GRAVITY_UP },
    { SDLK_DOWN,  input_action::MENU_DOWN },
    { SDLK_DOWN,  input_action::GRAVITY_DOWN },
    { SDLK_LEFT,  input_action::MENU_LEFT },
    { SDLK_LEFT,  input_action::GRAVITY_LEFT },
    { SDLK_RIGHT, input_action::MENU_RIGHT },
    { SDLK_RIGHT, input_action::GRAVITY_RIGHT },

    // Confirm
    { SDLK_RETURN, input_action::MENU_CONFIRM },
    { SDLK_SPACE,  input_action::MENU_CONFIRM },

    // Cancel
    { SDLK_ESCAPE, input_action::MENU_CANCEL },

    // Reset
    { SDLK_R, input_action::RESET },
};

static const u32 g_binding_count = sizeof(g_key_bindings) / sizeof(g_key_bindings[0]);

void input_init(input_state* state) {
    state->down = 0;
    state->pressed = 0;
    state->released = 0;
    state->prev_down = 0;
}

void input_update(input_state* state) {
    state->pressed = state->down & ~state->prev_down;
    state->released = ~state->down & state->prev_down;
    state->prev_down = state->down;
}

void input_handle_key(input_state* state, i32 keycode, bool is_down) {
    for (u32 i = 0; i < g_binding_count; i++) {
        if (g_key_bindings[i].keycode == keycode) {
            u32 bit = 1u << (u32)g_key_bindings[i].action;
            if (is_down) {
                state->down |= bit;
            } else {
                state->down &= ~bit;
            }
        }
    }
}

bool input_down(input_state* state, input_action action) {
    return (state->down & (1u << (u32)action)) != 0;
}

bool input_pressed(input_state* state, input_action action) {
    return (state->pressed & (1u << (u32)action)) != 0;
}

bool input_released(input_state* state, input_action action) {
    return (state->released & (1u << (u32)action)) != 0;
}
