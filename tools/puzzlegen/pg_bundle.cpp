struct puzzle_entry {
    level lvl;
    solve_result sol;
    f32 difficulty;
};

struct bundle_tier {
    f32 min_difficulty;
    f32 max_difficulty;
};

void bundle_tier_from_config(bundle_tier *tier, config *cfg, const char *tier_name) {
    // Default to medium
    tier->min_difficulty = 0.25f;
    tier->max_difficulty = 0.60f;

    char key[64];
    snprintf(key, sizeof(key), "bundle_tier_%s", tier_name);

    config_value val;
    if (config_read(cfg, key, &val) && val.type == value_type::RANGE) {
        tier->min_difficulty = val.range.min / 100.0f;
        tier->max_difficulty = val.range.max / 100.0f;
    }
}

// Sort puzzle pool by difficulty (insertion sort)
void pool_sort_by_difficulty(puzzle_entry *pool, i32 count) {
    for (i32 i = 1; i < count; i++) {
        puzzle_entry key = pool[i];
        i32 j = i - 1;
        while (j >= 0 && pool[j].difficulty > key.difficulty) {
            pool[j + 1] = pool[j];
            j--;
        }
        pool[j + 1] = key;
    }
}

bool bundle_assemble(bundle *b, puzzle_entry *sorted_pool, i32 pool_count, bundle_tier *tier) {
    // Find puzzles within tier range
    i32 tier_start = -1, tier_end = -1;
    for (i32 i = 0; i < pool_count; i++) {
        if (sorted_pool[i].difficulty >= tier->min_difficulty && tier_start < 0) {
            tier_start = i;
        }
        if (sorted_pool[i].difficulty <= tier->max_difficulty) {
            tier_end = i;
        }
    }

    if (tier_start < 0 || tier_end < 0 || (tier_end - tier_start + 1) < 5) {
        return false;
    }

    i32 range = tier_end - tier_start + 1;

    // Pick 5 puzzles at evenly spaced slots for escalating difficulty
    for (i32 slot = 0; slot < 5; slot++) {
        i32 idx = tier_start + (slot * (range - 1)) / 4;
        b->levels[slot] = sorted_pool[idx].lvl;
        b->difficulty_scores[slot] = sorted_pool[idx].difficulty;
        b->optimal_moves[slot] = sorted_pool[idx].sol.optimal_moves;
    }

    return true;
}
