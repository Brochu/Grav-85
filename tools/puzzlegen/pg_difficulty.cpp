struct difficulty_weights {
    f32 moves;
    f32 gems;
    f32 colors;
    f32 density;
};

void difficulty_weights_from_config(difficulty_weights *w, config *cfg) {
    config_value val;
    w->moves = 0.45f; w->gems = 0.20f; w->colors = 0.15f; w->density = 0.20f;

    if (config_read(cfg, "weight_moves", &val))  w->moves  = val.integer / 100.0f;
    if (config_read(cfg, "weight_gems", &val))   w->gems   = val.integer / 100.0f;
    if (config_read(cfg, "weight_colors", &val)) w->colors = val.integer / 100.0f;
    if (config_read(cfg, "weight_density", &val)) w->density = val.integer / 100.0f;
}

static f32 clamp01(f32 v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

static f32 normalize(f32 v, f32 lo, f32 hi) {
    return clamp01((v - lo) / (hi - lo));
}

f32 difficulty_score(level *lvl, solve_result *sol, difficulty_weights *w, i32 max_solve_moves) {
    f32 move_score = normalize((f32)sol->optimal_moves, 1.0f, (f32)max_solve_moves);

    f32 gem_score = normalize((f32)lvl->num_gems, 2.0f, 16.0f);

    // Count distinct colors
    i32 color_seen[3] = {};
    i32 color_counts[3] = {};
    for (i32 i = 0; i < lvl->num_gems; i++) {
        i32 c = (i32)lvl->gem_colors[i];
        if (c < 3) { color_seen[c] = 1; color_counts[c]++; }
    }
    i32 num_colors = color_seen[0] + color_seen[1] + color_seen[2];
    f32 color_score = normalize((f32)num_colors, 1.0f, 3.0f);

    // Grid density = walls / total interior cells
    i32 interior = (lvl->width - 2) * (lvl->height - 2);
    i32 wall_count = 0;
    for (i32 y = 1; y < lvl->height - 1; y++) {
        for (i32 x = 1; x < lvl->width - 1; x++) {
            if (level_is_solid(lvl, {x, y})) wall_count++;
        }
    }
    f32 density = (interior > 0) ? (f32)wall_count / (f32)interior : 0.0f;
    f32 density_score = normalize(density, 0.1f, 0.5f);

    f32 score = w->moves * move_score
              + w->gems * gem_score
              + w->colors * color_score
              + w->density * density_score;

    // Odd-color bonus: +0.05 per color with odd count
    for (i32 c = 0; c < 3; c++) {
        if (color_counts[c] > 0 && (color_counts[c] % 2) != 0) {
            score += 0.05f;
        }
    }

    return clamp01(score);
}
