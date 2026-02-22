#include <algorithm>
#include <cstdio>

#include "qg_bus.hpp"
#include "qg_config.hpp"
#include "qg_input.hpp"
#include "qg_math.hpp"
#include "qg_memory.hpp"
#include "shared.hpp"

#include "SDL3/SDL.h"

engine_api g_api {};
input_state *g_in {};

enum class game_action : u8 {
    GRAVITY_UP, GRAVITY_DOWN, GRAVITY_LEFT, GRAVITY_RIGHT,
    MENU_UP, MENU_DOWN, MENU_LEFT, MENU_RIGHT,
    MENU_CONFIRM, MENU_CANCEL,
    RESET, DEBUG_PREV_LEVEL, DEBUG_NEXT_LEVEL,
    COUNT
};

enum class color : u8 { RED, GREEN, BLUE, COUNT };
#define MAP_MAX_SIZE 256
#define ELEMENTS_MAX_NUM 32

struct level {
    u8 solid[MAP_MAX_SIZE / 8];       // 1 bit per cell
    ivec2 crate_starts[ELEMENTS_MAX_NUM];
    ivec2 gem_starts[ELEMENTS_MAX_NUM];
    color gem_colors[ELEMENTS_MAX_NUM];
    direction start_gravity;
    i8 width;
    i8 height;
    i8 num_crates;
    i8 num_gems;
};

inline bool level_is_solid(level *lvl, ivec2 pos) {
    u32 idx = (pos.y * lvl->width) + pos.x;
    return (lvl->solid[idx / 8] >> (idx % 8)) & 1;
}

enum element_type : u8 { CRATE, GEM, COUNT };
#define ATTEMPT_MAX_MOVES 99

struct attempt {
    u64 start_timestamp;
    ivec2 crates[ELEMENTS_MAX_NUM];
    ivec2 gems[ELEMENTS_MAX_NUM];
    vec2 crate_offsets[ELEMENTS_MAX_NUM];
    vec2 gem_offsets[ELEMENTS_MAX_NUM];
    direction moves[ATTEMPT_MAX_MOVES];
    direction current_gravity;
    u32 gems_active;
    i8 num_crates;
    i8 num_gems;
    i8 num_moves;
    bool animating;  // true = block input, offsets decaying
};

void attempt_level_init(attempt *att, level *lvl) {
    att->start_timestamp = SDL_GetTicksNS();

    memcpy_s(att->crates, sizeof(att->crates), lvl->crate_starts, sizeof(lvl->crate_starts));
    memset(att->crate_offsets, 0, sizeof(att->crate_offsets));
    memcpy_s(att->gems, sizeof(att->gems), lvl->gem_starts, sizeof(lvl->gem_starts));
    memset(att->gem_offsets, 0, sizeof(att->gem_offsets));

    att->current_gravity = lvl->start_gravity;
    att->gems_active = (1u << lvl->num_gems) - 1;
    att->num_crates = lvl->num_crates;
    att->num_gems = lvl->num_gems;
    att->num_moves = 0;
}

void attempt_level_reset(attempt *att, level *lvl) {
    memcpy_s(att->crates, sizeof(att->crates), lvl->crate_starts, sizeof(lvl->crate_starts));
    memset(att->crate_offsets, 0, sizeof(att->crate_offsets));
    memcpy_s(att->gems, sizeof(att->gems), lvl->gem_starts, sizeof(lvl->gem_starts));
    memset(att->gem_offsets, 0, sizeof(att->gem_offsets));

    att->current_gravity = lvl->start_gravity;
    att->gems_active = (1u << lvl->num_gems) - 1;
    att->num_crates = lvl->num_crates;
    att->num_gems = lvl->num_gems;
    att->num_moves = 0;
}

bool attempt_element_at(attempt *att, ivec2 pos) {
    for (i32 i = 0; i < att->num_crates; i++) {
        if (att->crates[i] == pos) {
            return true;
        }
    }

    for (i32 i = 0; i < att->num_gems; i++) {
        const bool gem_active = (att->gems_active & (1 << i)) != 0;
        if (gem_active && att->gems[i] == pos) {
            return true;
        }
    }

    return false;
}

void attempt_gravity_change(attempt *att, level *lvl, direction new_gravity) {
    //TODO: Do we need to store the level in the run itself?
    att->current_gravity = new_gravity;
    ivec2 dir = direction_vectors[(u8)new_gravity];
    ivec2 opp = direction_vectors[((u8)new_gravity + 2) % 4];
    i32 num_moves = 0;

    // Sort all elements so they will update from furthest along in the new_gravity direction first
    i32 update_indices[ELEMENTS_MAX_NUM*2]; // Store both crates and gems
    element_type update_types[ELEMENTS_MAX_NUM*2];
    for (i32 i = 0; i < att->num_crates; i++) {
        update_indices[i] = i;
        update_types[i] = element_type::CRATE;
    }
    for (i32 i = 0; i < att->num_gems; i++) {
        update_indices[att->num_crates+i] = i;
        update_types[att->num_crates+i] = element_type::GEM;
    }

    ivec2 *positions[element_type::COUNT];
    positions[element_type::CRATE] = att->crates;
    positions[element_type::GEM] = att->gems;
    for (i32 i = 0; i < att->num_crates + att->num_gems; i++) {
        i32 key = update_indices[i];
        element_type key_type = update_types[i];
        i32 key_dot = ivec2_dot(positions[key_type][key], dir);
        i32 j = i - 1;
        while (j >= 0 && ivec2_dot(positions[update_types[j]][update_indices[j]], dir) < key_dot) {
            update_indices[j+1] = update_indices[j];
            update_types[j+1] = update_types[j];
            j--;
        }
        update_indices[j+1] = key;
        update_types[j+1] = key_type;
    }

    vec2 *offsets[element_type::COUNT];
    offsets[element_type::CRATE] = att->crate_offsets;
    offsets[element_type::GEM] = att->gem_offsets;
    for (i32 i = 0; i < att->num_crates + att->num_gems; i++) {
        ivec2 start = positions[update_types[i]][update_indices[i]];
        ivec2 next = start + dir;

        while (!level_is_solid(lvl, next) && !attempt_element_at(att, next)) {
            next = next + dir;
        }
        ivec2 end = next + opp;

        if (end != start) {
            num_moves++;
            positions[update_types[i]][update_indices[i]] = end;
            offsets[update_types[i]][update_indices[i]] = to_vec2(start - end);
        }
    }

    if (num_moves > 0) {
        att->animating = true;
    }
}

void attempt_check_combos(attempt *att, level *lvl) {
    i32 num_combos = 0;
    u32 gems_to_check = att->gems_active;

    for (i32 i = 0; i < att->num_gems; i++) {
        if (gems_to_check == 0) {
            // Done checking all gems
            break;
        }
        assert(att->gem_offsets[i] == vec2_zero); // Make sure the gem is not stil moving

        const bool gem_inactive = (att->gems_active & (1u << i)) == 0;
        const bool gem_visited = (gems_to_check & (1u << i)) == 0;
        if (gem_inactive || gem_visited) {
            continue;
        }

        i32 q_head = 0;
        i32 q_tail = 0;
        i32 queue[ELEMENTS_MAX_NUM];
        queue[q_tail++] = i;
        gems_to_check &= ~(1 << i);

        i32 combo_elems[ELEMENTS_MAX_NUM];
        combo_elems[0] = i;
        i32 combo_len = 1;
        while (q_head != q_tail) {
            i32 current_index = queue[q_head++];
            ivec2 pos = att->gems[current_index];

            for (i32 d = 0; d < 4; d++) {
                ivec2 neighbor = pos + direction_vectors[d];

                for (i32 j = 0; j < att->num_gems; j++) {
                    const bool other_inactive = (att->gems_active & (1 << j)) == 0;
                    const bool other_visited = (gems_to_check & (1 << j)) == 0;
                    if (other_inactive || other_visited) {
                        continue;
                    }

                    if (neighbor == att->gems[j] && lvl->gem_colors[current_index] == lvl->gem_colors[j]) {
                        gems_to_check &= ~(1u << j);
                        queue[q_tail++] = j;
                        combo_elems[combo_len++] = j;
                    }
                }
            }
        }

        if (combo_len > 1) {
            num_combos++;

            for (i32 k = 0; k < combo_len; k++) {
                att->gems_active &= ~(1u << combo_elems[k]);
            }
        }
    }

    if (num_combos > 0) {
        attempt_gravity_change(att, lvl, att->current_gravity);
    }
}

#define NUM_LEVEL_PER_MATCH 5
#define BYTES_PER_LEVEL 108
#define BYTES_PER_MATCH NUM_LEVEL_PER_MATCH * BYTES_PER_LEVEL

struct match {
    level *levels;
    attempt *attempts;
    i8 *level_indices;

    i8 num_levels;
    i8 num_players;

    mem_arena _scratch;
};

void match_read_level(level *lvl, u8 *data, u64 length) {
    assert(length == BYTES_PER_LEVEL);
    u8 dims = (u8)data[0];
    lvl->width = (dims >> 4) & 0xf;
    lvl->height = dims & 0xf;

    lvl->start_gravity = direction::COUNT;
    lvl->num_crates = (i8)data[2];
    lvl->num_gems = (i8)data[3];

    u8 *crate_data = (u8*)&data[12];
    for (int i = 0; i < lvl->num_crates; i++) {
        lvl->crate_starts[i] = unpack_pos(crate_data[i]);
    }

    u64 colors_data;
    memcpy(&colors_data, &data[4], sizeof(u64));
    u8 *gem_data = (u8*)&data[44];
    for (int i = 0; i < lvl->num_gems; i++) {
        lvl->gem_colors[i] = (color)((colors_data >> (2 * i)) & 0b11);
        lvl->gem_starts[i] = unpack_pos(gem_data[i]);
    }

    u8 *solid_data = (u8*)&data[76];
    memcpy_s(lvl->solid, MAP_MAX_SIZE / 8, solid_data, MAP_MAX_SIZE / 8);
}

void match_init(match *match, i8 num_players, u8 *data, u64 length) {
    if (match->_scratch.base == nullptr) {
        u64 required_mem = 
            sizeof(level) * NUM_LEVEL_PER_MATCH + // Each level of the match
            sizeof(attempt) * NUM_LEVEL_PER_MATCH * num_players + // Each attempts / level / player
            sizeof(i8) * num_players + // Current level / player
            64;
        g_api.mem_arena_init(&match->_scratch, required_mem);
    } else {
        g_api.mem_arena_reset(&match->_scratch);
    }

    match->num_levels = NUM_LEVEL_PER_MATCH;
    match->levels = (level*)g_api.mem_arena_alloc(&match->_scratch, sizeof(level) * NUM_LEVEL_PER_MATCH, alignof(level)).p;
    for (i32 i = 0; i < NUM_LEVEL_PER_MATCH; i++) {
        match_read_level(&match->levels[i], &data[i * BYTES_PER_LEVEL], BYTES_PER_LEVEL);
    }
    match->level_indices = (i8*)g_api.mem_arena_alloc(&match->_scratch, sizeof(i8) * num_players, alignof(i8)).p;

    match->num_players = num_players;
    match->attempts = (attempt*)g_api.mem_arena_alloc(&match->_scratch, sizeof(attempt) * NUM_LEVEL_PER_MATCH * num_players, alignof(attempt)).p;
    for (i32 i = 0; i < num_players; i++) {
        match->level_indices[i] = 0;

        for (i32 j = 0; j < NUM_LEVEL_PER_MATCH; j++) {
            i32 attempt_index = (i * NUM_LEVEL_PER_MATCH) + j;
            attempt_level_init(&match->attempts[attempt_index], &match->levels[j]);
        }
    }
}

void match_current_attempt(match *match, i8 player_index, level **lvl, attempt **att) {
    i8 lvl_index = match->level_indices[player_index];
    *lvl = &match->levels[lvl_index];
    *att = &match->attempts[(player_index * match->num_levels) + lvl_index];
}

void match_close(match *match) {
    match->num_levels = 0;
    match->num_players = 0;

    g_api.mem_arena_clear(&match->_scratch);
}

enum class game_event_type : u16 {
    LEVEL_GRAVITY_CHANGED = (u16)event_type::GAME_EVENTS_START,
    LEVEL_GEM_COMBO,
    LEVEL_COMPLETED,
    LEVEL_RESET,

    MATCH_COMPLETED,
};

struct level_gravity_changed_event {
    i8 player_index;
    direction old_dir;
    direction new_dir;
};

struct level_gem_combo_event {
    i8 player_index;
    i8 count;
    color gem_color;
};

struct level_completed_event {
    i8 player_index;
    u64 time_ns;
};

struct level_reset_event {
    i8 player_index;
};

struct match_completed_event {
    i8 winner_index;
    u64 total_time_ns;
};

config g_cfg;
f32 g_gravity_speed = 0;

match g_match;
i8 player_index = 0;

void grav_init(engine_api api) {
    g_api = api;
    g_in = api.input;

    // Register key bindings
    auto bind = [&](key_code k, game_action a) {
        g_api.input_bind_key(g_in, k, (u8)a);
    };
    bind(key_code::W,     game_action::GRAVITY_UP);
    bind(key_code::A,     game_action::GRAVITY_LEFT);
    bind(key_code::S,     game_action::GRAVITY_DOWN);
    bind(key_code::D,     game_action::GRAVITY_RIGHT);

    bind(key_code::UP,    game_action::GRAVITY_UP);
    bind(key_code::UP,    game_action::MENU_UP);
    bind(key_code::DOWN,  game_action::GRAVITY_DOWN);
    bind(key_code::DOWN,  game_action::MENU_DOWN);
    bind(key_code::LEFT,  game_action::GRAVITY_LEFT);
    bind(key_code::LEFT,  game_action::MENU_LEFT);
    bind(key_code::RIGHT, game_action::GRAVITY_RIGHT);
    bind(key_code::RIGHT, game_action::MENU_RIGHT);

    bind(key_code::RETURN, game_action::MENU_CONFIRM);
    bind(key_code::SPACE,  game_action::MENU_CONFIRM);
    bind(key_code::ESCAPE, game_action::MENU_CANCEL);

    bind(key_code::R,        game_action::RESET);
    bind(key_code::PAGE_UP,  game_action::DEBUG_PREV_LEVEL);
    bind(key_code::PAGE_DOWN, game_action::DEBUG_NEXT_LEVEL);

    g_api.config_init(&g_cfg, "assets/game.cfg");
    config_value val;
    if (g_api.config_read(&g_cfg, "gravity_speed", &val)) {
        g_gravity_speed = val.flt;
    }
    printf("[GAME] Loaded config; g_gravity_speed = %f\n", g_gravity_speed);

    FILE *lvl_file;
    i32 err = fopen_s(&lvl_file, "assets/bundle.bin", "rb");
    assert(err == 0);

    u8 lvl_data[BYTES_PER_MATCH];
    u64 read_len = fread_s(lvl_data, BYTES_PER_MATCH, BYTES_PER_MATCH, 1, lvl_file);
    assert(read_len >= 1);
    fclose(lvl_file);

    match_init(&g_match, 1, lvl_data, BYTES_PER_MATCH);
}

void grav_tick(f32 dt) {
    level *lvl;
    attempt *att;
    match_current_attempt(&g_match, player_index, &lvl, &att);

    if (g_api.input_pressed(g_in, (u8)game_action::RESET)) {
        attempt_level_reset(att, lvl);
    }

    if (g_api.input_pressed(g_in, (u8)game_action::DEBUG_PREV_LEVEL) && g_match.level_indices[player_index] > 0) {
        g_match.level_indices[player_index]--;
        match_current_attempt(&g_match, player_index, &lvl, &att);
    }
    if (g_api.input_pressed(g_in, (u8)game_action::DEBUG_NEXT_LEVEL) && g_match.level_indices[player_index] < g_match.num_levels - 1) {
        g_match.level_indices[player_index]++;
        match_current_attempt(&g_match, player_index, &lvl, &att);
    }

    if (!att->animating  && g_api.input_pressed(g_in, (u8)game_action::GRAVITY_UP)) {
        attempt_gravity_change(att, lvl, direction::UP);
    }
    else if (!att->animating  && g_api.input_pressed(g_in, (u8)game_action::GRAVITY_RIGHT)) {
        attempt_gravity_change(att, lvl, direction::RIGHT);
    }
    else if (!att->animating  && g_api.input_pressed(g_in, (u8)game_action::GRAVITY_DOWN)) {
        attempt_gravity_change(att, lvl, direction::DOWN);
    }
    else if (!att->animating  && g_api.input_pressed(g_in, (u8)game_action::GRAVITY_LEFT)) {
        attempt_gravity_change(att, lvl, direction::LEFT);
    }

    if (att->animating) {
        i32 num_moves = 0;
        auto anim_func = [&](vec2 *offsets, i32 i) {
            if (offsets[i] != vec2_zero) {
                num_moves++;

                f32 nx = math_move_toward(offsets[i].x, 0.f, g_gravity_speed * dt);
                f32 ny = math_move_toward(offsets[i].y, 0.f, g_gravity_speed * dt);
                offsets[i] = { nx, ny };
            }
        };

        for (i32 i = 0; i < att->num_crates; i++) {
            anim_func(att->crate_offsets, i);
        }
        for (i32 i = 0; i < att->num_gems; i++) {
            anim_func(att->gem_offsets, i);
        }

        if (num_moves <= 0) {
            att->animating = false;
            attempt_check_combos(att, lvl);
        }
    }
}

constexpr i32 colors_def[5][4] = {
    { 255, 0  , 0  , 255 },
    { 0  , 255, 0  , 255 },
    { 0  , 0  , 255, 255 },

    { 255, 255, 255, 255 }, // Cratews
    { 127, 127, 127, 255 }, // Walls
};
#define CRATE_COLOR_INDEX 3
#define WALL_COLOR_INDEX 4
#define EXPAND_COLOR(idx) colors_def[idx][0], colors_def[idx][1], colors_def[idx][2], colors_def[idx][3]

void grav_draw(f32 dt) {
    level *lvl;
    attempt *att;
    match_current_attempt(&g_match, player_index, &lvl, &att);

    // Render gravity indicator
    f32 center_x = 600.0f, center_y = 30.0f;
    f32 len = 15.0f;
    ivec2 dir = direction_vectors[(i32)att->current_gravity];
    f32 dx = (f32)dir.x * len;
    f32 dy = (f32)dir.y * len;

    SDL_SetRenderDrawColor(g_api.context, 255, 255, 0, 255);  // yellow
    SDL_RenderLine(g_api.context, center_x - dx, center_y - dy, center_x + dx, center_y + dy);
    // arrowhead
    f32 ax = (f32)(-dir.y) * 6.0f;
    f32 ay = (f32)(dir.x) * 6.0f;
    SDL_RenderLine(g_api.context, center_x + dx, center_y + dy, center_x + dx*0.5f + ax, center_y + dy*0.5f + ay);
    SDL_RenderLine(g_api.context, center_x + dx, center_y + dy, center_x + dx*0.5f - ax, center_y + dy*0.5f - ay);

    // Render solid walls
    SDL_SetRenderDrawColor(g_api.context, EXPAND_COLOR(WALL_COLOR_INDEX));
    f32 cell_size = 32;

    for (i32 y = 0; y < lvl->height; y++) {
        for (i32 x = 0; x < lvl->width; x++) {
            ivec2 p { x, y };
            if (level_is_solid(lvl, p)) {
                SDL_FRect r { x*cell_size, y*cell_size, cell_size-5, cell_size-5 };
                SDL_RenderFillRect(g_api.context, &r);
            }
        }
    }

    // Render elements crates and gems
    SDL_SetRenderDrawColor(g_api.context, EXPAND_COLOR(CRATE_COLOR_INDEX));
    for (i32 i = 0; i < att->num_crates; i++) {
        auto [x, y] = att->crates[i];
        auto [ox, oy] = att->crate_offsets[i];
        SDL_FRect r { (x+ox)*cell_size, (y+oy)*cell_size, cell_size-5, cell_size-5 };
        SDL_RenderFillRect(g_api.context, &r);
    }

    for (i32 i = 0; i < att->num_gems; i++) {
        if (!((att->gems_active >> i) & 1)) continue;
        SDL_SetRenderDrawColor(g_api.context, EXPAND_COLOR((i32)lvl->gem_colors[i]));

        auto [x, y] = att->gems[i];
        auto [ox, oy] = att->gem_offsets[i];
        SDL_FRect r { (x+ox)*cell_size, (y+oy)*cell_size, cell_size-5, cell_size-5 };
        SDL_RenderFillRect(g_api.context, &r);
    }
    //TODO: Need to rework rendering to a lower level so I could take advantage of instanced rendering here
    //TODO: Could also look into baking the background once and reusing it with one copy operation per frame
}

void grav_exit() {
    match_close(&g_match);
    g_api.config_free(&g_cfg);
}
