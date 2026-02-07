struct sim_state {
    ivec2 crates[ELEMENTS_MAX_NUM];
    ivec2 gems[ELEMENTS_MAX_NUM];
    color gem_colors[ELEMENTS_MAX_NUM];
    direction current_gravity;
    u32 gems_active;
    i8 num_crates;
    i8 num_gems;
};

void sim_init(sim_state *s, level *lvl) {
    memcpy(s->crates, lvl->crate_starts, sizeof(lvl->crate_starts));
    memcpy(s->gems, lvl->gem_starts, sizeof(lvl->gem_starts));
    memcpy(s->gem_colors, lvl->gem_colors, sizeof(lvl->gem_colors));
    s->current_gravity = lvl->start_gravity;
    s->gems_active = (1u << lvl->num_gems) - 1;
    s->num_crates = lvl->num_crates;
    s->num_gems = lvl->num_gems;
}

static bool sim_element_at(sim_state *s, ivec2 pos) {
    for (i32 i = 0; i < s->num_crates; i++) {
        if (s->crates[i] == pos) return true;
    }
    for (i32 i = 0; i < s->num_gems; i++) {
        if ((s->gems_active >> i) & 1) {
            if (s->gems[i] == pos) return true;
        }
    }
    return false;
}

void sim_apply_gravity(sim_state *s, level *lvl, direction new_gravity) {
    s->current_gravity = new_gravity;
    ivec2 dir = direction_vectors[(u8)new_gravity];
    ivec2 opp = direction_vectors[((u8)new_gravity + 2) % 4];

    // Sort elements so furthest along gravity direction updates first
    i32 update_indices[ELEMENTS_MAX_NUM * 2];
    element_type update_types[ELEMENTS_MAX_NUM * 2];
    i32 total = 0;

    for (i32 i = 0; i < s->num_crates; i++) {
        update_indices[total] = i;
        update_types[total] = element_type::CRATE;
        total++;
    }
    for (i32 i = 0; i < s->num_gems; i++) {
        if (!((s->gems_active >> i) & 1)) continue;
        update_indices[total] = i;
        update_types[total] = element_type::GEM;
        total++;
    }

    ivec2 *positions[element_type::COUNT];
    positions[element_type::CRATE] = s->crates;
    positions[element_type::GEM] = s->gems;

    // Insertion sort by dot product with gravity direction (descending)
    for (i32 i = 1; i < total; i++) {
        i32 key = update_indices[i];
        element_type key_type = update_types[i];
        i32 key_dot = ivec2_dot(positions[key_type][key], dir);
        i32 j = i - 1;
        while (j >= 0 && ivec2_dot(positions[update_types[j]][update_indices[j]], dir) < key_dot) {
            update_indices[j + 1] = update_indices[j];
            update_types[j + 1] = update_types[j];
            j--;
        }
        update_indices[j + 1] = key;
        update_types[j + 1] = key_type;
    }

    for (i32 i = 0; i < total; i++) {
        ivec2 start = positions[update_types[i]][update_indices[i]];
        ivec2 next = start + dir;

        while (!level_is_solid(lvl, next) && !sim_element_at(s, next)) {
            next = next + dir;
        }
        ivec2 end = next + opp;
        positions[update_types[i]][update_indices[i]] = end;
    }
}

bool sim_check_combos(sim_state *s) {
    bool any_matched = false;
    bool visited[ELEMENTS_MAX_NUM] = {};
    i32 queue[ELEMENTS_MAX_NUM];

    for (i32 i = 0; i < s->num_gems; i++) {
        if (!((s->gems_active >> i) & 1)) continue;
        if (visited[i]) continue;

        // BFS flood fill for connected same-color gems
        i32 component[ELEMENTS_MAX_NUM];
        i32 comp_size = 0;
        i32 q_head = 0, q_tail = 0;

        queue[q_tail++] = i;
        visited[i] = true;
        component[comp_size++] = i;

        while (q_head < q_tail) {
            i32 cur = queue[q_head++];
            ivec2 pos = s->gems[cur];

            // Check all 4 neighbors
            for (i32 d = 0; d < 4; d++) {
                ivec2 neighbor = pos + direction_vectors[d];

                for (i32 j = 0; j < s->num_gems; j++) {
                    if (!((s->gems_active >> j) & 1)) continue;
                    if (visited[j]) continue;
                    if (s->gems[j] == neighbor && s->gem_colors[j] == s->gem_colors[cur]) {
                        visited[j] = true;
                        queue[q_tail++] = j;
                        component[comp_size++] = j;
                    }
                }
            }
        }

        if (comp_size >= 2) {
            any_matched = true;
            for (i32 k = 0; k < comp_size; k++) {
                s->gems_active &= ~(1u << component[k]);
            }
        }
    }

    return any_matched;
}

void sim_apply_move(sim_state *s, level *lvl, direction dir) {
    sim_apply_gravity(s, lvl, dir);
    while (sim_check_combos(s)) {
        sim_apply_gravity(s, lvl, s->current_gravity);
    }
}

bool sim_is_solved(sim_state *s) {
    return s->gems_active == 0;
}
