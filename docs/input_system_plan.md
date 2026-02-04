# Input System Implementation Plan

## Overview

Add an action-based input system to the engine that:
- Captures SDL keyboard events in the engine
- Exposes polling API for game code to query actions
- Supports future extension to touch/gamepad without game code changes

## Design Decisions

- **Action-based abstraction**: Game queries actions (GRAVITY_UP), not raw keys (SDLK_W)
- **Dual mapping**: Arrow keys trigger both MENU_* and GRAVITY_* actions; game decides based on state
- **Single press only**: Ignore SDL key repeat events
- **ESCAPE = MENU_CANCEL only**: Remove hardcoded quit; game handles quit through menu
- **State lives in engine**: Survives hot-reload; game gets read-only polling access

## Action Enum

```cpp
enum class input_action : u8 {
    // Menu navigation
    MENU_UP = 0, MENU_DOWN, MENU_LEFT, MENU_RIGHT, MENU_CONFIRM, MENU_CANCEL,
    // Gameplay
    GRAVITY_UP, GRAVITY_DOWN, GRAVITY_LEFT, GRAVITY_RIGHT, RESET,
    COUNT
};
```

## Input State (Bitmask-Based)

```cpp
struct input_state {
    u32 down;       // currently held
    u32 pressed;    // just pressed this frame
    u32 released;   // just released this frame
    u32 prev_down;  // previous frame (for edge detection)
};
```

## Module API (X-Macro Pattern)

Only polling functions are exported to game. Init/update are engine-internal.

```cpp
#define INPUT_MODULE_DEF \
    X(bool, input_down, (input_state*, input_action)) \
    X(bool, input_pressed, (input_state*, input_action)) \
    X(bool, input_released, (input_state*, input_action))
```

Engine-internal functions (declared in qg_input.hpp, not in MODULE_DEF):
- `input_init(input_state*)` - called once at engine startup
- `input_update(input_state*)` - called every frame before game tick
- `input_handle_key(input_state*, SDL_Keycode, bool)` - called during SDL event poll

## Default Key Bindings

| Key | Actions |
|-----|---------|
| W | GRAVITY_UP |
| A | GRAVITY_LEFT |
| S | GRAVITY_DOWN |
| D | GRAVITY_RIGHT |
| Arrow Up | MENU_UP, GRAVITY_UP |
| Arrow Down | MENU_DOWN, GRAVITY_DOWN |
| Arrow Left | MENU_LEFT, GRAVITY_LEFT |
| Arrow Right | MENU_RIGHT, GRAVITY_RIGHT |
| Enter/Space | MENU_CONFIRM |
| Escape | MENU_CANCEL |
| R | RESET |

## Files to Create

### `engine/qg_input.hpp`
- Forward declare `input_state` in shared.hpp
- Define full struct here with bitmask fields
- Declare internal function `input_handle_key()` (not exposed to game)

### `engine/qg_input.cpp`
- Key binding table (array of {SDL_Keycode, input_action})
- `input_init()` - zero state
- `input_update()` - compute pressed/released from down vs prev_down
- `input_handle_key()` - map SDL keycode to action bits, ignore repeat events
- `input_down/pressed/released()` - bit test and return

## Files to Modify

### `shared/shared.hpp`
1. Add `input_action` enum after RANDOM_MODULE_DEF
2. Forward declare `struct input_state`
3. Add INPUT_MODULE_DEF
4. Add to engine_api struct:
   - `struct { INPUT_MODULE_DEF };` in X-macro block
   - `input_state *input;` pointer field

### `engine/qg_main.cpp`
1. Add `#include "qg_input.hpp"`
2. Add global: `input_state g_input {};`
3. In init sequence (line ~123): add `INPUT_MODULE_DEF` to X-macro block
4. After bus init: `input_init(&g_input); g_eng.input = &g_input;`
5. Modify SDL event loop (line ~148):
   - Handle KEY_DOWN: call `input_handle_key(&g_input, key, true)`, skip if repeat
   - Handle KEY_UP: call `input_handle_key(&g_input, key, false)`
   - Remove hardcoded ESCAPE quit (keep SDL_EVENT_QUIT for window X button)
6. Call `input_update(&g_input)` BEFORE game tick loop

### `engine/~qg.cpp`
- Add `#include "qg_input.cpp"` before `qg_main.cpp`

## Frame Timing

```
SDL_PollEvent loop → input_handle_key() sets/clears bits
         ↓
    input_update() computes pressed/released edges
         ↓
    game_tick() polls actions via g_api.input_pressed()
         ↓
    bus_process()
```

## Game Usage Example

```cpp
void grav_tick(f32 dt) {
    if (g_api.input_pressed(g_api.input, input_action::GRAVITY_UP)) {
        apply_gravity(direction::UP);
    }
    if (g_api.input_pressed(g_api.input, input_action::RESET)) {
        run_level_reset(&g_run, &g_lvl);
    }
}
```

## Future Touch Extension

Design accommodates touch by adding:
```cpp
void input_handle_touch(input_state* state, f32 x, f32 y, bool is_down);
```

Touch regions map to same actions. Game code unchanged - queries actions, not input source.

## Verification

1. Build: `compile all`
2. Run: `run`
3. Test keyboard: WASD/arrows should log or trigger gravity changes in game
4. Test ESCAPE: Should NOT quit game (only window X button quits)
5. Test key repeat: Hold a key - action should only fire once
6. Hot-reload: Rebuild `gr` while running, input should still work
