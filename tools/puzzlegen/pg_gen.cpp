struct gen_params {
    i32 width_min, width_max;
    i32 height_min, height_max;
    i32 gems_min, gems_max;
    i32 crates_min, crates_max;
    i32 colors_min, colors_max;
    i32 wall_density_min, wall_density_max; // percentage 0-100
};

void gen_params_from_config(gen_params *p, config *cfg) {
    config_value val;

    p->width_min = 6; p->width_max = 10;
    p->height_min = 6; p->height_max = 10;
    p->gems_min = 4; p->gems_max = 12;
    p->crates_min = 0; p->crates_max = 4;
    p->colors_min = 2; p->colors_max = 3;
    p->wall_density_min = 15; p->wall_density_max = 35;

    if (config_read(cfg, "grid_width", &val) && val.type == value_type::RANGE) {
        p->width_min = val.range.min; p->width_max = val.range.max;
    }
    if (config_read(cfg, "grid_height", &val) && val.type == value_type::RANGE) {
        p->height_min = val.range.min; p->height_max = val.range.max;
    }
    if (config_read(cfg, "num_gems", &val) && val.type == value_type::RANGE) {
        p->gems_min = val.range.min; p->gems_max = val.range.max;
    }
    if (config_read(cfg, "num_crates", &val) && val.type == value_type::RANGE) {
        p->crates_min = val.range.min; p->crates_max = val.range.max;
    }
    if (config_read(cfg, "num_colors", &val) && val.type == value_type::RANGE) {
        p->colors_min = val.range.min; p->colors_max = val.range.max;
    }
    if (config_read(cfg, "wall_density", &val) && val.type == value_type::RANGE) {
        p->wall_density_min = val.range.min; p->wall_density_max = val.range.max;
    }
}

bool gen_random_level(level *lvl, gen_params *p) {
    memset(lvl, 0, sizeof(level));

    lvl->width = (i8)rand_int_min(p->width_min, p->width_max + 1);
    lvl->height = (i8)rand_int_min(p->height_min, p->height_max + 1);
    i32 num_colors = rand_int_min(p->colors_min, p->colors_max + 1);
    lvl->num_gems = (i8)rand_int_min(p->gems_min, p->gems_max + 1);
    lvl->num_crates = (i8)rand_int_min(p->crates_min, p->crates_max + 1);
    lvl->start_gravity = (direction)rand_int(4);

    // Set border cells as solid
    for (i32 y = 0; y < lvl->height; y++) {
        for (i32 x = 0; x < lvl->width; x++) {
            bool border = (x == 0 || y == 0 || x == lvl->width - 1 || y == lvl->height - 1);
            if (border) level_set_solid(lvl, {x, y}, true);
        }
    }

    // Place interior walls by density
    i32 interior_cells = (lvl->width - 2) * (lvl->height - 2);
    i32 density = rand_int_min(p->wall_density_min, p->wall_density_max + 1);
    i32 num_walls = (interior_cells * density) / 100;

    for (i32 w = 0; w < num_walls; w++) {
        i32 x = rand_int_min(1, lvl->width - 1);
        i32 y = rand_int_min(1, lvl->height - 1);
        level_set_solid(lvl, {x, y}, true);
    }

    // Collect open cells for element placement
    ivec2 open[MAP_MAX_SIZE];
    i32 num_open = 0;
    for (i32 y = 1; y < lvl->height - 1; y++) {
        for (i32 x = 1; x < lvl->width - 1; x++) {
            if (!level_is_solid(lvl, {x, y})) {
                open[num_open++] = {x, y};
            }
        }
    }

    i32 total_elements = lvl->num_gems + lvl->num_crates;
    if (num_open < total_elements) return false;

    // Fisher-Yates shuffle on open cells
    for (i32 i = num_open - 1; i > 0; i--) {
        i32 j = rand_int(i + 1);
        ivec2 tmp = open[i];
        open[i] = open[j];
        open[j] = tmp;
    }

    // Place gems
    for (i32 i = 0; i < lvl->num_gems; i++) {
        lvl->gem_starts[i] = open[i];
        lvl->gem_colors[i] = (color)(i % num_colors);
    }

    // Reject if any same-color gems are adjacent in starting state
    for (i32 i = 0; i < lvl->num_gems; i++) {
        for (i32 j = i + 1; j < lvl->num_gems; j++) {
            if (lvl->gem_colors[i] != lvl->gem_colors[j]) continue;
            ivec2 diff = lvl->gem_starts[i] - lvl->gem_starts[j];
            i32 dist = (diff.x < 0 ? -diff.x : diff.x) + (diff.y < 0 ? -diff.y : diff.y);
            if (dist == 1) return false;
        }
    }

    // Place crates
    for (i32 i = 0; i < lvl->num_crates; i++) {
        lvl->crate_starts[i] = open[lvl->num_gems + i];
    }

    return true;
}
