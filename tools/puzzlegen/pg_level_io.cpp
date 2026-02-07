#include "qg_math.hpp"

// Reuse constants and types from gr_main.cpp
enum class color : u8 {
    RED,
    GREEN,
    BLUE,
};

enum element_type : u8 {
    CRATE,
    GEM,
    COUNT,
};

#define ELEMENTS_MAX_NUM 32
#define MAP_MAX_SIZE 256
#define LEVEL_FILE_SIZE 108

struct level {
    u8 solid[MAP_MAX_SIZE / 8];
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

inline void level_set_solid(level *lvl, ivec2 pos, bool solid) {
    u32 idx = (pos.y * lvl->width) + pos.x;
    if (solid) lvl->solid[idx / 8] |=  (1 << (idx % 8));
    else       lvl->solid[idx / 8] &= ~(1 << (idx % 8));
}

inline ivec2 unpack_pos(u8 packed) {
    return { packed >> 4, packed & 0xF };
}

inline u8 pack_pos(ivec2 pos) {
    return (u8)((pos.x << 4) | (pos.y & 0xF));
}

void level_read_binary(const u8 data[LEVEL_FILE_SIZE], level *lvl) {
    u8 dims = data[0];
    lvl->width = (dims >> 4) & 0xf;
    lvl->height = dims & 0xf;

    lvl->start_gravity = (direction)data[1];
    lvl->num_crates = (i8)data[2];
    lvl->num_gems = (i8)data[3];

    const u8 *crate_data = &data[12];
    for (i32 i = 0; i < lvl->num_crates; i++) {
        lvl->crate_starts[i] = unpack_pos(crate_data[i]);
    }

    u64 colors_data;
    memcpy(&colors_data, &data[4], sizeof(u64));
    const u8 *gem_data = &data[44];
    for (i32 i = 0; i < lvl->num_gems; i++) {
        lvl->gem_colors[i] = (color)((colors_data >> (2 * i)) & 0b11);
        lvl->gem_starts[i] = unpack_pos(gem_data[i]);
    }

    const u8 *solid_data = &data[76];
    memcpy(lvl->solid, solid_data, MAP_MAX_SIZE / 8);
}

void level_write_binary(level *lvl, u8 data[LEVEL_FILE_SIZE]) {
    memset(data, 0, LEVEL_FILE_SIZE);

    data[0] = (u8)((lvl->width << 4) | (lvl->height & 0xF));
    data[1] = (u8)lvl->start_gravity;
    data[2] = (u8)lvl->num_crates;
    data[3] = (u8)lvl->num_gems;

    u64 colors_data = 0;
    for (i32 i = 0; i < lvl->num_gems; i++) {
        colors_data |= ((u64)lvl->gem_colors[i] & 0b11) << (2 * i);
    }
    memcpy(&data[4], &colors_data, sizeof(u64));

    u8 *crate_data = &data[12];
    for (i32 i = 0; i < lvl->num_crates; i++) {
        crate_data[i] = pack_pos(lvl->crate_starts[i]);
    }

    u8 *gem_data = &data[44];
    for (i32 i = 0; i < lvl->num_gems; i++) {
        gem_data[i] = pack_pos(lvl->gem_starts[i]);
    }

    memcpy(&data[76], lvl->solid, MAP_MAX_SIZE / 8);
}

bool level_file_read(level *lvl, const char *file_path) {
    FILE *f;
    i32 err = fopen_s(&f, file_path, "rb");
    if (err != 0 || !f) return false;

    u8 data[LEVEL_FILE_SIZE];
    u64 read_len = fread(data, LEVEL_FILE_SIZE, 1, f);
    fclose(f);
    if (read_len < 1) return false;

    level_read_binary(data, lvl);
    return true;
}

bool level_file_write(level *lvl, const char *file_path) {
    FILE *f;
    i32 err = fopen_s(&f, file_path, "wb");
    if (err != 0 || !f) return false;

    u8 data[LEVEL_FILE_SIZE];
    level_write_binary(lvl, data);
    u64 written = fwrite(data, LEVEL_FILE_SIZE, 1, f);
    fclose(f);
    return written == 1;
}

struct bundle {
    level levels[5];
    f32 difficulty_scores[5];
    i32 optimal_moves[5];
};

bool bundle_write(bundle *b, const char *bin_path, const char *meta_path) {
    FILE *f;
    i32 err = fopen_s(&f, bin_path, "wb");
    if (err != 0 || !f) return false;

    for (i32 i = 0; i < 5; i++) {
        u8 data[LEVEL_FILE_SIZE];
        level_write_binary(&b->levels[i], data);
        fwrite(data, LEVEL_FILE_SIZE, 1, f);
    }
    fclose(f);

    err = fopen_s(&f, meta_path, "w");
    if (err != 0 || !f) return false;

    fprintf(f, "# Bundle metadata\n");
    for (i32 i = 0; i < 5; i++) {
        fprintf(f, "level_%d: difficulty=%.4f optimal_moves=%d\n",
                i, b->difficulty_scores[i], b->optimal_moves[i]);
    }
    fclose(f);
    return true;
}
