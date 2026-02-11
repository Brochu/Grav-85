#include <cstdio>

#include "qg_config.hpp"
#include "qg_input.hpp"
#include "qg_math.hpp"
#include "qg_memory.hpp"
#include "shared.hpp"

#include "SDL3/SDL.h"

engine_api g_api {};
input_state *g_in {};

enum class color : u8 {
    RED,
    GREEN,
    BLUE,
};
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

#define NUM_LEVEL_PER_BUNDLE 5
#define BYTES_PER_LEVEL 108
#define BYTES_PER_BUNDLE NUM_LEVEL_PER_BUNDLE * BYTES_PER_LEVEL

/*
struct bundle {
    i8 num_levels;
    level *levels;
    mem_arena _scratch;
};
*/

enum element_type : u8 {
    CRATE,
    GEM,
    COUNT,
};
#define ELEMENTS_MAX_NUM 32
#define MAP_MAX_SIZE 256

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

void level_file_init(level *lvl, const char *file_path) {
    FILE *lvl_file;
    i32 err = fopen_s(&lvl_file, file_path, "rb");
    assert(err == 0);

    u8 lvl_data[BYTES_PER_LEVEL];
    u64 read_len = fread_s(lvl_data, BYTES_PER_LEVEL, BYTES_PER_LEVEL, 1, lvl_file);
    assert(read_len >= 1);
    fclose(lvl_file);

    u8 dims = (u8)lvl_data[0];
    lvl->width = (dims >> 4) & 0xf;
    lvl->height = dims & 0xf;

    lvl->start_gravity = direction::COUNT;//(direction)lvl_data[1];
    lvl->num_crates = (i8)lvl_data[2];
    lvl->num_gems = (i8)lvl_data[3];

    u8 *crate_data = (u8*)&lvl_data[12];
    for (int i = 0; i < lvl->num_crates; i++) {
        lvl->crate_starts[i] = unpack_pos(crate_data[i]);
    }

    u64 colors_data;
    memcpy(&colors_data, &lvl_data[4], sizeof(u64));
    u8 *gem_data = (u8*)&lvl_data[44];
    for (int i = 0; i < lvl->num_gems; i++) {
        lvl->gem_colors[i] = (color)((colors_data >> (2 * i)) & 0b11);
        lvl->gem_starts[i] = unpack_pos(gem_data[i]);
    }

    u8 *solid_data = (u8*)&lvl_data[76];
    memcpy_s(lvl->solid, MAP_MAX_SIZE / 8, solid_data, MAP_MAX_SIZE / 8);
}
void level_data_init(level *lvl, arena_ptr data) { }

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
        if (att->gems[i] == pos) {
            return true;
        }
    }

    return false;
}

void attempt_gravity_change(attempt *att, level *lvl, direction new_gravity) {
    //TODO: Do we need to store the level in the run itself?
    if (new_gravity == att->current_gravity) {
        return;
    }

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

void attempt_check_combos(attempt *att) {
    //TODO: Flood fill to check for gems that should de-activate
}

config g_cfg;
f32 g_gravity_speed = 0;

level g_lvl;
attempt g_att;

void grav_init(engine_api api) {
    g_api = api;
    g_in = api.input;

    g_api.config_init(&g_cfg, "assets/game.cfg");
    config_value val;
    if (g_api.config_read(&g_cfg, "gravity_speed", &val)) {
        g_gravity_speed = val.flt;
    }
    printf("[GAME] Loaded config; g_gravity_speed = %f\n", g_gravity_speed);

    level_file_init(&g_lvl, "assets/5-level.bin");
    attempt_level_init(&g_att, &g_lvl);
}

void grav_tick(f32 dt) {
    if (!g_att.animating  && g_api.input_pressed(g_in, input_action::RESET)) {
        attempt_level_reset(&g_att, &g_lvl);
    }

    if (!g_att.animating  && g_api.input_pressed(g_in, input_action::GRAVITY_UP)) {
        attempt_gravity_change(&g_att, &g_lvl, direction::UP);
    }
    else if (!g_att.animating  && g_api.input_pressed(g_in, input_action::GRAVITY_RIGHT)) {
        attempt_gravity_change(&g_att, &g_lvl, direction::RIGHT);
    }
    else if (!g_att.animating  && g_api.input_pressed(g_in, input_action::GRAVITY_DOWN)) {
        attempt_gravity_change(&g_att, &g_lvl, direction::DOWN);
    }
    else if (!g_att.animating  && g_api.input_pressed(g_in, input_action::GRAVITY_LEFT)) {
        attempt_gravity_change(&g_att, &g_lvl, direction::LEFT);
    }

    if (g_att.animating) {
        i32 num_moves = 0;
        auto anim_func = [&](vec2 *offsets, i32 i) {
            if (offsets[i] != vec2_zero) {
                num_moves++;

                f32 nx = math_move_toward(offsets[i].x, 0.f, g_gravity_speed * dt);
                f32 ny = math_move_toward(offsets[i].y, 0.f, g_gravity_speed * dt);
                offsets[i] = { nx, ny };
            }
        };

        for (i32 i = 0; i < g_att.num_crates; i++) {
            anim_func(g_att.crate_offsets, i);
        }
        for (i32 i = 0; i < g_att.num_gems; i++) {
            anim_func(g_att.gem_offsets, i);
        }

        if (num_moves <= 0) {
            g_att.animating = false;
        }
    }
}

void grav_draw(f32 dt) {
    // Render gravity indicator
    f32 center_x = 600.0f, center_y = 30.0f;
    f32 len = 15.0f;
    ivec2 dir = direction_vectors[(i32)g_att.current_gravity];
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

    for (i32 y = 0; y < g_lvl.height; y++) {
        for (i32 x = 0; x < g_lvl.width; x++) {
            ivec2 p { x, y };
            if (level_is_solid(&g_lvl, p)) {
                SDL_FRect r { x*cell_size, y*cell_size, cell_size-5, cell_size-5 };
                SDL_RenderFillRect(g_api.context, &r);
            }
        }
    }

    // Render elements crates and gems
    SDL_SetRenderDrawColor(g_api.context, EXPAND_COLOR(CRATE_COLOR_INDEX));
    for (i32 i = 0; i < g_att.num_crates; i++) {
        auto [x, y] = g_att.crates[i];
        auto [ox, oy] = g_att.crate_offsets[i];
        SDL_FRect r { (x+ox)*cell_size, (y+oy)*cell_size, cell_size-5, cell_size-5 };
        SDL_RenderFillRect(g_api.context, &r);
    }

    for (i32 i = 0; i < g_att.num_gems; i++) {
        if (!((g_att.gems_active >> i) & 1)) continue;
        SDL_SetRenderDrawColor(g_api.context, EXPAND_COLOR((i32)g_lvl.gem_colors[i]));

        auto [x, y] = g_att.gems[i];
        auto [ox, oy] = g_att.gem_offsets[i];
        SDL_FRect r { (x+ox)*cell_size, (y+oy)*cell_size, cell_size-5, cell_size-5 };
        SDL_RenderFillRect(g_api.context, &r);
    }
    //TODO: Need to rework rendering to a lower level so I could take advantage of instanced rendering here
    //TODO: Could also look into baking the background once and reusing it with one copy operation per frame
}

void grav_exit() {
    g_api.config_free(&g_cfg);
}
